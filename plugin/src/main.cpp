/*
 *   Bluetooth audio for the Wii U (BTAudio Aroma plugin)
 *
 *   Sends the console's sound to Bluetooth headphones. The Bluetooth side lives in IOS-PAD (30_bloopair.rpx),
 *   this plugin captures the sound, encodes it and manages the devices.
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 2 of the License, or
 *   (at your option) any later version.
 */

#include "btmanager.h"
#include "capture.h"
#include "i18n.h"

#include <wups.h>
#include <wups/config/WUPSConfigCategory.h>
#include <wups/button_combo/api.h>
#include <wups/config/WUPSConfigItemBoolean.h>
#include <wups/config/WUPSConfigItemButtonCombo.h>
#include <wups/config/WUPSConfigItemIntegerRange.h>
#include <wups/config/WUPSConfigItemMultipleValues.h>
#include <wups/config/WUPSConfigItemStub.h>
#include <wups/config_api.h>
#include <wups/storage.h>

#include <array>
#include <optional>
#include <functional>
#include <stdexcept>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>

WUPS_PLUGIN_NAME("BTAudio");
WUPS_PLUGIN_DESCRIPTION("Sends the console sound to Bluetooth headphones");
WUPS_PLUGIN_VERSION("v1.0");
WUPS_PLUGIN_AUTHOR("Xiao, based on Bloopair");
WUPS_PLUGIN_LICENSE("GPL");

WUPS_USE_WUT_DEVOPTAB();
WUPS_USE_STORAGE("btaudio");

namespace {

// texts of the open menu, the config API may keep pointers into them
std::vector<std::string> sMenuTexts;
std::vector<SavedDevice> sMenuSaved;
std::vector<FoundDevice> sMenuFound;

const char *keep(std::string text) {
    sMenuTexts.push_back(std::move(text));
    return sMenuTexts.back().c_str();
}

void enabledChanged(ConfigItemBoolean *, bool value) {
    btmgr::setEnabled(value);
}

void volumeChanged(ConfigItemIntegerRange *, int32_t value) {
    btmgr::setVolume(value);
}

void pairChosen(ConfigItemMultipleValues *, uint32_t value) {
    if (value > 0) {
        btmgr::cmdPair(value - 1);
    }
}

void sourceChosen(ConfigItemMultipleValues *, uint32_t value) {
    btmgr::setSource((int) value);
}

// button combo to connect / disconnect without opening the menu (held for a second, Aroma checks for conflicts)
constexpr auto DEFAULT_COMBO = (WUPSButtonCombo_Buttons) (WUPS_BUTTON_COMBO_BUTTON_L | WUPS_BUTTON_COMBO_BUTTON_R |
                                                          WUPS_BUTTON_COMBO_BUTTON_MINUS);
std::optional<WUPSButtonComboAPI::ButtonCombo> sCombo;

void comboTriggered(WUPSButtonCombo_ControllerTypes, WUPSButtonCombo_ComboHandle, void *) {
    btmgr::cmdToggle();
}

void comboChanged(ConfigItemButtonCombo *, uint32_t combo) {
    WUPSStorageAPI::Store<uint32_t>("combo", combo);
}

void resetAll() {
    btmgr::cmdReset();
    if (sCombo) {
        WUPSButtonCombo_ComboStatus status;
        (void) sCombo->UpdateButtonCombo(DEFAULT_COMBO, status);
    }
}

void batteryChanged(ConfigItemBoolean *, bool value) {
    btmgr::setBattery(value);
}

void latencyChanged(ConfigItemBoolean *, bool value) {
    btmgr::setLowLatency(value);
}

void logChanged(ConfigItemBoolean *, bool value) {
    btmgr::setLogEnabled(value);
}

// one switch per log topic
template <uint32_t TOPIC>
void logTopicChanged(ConfigItemBoolean *, bool value) {
    btmgr::setLogTopic(TOPIC, value);
}

void qualityChosen(ConfigItemMultipleValues *, uint32_t value) {
    btmgr::setQuality((int) value);
}

void statsChosen(ConfigItemMultipleValues *, uint32_t value) {
    btmgr::setStatsInterval((int) value);
}

void languageChosen(ConfigItemMultipleValues *, uint32_t value) {
    i18n::setOverride((int) value - 1);
    WUPSStorageAPI::Store<int32_t>("language", i18n::getOverride());
}

// menu entry which runs an action when A is pressed on it
class ButtonItem : public WUPSConfigItem {
public:
    static ButtonItem Create(std::string name, std::function<void()> action) {
        auto *ctx = new Context{std::move(name), std::move(action), false, false};

        WUPSConfigAPIItemCallbacksV2 callbacks = {
            .getCurrentValueDisplay         = display,
            .getCurrentValueSelectedDisplay = display,
            .onSelected                     = selected,
            .restoreDefault                 = nothing,
            .isMovementAllowed              = movementAllowed,
            .onCloseCallback                = nothing,
            .onInput                        = input,
            .onInputEx                      = inputEx,
            .onDelete                       = destroy,
        };
        WUPSConfigAPIItemOptionsV2 options = {
            .displayName = ctx->name.c_str(),
            .context     = ctx,
            .callbacks   = callbacks,
        };

        WUPSConfigItemHandle handle;
        if (WUPSConfigAPI_Item_Create(options, &handle) != WUPSCONFIG_API_RESULT_SUCCESS) {
            delete ctx;
            throw std::runtime_error("button item");
        }
        return ButtonItem(handle);
    }

private:
    struct Context {
        std::string name;
        std::function<void()> action;
        bool isSelected;
        bool done;
    };

    explicit ButtonItem(WUPSConfigItemHandle handle) : WUPSConfigItem(handle) {}

    static int32_t display(void *context, char *out, int32_t size) {
        auto *ctx = (Context *) context;
        // the hint only shows on the selected line, after pressing it says OK
        const char *text = ctx->done ? "OK" : (ctx->isSelected ? i18n::tr(i18n::M_PRESS_A) : "");
        snprintf(out, size, "%s", text);
        return 0;
    }

    static void selected(void *context, bool isSelected) {
        ((Context *) context)->isSelected = isSelected;
    }

    static void nothing(void *) {}

    static bool movementAllowed(void *) {
        return true;
    }

    static void input(void *context, WUPSConfigSimplePadData pad) {
        auto *ctx = (Context *) context;
        if (ctx->isSelected && (pad.buttons_d & WUPS_CONFIG_BUTTON_A)) {
            ctx->action();
            ctx->done = true;
        }
    }

    static void inputEx(void *, WUPSConfigComplexPadData) {}

    static void destroy(void *context) {
        delete (Context *) context;
    }
};

WUPSConfigAPICallbackStatus menuOpened(WUPSConfigCategoryHandle rootHandle) {
    using namespace i18n;
    WUPSConfigCategory root = WUPSConfigCategory(rootHandle);

    // the previous menu is gone, its texts can go too
    sMenuTexts.clear();
    sMenuTexts.reserve(64);
    sMenuSaved = btmgr::savedDevices();
    sMenuFound = btmgr::foundDevices();

    try {
        // the things needed every day
        root.add(WUPSConfigItemStub::Create(keep(format(M_STATUS, btmgr::statusText().c_str()))));
        root.add(WUPSConfigItemBoolean::CreateEx("enabled", tr(M_TITLE), true, btmgr::enabled(), enabledChanged,
                                                 tr(M_ON), tr(M_OFF)));

        if (btmgr::moduleMissing()) {
            // nothing else works without our module: offer to put it back
            root.add(ButtonItem::Create(tr(M_RESTORE), [] { btmgr::restoreModule(); }));
        } else if (btmgr::connected()) {
            root.add(ButtonItem::Create(tr(M_DISCONNECT), btmgr::cmdDisconnect));
        } else if (sMenuSaved.empty()) {
            root.add(WUPSConfigItemStub::Create(keep(std::string(tr(M_CONNECT)) + ": " + tr(M_NO_SAVED))));
        } else {
            // one line per saved device
            for (size_t i = 0; i < sMenuSaved.size(); i++) {
                root.add(ButtonItem::Create(std::string(tr(M_CONNECT)) + ": " + sMenuSaved[i].name,
                                            [i] { btmgr::cmdConnect(i); }));
            }
        }

        root.add(WUPSConfigItemIntegerRange::Create("volume", tr(M_VOLUME), 60, btmgr::volume(), 0, 100, volumeChanged));

        // pairing, step by step
        WUPSConfigCategory pair = WUPSConfigCategory::Create(tr(M_CAT_PAIR));
        pair.add(WUPSConfigItemStub::Create(tr(M_STEP1)));
        pair.add(ButtonItem::Create(tr(M_SEARCH), btmgr::cmdScan));
        pair.add(WUPSConfigItemStub::Create(tr(M_STEP3)));
        if (sMenuFound.empty()) {
            pair.add(WUPSConfigItemStub::Create(tr(M_NOTHING_FOUND)));
        } else {
            std::vector<WUPSConfigItemMultipleValues::ValuePair> found;
            found.push_back({0, "-"});
            for (size_t i = 0; i < sMenuFound.size(); i++) {
                found.push_back({(uint32_t) (i + 1), keep(sMenuFound[i].name)});
            }
            pair.add(WUPSConfigItemMultipleValues::CreateFromValue(std::nullopt, tr(M_PAIR), 0, 0, found, pairChosen));
        }
        root.add(std::move(pair));

        // rarely needed
        WUPSConfigCategory settings = WUPSConfigCategory::Create(tr(M_CAT_SETTINGS));
        const WUPSConfigItemMultipleValues::ValuePair sources[] = {
            {capture::SOURCE_DRC, "GamePad"},
            {capture::SOURCE_TV, keep(tr(M_SRC_TV))},
            {capture::SOURCE_AUTO, keep(tr(M_AUTO))},
        };
        settings.add(WUPSConfigItemMultipleValues::CreateFromValue(std::nullopt, tr(M_SOURCE), capture::SOURCE_DRC,
                                                                   (uint32_t) btmgr::source(), sources, sourceChosen));

        const WUPSConfigItemMultipleValues::ValuePair qualities[] = {
            {btmgr::QUALITY_HIGH, keep(tr(M_Q_HIGH))},
            {btmgr::QUALITY_MEDIUM, keep(tr(M_Q_MEDIUM))},
            {btmgr::QUALITY_STABLE, keep(tr(M_Q_STABLE))},
        };
        settings.add(WUPSConfigItemMultipleValues::CreateFromValue(std::nullopt, tr(M_QUALITY), btmgr::QUALITY_MEDIUM,
                                                                   (uint32_t) btmgr::quality(), qualities,
                                                                   qualityChosen));

        settings.add(WUPSConfigItemBoolean::CreateEx(std::nullopt, tr(M_BATTERY), false, btmgr::battery(), batteryChanged,
                                                     tr(M_ON), tr(M_OFF)));
        settings.add(WUPSConfigItemBoolean::CreateEx(std::nullopt, tr(M_LOW_LATENCY), false, btmgr::lowLatency(),
                                                     latencyChanged, tr(M_ON), tr(M_OFF)));
        if (sCombo) {
            settings.add(WUPSConfigItemButtonCombo::Create(std::nullopt, tr(M_HOTKEY), DEFAULT_COMBO, sCombo->getHandle(),
                                                           comboChanged));
        }

        std::vector<WUPSConfigItemMultipleValues::ValuePair> languages;
        languages.push_back({0, keep(tr(M_AUTO))});
        for (int i = 0; i < LANG_COUNT; i++) {
            languages.push_back({(uint32_t) (i + 1), languageName(i)});
        }
        settings.add(WUPSConfigItemMultipleValues::CreateFromValue(std::nullopt, tr(M_LANGUAGE), 0,
                                                                   (uint32_t) (getOverride() + 1), languages,
                                                                   languageChosen));

        // one line per saved device, the address is kept so a changed list cannot hit the wrong one
        for (const SavedDevice &d : sMenuSaved) {
            std::array<uint8_t, 6> addr;
            memcpy(addr.data(), d.addr, 6);
            settings.add(ButtonItem::Create(std::string(tr(M_REMOVE)) + ": " + d.name,
                                            [addr] { btmgr::removeDevice(addr.data()); }));
        }
        settings.add(ButtonItem::Create(tr(M_RESET), resetAll));

        // only for finding problems, a normal user leaves it off
        WUPSConfigCategory log = WUPSConfigCategory::Create(tr(M_CAT_LOG));
        log.add(WUPSConfigItemBoolean::CreateEx(std::nullopt, tr(M_LOG), false, btmgr::logEnabled(), logChanged, tr(M_ON),
                                                tr(M_OFF)));
        const struct {
            Msg name;
            uint32_t topic;
            BooleanValueChangedCallback callback;
        } topics[] = {
            {M_LOG_CONNECTION, btmgr::LOG_CONNECTION, logTopicChanged<btmgr::LOG_CONNECTION>},
            {M_LOG_SCAN, btmgr::LOG_SCAN, logTopicChanged<btmgr::LOG_SCAN>},
            {M_LOG_MENU, btmgr::LOG_MENU, logTopicChanged<btmgr::LOG_MENU>},
            {M_LOG_APP, btmgr::LOG_APP, logTopicChanged<btmgr::LOG_APP>},
        };
        for (const auto &t : topics) {
            log.add(WUPSConfigItemBoolean::CreateEx(std::nullopt, tr(t.name), true, btmgr::logTopic(t.topic), t.callback,
                                                    tr(M_ON), tr(M_OFF)));
        }
        const WUPSConfigItemMultipleValues::ValuePair stats[] = {
            {0, keep(tr(M_OFF))},
            {60, keep(format(M_EVERY, 60))},
            {10, keep(format(M_EVERY, 10))},
            {2, keep(format(M_EVERY, 2))},
        };
        uint32_t statsNow = (uint32_t) btmgr::statsInterval();
        if (statsNow != 0 && statsNow != 60 && statsNow != 10 && statsNow != 2) {
            statsNow = 60;
        }
        log.add(WUPSConfigItemMultipleValues::CreateFromValue(std::nullopt, tr(M_LOG_STATS), 60, statsNow, stats,
                                                              statsChosen));
        log.add(WUPSConfigItemStub::Create(tr(M_LOG_FILE)));
        settings.add(std::move(log));

        root.add(std::move(settings));
    } catch (std::exception &e) {
        return WUPSCONFIG_API_CALLBACK_RESULT_ERROR;
    }

    return WUPSCONFIG_API_CALLBACK_RESULT_SUCCESS;
}

void menuClosed() {
    WUPSStorageAPI::SaveStorage();
    btmgr::menuClosed();
}

} // namespace

INITIALIZE_PLUGIN() {
    i18n::init();
    int32_t language = i18n::LANG_AUTO;
    if (WUPSStorageAPI::Get<int32_t>("language", language) == WUPS_STORAGE_ERROR_SUCCESS) {
        i18n::setOverride(language);
    }

    WUPSConfigAPIOptionsV1 options = {.name = i18n::tr(i18n::M_TITLE)};
    WUPSConfigAPI_Init(options, menuOpened, menuClosed);

    btmgr::loadSettings();

    uint32_t combo = DEFAULT_COMBO;
    WUPSStorageAPI::Get<uint32_t>("combo", combo);
    WUPSButtonCombo_ComboStatus status;
    WUPSButtonCombo_Error error;
    sCombo = WUPSButtonComboAPI::CreateComboHold("BTAudio connect / disconnect", (WUPSButtonCombo_Buttons) combo, 1000,
                                                 comboTriggered, nullptr, status, error);
}

DEINITIALIZE_PLUGIN() {
    sCombo.reset();
}

ON_APPLICATION_START() {
    btmgr::onApplicationStart();
}

ON_APPLICATION_ENDS() {
    btmgr::onApplicationEnd();
}
