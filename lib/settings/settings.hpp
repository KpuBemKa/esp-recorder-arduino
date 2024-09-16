#pragma once

#include <cstdint>
#include <string_view>

// #include "hal/gpio_types.h"

constexpr std::size_t MIC_SAMPLE_RATE = 16'000; // Hz
// constexpr std::size_t MIC_SAMPLE_RATE = 32'000; // Hz
// constexpr std::size_t MIC_SAMPLE_RATE = 44'100; // Hz

// I2S microphone pins
constexpr int I2S_MIC_SERIAL_DATA = 9;
constexpr int I2S_MIC_SERIAL_CLOCK = 18;
constexpr int I2S_MIC_WORD_SELECT = 19;
// constexpr int I2S_MIC_SERIAL_DATA = 22;
// constexpr int I2S_MIC_LEFT_RIGHT_CLOCK = 21;

// Record button pin
constexpr int BUTTON_PIN = 13;

// Internal addressable RGB LED
constexpr int ARGB_LED_PIN = 8;

constexpr int SPI_PIN_CLK = 6;
constexpr int SPI_PIN_MOSI = 7;
constexpr int SPI_PIN_MISO = 2;

// SD card pins
constexpr int SD_PIN_CS = 12;

// Screen pins

constexpr int SCREEN_PIN_CS = 15;
constexpr int SCREEN_PIN_DC = 23;
constexpr int SCREEN_PIN_RST = 22;
constexpr int SCREEN_PIN_BUSY = 21;
constexpr int SCREEN_PIN_PWR = 20;

constexpr std::string_view VFS_MOUNT_POINT = "/storage";

constexpr std::string_view DEVICE_NAME = "esp-recorder";

#define DEBUG_SD 1
#define DEBUG_MIC 1
#define DEBUG_WAV 1
#define DEBUG_COM 1
#define DEBUG_SCREEN 1
#define DEBUG_SPI 1