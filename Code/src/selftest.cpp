#include "selftest.h"

#include <Arduino.h>
#include <MAX30105.h>
#include <WiFi.h>
#include <Wire.h>
#include <driver/i2s.h>

#include <math.h>
#include <stdio.h>

#include "camera.h"
#include "pins.h"
#include "settings.h"

namespace arcv {

namespace {

void printPinMap() {
  char line[128];
  Serial.println("[selftest] wiring reference:");
  snprintf(line, sizeof(line),
           "  i2s   sclk=%d ws=%d din=%d out=%d (mic_word=%d)",
           ARCI_PIN_MIC_SCLK, ARCI_PIN_MIC_WS, ARCI_PIN_MIC_DIN,
           ARCI_PIN_AMP_DIN, ARCI_MIC_RX_WORD);
  Serial.println(line);
  snprintf(line, sizeof(line), "  i2c   sda=%d scl=%d int=%d",
           ARCI_PIN_I2C_SDA, ARCI_PIN_I2C_SCL, ARCI_PIN_HR_INT);
  Serial.println(line);
  snprintf(line, sizeof(line),
           "  cam   xclk=%d pclk=%d vsync=%d href=%d d7=%d d6=%d d5=%d "
           "d4=%d d3=%d d2=%d d1=%d d0=%d",
           ARCI_PIN_CAM_XCLK, ARCI_PIN_CAM_PCLK, ARCI_PIN_CAM_VSYNC,
           ARCI_PIN_CAM_HREF, ARCI_PIN_CAM_D7, ARCI_PIN_CAM_D6,
           ARCI_PIN_CAM_D5, ARCI_PIN_CAM_D4, ARCI_PIN_CAM_D3,
           ARCI_PIN_CAM_D2, ARCI_PIN_CAM_D1, ARCI_PIN_CAM_D0);
  Serial.println(line);
}

bool probeI2s() {
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
  cfg.bits_per_chan = I2S_BITS_PER_CHAN_32BIT;
  i2s_pin_config_t pins = {.bck_io_num = ARCI_PIN_MIC_SCLK,
                           .ws_io_num = ARCI_PIN_MIC_WS,
                           .data_out_num = ARCI_PIN_AMP_DIN,
                           .data_in_num = ARCI_PIN_MIC_DIN};
  if (i2s_driver_install(I2S_NUM_0, &cfg, 0, nullptr) != ESP_OK) return false;
  if (i2s_set_pin(I2S_NUM_0, &pins) != ESP_OK) return false;
  i2s_start(I2S_NUM_0);

  static int32_t mix[2000 * 2];
  static int16_t tone[2000];
  for (size_t i = 0; i < 2000; ++i) {
    float t = (float)i / (float)ARCI_WIRE_RATE_HZ;
    tone[i] = (int16_t)(sinf(2.0f * 3.14159265f * 880.0f * t) * 32767.0f * 0.08f);
  }
  for (int block = 0; block < 3; ++block) {
    for (size_t i = 0; i < 2000; ++i) {
      mix[2 * i] = (int32_t)tone[i] << 16;
      mix[2 * i + 1] = 0;
    }
    size_t written = 0;
    i2s_write(I2S_NUM_0, mix, sizeof(mix), &written, pdMS_TO_TICKS(50));
    delay(10);
  }

  static int32_t raw[256];
  size_t got = 0;
  if (i2s_read(I2S_NUM_0, raw, sizeof(raw), &got, pdMS_TO_TICKS(200)) != ESP_OK) {
    return false;
  }
  size_t frames = got / (2 * sizeof(int32_t));
  int16_t p0 = 0, p1 = 0;
  for (size_t i = 0; i < frames; ++i) {
    int16_t a = (int16_t)(raw[2 * i] >> 16);
    int16_t b = (int16_t)(raw[2 * i + 1] >> 16);
    if (a < 0) a = -a;
    if (b < 0) b = -b;
    if (a > p0) p0 = a;
    if (b > p1) p1 = b;
  }
  Serial.printf("[selftest] mic word0 peak=%d word1 peak=%d (%u frames)\n",
                p0, p1, (unsigned)frames);
  Serial.printf("[selftest] set ARCI_MIC_RX_WORD to the higher-peak word\n");
  i2s_stop(I2S_NUM_0);
  i2s_driver_uninstall(I2S_NUM_0);
  return frames > 0;
}

bool probeHr() {
  Wire.begin(ARCI_PIN_I2C_SDA, ARCI_PIN_I2C_SCL);
  MAX30105 sensor;
  if (!sensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.printf("[selftest]   no i2c response on sda=%d scl=%d\n",
                  ARCI_PIN_I2C_SDA, ARCI_PIN_I2C_SCL);
    return false;
  }
  sensor.setup(0x1F, 8, 2, 100, 411, 4096);
  int32_t minIr = 1 << 30, maxIr = 0;
  for (int i = 0; i < 5; ++i) {
    long v = sensor.getIR();
    if (v < minIr) minIr = v;
    if (v > maxIr) maxIr = v;
    delay(20);
  }
  Serial.printf("[selftest] ir raw min=%d max=%d\n", (int)minIr, (int)maxIr);
  sensor.shutDown();
  return true;
}

bool probeCamera() {
  Camera cam;
  size_t len = 0;
  cam.onData([&len](const uint8_t* data, size_t n) { len = n; });
  bool ok = cam.captureJpeg();
  if (ok) {
    Serial.printf("[selftest] camera frame %u bytes\n", (unsigned)len);
  } else {
    Serial.println("[selftest]   camera init/capture failed - check J6 map");
  }
  cam.deinit();
  return ok;
}

bool probeWifi() {
  settings().begin();
  std::string ssid = settings().wifiSsid();
  if (ssid.empty()) {
    Serial.println("[selftest]   wifi skipped (no credentials stored)");
    return true;
  }
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), settings().wifiPass().c_str());
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 12000) {
    delay(100);
  }
  bool ok = WiFi.status() == WL_CONNECTED;
  if (ok) {
    Serial.printf("[selftest] wifi rssi=%d\n", WiFi.RSSI());
  } else {
    Serial.println("[selftest]   wifi connect failed");
    WiFi.disconnect();
  }
  return ok;
}

}  // namespace

void runSelftest() {
  int pass = 0, fail = 0;
  auto stage = [&pass, &fail](bool ok, const char* label) {
    Serial.printf("[selftest] %s: %s\n", ok ? "PASS" : "FAIL", label);
    if (ok) {
      ++pass;
    } else {
      ++fail;
    }
  };

  printPinMap();
  stage(probeI2s(), "i2s bus");
  stage(probeHr(), "max30102");
  stage(probeCamera(), "camera");
  stage(probeWifi(), "wifi");
  Serial.printf("[selftest] result: %d pass, %d fail\n", pass, fail);
}

}  // namespace arcv