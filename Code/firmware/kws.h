#pragma once
#include <stddef.h>
#include <stdint.h>

#include "arcinsight_config.h"

namespace arcv {

// On-device "jarvis" keyword spotter (Option A). Self-contained, no model
// files: it builds a 16x13 acoustic template of the word, trains it from
// three spoken samples over the serial console, and template-matches live
// audio with a cosine similarity gate. Runs purely on the S3 while the
// radio is off, so the glasses only ever connect to the PC after the word
// is heard.
//
// Serial setup (once, at 115200 baud):
//   cal_wake     -> prompts "say jarvis" 3 times, then saves the template
//   state        -> prints whether a template exists and the last score
class Kws {
 public:
  void begin();
  bool hasTemplate() const { return hasTemplate_; }
  float threshold() const { return threshold_; }

  void feed(const int16_t* pcm, size_t samples);
  bool detected() const { return detected_; }
  void consumeDetected() { detected_ = false; }

  void startCalibration();
  bool calibrating() const { return calibrating_; }

  void printState() const;

 private:
  static constexpr size_t kFft = 256;
  static constexpr size_t kBands = 12;
  static constexpr size_t kFeat = 13;
  static constexpr size_t kSlices = ARCI_KWS_SLICES;
  static constexpr float kPi = 3.14159265358979f;

  void buildTables();
  void computeFeatures(const int16_t* samples, float* feat, float* energy);
  void pushFrame(const float* feat, float energy);
  void finishWord();
  void scoreWord(const float slices[kSlices][kFeat]);
  void storeTemplate();
  void loadTemplate();
  float cosine(const float* a, const float* b) const;

  float twCo_[kFft / 2];
  float wnd_[kFft];
  float mel_[kBands][kFft / 2 + 1];
  uint8_t rev_[kFft];

  float tpl_[kSlices][kFeat];
  bool hasTemplate_ = false;
  float threshold_ = ARCI_KWS_THRESHOLD;
  float lastScore_ = 0.0f;

  float word_[ARCI_KWS_MAX_FRAMES][kFeat];
  size_t wordLen_ = 0;
  bool inWord_ = false;
  size_t silenceFrames_ = 0;
  float noise_ = 0.0f;

  float train_[ARCI_KWS_TRAIN_REPS][kSlices][kFeat];
  size_t trainRep_ = 0;
  bool calibrating_ = false;

  bool detected_ = false;
  uint32_t lastDetectMs_ = 0;
  uint32_t lastFeedMs_ = 0;
  size_t feedCount_ = 0;
};

}  // namespace arcv