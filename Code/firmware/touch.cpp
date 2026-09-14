#include "touch.h"

#include <Arduino.h>

#include "arcinsight_config.h"
#include "pins.h"

namespace arcv {

void Touch::begin() { calibrate(); }

void Touch::calibrate() {
  uint32_t sum = 0;
  for (int i = 0; i < 32; ++i) {
    sum += touchRead(ARCI_PIN_TOUCH);
    delay(2);
  }
  mBaseline = sum / 32;
}

bool Touch::poll(uint32_t nowMs) {
  uint32_t val = touchRead(ARCI_PIN_TOUCH);

  if (mSuppressed) {
    if (val >= mWakeHoldRaw / 2) {
      return true;
    }
    mSuppressed = false;
    calibrate();
  }

  bool touched = (val > mBaseline + ARCI_TOUCH_THRESHOLD);

  if (!touched) {
    mGrab = false;
    mPressStartMs = nowMs;
    mTouched = false;
    return false;
  }

  if (!mTouched) {
    mPressStartMs = nowMs;
  }
  mTouched = true;

  if (!mGrab && (nowMs - mPressStartMs >= ARCI_TOUCH_HOLD_MS)) {
    mGrab = true;
  }

  return true;
}

bool Touch::grab() const { return mGrab; }

void Touch::enableWake() const {
  touchSleepWakeUpEnable(ARCI_PIN_TOUCH, mBaseline + ARCI_TOUCH_THRESHOLD * 2);
}

void Touch::suppressRearm() {
  mSuppressed = true;
  mGrab = false;
  mWakeHoldRaw = touchRead(ARCI_PIN_TOUCH);
}

uint32_t Touch::raw() const { return touchRead(ARCI_PIN_TOUCH); }

}  // namespace arcv