#pragma once
#include <stdint.h>

#include <functional>

namespace arcv {

class Heart {
 public:
  typedef std::function<void(int bpm, float confidence)> Sender;

  void begin();
  void wake();
  void shutdown();
  void poll(uint32_t nowMs);
  void onData(Sender s) { sender_ = std::move(s); }

 private:
  void handleIrSample(uint32_t nowMs);
  void publish(uint32_t nowMs);

  Sender sender_;
  bool present_ = false;
  uint32_t lastBeatMs_ = 0;
  uint32_t lastHrMs_ = 0;
  int ibi_[10];
  size_t ibiCount_ = 0;
  int smoothedBpm_ = 0;
};

}  // namespace arcv