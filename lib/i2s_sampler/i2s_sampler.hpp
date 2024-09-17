#pragma once

#include <cstdint>
#include <vector>

#include "driver/i2s_std.h"

class I2sSampler
{
public:
  bool Init();
  bool DeInit();

  ~I2sSampler();

  void DiscardSamples(const std::size_t samples_ammount);

  std::vector<int16_t> ReadSamples(const std::size_t max_samples);

private:
  bool m_is_init = false;
  i2s_chan_handle_t m_rx_handle;

  // I have no idea what is this used for
  int16_t constval = 0;
};
