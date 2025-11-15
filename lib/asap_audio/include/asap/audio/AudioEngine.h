#pragma once

#include <stdint.h>

#include <asap/audio/AudioConfig.h>

namespace asap::audio
{

// Initializes internal engine state. Should be called once on startup or
// before starting snapshot renders in the native tests.
void init();

// Main per-sample entry point (used by MCU + native)
int16_t getSample();

// Geiger ---------------------------------------------------------------------

// Single click trigger.
void geigerTriggerClick();

// Burst trigger: schedules between [minCount, maxCount] clicks separated
// by small random gaps.
void geigerTriggerBurst(uint8_t minCount, uint8_t maxCount);

// Beeps (restored from previous engine) --------------------------------------

// Starts a single beep with the given frequency, duration and level.
// Level is specified in the range [0, 255].
void beep_start(uint16_t freq_hz, uint16_t duration_ms, uint8_t level_0_255);

// Stops any active beep and pattern.
void beep_stop_all();

// Starts a high-level beep pattern (IDs follow BeepPattern values).
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

}

