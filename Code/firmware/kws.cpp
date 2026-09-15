#include "kws.h"

#include <Arduino.h>
#include <math.h>
#include <string.h>

#include <Preferences.h>

#include "pins.h"

namespace arcv {

namespace {
constexpr size_t kSilenceEndFrames = 12;  // 240 ms of quiet ends a word
constexpr float kMinMel = 100.0f;
constexpr float kMaxMel = 7800.0f;
Preferences gPrefs;
}  // namespace

void Kws::buildTables() {
  for (size_t i = 0; i < kFft / 2; ++i) {
    twCo_[i] = cosf(2.0f * kPi * (float)i / (float)kFft);
  }
  for (size_t i = 0; i < kFft; ++i) {
    wnd_[i] = 0.54f - 0.46f * cosf(2.0f * kPi * (float)i / (float)(kFft - 1));
    size_t j = i;
    uint8_t r = 0;
    for (size_t b = 0; b < 8; ++b) {
      r = (uint8_t)((r << 1) | (j & 1));
      j >>= 1;
    }
    rev_[i] = r;
  }

  auto melOf = [](float f) { return 2595.0f * log10f(1.0f + f / 700.0f); };
  auto fOf = [](float m) { return 700.0f * (powf(10.0f, m / 2595.0f) - 1.0f); };
  float lo = melOf(kMinMel), hi = melOf(kMaxMel);
  float step = (hi - lo) / (kBands + 1.0f);
  for (size_t b = 0; b < kBands; ++b) {
    float c = lo + step * (float)(b + 1);
    float fl = fOf(c - step), fc = fOf(c), fr = fOf(c + step);
    for (size_t k = 0; k <= kFft / 2; ++k) {
      float f = (float)k * 16000.0f / (float)kFft;
      float w = 0.0f;
      if (f >= fl && f <= fc) w = (f - fl) / (fc - fl);
      else if (f > fc && f <= fr) w = (fr - f) / (fr - fc);
      mel_[b][k] = w;
    }
  }
}

void Kws::begin() {
  buildTables();
  loadTemplate();
  detected_ = false;
  inWord_ = false;
  wordLen_ = 0;
  silenceFrames_ = 0;
  noise_ = 0.0f;
  trainRep_ = 0;
  calibrating_ = false;
}

void Kws::loadTemplate() {
  gPrefs.begin("kws", true);
  size_t got = gPrefs.getBytes("tpl16", tpl_, sizeof(tpl_));
  gPrefs.end();
  hasTemplate_ = got == sizeof(tpl_);
}

void Kws::storeTemplate() {
  gPrefs.begin("kws", false);
  gPrefs.putBytes("tpl16", tpl_, sizeof(tpl_));
  gPrefs.end();
  hasTemplate_ = true;
}

void Kws::computeFeatures(const int16_t* samples, float* feat, float* energy) {
  float re[kFft];
  float im[kFft];
  for (size_t i = 0; i < kFft; ++i) {
    re[i] = (float)samples[i] * wnd_[i];
    im[i] = 0.0f;
  }

  for (size_t i = 0; i < kFft; ++i) {
    size_t j = rev_[i];
    if (j > i) {
      float t = re[i]; re[i] = re[j]; re[j] = t;
      t = im[i]; im[i] = im[j]; im[j] = t;
    }
  }
  for (size_t len = 2; len <= kFft; len <<= 1) {
    float ang = -2.0f * kPi / (float)len;
    float wc = cosf(ang), ws = sinf(ang);
    for (size_t i = 0; i < kFft; i += len) {
      float wr = 1.0f, wi = 0.0f;
      for (size_t k = 0; k < len / 2; ++k) {
        size_t a = i + k, b = i + k + len / 2;
        float tr = wr * re[b] - wi * im[b];
        float ti = wr * im[b] + wi * re[b];
        re[b] = re[a] - tr; im[b] = im[a] - ti;
        re[a] += tr; im[a] += ti;
        float nwr = wc * wr - ws * wi;
        wi = wc * wi + ws * wr;
        wr = nwr;
      }
    }
  }

  float total = 0.0f;
  for (size_t b = 0; b < kBands; ++b) {
    float acc = 0.0f;
    for (size_t k = 0; k <= kFft / 2; ++k) {
      float mag = re[k] * re[k] + im[k] * im[k];
      acc += mel_[b][k] * mag;
      total += mel_[b][k] * mag;
    }
    feat[b] = log10f(acc + 1e-8f);
  }
  feat[kBands] = log10f(total + 1e-8f);
  *energy = total;

  float ss = 0.0f;
  for (size_t i = 0; i < kFeat; ++i) ss += feat[i] * feat[i];
  ss = sqrtf(ss);
  if (ss > 1e-6f) {
    for (size_t i = 0; i < kFeat; ++i) feat[i] /= ss;
  }
}

void Kws::feed(const int16_t* pcm, size_t samples) {
  (void)samples;
  if (!ARCI_KWS_ENABLE) return;
  float feat[kFeat];
  float energy = 0.0f;
  computeFeatures(pcm, feat, &energy);
  pushFrame(feat, energy);
}

void Kws::pushFrame(const float* feat, float energy) {
  if (noise_ == 0.0f || energy < noise_) noise_ = energy;
  else noise_ += (energy - noise_) * 0.05f;

  float gate = noise_ * 4.0f;  // ~6 dB above ambient
  if (gate < 1e-4f) gate = 1e-4f;
  bool speech = energy > gate;

  if (!inWord_) {
    if (!speech) return;
    inWord_ = true;
    wordLen_ = 0;
    silenceFrames_ = 0;
  }

  if (speech) {
    silenceFrames_ = 0;
  } else {
    silenceFrames_++;
  }

  if (wordLen_ < ARCI_KWS_MAX_FRAMES) {
    memcpy(word_[wordLen_], feat, sizeof(word_[wordLen_]));
    wordLen_++;
  }

  if (wordLen_ >= ARCI_KWS_MAX_FRAMES || silenceFrames_ >= kSilenceEndFrames) {
    finishWord();
  }
}

void Kws::finishWord() {
  size_t n = wordLen_;
  inWord_ = false;
  wordLen_ = 0;
  silenceFrames_ = 0;
  if (n < ARCI_KWS_MIN_FRAMES) return;

  float slices[kSlices][kFeat];
  for (size_t s = 0; s < kSlices; ++s) {
    memset(slices[s], 0, sizeof(slices[s]));
    size_t a = s * n / kSlices, b = (s + 1) * n / kSlices;
    if (b <= a) b = a + 1;
    if (b > n) b = n;
    for (size_t i = a; i < b; ++i)
      for (size_t f = 0; f < kFeat; ++f) slices[s][f] += word_[i][f];
    float inv = 1.0f / (float)(b - a);
    float ss = 0.0f;
    for (size_t f = 0; f < kFeat; ++f) { slices[s][f] *= inv; ss += slices[s][f] * slices[s][f]; }
    ss = sqrtf(ss);
    if (ss > 1e-6f)
      for (size_t f = 0; f < kFeat; ++f) slices[s][f] /= ss;
  }

  if (calibrating_) {
    memcpy(train_[trainRep_], slices, sizeof(slices));
    trainRep_++;
    if (trainRep_ >= ARCI_KWS_TRAIN_REPS) {
      for (size_t s = 0; s < kSlices; ++s) {
        memset(tpl_[s], 0, sizeof(tpl_[s]));
        for (size_t r = 0; r < ARCI_KWS_TRAIN_REPS; ++r)
          for (size_t f = 0; f < kFeat; ++f) tpl_[s][f] += train_[r][s][f];
        float inv = 1.0f / (float)ARCI_KWS_TRAIN_REPS;
        float ss = 0.0f;
        for (size_t f = 0; f < kFeat; ++f) { tpl_[s][f] *= inv; ss += tpl_[s][f] * tpl_[s][f]; }
        ss = sqrtf(ss);
        if (ss > 1e-6f)
          for (size_t f = 0; f < kFeat; ++f) tpl_[s][f] /= ss;
      }
      storeTemplate();
      calibrating_ = false;
      Serial.println("[kws] wake template saved");
      printState();
    } else {
      Serial.printf("[kws] say jarvis #%u of %u\n", trainRep_ + 1, ARCI_KWS_TRAIN_REPS);
    }
    return;
  }

  if (!hasTemplate_) return;
  scoreWord(slices);
}

float Kws::cosine(const float* a, const float* b) const {
  float dot = 0.0f;
  for (size_t f = 0; f < kFeat; ++f) dot += a[f] * b[f];
  return dot;
}

void Kws::scoreWord(const float slices[kSlices][kFeat]) {
  float sim = 0.0f;
  for (size_t s = 0; s < kSlices; ++s) {
    sim += cosine(tpl_[s], slices[s]);
  }
  sim /= (float)kSlices;

  if (sim >= threshold_) {
    detected_ = true;
    lastDetectMs_ = millis();
    Serial.printf("[kws] wake word detected (score %.2f)\n", sim);
  }
  lastScore_ = sim;
}

void Kws::startCalibration() {
  if (calibrating_) {
    Serial.println("[kws] already calibrating");
    return;
  }
  calibrating_ = true;
  trainRep_ = 0;
  Serial.println("[kws] calibration: say \"jarvis\" naturally");
  Serial.printf("[kws] say jarvis #1 of %u\n", ARCI_KWS_TRAIN_REPS);
}

void Kws::printState() const {
  bool tmp = hasTemplate_;
  Serial.printf("kws state: template=%s threshold=%.2f detect=%d\n",
                tmp ? "yes" : "no", threshold_, detected_ ? 1 : 0);
  if (tmp) Serial.printf("kws last score: %.2f\n", lastScore_);
}

}  // namespace arcv