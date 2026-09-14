#pragma once
#include <stdint.h>

namespace arcv {

enum class BinaryOp : uint8_t {
  AudioPcm  = 0x01,
  ImageJpeg = 0x02,
  AudioMp3  = 0x03,
  FwChunk   = 0x04,
};

}  // namespace arcv