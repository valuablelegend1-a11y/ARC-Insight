#include "ancs.h"

#include <Arduino.h>
#include <string.h>

#include "NimBLEDevice.h"

#include "arcinsight_config.h"

namespace arcv {
namespace {

const NimBLEUUID kAncsServiceUUID("7905F431-B5CE-4E99-A40F-4B1E122D00D0");
const NimBLEUUID kNotificationSourceUUID("9FBF120D-6301-42D9-8C58-25E699A21DBD");
const NimBLEUUID kControlPointUUID("69D1D8F3-45E1-49A8-9821-9BBDFDAAD9D9");
const NimBLEUUID kDataSourceUUID("22EAC6E9-24D6-4BB5-BE44-B36ACE7C7BFB");

const uint8_t kEventNotificationRemoved = 2;
const uint8_t kCommandGetNotificationAttributes = 0;
const uint16_t kAttributeTitle = 1;
const uint16_t kAttributeSubtitle = 2;
const uint16_t kAttributeMessage = 3;
const size_t kDataSourceCap = 1024;
const uint32_t kRetryMs = 1200;
const uint32_t kResponseTimeoutMs = 5000;
const uint32_t kFlushMs = 150;

Ancs* gSelf = nullptr;

void sanitize(char* dst, size_t cap, const uint8_t* src, size_t len) {
  if (cap == 0) return;
  size_t n = 0;
  for (size_t i = 0; i < len && n + 1 < cap; ++i) {
    uint8_t c = src[i];
    if (c == '"' || c == '\\' || c < 0x20) {
      c = ' ';
    }
    dst[n++] = (char)c;
  }
  dst[n] = '\0';
}

struct AncsCore : public NimBLEServerCallbacks {
  NimBLEClient* client = nullptr;
  NimBLERemoteService* service = nullptr;
  NimBLERemoteCharacteristic* notifySource = nullptr;
  NimBLERemoteCharacteristic* controlPoint = nullptr;
  NimBLERemoteCharacteristic* dataSource = nullptr;
  bool subscribed = false;
  bool secured = false;
  bool wantDiscover = false;
  bool requesting = false;
  uint32_t lastTryMs = 0;
  uint32_t lastRequestMs = 0;

  uint8_t dsBuffer[kDataSourceCap];
  size_t dsLen = 0;
  uint32_t dsLastMs = 0;
  uint32_t pendingUid = 0;
  uint8_t pendingCategory = 0;

  void onConnect(NimBLEServer* server, NimBLEConnInfo& connInfo) override {
    Serial.printf("[ancs] connection from %s\n",
                  connInfo.getAddress().toString().c_str());
    client = server->getClient(connInfo);
    service = nullptr;
    notifySource = nullptr;
    controlPoint = nullptr;
    dataSource = nullptr;
    subscribed = false;
    wantDiscover = true;
    requesting = false;
    dsLen = 0;
  }

  void onDisconnect(NimBLEServer* server, NimBLEConnInfo& connInfo,
                    int reason) override {
    Serial.printf("[ancs] disconnected %s (%d)\n",
                  connInfo.getAddress().toString().c_str(), reason);
    client = nullptr;
    service = nullptr;
    notifySource = nullptr;
    controlPoint = nullptr;
    dataSource = nullptr;
    subscribed = false;
    secured = false;
    requesting = false;
    dsLen = 0;
  }

  void onAuthenticationComplete(NimBLEConnInfo& connInfo) override {
    secured = connInfo.isEncrypted() || connInfo.isBonded();
    Serial.printf("[ancs] security complete encrypted=%d bonded=%d\n",
                  (int)connInfo.isEncrypted(), (int)connInfo.isBonded());
  }

  void tryAncs() {
    if (client == nullptr) return;
    if (service == nullptr) {
      if (wantDiscover) {
        client->discoverAttributes();
        wantDiscover = false;
      }
      service = client->getService(kAncsServiceUUID);
      if (service == nullptr) return;
      Serial.println("[ancs] ANCS service found");
      notifySource = service->getCharacteristic(kNotificationSourceUUID);
      controlPoint = service->getCharacteristic(kControlPointUUID);
      dataSource = service->getCharacteristic(kDataSourceUUID);
      if (notifySource == nullptr || controlPoint == nullptr ||
          dataSource == nullptr) {
        Serial.println("[ancs] ANCS characteristics missing");
        service = nullptr;
        wantDiscover = false;
        return;
      }
      Serial.println("[ancs] ANCS characteristics found");
    }
    dataSource->subscribe(
        true, [](NimBLERemoteCharacteristic* c, uint8_t* d, size_t n,
                 bool) {
          if (gSelf) gSelf->onDataSource(d, n);
        });
    notifySource->subscribe(
        true, [](NimBLERemoteCharacteristic* c, uint8_t* d, size_t n,
                 bool) {
          if (gSelf) gSelf->onNotifyEvent(d, n);
        });
    subscribed = true;
    Serial.println("[ancs] subscribed to notification source");
  }
};

AncsCore gCore;

}  // namespace

void Ancs::begin() {
  gSelf = this;
  NimBLEDevice::init(ARCI_ANCS_DEV_NAME);
  NimBLEDevice::setSecurityAuth(true, true, true);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_YESNO);
  NimBLEServer* server = NimBLEDevice::createServer();
  server->setCallbacks(&gCore);
  server->advertiseOnDisconnect(true);
  NimBLEAdvertising* adv = server->getAdvertising();
  NimBLEAdvertisementData advData;
  advData.setFlags(0x06);
  advData.addServiceUUID(kAncsServiceUUID);
  adv->setAdvertisementData(advData);
  NimBLEAdvertisementData scanData;
  scanData.setName(ARCI_ANCS_DEV_NAME);
  adv->setScanResponseData(scanData);
  adv->start();
  Serial.printf("[ancs] advertising as %s\n", ARCI_ANCS_DEV_NAME);
}

void Ancs::poll() {
  if (gCore.client == nullptr || !gCore.client->isConnected()) return;
  if (!gCore.secured && gCore.client->getConnInfo().isEncrypted()) {
    gCore.secured = true;
  }
  if (!gCore.secured) return;
  if (!gCore.subscribed) {
    if (millis() - gCore.lastTryMs < kRetryMs) return;
    gCore.lastTryMs = millis();
    gCore.tryAncs();
  } else if (gCore.dsLen && millis() - gCore.dsLastMs > kFlushMs) {
    flushDataSource();
  }
  if (gCore.requesting && millis() - gCore.lastRequestMs > kResponseTimeoutMs) {
    gCore.requesting = false;
  }
}

void Ancs::onNotifyEvent(const uint8_t* data, size_t len) {
  if (len < 8) return;
  uint8_t event = data[0];
  if (event == kEventNotificationRemoved) return;
  if (gCore.requesting) return;
  if (gCore.controlPoint == nullptr) return;
  gCore.requesting = true;
  gCore.lastRequestMs = millis();
  gCore.pendingUid = uint32_t(data[4]) | (uint32_t(data[5]) << 8) |
                     (uint32_t(data[6]) << 16) | (uint32_t(data[7]) << 24);
  gCore.pendingCategory = data[2];
  uint8_t cmd[16];
  size_t n = 0;
  cmd[n++] = kCommandGetNotificationAttributes;
  cmd[n++] = data[4];
  cmd[n++] = data[5];
  cmd[n++] = data[6];
  cmd[n++] = data[7];
  cmd[n++] = (uint8_t)kAttributeTitle;
  cmd[n++] = ARCI_ANCS_TITLE_MAX & 0xFF;
  cmd[n++] = (ARCI_ANCS_TITLE_MAX >> 8) & 0xFF;
  cmd[n++] = (uint8_t)kAttributeSubtitle;
  cmd[n++] = ARCI_ANCS_TITLE_MAX & 0xFF;
  cmd[n++] = (ARCI_ANCS_TITLE_MAX >> 8) & 0xFF;
  cmd[n++] = (uint8_t)kAttributeMessage;
  cmd[n++] = ARCI_ANCS_TEXT_MAX & 0xFF;
  cmd[n++] = (ARCI_ANCS_TEXT_MAX >> 8) & 0xFF;
  bool ok = gCore.controlPoint->writeValue(cmd, n, true);
  Serial.printf("[ancs] event=%d uid=%lu category=%d request=%s\n",
                (int)event, (unsigned long)gCore.pendingUid,
                (int)gCore.pendingCategory, ok ? "ok" : "failed");
  if (!ok) gCore.requesting = false;
}

void Ancs::onDataSource(const uint8_t* data, size_t len) {
  if (len == 0) return;
  if (gCore.dsLen + len > kDataSourceCap) gCore.dsLen = 0;
  memcpy(gCore.dsBuffer + gCore.dsLen, data, len);
  gCore.dsLen += len;
  gCore.dsLastMs = millis();
  uint16_t mtu = gCore.client ? gCore.client->getMTU() : 0;
  size_t fullLen = (mtu > 3) ? (size_t)(mtu - 3) : 509;
  if (len < fullLen) flushDataSource();
}

void Ancs::flushDataSource() {
  if (gCore.dsLen < 5) {
    gCore.dsLen = 0;
    gCore.requesting = false;
    return;
  }
  if (gCore.dsBuffer[0] != kCommandGetNotificationAttributes) {
    gCore.dsLen = 0;
    gCore.requesting = false;
    return;
  }
  Msg msg;
  msg.uid = gCore.pendingUid;
  msg.category = gCore.pendingCategory;
  size_t pos = 5;
  while (pos + 3 <= gCore.dsLen) {
    uint8_t id = gCore.dsBuffer[pos];
    uint16_t attrLen = uint16_t(gCore.dsBuffer[pos + 1]) |
                       (uint16_t(gCore.dsBuffer[pos + 2]) << 8);
    pos += 3;
    if (pos + attrLen > gCore.dsLen) break;
    const uint8_t* value = gCore.dsBuffer + pos;
    if (id == 0) {
      sanitize(msg.app, sizeof(msg.app), value, attrLen);
    } else if (id == (uint8_t)kAttributeTitle ||
               id == (uint8_t)kAttributeSubtitle) {
      if (msg.source[0] == '\0') {
        sanitize(msg.source, sizeof(msg.source), value, attrLen);
      }
    } else if (id == (uint8_t)kAttributeMessage) {
      sanitize(msg.text, sizeof(msg.text), value, attrLen);
    }
    pos += attrLen;
  }
  gCore.dsLen = 0;
  gCore.requesting = false;
  if (handler_) handler_(msg);
}

}  // namespace arcv