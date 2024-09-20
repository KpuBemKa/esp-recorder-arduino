#pragma once

#include "limits.h"
#include <cstdio>

class RotaryEncoder
{
public:
  RotaryEncoder(const int max_position = INT_MAX, const int min_position = INT_MIN);

  void Init(const int pin_a, const int pin_b, const int pin_sw);
  void DeInit();
  void PrepareForSleep();

  int GetPosition() { return m_position; }
  void SetPosition(const int new_value) { m_position = new_value; }

private:
  void IsrExecutorPinA();

  void IsrExecutorPinB();

  void IsrExecutorPinSW() { m_time_counter = 0; }

private:
  const int m_max_position, m_min_position;
  int m_pin_a, m_pin_b, m_pin_reset;

  int m_position = 0;
  std::size_t m_time_counter = 0;
};
