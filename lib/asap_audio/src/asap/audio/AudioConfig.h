#pragma once

#include <stdint.h>

namespace asap::audio
{

// Core engine configuration
constexpr uint32_t kSampleRateHz = 8000U;

// Geiger click parameters
constexpr uint16_t kGeigerClickLengthSamples = 40U;   // ~5 ms at 8 kHz
constexpr uint8_t kGeigerMaxEnvelope = 255U;          // start envelope level

// When a beep is active, the geiger channel is attenuated by this factor
// (fixed-point: value / 256.0f). 102 ~= 0.4.
constexpr uint8_t kGeigerAttenuationWhileBeep = 102U;

// Beep limits
constexpr uint16_t kBeepMinFreqHz = 200U;
constexpr uint16_t kBeepMaxFreqHz = 3000U;

// Default beep parameters for high-level patterns. These are chosen to be
// easily tweakable by humans and are documented in docs/audio.md.
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

