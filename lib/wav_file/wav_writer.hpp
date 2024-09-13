#pragma once

#include <cstdint>
#include <cstdio>
#include <span>
#include <string_view>

#include "esp_err.h"

#include "wav_header.hpp"

class WavWriter
{
public:
  bool Open(const std::string_view file_path/* , const std::size_t sample_rate */);
  bool Close();

  ~WavWriter();

  void WriteSamples(const std::span<int16_t> samples);

private:
  bool FinishAndClose();

private:
  FILE* m_fp;

  wav_header_t m_header;
  int m_file_size;
};