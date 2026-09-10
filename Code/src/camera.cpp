#include "camera.h"

#include "esp_camera.h"
#include "pins.h"

namespace arcv {

namespace {
camera_config_t gCamCfg = {};
}

bool Camera::initInternal() {
  gCamCfg.ledc_channel = LEDC_CHANNEL_0;
  gCamCfg.ledc_timer = LEDC_TIMER_0;
  gCamCfg.pin_d0 = ARCI_PIN_CAM_D0;
  gCamCfg.pin_d1 = ARCI_PIN_CAM_D1;
  gCamCfg.pin_d2 = ARCI_PIN_CAM_D2;
  gCamCfg.pin_d3 = ARCI_PIN_CAM_D3;
  gCamCfg.pin_d4 = ARCI_PIN_CAM_D4;
  gCamCfg.pin_d5 = ARCI_PIN_CAM_D5;
  gCamCfg.pin_d6 = ARCI_PIN_CAM_D6;
  gCamCfg.pin_d7 = ARCI_PIN_CAM_D7;
  gCamCfg.pin_xclk = ARCI_PIN_CAM_XCLK;
  gCamCfg.pin_pclk = ARCI_PIN_CAM_PCLK;
  gCamCfg.pin_vsync = ARCI_PIN_CAM_VSYNC;
  gCamCfg.pin_href = ARCI_PIN_CAM_HREF;
  gCamCfg.pin_sccb_sda = ARCI_PIN_CAM_SIOD;
  gCamCfg.pin_sccb_scl = ARCI_PIN_CAM_SIOC;
  gCamCfg.pin_pwdn = ARCI_PIN_CAM_PWDN;
  gCamCfg.pin_reset = ARCI_PIN_CAM_RESET;
  gCamCfg.xclk_freq_hz = 16000000;
  gCamCfg.pixel_format = PIXFORMAT_JPEG;
  gCamCfg.frame_size = FRAMESIZE_QVGA;
  gCamCfg.jpeg_quality = 12;
  gCamCfg.fb_count = 2;
  return esp_camera_init(&gCamCfg) == ESP_OK;
}

bool Camera::captureJpeg() {
  if (!inited_) {
    if (!initInternal()) return false;
    inited_ = true;
  }
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) return false;
  bool ok = true;
  if (sender_ && fb->buf && fb->len) sender_(fb->buf, fb->len);
  esp_camera_fb_return(fb);
  return ok;
}

void Camera::deinit() {
  if (!inited_) return;
  esp_camera_deinit();
  inited_ = false;
}

}  // namespace arcv