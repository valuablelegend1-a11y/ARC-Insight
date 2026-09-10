#include "sleep.h"

#include <Arduino.h>
#include <driver/gpio.h>
#include <esp_sleep.h>

#include "pins.h"

namespace arcv {

WakeReason wakeReason() {
  switch (esp_sleep_get_wakeup_cause()) {
    case ESP_SLEEP_WAKEUP_EXT0:
      return WakeReason::Comparator;
    case ESP_SLEEP_WAKEUP_TIMER:
      return WakeReason::Timer;
    case ESP_SLEEP_WAKEUP_UNDEFINED:
      return WakeReason::PowerOn;
    default:
      return WakeReason::Unknown;
  }
}

void enterDeepSleep(uint32_t guardSeconds) {
  esp_sleep_enable_ext0_wakeup((gpio_num_t)ARCI_PIN_WAKE, 1);
  if (guardSeconds) {
    esp_sleep_enable_timer_wakeup((uint64_t)guardSeconds * 1000000ULL);
  }
  Serial.flush();
  esp_deep_sleep_start();
}

}  // namespace arcv