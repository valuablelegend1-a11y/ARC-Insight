#pragma once
#include <stdint.h>

namespace arcv {

class Touch {
 public:
  enum class Gesture : uint8_t { None = 0, Tap, DoubleTap, Hold };

  void begin();
  Gesture poll(uint32_t nowMs);

 private:
  void calibrate();

  uint32_t mBaseline_ = 0;
  bool mPressed_ = false;
  bool mHoldDelivered_ = false;
  uint32_t mPressStartMs_ = 0;
  uint32_t mLastTapReleaseMs_ = 0;
  uint8_t mTapCount_ = 0;
};

}  // namespace arcv