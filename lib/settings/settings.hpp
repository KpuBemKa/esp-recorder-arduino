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

constexpr int I2C_PIN_SDA = 5;
constexpr int I2C_PIN_SCL = 4;

// SD card pins
constexpr int SD_PIN_CS = 12;

// Screen pins
constexpr int SCREEN_PIN_CS = 15;
constexpr int SCREEN_PIN_DC = 23;
constexpr int SCREEN_PIN_RST = 22;
constexpr int SCREEN_PIN_BUSY = 21;
constexpr int SCREEN_PIN_PWR = 20;

// Rotary Encoder
constexpr int ROT_ENC_PIN_SIA = 11;
constexpr int ROT_ENC_PIN_SIB = 10;
constexpr int ROT_ENC_PIN_SW = 3;

constexpr std::string_view VFS_MOUNT_POINT = "/storage";

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