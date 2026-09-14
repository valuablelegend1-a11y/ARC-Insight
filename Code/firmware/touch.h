#pragma once
#include <stdint.h>

namespace arcv {

class Touch {
 public:
  void begin();
  bool poll(uint32_t nowMs);
  bool grab() const;
  void enableWake() const;
  void suppressRearm();
  uint32_t raw() const;

 private:
  void calibrate();

  uint32_t mBaseline = 0;
  uint32_t mWakeHoldRaw = 0;
  bool mTouched = false;
  bool mGrab = false;
  bool mSuppressed = false;
  uint32_t mPressStartMs = 0;
};

}  // namespace arcv