#pragma once

#include <array>
#include <cstdint>
#include <string_view>
#include <tuple>

// #include "hal/gpio_types.h"

constexpr std::size_t MIC_SAMPLE_RATE = 16'000; // Hz
// constexpr std::size_t MIC_SAMPLE_RATE = 32'000; // Hz
// constexpr std::size_t MIC_SAMPLE_RATE = 44'100; // Hz

namespace pins {

// I2S microphone
constexpr int I2S_MIC_SERIAL_DATA = 4;
constexpr int I2S_MIC_SERIAL_CLOCK = 5;
constexpr int I2S_MIC_WORD_SELECT = 0;

// Record button
constexpr int BUTTON = 1;

// SPI
constexpr int SPI_CLK = 6;
constexpr int SPI_MOSI = 7;
constexpr int SPI_MISO = 2;

// I2C
constexpr int I2C_SCL = 13;
constexpr int I2C_SDA = 12;

// SD card
// constexpr int SD_CS = 9;
constexpr int SD_CS = 17;

// Screen
constexpr int SCREEN_PDC = 15;

constexpr int SCREEN_1_CS = 23;
constexpr int SCREEN_1_RST = 22;
constexpr int SCREEN_1_BUSY = 21;

constexpr int SCREEN_2_CS = 20;
constexpr int SCREEN_2_RST = 19;
constexpr int SCREEN_2_BUSY = 18;

// Addressable RGB LED strip
constexpr int ARGB_LED = 8;

// Rotary Encoder
// constexpr int ROT_ENC_SIA = 10;
// constexpr int ROT_ENC_SIB = 11;
constexpr int ROT_ENC_SIA = 11;
constexpr int ROT_ENC_SIB = 10;
constexpr int ROT_ENC_SW = 3;

} // namespace pins

constexpr int ARGB_LEDS_COUNT = 5;
constexpr std::array<std::tuple<uint8_t, uint8_t, uint8_t>, 3> ARGB_COLORS = { {
  { 255, 0, 0 },
  { 0, 255, 0 },
  { 0, 0, 255 },
} };

constexpr int ROT_MAX_VALUE = 32;
constexpr int ROT_MIN_VALUE = 0;
constexpr int ROT_DEF_VALUE = 1;

constexpr std::size_t SLEEP_TIMEOUT_MS = 10'000;

constexpr std::string_view VFS_MOUNT_POINT = "/storage";
constexpr uint64_t FULL_STORAGE_THRESHOLD = 100 * 1024 * 1024; // bytes

constexpr std::string_view DEVICE_NAME = "esp-recorder";

constexpr std::string_view WIFI_SSID = "Penzari";
constexpr std::string_view WIFI_PASS = "068882210";

// constexpr std::string_view CONFIG_FTP_SERVER = "192.168.50.111";
constexpr std::string_view CONFIG_FTP_SERVER = "192.168.0.136";
constexpr uint16_t CONFIG_FTP_PORT = 21;
constexpr std::string_view CONFIG_FTP_USER = "esp-recordings";
constexpr std::string_view CONFIG_FTP_PASSWORD = "Admin0308";

#define DEBUG_SD 1
#define DEBUG_MIC 1
#define DEBUG_WAV 1
#define DEBUG_COM 1
#define DEBUG_SCREEN 1
#define DEBUG_SPI 1
#define DEBUG_TIMER 1