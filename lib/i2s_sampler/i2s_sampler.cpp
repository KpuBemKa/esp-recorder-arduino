#include "i2s_sampler.hpp"

#include <Arduino.h>

#include "esp_err.h"
#include "esp_log.h"

#include "settings.hpp"

#if DEBUG_MIC
#define LOG(...) Serial.printf(__VA_ARGS__)
#else
#define LOG(...)
#endif

constexpr i2s_std_config_t I2S_MIC_CONFIG = { 
  .clk_cfg = {
    .sample_rate_hz = MIC_SAMPLE_RATE,
    .clk_src = I2S_CLK_SRC_DEFAULT,
    // .ext_clk_freq_hz = 0,
    .mclk_multiple = I2S_MCLK_MULTIPLE_256,
  },
  .slot_cfg = {
    .data_bit_width = I2S_DATA_BIT_WIDTH_32BIT, 
    .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO, 
    .slot_mode = I2S_SLOT_MODE_STEREO, 
    .slot_mask = I2S_STD_SLOT_LEFT,
    .ws_width = I2S_DATA_BIT_WIDTH_32BIT, 
    .ws_pol = false, 
    .bit_shift = true, 
    .left_align = true, 
    .big_endian = false, 
    .bit_order_lsb = false 
  },
    
  .gpio_cfg = {
    .mclk = I2S_GPIO_UNUSED,  
    .bclk = static_cast<gpio_num_t>(pins::I2S_MIC_SERIAL_CLOCK),
    .ws   = static_cast<gpio_num_t>(pins::I2S_MIC_WORD_SELECT),
    .dout = gpio_num_t::GPIO_NUM_NC,
    .din  = static_cast<gpio_num_t>(pins::I2S_MIC_SERIAL_DATA),
    .invert_flags = {
      .mclk_inv = false,
      .bclk_inv = false,
      .ws_inv   = false,
    },
  }
};

bool
I2sSampler::Init()
{
  if (m_is_init) {
    LOG("I2S sampler is already initialized.\n");

    // esp_err_t result = DeInit();
    // if (result != ESP_OK) {
    //   LOG("%s:%d | De-initialize failed: %s", __FILE__, __LINE__, esp_err_to_name(result));
    // } else {
    //   LOG("De-initialized.");
    // }
    return true;
  }

  LOG("Initializing I2S sampler...\n");

  // Create a new I2S channel
  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
  esp_err_t esp_result = i2s_new_channel(&chan_cfg, nullptr, &m_rx_handle);
  if (esp_result != ESP_OK) {
    LOG("%s:%d | Unable to create a new I2S channel: %s\n",
        __FILE__,
        __LINE__,
        esp_err_to_name(esp_result));
    return false;
  }

  // Initialize this new channel in standard mode
  esp_result = i2s_channel_init_std_mode(m_rx_handle, &I2S_MIC_CONFIG);
  if (esp_result != ESP_OK) {
    LOG("%s:%d | Unable to initialize I2S rx channel: %s\n",
        __FILE__,
        __LINE__,
        esp_err_to_name(esp_result));
    return false;
  }

  // Enable the channel
  esp_result = i2s_channel_enable(m_rx_handle);
  if (esp_result != ESP_OK) {
    LOG("%s:%d | Unable to enable I2S RX channel: %s\n",
        __FILE__,
        __LINE__,
        esp_err_to_name(esp_result));
    return false;
  }

  m_is_init = true;

  return true;
}

bool
I2sSampler::DeInit()
{
  if (!m_is_init) {
    LOG("I2S is not initialized. Skipping deinitialization...\n");
    return true;
  }

  esp_err_t esp_result = i2s_channel_disable(m_rx_handle);
  if (esp_result != ESP_OK) {
    LOG("%s:%d | Unable to disable I2S RX channel: %s\n",
        __FILE__,
        __LINE__,
        esp_err_to_name(esp_result));
    return false;
  }

  esp_result = i2s_del_channel(m_rx_handle);
  if (esp_result != ESP_OK) {
    LOG("%s:%d | Unable to delete I2S RX channel: %s\n",
        __FILE__,
        __LINE__,
        esp_err_to_name(esp_result));
    return false;
  }

  m_is_init = false;

  return true;
}

I2sSampler::~I2sSampler()
{
  DeInit();
}

void
I2sSampler::DiscardSamples(const std::size_t samples_ammount)
{
  std::vector<uint32_t> buffer;
  buffer.resize(samples_ammount);

  const esp_err_t esp_result =
    i2s_channel_read(m_rx_handle, buffer.data(), samples_ammount * sizeof(buffer[0]), nullptr, 100);

  if (esp_result != ESP_OK) {
    LOG(
      "%s:%d | Unable to discard RX buffer: %s\n", __FILE__, __LINE__, esp_err_to_name(esp_result));
  }
}

std::vector<int16_t>
I2sSampler::ReadSamples(const std::size_t max_samples)
{
  // a buffer which will store read data
  std::vector<int32_t> raw_samples_vector;
  raw_samples_vector.resize(max_samples);

  // how many bytes have to be read
  const std::size_t bytes_to_read = max_samples * sizeof(raw_samples_vector[0]);
  // will store how many bytes were actually read
  std::size_t bytes_read = 0;

  // read the data
  const esp_err_t esp_result =
    i2s_channel_read(m_rx_handle, raw_samples_vector.data(), bytes_to_read, &bytes_read, 100);

  LOG("%u bytes have been read.\n", bytes_read);

  // check for errors
  if (esp_result != ESP_OK) {
    LOG(
      "%s:%d | Unable to read I2S RX channel: %s\n", __FILE__, __LINE__, esp_err_to_name(esp_result));
    return {}; // return empty array
  }

  // number of samples that have been read
  const std::size_t samples_read = bytes_read / sizeof(raw_samples_vector[0]);

  std::vector<int16_t> result_samples;
  result_samples.reserve(samples_read);

  for (std::size_t i = 0; i < samples_read; ++i) {
    // data occupies 24 of 32 bits
    // shifting discards 8 zero and 8 least significant bits
    result_samples.push_back(raw_samples_vector[i] >> 16);
  }

  return result_samples;
}

// esp_err_t
// I2sSampler::ReadSamples(std::span<int16_t> output_buf)
// {
//   const std::size_t bytes_to_read = output_buf.size() * sizeof(int16_t);

//   const esp_err_t esp_result =
//     i2s_channel_read(m_rx_handle, output_buf.data(), bytes_to_read, &bytes_to_read, 1'000);

//   if (esp_result != ESP_OK) {
//     LOG(
//       "%s:%d | Unable to read I2S RX channel: %s", __FILE__, __LINE__,
//       esp_err_to_name(esp_result));
//   }

//   return esp_result;
// }

// template<std::size_t N>
// std::expected<std::array<int16_t, N>, esp_err_t>
// I2sSampler::ReadSamples()
// {
//   // read from i2s
//   // int32_t* raw_samples = (int32_t*)malloc(sizeof(int32_t) * count);
//   // size_t bytes_read = 0;
//   // i2s_read(m_i2sPort, raw_samples, sizeof(int32_t) * count, &bytes_read, 100);
//   // int samples_read = bytes_read / sizeof(int32_t);

//   constexpr std::size_t bytes_to_read = N * 2;
//   std::array<int16_t, N> samples;

//   const esp_err_t esp_result =
//     i2s_channel_read(m_rx_handle, &samples.data(), bytes_to_read, bytes_to_read, 1'000);
//   if (esp_result != ESP_OK) {
//     LOG(
//       "%s:%d | Unable to read I2S RX channel: %s", __FILE__, __LINE__,
//       esp_err_to_name(esp_result));
//     return std::unexpected(esp_result);
//   }

//   return samples;
// }
