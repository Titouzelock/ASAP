#include <unity.h>

#include <stdint.h>

#ifndef ARDUINO

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>

#include <asap/audio/AudioEngine.h>

namespace
{

constexpr double kPi = 3.14159265358979323846;

// RC / sample-rate parameters for the simulation layer.
constexpr double kRcCutoffHzLegacy = 3000.0;     // legacy 1-pole reference
constexpr double kOutputSampleRateHz = 48000.0;

// Simple first-order low-pass filter matching the original analog RC.
// Kept as a reference; current snapshots use the 2-pole variant below.
class RcFilter
{
 public:
  RcFilter()
  {
    const double omega = 2.0 * kPi * kRcCutoffHzLegacy;
    alpha_ = omega / (omega + kOutputSampleRateHz);
    state_ = 0.0;
  }

  void reset()
  {
    state_ = 0.0;
  }

  double process(double x)
  {
    state_ = state_ + alpha_ * (x - state_);
    return state_;
  }

 private:
  double alpha_;
  double state_;
};

// 2-pole RC filter at fc ≈ 7.2 kHz (per pole), fs = 48 kHz. Implemented as
// two cascaded first-order sections using a shared alpha.
namespace
{

float gRcAlpha = 0.0f;
float gRc1State = 0.0f;
float gRc2State = 0.0f;
bool gRcInitialized = false;

void rc2_init()
{
  const float fc = 7200.0f;
  const float fs = static_cast<float>(kOutputSampleRateHz);
  const float omega = 2.0f * static_cast<float>(kPi) * fc;
  gRcAlpha = omega / (omega + fs);
  gRc1State = 0.0f;
  gRc2State = 0.0f;
  gRcInitialized = true;
}

float rc2_apply(float x)
{
  if (!gRcInitialized)
  {
    rc2_init();
  }

  gRc1State = gRc1State + gRcAlpha * (x - gRc1State);
  gRc2State = gRc2State + gRcAlpha * (gRc1State - gRc2State);
  return gRc2State;
}

}  // namespace

// WAV writer for mono 16-bit PCM at 48 kHz.
void writeWav16Mono(const std::filesystem::path& path,
                    const int16_t* samples,
                    uint32_t sampleCount)
{
  std::filesystem::create_directories(path.parent_path());

  std::ofstream out(path, std::ios::binary);
  TEST_ASSERT_TRUE(out.good());

  const uint32_t sampleRate = static_cast<uint32_t>(kOutputSampleRateHz);
  const uint16_t bitsPerSample = 16;
  const uint16_t numChannels = 1;
  const uint16_t blockAlign = numChannels * bitsPerSample / 8;
  const uint32_t byteRate = sampleRate * blockAlign;
  const uint32_t dataSize = sampleCount * blockAlign;
  const uint32_t fmtChunkSize = 16;
  const uint32_t riffSize = 4 + (8 + fmtChunkSize) + (8 + dataSize);

  auto writeU32 = [&](uint32_t v)
  {
    out.put(static_cast<char>(v & 0xFF));
    out.put(static_cast<char>((v >> 8) & 0xFF));
    out.put(static_cast<char>((v >> 16) & 0xFF));
    out.put(static_cast<char>((v >> 24) & 0xFF));
  };

  auto writeU16 = [&](uint16_t v)
  {
    out.put(static_cast<char>(v & 0xFF));
    out.put(static_cast<char>((v >> 8) & 0xFF));
  };

  // RIFF header
  out.write("RIFF", 4);
  writeU32(riffSize);
  out.write("WAVE", 4);

  // fmt chunk
  out.write("fmt ", 4);
  writeU32(fmtChunkSize);
  writeU16(1);               // PCM
  writeU16(numChannels);
  writeU32(sampleRate);
  writeU32(byteRate);
  writeU16(blockAlign);
  writeU16(bitsPerSample);

  // data chunk
  out.write("data", 4);
  writeU32(dataSize);
  out.write(reinterpret_cast<const char*>(samples), dataSize);

  out.close();
  TEST_ASSERT_TRUE(out.good());
}

std::filesystem::path snapshotsDir()
{
  std::filesystem::path dir =
      std::filesystem::current_path() / "snapshots";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  return dir;
}

// Render helper: runs the MCU engine at 8 kHz, upsamples to 48 kHz using ZOH,
// applies the RC filter and stores 16-bit PCM samples.
void renderSnapshot(int16_t* buffer,
                    uint32_t outputSampleCount,
                    void (*triggerFn)())
{
  asap_audio_init();

  const uint32_t upsampleFactor =
      static_cast<uint32_t>(kOutputSampleRateHz /
                            asap::audio::kSampleRateHz);

  int16_t currentSourceSample = 0;
  uint32_t samplesUntilNextSource = 0;

  // Initial silence: advance the engine for a short time before triggering.
  const uint32_t preSilenceSamples =
      static_cast<uint32_t>(0.1 * kOutputSampleRateHz);  // 100 ms

  for (uint32_t i = 0; i < outputSampleCount; ++i)
  {
    if (i == preSilenceSamples && triggerFn != nullptr)
    {
      triggerFn();
    }

    if (samplesUntilNextSource == 0)
    {
      currentSourceSample = asap_audio_get_sample();
      samplesUntilNextSource = upsampleFactor - 1U;
    }
    else
    {
      --samplesUntilNextSource;
    }

    const float x =
        static_cast<float>(currentSourceSample) / 32768.0f;
    const float y = rc2_apply(x);
    float clamped = y;
    if (clamped > 1.0)
    {
      clamped = 1.0;
    }
    else if (clamped < -1.0)
    {
      clamped = -1.0;
    }

    buffer[i] =
        static_cast<int16_t>(clamped * 32767.0f);
  }
}

// Triggers for the different snapshots.

void triggerGeigerHybridSingle()
{
  geiger_trigger_click();
}

void triggerGeigerBurst3()
{
  geiger_trigger_burst(3, 3);
}

void triggerGeigerBurst5()
{
  geiger_trigger_burst(5, 5);
}

// Extern used by Unity via DllImport: returns one filtered sample at 48 kHz.
extern "C" float asap_audio_render_sample()
{
  static bool sInitialized = false;
  static int16_t sCurrentSourceSample = 0;
  static uint32_t sSamplesUntilNextSource = 0;

  if (!sInitialized)
  {
    asap_audio_init();
    rc2_init();
    sCurrentSourceSample = 0;
    sSamplesUntilNextSource = 0;
    sInitialized = true;
  }

  const uint32_t upsampleFactor =
      static_cast<uint32_t>(kOutputSampleRateHz /
                            asap::audio::kSampleRateHz);

  if (sSamplesUntilNextSource == 0)
  {
    sCurrentSourceSample = asap_audio_get_sample();
    sSamplesUntilNextSource = upsampleFactor - 1U;
  }
  else
  {
    --sSamplesUntilNextSource;
  }

  const float x =
      static_cast<float>(sCurrentSourceSample) / 32768.0f;
  const float y = rc2_apply(x);

  float clamped = y;
  if (clamped > 1.0)
  {
    clamped = 1.0;
  }
  else if (clamped < -1.0)
  {
    clamped = -1.0;
  }

  return static_cast<float>(clamped);
}

}  // namespace

void test_audio_snapshots_generate_wav_files()
{
  const uint32_t totalSamples =
      static_cast<uint32_t>(
          (asap::audio::kSnapshotDurationMs / 1000.0) *
          kOutputSampleRateHz);

  std::filesystem::path dir = snapshotsDir();

  int16_t* buffer = new int16_t[totalSamples];

  // Hybrid Geiger click snapshots
  renderSnapshot(buffer, totalSamples, &triggerGeigerHybridSingle);
  writeWav16Mono(dir / "geiger_hybrid_single.wav", buffer, totalSamples);

  renderSnapshot(buffer, totalSamples, &triggerGeigerBurst3);
  writeWav16Mono(dir / "geiger_hybrid_burst3.wav", buffer, totalSamples);

  renderSnapshot(buffer, totalSamples, &triggerGeigerBurst5);
  writeWav16Mono(dir / "geiger_hybrid_burst5.wav", buffer, totalSamples);

  delete[] buffer;
}

// Provide a stub for the UART frame round-trip test so the shared main in
// test_main.cpp can link in this suite without pulling test_player_data.
void test_uart_frame_playerpersistent_roundtrip(void)
{
  // Intentionally empty in this test group.
}

#endif  // ARDUINO
