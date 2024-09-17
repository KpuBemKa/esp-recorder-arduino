#pragma once

#include <cstdint>
#include <string_view>
#include <type_traits>

// base class GxEPD2_GFX can be used to pass references or pointers to the
// display instance as parameter, uses ~1.2k more code enable or disable
// GxEPD2_GFX base class
#define ENABLE_GxEPD2_GFX 0

#include <Fonts/FreeMonoBold24pt7b.h>
#include <GxEPD2_BW.h>

#include "settings.hpp"

template<class Driver>
class ScreenDriver
{
  static_assert(std::is_base_of<GxEPD2_EPD, Driver>::value);

public:
  ScreenDriver()
    : m_display(Driver(SCREEN_PIN_CS, SCREEN_PIN_DC, SCREEN_PIN_RST, SCREEN_PIN_BUSY))
  {
  }

  bool Init();
  void DeInit();

  void Clear();

  void DisplayStandby();
  void DisplayAfterRecording();

private:
  bool m_is_init = false;

  GxEPD2_BW<Driver, Driver::HEIGHT> m_display;
};

template<class Driver>
bool
ScreenDriver<Driver>::Init()
{
  pinMode(SCREEN_PIN_PWR, OUTPUT);

  delay(100);

  digitalWrite(SCREEN_PIN_PWR, HIGH);

  // Init SPI. If initialized, will immediately return
  SPI.begin(SPI_PIN_CLK, SPI_PIN_MISO, SPI_PIN_MOSI, (-1));
  if (SPI.bus() == nullptr) {
    Serial.print("Failed to initialize the SPI bus.");
    return false;
  }

  // USE THIS for Waveshare boards with "clever" reset circuit, 2ms reset pulse
  m_display.init(115200, true, 2, false);

  // Initialize, and immediately go to sleep
  m_display.hibernate();

  return true;
}

template<class Driver>
void
ScreenDriver<Driver>::DeInit()
{
  m_display.powerOff();
  m_display.end();

  digitalWrite(SCREEN_PIN_PWR, LOW);
}

template<class Driver>
void
ScreenDriver<Driver>::Clear()
{
  m_display.clearScreen();
}

template<class Driver>
void
ScreenDriver<Driver>::DisplayStandby()
{
  const char text[] = "Standby";

  m_display.setRotation(1);
  m_display.setFont(&FreeMonoBold24pt7b);
  m_display.setTextColor(GxEPD_BLACK);
  int16_t tbx, tby;
  uint16_t tbw, tbh;
  m_display.getTextBounds(text, 0, 0, &tbx, &tby, &tbw, &tbh);
  // center the bounding box by transposition of the origin:
  uint16_t x = ((m_display.width() - tbw) / 2) - tbx;
  uint16_t y = ((m_display.height() - tbh) / 2) - tby;
  m_display.setFullWindow();
  m_display.firstPage();
  do {
    m_display.fillScreen(GxEPD_WHITE);
    m_display.setCursor(x, y);
    m_display.print(text);
  } while (m_display.nextPage());
}

template<class Driver>
void
ScreenDriver<Driver>::DisplayAfterRecording()
{
  const char text[] = "Recorded";

  m_display.setRotation(1);
  m_display.setFont(&FreeMonoBold24pt7b);
  m_display.setTextColor(GxEPD_BLACK);
  int16_t tbx, tby;
  uint16_t tbw, tbh;
  m_display.getTextBounds(text, 0, 0, &tbx, &tby, &tbw, &tbh);
  // center the bounding box by transposition of the origin:
  uint16_t x = ((m_display.width() - tbw) / 2) - tbx;
  uint16_t y = ((m_display.height() - tbh) / 2) - tby;
  m_display.setFullWindow();
  m_display.firstPage();
  do {
    m_display.fillScreen(GxEPD_WHITE);
    m_display.setCursor(x, y);
    m_display.print(text);
  } while (m_display.nextPage());
}