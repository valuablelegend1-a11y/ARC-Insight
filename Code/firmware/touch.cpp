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
  mBaseline_ = sum / 32;
  mPressed_ = false;
  mHoldDelivered_ = false;
}

Touch::Gesture Touch::poll(uint32_t nowMs) {
  bool touched = touchRead(ARCI_PIN_TOUCH) > mBaseline_ + ARCI_TOUCH_THRESHOLD;

  if (touched && !mPressed_) {
    if (mTapCount_ == 1 && nowMs - mLastTapReleaseMs_ > ARCI_DOUBLE_TAP_MS) {
      mTapCount_ = 0;
    }
    mPressed_ = true;
    mPressStartMs_ = nowMs;
    mHoldDelivered_ = false;
  }

  if (!touched && mPressed_) {
    mPressed_ = false;
    if (nowMs - mPressStartMs_ < ARCI_TAP_MAX_MS && !mHoldDelivered_) {
      mTapCount_++;
      mLastTapReleaseMs_ = nowMs;
    } else {
      mTapCount_ = 0;
    }
  }

  if (mTapCount_ == 2) {
    mTapCount_ = 0;
    return Gesture::DoubleTap;
  }

  if (touched && !mHoldDelivered_ && nowMs - mPressStartMs_ >= ARCI_TOUCH_HOLD_MS) {
    mHoldDelivered_ = true;
    mTapCount_ = 0;
    return Gesture::Hold;
  }

  if (mTapCount_ == 1 && nowMs - mLastTapReleaseMs_ > ARCI_DOUBLE_TAP_MS) {
    mTapCount_ = 0;
    return Gesture::Tap;
  }

  return Gesture::None;
}

}  // namespace arcv