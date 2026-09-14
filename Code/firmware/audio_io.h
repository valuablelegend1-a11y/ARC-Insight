#pragma once
#include <stddef.h>
#include <stdint.h>

#include <functional>

#include "pins.h"
#include "resample.h"

namespace arcv {

class AudioIO {
 public:
  AudioIO();
  ~AudioIO();
  typedef std::function<void(const int16_t* pcm, size_t samples)> PcmHandler;

  bool begin();
  void poll();
  void feedMp3(const uint8_t* data, size_t len);
  void setStreaming(bool on);
  bool streaming() const { return streaming_; }
  void onPcmReady(PcmHandler h) { pcmHandler_ = std::move(h); }
  void beep();

 private:
  void readMicBlock();
  void playPcm16(const int16_t* pcm, size_t samples);

  RatioResampler dec_;
  int32_t raw_[1024];
  int16_t pcmFrame_[ARCI_PCM_FRAME_SAMPLES];
  size_t pcmFrameUsed_ = 0;
  bool streaming_ = false;
  PcmHandler pcmHandler_;
  bool i2sReady_ = false;
};

}  // namespace arcv