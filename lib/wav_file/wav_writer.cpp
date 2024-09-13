#include "wav_writer.hpp"

#include <cerrno>
#include <cstring>

#include <Arduino.h>

#include "settings.hpp"

bool
WavWriter::Open(
  const std::string_view file_path /* , const std::size_t sample_rate */)
{
  Serial.printf("Opening file '%.*s'...\n", file_path.size(), file_path.data());

  m_fp = fopen(file_path.data(), "wb");

  if (m_fp == nullptr) {
    perror("");
    return false;
  } else {
    // Serial.println("File opened.");
  }

  // m_header.sample_rate = sample_rate;
  // write out the header - we'll fill in some of the blanks later
  const std::size_t written =
    std::fwrite(&m_header, sizeof(wav_header_t), 1, m_fp);
  if (written != 1) {
    Serial.printf("%s:%d | Error writing the WAV header:\n", __FILE__, __LINE__);
    perror("");
    return false;
  }

  m_file_size = sizeof(wav_header_t);

  return true;
}

bool
WavWriter::Close()
{
  if (m_fp != nullptr) {
    return FinishAndClose();
  } else {
    return true;
  }
}

WavWriter::~WavWriter()
{
  Close();
}

void
WavWriter::WriteSamples(const std::span<int16_t> samples)
{
  // write the samples and keep track of the file size so far
  const std::size_t written =
    fwrite(samples.data(), sizeof(samples[0]), samples.size(), m_fp);
  if (written != samples.size()) {
    Serial.printf("%s:%d | Error writing samples. Samples to write: %u | written: %u\n",
          __FILE__,
          __LINE__,
          samples.size(),
          written);
    perror("");
  }

  m_file_size += sizeof(samples[0]) * written;
}

bool
WavWriter::FinishAndClose()
{
  Serial.printf("Finished wav file size: %d\n", m_file_size);

  // now fill in the header with the correct information and write it again
  m_header.data_bytes = m_file_size - sizeof(wav_header_t);
  m_header.wav_size = m_file_size - 8;

  fseek(m_fp, 0, SEEK_SET);
  fwrite(&m_header, sizeof(m_header), 1, m_fp);

  if (fclose(m_fp) != 0) {
    Serial.printf("%s:%d | Unable to close the file. errno: %d = %s",
          __FILE__,
          __LINE__,
          errno,
          std::strerror(errno));
    return false;
  }

  m_fp = nullptr;

  return true;
}