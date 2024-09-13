#pragma once

#include <cstdint>
#include <vector>

#include "driver/i2s_std.h"

class I2sSampler
{
public:
  esp_err_t Init(const i2s_std_config_t& i2s_config);
  esp_err_t DeInit();

  ~I2sSampler();

  void DiscardSamples(const std::size_t samples_ammount);

  std::vector<int16_t> ReadSamples(const std::size_t max_samples);

private:
  bool m_is_init = false;
  i2s_chan_handle_t m_rx_handle;

  // I have no idea what is this used for
  int16_t constval = 0;
};
