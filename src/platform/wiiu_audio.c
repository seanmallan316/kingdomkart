// wiiu_audio.c - plays the synthesized sound through the Wii U sound core (AX): two looping 16-bit voices (left, right)
// that the 3 ms audio frame callback keeps topped up from sfx_render().
#include "wiiu_audio.h"
#include "../audio/sfx.h"
#include "../render/gfx.h"
#include <sndcore2/core.h>
#include <sndcore2/voice.h>
#include <sndcore2/device.h>
#include <coreinit/cache.h>
#include <malloc.h>
#include <string.h>

#define RING 8192                  /* samples per channel (about a quarter second at 32 kHz) */
#define LEAD 1024                  /* how far ahead of the play position we keep the buffer filled */
static AXVoice *vL, *vR;
static int16_t *bufL, *bufR;
static uint32_t write_pos;
static int running;

static void frame_cb(void) {
    if (!running) return;
    AXVoiceOffsets off; AXGetVoiceOffsets(vL, &off);
    uint32_t play = off.currentOffset % RING;
    uint32_t ahead = (write_pos + RING - play) % RING;      /* samples already written and not yet played */
    if (ahead >= LEAD) return;
    uint32_t need = LEAD - ahead;
    static int16_t tl[LEAD], tr[LEAD];
    sfx_render(tl, tr, (int)need);
    for (uint32_t i = 0; i < need; i++) { uint32_t p = (write_pos + i) % RING; bufL[p] = tl[i]; bufR[p] = tr[i]; }
    write_pos = (write_pos + need) % RING;
    DCFlushRange(bufL, RING * sizeof(int16_t)); DCFlushRange(bufR, RING * sizeof(int16_t));
}

static void setup_voice(AXVoice *v, int16_t *buf, int right) {
    AXVoiceBegin(v);
    AXSetVoiceType(v, 0);
    AXVoiceOffsets off; memset(&off, 0, sizeof off);
    off.dataType = AX_VOICE_FORMAT_LPCM16; off.loopingEnabled = AX_VOICE_LOOP_ENABLED;
    off.loopOffset = 0; off.endOffset = RING - 1; off.currentOffset = 0; off.data = buf;
    AXSetVoiceOffsets(v, &off);
    AXSetVoiceSrcType(v, AX_VOICE_SRC_TYPE_NONE); AXSetVoiceSrcRatio(v, 1.0f);
    AXVoiceVeData ve = {0x8000, 0}; AXSetVoiceVe(v, &ve);
    AXVoiceDeviceMixData mix[6]; memset(mix, 0, sizeof mix);
    mix[right ? 1 : 0].bus[0].volume = 0x8000;
    AXSetVoiceDeviceMix(v, AX_DEVICE_TYPE_TV, 0, mix);
    AXSetVoiceDeviceMix(v, AX_DEVICE_TYPE_DRC, 0, mix);
    AXSetVoiceCurrentOffset(v, 0);
    AXSetVoiceState(v, AX_VOICE_STATE_PLAYING);
    AXVoiceEnd(v);
}

int wiiu_audio_start(void) {
    AXInitParams params; memset(&params, 0, sizeof params);
    params.renderer = AX_INIT_RENDERER_32KHZ; params.pipeline = AX_INIT_PIPELINE_SINGLE;
    AXInitWithParams(&params);
    sfx_init(32000);
    bufL = memalign(64, RING * sizeof(int16_t)); bufR = memalign(64, RING * sizeof(int16_t));
    if (!bufL || !bufR) { gfx_log("audio: no memory"); return 0; }
    memset(bufL, 0, RING * sizeof(int16_t)); memset(bufR, 0, RING * sizeof(int16_t));
    DCFlushRange(bufL, RING * sizeof(int16_t)); DCFlushRange(bufR, RING * sizeof(int16_t));
    vL = AXAcquireVoice(31, NULL, NULL); vR = AXAcquireVoice(31, NULL, NULL);
    if (!vL || !vR) { gfx_log("audio: could not get two voices"); return 0; }
    /* start a little ahead so the first frame has something to play */
    static int16_t tl[LEAD], tr[LEAD]; sfx_render(tl, tr, LEAD);
    memcpy(bufL, tl, sizeof tl); memcpy(bufR, tr, sizeof tr); write_pos = LEAD;
    DCFlushRange(bufL, RING * sizeof(int16_t)); DCFlushRange(bufR, RING * sizeof(int16_t));
    setup_voice(vL, bufL, 0); setup_voice(vR, bufR, 1);
    running = 1;
    AXRegisterAppFrameCallback(frame_cb);
    gfx_log("audio: started (%u samples per frame)", AXGetInputSamplesPerFrame());
    return 1;
}

void wiiu_audio_stop(void) {
    if (!running) return;
    running = 0;
    AXDeregisterAppFrameCallback(frame_cb);
    AXVoiceBegin(vL); AXSetVoiceState(vL, AX_VOICE_STATE_STOPPED); AXVoiceEnd(vL);
    AXVoiceBegin(vR); AXSetVoiceState(vR, AX_VOICE_STATE_STOPPED); AXVoiceEnd(vR);
    AXFreeVoice(vL); AXFreeVoice(vR);
    AXQuit();
}
