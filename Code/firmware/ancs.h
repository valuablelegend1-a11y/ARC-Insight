#pragma once
#include <stddef.h>
#include <stdint.h>

#include <functional>

namespace arcv {

class Ancs {
 public:
  struct Msg {
    uint32_t uid = 0;
    uint8_t category = 0;
    char app[32] = {0};
    char source[72] = {0};
    char text[240] = {0};
  };

  typedef std::function<void(const Msg& msg)> NotifyHandler;

  void begin();
  void poll();
  void setHandler(NotifyHandler h) { handler_ = h; }

  void onNotifyEvent(const uint8_t* data, size_t len);
  void onDataSource(const uint8_t* data, size_t len);
  void flushDataSource();

 private:
  NotifyHandler handler_;
};

}