#pragma once

#include "settings.hpp"

struct wav_header_t
{
  // RIFF Header
  const char riff_header[4] = { 'R', 'I', 'F', 'F' };
  int32_t wav_size = 0; // Size of the wav portion of the file, which follows the
                        // first 8 bytes. File size - 8
  const char wave_header[4] = { 'W', 'A', 'V', 'E' };

  // Format Header
  const char fmt_header[4] = { 'f', 'm', 't', ' ' }; // Contains "fmt " (includes trailing space)
  const int32_t fmt_chunk_size = 16;                 // Number of bytes left in "fmt" subchunk
  const int16_t audio_format = 1;                    // Should be 1 for PCM. 3 for IEEE Float
  const int16_t num_channels = 1;                    // Mono
  const int32_t sample_rate = static_cast<int>(MIC_SAMPLE_RATE);

  // Number of bytes per second. sample_rate * num_channels * Bytes Per Sample
  const int32_t byte_rate = sample_rate * 2;

  // The number of bytes for one sample including all channels. num_channels * Bytes Per Sample
  const int16_t sample_alignment = 2;
  // const int16_t sample_alignment = 2;

  // Number of bits per sample
  const int16_t bits_per_sample = 16;

  // Data
  const char data_header[4] = { 'd', 'a', 't', 'a' };
  int data_bytes = 0; // Number of bytes in data
                      // Number of samples * num_channels * sample byte size

} __attribute__((packed));