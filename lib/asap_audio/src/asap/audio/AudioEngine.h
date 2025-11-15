#pragma once

#include <stdint.h>
#include <asap/audio/AudioConfig.h>

namespace asap::audio
{

// Initializes internal engine state. Should be called once on startup or
// before starting snapshot renders in the native tests.
void init();

// Geiger click API -----------------------------------------------------------

// Arms a single geiger click. The click is synthesized over the next
// kGeigerClickLengthSamples samples returned by getSample().
void geigerTriggerClick();

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

// Beep control.
void beep_start(uint16_t freqHz, uint16_t durationMs, uint8_t level0_255);
int16_t beep_get_sample();

// Pattern selection: the patternId follows asap::audio::BeepPattern values.
void beep_pattern_start(uint8_t patternId);

}

