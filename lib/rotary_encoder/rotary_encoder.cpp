#include "rotary_encoder.hpp"

#include <Arduino.h>

RotaryEncoder::RotaryEncoder(const int default_position,
                             const int min_position,
                             const int max_position)
  : m_default_position(default_position)
  , m_min_position(min_position)
  , m_max_position(max_position)
  , m_position(default_position)
{
}

void
RotaryEncoder::Init(const int pin_a, const int pin_b, const int pin_reset)
{
  m_pin_a = pin_a;
  m_pin_b = pin_b;
  m_pin_reset = pin_reset;

  pinMode(pin_a, INPUT_PULLUP);
  pinMode(pin_b, INPUT_PULLUP);
  pinMode(pin_reset, INPUT_PULLUP);
  digitalWrite(pin_a, HIGH);     // activate the pullup
  digitalWrite(pin_b, HIGH);     // activate the pullup
  digitalWrite(pin_reset, HIGH); // activate the pullup

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
    digitalPinToInterrupt(pin_reset),
    [](void* args) { reinterpret_cast<RotaryEncoder*>(args)->IsrExecutorPinSW(); },
    this,
    FALLING);

  m_time_counter = millis();
}

void
RotaryEncoder::DeInit()
{
  pinMode(m_pin_a, OUTPUT);
  pinMode(m_pin_b, OUTPUT);
  pinMode(m_pin_reset, OUTPUT);

  detachInterrupt(m_pin_a);
  detachInterrupt(m_pin_b);
  detachInterrupt(m_pin_reset);
}

void
RotaryEncoder::PrepareForSleep()
{
  gpio_wakeup_enable(static_cast<gpio_num_t>(m_pin_a), gpio_int_type_t::GPIO_INTR_LOW_LEVEL);
  gpio_wakeup_enable(static_cast<gpio_num_t>(m_pin_b), gpio_int_type_t::GPIO_INTR_LOW_LEVEL);
  gpio_wakeup_enable(static_cast<gpio_num_t>(m_pin_reset), gpio_int_type_t::GPIO_INTR_LOW_LEVEL);
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