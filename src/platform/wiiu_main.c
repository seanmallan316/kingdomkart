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
#include "../game/save.h"
#include "wiiu_audio.h"

void gfx_set_logfile(FILE *f);

static void read_pads(Pad *pads) {
    memset(pads, 0, sizeof(Pad) * KK_PADS);
    Pad *p = &pads[0];
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
        if (vs.tpNormal.touched && vs.tpNormal.validity == VPAD_VALID) {   /* the touch screen, in the same 1280 x 720 units the menus are laid out in */
            VPADTouchData tp; VPADGetTPCalibratedPointEx(VPAD_CHAN_0, VPAD_TP_1280X720, &tp, &vs.tpNormal);
            p->touch = 1; p->tx = tp.x; p->ty = tp.y; }
    }
    /* the Wii controllers, one slot each: a Pro Controller, a Classic Controller, a Wii Remote held sideways, or a Remote with a Nunchuk */
    for (int ch = 0; ch < 4; ch++) {
        Pad *w = &pads[1 + ch]; KPADStatus ks; KPADError ke = KPAD_ERROR_OK;
        if (KPADReadEx((WPADChan)ch, &ks, 1, &ke) <= 0 || ke != KPAD_ERROR_OK) continue;
        if (ks.extensionType == WPAD_EXT_PRO_CONTROLLER) {
            w->sx = ks.pro.leftStick.x; w->sy = ks.pro.leftStick.y;
            if (ks.pro.hold & WPAD_PRO_BUTTON_LEFT) w->sx = -1;
            if (ks.pro.hold & WPAD_PRO_BUTTON_RIGHT) w->sx = 1;
            if (ks.pro.hold & WPAD_PRO_BUTTON_UP) w->sy = 1;
            if (ks.pro.hold & WPAD_PRO_BUTTON_DOWN) w->sy = -1;
            w->a = !!(ks.pro.hold & WPAD_PRO_BUTTON_A); w->b = !!(ks.pro.hold & WPAD_PRO_BUTTON_B);
            w->x = !!(ks.pro.hold & WPAD_PRO_BUTTON_X); w->y = !!(ks.pro.hold & WPAD_PRO_BUTTON_Y);
            w->zl = !!(ks.pro.hold & (WPAD_PRO_TRIGGER_ZL | WPAD_PRO_TRIGGER_L)); w->zr = !!(ks.pro.hold & (WPAD_PRO_TRIGGER_ZR | WPAD_PRO_TRIGGER_R));
            w->plus = !!(ks.pro.trigger & WPAD_PRO_BUTTON_PLUS);
            w->source = 2;
        } else if (ks.extensionType == WPAD_EXT_CLASSIC || ks.extensionType == WPAD_EXT_MPLUS_CLASSIC) {
            w->sx = ks.classic.leftStick.x; w->sy = ks.classic.leftStick.y;
            if (ks.classic.hold & WPAD_CLASSIC_BUTTON_LEFT) w->sx = -1;
            if (ks.classic.hold & WPAD_CLASSIC_BUTTON_RIGHT) w->sx = 1;
            if (ks.classic.hold & WPAD_CLASSIC_BUTTON_UP) w->sy = 1;
            if (ks.classic.hold & WPAD_CLASSIC_BUTTON_DOWN) w->sy = -1;
            w->a = !!(ks.classic.hold & WPAD_CLASSIC_BUTTON_A); w->b = !!(ks.classic.hold & WPAD_CLASSIC_BUTTON_B);
            w->x = !!(ks.classic.hold & WPAD_CLASSIC_BUTTON_X); w->y = !!(ks.classic.hold & WPAD_CLASSIC_BUTTON_Y);
            w->zl = !!(ks.classic.hold & (WPAD_CLASSIC_BUTTON_ZL | WPAD_CLASSIC_BUTTON_L)); w->zr = !!(ks.classic.hold & (WPAD_CLASSIC_BUTTON_ZR | WPAD_CLASSIC_BUTTON_R));
            w->plus = !!(ks.classic.trigger & WPAD_CLASSIC_BUTTON_PLUS);
            w->source = 8;
        } else if (ks.extensionType == WPAD_EXT_NUNCHUK || ks.extensionType == WPAD_EXT_MPLUS_NUNCHUK) {
            /* Remote + Nunchuk: stick steers, A gas, B brake, Z drift, C uses the blessing */
            w->sx = ks.nunchuk.stick.x; w->sy = ks.nunchuk.stick.y;
            w->a = !!(ks.hold & WPAD_BUTTON_A); w->b = !!(ks.hold & WPAD_BUTTON_B);
            w->zr = !!(ks.nunchuk.hold & WPAD_NUNCHUK_BUTTON_Z); w->zl = !!(ks.nunchuk.hold & WPAD_NUNCHUK_BUTTON_C);
            w->x = !!(ks.hold & WPAD_BUTTON_1); w->y = !!(ks.hold & WPAD_BUTTON_2);
            w->plus = !!(ks.trigger & WPAD_BUTTON_PLUS);
            w->source = 16;
        } else if (ks.extensionType == WPAD_EXT_CORE || ks.extensionType == WPAD_EXT_MPLUS) {
            /* a Wii Remote held sideways: the d-pad's up/down become left/right, 2 is gas, 1 is brake, A uses the blessing, B (the trigger) drifts */
            if (ks.hold & WPAD_BUTTON_UP) w->sx = -1;
            if (ks.hold & WPAD_BUTTON_DOWN) w->sx = 1;
            if (ks.hold & WPAD_BUTTON_RIGHT) w->sy = 1;
            if (ks.hold & WPAD_BUTTON_LEFT) w->sy = -1;
            w->a = !!(ks.hold & WPAD_BUTTON_2); w->b = !!(ks.hold & WPAD_BUTTON_1);
            w->zl = !!(ks.hold & WPAD_BUTTON_A); w->zr = !!(ks.hold & WPAD_BUTTON_B);
            w->plus = !!(ks.trigger & WPAD_BUTTON_PLUS);
            w->source = 4;
        } else continue;
        w->sx = clampf(w->sx, -1, 1); w->sy = clampf(w->sy, -1, 1);
        if (fabsf(w->sx) < 0.12f) w->sx = 0;
        if (fabsf(w->sy) < 0.12f) w->sy = 0;
    }
    p->sx = clampf(p->sx, -1, 1); p->sy = clampf(p->sy, -1, 1);
    if (fabsf(p->sx) < 0.12f) p->sx = 0;
    if (fabsf(p->sy) < 0.12f) p->sy = 0;
}

/* vibration: the GamePad takes a bit pattern (each bit is 1/120 s), the Wii controllers only on/off, so a timer stops them */
static float g_rumble_left[KK_PADS]; static int g_wii_rumbling[4];
void platform_rumble_pad(int slot, float strength, float seconds) {
    if (strength <= 0 || seconds <= 0 || slot < 0 || slot >= KK_PADS) return;
    if (slot == 0) {
        uint8_t pattern[15]; int len = (int)(seconds * 15.0f); if (len < 1) len = 1; if (len > 15) len = 15;
        uint8_t byte = strength > 0.75f ? 0xFF : (strength > 0.45f ? 0xEE : (strength > 0.25f ? 0xAA : 0x88));
        memset(pattern, byte, sizeof pattern);
        VPADControlMotor(VPAD_CHAN_0, pattern, (uint8_t)len);
    } else if (strength > 0.3f) { WPADControlMotor((WPADChan)(slot - 1), TRUE); g_wii_rumbling[slot - 1] = 1; }
    if (seconds > g_rumble_left[slot]) g_rumble_left[slot] = seconds;
}
void platform_rumble(float strength, float seconds) { platform_rumble_pad(0, strength, seconds); platform_rumble_pad(1, strength, seconds); }   /* player 1 alone: the GamePad and the first Wii controller both shake */
static void rumble_tick(float dt) {
    for (int s = 0; s < KK_PADS; s++) if (g_rumble_left[s] > 0) { g_rumble_left[s] -= dt; if (g_rumble_left[s] <= 0 && s > 0 && g_wii_rumbling[s - 1]) { WPADControlMotor((WPADChan)(s - 1), FALSE); g_wii_rumbling[s - 1] = 0; } }
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    WHBLogCafeInit(); WHBLogUdpInit();
    WHBProcInit();
    FILE *logfile = NULL;
    if (WHBMountSdCard()) {
        char path[256]; snprintf(path, sizeof path, "%s/wiiu/kingdomkart_log.txt", WHBGetSdCardMountPath());
        logfile = fopen(path, "w"); gfx_set_logfile(logfile);
        snprintf(path, sizeof path, "%s/wiiu/kingdomkart_save.txt", WHBGetSdCardMountPath()); save_set_path(path);
    }
    gfx_log("Kingdom Kart %s starting", KK_VERSION);
    KPADInit(); WPADEnableURCC(TRUE);
    int status = gfx_init();
    gfx_log("graphics status %d (0 = ok)", status);
    game_init(status);
    wiiu_audio_start();

    OSTime last = OSGetTime(); uint64_t frames = 0; double acc_ms = 0, worst_ms = 0; Pad pads[KK_PADS]; memset(pads, 0, sizeof pads);
    while (WHBProcIsRunning()) {
        OSTime now = OSGetTime();
        float dt = (float)OSTicksToMicroseconds(now - last) / 1000000.0f; last = now;
        if (dt > 0.1f) dt = 0.1f;
        frames++; acc_ms += dt * 1000.0; if (dt * 1000.0 > worst_ms) worst_ms = dt * 1000.0;
        if (frames % 300 == 0) { gfx_log("frame %llu: avg %.2f ms, worst %.2f ms, controllers %d/%d/%d/%d/%d", (unsigned long long)frames, acc_ms / 300.0, worst_ms, pads[0].source, pads[1].source, pads[2].source, pads[3].source, pads[4].source); acc_ms = 0; worst_ms = 0; }
        read_pads(pads);
        game_frame(pads, KK_PADS, dt);
        rumble_tick(dt);
    }
    gfx_log("shutting down after %llu frames", (unsigned long long)frames);
    VPADStopMotor(VPAD_CHAN_0); for (int ch = 0; ch < 4; ch++) if (g_wii_rumbling[ch]) WPADControlMotor((WPADChan)ch, FALSE);
    wiiu_audio_stop();
    game_shutdown();
    gfx_shutdown();
    KPADShutdown();
    if (logfile) fclose(logfile);
    WHBUnmountSdCard();
    WHBProcShutdown();
    WHBLogUdpDeinit(); WHBLogCafeDeinit();
    return 0;
}
