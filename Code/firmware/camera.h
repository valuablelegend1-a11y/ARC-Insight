#pragma once
#include <stddef.h>
#include <stdint.h>

#include <functional>

namespace arcv {

class Camera {
 public:
  typedef std::function<void(const uint8_t* jpeg, size_t len)> Sender;

  void onData(Sender s) { sender_ = std::move(s); }
  bool captureJpeg();
  void deinit();

 private:
  bool initInternal();
  Sender sender_;
  bool inited_ = false;
};

}  // namespace arcv