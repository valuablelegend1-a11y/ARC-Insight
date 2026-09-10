#pragma once
#include <stddef.h>
#include <stdint.h>

#include "libhelix-mp3/mp3dec.h"

namespace arcv {

class Mp3Decoder {
 public:
  typedef void (*PcmSink)(const int16_t* pcm, size_t samples, void* ctx);

  bool begin();
  bool active() const { return h_ != nullptr; }
  void setSink(PcmSink sink, void* ctx) {
    sink_ = sink;
    ctx_ = ctx;
  }
  void reset();
  void push(const uint8_t* data, size_t len);

 private:
  void decodeAll();
  void emitPcm();

  HMP3Decoder h_ = nullptr;
  uint8_t buf_[8192];
  size_t len_ = 0;
  size_t rd_ = 0;
  PcmSink sink_ = nullptr;
  void* ctx_ = nullptr;
  int16_t out_[2304];
  int16_t mono_[1152];
};

}  // namespace arcv