#pragma once
#include "esp_event.h"

ESP_EVENT_DECLARE_BASE(LOCK_EVENT);
enum {
    LOCK_STATE_CHANGED,
    LOCK_UPDATE_STATE,
    LOCK_TARGET_STATE_CHANGED,
    LOCK_OVERRIDE_STATE,
};

ESP_EVENT_DECLARE_BASE(NFC_EVENT);
enum {
    NFC_TAP_EVENT,
    NFC_STATUS_CHANGED,
};

ESP_EVENT_DECLARE_BASE(HK_EVENT);
enum {
    HK_INTERNAL_EVENT,
};

ESP_EVENT_DECLARE_BASE(HW_EVENT);
enum {
    HW_ACTION,
    HW_ALT_ACTION,
    HW_CONFIG_CHANGED,
    HW_DISPLAY_COMMAND,        // uint8_t DisplayCommand (MQTT -> status display)
    HW_DISPLAY_STATE_CHANGED,  // uint8_t: 1 = screen on, 0 = off (status display -> MQTT)
};

/// Payload of HW_DISPLAY_COMMAND.
enum DisplayCommand : uint8_t {
    DISPLAY_CMD_OFF = 0,
    DISPLAY_CMD_ON = 1,          // on until OFF (no timeout)
    DISPLAY_CMD_WAKE = 2,        // on until the inactivity timeout
    DISPLAY_CMD_WAKE_STATE = 3,  // like WAKE, but show the lock state even in neutral mode
    DISPLAY_CMD_REPORT = 4,      // re-announce the current on/off state
};

ESP_EVENT_DECLARE_BASE(MQTT_EVENT);
enum {
    MQTT_STATUS_CHANGED,
};

// ETH_EVENT is taken by esp_eth (esp_eth_com.h), hence the APP_ prefix.
ESP_EVENT_DECLARE_BASE(ETH_APP_EVENT);
enum {
    ETH_STARTED,
    ETH_GOT_IP,
    ETH_LOST_IP,
    ETH_DISCONNECTED,
    ETH_STOPPED,
};

ESP_EVENT_DECLARE_BASE(KEYPAD_EVENT);
enum {
    KEYPAD_CODE_ENTERED,
    KEYPAD_DOORBELL,
};
