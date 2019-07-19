#ifndef DRM_H
#define DRM_H

#include <xboxrt/debug.h>
#include <pbkit/pbkit.h>
#include <hal/xbox.h>
#include <hal/video.h>
#include "stdio.h"
#include "string.h"
#include <SDL.h>
#include <assert.h>

#define EEPROM_INDEX_MACADDR    0x101

// #include <hal/led.h>
// void drm_check_failed(void)
// {
//   XSetCustomLED(XLED_RED, XLED_OFF, XLED_RED, XLED_OFF);
//   // debugPrint("FAILED\n");
//   // while(1);
// }

/*
 * Emulator check #1: Check the NIC MAC address stored in the EEPROM
 * to determine if its Bunnie's EEPROM, which is used by XQEMU by default.
 */
static void check_eeprom(void)
{
    ULONG type;
    unsigned char ethaddr[6];
    ULONG len;

    // debugPrint("Checking MAC adress...");
    ExQueryNonVolatileSetting(EEPROM_INDEX_MACADDR,&type,ethaddr,6,&len);
    assert(len == 6);

    unsigned char bunnies_mac[6] = "\x00\x50\xf2\x4f\x65\x52";
    if (memcmp(ethaddr, bunnies_mac, 6) == 0) {
        drm_check_failed();
    }
}


static void do_cpuid(int eax_in, int *a, int *b, int *c, int *d)
{
    int _a, _b, _c, _d;
    asm volatile (
      "cpuid;"
      /* Outputs  */ : "=a" (_a), "=b" (_b), "=c" (_c), "=d" (_d)
      /* Inputs   */ : "a" (eax_in)
      /* Clobbers */ : /* None */
      );

    if (a) *a = _a;
    if (b) *b = _b;
    if (c) *c = _c;
    if (d) *d = _d;
}

static void check_cpuid(void)
{
    int a, b, c, d;

    // debugPrint("Checking CPUID...");
    do_cpuid(1, &a, &b, &c, &d);

    // Model: a=00000673, b=00000800 // xqemu
    // a=68a, b=0 // xbox

    if (a == 0x673 && b == 0x800) {
      drm_check_failed();
    }
}

#endif
