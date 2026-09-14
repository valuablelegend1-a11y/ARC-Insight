#pragma once
#include <stddef.h>
#include <stdint.h>

namespace arcv {

class RatioResampler {
 public:
  RatioResampler(size_t taps, float inRate, float outRate, float cutoffScale);
  ~RatioResampler();
  void reset();
  void push(int16_t sample);
  size_t pull(int16_t* out, size_t maxOut);

 private:
  void buildFilter();
  const size_t taps_;
  const float inPerOut_;
  float cutoffN_;      // normalized cutoff, cycles per input sample
  float* h_;
  int16_t* ring_;
  long newestAbs_ = 0;
  float pos_ = 0.0f;
};

}  // namespace arcv