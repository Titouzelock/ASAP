#pragma once

#include <stdint.h>

#include <asap/audio/AudioConfig.h>

namespace asap::audio
{

// Initializes all internal audio engine state (Geiger + beeps).
// Call this once on startup and again before running any offline snapshots.
void init();

// Returns the next mixed audio sample at kSampleRateHz.
// This is the main per-sample entry point used by both MCU and native builds.
int16_t getSample();

// Geiger ---------------------------------------------------------------------

// Triggers a single Geiger click immediately. The click uses:
//  - A recorded 64-sample attack (from kGeigerAttack64),
//  - A synthetic damped 440 Hz tail shaped by a 16-bit envelope.
// The current implementation is monophonic: starting a new click clears any
// previous click tail that may still be playing.
void geigerTriggerClick();

// Burst trigger: schedules a random number of clicks between [minCount,
// maxCount]. Clicks inside the burst are separated by small random delays in
// the range [kGeigerBurstMinDelaySamples, kGeigerBurstMaxDelaySamples] at the
// engine sample rate. Each new click also resets any previous tail (monophonic
// behavior), so only one tail is active at a time.
void geigerTriggerBurst(uint8_t minCount, uint8_t maxCount);

// Beeps (restored from previous engine) --------------------------------------

// Starts a single square-wave beep with the given frequency (Hz),
// duration (ms) and level (0-255). Level is mapped to a signed 16-bit
// amplitude. Beep generation is done using a 32-bit phase accumulator and
// is safe to call from a 16 kHz "audio ISR"-style loop.
void beep_start(uint16_t freq_hz, uint16_t duration_ms, uint8_t level_0_255);

// Stops any active beep and cancels any high-level pattern that is running.
void beep_stop_all();

// Starts a high-level beep pattern. The pattern_id is one of the values from
// the BeepPattern enum in AudioConfig.h, interpreted as:
//  - Single, Double, Error, Alert.
// Patterns are implemented as a small state machine that sequences calls to
// beep_start() and silent gaps.
void beep_pattern_start(uint8_t pattern_id);

}  // namespace asap::audio

// C wrappers (for C code / MCU main) -----------------------------------------
extern "C"
{

void asap_audio_init();

int16_t asap_audio_get_sample();

void geiger_trigger_click();

void geiger_trigger_burst(uint8_t minCount, uint8_t maxCount);

void beep_start(uint16_t freq_hz, uint16_t duration_ms, uint8_t level_0_255);

void beep_stop_all();

void beep_pattern_start(uint8_t pattern_id);

// Global volume control (MCU path only). The audio engine always renders
// at full internal amplitude; volume is applied when mapping to the PWM
// duty cycle in the TIM3 interrupt on the MCU.
//
// Volume range:
//  - 0   -> mute
//  - 100 -> full scale
void asap_audio_set_volume(uint8_t vol);
uint8_t asap_audio_get_volume();

}
