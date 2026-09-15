#pragma once

#define ARCI_PC_HOST      "jarvis.local"
#define ARCI_PC_IP        "192.168.1.100"
#define ARCI_PC_PORT      8765
#define ARCI_WS_PATH      "/"

#define ARCI_WIFI_SSID    ""
#define ARCI_WIFI_PASS    ""

#define ARCI_IDLE_TIMEOUT_MS   60000
#define ARCI_WS_TIMEOUT_MS     20000
#define ARCI_SLEEP_GUARD_HOURS 8
#define ARCI_LISTEN_GUARD_HOURS 8

// On-device wake-word (Option A). The glasses rest in a low-power listen
// state (Wi-Fi off) and only connect/stream after hearing "jarvis".
#define ARCI_KWS_ENABLE        1
#define ARCI_KWS_THRESHOLD     0.68f
#define ARCI_KWS_MIN_FRAMES    12
#define ARCI_KWS_MAX_FRAMES    60
#define ARCI_KWS_SLICES        16
#define ARCI_KWS_TRAIN_REPS    3

// Frame-as-a-button gestures (touch pad, voice-only deep-sleep wake).
#define ARCI_TOUCH_HOLD_MS     5000
#define ARCI_TAP_MAX_MS        400
#define ARCI_DOUBLE_TAP_MS     500
#define ARCI_TOUCH_THRESHOLD   80