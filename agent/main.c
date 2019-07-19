#include "lwip/debug.h"
#include "lwip/dhcp.h"
#include "lwip/init.h"
#include "lwip/netif.h"
#include "lwip/sys.h"
#include "lwip/tcpip.h"
#include "lwip/timers.h"
#include "netif/etharp.h"
#include "pktdrv.h"

// #undef printf // FIXME: I #defined this to debugPrint but now it conflicts
// #include <stdio.h>

#include <hal/input.h>
#include <hal/xbox.h>
#include <pbkit/pbkit.h>
#include <xboxkrnl/xboxkrnl.h>
#include <debug.h>
#include "lwip/opt.h"
#include "lwip/arch.h"
#include "lwip/api.h"
#include <string.h>
#include <winapi/windef.h>
#include <hal/fileio.h>
#include <assert.h>
#include <stdlib.h>

#undef _MSC_VER
#include "xxHash/xxhash.h"

#ifndef HTTPD_DEBUG
#define HTTPD_DEBUG         LWIP_DBG_OFF
#endif

#define USE_DHCP         1
#define PKT_TMR_INTERVAL 1 /* ms */
#define DEBUGGING        0

#define DPRINTF(...) do {} while (0);

struct netif nforce_netif, *g_pnetif;

err_t nforceif_init(struct netif *netif);
static void packet_timer(void *arg);

//-----------------------------------------------------------------------------
// Agent Configuration File
//-----------------------------------------------------------------------------

struct agent_config {
    uint32_t ip_addr;
    uint32_t ip_mask;
    uint32_t ip_gw;
    uint32_t cs_ip;
} agent_config;

ip_addr_t xbox_ip;
ip4_addr_t central_server_ip;


static void load_default_config(void)
{
    printf("[**] Loading default config\n");
    agent_config.ip_addr = (10  << 0) | (0   << 8) | (2   << 16) | (10 << 24);
    agent_config.ip_mask = (255 << 0) | (255 << 8) | (255 << 16) | (0 << 24);
    agent_config.ip_gw   = (10  << 0) | (0   << 8) | (2   << 16) | (1 << 24);
    agent_config.cs_ip   = (10  << 0) | (0   << 8) | (2   << 16) | (2 << 24);
}

static int load_agent_config(void)
{
    int status;
    int file_handle;
    unsigned int read;

    const char *path = "E:\\agent_config.bin";

    status = XCreateFile(&file_handle,
                         path,
                         GENERIC_READ,
                         FILE_SHARE_READ,
                         OPEN_EXISTING,
                         FILE_ATTRIBUTE_NORMAL);

    if (status != 0) {
        printf("[!!] Failed to load agent config file\n");
        load_default_config();
        return -1;
    }

    unsigned int file_size;
    XGetFileSize(file_handle, &file_size);

    if (file_size != sizeof(agent_config)) {
        printf("[!!] Invalid agent config file\n");
        XCloseHandle(file_handle);
        load_default_config();
        return -1;
    }

    status = XReadFile(file_handle, &agent_config, file_size, &read);
    if ((status == 0) || (read != sizeof(agent_config))) {
        printf("[!!] Failed to load agent config file\n");
        XCloseHandle(file_handle);
        load_default_config();
        return -1;
    }

    printf("[**] Agent config loaded\n");
    XCloseHandle(file_handle);
    return 0;
}

//-----------------------------------------------------------------------------
// Dumb Command Interface Headers and Callbacks
//-----------------------------------------------------------------------------

enum CMD {
    CMD_WRITE  = 0x45545257, // 'WRTE'
    CMD_LAUNCH = 0x48434e4c, // 'LNCH'
    CMD_REBOOT = 0x54455352, // 'RSET'
};

#pragma pack(1)

struct cmd_reboot {
};

struct cmd_write {
    char file_name[32];
};

struct cmd_launch {
    char file_name[32];
};

// Requests are always same length followed by any request data
struct cmd_request {
    uint32_t cmd_id;
    uint32_t length; // Total length of this request including header and data
    union {
        struct cmd_reboot reboot;
        struct cmd_write  write;
        struct cmd_launch launch;
    };
};

#pragma pack()

struct connection;
struct cmd_cb;

struct connection {
    struct netconn *conn;
    ip_addr_t naddr;
    u16_t port;
    struct cmd_request request;
    struct cmd_cb *cb;
    size_t request_bytes_received;
    size_t total_bytes_received;
};

// static void write(struct connection *c, const char *data, size_t len)
// {
//     netconn_write(c->conn, data, len, NETCONN_COPY);
// }

typedef enum {
    CB_STATUS_OK = 0,
    CB_STATUS_ERR = -1,
} cb_status;

typedef cb_status (*cb_on_connect)(struct connection *c);
typedef cb_status (*cb_on_data)(struct connection *c, const char *data, size_t len);
typedef cb_status (*cb_on_close)(struct connection *c);

struct cmd_cb {
    cb_on_connect on_connect;
    cb_on_data    on_data;
    cb_on_close   on_close;
};

static cb_status on_connect_reboot(struct connection *c);
static cb_status on_data_reboot(struct connection *c, const char *data, size_t len);
static cb_status on_close_reboot(struct connection *c);

struct cmd_cb reboot_cb = {
    .on_connect = &on_connect_reboot,
    .on_data    = &on_data_reboot,
    .on_close   = &on_close_reboot,
};

static cb_status on_connect_reboot(struct connection *c)
{
    DPRINTF("%s\n", __func__);

    printf("REBOOTING!\n");

    pb_kill();
    Pktdrv_Quit();
    XReboot();

    return CB_STATUS_OK;
}

static cb_status on_data_reboot(struct connection *c, const char *data, size_t len)
{
    DPRINTF("%s\n", __func__);
    return CB_STATUS_OK;
}

static cb_status on_close_reboot(struct connection *c)
{
    DPRINTF("%s\n", __func__);
    return CB_STATUS_OK;
}


static cb_status on_connect_write(struct connection *c);
static cb_status on_data_write(struct connection *c, const char *data, size_t len);
static cb_status on_close_write(struct connection *c);

struct cmd_cb write_cb = {
    .on_connect = &on_connect_write,
    .on_data    = &on_data_write,
    .on_close   = &on_close_write,
};

XXH64_state_t* state;
int file_handle;
char *buffer = NULL;
size_t buffer_pos;
size_t buffer_size;

static cb_status on_connect_write(struct connection *c)
{
    DPRINTF("%s\n", __func__);

    /* create a hash state */
    state = XXH64_createState();
    if (state == NULL) {
        printf("XXH64 error\n");
        return CB_STATUS_ERR;
    }

    XXH_errorcode const resetResult = XXH64_reset(state, 0);
    if (resetResult == XXH_ERROR) {
        printf("XXH64 error\n");
        return CB_STATUS_ERR;
    }

    char path[MAX_PATH];
    memcpy(path, c->request.launch.file_name, sizeof(c->request.launch.file_name));
    path[sizeof(c->request.launch.file_name)] = '\x00';


    // Allocate a small buffer in memory that we can use to reduce number of file writes
    buffer_size = 4 * 1024 * 1024;
    buffer = malloc(buffer_size);
    if (buffer == NULL) {
        printf("Failed to allocate a buffer\n");
        return CB_STATUS_ERR;
    }
    buffer_pos = 0;

    printf("Creating file %s\n", path);

    int result = XCreateFile(&file_handle,
        path,
        GENERIC_READ | GENERIC_WRITE,
        0,
        CREATE_ALWAYS, // Create, overwrite if exists
        FILE_ATTRIBUTE_NORMAL);

    if ((result != STATUS_SUCCESS) && (result != ERROR_ALREADY_EXISTS)) {
        printf("Failed to open file\n");
        free(buffer);
        return CB_STATUS_ERR;
    }

    return CB_STATUS_OK;
}

static int flush_write_buffer_to_disk()
{
    unsigned int bytes_written;

    assert(file_handle != NULL);
    int result = XWriteFile(file_handle, buffer, buffer_pos, &bytes_written);

    if (result != TRUE) {
        printf("Failed to write to file\n");
        return CB_STATUS_ERR;
    }

    if (bytes_written != buffer_pos) {
        printf("Could not write all bytes\n");
        return CB_STATUS_ERR;
    }

    printf("Wrote %zd bytes to disk\n", bytes_written);

    buffer_pos = 0;

    return CB_STATUS_OK;
}

static cb_status on_data_write(struct connection *c, const char *data, size_t len)
{
    cb_status status = 0;
    DPRINTF("%s\n", __func__);

    XXH_errorcode const updateResult = XXH64_update(state, data, len);
    if (updateResult == XXH_ERROR) {
        printf("Digest update failed!\n");
        return CB_STATUS_ERR;
    }

    // Flush to disk if we don't have enough space remaining in the buf
    if ((buffer_pos + len) >= buffer_size) {
        status = flush_write_buffer_to_disk();
        if (status != CB_STATUS_OK) {
            return status;
        }
    }

    memcpy(buffer + buffer_pos, data, len);
    buffer_pos += len;

    return CB_STATUS_OK;
}

static cb_status on_close_write(struct connection *c)
{
    cb_status status;

    printf("%s\n", __func__);

    if (state) {
        printf("getting digest\n");
        XXH64_hash_t const hash = XXH64_digest(state);
        printf("HASH: %016llx\n", hash);
        XXH64_freeState(state);
        state = NULL;
    }

    if (buffer) {
        status = flush_write_buffer_to_disk();
        if (status != CB_STATUS_OK) {
            return status;
        }
        XCloseHandle(file_handle);
        file_handle = NULL;
        free(buffer);
        buffer = NULL;
    }

    return CB_STATUS_OK;
}


static cb_status on_connect_launch(struct connection *c);
static cb_status on_data_launch(struct connection *c, const char *data, size_t len);
static cb_status on_close_launch(struct connection *c);

struct cmd_cb launch_cb = {
    .on_connect = &on_connect_launch,
    .on_data    = &on_data_launch,
    .on_close   = &on_close_launch,
};

static cb_status on_connect_launch(struct connection *c)
{
    DPRINTF("%s\n", __func__);

    char path[MAX_PATH];
    memcpy(path, c->request.launch.file_name, sizeof(c->request.launch.file_name));
    path[sizeof(c->request.launch.file_name)] = '\x00';

    printf(">>>> Launching '%s'\n", path);

    pb_kill();
    Pktdrv_Quit();
    XLaunchXBE(path);

    // If we get here, we failed to launch...
    return CB_STATUS_ERR;
}

static cb_status on_data_launch(struct connection *c, const char *data, size_t len)
{
    DPRINTF("%s\n", __func__);
    
    // Nothing to do

    return CB_STATUS_OK;
}

static cb_status on_close_launch(struct connection *c)
{
    DPRINTF("%s\n", __func__);
    
    // Nothing to do

    return CB_STATUS_OK;
}



static void respond_ok(struct netconn *conn)
{
  netconn_write(conn, "OK\x00", 2, NETCONN_COPY);
}

static void respond_error(struct netconn *conn)
{
  netconn_write(conn, "NO\x00", 2, NETCONN_COPY);
}

// Called when packet data is available
// Returns number of bytes processed
static int data_received(struct connection *c, const char *data, size_t len)
{
    int status;
    size_t amount_to_copy = len;

    //
    // Read fixed-length request header
    //
    if (c->request_bytes_received < sizeof(c->request)) {
        if (amount_to_copy > sizeof(c->request)) {
            amount_to_copy = sizeof(c->request);
        }
        
        memcpy((char*)(&c->request) + c->request_bytes_received, data, amount_to_copy);
        c->request_bytes_received += amount_to_copy;
        c->total_bytes_received += amount_to_copy;

        // Did we receive the full header?
        if (c->request_bytes_received == sizeof(c->request)) {
            printf("[**] Request header received\n");

            switch (c->request.cmd_id) {
            case CMD_REBOOT: c->cb = &reboot_cb; break;
            case CMD_WRITE:  c->cb = &write_cb;  break;
            case CMD_LAUNCH: c->cb = &launch_cb; break;
            default:
                // Error! Invalid header
                printf("[!!] Invalid header!\n");
                return -1;
            };

            status = c->cb->on_connect(c);
            if (status != CB_STATUS_OK) {
                printf("[!!] on_connect callback failed\n");
                return -1;
            }
        }

        return amount_to_copy;
    } else {
        // We have the header, continue to read rest of request data
        size_t amount_request_remaining = c->request.length - c->request_bytes_received;
        if (amount_to_copy > amount_request_remaining) {
            amount_to_copy = amount_request_remaining;
        }

        c->request_bytes_received += amount_to_copy;
        c->total_bytes_received += amount_to_copy;

        status = c->cb->on_data(c, data, amount_to_copy);
        if (status != CB_STATUS_OK) {
            printf("[!!] on_data callback failed\n");
            return -1;
        }

        if (c->request_bytes_received == c->request.length) {
            // Completed reading request data
            status = c->cb->on_close(c);
            if (status != CB_STATUS_OK) {
                printf("[!!] on_close callback failed\n");
                return -1;
            }

            respond_ok(c->conn);

            // Zero out request_bytes_received to force new request creation
            c->request_bytes_received = 0;
            c->cb = NULL;
        }

        return amount_to_copy;
    }
}

static void handle_connection(struct netconn *conn)
{
    struct connection c;
    struct netbuf *inbuf;
    char *buf;
    u16_t buflen;
    err_t err;
    cb_status status;

    memset(&c, 0, sizeof(c));
    c.conn = conn;
    netconn_peer(conn, &c.naddr, &c.port);
    printf("[**] New connection from %s\n", ip4addr_ntoa(ip_2_ip4(&c.naddr)));

    int num_packets_received = 0;
    while (1) {
        //
        // Recieve data from remote
        //
        err = netconn_recv(conn, &inbuf);
        if (err != ERR_OK) {
            printf("Error receiving\n");
            goto cleanup;
        }

        // printf("[>>] Packet %d\n", num_packets_received++);

        //
        // Read all parts of the packet
        //
        int parts = 0;
        while (1) {
            buflen = 0;
            netbuf_data(inbuf, (void**)&buf, &buflen);
            // printf("[->] Part %d (%dB)\n", parts, buflen);
            parts++;

            size_t bytes_processed = 0;
            while (bytes_processed < buflen) {
                int result = data_received(&c, buf+bytes_processed, buflen-bytes_processed);
                if (result < 0) {
                    printf("[!!] An error occured. Closing connection\n");
                    goto cleanup;
                } else {
                    bytes_processed += result;
                }
            }

            int nb_status = netbuf_next(inbuf);
            if (nb_status < 0) break;
        }

        netbuf_delete(inbuf);
        inbuf = NULL;
    }

cleanup:
    if (c.cb) {
        printf("[..] Running on_close on cleanup\n");
        c.cb->on_close(&c);
    }
    printf("[**] Closed connection\n");
    netconn_close(conn);
    if (inbuf) netbuf_delete(inbuf);
}

int attempt = 0;

static int start_connection()
{
    int status = 0;

    // Alert the server that we have come online
    struct netconn *conn = netconn_new(NETCONN_TCP);

    if (conn == NULL) {
        printf("Failed to allocate conn\n");
        return -1;
    }

#if 0
    ip_addr_t addr;
#if !USE_DHCP
    // IP_ADDR4(&addr,192,168,1,4);
    ip_addr_set_ip4_u32(&addr, agent_config.cs_ip);
#else
    IP_ADDR4(&addr,10,0,2,2);
#endif
#endif
    printf("[**] Attempting to contact central server at %s (attempt #%d)..\n", ip4addr_ntoa(ip_2_ip4(&central_server_ip)), attempt++);

    err_t err;
    err = netconn_connect(conn, &central_server_ip, 6666);
    if (err != ERR_OK) {
        printf("[!!] Failed to connect\n");
        status = -1;
        goto cleanup;
    }

    // Announce to server
    struct {
        char msg[32];
        char serial[12];
    } announce;

    memset(&announce, 0, sizeof(announce));

    #define EEPROM_INDEX_MACADDR    0x101
    ULONG type;
    ULONG len;
    ExQueryNonVolatileSetting(0x100, &type, announce.serial, 12, &len);
    snprintf(announce.msg, 32, "Hello!\n");

    err = netconn_write(conn, &announce, sizeof(announce), NETCONN_COPY);
    if (err != ERR_OK) {
        printf("[!!] Failed to write to central server!\n");
        status = -1;
        goto cleanup;
    }

    printf("[**] Notified server, waiting for requests\n");

    status = 0;
    handle_connection(conn);

cleanup:
    if (conn) {
        netconn_close(conn);
        netconn_delete(conn);
    }

    return status;
}

static void serve()
{
    while (1) {
        start_connection();
        XSleep(2500);
    }
}

static void tcpip_init_done(void *arg)
{
	sys_sem_t *init_complete = arg;
	sys_sem_signal(init_complete);
}

static void packet_timer(void *arg)
{
  LWIP_UNUSED_ARG(arg);
  Pktdrv_ReceivePackets();
  sys_timeout(PKT_TMR_INTERVAL, packet_timer, NULL);
}

void main(void)
{
	sys_sem_t init_complete;
	const ip4_addr_t *ip;
	static ip4_addr_t ipaddr, netmask, gw;

#if DEBUGGING
	asm volatile ("jmp .");
	debug_flags = LWIP_DBG_ON;
#else
	debug_flags = 0;
#endif

#if USE_DHCP
	IP4_ADDR(&gw, 0,0,0,0);
	IP4_ADDR(&ipaddr, 0,0,0,0);
	IP4_ADDR(&netmask, 0,0,0,0);
#else
	IP4_ADDR(&gw, 192,168,1,1);
	IP4_ADDR(&ipaddr, 192,168,1,2);
	IP4_ADDR(&netmask, 255,255,255,0);
#endif

	/* Initialize the TCP/IP stack. Wait for completion. */
	sys_sem_new(&init_complete, 0);
	tcpip_init(tcpip_init_done, &init_complete);
	sys_sem_wait(&init_complete);
	sys_sem_free(&init_complete);

	pb_init();
	pb_show_debug_screen();

#if !USE_DHCP
    load_agent_config();
    ip_addr_set_ip4_u32(&xbox_ip, agent_config.ip_addr);
    memcpy(&ipaddr, &xbox_ip, sizeof(xbox_ip));
    ip_addr_set_ip4_u32(&netmask, agent_config.ip_mask);
    ip_addr_set_ip4_u32(&gw, agent_config.ip_gw);
#endif

	g_pnetif = netif_add(&nforce_netif, &ipaddr, &netmask, &gw,
	                     NULL, nforceif_init, ethernet_input);
	if (!g_pnetif) {
		debugPrint("netif_add failed\n");
		return;
	}

	netif_set_default(g_pnetif);
	netif_set_up(g_pnetif);

#if USE_DHCP
	dhcp_start(g_pnetif);
#endif

	packet_timer(NULL);

#if USE_DHCP
	debugPrint("Waiting for DHCP...\n");
	while (g_pnetif->dhcp->state != DHCP_STATE_BOUND)
		NtYieldExecution();
	debugPrint("DHCP bound!\n");
#endif

    // Calculate central server
    int team = ip4_addr3(netif_ip4_addr(g_pnetif));
    assert(team >= 100);
    team -= 100;
    debugPrint("Team = %d\n", team);
    IP4_ADDR(&central_server_ip, 10,13,37,team);

    debugPrint("\n");
    debugPrint("IP address....... %s\n", ip4addr_ntoa(netif_ip4_addr(g_pnetif)));
    debugPrint("Mask............. %s\n", ip4addr_ntoa(netif_ip4_netmask(g_pnetif)));
    debugPrint("Gateway.......... %s\n", ip4addr_ntoa(netif_ip4_gw(g_pnetif)));
    debugPrint("Central Server... %s\n", ip4addr_ntoa(&central_server_ip));
    debugPrint("\n");

	serve();
	Pktdrv_Quit();
	return;
}
