#include <asap/audio/AudioEngine.h>

namespace asap::audio
{

namespace
{

// ---------------------------------------------------------------------------
// Geiger click engine (hybrid: recorded attack + synthetic tail)
// ---------------------------------------------------------------------------

struct GeigerClick
{
  bool active;
  uint16_t pos;        // 0..kGeigerTailMaxSamples
  uint16_t tailEnv;    // 0..kGeigerTailMaxEnv
  uint16_t tailPhase;  // phase accumulator for tail tone
};

struct GeigerBurstState
{
  bool active;
  uint8_t remaining;
  uint16_t delay;   // samples until next click
};

static GeigerClick gClicks[4];
static GeigerBurstState gBurst{
    /*active=*/false,
    /*remaining=*/0U,
    /*delay=*/0U};

static uint16_t gNoiseLfsr = 0xACE1u;

// 4 ms recorded attack at 16 kHz (64 samples).
static const int16_t kGeigerAttack64[kGeigerAttackSamples] = {
    -26612, -15939, 24189,  32767,  5658,   -21471, -15114, 11338,
    30639,  28004,  1996,   -31360, -28663, 4494,   18426,  7844,
    -14819, -30689, -1667,  19378,  2287,   -16351, -9988,  -3017,
    2211,   1420,   -8898,  -5941,  5718,   8788,   -2425,  -2312,
    2107,   2639,   4447,   7048,   2570,   2237,   7563,   6984,
    3515,   3657,   3819,   3198,   3517,   -946,   -4220,  1274,
    4574,   46,     -3741,  -3540,  -2690,  -2745,  -2499,  -4711,
    -6084,  -4363,  -2644,  -3323,  -3966,  -4482,  -3431,  -1200};

// 256-entry sine LUT, full-scale [-32767..32767].
static const int16_t kSinLut16[256] = {
    0,     804,   1608,  2410,  3212,  4011,  4808,  5602,
    6393,  7179,  7962,  8739,  9512,  10278, 11039, 11793,
    12539, 13279, 14010, 14732, 15446, 16151, 16846, 17530,
    18204, 18868, 19519, 20159, 20787, 21403, 22005, 22594,
    23170, 23731, 24279, 24811, 25329, 25832, 26319, 26790,
    27245, 27683, 28105, 28510, 28898, 29268, 29621, 29956,
    30273, 30571, 30852, 31113, 31356, 31580, 31785, 31971,
    32137, 32285, 32412, 32521, 32609, 32678, 32728, 32757,
    32767, 32757, 32728, 32678, 32609, 32521, 32412, 32285,
    32137, 31971, 31785, 31580, 31356, 31113, 30852, 30571,
    30273, 29956, 29621, 29268, 28898, 28510, 28105, 27683,
    27245, 26790, 26319, 25832, 25329, 24811, 24279, 23731,
    23170, 22594, 22005, 21403, 20787, 20159, 19519, 18868,
    18204, 17530, 16846, 16151, 15446, 14732, 14010, 13279,
    12539, 11793, 11039, 10278, 9512,  8739,  7962,  7179,
    6393,  5602,  4808,  4011,  3212,  2410,  1608,  804,
    0,     -804,  -1608, -2410, -3212, -4011, -4808, -5602,
    -6393, -7179, -7962, -8739, -9512, -10278, -11039, -11793,
    -12539, -13279, -14010, -14732, -15446, -16151, -16846, -17530,
    -18204, -18868, -19519, -20159, -20787, -21403, -22005, -22594,
    -23170, -23731, -24279, -24811, -25329, -25832, -26319, -26790,
    -27245, -27683, -28105, -28510, -28898, -29268, -29621, -29956,
    -30273, -30571, -30852, -31113, -31356, -31580, -31785, -31971,
    -32137, -32285, -32412, -32521, -32609, -32678, -32728, -32757,
    -32767, -32757, -32728, -32678, -32609, -32521, -32412, -32285,
    -32137, -31971, -31785, -31580, -31356, -31113, -30852, -30571,
    -30273, -29956, -29621, -29268, -28898, -28510, -28105, -27683,
    -27245, -26790, -26319, -25832, -25329, -24811, -24279, -23731,
    -23170, -22594, -22005, -21403, -20787, -20159, -19519, -18868,
    -18204, -17530, -16846, -16151, -15446, -14732, -14010, -13279,
    -12539, -11793, -11039, -10278, -9512,  -8739,  -7962,  -7179,
    -6393,  -5602,  -4808,  -4011,  -3212,  -2410,  -1608,  -804};

constexpr uint16_t kGeigerTailPhaseStep = 1802U;  // ~440 Hz at 16 kHz

static uint16_t nextRandom()
{
  // 16-bit maximal-length LFSR with taps at 16, 14, 13, 11 (0xB400).
  uint16_t lfsr = gNoiseLfsr;
  uint16_t lsb = static_cast<uint16_t>(lfsr & 1U);
  lfsr = static_cast<uint16_t>(lfsr >> 1U);
  if (lsb != 0U)
  {
    lfsr = static_cast<uint16_t>(lfsr ^ 0xB400u);
  }

  if (lfsr == 0U)
  {
    lfsr = 0xACE1u;
  }

  gNoiseLfsr = lfsr;
  return lfsr;
}

static uint16_t randomInRange(uint16_t minVal, uint16_t maxVal)
{
  if (maxVal < minVal)
  {
    maxVal = minVal;
  }

  uint16_t r = nextRandom();
  uint16_t span = static_cast<uint16_t>(maxVal - minVal + 1U);
  return static_cast<uint16_t>(minVal + (r % span));
}

static int16_t geigerGetSampleInternal()
{
  // Precomputed attack envelope LUT (generated on first use) using the
  // configured exponential decay factor.
  static bool sAttackEnvInitialized = false;
  static uint16_t sAttackEnvLut[kGeigerAttackSamples];

  if (!sAttackEnvInitialized)
  {
    uint32_t env = kGeigerAttackInitialEnv;
    for (uint16_t i = 0; i < kGeigerAttackSamples; ++i)
    {
      sAttackEnvLut[i] = static_cast<uint16_t>(env);
      env = (env * kGeigerAttackDecayFactor) >> 16;
      if (env == 0U)
      {
        env = 1U;
      }
    }
    sAttackEnvInitialized = true;
  }

  int32_t acc = 0;
  for (uint32_t i = 0; i < 4U; ++i)
  {
    GeigerClick &c = gClicks[i];
    if (c.active)
    {
      uint16_t pos = c.pos;

      // End-of-life based on maximum tail length.
      if (pos >= kGeigerTailMaxSamples)
      {
        c.active = false;
        continue;
      }

      int32_t wave = 0;

      if (pos < kGeigerAttackSamples)
      {
        // Attack region: apply a light exponential envelope to the
        // recorded attack sample.
        const uint16_t attackEnv = sAttackEnvLut[pos];
        const int16_t raw = kGeigerAttack64[pos];
        wave = (static_cast<int32_t>(raw) *
                static_cast<int32_t>(attackEnv)) >>
               16;
      }
      else
      {
        // Tail region: decaying 440 Hz tone with small random jitter
        // and a low-level noise component for realism.
        uint16_t tailEnv = c.tailEnv;
        if (tailEnv == 0U)
        {
          c.active = false;
          continue;
        }

        // Random frequency jitter of roughly ±2%.
        const uint16_t rJitter = nextRandom();
        const int16_t jitter =
            static_cast<int16_t>((rJitter & kGeigerTailJitterMask) -
                                 kGeigerTailJitterOffset);
        int32_t stepWithJitter =
            static_cast<int32_t>(kGeigerTailPhaseStep) +
            static_cast<int32_t>(jitter);
        if (stepWithJitter < 0)
        {
          stepWithJitter = 0;
        }

        c.tailPhase = static_cast<uint16_t>(
            c.tailPhase +
            static_cast<uint16_t>(stepWithJitter));
        const uint8_t idx =
            static_cast<uint8_t>(c.tailPhase >> 8);
        const int16_t s = kSinLut16[idx];

        const int32_t tone =
            (static_cast<int32_t>(s) *
             static_cast<int32_t>(tailEnv)) >>
            16;

        // Wideband noise at about -40 dB relative to full-scale,
        // also shaped by the tail envelope.
        const uint16_t rNoise = nextRandom();
        const int16_t noiseSmall =
            static_cast<int16_t>(
                (static_cast<int16_t>(rNoise & kGeigerTailNoiseMask)) -
                kGeigerTailNoiseOffset);
        const int32_t noise =
            (static_cast<int32_t>(noiseSmall) *
             static_cast<int32_t>(tailEnv)) >>
            16;

        wave = tone + noise;

        // Tail envelope decay: simple exponential.
        uint16_t decayed =
            static_cast<uint16_t>(
                (static_cast<uint32_t>(tailEnv) *
                 static_cast<uint32_t>(kGeigerTailDecayFactor)) >>
                16);
        c.tailEnv = decayed;
      }

      c.pos = static_cast<uint16_t>(pos + 1U);

      if (wave > kMaxSampleValue)
      {
        wave = kMaxSampleValue;
      }
      else if (wave < kMinSampleValue)
      {
        wave = kMinSampleValue;
      }

      acc += wave;
    }
  }

  if (acc > kMaxSampleValue)
  {
    acc = kMaxSampleValue;
  }
  else if (acc < kMinSampleValue)
  {
    acc = kMinSampleValue;
  }

  return static_cast<int16_t>(acc);
}

// ---------------------------------------------------------------------------
// Beep engine (restored from legacy implementation)
// ---------------------------------------------------------------------------

struct BeepState
{
  uint32_t phase;
  uint32_t phaseStep;
  uint32_t samplesRemaining;
  uint8_t level;
};

struct PatternStep
{
  bool beep;
  uint16_t freqHz;
  uint16_t durationMs;
  uint8_t level;
};

struct PatternState
{
  const PatternStep *steps;
  uint8_t stepCount;
  uint8_t currentIndex;
  uint32_t samplesRemainingInStep;
  bool active;
};

static BeepState gBeep{
    /*phase=*/0U,
    /*phaseStep=*/0U,
    /*samplesRemaining=*/0U,
    /*level=*/0U};

static PatternState gPattern{
    /*steps=*/nullptr,
    /*stepCount=*/0U,
    /*currentIndex=*/0U,
    /*samplesRemainingInStep=*/0U,
    /*active=*/false};

constexpr PatternStep kPatternSingle[] = {
    {/*beep=*/true, kBeepSingleFreqHz, kBeepSingleDurationMs,
     kBeepSingleLevel},
};

constexpr PatternStep kPatternDouble[] = {
    {/*beep=*/true, kBeepDoubleFreqHz, kBeepDoubleToneDurationMs,
     kBeepDoubleLevel},
    {/*beep=*/false, 0U, kBeepDoubleGapDurationMs, 0U},
    {/*beep=*/true, kBeepDoubleFreqHz, kBeepDoubleToneDurationMs,
     kBeepDoubleLevel},
};

constexpr PatternStep kPatternError[] = {
    {/*beep=*/true, kBeepErrorFreqHz, kBeepErrorDurationMs, kBeepErrorLevel},
};

constexpr PatternStep kPatternAlert[] = {
    {/*beep=*/true, kBeepAlertFreqHz, kBeepAlertDurationMs, kBeepAlertLevel},
};

static bool isBeepActive()
{
  return gBeep.samplesRemaining > 0U;
}

static void startPattern(BeepPattern pattern)
{
  switch (pattern)
  {
    case BeepPattern::Single:
      gPattern.steps = kPatternSingle;
      gPattern.stepCount =
          static_cast<uint8_t>(sizeof(kPatternSingle) /
                               sizeof(PatternStep));
      break;
    case BeepPattern::Double:
      gPattern.steps = kPatternDouble;
      gPattern.stepCount =
          static_cast<uint8_t>(sizeof(kPatternDouble) /
                               sizeof(PatternStep));
      break;
    case BeepPattern::Error:
      gPattern.steps = kPatternError;
      gPattern.stepCount =
          static_cast<uint8_t>(sizeof(kPatternError) /
                               sizeof(PatternStep));
      break;
    case BeepPattern::Alert:
      gPattern.steps = kPatternAlert;
      gPattern.stepCount =
          static_cast<uint8_t>(sizeof(kPatternAlert) /
                               sizeof(PatternStep));
      break;
    default:
      gPattern.steps = nullptr;
      gPattern.stepCount = 0U;
      break;
  }

  gPattern.currentIndex = 0U;
  gPattern.samplesRemainingInStep = 0U;
  gPattern.active =
      (gPattern.steps != nullptr && gPattern.stepCount > 0U);
}

static int16_t beepGetSampleInternal();

static void advancePatternSample()
{
  if (!gPattern.active || gPattern.steps == nullptr ||
      gPattern.stepCount == 0U)
  {
    return;
  }

  if (gPattern.samplesRemainingInStep == 0U)
  {
    if (gPattern.currentIndex >= gPattern.stepCount)
    {
      gPattern.active = false;
      return;
    }

    const PatternStep &step = gPattern.steps[gPattern.currentIndex];
    const uint32_t samples =
        (static_cast<uint32_t>(step.durationMs) * kSampleRateHz +
         999U) /
        1000U;
    gPattern.samplesRemainingInStep = samples;

    if (step.beep)
    {
      // Use public API equivalent to legacy beep behavior.
      beep_start(step.freqHz, step.durationMs, step.level);
    }

    ++gPattern.currentIndex;
  }

  if (gPattern.samplesRemainingInStep > 0U)
  {
    --gPattern.samplesRemainingInStep;
  }
}

static int16_t beepGetSampleInternal()
{
  if (!isBeepActive())
  {
    return 0;
  }

  const bool high = (gBeep.phase & 0x80000000u) != 0U;
  gBeep.phase += gBeep.phaseStep;

  if (gBeep.samplesRemaining > 0U)
  {
    --gBeep.samplesRemaining;
  }

  const int32_t amplitude =
      static_cast<int32_t>(gBeep.level) * 128;
  const int32_t sample = high ? amplitude : -amplitude;
  return static_cast<int16_t>(sample);
}

// ---------------------------------------------------------------------------
// Mixer
// ---------------------------------------------------------------------------

static int16_t mixSample()
{
  int32_t acc = 0;

  // Burst scheduling: handled at mixer rate (16 kHz).
  if (gBurst.active)
  {
    if (gBurst.delay > 0U)
    {
      --gBurst.delay;
    }

    if (gBurst.delay == 0U && gBurst.remaining > 0U)
    {
      // Monophonic behavior: kill any existing tails and start a fresh click
      // in the first slot so only one tail is ever active.
      for (uint32_t i = 0; i < 4U; ++i)
      {
        gClicks[i].active = false;
        gClicks[i].pos = 0U;
        gClicks[i].tailEnv = 0U;
        gClicks[i].tailPhase = 0U;
      }

      GeigerClick &c = gClicks[0];
      c.active = true;
      c.pos = 0U;
      c.tailEnv = kGeigerTailInitialEnv;
      c.tailPhase = 0U;

      if (gBurst.remaining > 0U)
      {
        --gBurst.remaining;
      }

      if (gBurst.remaining == 0U)
      {
        gBurst.active = false;
      }
      else
      {
        gBurst.delay = randomInRange(32U, 512U);
      }
    }
  }

  acc += static_cast<int32_t>(geigerGetSampleInternal());
  acc += static_cast<int32_t>(beepGetSampleInternal());

  if (acc > kMaxSampleValue)
  {
    acc = kMaxSampleValue;
  }
  else if (acc < kMinSampleValue)
  {
    acc = kMinSampleValue;
  }

  return static_cast<int16_t>(acc);
}

}  // namespace

// Public API -----------------------------------------------------------------

void init()
{
  for (uint32_t i = 0; i < 4U; ++i)
  {
    gClicks[i].active = false;
    gClicks[i].pos = 0U;
    gClicks[i].tailEnv = 0U;
    gClicks[i].tailPhase = 0U;
  }

  gBurst.active = false;
  gBurst.remaining = 0U;
  gBurst.delay = 0U;

  gNoiseLfsr = 0xACE1u;

  gBeep.phase = 0U;
  gBeep.phaseStep = 0U;
  gBeep.samplesRemaining = 0U;
  gBeep.level = 0U;

  gPattern.steps = nullptr;
  gPattern.stepCount = 0U;
  gPattern.currentIndex = 0U;
  gPattern.samplesRemainingInStep = 0U;
  gPattern.active = false;
}

int16_t getSample()
{
  advancePatternSample();
  return mixSample();
}

void geigerTriggerClick()
{
  // Monophonic behavior: clear any existing click tails and re-arm slot 0.
  for (uint32_t i = 0; i < 4U; ++i)
  {
    gClicks[i].active = false;
    gClicks[i].pos = 0U;
    gClicks[i].tailEnv = 0U;
    gClicks[i].tailPhase = 0U;
  }

  GeigerClick &c = gClicks[0];
  c.active = true;
  c.pos = 0U;
  c.tailEnv = kGeigerTailInitialEnv;
  c.tailPhase = 0U;
}

void geigerTriggerBurst(uint8_t minCount, uint8_t maxCount)
{
  if (minCount == 0U)
  {
    minCount = 1U;
  }

  if (maxCount < minCount)
  {
    maxCount = minCount;
  }

  uint8_t count =
      static_cast<uint8_t>(randomInRange(minCount, maxCount));

  gBurst.active = true;
  gBurst.remaining = count;
  gBurst.delay = randomInRange(32U, 512U);
}

void beep_start(uint16_t freq_hz,
                uint16_t duration_ms,
                uint8_t level_0_255)
{
  if (freq_hz < kBeepMinFreqHz)
  {
    freq_hz = kBeepMinFreqHz;
  }
  else if (freq_hz > kBeepMaxFreqHz)
  {
    freq_hz = kBeepMaxFreqHz;
  }

  const uint32_t samples =
      (static_cast<uint32_t>(duration_ms) * kSampleRateHz + 999U) /
      1000U;

  if (samples == 0U || level_0_255 == 0U)
  {
    gBeep.samplesRemaining = 0U;
    gBeep.level = 0U;
    return;
  }

  gBeep.level = level_0_255;
  gBeep.samplesRemaining = samples;

  const uint64_t numerator =
      static_cast<uint64_t>(freq_hz) *
      (static_cast<uint64_t>(1) << 32);
  gBeep.phaseStep = static_cast<uint32_t>(
      numerator / static_cast<uint64_t>(kSampleRateHz));
}

void beep_stop_all()
{
  gBeep.phase = 0U;
  gBeep.phaseStep = 0U;
  gBeep.samplesRemaining = 0U;
  gBeep.level = 0U;

  gPattern.steps = nullptr;
  gPattern.stepCount = 0U;
  gPattern.currentIndex = 0U;
  gPattern.samplesRemainingInStep = 0U;
  gPattern.active = false;
}

void beep_pattern_start(uint8_t pattern_id)
{
  BeepPattern pattern = BeepPattern::Single;

  switch (pattern_id)
  {
    case static_cast<uint8_t>(BeepPattern::Single):
      pattern = BeepPattern::Single;
      break;
    case static_cast<uint8_t>(BeepPattern::Double):
      pattern = BeepPattern::Double;
      break;
    case static_cast<uint8_t>(BeepPattern::Error):
      pattern = BeepPattern::Error;
      break;
    case static_cast<uint8_t>(BeepPattern::Alert):
      pattern = BeepPattern::Alert;
      break;
    default:
      pattern = BeepPattern::Single;
      break;
  }

  startPattern(pattern);
}

}  // namespace asap::audio

// C wrappers -----------------------------------------------------------------

extern "C"
{

void asap_audio_init()
{
  asap::audio::init();
}

int16_t asap_audio_get_sample()
{
  return asap::audio::getSample();
}

void geiger_trigger_click()
{
  asap::audio::geigerTriggerClick();
}

void geiger_trigger_burst(uint8_t minCount, uint8_t maxCount)
{
  asap::audio::geigerTriggerBurst(minCount, maxCount);
}

void beep_start(uint16_t freq_hz,
                uint16_t duration_ms,
                uint8_t level_0_255)
{
  asap::audio::beep_start(freq_hz, duration_ms, level_0_255);
}

void beep_stop_all()
{
  asap::audio::beep_stop_all();
}

void beep_pattern_start(uint8_t pattern_id)
{
  asap::audio::beep_pattern_start(pattern_id);
}

}
