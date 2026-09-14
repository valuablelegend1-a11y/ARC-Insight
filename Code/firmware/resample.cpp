#include "resample.h"

#include <string.h>
#include <math.h>

namespace arcv {

RatioResampler::RatioResampler(size_t taps, float inRate, float outRate,
                               float cutoffScale)
    : taps_(taps % 2 ? taps + 1 : taps),
      inPerOut_(inRate / outRate),
      cutoffN_(cutoffScale) {
  if (cutoffN_ > 0.5f) cutoffN_ = 0.5f;
  if (cutoffN_ < 0.05f) cutoffN_ = 0.05f;
  h_ = new float[taps_];
  ring_ = new int16_t[taps_];
  buildFilter();
  reset();
}

RatioResampler::~RatioResampler() {
  delete[] h_;
  delete[] ring_;
}

void RatioResampler::buildFilter() {
  const float centerF = (float)(taps_ - 1) * 0.5f;
  const float pi = 3.14159265358979f;
  float sum = 0.0f;
  for (size_t i = 0; i < taps_; ++i) {
    float n = (float)i - centerF;
    float arg = pi * cutoffN_ * n;
    float sinc = (n == 0.0f) ? 1.0f : (sinf(arg) / arg);
    float t = (float)i / (float)(taps_ - 1);
    float window = 0.42f - 0.5f * cosf(2.0f * pi * t) +
                   0.08f * cosf(4.0f * pi * t);
    h_[i] = cutoffN_ * sinc * window;
    sum += h_[i];
  }
  for (size_t i = 0; i < taps_; ++i) h_[i] /= sum;
}

void RatioResampler::reset() {
  memset(ring_, 0, taps_ * sizeof(int16_t));
  newestAbs_ = 0;
  pos_ = 0.0f;
}

void RatioResampler::push(int16_t sample) {
  ring_[newestAbs_ % taps_] = sample;
  ++newestAbs_;
}

size_t RatioResampler::pull(int16_t* out, size_t maxOut) {
  size_t written = 0;
  const long center = (long)(taps_ / 2);
  while (written < maxOut && (long)pos_ + center <= newestAbs_) {
    long k = (long)pos_;
    float frac = pos_ - (float)k;
    float acc = 0.0f;
    for (size_t j = 0; j < taps_; ++j) {
      long n = k + (long)j - center;
      long idx = n % (long)taps_;
      if (idx < 0) idx += (long)taps_;
      acc += h_[j] * (float)ring_[idx];
    }
    float v = acc;
    if (v > 32767.0f) v = 32767.0f;
    if (v < -32768.0f) v = -32768.0f;
    (void)frac;
    out[written++] = (int16_t)v;
    pos_ += inPerOut_;
  }
  return written;
}

}  // namespace arcv