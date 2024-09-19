#include "rotary_encoder.hpp"

#include <Arduino.h>

RotaryEncoder::RotaryEncoder(const int max_position, const int min_position)
  : m_max_position(max_position)
  , m_min_position(min_position)
{
}

void
RotaryEncoder::Init(const int pin_a, const int pin_b, const int pin_sw)
{
  pinMode(pin_a, INPUT_PULLUP);
  pinMode(pin_b, INPUT_PULLUP);
  pinMode(pin_sw, INPUT_PULLUP);
  digitalWrite(pin_a, HIGH); // activate the pullup
  digitalWrite(pin_b, HIGH); // activate the pullup
  digitalWrite(pin_sw, HIGH); // activate the pullup

  attachInterruptArg(
    digitalPinToInterrupt(pin_a),
    [](void* args) { reinterpret_cast<RotaryEncoder*>(args)->IsrExecutorPinA(); },
    this,
    ONLOW);

  attachInterruptArg(
    digitalPinToInterrupt(pin_b),
    [](void* args) { reinterpret_cast<RotaryEncoder*>(args)->IsrExecutorPinB(); },
    this,
    ONLOW);

  attachInterruptArg(
    digitalPinToInterrupt(pin_sw),
    [](void* args) { reinterpret_cast<RotaryEncoder*>(args)->IsrExecutorPinSW(); },
    this,
    FALLING);

  m_time_counter = millis();
}

void
RotaryEncoder::IsrExecutorPinA()
{
  if ((millis() - m_time_counter) > 3 && m_position < m_max_position) {
    ++m_position;
  }
  m_time_counter = millis();
}

void
RotaryEncoder::IsrExecutorPinB()
{
  if ((millis() - m_time_counter) > 3 && m_position > m_min_position) {
    --m_position;
  }
  m_time_counter = millis();
}