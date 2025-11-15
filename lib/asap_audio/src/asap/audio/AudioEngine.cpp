#include <asap/audio/AudioEngine.h>

namespace asap::audio
{

namespace
{

// ---------------------------------------------------------------------------
// Geiger click engine
// ---------------------------------------------------------------------------

struct GeigerState
{
  uint16_t lfsr;
  uint16_t position;
  bool active;
  uint8_t envelope;
  uint8_t tailEnvelope;
  uint16_t tailPhase;
};

GeigerState gGeiger{
    /*lfsr=*/0xACE1u,
    /*position=*/0U,
    /*active=*/false,
    /*envelope=*/0U,
    /*tailEnvelope=*/0U,
    /*tailPhase=*/0U,
};

// Hybrid Geiger click: 64-sample real attack captured from a measured waveform.
static const int16_t realAttack64[64] = {
    -26612, -15939,  24189,  32767,   5658, -21471, -15114,  11338,
     30639,  28004,   1996, -31360, -28663,   4494,  18426,   7844,
    -14819, -30689,  -1667,  19378,   2287, -16351,  -9988,  -3017,
      2211,   1420,  -8898,  -5941,   5718,   8788,  -2425,  -2312,
      2107,   2639,   4447,   7048,   2570,   2237,   7563,   6984,
      3515,   3657,   3819,   3198,   3517,   -946,  -4220,   1274,
      4574,     46,  -3741,  -3540,  -2690,  -2745,  -2499,  -4711,
     -6084,  -4363,  -2644,  -3323,  -3966,  -4482,  -3431,  -1200
};

// 256-entry sine lookup table covering 0..2π, scaled to int16_t.
static const int16_t kSinLut16[256] = {
    0,    804,   1608,  2410,  3212,  4011,  4808,  5602,
    6393, 7179,  7962,  8739,  9512,  10278, 11039, 11793,
    12539,13278, 14010, 14732, 15446, 16150, 16846, 17530,
    18204,18868, 19519, 20159, 20787, 21402, 22004, 22594,
    23170,23731, 24279, 24811, 25329, 25832, 26319, 26790,
    27246,27684, 28107, 28511, 28899, 29269, 29621, 29956,
    30272,30571, 30851, 31113, 31356, 31580, 31785, 31971,
    32137,32285, 32413, 32522, 32611, 32681, 32731, 32762,
    32767,32762, 32731, 32681, 32611, 32522, 32413, 32285,
    32137,31971, 31785, 31580, 31356, 31113, 30851, 30571,
    30272,29956, 29621, 29269, 28899, 28511, 28107, 27684,
    27246,26790, 26319, 25832, 25329, 24811, 24279, 23731,
    23170,22594, 22004, 21402, 20787, 20159, 19519, 18868,
    18204,17530, 16846, 16150, 15446, 14732, 14010, 13278,
    12539,11793, 11039, 10278, 9512,  8739,  7962,  7179,
    6393, 5602,  4808,  4011,  3212,  2410,  1608,  804,
    0,    -804,  -1608, -2410, -3212, -4011, -4808, -5602,
    -6393,-7179, -7962, -8739, -9512, -10278,-11039,-11793,
    -12539,-13278,-14010,-14732,-15446,-16150,-16846,-17530,
    -18204,-18868,-19519,-20159,-20787,-21402,-22004,-22594,
    -23170,-23731,-24279,-24811,-25329,-25832,-26319,-26790,
    -27246,-27684,-28107,-28511,-28899,-29269,-29621,-29956,
    -30272,-30571,-30851,-31113,-31356,-31580,-31785,-31971,
    -32137,-32285,-32413,-32522,-32611,-32681,-32731,-32762,
    -32767,-32762,-32731,-32681,-32611,-32522,-32413,-32285,
    -32137,-31971,-31785,-31580,-31356,-31113,-30851,-30571,
    -30272,-29956,-29621,-29269,-28899,-28511,-28107,-27684,
    -27246,-26790,-26319,-25832,-25329,-24811,-24279,-23731,
    -23170,-22594,-22004,-21402,-20787,-20159,-19519,-18868,
    -18204,-17530,-16846,-16150,-15446,-14732,-14010,-13278,
    -12539,-11793,-11039,-10278,-9512, -8739, -7962, -7179,
    -6393,-5602, -4808, -4011, -3212, -2410, -1608, -804
};

struct BurstState
{
  bool active;
  uint8_t remaining;
  uint16_t nextTriggerDelay;  // in samples
};

BurstState gBurst{
    /*active=*/false,
    /*remaining=*/0U,
    /*nextTriggerDelay=*/0U,
};

static uint16_t nextRandom()
{
  // Simple 16-bit LFSR-based pseudo-random generator.
  static uint16_t lfsr = 0xACE1u;
  uint16_t lsb = lfsr & 1U;
  lfsr >>= 1U;
  if (lsb != 0U)
  {
    lfsr ^= 0xB400u;
  }
  if (lfsr == 0U)
  {
    lfsr = 0xACE1u;
  }
  return lfsr;
}

static uint16_t randomInRange(uint16_t minVal, uint16_t maxVal)
{
  if (minVal > maxVal)
  {
    uint16_t tmp = minVal;
    minVal = maxVal;
    maxVal = tmp;
  }
  const uint16_t span = static_cast<uint16_t>(maxVal - minVal + 1U);
  const uint16_t r = nextRandom();
  return static_cast<uint16_t>(minVal + (r % span));
}

int16_t generateNoiseSample()
{
  // 16-bit maximal-length LFSR with taps at 16, 14, 13, 11 (0xB400).
  // This is simple, deterministic and runs well on STM32-class MCUs.
  uint16_t lfsr = gGeiger.lfsr;
  uint16_t lsb = lfsr & 1U;
  lfsr >>= 1U;
  if (lsb != 0U)
  {
    lfsr ^= 0xB400u;
  }
  gGeiger.lfsr = lfsr == 0U ? 0xACE1u : lfsr;  // avoid the all-zero lock-up

  // Spread bits over signed 16-bit space and apply a simple shaping by
  // centering around zero.
  int16_t raw = static_cast<int16_t>(gGeiger.lfsr);
  return raw;
}

int16_t geigerSampleInternal()
{
  if (!gGeiger.active)
  {
    return 0;
  }

  if (gGeiger.position >= kGeigerClickTotalSamples)
  {
    gGeiger.active = false;
    return 0;
  }

  const uint16_t pos = gGeiger.position;
  ++gGeiger.position;

  uint16_t masterEnv = gGeiger.envelope;
  uint16_t tailEnv = gGeiger.tailEnvelope;

  int32_t wave = 0;
  if (pos < kGeigerClickAttackSamples)
  {
    // ATTACK: fast decay on master envelope, tail envelope held full.
    masterEnv = static_cast<uint16_t>((masterEnv * 241U) >> 8U);
    tailEnv = kGeigerMaxEnvelope;

    const int16_t attackSample = realAttack64[pos];
    wave = static_cast<int32_t>(attackSample) *
           static_cast<int32_t>(masterEnv);
  }
  else
  {
    // TAIL: master envelope full, slow decay on tail envelope only.
    masterEnv = kGeigerMaxEnvelope;
    tailEnv = static_cast<uint16_t>((tailEnv * 255U) >> 8U);

    if (tailEnv == 0U)
    {
      gGeiger.active = false;
      gGeiger.envelope = 0U;
      gGeiger.tailEnvelope = 0U;
      return 0;
    }

    constexpr uint16_t kTailPhaseStep = 1802U;  // 440 Hz @ 16 kHz
    gGeiger.tailPhase =
        static_cast<uint16_t>(gGeiger.tailPhase + kTailPhaseStep);
    const uint8_t phaseIndex =
        static_cast<uint8_t>(gGeiger.tailPhase >> 8);
    const int16_t sinSample = kSinLut16[phaseIndex];

    wave = static_cast<int32_t>(sinSample) *
           static_cast<int32_t>(tailEnv);
  }

  gGeiger.envelope = static_cast<uint8_t>(masterEnv);
  gGeiger.tailEnvelope = static_cast<uint8_t>(tailEnv);

  // Single scaling stage for both attack and tail.
  int32_t scaled = wave >> 8;

  if (scaled > 32767)
  {
    scaled = 32767;
  }
  else if (scaled < -32768)
  {
    scaled = -32768;
  }

  return static_cast<int16_t>(scaled);
}

// ---------------------------------------------------------------------------
// Beep engine
// ---------------------------------------------------------------------------

struct BeepState
{
  uint32_t phase;
  uint32_t phaseStep;
  uint32_t samplesRemaining;
  uint8_t level;
};

BeepState gBeep{
    /*phase=*/0U,
    /*phaseStep=*/0U,
    /*samplesRemaining=*/0U,
    /*level=*/0U,
};

bool isBeepActive()
{
  return gBeep.samplesRemaining > 0U;
}

int16_t beepSampleInternal()
{
  if (!isBeepActive())
  {
    return 0;
  }

  // Simple square-wave using the top bit of the phase accumulator.
  const bool high = (gBeep.phase & 0x80000000u) != 0U;
  gBeep.phase += gBeep.phaseStep;

  if (gBeep.samplesRemaining > 0U)
  {
    --gBeep.samplesRemaining;
  }

  const int32_t amplitude =
      static_cast<int32_t>(gBeep.level) * 128;  // maps 255 -> ~32768
  const int32_t sample = high ? amplitude : -amplitude;
  return static_cast<int16_t>(sample);
}

// ---------------------------------------------------------------------------
// Beep pattern controller
// ---------------------------------------------------------------------------

struct PatternStep
{
  bool beep;
  uint16_t freqHz;
  uint16_t durationMs;
  uint8_t level;
};

struct PatternState
{
  const PatternStep* steps;
  uint8_t stepCount;
  uint8_t currentIndex;
  uint32_t samplesRemainingInStep;
  bool active;
};

PatternState gPattern{
    /*steps=*/nullptr,
    /*stepCount=*/0U,
    /*currentIndex=*/0U,
    /*samplesRemainingInStep=*/0U,
    /*active=*/false,
};

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

void startPattern(BeepPattern pattern)
{
  switch (pattern)
  {
    case BeepPattern::Single:
      gPattern.steps = kPatternSingle;
      gPattern.stepCount =
          static_cast<uint8_t>(sizeof(kPatternSingle) / sizeof(PatternStep));
      break;
    case BeepPattern::Double:
      gPattern.steps = kPatternDouble;
      gPattern.stepCount =
          static_cast<uint8_t>(sizeof(kPatternDouble) / sizeof(PatternStep));
      break;
    case BeepPattern::Error:
      gPattern.steps = kPatternError;
      gPattern.stepCount =
          static_cast<uint8_t>(sizeof(kPatternError) / sizeof(PatternStep));
      break;
    case BeepPattern::Alert:
      gPattern.steps = kPatternAlert;
      gPattern.stepCount =
          static_cast<uint8_t>(sizeof(kPatternAlert) / sizeof(PatternStep));
      break;
    default:
      gPattern.steps = nullptr;
      gPattern.stepCount = 0U;
      break;
  }

  gPattern.currentIndex = 0U;
  gPattern.samplesRemainingInStep = 0U;
  gPattern.active = (gPattern.steps != nullptr && gPattern.stepCount > 0U);
}

void advancePatternSample()
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

    const PatternStep& step = gPattern.steps[gPattern.currentIndex];
    const uint32_t samples =
        (static_cast<uint32_t>(step.durationMs) * kSampleRateHz + 999U) /
        1000U;
    gPattern.samplesRemainingInStep = samples;

    if (step.beep)
    {
      beepStart(step.freqHz, step.durationMs, step.level);
    }

    ++gPattern.currentIndex;
  }

  if (gPattern.samplesRemainingInStep > 0U)
  {
    --gPattern.samplesRemainingInStep;
  }
}

// ---------------------------------------------------------------------------
// Mixer
// ---------------------------------------------------------------------------

int16_t mixSample()
{
  const int16_t geigerSample =
      isBeepActive()
          ? static_cast<int16_t>(
                (static_cast<int32_t>(geigerSampleInternal()) *
                 static_cast<int32_t>(kGeigerAttenuationWhileBeep)) /
                256)
          : geigerSampleInternal();

  const int16_t beepSample = beepSampleInternal();

  int32_t mixed =
      static_cast<int32_t>(geigerSample) + static_cast<int32_t>(beepSample);

  if (mixed > 32767)
  {
    mixed = 32767;
  }
  else if (mixed < -32768)
  {
    mixed = -32768;
  }

  return static_cast<int16_t>(mixed);
}

}  // namespace

// Public API -----------------------------------------------------------------

void init()
{
  gGeiger.lfsr = 0xACE1u;
  gGeiger.position = 0U;
  gGeiger.active = false;
  gGeiger.envelope = 0U;
  gGeiger.tailEnvelope = 0U;
  gGeiger.tailPhase = 0U;

  gBeep.phase = 0U;
  gBeep.phaseStep = 0U;
  gBeep.samplesRemaining = 0U;
  gBeep.level = 0U;

  gPattern.steps = nullptr;
  gPattern.stepCount = 0U;
  gPattern.currentIndex = 0U;
  gPattern.samplesRemainingInStep = 0U;
  gPattern.active = false;

  gBurst.active = false;
  gBurst.remaining = 0U;
  gBurst.nextTriggerDelay = 0U;
}

void geigerTriggerClick()
{
  gGeiger.position = 0U;
  gGeiger.active = true;
  gGeiger.envelope = kGeigerMaxEnvelope;
  gGeiger.tailEnvelope = kGeigerMaxEnvelope;
  gGeiger.tailPhase = 0U;
}

void geigerTriggerBurst(uint8_t countMin, uint8_t countMax)
{
  if (countMin == 0U && countMax == 0U)
  {
    return;
  }
  const uint8_t minCount = (countMin == 0U) ? 1U : countMin;
  const uint8_t maxCount = (countMax < minCount) ? minCount : countMax;
  const uint16_t count =
      randomInRange(static_cast<uint16_t>(minCount),
                    static_cast<uint16_t>(maxCount));

  gBurst.active = true;
  gBurst.remaining = static_cast<uint8_t>(count);
  gBurst.nextTriggerDelay = randomInRange(64U, 960U);  // 4–60 ms at 16 kHz
}



void beepStart(uint16_t freqHz, uint16_t durationMs, uint8_t level0_255)
{
  if (freqHz < kBeepMinFreqHz)
  {
    freqHz = kBeepMinFreqHz;
  }
  else if (freqHz > kBeepMaxFreqHz)
  {
    freqHz = kBeepMaxFreqHz;
  }

  const uint32_t samples =
      (static_cast<uint32_t>(durationMs) * kSampleRateHz + 999U) / 1000U;

  if (samples == 0U || level0_255 == 0U)
  {
    gBeep.samplesRemaining = 0U;
    gBeep.level = 0U;
    return;
  }

  gBeep.level = level0_255;
  gBeep.samplesRemaining = samples;

  // Phase increment for 32-bit accumulator: step = freq * 2^32 / sampleRate.
  const uint64_t numerator =
      static_cast<uint64_t>(freqHz) * (static_cast<uint64_t>(1) << 32);
  gBeep.phaseStep =
      static_cast<uint32_t>(numerator / static_cast<uint64_t>(kSampleRateHz));
}

int16_t beepGetSample()
{
  return beepSampleInternal();
}

void beepPatternStart(BeepPattern pattern)
{
  startPattern(pattern);
}

int16_t getSample()
{
  // Burst scheduler: non-blocking Geiger click bursts.
  if (gBurst.active)
  {
    // Ensure delays stay in the configured 2–32 ms window.
    if (gBurst.nextTriggerDelay < 32U || gBurst.nextTriggerDelay > 512U)
    {
      gBurst.nextTriggerDelay = randomInRange(32U, 512U);
    }

    if (gBurst.nextTriggerDelay > 0U)
    {
      --gBurst.nextTriggerDelay;
    }
    if (gBurst.nextTriggerDelay == 0U && gBurst.remaining > 0U)
    {
      geigerTriggerClick();
      --gBurst.remaining;
      if (gBurst.remaining == 0U)
      {
        gBurst.active = false;
      }
      else
      {
        gBurst.nextTriggerDelay = randomInRange(32U, 512U);
      }
    }
  }

  advancePatternSample();
  return mixSample();
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



void geiger_trigger_burst(uint8_t countMin, uint8_t countMax)
{
  asap::audio::geigerTriggerBurst(countMin, countMax);
}

void beep_start(uint16_t freqHz, uint16_t durationMs, uint8_t level0_255)
{
  asap::audio::beepStart(freqHz, durationMs, level0_255);
}

int16_t beep_get_sample()
{
  return asap::audio::beepGetSample();
}

void beep_pattern_start(uint8_t patternId)
{
  asap::audio::BeepPattern pattern = asap::audio::BeepPattern::Single;

  switch (patternId)
  {
    case static_cast<uint8_t>(asap::audio::BeepPattern::Single):
      pattern = asap::audio::BeepPattern::Single;
      break;
    case static_cast<uint8_t>(asap::audio::BeepPattern::Double):
      pattern = asap::audio::BeepPattern::Double;
      break;
    case static_cast<uint8_t>(asap::audio::BeepPattern::Error):
      pattern = asap::audio::BeepPattern::Error;
      break;
    case static_cast<uint8_t>(asap::audio::BeepPattern::Alert):
      pattern = asap::audio::BeepPattern::Alert;
      break;
    default:
      pattern = asap::audio::BeepPattern::Single;
      break;
  }

  asap::audio::beepPatternStart(pattern);
}

}
