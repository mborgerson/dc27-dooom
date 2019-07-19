#include "lwip/debug.h"
#include "lwip/dhcp.h"
#include "lwip/init.h"
#include "lwip/netif.h"
#include "lwip/sys.h"
#include "lwip/tcpip.h"
#include "lwip/timers.h"
#include "netif/etharp.h"
#include "pktdrv.h"

#undef printf // FIXME: I #defined this to debugPrint but now it conflicts

#include <hal/input.h>
#include <hal/xbox.h>
#include <pbkit/pbkit.h>
#include <xboxkrnl/xboxkrnl.h>
#include <debug.h>
#include <string.h>
#include <lwip/opt.h>
#include <lwip/arch.h>
#include <lwip/api.h>
#include <stdio.h>
#include <assert.h>
#include <assert.h>
#include <stdlib.h>

#define USE_DHCP 1
#define PKT_TMR_INTERVAL 1

int check__2 = 0;
static void drm_check_failed(void)
{
    check__2 = 1;
}

#include "drm.h"

struct netif nforce_netif, *g_pnetif;
err_t nforceif_init(struct netif *netif);
int hacky_watchdog_timer_start;
int match_connected = 0;
ip4_addr_t central_server_ip;
char xbox_ip_str[16];
char central_server_ip_str[16];

static void packet_timer(void *arg)
{
  LWIP_UNUSED_ARG(arg);
  Pktdrv_ReceivePackets();
  sys_timeout(PKT_TMR_INTERVAL, packet_timer, NULL);

    int since_start = (XGetTickCount()-hacky_watchdog_timer_start) / 1000;
    // debugPrint("since start = %d\n", since_start);
    if (since_start > 15) {
        if (!match_connected) {
            XReboot();
        } else {
            // debugPrint("match was connected\n"); 
            return;
        }
    }

}

static void tcpip_init_done(void *arg)
{
    sys_sem_t *init_complete = arg;
    sys_sem_signal(init_complete);
}

int init_if(void)
{
    sys_sem_t init_complete;
    const ip4_addr_t *ip;
    static ip4_addr_t ipaddr, netmask, gw;

#if DEBUGGING
    // asm volatile ("jmp .");
    debug_flags = LWIP_DBG_ON;
#else
    debug_flags = 0;
#endif

#if USE_DHCP
    IP4_ADDR(&gw, 0,0,0,0);
    IP4_ADDR(&ipaddr, 0,0,0,0);
    IP4_ADDR(&netmask, 0,0,0,0);
#else
    IP4_ADDR(&gw, 10,0,2,1);
    IP4_ADDR(&ipaddr, 10,0,2,10);
    IP4_ADDR(&netmask, 255,255,255,0);
#endif

    /* Initialize the TCP/IP stack. Wait for completion. */
    sys_sem_new(&init_complete, 0);
    tcpip_init(tcpip_init_done, &init_complete);
    sys_sem_wait(&init_complete);
    sys_sem_free(&init_complete);

    g_pnetif = netif_add(&nforce_netif, &ipaddr, &netmask, &gw,
                         NULL, nforceif_init, ethernet_input);
    assert(g_pnetif != NULL);

    netif_set_default(g_pnetif);
    netif_set_up(g_pnetif);

#if USE_DHCP
    dhcp_start(g_pnetif);
#endif

    hacky_watchdog_timer_start = XGetTickCount();
    packet_timer(NULL);

#if USE_DHCP
    debugPrint("Waiting for DHCP...\n");
    while (g_pnetif->dhcp->state != DHCP_STATE_BOUND) {
        NtYieldExecution();
    }
    debugPrint("DHCP bound!\n");
#endif

    check_cpuid();

#if USE_DHCP
    // Calculate central server
    int team = ip4_addr3(netif_ip4_addr(g_pnetif));
    assert(team >= 100);
    team -= 100;
    debugPrint("Team = %d\n", team);
    IP4_ADDR(&central_server_ip, 10,13,37,team);
#endif

    debugPrint("\n");
    debugPrint("IP address....... %s\n", ip4addr_ntoa(netif_ip4_addr(g_pnetif)));
    strcpy(xbox_ip_str, ip4addr_ntoa(netif_ip4_addr(g_pnetif)));
    debugPrint("Mask............. %s\n", ip4addr_ntoa(netif_ip4_netmask(g_pnetif)));
    debugPrint("Gateway.......... %s\n", ip4addr_ntoa(netif_ip4_gw(g_pnetif)));
    strcpy(central_server_ip_str, ip4addr_ntoa(&central_server_ip));
    debugPrint("Central Server... %s\n", central_server_ip_str);
    debugPrint("\n");

#if !USE_DHCP
    strcpy(central_server_ip_str, "10.0.2.2");
#endif

    debugPrint("< will reboot in 10 seconds if match is not ready >\n");

    return 0;
}
