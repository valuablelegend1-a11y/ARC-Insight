#include "heart.h"

#include <algorithm>

#include <Wire.h>

#include "MAX30105.h"
#include "heartRate.h"
#include "pins.h"

namespace arcv {

namespace {
MAX30105 gSensor;
}

void Heart::begin() {
  Wire.begin(ARCI_PIN_I2C_SDA, ARCI_PIN_I2C_SCL);
  if (!gSensor.begin(Wire, I2C_SPEED_FAST)) return;
  gSensor.setup(0x1F, 8, 2, 100, 411, 4096);
  present_ = true;
  shutdown();
}

void Heart::wake() {
  if (!present_) return;
  gSensor.wakeUp();
  ibiCount_ = 0;
  smoothedBpm_ = 0;
  lastBeatMs_ = millis();
}

void Heart::shutdown() {
  if (present_) gSensor.shutDown();
}

void Heart::poll(uint32_t nowMs) {
  if (!present_ || nowMs - lastHrMs_ < 10) return;
  lastHrMs_ = nowMs;

  long ir = gSensor.getIR();
  if (checkForBeat((long)ir)) {
    long ibi = (long)(nowMs - lastBeatMs_);
    if (ibi > 300 && ibi < 1200) {
      ibi_[ibiCount_ % 10] = (int)ibi;
      ibiCount_++;
    }
    lastBeatMs_ = nowMs;
  }

  if (ibiCount_ > 2 && nowMs - lastBeatMs_ > 1500) {
    size_t n = ibiCount_ < 10 ? ibiCount_ : 10;
    int sum = 0;
    int minV = 1 << 30;
    int maxV = 0;
    for (size_t i = 0; i < n; ++i) {
      sum += ibi_[i];
      minV = std::min(minV, ibi_[i]);
      maxV = std::max(maxV, ibi_[i]);
    }
    int bpm = (int)(60000.0f * n / (float)sum);
    if (bpm >= 40 && bpm <= 200) {
      int ibiMean = sum / (int)n;
      float spread = (float)(maxV - minV) / (float)(ibiMean ? ibiMean : 1);
      float confidence = 0.9f - spread * 0.35f;
      if (confidence < 0.3f) confidence = 0.3f;
      if (confidence > 0.95f) confidence = 0.95f;
      if (sender_) sender_(bpm, confidence);
    }
    ibiCount_ = 0;
  }
}

}  // namespace arcv