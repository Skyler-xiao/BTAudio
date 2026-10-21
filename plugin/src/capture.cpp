/*
 *   System audio capture for the Bluetooth audio plugin
 *
 *   The audio engine (sndcore2) calls a "final mix callback" for every device (TV, GamePad) every 3 ms
 *   with the finished mix, 32 bit samples with 16 bit range, one pointer per channel. Applications may
 *   register their own callback to change the mix, so we register ours instead and call theirs first.
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 2 of the License, or
 *   (at your option) any later version.
 */

#include "capture.h"

#include <coreinit/dynload.h>
#include <coreinit/time.h>
#include <sndcore2/core.h>
#include <sndcore2/device.h>
#include <wups.h>

#include <stdio.h>
#include <string.h>

namespace {

constexpr size_t RING_FRAMES = 16384; // power of two, about 340 ms
constexpr int16_t SILENCE_LEVEL = 24;

// layout of the callback parameter (matches the console's sndcore2)
struct FinalMixParam {
    int32_t **data;
    uint16_t numChannelInput;
    uint16_t numSamples;
    uint16_t numDevices;
    uint16_t numChannelOutput;
};

struct Ring {
    int16_t samples[RING_FRAMES * 2];
    volatile uint32_t head; // frames written
    volatile uint32_t tail; // frames read
    volatile OSTime lastLoud;
    volatile uint32_t rate; // the engine runs at 48 or 32 kHz, depending on the application
};

Ring sRings[2]; // 0 = TV, 1 = GamePad
volatile bool sActive = false;

uint32_t sCalls[2] = {0, 0};
uint32_t sPushed[2] = {0, 0};
uint32_t sInstallCount = 0;
uint32_t sRegisterCalls = 0;
uint16_t sLastChannels = 0;
uint16_t sLastSamples = 0;

using RegisterFn = AXResult (*)(AXDeviceType, AXDeviceFinalMixCallback);

// The console has two audio engines with the same functions: games and the system menu use "snd_core",
// homebrew built with wut (for example media players using SDL) uses "sndcore2". Each is hooked on its own.
struct Engine {
    const char *library;
    OSDynLoad_Module module;
    int (*isInit)();
    int (*getFinalMix)(int, AXDeviceFinalMixCallback *);
    RegisterFn *realRegister; // filled in by the function patcher
    AXDeviceFinalMixCallback appCallback[2];
    bool installed;
};

} // namespace

AXResult (*real_AXRegisterDeviceFinalMixCallback)(AXDeviceType, AXDeviceFinalMixCallback) __attribute__((section(".data")));
AXResult (*real_AXRegisterDeviceFinalMixCallback2)(AXDeviceType, AXDeviceFinalMixCallback) __attribute__((section(".data")));
AXResult (*real_AXRegisterDeviceFinalMixCallbackHome)(AXDeviceType, AXDeviceFinalMixCallback) __attribute__((section(".data")));

namespace {

// The HOME Menu is a process of its own with its own snd_core. Its engine is hooked right after its AXInit,
// it runs while the application is in the background.
constexpr int ENGINE_HOME = 2;

Engine sEngines[3] = {
    {"snd_core", nullptr, nullptr, nullptr, &real_AXRegisterDeviceFinalMixCallback, {nullptr, nullptr}, false},
    {"sndcore2", nullptr, nullptr, nullptr, &real_AXRegisterDeviceFinalMixCallback2, {nullptr, nullptr}, false},
    {"snd_core", nullptr, nullptr, nullptr, &real_AXRegisterDeviceFinalMixCallbackHome, {nullptr, nullptr}, false},
};

uint32_t sHomeInits = 0;
uint32_t sHomeCalls = 0;
volatile OSTime sHomeLastCall = 0;

// while the HOME Menu plays, the application's engine must not write into the rings as well
bool homeMenuPlaying() {
    return sHomeLastCall && OSTicksToMilliseconds(OSGetTime() - sHomeLastCall) < 50;
}

void resolveEngine(Engine &e) {
    if (e.isInit) {
        return;
    }
    if (OSDynLoad_Acquire(e.library, &e.module) != OS_DYNLOAD_OK) {
        return;
    }
    OSDynLoad_FindExport(e.module, OS_DYNLOAD_EXPORT_FUNC, "AXGetDeviceFinalMixCallback", (void **) &e.getFinalMix);
    OSDynLoad_FindExport(e.module, OS_DYNLOAD_EXPORT_FUNC, "AXIsInit", (void **) &e.isInit);
}

bool engineRunning(Engine &e) {
    resolveEngine(e);
    return e.isInit && e.isInit();
}

inline int16_t clamp16(int32_t v) {
    return v > 32767 ? 32767 : (v < -32768 ? -32768 : (int16_t) v);
}

void pushFrames(Ring &ring, const FinalMixParam *p) {
    if (p) {
        sLastChannels = p->numChannelInput;
        sLastSamples  = p->numSamples;
        // one call covers 3 ms: 144 samples = 48 kHz, 96 samples = 32 kHz
        if (p->numSamples) {
            ring.rate = (uint32_t) p->numSamples * 1000 / 3;
        }
    }
    if (!p || !p->data || p->numChannelInput < 2 || !p->data[0] || !p->data[1]) {
        return;
    }

    uint32_t head = ring.head;
    uint32_t free = RING_FRAMES - (head - ring.tail);
    uint32_t count = p->numSamples;
    if (count > free) {
        count = free; // the reader is not keeping up, drop the rest
    }

    bool loud = false;
    for (uint32_t i = 0; i < count; i++) {
        int16_t l = clamp16(p->data[0][i]);
        int16_t r = clamp16(p->data[1][i]);
        if (l > SILENCE_LEVEL || l < -SILENCE_LEVEL || r > SILENCE_LEVEL || r < -SILENCE_LEVEL) {
            loud = true;
        }
        size_t idx = ((head + i) & (RING_FRAMES - 1)) * 2;
        ring.samples[idx]     = l;
        ring.samples[idx + 1] = r;
    }

    if (loud) {
        ring.lastLoud = OSGetTime();
    }

    __sync_synchronize();
    ring.head = head + count;
}

template <int ENGINE, int DEVICE>
void mixCallback(void *param) {
    sCalls[DEVICE]++;
    AXDeviceFinalMixCallback app = sEngines[ENGINE].appCallback[DEVICE];
    if (app) {
        app(param);
    }
    if (ENGINE == ENGINE_HOME) {
        sHomeCalls++;
        sHomeLastCall = OSGetTime();
    } else if (homeMenuPlaying()) {
        return;
    }
    if (sActive) {
        sPushed[DEVICE]++;
        pushFrames(sRings[DEVICE], (const FinalMixParam *) param);
    }
}

const AXDeviceFinalMixCallback sOurCallbacks[3][2] = {
    {mixCallback<0, 0>, mixCallback<0, 1>},
    {mixCallback<1, 0>, mixCallback<1, 1>},
    {mixCallback<2, 0>, mixCallback<2, 1>},
};

AXResult registerHook(Engine &e, AXDeviceType type, AXDeviceFinalMixCallback func) {
    sRegisterCalls++;
    // remember the application's callback; while ours is registered it calls theirs
    if (type == AX_DEVICE_TYPE_TV || type == AX_DEVICE_TYPE_DRC) {
        e.appCallback[type == AX_DEVICE_TYPE_TV ? 0 : 1] = func;
        if (e.installed) {
            return 0;
        }
    }
    return (*e.realRegister)(type, func);
}

// only called from the plugin's worker thread, never from inside the audio engine's own start
void installCallbacks(int index) {
    Engine &e = sEngines[index];
    if (e.installed || !*e.realRegister) {
        return;
    }

    AXDeviceFinalMixCallback existing = nullptr;
    if (e.getFinalMix && e.getFinalMix(AX_DEVICE_TYPE_TV, &existing) == 0 && existing) {
        e.appCallback[0] = existing;
    }
    existing = nullptr;
    if (e.getFinalMix && e.getFinalMix(AX_DEVICE_TYPE_DRC, &existing) == 0 && existing) {
        e.appCallback[1] = existing;
    }

    e.installed = true;
    (*e.realRegister)(AX_DEVICE_TYPE_TV, sOurCallbacks[index][0]);
    (*e.realRegister)(AX_DEVICE_TYPE_DRC, sOurCallbacks[index][1]);
    sInstallCount++;
}

void removeCallbacks(Engine &e) {
    if (!e.installed) {
        return;
    }
    e.installed = false;
    (*e.realRegister)(AX_DEVICE_TYPE_TV, e.appCallback[0]);
    (*e.realRegister)(AX_DEVICE_TYPE_DRC, e.appCallback[1]);
}

void installWhereRunning() {
    for (int i = 0; i < 2; i++) {
        if (!sEngines[i].installed && engineRunning(sEngines[i])) {
            installCallbacks(i);
        }
    }
}

void forgetEngine(Engine &e) {
    // the engine forgets all callbacks
    e.installed = false;
    e.appCallback[0] = e.appCallback[1] = nullptr;
}

} // namespace

// written by hand instead of DECL_FUNCTION, the real functions live in the engine table above
AXResult my_AXRegisterDeviceFinalMixCallback(AXDeviceType type, AXDeviceFinalMixCallback func) {
    return registerHook(sEngines[0], type, func);
}
AXResult my_AXRegisterDeviceFinalMixCallback2(AXDeviceType type, AXDeviceFinalMixCallback func) {
    return registerHook(sEngines[1], type, func);
}
AXResult my_AXRegisterDeviceFinalMixCallbackHome(AXDeviceType type, AXDeviceFinalMixCallback func) {
    return registerHook(sEngines[ENGINE_HOME], type, func);
}

// HOME Menu: the engine is ready once its AXInit returned, then our callbacks go in (no OSDynLoad here)
void homeEngineStarted() {
    sHomeInits++;
    Engine &e = sEngines[ENGINE_HOME];
    e.appCallback[0] = e.appCallback[1] = nullptr;
    e.installed = false;
    installCallbacks(ENGINE_HOME);
}

DECL_FUNCTION(void, AXInitHome) {
    real_AXInitHome();
    homeEngineStarted();
}

DECL_FUNCTION(void, AXInitWithParamsHome, void *params) {
    real_AXInitWithParamsHome(params);
    homeEngineStarted();
}

DECL_FUNCTION(void, AXQuitHome) {
    forgetEngine(sEngines[ENGINE_HOME]);
    sHomeLastCall = 0;
    real_AXQuitHome();
}

DECL_FUNCTION(void, AXQuit) {
    forgetEngine(sEngines[0]);
    real_AXQuit();
}

DECL_FUNCTION(void, AXQuit2) {
    forgetEngine(sEngines[1]);
    real_AXQuit2();
}

WUPS_MUST_REPLACE(AXRegisterDeviceFinalMixCallback, WUPS_LOADER_LIBRARY_SND_CORE, AXRegisterDeviceFinalMixCallback);
WUPS_MUST_REPLACE(AXQuit, WUPS_LOADER_LIBRARY_SND_CORE, AXQuit);
WUPS_MUST_REPLACE(AXRegisterDeviceFinalMixCallback2, WUPS_LOADER_LIBRARY_SNDCORE2, AXRegisterDeviceFinalMixCallback);
WUPS_MUST_REPLACE(AXQuit2, WUPS_LOADER_LIBRARY_SNDCORE2, AXQuit);
WUPS_MUST_REPLACE_FOR_PROCESS(AXRegisterDeviceFinalMixCallbackHome, WUPS_LOADER_LIBRARY_SND_CORE,
                              AXRegisterDeviceFinalMixCallback, WUPS_FP_TARGET_PROCESS_HOME_MENU);
WUPS_MUST_REPLACE_FOR_PROCESS(AXInitHome, WUPS_LOADER_LIBRARY_SND_CORE, AXInit, WUPS_FP_TARGET_PROCESS_HOME_MENU);
WUPS_MUST_REPLACE_FOR_PROCESS(AXInitWithParamsHome, WUPS_LOADER_LIBRARY_SND_CORE, AXInitWithParams,
                              WUPS_FP_TARGET_PROCESS_HOME_MENU);
WUPS_MUST_REPLACE_FOR_PROCESS(AXQuitHome, WUPS_LOADER_LIBRARY_SND_CORE, AXQuit, WUPS_FP_TARGET_PROCESS_HOME_MENU);

namespace capture {

void setActive(bool active) {
    if (active) {
        installWhereRunning();
    } else {
        removeCallbacks(sEngines[0]);
        removeCallbacks(sEngines[1]);
    }
    if (active && !sActive) {
        for (auto &ring : sRings) {
            ring.tail = ring.head;
        }
    }
    sActive = active;
}

volatile int sSource = SOURCE_DRC;

static Ring &source() {
    if (sSource == SOURCE_TV) {
        return sRings[0];
    }
    if (sSource == SOURCE_DRC) {
        return sRings[1];
    }
    // automatic: the TV mix unless it has been silent for a while and the GamePad is playing
    OSTime now   = OSGetTime();
    OSTime limit = OSSecondsToTicks(2);
    bool tvLoud  = (now - sRings[0].lastLoud) < limit;
    bool drcLoud = (now - sRings[1].lastLoud) < limit;
    return (!tvLoud && drcLoud) ? sRings[1] : sRings[0];
}

void setSource(int source) {
    sSource = source;
}

uint32_t sampleRate() {
    uint32_t rate = source().rate;
    return rate ? rate : SAMPLE_RATE;
}

size_t available() {
    Ring &ring = source();
    return ring.head - ring.tail;
}

size_t read(int16_t *out, size_t maxFrames) {
    Ring &ring   = source();
    uint32_t tail = ring.tail;
    size_t count = ring.head - tail;
    if (count > maxFrames) {
        count = maxFrames;
    }

    __sync_synchronize();
    for (size_t i = 0; i < count; i++) {
        size_t idx   = ((tail + i) & (RING_FRAMES - 1)) * 2;
        out[i * 2]     = ring.samples[idx];
        out[i * 2 + 1] = ring.samples[idx + 1];
    }
    ring.tail = tail + count;

    // the other device is not used, drop what it collected
    Ring &other = (&ring == &sRings[0]) ? sRings[1] : sRings[0];
    other.tail  = other.head;
    return count;
}

void poll() {
    // an engine started after the connection: hook in as soon as it exists
    if (sActive) {
        installWhereRunning();
    }
}

void trim(size_t keepFrames) {
    Ring &ring = source();
    size_t have = ring.head - ring.tail;
    if (have > keepFrames) {
        ring.tail = ring.tail + (have - keepFrames);
    }
}

} // namespace capture

namespace capture {

bool homeMenuActive() {
    return homeMenuPlaying();
}

void homeStats(uint32_t *inits, uint32_t *calls, bool *installed) {
    *inits     = sHomeInits;
    *calls     = sHomeCalls;
    *installed = sEngines[ENGINE_HOME].installed;
}

void describe(char *out, size_t size) {
    snprintf(out, size, "snd_core %d/%d, sndcore2 %d/%d (running/hooked), x%u, calls tv %u drc %u, pushed tv %u drc %u, app regs %u, ch %u smp %u",
             (int) engineRunning(sEngines[0]), (int) sEngines[0].installed, (int) engineRunning(sEngines[1]),
             (int) sEngines[1].installed, (unsigned) sInstallCount, (unsigned) sCalls[0],
             (unsigned) sCalls[1], (unsigned) sPushed[0], (unsigned) sPushed[1], (unsigned) sRegisterCalls,
             (unsigned) sLastChannels, (unsigned) sLastSamples);
}

} // namespace capture

namespace capture {

void reset() {
    // a new application: nothing of the old one is valid any more
    for (auto &e : sEngines) {
        forgetEngine(e);
        e.module      = nullptr;
        e.isInit      = nullptr;
        e.getFinalMix = nullptr;
    }
    sActive = false;
}

} // namespace capture
