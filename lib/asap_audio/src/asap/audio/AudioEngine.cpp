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
};

GeigerState gGeiger{
    /*lfsr=*/0xACE1u,
    /*position=*/0U,
    /*active=*/false,
};

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

  if (gGeiger.position >= kGeigerClickLengthSamples)
  {
    gGeiger.active = false;
    return 0;
  }

  // Linear decay of the envelope from kGeigerMaxEnvelope to 0 over the
  // configured click length.
  const uint16_t remaining =
      static_cast<uint16_t>(kGeigerClickLengthSamples - gGeiger.position);
  const uint8_t envelope =
      static_cast<uint8_t>((static_cast<uint32_t>(kGeigerMaxEnvelope) *
                            remaining) /
                           kGeigerClickLengthSamples);

  ++gGeiger.position;

  const int16_t noise = generateNoiseSample();
  const int32_t scaled =
      (static_cast<int32_t>(noise) * static_cast<int32_t>(envelope)) /
      static_cast<int32_t>(kGeigerMaxEnvelope);
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

void geigerTriggerClick()
{
  gGeiger.position = 0U;
  gGeiger.active = true;
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

