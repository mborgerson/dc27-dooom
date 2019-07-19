#include <xboxrt/debug.h>
#include <pbkit/pbkit.h>
#include <hal/xbox.h>
#include <hal/video.h>
#include "stdio.h"
#include "string.h"
#include <SDL.h>
#include <assert.h>

int drm_check_failed_1 = 0;
static void drm_check_failed(void)
{
    drm_check_failed_1 = 1;
}
#include "drm.h"

extern int  myargc;
extern char **myargv;
extern char central_server_ip_str[16];
void D_DoomMain (void);

void main(void)
{
    if (pb_init() != 0)
    {
        XSleep(2000);
        XReboot();
        return;
    }

    init_if();
    check_eeprom();

    int argc = 9;
    char *argv[] = {
      "dcdoom",
      "-iwad", "freedm.wad",
      "-nomonsters",
      "-deathmatch",
      "-connect", central_server_ip_str,
      "-port", "2342",
      "",
    };

    myargc = argc;
    myargv = argv;

    pb_show_debug_screen();

    debugPrint("WELCOME TO DOOOM!\n");
    check_cpuid();

    D_DoomMain();

    printf("DOOM has exited.\n");
    while (1);

    pb_kill();
    XReboot();
}
