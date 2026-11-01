// Kingdom Kart: Road to Glory - Wii U entry point. Sets up the console, reads controllers, and runs the game loop.
//
// Screen colors if something is wrong at startup:
//   solid ORANGE = glslcompiler.rpl was not found in sd:/wiiu/libs/
//   solid RED    = the shader compiler was found but our shaders failed to compile (see sd:/wiiu/kingdomkart_log.txt)
#include <coreinit/time.h>
#include <coreinit/systeminfo.h>
#include <padscore/kpad.h>
#include <padscore/wpad.h>
#include <vpad/input.h>
#include <whb/log.h>
#include <whb/log_cafe.h>
#include <whb/log_udp.h>
#include <whb/proc.h>
#include <whb/sdcard.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "../render/gfx.h"
#include "../game/game.h"

#define KK_VERSION "M5-0.1"
void gfx_set_logfile(FILE *f);

static void read_pad(Pad *p) {
    memset(p, 0, sizeof *p);
    VPADStatus vs; VPADReadError ve = VPAD_READ_SUCCESS;
    if (VPADRead(VPAD_CHAN_0, &vs, 1, &ve) > 0 && ve == VPAD_READ_SUCCESS) {
        p->sx = vs.leftStick.x; p->sy = vs.leftStick.y;
        if (vs.hold & VPAD_BUTTON_LEFT) p->sx = -1;
        if (vs.hold & VPAD_BUTTON_RIGHT) p->sx = 1;
        if (vs.hold & VPAD_BUTTON_UP) p->sy = 1;
        if (vs.hold & VPAD_BUTTON_DOWN) p->sy = -1;
        p->a = !!(vs.hold & VPAD_BUTTON_A); p->b = !!(vs.hold & VPAD_BUTTON_B);
        p->x = !!(vs.hold & VPAD_BUTTON_X); p->y = !!(vs.hold & VPAD_BUTTON_Y);
        p->zl = !!(vs.hold & (VPAD_BUTTON_ZL | VPAD_BUTTON_L)); p->zr = !!(vs.hold & (VPAD_BUTTON_ZR | VPAD_BUTTON_R));
        p->plus = !!(vs.trigger & VPAD_BUTTON_PLUS);
        p->source = 1;
    }
    KPADStatus ks; KPADError ke = KPAD_ERROR_OK;
    if (KPADReadEx(WPAD_CHAN_0, &ks, 1, &ke) > 0 && ke == KPAD_ERROR_OK && ks.extensionType == WPAD_EXT_PRO_CONTROLLER) {
        if (fabsf(ks.pro.leftStick.x) > 0.1f) p->sx = ks.pro.leftStick.x;
        if (fabsf(ks.pro.leftStick.y) > 0.1f) p->sy = ks.pro.leftStick.y;
        if (ks.pro.hold & WPAD_PRO_BUTTON_LEFT) p->sx = -1;
        if (ks.pro.hold & WPAD_PRO_BUTTON_RIGHT) p->sx = 1;
        if (ks.pro.hold & WPAD_PRO_BUTTON_UP) p->sy = 1;
        if (ks.pro.hold & WPAD_PRO_BUTTON_DOWN) p->sy = -1;
        p->a |= !!(ks.pro.hold & WPAD_PRO_BUTTON_A); p->b |= !!(ks.pro.hold & WPAD_PRO_BUTTON_B);
        p->x |= !!(ks.pro.hold & WPAD_PRO_BUTTON_X); p->y |= !!(ks.pro.hold & WPAD_PRO_BUTTON_Y);
        p->zl |= !!(ks.pro.hold & (WPAD_PRO_TRIGGER_ZL | WPAD_PRO_TRIGGER_L)); p->zr |= !!(ks.pro.hold & (WPAD_PRO_TRIGGER_ZR | WPAD_PRO_TRIGGER_R));
        p->plus |= !!(ks.pro.trigger & WPAD_PRO_BUTTON_PLUS);
        p->source |= 2;
    }
    p->sx = clampf(p->sx, -1, 1); p->sy = clampf(p->sy, -1, 1);
    if (fabsf(p->sx) < 0.12f) p->sx = 0;
    if (fabsf(p->sy) < 0.12f) p->sy = 0;
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    WHBLogCafeInit(); WHBLogUdpInit();
    WHBProcInit();
    FILE *logfile = NULL;
    if (WHBMountSdCard()) {
        char path[256]; snprintf(path, sizeof path, "%s/wiiu/kingdomkart_log.txt", WHBGetSdCardMountPath());
        logfile = fopen(path, "w"); gfx_set_logfile(logfile);
    }
    gfx_log("Kingdom Kart %s starting", KK_VERSION);
    KPADInit(); WPADEnableURCC(TRUE);
    int status = gfx_init();
    gfx_log("graphics status %d (0 = ok)", status);
    game_init(status);

    OSTime last = OSGetTime(); uint64_t frames = 0; double acc_ms = 0, worst_ms = 0; Pad pad;
    while (WHBProcIsRunning()) {
        OSTime now = OSGetTime();
        float dt = (float)OSTicksToMicroseconds(now - last) / 1000000.0f; last = now;
        if (dt > 0.1f) dt = 0.1f;
        frames++; acc_ms += dt * 1000.0; if (dt * 1000.0 > worst_ms) worst_ms = dt * 1000.0;
        if (frames % 300 == 0) { gfx_log("frame %llu: avg %.2f ms, worst %.2f ms, controller %d", (unsigned long long)frames, acc_ms / 300.0, worst_ms, pad.source); acc_ms = 0; worst_ms = 0; }
        read_pad(&pad);
        game_frame(&pad, dt);
    }
    gfx_log("shutting down after %llu frames", (unsigned long long)frames);
    game_shutdown();
    gfx_shutdown();
    KPADShutdown();
    if (logfile) fclose(logfile);
    WHBUnmountSdCard();
    WHBProcShutdown();
    WHBLogUdpDeinit(); WHBLogCafeDeinit();
    return 0;
}
