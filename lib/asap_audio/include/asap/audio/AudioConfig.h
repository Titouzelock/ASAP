#pragma once

#include <stdint.h>

namespace asap::audio
{

// Core engine configuration ---------------------------------------------------
// All audio computations in the engine are done at this fixed sample rate.
// Both the MCU build and the native test harness assume 16 kHz.
constexpr uint32_t kSampleRateHz = 16000U;
constexpr uint32_t kAudioSampleRate = kSampleRateHz;
constexpr int16_t kMaxSampleValue = 32767;
constexpr int16_t kMinSampleValue = -32768;

// Geiger click parameters -----------------------------------------------------
// Number of samples in the recorded attack segment. At 16 kHz this is 4 ms.
constexpr uint16_t kGeigerAttackSamples = 64U;      // 4 ms @ 16 kHz
// Maximum number of samples a click tail is allowed to live. At 16 kHz this
// is 80 ms. The exact perceived length also depends on the envelope decay.
constexpr uint16_t kGeigerTailMaxSamples = 1280U;   // 80 ms max tail
// Maximum value for the tail envelope. Envelopes are 16-bit fixed-point
// values in the range [0, 65535]. 65535 represents "full scale".
constexpr uint16_t kGeigerTailMaxEnv = 12000U;
// Tail decay factor: 16-bit fixed-point multiplier applied once per sample.
// Effective multiplier is (kGeigerTailDecayFactor / 65536.0).
constexpr uint16_t kGeigerTailDecayFactor =
    65300U;  // 255/256 ≈ 0.996 per sample

// Geiger burst spacing (engine samples @ 16 kHz) -----------------------------
// Minimum and maximum delay (in engine samples) between clicks inside a burst.
// 32 samples ≈ 2 ms, 200 samples ≈ 12.5 ms at 16 kHz.
constexpr uint16_t kGeigerBurstMinDelaySamples = 32U;   // 2 ms
constexpr uint16_t kGeigerBurstMaxDelaySamples = 512U;  

// Geiger envelopes / decay tuning (16-bit envelopes) -------------------------
// Initial value for the attack envelope lookup table (full scale).
constexpr uint16_t kGeigerAttackInitialEnv = 65535U;
// Attack decay factor: 16-bit fixed-point multiplier per sample. This is the
// 16-bit equivalent of 241/256 ≈ 0.94 used in the original engine.
constexpr uint16_t kGeigerAttackDecayFactor =
    65500U;  // 241/256 * 65536

// Initial tail envelope level when a new click is started. For monophonic
// Geiger behavior this controls the loudness of each tail.
constexpr uint16_t kGeigerTailInitialEnv = kGeigerTailMaxEnv;

// Geiger tail realism parameters (frequency jitter and noise blend) ----------
// Bit mask and offset used to extract a small signed jitter from the RNG and
// perturb the 440 Hz tail frequency.
constexpr uint8_t kGeigerTailJitterMask = 0x3FU;       // bits used for jitter
constexpr int8_t kGeigerTailJitterOffset = 32;         // center of jitter range
// Bit mask and offset used to derive a small signed noise sample from the RNG.
constexpr uint16_t kGeigerTailNoiseMask = 0x03FFU;     // noise magnitude mask
constexpr int16_t kGeigerTailNoiseOffset = 512;        // center of noise range

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




