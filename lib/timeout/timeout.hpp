#pragma once

#include "driver/gptimer.h"
#include "esp_attr.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

class Timeout
{
public:
  bool Init(const std::size_t timeout_ms);
  bool DeInit();

  bool Start();
  bool Stop();
  bool Reset();

  bool IsTimeoutReached();

private:
  static bool IRAM_ATTR TimerCallback(gptimer_handle_t timer,
                                      const gptimer_alarm_event_data_t* edata,
                                      void* user_data)
  {
    BaseType_t high_task_awoken = pdFALSE;
    bool* sleep_flag = reinterpret_cast<bool*>(user_data);

    // stop timer immediately
    gptimer_stop(timer);

    *sleep_flag = true;

    // return whether we need to yield at the end of ISR
    return (high_task_awoken == pdTRUE);
  }

private:
  gptimer_handle_t m_timer_handle;
  bool m_callback_fired = false;
};