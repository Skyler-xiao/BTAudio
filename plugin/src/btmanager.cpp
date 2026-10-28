/*
 *   Connection manager of the Bluetooth audio plugin
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 2 of the License, or
 *   (at your option) any later version.
 */

#include "btmanager.h"
#include "capture.h"
#include "i18n.h"

#include <notifications/notifications.h>

#include <bloopair/bloopair.h>
#include <sbc.h>

#include <coreinit/mutex.h>
#include <coreinit/thread.h>
#include <coreinit/time.h>
#include <coreinit/dynload.h>
#include <wups.h>
#include <wups/storage.h>

#include <malloc.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

namespace {

constexpr size_t THREAD_STACK_SIZE = 0x8000;
constexpr uint32_t MAX_PENDING_FRAMES = 8; // more only adds delay, the IOS side has its own queue

enum class CmdType { Scan, Connect, Pair, Disconnect, Reset, Toggle, Reconnect };
struct Cmd {
    CmdType type;
    size_t index;
};

OSMutex sLock;
bool sLockInit = false;

bool sEnabled = true;
int sVolume   = 60;
int sSource   = capture::SOURCE_DRC;
int sQuality  = btmgr::QUALITY_MEDIUM;
bool sLowLatency = false;
bool sBattery    = false;        // read the battery over the hands-free profile (test feature)
volatile int sBatteryPercent = -1; // last reported level, -1 = not known
bool sLowBatteryNotified = false;  // once until it was charged again
uint8_t sLastAddr[6] = {}; // device of the last connection, for the button combo
int sUsedQuality  = -1;     // settings the running connection was made with
bool sUsedBattery = false;
std::vector<SavedDevice> sSaved;
std::vector<FoundDevice> sFound;
std::vector<Cmd> sCmds;
i18n::Msg sStatusMsg = i18n::S_READY;
std::string sStatusArg;
std::string sCurrentName;

OSThread *sThread = nullptr;
uint8_t *sStack   = nullptr;
volatile bool sRun = false;

struct Lock {
    Lock() {
        if (!sLockInit) {
            OSInitMutex(&sLock);
            sLockInit = true;
        }
        OSLockMutex(&sLock);
    }
    ~Lock() { OSUnlockMutex(&sLock); }
};

// small log file on the SD card, for finding problems (the menu has no room for details).
// Off by default, the settings choose which topics are written.
bool sLogEnabled       = false;
uint32_t sLogTopics    = btmgr::LOG_ALL;
int sStatsInterval     = 60; // seconds between audio statistics, 0 = none

constexpr const char *LOG_PATH     = "/vol/external01/wiiu/btaudio/plugin.txt";
constexpr const char *LOG_OLD_PATH = "/vol/external01/wiiu/btaudio/plugin_old.txt";
constexpr long LOG_MAX_SIZE        = 64 * 1024;
constexpr size_t LOG_BUFFER_MAX    = 16 * 1024;
constexpr int LOG_START_QUIET_MS   = 8000; // no SD access while an application is starting
constexpr int LOG_FLUSH_MS         = 3000;

// Lines are collected in memory and written by the background thread now and then. Writing to the SD card right
// while an application starts could hang the console (seen together with SDCafiine and the FTP plugin).
OSMutex sLogLock;
bool sLogLockInit = false;
std::string sLogBuffer;
bool sLogOverflow = false;
OSTime sAppStart  = 0;
OSTime sLastFlush = 0;

struct LogLock {
    LogLock() {
        if (!sLogLockInit) {
            OSInitMutex(&sLogLock);
            sLogLockInit = true;
        }
        OSLockMutex(&sLogLock);
    }
    ~LogLock() { OSUnlockMutex(&sLogLock); }
};

void logLine(uint32_t topic, const char *format, ...) {
    if (!sLogEnabled || (topic != 0 && !(sLogTopics & topic))) {
        return;
    }

    char line[512];
    int len = snprintf(line, sizeof(line), "[%u s] ", (unsigned) OSTicksToSeconds(OSGetTime()));
    va_list args;
    va_start(args, format);
    vsnprintf(line + len, sizeof(line) - len, format, args);
    va_end(args);

    LogLock l;
    if (sLogBuffer.size() + strlen(line) + 1 > LOG_BUFFER_MAX) {
        sLogOverflow = true; // the card was not written for a long time, keep the oldest lines
        return;
    }
    sLogBuffer += line;
    sLogBuffer += '\n';
}

// called by the background thread only. While a connection is being made it writes at once: if the console
// crashes there, the last lines are what is needed.
void flushLog(bool urgent = false) {
    OSTime now = OSGetTime();
    if (OSTicksToMilliseconds(now - sAppStart) < LOG_START_QUIET_MS ||
        (!urgent && OSTicksToMilliseconds(now - sLastFlush) < LOG_FLUSH_MS)) {
        return;
    }
    sLastFlush = now;

    std::string text;
    {
        LogLock l;
        if (sLogBuffer.empty()) {
            return;
        }
        text.swap(sLogBuffer);
        if (sLogOverflow) {
            text += "(log lines lost, too many at once)\n";
            sLogOverflow = false;
        }
    }

    mkdir("/vol/external01/wiiu/btaudio", 0777);

    // keep the file small: when it is full it becomes plugin_old.txt and a new one starts
    struct stat info;
    if (stat(LOG_PATH, &info) == 0 && info.st_size > LOG_MAX_SIZE) {
        remove(LOG_OLD_PATH);
        rename(LOG_PATH, LOG_OLD_PATH);
    }
    FILE *f = fopen(LOG_PATH, "a");
    if (!f) {
        return;
    }
    fwrite(text.data(), 1, text.size(), f);
    fclose(f);
}

// the status is kept as message + device name, so the menu shows it in the language in use
void setStatus(i18n::Msg msg, const std::string &arg = "") {
    {
        Lock l;
        sStatusMsg = msg;
        sStatusArg = arg;
    }
    bool scan = msg == i18n::S_SCAN_BUSY || msg == i18n::S_SCANNING || msg == i18n::S_SCAN_UNAVAILABLE ||
                msg == i18n::S_SCAN_NONE || msg == i18n::S_SCAN_DONE;
    logLine(scan ? btmgr::LOG_SCAN : btmgr::LOG_CONNECTION, "%s", i18n::formatEnglish(msg, arg.c_str()).c_str());
}

// short message in the corner of the screen (Aroma's notification module), only for things worth knowing
bool sNotifyReady = false;
uint8_t sNotifiedState = BLOOPAIR_AUDIO_STATE_IDLE; // survives application changes, so a switch does not repeat it

void notify(const std::string &text, bool error = false) {
    if (!sNotifyReady) {
        return;
    }
    std::string message = i18n::tr(i18n::N_PREFIX) + text;
    if (error) {
        NotificationModule_AddErrorNotification(message.c_str());
    } else {
        NotificationModule_AddInfoNotification(message.c_str());
    }
}

void notifyState(uint8_t state, const std::string &name, bool pairing) {
    if (state == sNotifiedState) {
        return;
    }
    switch (state) {
        case BLOOPAIR_AUDIO_STATE_STREAMING:
            notify(i18n::format(i18n::N_CONNECTED, name.c_str()));
            break;
        case BLOOPAIR_AUDIO_STATE_FAILED:
            notify(i18n::tr(pairing ? i18n::N_PAIR_FAILED : i18n::N_CONNECT_FAILED), true);
            break;
        case BLOOPAIR_AUDIO_STATE_CLOSED:
            if (sNotifiedState == BLOOPAIR_AUDIO_STATE_STREAMING) {
                notify(i18n::tr(i18n::S_DISCONNECTED));
            }
            break;
        default:
            return; // in-between states are not shown
    }
    sNotifiedState = state;
}

void saveDevices() {
    std::vector<uint8_t> blob((const uint8_t *) sSaved.data(), (const uint8_t *) (sSaved.data() + sSaved.size()));
    WUPSStorageAPI::Store<std::vector<uint8_t>>("devices", blob);
    WUPSStorageAPI::SaveStorage();
}

// ------------------------------------------------------------------------------------------
// encoder: capture ring -> volume / resampling -> SBC -> IOS side
// ------------------------------------------------------------------------------------------
class Streamer {
public:
    bool running = false;

    bool start(const BloopairAudioStatus &st) {
        static const uint32_t rates[] = {16000, 32000, 44100, 48000};
        static const uint8_t blocksEnum[] = {4, 8, 12, 16};

        if (st.sbcFrequency > 3 || st.sbcMode == SBC_MODE_MONO) {
            return false;
        }

        sbc_init(&mSbc, 0);
        mSbc.frequency  = st.sbcFrequency;
        mSbc.mode       = st.sbcMode;
        mSbc.subbands   = st.sbcSubbands == 4 ? SBC_SB_4 : SBC_SB_8;
        mSbc.allocation = st.sbcAllocation;
        mSbc.bitpool    = st.sbcBitpool;
        mSbc.endian     = SBC_BE;
        mSbc.blocks     = SBC_BLK_16;
        for (int i = 0; i < 4; i++) {
            if (blocksEnum[i] == st.sbcBlocks) {
                mSbc.blocks = i;
            }
        }

        mRate            = rates[st.sbcFrequency];
        mFrameLength     = sbc_get_frame_length(&mSbc);
        mCodeSize        = sbc_get_codesize(&mSbc);
        mSamplesPerFrame = st.sbcBlocks * st.sbcSubbands;
        if (mFrameLength != st.sbcFrameLength) {
            return false;
        }

        mInRate = capture::sampleRate();
        mStep   = (double) mInRate / (double) mRate;
        mPos    = 0.0;
        mFifo.clear();
        mPending.clear();
        mGain   = 0.0f;
        running = true;
        return true;
    }

    void stop() {
        running = false;
        mFifo.clear();
        mPending.clear();
    }

    void pump(IOSHandle handle, int volumePercent) {
        if (!running) {
            return;
        }

        // the application may run the engine at another rate (for example 32 kHz)
        uint32_t inRate = capture::sampleRate();
        if (inRate != mInRate) {
            mInRate = inRate;
            mStep   = (double) mInRate / (double) mRate;
            mPos    = 0.0;
            mFifo.clear();
            logLine(btmgr::LOG_APP, "capture rate %u Hz", (unsigned) mInRate);
        }

        // never let the delay grow: keep about 100 ms in the ring (40 ms with low latency). In the HOME Menu the
        // application is in the background and this thread runs less evenly, so it always keeps the larger buffer.
        bool small = sLowLatency && !capture::homeMenuActive();
        capture::trim(small ? mInRate / 25 : mInRate / 10);

        float target = (float) volumePercent / 100.0f;
        target       = target * target;

        std::vector<int16_t> &pcm = mPcm;
        std::vector<uint8_t> &out = mOut;
        pcm.resize(mSamplesPerFrame * 2);
        out.resize(mFrameLength + 16);

        uint32_t maxPending = small ? MAX_PENDING_FRAMES / 2 : MAX_PENDING_FRAMES;
        while (mPending.size() < maxPending * mFrameLength) {
            size_t needed = (size_t) (mPos + mStep * (double) mSamplesPerFrame) + 3;
            while (mFifo.size() / 2 < needed) {
                int16_t tmp[576 * 2];
                size_t n = capture::read(tmp, 576);
                if (n == 0) {
                    break;
                }
                mFifo.insert(mFifo.end(), tmp, tmp + n * 2);
            }
            if (mFifo.size() / 2 < needed) {
                break;
            }

            for (uint32_t i = 0; i < mSamplesPerFrame; i++) {
                double p   = mPos + mStep * (double) i;
                size_t idx = (size_t) p;
                float frac = (float) (p - (double) idx);
                float l = mFifo[idx * 2];
                float r = mFifo[idx * 2 + 1];
                if (frac > 0.0f) {
                    l += (mFifo[idx * 2 + 2] - l) * frac;
                    r += (mFifo[idx * 2 + 3] - r) * frac;
                }
                mGain += (target - mGain) * 0.0005f;
                l *= mGain;
                r *= mGain;
                pcm[i * 2]     = (int16_t) (l > 32767.0f ? 32767.0f : (l < -32768.0f ? -32768.0f : l));
                pcm[i * 2 + 1] = (int16_t) (r > 32767.0f ? 32767.0f : (r < -32768.0f ? -32768.0f : r));
            }

            mPos += mStep * (double) mSamplesPerFrame;
            size_t used = (size_t) mPos;
            mPos -= (double) used;
            mFifo.erase(mFifo.begin(), mFifo.begin() + used * 2);

            ssize_t written = 0;
            ssize_t consumed = sbc_encode(&mSbc, pcm.data(), mCodeSize, out.data(), out.size(), &written);
            if (consumed < 0 || (uint32_t) written != mFrameLength) {
                break;
            }
            mPending.insert(mPending.end(), out.begin(), out.begin() + written);
        }

        // hand over as much as the IOS side takes
        while (!mPending.empty()) {
            uint32_t frames = (uint32_t) (mPending.size() / mFrameLength);
            uint32_t maxFrames = BLOOPAIR_AUDIO_WRITE_MAX / mFrameLength;
            if (frames > maxFrames) {
                frames = maxFrames;
            }
            if (frames == 0) {
                break;
            }

            IOSError res = Bloopair_AudioWrite(handle, mPending.data(), frames * mFrameLength);
            if (res <= 0) {
                break;
            }
            mPending.erase(mPending.begin(), mPending.begin() + (size_t) res * mFrameLength);
            if ((uint32_t) res < frames) {
                break;
            }
        }
    }

private:
    sbc_t mSbc;
    uint32_t mRate = 48000;
    uint32_t mInRate = 48000;
    uint32_t mFrameLength = 0;
    uint32_t mCodeSize = 0;
    uint32_t mSamplesPerFrame = 0;
    double mStep = 1.0;
    double mPos = 0.0;
    float mGain = 0.0f;
    std::vector<int16_t> mFifo;
    std::vector<uint8_t> mPending;
    std::vector<int16_t> mPcm;
    std::vector<uint8_t> mOut;
};

// ------------------------------------------------------------------------------------------
// background thread
// ------------------------------------------------------------------------------------------
struct Worker {
    IOSHandle handle = -1;
    bool active = false;
    Streamer streamer;
    BloopairAudioStatus st;
    OSTime lastPoll = 0;
    OSTime lastLoop = 0;          // diagnostics: end of the previous loop pass
    bool connecting = false;      // we asked for a connection and wait for the result
    OSTime connectStart = 0;
    bool pairing = false;         // the connection includes a pairing, the key must be stored
    SavedDevice target;
    bool hasTarget = false;
    uint8_t lastState = 0xFF;
    OSTime ignoreUntil = 0;       // right after a request the status still shows the old state
    OSTime lastStats = 0;
    uint32_t lastCongestions = 0; // already reported congestions of the link
    uint8_t logShown = 0;         // entries of the IOS event log already written
    uint8_t logGeneration = 0;
    uint8_t lastHfpState = 0;
    uint8_t lastBattery  = 0xFF;
    OSTime lastCongestionLog = 0;
    bool reconnect = false;       // connect again as soon as the old connection is closed
};

bool isBusy(uint8_t state) {
    return state == BLOOPAIR_AUDIO_STATE_PAIRING || state == BLOOPAIR_AUDIO_STATE_CONNECTING ||
           state == BLOOPAIR_AUDIO_STATE_CONFIGURING || state == BLOOPAIR_AUDIO_STATE_OPEN ||
           state == BLOOPAIR_AUDIO_STATE_STREAMING;
}

// the device of the last connection, or the first saved one
bool lastDevice(SavedDevice &dev) {
    Lock l;
    for (const auto &d : sSaved) {
        if (memcmp(d.addr, sLastAddr, 6) == 0) {
            dev = d;
            return true;
        }
    }
    if (!sSaved.empty()) {
        dev = sSaved[0];
        return true;
    }
    return false;
}

void startConnect(Worker &w, const SavedDevice &dev, bool pair);

void poll(Worker &w) {
    if (OSGetTime() < w.ignoreUntil) {
        return;
    }

    if (Bloopair_GetAudioStatus(w.handle, &w.st) < 0) {
        return;
    }

    capture::poll();

    if (w.st.state == BLOOPAIR_AUDIO_STATE_STREAMING) {
        if (!w.streamer.running) {
            if (w.streamer.start(w.st)) {
                capture::setActive(true);
            } else {
                setStatus(i18n::S_CODEC_BAD);
                Bloopair_AudioDisconnect(w.handle);
            }
        }

        // a new pairing finished: remember the device and its key
        if (w.pairing && w.st.keyValid && w.hasTarget) {
            memcpy(w.target.key, w.st.linkKey, 16);
            w.target.keyType = w.st.keyType;
            {
                Lock l;
                bool found = false;
                for (auto &d : sSaved) {
                    if (memcmp(d.addr, w.target.addr, 6) == 0) {
                        d = w.target;
                        found = true;
                    }
                }
                if (!found) {
                    sSaved.push_back(w.target);
                }
                saveDevices();
            }
            notify(i18n::format(i18n::N_PAIRED, w.target.name));
            w.pairing = false;
        }
        w.connecting = false;
    } else if (w.streamer.running) {
        w.streamer.stop();
        capture::setActive(false);
    }

    if (sStatsInterval > 0 && w.st.state == BLOOPAIR_AUDIO_STATE_STREAMING) {
        if (OSTicksToSeconds(OSGetTime() - w.lastStats) >= (uint64_t) sStatsInterval) {
            w.lastStats = OSGetTime();
            logLine(btmgr::LOG_AUDIO,
                    "stream: %u frames sent, %u gaps, %u congestions (%u ms in total, longest %u ms), %u frames "
                    "dropped, queue %u/%u, capture ring %u frames",
                    (unsigned) w.st.framesSent, (unsigned) w.st.underruns, (unsigned) w.st.congestions,
                    (unsigned) w.st.congestedMs, (unsigned) w.st.maxCongestedMs, (unsigned) w.st.droppedFrames,
                    (unsigned) w.st.ringFrames, (unsigned) w.st.ringCapacity, (unsigned) capture::available());

            uint32_t homeInits, homeCalls;
            bool homeHooked;
            capture::homeStats(&homeInits, &homeCalls, &homeHooked);
            if (homeInits || homeCalls) {
                logLine(btmgr::LOG_AUDIO, "home menu: %u starts, %u mix calls, hooked %d", (unsigned) homeInits,
                        (unsigned) homeCalls, (int) homeHooked);
            }
        }

        // the radio link could not keep up (distance, walls, wifi): report it when it happens, at most once a second
        if (w.st.congestions < w.lastCongestions) {
            w.lastCongestions = 0; // a new stream started
        }
        if (w.st.congestions != w.lastCongestions && OSTicksToMilliseconds(OSGetTime() - w.lastCongestionLog) >= 1000) {
            w.lastCongestionLog = OSGetTime();
            logLine(btmgr::LOG_AUDIO, "radio congested %u times (total %u), longest %u ms, queue %u/%u",
                    (unsigned) (w.st.congestions - w.lastCongestions), (unsigned) w.st.congestions,
                    (unsigned) w.st.maxCongestedMs, (unsigned) w.st.ringFrames, (unsigned) w.st.ringCapacity);
            w.lastCongestions = w.st.congestions;
        }
    }

    // battery over the hands-free profile
    if (w.st.hfpState != w.lastHfpState) {
        logLine(btmgr::LOG_CONNECTION, "hands-free: state %u, detail 0x%02x, channel %u", (unsigned) w.st.hfpState,
                (unsigned) w.st.hfpDetail, (unsigned) w.st.hfpChannel);
        w.lastHfpState = w.st.hfpState;
    }
    if (w.st.battery != w.lastBattery) {
        if (w.st.battery != 0xFF) {
            logLine(btmgr::LOG_CONNECTION, "battery %u %%", (unsigned) w.st.battery);
            if (w.st.battery <= 20 && !sLowBatteryNotified) {
                sLowBatteryNotified = true;
                notify(i18n::format(i18n::N_BATTERY_LOW, (int) w.st.battery), true);
            } else if (w.st.battery >= 40) {
                sLowBatteryNotified = false;
            }
        }
        w.lastBattery = w.st.battery;
    }
    sBatteryPercent = (w.st.state == BLOOPAIR_AUDIO_STATE_STREAMING && w.st.battery != 0xFF) ? w.st.battery : -1;

    // the Bluetooth side's own event log (pairing, HCI traffic) while a connection is made
    if (w.st.logCount < w.logShown || w.st.logGeneration != w.logGeneration) {
        w.logShown      = 0; // a new connection attempt (or the battery part) started a new log
        w.logGeneration = w.st.logGeneration;
    }
    while (w.logShown < w.st.logCount && w.logShown < BLOOPAIR_AUDIO_LOG_SIZE) {
        const BloopairAudioLogEntry &e = w.st.log[w.logShown++];
        logLine(btmgr::LOG_CONNECTION, "ios event %u value 0x%04x data 0x%08x", (unsigned) e.event, (unsigned) e.value,
                (unsigned) e.data);
    }

    if (w.st.state != w.lastState) {
        logLine(btmgr::LOG_CONNECTION, "IOS state %u, sbc freq %u mode %u bitpool %u frame %u", (unsigned) w.st.state,
                (unsigned) w.st.sbcFrequency, (unsigned) w.st.sbcMode, (unsigned) w.st.sbcBitpool, (unsigned) w.st.sbcFrameLength);
        w.lastState = w.st.state;
        std::string name;
        {
            Lock l;
            name = sCurrentName;
        }
        if (name.empty()) {
            // carried over from an earlier application: look the device up by its address
            Lock l;
            for (const auto &d : sSaved) {
                if (memcmp(d.addr, w.st.bd_address, 6) == 0) {
                    sCurrentName = d.name;
                    name = d.name;
                }
            }
        }
        switch (w.st.state) {
            case BLOOPAIR_AUDIO_STATE_STREAMING:
                setStatus(i18n::S_CONNECTED, name);
                notifyState(w.st.state, name, w.pairing);
                break;
            case BLOOPAIR_AUDIO_STATE_FAILED:
                setStatus(w.pairing ? i18n::S_PAIR_FAILED : i18n::S_CONNECT_FAILED);
                notifyState(w.st.state, name, w.pairing);
                w.connecting = false;
                w.pairing = false;
                break;
            case BLOOPAIR_AUDIO_STATE_CLOSED:
                w.connecting = false;
                if (w.reconnect) {
                    // new settings: connect again without "disconnected" / "connected" notifications
                    w.reconnect = false;
                    SavedDevice dev;
                    if (lastDevice(dev)) {
                        startConnect(w, dev, false);
                        break;
                    }
                }
                setStatus(i18n::S_DISCONNECTED);
                notifyState(w.st.state, name, w.pairing);
                break;
            case BLOOPAIR_AUDIO_STATE_IDLE:
                break;
            default:
                setStatus(w.pairing ? i18n::S_PAIRING : i18n::S_CONNECTING, name);
                break;
        }
    }

    // give up if a connection attempt takes far too long
    if (w.connecting && isBusy(w.st.state) && w.st.state != BLOOPAIR_AUDIO_STATE_STREAMING &&
        OSTicksToSeconds(OSGetTime() - w.connectStart) > 45) {
        Bloopair_AudioDisconnect(w.handle);
        w.connecting = false;
        setStatus(i18n::S_TIMEOUT);
        notify(i18n::tr(i18n::S_TIMEOUT), true);
        sNotifiedState = BLOOPAIR_AUDIO_STATE_FAILED;
    }
}

void runScan(Worker &w) {
    if (isBusy(w.st.state)) {
        setStatus(i18n::S_SCAN_BUSY);
        return;
    }

    setStatus(i18n::S_SCANNING);
    Bloopair_SetScanMode(w.handle, TRUE, TRUE);

    // same as pressing the SYNC button: the console searches for devices. padscore is looked up at run time so
    // the plugin also loads in applications which do not use it.
    OSDynLoad_Module padscore = nullptr;
    if (OSDynLoad_Acquire("padscore", &padscore) == OS_DYNLOAD_OK) {
        void (*startSync)() = nullptr;
        if (OSDynLoad_FindExport(padscore, OS_DYNLOAD_EXPORT_FUNC, "WPADStartSyncDevice", (void **) &startSync) ==
                OS_DYNLOAD_OK && startSync) {
            startSync();
        } else {
            setStatus(i18n::S_SCAN_UNAVAILABLE);
        }
    } else {
        setStatus(i18n::S_SCAN_UNAVAILABLE);
    }

    BloopairScanResults results;
    memset(&results, 0, sizeof(results));
    for (int i = 0; i < 40 && sRun; i++) {
        OSSleepTicks(OSMillisecondsToTicks(500));
        Bloopair_GetScanResults(w.handle, &results);
    }
    Bloopair_SetScanMode(w.handle, FALSE, FALSE);
    Bloopair_GetScanResults(w.handle, &results);

    std::vector<FoundDevice> found;
    for (uint32_t i = 0; i < results.count && i < BLOOPAIR_SCAN_MAX_RESULTS; i++) {
        const BloopairScanEntry &e = results.entries[i];
        if ((e.dev_class[1] & 0x1F) != 4) { // audio / video devices only
            continue;
        }
        FoundDevice d;
        memset(&d, 0, sizeof(d));
        memcpy(d.addr, e.bd_address, 6);
        if (e.flags & BLOOPAIR_SCAN_FLAG_NAME_KNOWN) {
            memcpy(d.name, e.name, sizeof(d.name) - 1);
        } else {
            snprintf(d.name, sizeof(d.name), "%02X:%02X:%02X:%02X:%02X:%02X", e.bd_address[0], e.bd_address[1],
                     e.bd_address[2], e.bd_address[3], e.bd_address[4], e.bd_address[5]);
        }
        d.rssi = e.rssi;
        found.push_back(d);
    }

    {
        Lock l;
        sFound = found;
    }
    for (const auto &d : found) {
        logLine(btmgr::LOG_SCAN, "found: %s (rssi %d)", d.name, (int) d.rssi);
    }
    notify(found.empty() ? std::string(i18n::tr(i18n::N_SCAN_NONE)) : i18n::format(i18n::N_SCAN_DONE, (int) found.size()));
    setStatus(found.empty() ? i18n::S_SCAN_NONE : i18n::S_SCAN_DONE);
}

void startConnect(Worker &w, const SavedDevice &dev, bool pair) {
    if (isBusy(w.st.state)) {
        setStatus(i18n::S_BUSY);
        return;
    }

    {
        Lock l;
        sCurrentName = dev.name;
        memcpy(sLastAddr, dev.addr, 6);
    }
    sUsedQuality = sQuality;
    sUsedBattery = sBattery;
    w.target     = dev;
    w.hasTarget  = true;
    w.pairing    = pair;
    w.connecting = true;
    w.connectStart = OSGetTime();
    w.lastState  = 0xFF;
    w.ignoreUntil = OSGetTime() + OSMillisecondsToTicks(600);
    setStatus(pair ? i18n::S_PAIRING : i18n::S_CONNECTING, dev.name);
    // lower quality = smaller SBC frames, fewer dropouts when the radio is weak
    static const uint8_t bitpools[] = {0, 30, 24};
    Bloopair_AudioConnect(w.handle, dev.addr, pair ? nullptr : dev.key, dev.keyType, bitpools[sQuality],
                          sBattery ? 1 : 0);
}

// everything back to how a fresh installation starts
void resetSettings() {
    {
        Lock l;
        sSaved.clear();
        sFound.clear();
        sVolume  = 60;
        sQuality = btmgr::QUALITY_MEDIUM;
        memset(sLastAddr, 0, sizeof(sLastAddr));
    }
    sLowLatency = false;
    sBattery    = false;
    sSource = capture::SOURCE_DRC;
    capture::setSource(sSource);
    sLogTopics     = btmgr::LOG_ALL;
    sStatsInterval = 60;
    sLogEnabled    = false;
    i18n::setOverride(i18n::LANG_AUTO);
    WUPSStorageAPI::WipeStorage();
    WUPSStorageAPI::SaveStorage();
}

void handleCommands(Worker &w) {
    std::vector<Cmd> cmds;
    {
        Lock l;
        cmds.swap(sCmds);
    }

    for (const Cmd &c : cmds) {
        switch (c.type) {
            case CmdType::Scan:
                runScan(w);
                break;
            case CmdType::Connect: {
                SavedDevice dev;
                {
                    Lock l;
                    if (c.index >= sSaved.size()) {
                        continue;
                    }
                    dev = sSaved[c.index];
                }
                startConnect(w, dev, false);
                break;
            }
            case CmdType::Pair: {
                FoundDevice f;
                {
                    Lock l;
                    if (c.index >= sFound.size()) {
                        continue;
                    }
                    f = sFound[c.index];
                }
                SavedDevice dev;
                memset(&dev, 0, sizeof(dev));
                memcpy(dev.addr, f.addr, 6);
                memcpy(dev.name, f.name, sizeof(dev.name) - 1); // both are 40 bytes, dev is zeroed
                startConnect(w, dev, true);
                break;
            }
            case CmdType::Disconnect:
                // a second press after the connection is already gone must not bring back "disconnecting"
                if (!isBusy(w.st.state)) {
                    break;
                }
                setStatus(i18n::S_DISCONNECTING);
                Bloopair_AudioDisconnect(w.handle);
                break;
            case CmdType::Toggle: {
                // button combo: disconnect a running connection, otherwise connect to the last used device
                if (isBusy(w.st.state)) {
                    setStatus(i18n::S_DISCONNECTING);
                    Bloopair_AudioDisconnect(w.handle);
                    break;
                }
                SavedDevice dev;
                if (lastDevice(dev)) {
                    startConnect(w, dev, false);
                }
                break;
            }
            case CmdType::Reconnect:
                if (w.st.state == BLOOPAIR_AUDIO_STATE_STREAMING) {
                    w.reconnect = true;
                    Bloopair_AudioDisconnect(w.handle);
                }
                break;
            case CmdType::Reset:
                if (isBusy(w.st.state)) {
                    Bloopair_AudioDisconnect(w.handle);
                }
                resetSettings();
                setStatus(i18n::S_RESET);
                break;
        }
    }
}

// the Bluetooth module answers, but is it ours? An update of Bloopair (App Store) replaces it with one without audio.
bool sModuleReplaced  = false;
bool sReplacedNotified = false; // once per console start, not on every application change

bool becameActive(Worker &w) {
    BloopairAudioStatus probe;
    if (Bloopair_GetAudioStatus(w.handle, &probe) < 0) {
        sModuleReplaced = true;
        logLine(btmgr::LOG_APP, "Bloopair module without audio support (replaced by an update?)");
        setStatus(i18n::S_MODULE_REPLACED);
        if (!sReplacedNotified) {
            sReplacedNotified = true;
            notify(i18n::tr(i18n::N_MODULE_REPLACED), true);
        }
        return false;
    }
    sModuleReplaced = false;

    // an earlier application may already have a running connection, then we just carry on and keep its status
    if (sNotifiedState != BLOOPAIR_AUDIO_STATE_STREAMING) {
        setStatus(i18n::S_READY);
    }
    poll(w);
    return true;
}

int threadMain(int, const char **) {
    Worker w;
    memset(&w.st, 0, sizeof(w.st));

    w.handle = Bloopair_Open();
    w.active = w.handle >= 0 && Bloopair_IsActive(w.handle);
    logLine(btmgr::LOG_APP, "application started, Bloopair handle %d, active %d", (int) w.handle, (int) w.active);
    OSTime start      = OSGetTime();
    OSTime lastRetry  = start;
    bool notActiveSet = false;
    if (w.active && !becameActive(w)) {
        w.active     = false;
        notActiveSet = true; // nothing to wait for, the module is just not ours
    }
    while (sRun) {
        // right after switching the console on the first application can be faster than the module: ask again
        if (!w.active && OSTicksToMilliseconds(OSGetTime() - lastRetry) >= 1000) {
            lastRetry = OSGetTime();
            if (!notActiveSet) {
                w.active = w.handle >= 0 && Bloopair_IsActive(w.handle);
            }
            if (w.active) {
                logLine(btmgr::LOG_APP, "Bloopair module ready");
                if (!becameActive(w)) {
                    w.active     = false;
                    notActiveSet = true;
                }
            } else if (!notActiveSet && OSTicksToSeconds(OSGetTime() - start) >= 10) {
                notActiveSet = true;
                setStatus(i18n::S_NOT_ACTIVE);
            }
        }

        // without a connection the thread only waits for menu commands: ask the module less often and sleep longer
        bool busy = w.active && (w.streamer.running || w.connecting || isBusy(w.st.state));
        // diagnostics: find out where the stream stops for seconds around the HOME Menu
        OSTime tLoop = OSGetTime();
        OSTime tPoll = 0, tPump = 0;
        if (w.active) {
            handleCommands(w);

            OSTime now = OSGetTime();
            if (OSTicksToMilliseconds(now - w.lastPoll) >= (busy ? 100 : 500)) {
                w.lastPoll = now;
                poll(w);
                tPoll = OSGetTime() - now;
            }

            if (w.streamer.running) {
                int vol;
                {
                    Lock l;
                    vol = sVolume;
                }
                OSTime t = OSGetTime();
                w.streamer.pump(w.handle, vol);
                tPump = OSGetTime() - t;
            }
        }
        OSTime t = OSGetTime();
        flushLog(w.connecting);
        OSTime tFlush = OSGetTime() - t;
        uint32_t gap = w.lastLoop ? (uint32_t) OSTicksToMilliseconds(tLoop - w.lastLoop) : 0;
        if (w.streamer.running && (gap > 150 || OSTicksToMilliseconds(tPoll) > 100 || OSTicksToMilliseconds(tPump) > 100 ||
                                   OSTicksToMilliseconds(tFlush) > 100)) {
            logLine(btmgr::LOG_AUDIO, "slow: status %u ms, send %u ms, log %u ms, loop gap %u ms",
                    (unsigned) OSTicksToMilliseconds(tPoll), (unsigned) OSTicksToMilliseconds(tPump),
                    (unsigned) OSTicksToMilliseconds(tFlush), (unsigned) gap);
        }
        w.lastLoop = OSGetTime();
        OSSleepTicks(OSMillisecondsToTicks(busy ? 4 : 20));
    }

    capture::setActive(false);
    w.streamer.stop();
    if (w.handle >= 0) {
        Bloopair_Close(w.handle);
    }
    return 0;
}

void startThread() {
    if (sThread || !sEnabled) {
        return;
    }

    sThread = (OSThread *) memalign(16, sizeof(OSThread));
    sStack  = (uint8_t *) memalign(16, THREAD_STACK_SIZE);
    if (!sThread || !sStack) {
        free(sThread);
        free(sStack);
        sThread = nullptr;
        sStack  = nullptr;
        return;
    }

    sRun = true;
    OSCreateThread(sThread, threadMain, 0, nullptr, sStack + THREAD_STACK_SIZE, THREAD_STACK_SIZE, 18,
                   OS_THREAD_ATTRIB_AFFINITY_ANY);
    OSSetThreadName(sThread, "BloopairAudio");
    OSResumeThread(sThread);
}

void stopThread() {
    if (!sThread) {
        return;
    }

    sRun = false;
    OSJoinThread(sThread, nullptr);
    free(sThread);
    free(sStack);
    sThread = nullptr;
    sStack  = nullptr;
}

void queue(CmdType type, size_t index = 0) {
    Lock l;
    if (type == CmdType::Scan) {
        for (const Cmd &c : sCmds) {
            if (c.type == CmdType::Scan) {
                return; // a search is already waiting
            }
        }
    }
    sCmds.push_back({type, index});
}

} // namespace

namespace btmgr {

void loadSettings() {
    sNotifyReady = NotificationModule_InitLibrary() == NOTIFICATION_MODULE_RESULT_SUCCESS;

    bool enabledValue = true;
    if (WUPSStorageAPI::Get<bool>("enabled", enabledValue) == WUPS_STORAGE_ERROR_SUCCESS) {
        sEnabled = enabledValue;
    }

    int32_t volumeValue = 60;
    if (WUPSStorageAPI::Get<int32_t>("volume", volumeValue) == WUPS_STORAGE_ERROR_SUCCESS) {
        sVolume = volumeValue;
    }

    int32_t sourceValue = capture::SOURCE_DRC;
    if (WUPSStorageAPI::Get<int32_t>("source", sourceValue) == WUPS_STORAGE_ERROR_SUCCESS && sourceValue >= 0 &&
        sourceValue <= 2) {
        sSource = sourceValue;
    }
    capture::setSource(sSource);

    int32_t qualityValue = QUALITY_MEDIUM;
    if (WUPSStorageAPI::Get<int32_t>("quality", qualityValue) == WUPS_STORAGE_ERROR_SUCCESS && qualityValue >= 0 &&
        qualityValue <= QUALITY_STABLE) {
        sQuality = qualityValue;
    }

    bool batteryValue = false;
    if (WUPSStorageAPI::Get<bool>("battery", batteryValue) == WUPS_STORAGE_ERROR_SUCCESS) {
        sBattery = batteryValue;
    }

    bool latencyValue = false;
    if (WUPSStorageAPI::Get<bool>("low_latency", latencyValue) == WUPS_STORAGE_ERROR_SUCCESS) {
        sLowLatency = latencyValue;
    }

    bool logValue = false;
    if (WUPSStorageAPI::Get<bool>("log", logValue) == WUPS_STORAGE_ERROR_SUCCESS) {
        sLogEnabled = logValue;
    }
    int32_t topicsValue = (int32_t) LOG_ALL;
    if (WUPSStorageAPI::Get<int32_t>("log_topics", topicsValue) == WUPS_STORAGE_ERROR_SUCCESS) {
        sLogTopics = (uint32_t) topicsValue & LOG_ALL;
    }
    int32_t statsValue = 60;
    if (WUPSStorageAPI::Get<int32_t>("log_stats", statsValue) == WUPS_STORAGE_ERROR_SUCCESS && statsValue >= 0) {
        sStatsInterval = statsValue;
    }

    std::vector<uint8_t> blob;
    if (WUPSStorageAPI::Get<std::vector<uint8_t>>("devices", blob) == WUPS_STORAGE_ERROR_SUCCESS) {
        Lock l;
        sSaved.clear();
        for (size_t off = 0; off + sizeof(SavedDevice) <= blob.size(); off += sizeof(SavedDevice)) {
            SavedDevice d;
            memcpy(&d, blob.data() + off, sizeof(d));
            d.name[sizeof(d.name) - 1] = 0;
            if (d.name[0]) {
                sSaved.push_back(d);
            }
        }
    }
}

void onApplicationStart() {
    sAppStart = OSGetTime();
    capture::reset();
    startThread();
}

void onApplicationEnd() {
    stopThread();
}

bool enabled() {
    return sEnabled;
}

void setEnabled(bool value) {
    sEnabled = value;
    WUPSStorageAPI::Store<bool>("enabled", value);
    WUPSStorageAPI::SaveStorage();
    if (value) {
        startThread();
    } else {
        stopThread();
    }
}

int volume() {
    return sVolume;
}

void setVolume(int percent) {
    Lock l;
    sVolume = percent < 0 ? 0 : (percent > 100 ? 100 : percent);
    WUPSStorageAPI::Store<int32_t>("volume", sVolume);
}

int source() {
    return sSource;
}

void setSource(int value) {
    sSource = value < 0 || value > 2 ? capture::SOURCE_DRC : value;
    capture::setSource(sSource);
    WUPSStorageAPI::Store<int32_t>("source", sSource);
}

int quality() {
    return sQuality;
}

void setQuality(int value) {
    sQuality = value < 0 || value > QUALITY_STABLE ? QUALITY_MEDIUM : value;
    logLine(LOG_MENU, "menu: quality %d", sQuality);
    WUPSStorageAPI::Store<int32_t>("quality", sQuality);
}

void cmdScan() {
    logLine(LOG_MENU, "menu: search");
    queue(CmdType::Scan);
}
void cmdConnect(size_t i) {
    logLine(LOG_MENU, "menu: connect entry %d", (int) i);
    queue(CmdType::Connect, i);
}
void cmdPair(size_t i) {
    logLine(LOG_MENU, "menu: pair entry %d", (int) i);
    queue(CmdType::Pair, i);
}
void cmdDisconnect() {
    logLine(LOG_MENU, "menu: disconnect");
    queue(CmdType::Disconnect);
}
void removeDevice(const uint8_t addr[6]) {
    std::string name;
    {
        Lock l;
        for (size_t i = 0; i < sSaved.size(); i++) {
            if (memcmp(sSaved[i].addr, addr, 6) == 0) {
                name = sSaved[i].name;
                sSaved.erase(sSaved.begin() + i);
                saveDevices();
                break;
            }
        }
    }
    if (!name.empty()) {
        logLine(LOG_MENU, "menu: remove %s", name.c_str());
        setStatus(i18n::S_REMOVED, name);
    }
}

bool moduleMissing() {
    return sModuleReplaced || sStatusMsg == i18n::S_NOT_ACTIVE;
}

bool restoreModule() {
    constexpr const char *BACKUP = "/vol/external01/wiiu/btaudio/module/30_bloopair.rpx";
    constexpr const char *TARGET = "/vol/external01/wiiu/environments/aroma/modules/setup/30_bloopair.rpx";
    constexpr const char *TEMP   = "/vol/external01/wiiu/environments/aroma/modules/setup/30_bloopair.rpx.new";

    // copy to a new file first, the old one is only replaced once the copy is complete
    bool ok  = false;
    FILE *in = fopen(BACKUP, "rb");
    if (in) {
        FILE *out = fopen(TEMP, "wb");
        if (out) {
            ok = true;
            std::vector<uint8_t> buf(0x8000);
            size_t n;
            while ((n = fread(buf.data(), 1, buf.size(), in)) > 0) {
                if (fwrite(buf.data(), 1, n, out) != n) {
                    ok = false;
                    break;
                }
            }
            if (ferror(in)) {
                ok = false;
            }
            fclose(out);
        }
        fclose(in);
    }
    if (ok) {
        remove(TARGET);
        ok = rename(TEMP, TARGET) == 0;
    } else {
        remove(TEMP);
    }

    logLine(LOG_MENU, "menu: restore module -> %s", ok ? "done" : "failed");
    setStatus(ok ? i18n::S_RESTORED : i18n::S_RESTORE_FAILED);
    return ok;
}

void cmdToggle() {
    logLine(LOG_MENU, "button combo: connect / disconnect");
    queue(CmdType::Toggle);
}

bool battery() {
    return sBattery;
}

void setBattery(bool value) {
    logLine(LOG_MENU, "menu: battery %s", value ? "on" : "off");
    sBattery = value;
    WUPSStorageAPI::Store<bool>("battery", value);
}

bool lowLatency() {
    return sLowLatency;
}

void setLowLatency(bool value) {
    logLine(LOG_MENU, "menu: low latency %s", value ? "on" : "off");
    sLowLatency = value;
    WUPSStorageAPI::Store<bool>("low_latency", value);
}

void cmdReset() {
    logLine(LOG_MENU, "menu: reset plugin");
    queue(CmdType::Reset);
}

void menuClosed() {
    if (connected() && sUsedQuality >= 0 && (sQuality != sUsedQuality || sBattery != sUsedBattery)) {
        logLine(LOG_MENU, "menu: settings changed, reconnecting");
        queue(CmdType::Reconnect);
    }
}

bool logEnabled() {
    return sLogEnabled;
}

void setLogEnabled(bool value) {
    if (!value) {
        logLine(0, "log switched off"); // the last line, written while it is still on
    }
    sLogEnabled = value;
    WUPSStorageAPI::Store<bool>("log", value);
    if (value) {
        logLine(0, "log switched on (topics 0x%02X, statistics every %d s)", (unsigned) sLogTopics,
                sStatsInterval);
    }
}

bool logTopic(uint32_t topic) {
    return (sLogTopics & topic) != 0;
}

void setLogTopic(uint32_t topic, bool on) {
    sLogTopics = on ? (sLogTopics | topic) : (sLogTopics & ~topic);
    WUPSStorageAPI::Store<int32_t>("log_topics", (int32_t) sLogTopics);
}

int statsInterval() {
    return sStatsInterval;
}

void setStatsInterval(int seconds) {
    sStatsInterval = seconds < 0 ? 0 : seconds;
    WUPSStorageAPI::Store<int32_t>("log_stats", sStatsInterval);
}

std::string statusText() {
    Lock l;
    std::string text = i18n::format(sStatusMsg, sStatusArg.c_str());
    int percent = sBatteryPercent;
    if (sStatusMsg == i18n::S_CONNECTED && percent >= 0) {
        text += " (" + std::to_string(percent) + " %)";
    }
    return text;
}

bool connected() {
    return sNotifiedState == BLOOPAIR_AUDIO_STATE_STREAMING;
}

std::vector<SavedDevice> savedDevices() {
    Lock l;
    return sSaved;
}

std::vector<FoundDevice> foundDevices() {
    Lock l;
    return sFound;
}

} // namespace btmgr
