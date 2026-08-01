#pragma once

#include <stdint.h>

// This header is pre-included when TFT_eSPI is compiled and is also consumed
// by the ASkompu board profile. Keep the ILI9488 wiring in this one place.
#define TFT_BL 9
#define TFT_SCLK 10
#define TFT_MOSI 11
#define TFT_DC 12
#define TFT_RST 13
#define TFT_CS 14
// The display SDO pin is physically unconnected. TFT_eSPI rewrites MISO=-1 to
// MOSI on ESP32-S3, which gives SPI.begin() duplicate GPIOs and can panic.
// GPIO0 is therefore a harmless input-only dummy for the library after boot.
#define TFT_MISO 0
#define TFT_BACKLIGHT_ON 1

#ifdef __cplusplus
namespace Esp32S3Ili9488Pins {

constexpr uint8_t BUTTON_LEFT = 4;
constexpr uint8_t BUTTON_UP = 5;
constexpr uint8_t BUTTON_DOWN = 6;
constexpr uint8_t BUTTON_RIGHT = 7;
constexpr uint8_t LIGHT_SWITCH = 8;

constexpr uint8_t DISPLAY_BACKLIGHT = TFT_BL;
constexpr uint8_t DISPLAY_SCLK = TFT_SCLK;
constexpr uint8_t DISPLAY_MOSI = TFT_MOSI;
constexpr uint8_t DISPLAY_DC = TFT_DC;
constexpr uint8_t DISPLAY_RESET = TFT_RST;
constexpr uint8_t DISPLAY_CS = TFT_CS;
constexpr int8_t DISPLAY_MISO = -1;
constexpr bool DISPLAY_BACKLIGHT_ACTIVE_HIGH = TFT_BACKLIGHT_ON == 1;

constexpr uint8_t BUTTON_POINT = 15;
constexpr uint8_t BUTTON_AT = 16;
constexpr uint8_t BUTTON_TRIP1_RESET = 17;
constexpr uint8_t BUTTON_TRIP2_RESET = 39;
constexpr uint8_t PULSE_INPUT = 40;
constexpr bool PULSE_ACTIVE_LOW = true;
constexpr uint8_t REVERSE_INPUT = 41;
constexpr uint8_t BUTTON_FOOT_RESET = 42;
constexpr uint8_t EXTERNAL_TRIP_TX_RESERVED = 47;
constexpr uint8_t EXTERNAL_TRIP_RX_RESERVED = 48;

constexpr uint8_t UART0_TX = 43;
constexpr uint8_t UART0_RX = 44;
constexpr bool NATIVE_USB_ENABLED = true;

constexpr uint64_t pinBit(uint8_t pin) { return UINT64_C(1) << pin; }
constexpr uint8_t bitCount(uint64_t value) {
  return value == 0 ? 0 : static_cast<uint8_t>((value & 1U) +
                                               bitCount(value >> 1U));
}

constexpr uint64_t USED_GPIO_MASK =
    pinBit(BUTTON_LEFT) | pinBit(BUTTON_UP) | pinBit(BUTTON_DOWN) |
    pinBit(BUTTON_RIGHT) | pinBit(LIGHT_SWITCH) |
    pinBit(DISPLAY_BACKLIGHT) |
    pinBit(DISPLAY_SCLK) | pinBit(DISPLAY_MOSI) | pinBit(DISPLAY_DC) |
    pinBit(DISPLAY_RESET) | pinBit(DISPLAY_CS) | pinBit(BUTTON_POINT) |
    pinBit(BUTTON_AT) | pinBit(BUTTON_TRIP1_RESET) |
    pinBit(BUTTON_TRIP2_RESET) | pinBit(PULSE_INPUT) |
    pinBit(REVERSE_INPUT) | pinBit(BUTTON_FOOT_RESET) |
    pinBit(EXTERNAL_TRIP_TX_RESERVED) | pinBit(EXTERNAL_TRIP_RX_RESERVED);

constexpr uint64_t RELEASED_GPIO_MASK =
    pinBit(1) | pinBit(2) | pinBit(18) | pinBit(21) | pinBit(38);
constexpr uint64_t OCTAL_PSRAM_RESERVED_GPIO_MASK =
    pinBit(35) | pinBit(36) | pinBit(37);

static_assert(bitCount(USED_GPIO_MASK) == 20,
              "ESP32-S3 ILI9488 GPIO assignments must be unique");
static_assert((USED_GPIO_MASK & RELEASED_GPIO_MASK) == 0,
              "Released GPIO1/2/18/21/38 must remain unused");
static_assert((USED_GPIO_MASK & OCTAL_PSRAM_RESERVED_GPIO_MASK) == 0,
              "GPIO35-37 are reserved by N16R8 Octal PSRAM");
static_assert(PULSE_INPUT != 19 && REVERSE_INPUT != 20,
              "GPIO19/20 must remain available for native USB");

}  // namespace Esp32S3Ili9488Pins
#endif

#if defined(ASKOMPU_BOARD_ILI9488_MAIN) && \
    (!defined(ARDUINO_USB_MODE) || ARDUINO_USB_MODE != 1)
#error "esp32s3-ili9488-main requires Hardware CDC and JTAG USB mode"
#endif
#if defined(ASKOMPU_BOARD_ILI9488_MAIN) && \
    (!defined(ARDUINO_USB_CDC_ON_BOOT) || ARDUINO_USB_CDC_ON_BOOT != 1)
#error "esp32s3-ili9488-main requires USB CDC on boot"
#endif
#if defined(ARDUINO_USB_MSC_ON_BOOT) && ARDUINO_USB_MSC_ON_BOOT != 0
#error "esp32s3-ili9488-main must not enable USB MSC on boot"
#endif
#if defined(ARDUINO_USB_DFU_ON_BOOT) && ARDUINO_USB_DFU_ON_BOOT != 0
#error "esp32s3-ili9488-main must not enable USB DFU on boot"
#endif
