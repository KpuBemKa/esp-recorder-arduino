#include "timeout.hpp"

#include "esp_log.h"

#include <Arduino.h>

#include "settings.hpp"

#if DEBUG_TIMER
#define LOG(...) Serial.printf(__VA_ARGS__)
#else
#define LOG(...)
#endif

constexpr gptimer_config_t k_timer_config = { .clk_src = GPTIMER_CLK_SRC_DEFAULT,
                                              .direction = GPTIMER_COUNT_UP,
                                              .resolution_hz = 1'000'000, // 1MHz, 1 tick=1us
                                              .intr_priority = 0,
                                              .flags = { .intr_shared = false } };

bool
Timeout::Init(const std::size_t timeout_ms)
{
  const gptimer_alarm_config_t k_alarm_config = { .alarm_count = timeout_ms * 1'000,
                                                  .reload_count = 0,
                                                  .flags = { .auto_reload_on_alarm = 0 } };

  LOG("Initializing timeout timer...\n");

  esp_err_t esp_result = gptimer_new_timer(&k_timer_config, &m_timer_handle);
  if (esp_result != ESP_OK) {
    LOG("Failed to create timeout timer: %s\n", esp_err_to_name(esp_result));
    return false;
  }

  gptimer_event_callbacks_t timer_callbacks = { .on_alarm = TimerCallback };
  esp_result =
    gptimer_register_event_callbacks(m_timer_handle, &timer_callbacks, &m_callback_fired);
  if (esp_result != ESP_OK) {
    LOG("Failed to register timeout timer callback: %s\n", esp_err_to_name(esp_result));
    return false;
  }

  esp_result = gptimer_enable(m_timer_handle);
  if (esp_result != ESP_OK) {
    LOG("Failed to enable timeout timer: %s\n", esp_err_to_name(esp_result));
    return false;
  }

  esp_result = gptimer_set_alarm_action(m_timer_handle, &k_alarm_config);
  if (esp_result != ESP_OK) {
    LOG("Failed to set timeout timer action: %s\n", esp_err_to_name(esp_result));
    return false;
  }

  m_callback_fired = false;

  LOG("Timeout timer has been initialized.\n");

  return true;
}

bool
Timeout::DeInit()
{
  LOG("Stopping timeout timer...\n");

  if (!Stop()) {
    return false;
  }

  esp_err_t esp_result = gptimer_disable(m_timer_handle);
  if (esp_result != ESP_OK) {
    LOG("Failed to disable timeout timer: %s\n", esp_err_to_name(esp_result));
    return false;
  }

  esp_result = gptimer_del_timer(m_timer_handle);
  if (esp_result != ESP_OK) {
    LOG("Failed to delete timeout timer: %s\n", esp_err_to_name(esp_result));
    return false;
  }

  m_callback_fired = false;

  LOG("Timeout timer has been stopped.\n");

  return true;
}

bool
Timeout::Start()
{
  const esp_err_t esp_result = gptimer_start(m_timer_handle);
  if (esp_result != ESP_OK && esp_result != ESP_ERR_INVALID_STATE) {
    LOG("Failed to start timeout timer: %s\n", esp_err_to_name(esp_result));
    return false;
  }

  m_callback_fired = false;

  LOG("Timeout timer has been started.\n");

  return true;
}

bool
Timeout::Stop()
{
  const esp_err_t esp_result = gptimer_stop(m_timer_handle);
  if (esp_result != ESP_OK && esp_result != ESP_ERR_INVALID_STATE) {
    LOG("Failed to stop timeout timer: %s\n", esp_err_to_name(esp_result));
    return false;
  }

  LOG("Timeout timer has been stopped.\n");

  return true;
}

bool
Timeout::Reset()
{
  LOG("Resetting timeout timer...\n");

  m_callback_fired = false;

  const esp_err_t esp_result = gptimer_set_raw_count(m_timer_handle, 0);
  if (esp_result != ESP_OK) {
    LOG("Failed to reset timeout timer: %s\n", esp_err_to_name(esp_result));
    return false;
  }

  return true;
}

bool
Timeout::IsTimeoutReached()
{
  return m_callback_fired;
}
