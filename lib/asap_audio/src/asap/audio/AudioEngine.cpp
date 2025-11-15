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
  GeigerClickType type;
  GeigerAttackShape attackShape;
};

GeigerState gGeiger{
    /*lfsr=*/0xACE1u,
    /*position=*/0U,
    /*active=*/false,
    /*envelope=*/0U,
    /*type=*/GeigerClickType::Short,
    /*attackShape=*/GeigerAttackShape::Default,
};

static const int8_t spark8_stalker[8] = {
    0, 100, -80, 60, -40, 20, -10, 0,
};

static const int8_t spark8_metallic[8] = {
    0, 110, -90, 70, -55, 40, -20, 0,
};

static const int8_t spark8_scifi[8] = {
    0, 127, -95, 50, -20, 5, 0, 0,
};

static const int8_t spark8_bio[8] = {
    0, 70, -60, 45, -30, 15, -5, 0,
};

// Real-data 32-sample kernel derived from an analyzed Geiger waveform.
static const int8_t kGeigerClickKernel32[32] = {
    -97,  -57,   95,  127,   25,  -77,  -53,   46,
    119,  109,   11, -115, -104,   20,   73,   33,
    -52, -112,   -3,   77,   12,  -58,  -34,   -8,
     12,    9,  -30,  -19,   25,   37,   -6,   -5,
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

int16_t generateHFNoise()
{
  static int16_t previous = 0;
  const int16_t current = generateNoiseSample();
  const int16_t out = static_cast<int16_t>(
      static_cast<int32_t>(current) - static_cast<int32_t>(previous));
  previous = current;
  return out;
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

  ++gGeiger.position;

  // Two-stage envelope derived from measured Geiger data:
  // - Stage A (attack region): faster decay
  // - Stage B (tail region): slow decay
  uint16_t attackDecay = 241U;  // default ~0.94 per sample
  uint16_t tailDecay = 255U;    // default ~0.996 per sample

  switch (gGeiger.attackShape)
  {
    case GeigerAttackShape::Snappier1:
      attackDecay = 180U;
      tailDecay = 250U;
      break;
    case GeigerAttackShape::Snappier2:
      attackDecay = 160U;
      tailDecay = 245U;
      break;
    default:
      attackDecay = 241U;
      tailDecay = 255U;
      break;
  }

  const uint16_t decay =
      (gGeiger.position < kGeigerClickAttackSamples) ? attackDecay : tailDecay;

  gGeiger.envelope = static_cast<uint8_t>(
      (static_cast<uint16_t>(gGeiger.envelope) * decay) >> 8U);

  if (gGeiger.envelope == 0U)
  {
    gGeiger.active = false;
    return 0;
  }

  int32_t wave = 0;

  // Real-data kernel applied over the first 32 samples.
  if (gGeiger.position < 32U)
  {
    wave += static_cast<int16_t>(
        static_cast<int16_t>(kGeigerClickKernel32[gGeiger.position]) << 7);
  }

  const int8_t* kernel = nullptr;
  switch (gGeiger.type)
  {
    case GeigerClickType::ResonantStalker:
      kernel = spark8_stalker;
      break;
    case GeigerClickType::ResonantMetallic:
      kernel = spark8_metallic;
      break;
    case GeigerClickType::ResonantSciFi:
      kernel = spark8_scifi;
      break;
    case GeigerClickType::ResonantBio:
      kernel = spark8_bio;
      break;
    default:
      break;
  }

  if (kernel != nullptr && gGeiger.position < 8U)
  {
    wave += static_cast<int16_t>(static_cast<int32_t>(
        static_cast<int16_t>(kernel[gGeiger.position]) << 7));
  }

  int32_t scaled = (wave * static_cast<int32_t>(gGeiger.envelope)) >> 8;

  // Optional attack overshoot for the strongest snappy variants: slightly
  // boost the very first samples to create a sharper transient.
  if (gGeiger.attackShape == GeigerAttackShape::Snappier2)
  {
    if (gGeiger.position == 0U)
    {
      scaled = (scaled * 140) / 100;  // +40%
    }
    else if (gGeiger.position == 1U)
    {
      scaled = (scaled * 120) / 100;  // +20%
    }
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
  gGeiger.type = GeigerClickType::Short;
  gGeiger.attackShape = GeigerAttackShape::Default;

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
  gGeiger.type = GeigerClickType::Short;
  gGeiger.attackShape = GeigerAttackShape::Default;
}

void geigerTriggerClickResonantStalker()
{
  gGeiger.position = 0U;
  gGeiger.active = true;
  gGeiger.envelope = kGeigerMaxEnvelope;
  gGeiger.type = GeigerClickType::ResonantStalker;
  gGeiger.attackShape = GeigerAttackShape::Default;
}

void geigerTriggerClickResonantMetallic()
{
  gGeiger.position = 0U;
  gGeiger.active = true;
  gGeiger.envelope = kGeigerMaxEnvelope;
  gGeiger.type = GeigerClickType::ResonantMetallic;
  gGeiger.attackShape = GeigerAttackShape::Default;
}

void geigerTriggerClickResonantSciFi()
{
  gGeiger.position = 0U;
  gGeiger.active = true;
  gGeiger.envelope = kGeigerMaxEnvelope;
  gGeiger.type = GeigerClickType::ResonantSciFi;
  gGeiger.attackShape = GeigerAttackShape::Default;
}

void geigerTriggerClickResonantBio()
{
  gGeiger.position = 0U;
  gGeiger.active = true;
  gGeiger.envelope = kGeigerMaxEnvelope;
  gGeiger.type = GeigerClickType::ResonantBio;
  gGeiger.attackShape = GeigerAttackShape::Default;
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

void geigerTriggerClickResonantStalkerSnappyMid()
{
  gGeiger.position = 0U;
  gGeiger.active = true;
  gGeiger.envelope = kGeigerMaxEnvelope;
  gGeiger.type = GeigerClickType::ResonantStalker;
  gGeiger.attackShape = GeigerAttackShape::Snappier1;
}

void geigerTriggerClickResonantMetallicSnappyMid()
{
  gGeiger.position = 0U;
  gGeiger.active = true;
  gGeiger.envelope = kGeigerMaxEnvelope;
  gGeiger.type = GeigerClickType::ResonantMetallic;
  gGeiger.attackShape = GeigerAttackShape::Snappier1;
}

void geigerTriggerClickResonantSciFiSnappyMid()
{
  gGeiger.position = 0U;
  gGeiger.active = true;
  gGeiger.envelope = kGeigerMaxEnvelope;
  gGeiger.type = GeigerClickType::ResonantSciFi;
  gGeiger.attackShape = GeigerAttackShape::Snappier1;
}

void geigerTriggerClickResonantBioSnappyMid()
{
  gGeiger.position = 0U;
  gGeiger.active = true;
  gGeiger.envelope = kGeigerMaxEnvelope;
  gGeiger.type = GeigerClickType::ResonantBio;
  gGeiger.attackShape = GeigerAttackShape::Snappier1;
}

void geigerTriggerClickResonantStalkerSnappyStrong()
{
  gGeiger.position = 0U;
  gGeiger.active = true;
  gGeiger.envelope = kGeigerMaxEnvelope;
  gGeiger.type = GeigerClickType::ResonantStalker;
  gGeiger.attackShape = GeigerAttackShape::Snappier2;
}

void geigerTriggerClickResonantMetallicSnappyStrong()
{
  gGeiger.position = 0U;
  gGeiger.active = true;
  gGeiger.envelope = kGeigerMaxEnvelope;
  gGeiger.type = GeigerClickType::ResonantMetallic;
  gGeiger.attackShape = GeigerAttackShape::Snappier2;
}

void geigerTriggerClickResonantSciFiSnappyStrong()
{
  gGeiger.position = 0U;
  gGeiger.active = true;
  gGeiger.envelope = kGeigerMaxEnvelope;
  gGeiger.type = GeigerClickType::ResonantSciFi;
  gGeiger.attackShape = GeigerAttackShape::Snappier2;
}

void geigerTriggerClickResonantBioSnappyStrong()
{
  gGeiger.position = 0U;
  gGeiger.active = true;
  gGeiger.envelope = kGeigerMaxEnvelope;
  gGeiger.type = GeigerClickType::ResonantBio;
  gGeiger.attackShape = GeigerAttackShape::Snappier2;
}

int16_t geigerGetSample()
{
  return geigerSampleInternal();
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
    if (gBurst.nextTriggerDelay > 0U)
    {
      --gBurst.nextTriggerDelay;
    }
    if (gBurst.nextTriggerDelay == 0U && gBurst.remaining > 0U)
    {
      // For bursts, use the Stalker resonant click as the default voice.
      geigerTriggerClickResonantStalker();
      --gBurst.remaining;
      if (gBurst.remaining == 0U)
      {
        gBurst.active = false;
      }
      else
      {
        gBurst.nextTriggerDelay = randomInRange(64U, 960U);
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

int16_t geiger_get_sample()
{
  return asap::audio::geigerGetSample();
}

void geiger_trigger_click_resonant_stalker()
{
  asap::audio::geigerTriggerClickResonantStalker();
}

void geiger_trigger_click_resonant_metallic()
{
  asap::audio::geigerTriggerClickResonantMetallic();
}

void geiger_trigger_click_resonant_scifi()
{
  asap::audio::geigerTriggerClickResonantSciFi();
}

void geiger_trigger_click_resonant_bio()
{
  asap::audio::geigerTriggerClickResonantBio();
}

void geiger_trigger_click_resonant_stalker_snappy_mid()
{
  asap::audio::geigerTriggerClickResonantStalkerSnappyMid();
}

void geiger_trigger_click_resonant_metallic_snappy_mid()
{
  asap::audio::geigerTriggerClickResonantMetallicSnappyMid();
}

void geiger_trigger_click_resonant_scifi_snappy_mid()
{
  asap::audio::geigerTriggerClickResonantSciFiSnappyMid();
}

void geiger_trigger_click_resonant_bio_snappy_mid()
{
  asap::audio::geigerTriggerClickResonantBioSnappyMid();
}

void geiger_trigger_click_resonant_stalker_snappy_strong()
{
  asap::audio::geigerTriggerClickResonantStalkerSnappyStrong();
}

void geiger_trigger_click_resonant_metallic_snappy_strong()
{
  asap::audio::geigerTriggerClickResonantMetallicSnappyStrong();
}

void geiger_trigger_click_resonant_scifi_snappy_strong()
{
  asap::audio::geigerTriggerClickResonantSciFiSnappyStrong();
}

void geiger_trigger_click_resonant_bio_snappy_strong()
{
  asap::audio::geigerTriggerClickResonantBioSnappyStrong();
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
