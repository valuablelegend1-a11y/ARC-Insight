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
#include "kws.h"
#include "link.h"
#include "pins.h"
#include "protocol.h"
#include "selftest.h"
#include "settings.h"
#include "sleep.h"
#include "touch.h"

using namespace arcv;

namespace {

Link gLink;
AudioIO gAudio;
Heart gHeart;
Camera gCamera;
Touch gTouch;
Kws gKws;

enum class DevState { Listen, Active };
DevState gState = DevState::Listen;
bool gStreaming = false;
bool gSnapshotPending = false;
uint32_t gLastCmdMs = 0;
uint32_t gStateSinceMs = 0;
uint32_t gOtaRecv = 0;
uint32_t gOtaSize = 0;
bool gOtaActive = false;
esp_ota_handle_t gOtaHandle = 0;
const esp_partition_t* gOtaPartition = nullptr;

void finalizeOta() {
  esp_err_t err = esp_ota_end(gOtaHandle);
  gOtaActive = false;
  if (err != ESP_OK) {
    gLink.sendText("{\"type\":\"ota_event\",\"event\":\"device_error\","
                   "\"detail\":\"verify failed (%d)\"}",
                   (int)err);
    return;
  }
  gLink.sendText("{\"type\":\"ota_event\",\"event\":\"ota_done\"}");
  esp_ota_set_boot_partition(gOtaPartition);
  esp_restart();
}

bool linkConnection() {
  return gLink.connect(settings().wifiSsid().c_str(),
                       settings().wifiPass().c_str(),
                       settings().pcHost().c_str(),
                       settings().pcPort());
}

void enterListen() {
  gLink.disconnect();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  gHeart.shutdown();
  gCamera.deinit();
  gAudio.setStreaming(true);  // mic -> on-device wake-word listener
  gState = DevState::Listen;
  gStateSinceMs = millis();
  Serial.println("[mode] listen (wi-fi off, wake word armed)");
}

void enterActive() {
  if (!linkConnection()) {
    enterListen();
    return;
  }
  gHeart.wake();
  gAudio.setStreaming(true);  // mic -> streamed up to Jarvis
  gState = DevState::Active;
  gStateSinceMs = millis();
  gLastCmdMs = millis();
  gLink.sendText("{\"type\":\"hello\",\"name\":\"arc-insight\",\"fw\":\"0.1\","
                 "\"wake\":%d}",
                 (int)wakeReason());
  gAudio.beep();
  Serial.println("[mode] active (streaming to jarvis)");
}

void enterDeepSleepNow() {
  gAudio.setStreaming(false);
  gAudio.beep();
  delay(120);
  gCamera.deinit();
  gHeart.shutdown();
  gLink.disconnect();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  Serial.println("[mode] deep sleep (voice wake only)");
  enterDeepSleep(ARCI_SLEEP_GUARD_HOURS * 3600);
}

void doSnapshot() {
  bool wasActive = (gState == DevState::Active);
  if (!wasActive && !linkConnection()) {
    enterListen();
    return;
  }
  gSnapshotPending = true;
  gCamera.captureJpeg();
  gSnapshotPending = false;
  if (!wasActive) {
    uint32_t t0 = millis();
    while (millis() - t0 < 250) {
      gLink.loop();
      delay(2);
    }
    enterListen();
  }
}

void onPcm(const int16_t* pcm, size_t samples) {
  if (gState == DevState::Active) {
    gLink.sendBinary((uint8_t)BinaryOp::AudioPcm, (const uint8_t*)pcm,
                     samples * 2);
  } else {
    gKws.feed(pcm, samples);
  }
}

void onHr(int bpm, float confidence) {
  if (gState != DevState::Active) return;
  gLink.sendText("{\"type\":\"hr\",\"sample\":{\"bpm\":%d,"
                 "\"confidence\":%.2f,\"timestamp\":%lu}}",
                 bpm, confidence, (unsigned long)(millis() / 1000));
}

void onJpeg(const uint8_t* jpeg, size_t len) {
  if (gSnapshotPending) {
    gLink.sendBinary((uint8_t)BinaryOp::Snapshot, jpeg, len);
  } else {
    gLink.sendBinary((uint8_t)BinaryOp::ImageJpeg, jpeg, len);
  }
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
          finalizeOta();
        }
      }
      break;
    default:
      break;
  }
}

void parseWifiSet(const char* text);
void handleOtaBegin(const char* text);
void handleOtaEnd(const char* text);
void handleOtaAbort();

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
    enterDeepSleepNow();
  } else if (strstr(text, "ota_begin")) {
    handleOtaBegin(text);
  } else if (strstr(text, "ota_end")) {
    handleOtaEnd(text);
  } else if (strstr(text, "ota_abort")) {
    handleOtaAbort();
  } else if (strstr(text, "wifi_set")) {
    parseWifiSet(text);
  } else if (strstr(text, "get_status")) {
    gLink.sendText("{\"type\":\"status\",\"heap\":%lu,"
                   "\"rssi\":%d,\"streaming\":%d,\"mode\":%d}",
                   (unsigned long)ESP.getFreeHeap(), WiFi.RSSI(), gStreaming,
                   (int)gState);
  }
}

void parseWifiSet(const char* text) {
  const char* ssid = strstr(text, "ssid:");
  const char* pass = strstr(text, "pass:");
  const char* host = strstr(text, "host:");
  char s[64] = {0};
  char p[64] = {0};
  char h[96] = {0};
  if (ssid) sscanf(ssid, "ssid:%63[^|]", s);
  if (pass) sscanf(pass, "pass:%63[^|]", p);
  if (host) sscanf(host, "host:%95[^|]", h);
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

void handleOtaEnd(const char* text) {
  (void)text;
  if (!gOtaActive || !gOtaPartition) return;
  if (gOtaRecv < gOtaSize) {
    gLink.sendText("{\"type\":\"ota_event\",\"event\":\"device_error\","
                   "\"detail\":\"short transfer (%u of %u bytes)\"}",
                   (unsigned)gOtaRecv, (unsigned)gOtaSize);
    esp_ota_abort(gOtaHandle);
    gOtaActive = false;
    return;
  }
  finalizeOta();
}

void handleOtaAbort() {
  if (!gOtaActive || !gOtaPartition) return;
  esp_ota_abort(gOtaHandle);
  gOtaActive = false;
}

void handleGesture(Touch::Gesture g) {
  switch (g) {
    case Touch::Gesture::Tap:
      if (gState == DevState::Active) {
        Serial.println("[gesture] single tap -> listen");
        enterListen();
      }
      break;
    case Touch::Gesture::DoubleTap:
      Serial.println("[gesture] double tap -> snapshot");
      doSnapshot();
      break;
    case Touch::Gesture::Hold:
      Serial.println("[gesture] hold 5s -> deep sleep");
      enterDeepSleepNow();
      break;
    default:
      break;
  }
}

void handleSerial() {
  if (!Serial.available()) return;
  static String line;
  while (Serial.available()) {
    int c = Serial.read();
    if (c < 0) break;
    if (c == '\n') {
      line.trim();
      if (line == "cal_wake") {
        gKws.startCalibration();
      } else if (line == "state") {
        gKws.printState();
        Serial.printf("mode: %d (0=listen 1=active)\n", (int)gState);
      }
      line = "";
    } else {
      line += (char)c;
    }
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(120);
  Serial.println("ARC-INSIGHT v0.2");

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
  gTouch.begin();
  gKws.begin();

  if (ARCI_KWS_ENABLE && !gKws.hasTemplate()) {
    Serial.println("[kws] no wake template yet: send \"cal_wake\" on serial");
  }

  enterListen();
}

void loop() {
#ifdef ARCI_SELFTEST
  delay(1000);
  return;
#endif

  handleSerial();

  Touch::Gesture g = gTouch.poll(millis());
  if (g != Touch::Gesture::None) handleGesture(g);

  gLink.loop();
  gAudio.poll();  // PCM goes to kws (listen) or the link (active)

  if (gState == DevState::Active) {
    if (gLink.connected()) {
      gHeart.poll(millis());
      if (millis() - gLastCmdMs > ARCI_IDLE_TIMEOUT_MS) {
        Serial.println("[idle] returning to listen");
        enterListen();
      }
    } else if (millis() - gStateSinceMs > ARCI_WS_TIMEOUT_MS) {
      enterListen();
    }
  } else {
    if (ARCI_KWS_ENABLE && gKws.detected()) {
      gKws.consumeDetected();
      enterActive();
    }
    if (millis() - gStateSinceMs >
        (uint32_t)ARCI_LISTEN_GUARD_HOURS * 3600000UL) {
      Serial.println("[guard] long idle, deep sleep");
      enterDeepSleepNow();
    }
  }
}