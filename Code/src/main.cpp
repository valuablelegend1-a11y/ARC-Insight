#include <Arduino.h>
#include <WiFi.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>

#include <stdio.h>
#include <string.h>

#include "arcinsight_config.h"
#include "audio_io.h"
#include "camera.h"
#include "heart.h"
#include "link.h"
#include "pins.h"
#include "protocol.h"
#include "selftest.h"
#include "settings.h"
#include "sleep.h"

using namespace arcv;

namespace {

Link gLink;
AudioIO gAudio;
Heart gHeart;
Camera gCamera;

bool gStreaming = false;
bool gWasConnected = false;
uint32_t gBootMs = 0;
uint32_t gLastCmdMs = 0;
uint32_t gOtaRecv = 0;
uint32_t gOtaSize = 0;
bool gOtaActive = false;
esp_ota_handle_t gOtaHandle = 0;
const esp_partition_t* gOtaPartition = nullptr;

void parseWifiSet(const char* text);
void handleOtaBegin(const char* text);

void goToSleep() {
  gCamera.deinit();
  gHeart.shutdown();
  gLink.disconnect();
  enterDeepSleep(ARCI_SLEEP_GUARD_HOURS * 3600);
}

void onPcm(const int16_t* pcm, size_t samples) {
  gLink.sendBinary((uint8_t)BinaryOp::AudioPcm, (const uint8_t*)pcm,
                   samples * 2);
}

void onHr(int bpm, float confidence) {
  gLink.sendText("{\"type\":\"hr\",\"sample\":{\"bpm\":%d,"
                 "\"confidence\":%.2f,\"timestamp\":%lu}}",
                 bpm, confidence, (unsigned long)(millis() / 1000));
}

void onJpeg(const uint8_t* jpeg, size_t len) {
  gLink.sendBinary((uint8_t)BinaryOp::ImageJpeg, jpeg, len);
}

void onBinary(uint8_t op, const uint8_t* data, size_t len) {
  switch ((BinaryOp)op) {
    case BinaryOp::AudioMp3:
      gAudio.feedMp3(data, len);
      gLastCmdMs = millis();
      break;
    case BinaryOp::FwChunk:
      if (gOtaActive && gOtaPartition) {
        if (esp_ota_write(gOtaHandle, data, len) == ESP_OK) {
          gOtaRecv += (uint32_t)len;
        }
        if (gOtaRecv >= gOtaSize) {
          esp_ota_end(gOtaHandle);
          esp_ota_set_boot_partition(gOtaPartition);
          esp_restart();
        }
      }
      break;
    default:
      break;
  }
}

void onText(const char* text, size_t len) {
  (void)len;
  gLastCmdMs = millis();
  if (strstr(text, "ping")) {
    gLink.sendText("{\"type\":\"pong\"}");
  } else if (strstr(text, "capture")) {
    gCamera.captureJpeg();
  } else if (strstr(text, "microphone_on")) {
    gStreaming = true;
    gAudio.setStreaming(true);
  } else if (strstr(text, "microphone_off")) {
    gStreaming = false;
    gAudio.setStreaming(false);
  } else if (strstr(text, "go_sleep")) {
    goToSleep();
  } else if (strstr(text, "ota_begin")) {
    handleOtaBegin(text);
  } else if (strstr(text, "wifi_set")) {
    parseWifiSet(text);
  } else if (strstr(text, "get_status")) {
    gLink.sendText("{\"type\":\"status\",\"heap\":%lu,"
                   "\"rssi\":%d,\"streaming\":%d}",
                   (unsigned long)ESP.getFreeHeap(), WiFi.RSSI(), gStreaming);
  }
}

void parseWifiSet(const char* text) {
  const char* ssid = strstr(text, "ssid:");
  const char* pass = strstr(text, "pass:");
  const char* host = strstr(text, "host:");
  char s[64] = {0};
  char p[64] = {0};
  char h[96] = {0};
  if (ssid) {
    sscanf(ssid, "ssid:%63[^|]", s);
  }
  if (pass) {
    sscanf(pass, "pass:%63[^|]", p);
  }
  if (host) {
    sscanf(host, "host:%95[^|]", h);
  }
  if (s[0] && p[0]) settings().setWifi(s, p);
  if (h[0]) settings().setPcHost(h);
}

void handleOtaBegin(const char* text) {
  uint32_t size = 0;
  const char* key = strstr(text, "\"size\"");
  if (key) {
    const char* colon = strchr(key + 6, ':');
    if (colon) sscanf(colon + 1, "%u", &size);
  }
  if (!size) {
    const char* m = strstr(text, "size:");
    if (m) sscanf(m, "size:%u", &size);
  }
  if (!size) sscanf(text, "ota_begin %*[^:] : %u", &size);
  if (!size) return;
  const esp_partition_t* part =
      esp_partition_find_first(ESP_PARTITION_TYPE_APP,
                               ESP_PARTITION_SUBTYPE_APP_OTA_1, nullptr);
  if (!part) return;
  if (esp_ota_begin(part, size, &gOtaHandle) != ESP_OK) return;
  gOtaPartition = part;
  gOtaSize = size;
  gOtaRecv = 0;
  gOtaActive = true;
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(120);
  Serial.println("ARC-INSIGHT v0.1");

#ifdef ARCI_SELFTEST
  runSelftest();
  Serial.println("[selftest] done");
  return;
#endif

  settings().begin();

  gLink.setHandlers(onBinary, onText);
  gAudio.onPcmReady(onPcm);
  gHeart.onData(onHr);
  gCamera.onData(onJpeg);

  gHeart.begin();
  gAudio.begin();

  WakeReason reason = wakeReason();
  bool linked = gLink.connect(settings().wifiSsid().c_str(),
                              settings().wifiPass().c_str(),
                              settings().pcHost().c_str(),
                              settings().pcPort());
  if (linked) {
    gStreaming = true;
    gAudio.setStreaming(true);
    gHeart.wake();
    if (reason == WakeReason::Comparator || reason == WakeReason::PowerOn) {
      gAudio.beep();
    }
  }
  gBootMs = millis();
  gLastCmdMs = millis();
}

void loop() {
#ifdef ARCI_SELFTEST
  delay(1000);
  return;
#endif

  gLink.loop();

  bool connectedNow = gLink.connected();
  if (connectedNow && !gWasConnected) {
    gLink.sendText("{\"type\":\"hello\",\"name\":\"arc-insight\","
                   "\"fw\":\"0.1\",\"wake\":%d}",
                   (int)wakeReason());
  }
  gWasConnected = connectedNow;

  if (connectedNow) {
    gAudio.poll();
    gHeart.poll(millis());
    if (millis() - gLastCmdMs > ARCI_IDLE_TIMEOUT_MS) {
      goToSleep();
    }
  } else if (millis() - gBootMs > ARCI_WS_TIMEOUT_MS) {
    goToSleep();
  }
}