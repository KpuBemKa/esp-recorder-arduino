#pragma once

#include <cstdint>
#include <string_view>
#include <type_traits>

#include <FS.h>
#include <SD.h>

// base class GxEPD2_GFX can be used to pass references or pointers to the
// display instance as parameter, uses ~1.2k more code enable or disable
// GxEPD2_GFX base class
#define ENABLE_GxEPD2_GFX 0

#include <Fonts/FreeMonoBold24pt7b.h>
#include <GxEPD2_BW.h>

#include "sd_card.hpp"
#include "settings.hpp"

static const uint16_t input_buffer_pixels = 20; // may affect performance
// static const uint16_t input_buffer_pixels = 800; // may affect performance

static const uint16_t max_row_width = 200;      // for up to 6" display 1448x1072
static const uint16_t max_palette_pixels = 256; // for depth <= 8

uint8_t input_buffer[3 * input_buffer_pixels];        // up to depth 24
uint8_t output_row_mono_buffer[max_row_width / 8];    // buffer for at least one row of b/w bits
uint8_t output_row_color_buffer[max_row_width / 8];   // buffer for at least one row of color bits
uint8_t mono_palette_buffer[max_palette_pixels / 8];  // palette buffer for depth <= 8 b/w
uint8_t color_palette_buffer[max_palette_pixels / 8]; // palette buffer for depth <= 8 c/w
uint16_t rgb_palette_buffer[max_palette_pixels];      // palette buffer for depth <= 8 for buffered
                                                      // graphics, needed for 7-color display

template<class Driver>
class ScreenDriver
{
  static_assert(std::is_base_of<GxEPD2_EPD, Driver>::value);

public:
  ScreenDriver(const int pin_cs,
               const int pin_dc,
               const int pin_rst,
               const int pin_busy,
               const int pin_pwr = (-1))
    : m_display(Driver(pin_cs, pin_dc, pin_rst, pin_busy))
    , m_pin_pwr(pin_pwr)
  {
  }

  bool Init();
  void DeInit();

  void Clear();

  void DrawImageFromStorage(const std::string_view file_path,
                            int16_t x,
                            int16_t y,
                            bool with_color);

  void EnablePower();
  void DisablePower();

private:
  uint16_t read16(File& f);
  uint32_t read32(File& f);

private:
  GxEPD2_BW<Driver, Driver::HEIGHT> m_display;
  const int m_pin_pwr = -1;

  bool m_is_init = false;
};

template<class Driver>
bool
ScreenDriver<Driver>::Init()
{
  if (m_pin_pwr != -1)
    pinMode(m_pin_pwr, OUTPUT);
  // delay(100);

  EnablePower();

  // USE THIS for Waveshare boards with "clever" reset circuit, 2ms reset pulse
  m_display.init(115200, true, 2, false);

  // Initialize, and immediately go to sleep
  m_display.hibernate();

  DisablePower();

  return true;
}

template<class Driver>
void
ScreenDriver<Driver>::DeInit()
{
  EnablePower();

  m_display.endWrite();
  m_display.hibernate();
  m_display.end();

  DisablePower();
}

template<class Driver>
void
ScreenDriver<Driver>::Clear()
{
  EnablePower();
  m_display.clearScreen();
  DisablePower();
}

template<class Driver>
void
ScreenDriver<Driver>::DrawImageFromStorage(const std::string_view filename,
                                           int16_t x,
                                           int16_t y,
                                           bool with_color)
{
  EnablePower();

  bool flip = true; // bitmap is stored bottom-to-top
  const uint32_t startTime = millis();

  if ((x >= m_display.epd2.WIDTH) || (y >= m_display.epd2.HEIGHT))
    return;

  Serial.println();
  Serial.print("Loading image '");
  Serial.print(filename.data());
  Serial.println('\'');

  File file = SD.open(filename.data(), FILE_READ);
  if (!file) {
    Serial.print("File not found");
    return;
  }

  // Parse BMP header BMP signature
  if (read16(file) != 0x4D42) {
    file.close();
    Serial.println("File does not contain a bitmap sginature.");
    return;
  }

  const uint32_t fileSize = read32(file);
  const uint32_t creatorBytes = read32(file);
  (void)creatorBytes;                        // unused
  const uint32_t imageOffset = read32(file); // Start of image data
  const uint32_t headerSize = read32(file);
  const uint32_t width = read32(file);
  int32_t height = (int32_t)read32(file);
  const uint16_t planes = read16(file);
  const uint16_t depth = read16(file); // bits per pixel
  const uint32_t format = read32(file);

  // uncompressed is handled, 565 also
  if (!((planes == 1) && ((format == 0) || (format == 3)))) {
    file.close();
    Serial.println("Failed to verify planes & format.");
    return;
  }

  Serial.print("File size: ");
  Serial.println(fileSize);
  Serial.print("Image Offset: ");
  Serial.println(imageOffset);
  Serial.print("Header size: ");
  Serial.println(headerSize);
  Serial.print("Bit Depth: ");
  Serial.println(depth);
  Serial.print("Image size: ");
  Serial.print(width);
  Serial.print('x');
  Serial.println(height);

  // BMP rows are padded (if needed) to 4-byte boundary
  uint32_t rowSize = (width * depth / 8 + 3) & ~3;
  if (depth < 8)
    rowSize = ((width * depth + 8 - depth) / 8 + 3) & ~3;
  if (height < 0) {
    height = -height;
    flip = false;
  }

  uint16_t w = width;
  uint16_t h = height;

  if ((x + w - 1) >= m_display.epd2.WIDTH)
    w = m_display.epd2.WIDTH - x;
  if ((y + h - 1) >= m_display.epd2.HEIGHT)
    h = m_display.epd2.HEIGHT - y;

  if (w > max_row_width) // handle with direct drawing
  {
    file.close();
    Serial.println("Calculated width does not fit into `max_row_width`.");
    return;
  }

  const uint8_t bitshift = 8 - depth;
  uint8_t bitmask = 0xFF;
  uint16_t red, green, blue;
  bool whitish = false;
  bool colored = false;

  if (depth == 1)
    with_color = false;

  if (depth <= 8) {
    if (depth < 8)
      bitmask >>= depth;

    // file.seek(54); //palette is always @ 54
    file.seek(imageOffset - (4 << depth)); // 54 for regular, diff for colorsimportant

    for (uint16_t pn = 0; pn < (1 << depth); pn++) {
      blue = file.read();
      green = file.read();
      red = file.read();
      file.read();
      // Serial.print(red); Serial.print(" "); Serial.print(green); Serial.print(" ");
      // Serial.println(blue);
      whitish = with_color ? ((red > 0x80) && (green > 0x80) && (blue > 0x80))
                           : ((red + green + blue) > 3 * 0x80);    // whitish
      colored = (red > 0xF0) || ((green > 0xF0) && (blue > 0xF0)); // reddish or yellowish?
      if (0 == pn % 8)
        mono_palette_buffer[pn / 8] = 0;
      mono_palette_buffer[pn / 8] |= whitish << pn % 8;
      if (0 == pn % 8)
        color_palette_buffer[pn / 8] = 0;
      color_palette_buffer[pn / 8] |= colored << pn % 8;
    }
  }

  m_display.clearScreen();
  uint32_t rowPosition = flip ? imageOffset + (height - h) * rowSize : imageOffset;
  for (uint16_t row = 0; row < h; row++, rowPosition += rowSize) // for each line
  {
    uint32_t in_remain = rowSize;
    uint32_t in_idx = 0;
    uint32_t in_bytes = 0;
    uint8_t in_byte = 0;           // for depth <= 8
    uint8_t in_bits = 0;           // for depth <= 8
    uint8_t out_byte = 0xFF;       // white (for w%8!=0 border)
    uint8_t out_color_byte = 0xFF; // white (for w%8!=0 border)
    uint32_t out_idx = 0;
    file.seek(rowPosition);

    for (uint16_t col = 0; col < w; col++) // for each pixel
    {
      // Time to read more pixel data?
      if (in_idx >= in_bytes) // ok, exact match for 24bit also (size IS multiple of 3)
      {
        in_bytes = file.read(input_buffer,
                             in_remain > sizeof(input_buffer) ? sizeof(input_buffer) : in_remain);
        in_remain -= in_bytes;
        in_idx = 0;
      }

      switch (depth) {
        case 32:
          blue = input_buffer[in_idx++];
          green = input_buffer[in_idx++];
          red = input_buffer[in_idx++];
          in_idx++; // skip alpha
          whitish = with_color ? ((red > 0x80) && (green > 0x80) && (blue > 0x80))
                               : ((red + green + blue) > 3 * 0x80);    // whitish
          colored = (red > 0xF0) || ((green > 0xF0) && (blue > 0xF0)); // reddish or yellowish?
          break;
        case 24:
          blue = input_buffer[in_idx++];
          green = input_buffer[in_idx++];
          red = input_buffer[in_idx++];
          whitish = with_color ? ((red > 0x80) && (green > 0x80) && (blue > 0x80))
                               : ((red + green + blue) > 3 * 0x80);    // whitish
          colored = (red > 0xF0) || ((green > 0xF0) && (blue > 0xF0)); // reddish or yellowish?
          break;
        case 16: {
          uint8_t lsb = input_buffer[in_idx++];
          uint8_t msb = input_buffer[in_idx++];
          if (format == 0) // 555
          {
            blue = (lsb & 0x1F) << 3;
            green = ((msb & 0x03) << 6) | ((lsb & 0xE0) >> 2);
            red = (msb & 0x7C) << 1;
          } else // 565
          {
            blue = (lsb & 0x1F) << 3;
            green = ((msb & 0x07) << 5) | ((lsb & 0xE0) >> 3);
            red = (msb & 0xF8);
          }
          whitish = with_color ? ((red > 0x80) && (green > 0x80) && (blue > 0x80))
                               : ((red + green + blue) > 3 * 0x80);    // whitish
          colored = (red > 0xF0) || ((green > 0xF0) && (blue > 0xF0)); // reddish or yellowish?
        } break;
        case 1:
        case 2:
        case 4:
        case 8: {
          if (0 == in_bits) {
            in_byte = input_buffer[in_idx++];
            in_bits = 8;
          }
          uint16_t pn = (in_byte >> bitshift) & bitmask;
          whitish = mono_palette_buffer[pn / 8] & (0x1 << pn % 8);
          colored = color_palette_buffer[pn / 8] & (0x1 << pn % 8);
          in_byte <<= depth;
          in_bits -= depth;
        } break;
      }

      if (whitish) {
        // keep white
      } else if (colored && with_color) {
        out_color_byte &= ~(0x80 >> col % 8); // colored
      } else {
        out_byte &= ~(0x80 >> col % 8); // black
      }

      if ((7 == col % 8) || (col == w - 1)) // write that last byte! (for w%8!=0 border)
      {
        output_row_color_buffer[out_idx] = out_color_byte;
        output_row_mono_buffer[out_idx++] = out_byte;
        out_byte = 0xFF;       // white (for w%8!=0 border)
        out_color_byte = 0xFF; // white (for w%8!=0 border)
      }
    } // end pixel

    uint16_t yrow = y + (flip ? h - row - 1 : row);
    m_display.writeImage(output_row_mono_buffer, output_row_color_buffer, x, yrow, w, 1);

  } // end line

  file.close();

  Serial.print("loaded in ");
  Serial.print(millis() - startTime);
  Serial.println(" ms");

  m_display.refresh();

  DisablePower();
}

template<class Driver>
void
ScreenDriver<Driver>::EnablePower()
{
  if (m_pin_pwr != -1)
    digitalWrite(m_pin_pwr, HIGH);
}

template<class Driver>
void
ScreenDriver<Driver>::DisablePower()
{
  m_display.hibernate();

  if (m_pin_pwr != -1)
    digitalWrite(m_pin_pwr, LOW);
}

template<class Driver>
uint16_t
ScreenDriver<Driver>::read16(File& f)
{
  // BMP data is stored little-endian, same as Arduino.
  uint16_t result;
  ((uint8_t*)&result)[0] = f.read(); // LSB
  ((uint8_t*)&result)[1] = f.read(); // MSB
  return result;
}

template<class Driver>
uint32_t
ScreenDriver<Driver>::read32(File& f)
{
  // BMP data is stored little-endian, same as Arduino.
  uint32_t result;
  ((uint8_t*)&result)[0] = f.read(); // LSB
  ((uint8_t*)&result)[1] = f.read();
  ((uint8_t*)&result)[2] = f.read();
  ((uint8_t*)&result)[3] = f.read(); // MSB
  return result;
}