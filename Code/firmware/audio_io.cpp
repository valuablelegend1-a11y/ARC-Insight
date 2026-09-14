#include "audio_io.h"

#include <math.h>
#include <string.h>

#include "driver/i2s.h"
#include "esp_err.h"
#include "mp3_decode.h"
#include "pins.h"

namespace arcv {

namespace {
Mp3Decoder gMp3;
const i2s_port_t kI2sPort = I2S_NUM_0;
}

AudioIO::AudioIO()
    : dec_(96, (float)ARCI_WIRE_RATE_HZ, (float)ARCI_PCM_RATE_HZ, 0.40f) {}

AudioIO::~AudioIO() {}

bool AudioIO::begin() {
  if (i2sReady_) return true;

  i2s_config_t cfg = {};
  cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_RX);
  cfg.sample_rate = ARCI_WIRE_RATE_HZ;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  cfg.dma_buf_count = 6;
  cfg.dma_buf_len = 240;
  cfg.use_apll = false;
  cfg.tx_desc_auto_clear = true;
  cfg.mclk_multiple = I2S_MCLK_MULTIPLE_DEFAULT;
  cfg.bits_per_chan = I2S_BITS_PER_CHAN_32BIT;

  i2s_pin_config_t pins = {.bck_io_num = ARCI_PIN_MIC_SCLK,
                           .ws_io_num = ARCI_PIN_MIC_WS,
                           .data_out_num = ARCI_PIN_AMP_DIN,
                           .data_in_num = ARCI_PIN_MIC_DIN};

  if (i2s_driver_install(kI2sPort, &cfg, 0, NULL) != ESP_OK) return false;
  if (i2s_set_pin(kI2sPort, &pins) != ESP_OK) return false;
  if (i2s_start(kI2sPort) != ESP_OK) return false;

  if (!gMp3.begin()) return false;
  gMp3.setSink(
      [](const int16_t* pcm, size_t samples, void* ctx) {
        static_cast<AudioIO*>(ctx)->playPcm16(pcm, samples);
      },
      this);

  dec_.reset();
  pcmFrameUsed_ = 0;
  i2sReady_ = true;
  return true;
}

void AudioIO::setStreaming(bool on) { streaming_ = on; }

void AudioIO::readMicBlock() {
  size_t got = 0;
  esp_err_t err =
      i2s_read(kI2sPort, raw_, sizeof(raw_), &got, pdMS_TO_TICKS(4));
  if (err != ESP_OK) return;
  size_t frames = got / (2 * sizeof(int32_t));
  // Stereo frame [L,R]: amplifier on left, mic on ARCI_MIC_RX_WORD.
  for (size_t i = 0; i < frames; ++i) {
    int16_t sample = (int16_t)(raw_[2 * i + ARCI_MIC_RX_WORD] >> 16);
    dec_.push(sample);
  }
}

void AudioIO::poll() {
  if (!i2sReady_) return;
  if (streaming_) {
    readMicBlock();
    int16_t out[512];
    size_t n = dec_.pull(out, 512);
    if (n == 0) return;
    for (size_t i = 0; i < n; ++i) {
      pcmFrame_[pcmFrameUsed_++] = out[i];
      if (pcmFrameUsed_ == ARCI_PCM_FRAME_SAMPLES) {
        if (pcmHandler_) pcmHandler_(pcmFrame_, pcmFrameUsed_);
        pcmFrameUsed_ = 0;
      }
    }
  } else {
    dec_.reset();
    pcmFrameUsed_ = 0;
  }
}

void AudioIO::feedMp3(const uint8_t* data, size_t len) { gMp3.push(data, len); }

void AudioIO::playPcm16(const int16_t* pcm, size_t samples) {
  if (!i2sReady_ || samples == 0) return;
  static int32_t mix[1152 * 2];
  if (samples > 1152) samples = 1152;
  for (size_t i = 0; i < samples; ++i) {
    mix[2 * i] = (int32_t)pcm[i] << 16;  // left slot -> amplifier
    mix[2 * i + 1] = 0;                  // right slot silent
  }
  size_t written = 0;
  i2s_write(kI2sPort, mix, samples * 2 * sizeof(int32_t), &written,
            pdMS_TO_TICKS(50));
}

void AudioIO::beep() {
  const size_t n = 160;
  int16_t burst[160];
  for (size_t i = 0; i < n; ++i) {
    float t = (float)i / (float)ARCI_WIRE_RATE_HZ;
    float v = sinf(2.0f * 3.14159265f * 880.0f * t) * 0.06f;
    burst[i] = (int16_t)(v * 32767.0f);
  }
  playPcm16(burst, n);
}

}  // namespace arcv