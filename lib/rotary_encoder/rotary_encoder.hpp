#pragma once

#include "limits.h"
#include <cstdio>

class RotaryEncoder
{
public:
  RotaryEncoder(const int default_position,
                const int min_position = INT_MIN,
                const int max_position = INT_MAX);

  void Init(const int pin_a, const int pin_b, const int pin_sw);
  void DeInit();
  void PrepareForSleep();

  void SetPosition(const int new_value) { m_position = new_value; }
  int GetPosition() { return m_position; }

  void SetButtonCounter(const uint32_t new_value) { m_button_counter = new_value; }
  uint32_t GetButtonCounter() { return m_button_counter; }

private:
  void IsrExecutorPinA();
  void IsrExecutorPinB();
  void IsrExecutorPinSW() { ++m_button_counter; }

private:
  const int m_default_position, m_min_position, m_max_position;

  int m_position = 0;
  uint32_t m_button_counter = 0;
  std::size_t m_time_counter = 0;

  int m_pin_a, m_pin_b, m_pin_reset;
};
