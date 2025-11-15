#pragma once

#include <stdint.h>
#include <asap/audio/AudioConfig.h>

namespace asap::audio
{

enum class GeigerClickType : uint8_t
{
  Short = 0,
  ResonantStalker,
  ResonantMetallic,
  ResonantSciFi,
  ResonantBio,
};

enum class GeigerAttackShape : uint8_t
{
  Default = 0,   // 200/256 decay, no overshoot
  Snappier1,     // 180/256 decay
  Snappier2,     // 160/256 decay + overshoot
};

// Initializes internal engine state. Should be called once on startup or
// before starting snapshot renders in the native tests.
void init();

// Geiger click API -----------------------------------------------------------

// Arms a single geiger click. The click is synthesized over the next
// kGeigerClickLengthSamples samples returned by getSample().
void geigerTriggerClick();
void geigerTriggerClickResonantStalker();
void geigerTriggerClickResonantMetallic();
void geigerTriggerClickResonantSciFi();
void geigerTriggerClickResonantBio();
void geigerTriggerClickResonantStalkerSnappyMid();
void geigerTriggerClickResonantMetallicSnappyMid();
void geigerTriggerClickResonantSciFiSnappyMid();
void geigerTriggerClickResonantBioSnappyMid();
void geigerTriggerClickResonantStalkerSnappyStrong();
void geigerTriggerClickResonantMetallicSnappyStrong();
void geigerTriggerClickResonantSciFiSnappyStrong();
void geigerTriggerClickResonantBioSnappyStrong();
void geigerTriggerBurst(uint8_t countMin, uint8_t countMax);

// Low-level access to the geiger channel only; normally the mixer should be
// used via getSample().
int16_t geigerGetSample();

// Beep engine API ------------------------------------------------------------

// Starts a single beep with the given frequency, duration and level. The level
// is specified in the range [0, 255] and is mapped to a signed 16-bit output.
void beepStart(uint16_t freqHz, uint16_t durationMs, uint8_t level0_255);

// Returns the current beep sample (or 0 when idle).
int16_t beepGetSample();

// High-level non-blocking pattern controller. Patterns are implemented as a
// small state machine that sequences beepStart() and silent gaps.
void beepPatternStart(BeepPattern pattern);

// Mixer / top-level API ------------------------------------------------------

// Returns the next mixed audio sample at kSampleRateHz. This is the main MCU
// entry point for the audio subsystem.
int16_t getSample();

}  // namespace asap::audio

// C-compatible wrapper APIs used by tests and potential external bindings.
extern "C"
{

// Initializes the internal engine state.
void asap_audio_init();

// Returns the next mixed audio sample (8 kHz).
int16_t asap_audio_get_sample();

// Geiger control.
void geiger_trigger_click();
int16_t geiger_get_sample();
void geiger_trigger_click_resonant_stalker();
void geiger_trigger_click_resonant_metallic();
void geiger_trigger_click_resonant_scifi();
void geiger_trigger_click_resonant_bio();
void geiger_trigger_click_resonant_stalker_snappy_mid();
void geiger_trigger_click_resonant_metallic_snappy_mid();
void geiger_trigger_click_resonant_scifi_snappy_mid();
void geiger_trigger_click_resonant_bio_snappy_mid();
void geiger_trigger_click_resonant_stalker_snappy_strong();
void geiger_trigger_click_resonant_metallic_snappy_strong();
void geiger_trigger_click_resonant_scifi_snappy_strong();
void geiger_trigger_click_resonant_bio_snappy_strong();
void geiger_trigger_burst(uint8_t countMin, uint8_t countMax);

// Beep control.
void beep_start(uint16_t freqHz, uint16_t durationMs, uint8_t level0_255);
int16_t beep_get_sample();

// Pattern selection: the patternId follows asap::audio::BeepPattern values.
void beep_pattern_start(uint8_t patternId);

}
