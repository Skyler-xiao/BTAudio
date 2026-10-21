/*
 *   Translations of the Bluetooth audio plugin (all languages of the Wii U system settings)
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 2 of the License, or
 *   (at your option) any later version.
 */

#pragma once

#include <string>

namespace i18n {

// same numbers as the console setting "cafe.language"
enum Language {
    LANG_JAPANESE = 0,
    LANG_ENGLISH,
    LANG_FRENCH,
    LANG_GERMAN,
    LANG_ITALIAN,
    LANG_SPANISH,
    LANG_CHINESE_SIMPLIFIED,
    LANG_KOREAN,
    LANG_DUTCH,
    LANG_PORTUGUESE,
    LANG_RUSSIAN,
    LANG_CHINESE_TRADITIONAL,
    LANG_COUNT,
    LANG_AUTO = -1, // follow the console
};

enum Msg {
    // status line
    S_READY,
    S_NOT_ACTIVE,
    S_CODEC_BAD,
    S_CONNECTED,
    S_PAIR_FAILED,
    S_CONNECT_FAILED,
    S_DISCONNECTED,
    S_CONNECTING,
    S_PAIRING,
    S_TIMEOUT,
    S_SCAN_BUSY,
    S_SCANNING,
    S_SCAN_UNAVAILABLE,
    S_SCAN_NONE,
    S_SCAN_DONE,
    S_BUSY,
    S_DISCONNECTING,
    S_RESET,
    S_MODULE_REPLACED,
    S_RESTORED,
    S_RESTORE_FAILED,
    S_REMOVED,

    // on-screen messages
    N_PREFIX,
    N_CONNECTED,
    N_PAIRED,
    N_SCAN_DONE,
    N_SCAN_NONE,
    N_PAIR_FAILED,
    N_CONNECT_FAILED,
    N_MODULE_REPLACED,
    N_BATTERY_LOW,

    // menu
    M_TITLE,
    M_STATUS,
    M_ON,
    M_OFF,
    M_CONNECT,
    M_NO_SAVED,
    M_DISCONNECT,
    M_PRESS_A,
    M_VOLUME,
    M_CAT_PAIR,
    M_STEP1,
    M_SEARCH,
    M_STEP3,
    M_PAIR,
    M_NOTHING_FOUND,
    M_CAT_SETTINGS,
    M_SOURCE,
    M_SRC_TV,
    M_AUTO,
    M_RESET,
    M_REMOVE,
    M_HOTKEY,
    M_LOW_LATENCY,
    M_RESTORE,
    M_BATTERY,
    M_LANGUAGE,
    M_CAT_LOG,
    M_LOG,
    M_LOG_CONNECTION,
    M_LOG_SCAN,
    M_LOG_MENU,
    M_LOG_APP,
    M_LOG_STATS,
    M_EVERY,
    M_LOG_FILE,
    M_QUALITY,
    M_Q_HIGH,
    M_Q_MEDIUM,
    M_Q_STABLE,

    MSG_COUNT,
};

// reads the console language once (call at plugin start)
void init();

// LANG_AUTO or a fixed language chosen in the menu
void setOverride(int language);
int getOverride();

// the language in use
int current();

const char *tr(Msg msg);

// tr() with one %s or %d filled in
std::string format(Msg msg, const char *text);
std::string format(Msg msg, int number);

// the same in English, for the log file
std::string formatEnglish(Msg msg, const char *text);

// name of a language in that language
const char *languageName(int language);

} // namespace i18n
