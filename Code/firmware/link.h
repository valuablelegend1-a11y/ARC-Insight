#pragma once
#include <stddef.h>
#include <stdint.h>

#include <functional>

namespace arcv {

class Link {
 public:
  typedef std::function<void(uint8_t op, const uint8_t* data, size_t len)>
      BinaryHandler;
  typedef std::function<void(const char* text, size_t len)> TextHandler;

  void setHandlers(BinaryHandler bin, TextHandler txt);

  bool connect(const char* ssid, const char* pass, const char* host,
               uint16_t port);
  void loop();
  bool connected() const;
  void disconnect();
  bool sendBinary(uint8_t op, const uint8_t* data, size_t len);
  void sendText(const char* fmt, ...);
  bool linkStable() const { return stable_; }

 private:
  void tryWsConnect(const char* host, uint16_t port);
  void onBinaryEvent(const uint8_t* data, size_t len);
  void onTextEvent(const char* text, size_t len);
  void onDisconnected();

  struct Impl;
  Impl* impl_ = nullptr;
  BinaryHandler bin_;
  TextHandler txt_;
  bool stable_ = false;
  bool connecting_ = false;
  uint32_t lastTryMs_ = 0;
  uint32_t stableSinceMs_ = 0;
};

}  // namespace arcv