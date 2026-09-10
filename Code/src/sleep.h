#pragma once
#include <stdint.h>

namespace arcv {

enum class WakeReason : uint8_t {
  Unknown = 0,
  Comparator,
  Timer,
  PowerOn,
};

WakeReason wakeReason();
void enterDeepSleep(uint32_t guardSeconds);

}  // namespace arcv