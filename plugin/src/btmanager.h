/*
 *   Connection manager of the Bluetooth audio plugin: saved devices, pairing, connecting, streaming
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 2 of the License, or
 *   (at your option) any later version.
 */

#pragma once

#include <stdint.h>
#include <string>
#include <vector>

struct SavedDevice {
    uint8_t addr[6];
    uint8_t keyType;
    uint8_t key[16];
    char name[40];
} __attribute__((packed));

struct FoundDevice {
    uint8_t addr[6];
    char name[40];
    int8_t rssi;
};

namespace btmgr {

// topics of the log file, chosen in the settings
enum LogTopic : uint32_t {
    LOG_CONNECTION = 1 << 0, // pairing, connecting, status changes
    LOG_SCAN       = 1 << 1, // device search and its results
    LOG_MENU       = 1 << 2, // what was chosen in the menu
    LOG_APP        = 1 << 3, // application changes, sample rate of the game
    LOG_AUDIO      = 1 << 4, // statistics of the stream and radio problems
    LOG_ALL        = 0x1F,
};

// settings and saved devices from the plugin storage
void loadSettings();

// start / stop the background thread (it belongs to one application, the connection itself lives in IOS)
void onApplicationStart();
void onApplicationEnd();

bool enabled();
void setEnabled(bool value);

int volume(); // percent
void setVolume(int percent);
int source(); // capture::SOURCE_*
void setSource(int value);

// sound quality against robustness of the radio link; a change reconnects when the menu closes
enum Quality { QUALITY_HIGH = 0, QUALITY_MEDIUM, QUALITY_STABLE };
bool battery(); // read the device's battery over the hands-free profile; a change reconnects when the menu closes
void setBattery(bool value);
bool lowLatency(); // smaller buffers: less delay, more risk of dropouts
void setLowLatency(bool value);
int quality();
void setQuality(int value);

// actions requested from the menu, they run in the background
void cmdScan();
void cmdConnect(size_t savedIndex);
void cmdPair(size_t foundIndex);
void cmdDisconnect();
void cmdToggle(); // button combo: connect to the last device or disconnect
void cmdReset();
// the menu was closed: reconnect if a setting of the running connection changed
void menuClosed();

// the Bluetooth module is missing or was replaced by Bloopair without audio (an update)
bool moduleMissing();
// copies the backup from wiiu/btaudio/module back into the Aroma modules (needs a restart)
bool restoreModule(); // disconnect, forget all devices and set every setting back to its default
void removeDevice(const uint8_t addr[6]); // forget one saved device (runs at once)

// log file on the SD card (off by default)
bool logEnabled();
void setLogEnabled(bool value);
bool logTopic(uint32_t topic);
void setLogTopic(uint32_t topic, bool on);
int statsInterval(); // seconds, 0 = no statistics
void setStatsInterval(int seconds);

std::string statusText();
bool connected(); // streaming to a device at the moment
std::vector<SavedDevice> savedDevices();
std::vector<FoundDevice> foundDevices();

} // namespace btmgr
