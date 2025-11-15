#pragma once

#include <stdint.h>

namespace asap::audio
{

// Core engine configuration
constexpr uint32_t kSampleRateHz = 16000U;
constexpr uint32_t kAudioSampleRate = kSampleRateHz;
constexpr int16_t kMaxSampleValue = 32767;
constexpr int16_t kMinSampleValue = -32768;

// Geiger click parameters
constexpr uint16_t kGeigerAttackSamples = 64U;      // 4 ms @ 16 kHz
constexpr uint16_t kGeigerTailMaxSamples = 1280U;   // 80 ms max tail
constexpr uint8_t kGeigerTailMaxEnv = 255U;
constexpr uint8_t kGeigerTailDecayFactor = 255U;    // ≈0.995 per sample

// Beep limits (restored from legacy engine)
constexpr uint16_t kBeepMinFreqHz = 200U;
constexpr uint16_t kBeepMaxFreqHz = 3000U;

// Default beep parameters for high-level patterns.
constexpr uint16_t kBeepSingleFreqHz = 1000U;
constexpr uint16_t kBeepSingleDurationMs = 200U;
constexpr uint8_t kBeepSingleLevel = 192U;  // 0-255

constexpr uint16_t kBeepDoubleFreqHz = 1000U;
constexpr uint16_t kBeepDoubleToneDurationMs = 150U;
constexpr uint16_t kBeepDoubleGapDurationMs = 150U;
constexpr uint8_t kBeepDoubleLevel = 192U;

constexpr uint16_t kBeepErrorFreqHz = 400U;
constexpr uint16_t kBeepErrorDurationMs = 700U;
constexpr uint8_t kBeepErrorLevel = 224U;

constexpr uint16_t kBeepAlertFreqHz = 2000U;
constexpr uint16_t kBeepAlertDurationMs = 80U;
constexpr uint8_t kBeepAlertLevel = 224U;

// Total snapshot duration used in the test harness (ms).
constexpr uint32_t kSnapshotDurationMs = 2000U;

enum class BeepPattern : uint8_t
{
  Single = 0,
  Double = 1,
  Error = 2,
  Alert = 3,
};

}  // namespace asap::audio

