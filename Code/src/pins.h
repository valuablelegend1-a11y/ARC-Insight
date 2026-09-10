#pragma once
#include <stdint.h>

namespace arcv {

#define ARCI_PIN_MIC_SCLK 11
#define ARCI_PIN_MIC_WS   10
#define ARCI_PIN_AMP_DIN  12
#define ARCI_PIN_MIC_DIN   9

// Amplifier is driven on the left slot; mic data lands on ARCI_MIC_RX_WORD.
#define ARCI_MIC_RX_WORD   1

#define ARCI_PIN_I2C_SDA   4
#define ARCI_PIN_I2C_SCL   5
#define ARCI_PIN_HR_INT    2

#define ARCI_PIN_WAKE     18

#define ARCI_PIN_CAM_XCLK 13
#define ARCI_PIN_CAM_PCLK  8
#define ARCI_PIN_CAM_VSYNC 6
#define ARCI_PIN_CAM_HREF  7
#define ARCI_PIN_CAM_D7   39
#define ARCI_PIN_CAM_D6   40
#define ARCI_PIN_CAM_D5   41
#define ARCI_PIN_CAM_D4   42
#define ARCI_PIN_CAM_D3   38
#define ARCI_PIN_CAM_D2   45
#define ARCI_PIN_CAM_D1   21
#define ARCI_PIN_CAM_D0   14
#define ARCI_PIN_CAM_SIOD  4
#define ARCI_PIN_CAM_SIOC  5
#define ARCI_PIN_CAM_PWDN -1
#define ARCI_PIN_CAM_RESET -1

#define ARCI_WIRE_RATE_HZ  24000
#define ARCI_WIRE_BITS     (32)
#define ARCI_PCM_RATE_HZ   16000
#define ARCI_PCM_FRAME_MS  20
#define ARCI_PCM_FRAME_SAMPLES (ARCI_PCM_RATE_HZ * ARCI_PCM_FRAME_MS / 1000)

}  // namespace arcv