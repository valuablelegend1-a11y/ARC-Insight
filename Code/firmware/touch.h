#pragma once
#include <stdint.h>

namespace arcv {

// Frame-as-a-button gestures on the touch pad. Deep-sleep wake is voice-only;
// this only ever runs while the device is awake.
//   single tap   -> basic sleep (drop Wi-Fi back to listening)
//   double tap   -> take a plain picture, saved by the PC (no AI)
//   hold 5 s     -> deep sleep
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