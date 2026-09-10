#include "link.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <WiFi.h>
#include <WebSocketsClient.h>

#include "arcinsight_config.h"

namespace arcv {

struct Link::Impl {
  WebSocketsClient ws;
  char host[96];
  uint16_t port = 0;
};

namespace {
Link* gLink = nullptr;
}

void Link::setHandlers(BinaryHandler bin, TextHandler txt) {
  bin_ = std::move(bin);
  txt_ = std::move(txt);
}

void Link::onBinaryEvent(const uint8_t* data, size_t len) {
  if (len == 0) return;
  if (bin_) bin_(data[0], data + 1, len - 1);
}

void Link::onTextEvent(const char* text, size_t len) {
  if (txt_) txt_(text, len);
}

void Link::onDisconnected() { connecting_ = false; }

bool Link::connect(const char* ssid, const char* pass, const char* host,
                   uint16_t port) {
  if (!impl_) impl_ = new Impl();
  gLink = this;
  stable_ = false;
  lastTryMs_ = 0;

  if (WiFi.status() != WL_CONNECTED) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, pass);
    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 12000) {
      delay(100);
    }
    if (WiFi.status() != WL_CONNECTED) {
      WiFi.disconnect();
      return false;
    }
  }

  snprintf(impl_->host, sizeof(impl_->host), "%s", host);
  impl_->port = port;
  tryWsConnect(host, port);
  return WiFi.status() == WL_CONNECTED;
}

void Link::tryWsConnect(const char* host, uint16_t port) {
  IPAddress ip;
  if (WiFi.hostByName(host, ip)) {
    impl_->ws.begin(ip, port, ARCI_WS_PATH);
  } else {
    impl_->ws.begin(host, port, ARCI_WS_PATH);
  }
  impl_->ws.onEvent([](WStype_t type, uint8_t* payload, size_t length) {
    if (!gLink || !gLink->impl_) return;
    switch (type) {
      case WStype_CONNECTED:
        gLink->stable_ = true;
        gLink->stableSinceMs_ = millis();
        break;
      case WStype_DISCONNECTED:
      case WStype_ERROR:
        gLink->stable_ = false;
        gLink->onDisconnected();
        break;
      case WStype_BIN:
        gLink->onBinaryEvent(payload, length);
        break;
      case WStype_TEXT:
        gLink->onTextEvent((const char*)payload, length);
        break;
      default:
        break;
    }
  });
  impl_->ws.enableHeartbeat(15000, 4500, 2);
  impl_->ws.setReconnectInterval(2000);
  connecting_ = true;
}

void Link::loop() {
  if (!impl_) return;
  impl_->ws.loop();
  if (!stable_ && !connecting_ && millis() - lastTryMs_ > 3000) {
    lastTryMs_ = millis();
    connecting_ = true;
    tryWsConnect(impl_->host, impl_->port);
  }
}

bool Link::connected() const {
  return impl_ && stable_ && WiFi.status() == WL_CONNECTED;
}

void Link::disconnect() {
  if (!impl_) return;
  impl_->ws.disconnect();
  stable_ = false;
}

bool Link::sendBinary(uint8_t op, const uint8_t* data, size_t len) {
  if (!impl_ || !stable_) return false;
  uint8_t* frame = (uint8_t*)malloc(len + 1);
  if (!frame) return false;
  frame[0] = op;
  if (len) memcpy(frame + 1, data, len);
  bool ok = impl_->ws.sendBIN(frame, len + 1);
  free(frame);
  return ok;
}

void Link::sendText(const char* fmt, ...) {
  if (!impl_ || !stable_) return;
  char buf[256];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  impl_->ws.sendTXT(buf, strlen(buf));
}

}  // namespace arcv