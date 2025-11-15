# ASAP Audio Engine

This document describes the rebuilt ASAP audio engine used both on the STM32 firmware and in the native snapshot tests.

## Sample Rates

- Core engine sample rate: `16 kHz` (MCU and native).
- Snapshot output sample rate: `48 kHz` mono, 16-bit PCM.
- Native snapshots upsample the 16 kHz engine output using zero-order hold �3,
  then pass the 48 kHz stream through a 2-pole RC low-pass filter before
  writing WAV files.

Signal chain for snapshots:

`engine (16 kHz) ? ZOH�3 (48 kHz) ? 2-pole RC (fc � 7.2 kHz) ? WAV`

## Geiger Engine

### High-level behavior

- Engine sample rate: 16 kHz.
- Each Geiger "click" is a **hybrid** of:
  - A **real 64-sample attack** (`4 ms @ 16 kHz`) taken from a recorded click.
  - A **synthetic tail**: damped 440 Hz tone with added jitter and noise.
- The current implementation is **monophonic**:
  - At any time, only one click tail is active.
  - Starting a new click (via single click or burst) clears any previous tail
    and restarts the click state in slot 0.

### Attack

- Data:
  - Stored as `int16_t kGeigerAttack64[64]`.
- Envelope:
  - A 16-bit fixed-point attack envelope LUT is generated on first use:
    - Starts at `kGeigerAttackInitialEnv` (65535).
    - Decays sample-by-sample by multiplying with
      `kGeigerAttackDecayFactor / 65536` (0.94).
  - Each attack sample is:
    - `wave = (attackSample * attackEnv) >> 16`.

### Tail

- Waveform:
  - 256-entry sine LUT (`kSinLut16`), full-scale `[-32767, 32767]`.
  - 16-bit phase accumulator, step set so that `440 Hz @ 16 kHz`.
  - Small per-sample random **frequency jitter**:
    - Jitter extracted from the LFSR via `kGeigerTailJitterMask` and
      `kGeigerTailJitterOffset` (2% of the nominal phase step).
  - Wideband **noise** mixed with the tone:
    - Noise value built from the LFSR using `kGeigerTailNoiseMask` and
      `kGeigerTailNoiseOffset`, scaled by the tail envelope.

- Envelope:
  - Tail envelope is 16-bit (`0..kGeigerTailMaxEnv`).
  - Initial value on click start:
    - `kGeigerTailInitialEnv` (typically 65535, configurable).
  - Per-sample decay:
    - `tailEnv = (tailEnv * kGeigerTailDecayFactor) >> 16;`
    - `kGeigerTailDecayFactor / 65536`  0.996 by default.
  - Tail life is limited by:
    - `kGeigerTailMaxSamples` (80 ms at 16 kHz).

### Bursts

- Burst state:
  - `active`: whether a burst is currently running.
  - `remaining`: how many clicks still need to fire in this burst.
  - `delay`: engine samples until next click.
- Trigger:
  - `geigerTriggerBurst(minCount, maxCount)`:
    - Clamps min/max, picks `remaining` in `[minCount, maxCount]`.
    - Sets initial `delay` to a random value in
      `[kGeigerBurstMinDelaySamples, kGeigerBurstMaxDelaySamples]`.
- Scheduling (in `getSample()` / mixer path):
  - On each sample at 16 kHz:
    - If `delay > 0`, decrement `delay`.
    - When `delay == 0` and `remaining > 0`:
      - Start a new click (monophonic: clears old tail and starts slot 0).
      - Decrement `remaining`.
      - If `remaining > 0`, choose the next `delay` in the same configured
        range `[kGeigerBurstMinDelaySamples, kGeigerBurstMaxDelaySamples]`.
- Typical spacing:
  - At default settings, intra-burst delays are between **2 ms and 12.5 ms**.

## Beep Engine

- The beep engine is restored from the original ASAP implementation:
  - Square-wave generator using a 32-bit phase accumulator.
  - Frequency, duration and level parameters:
    - `beep_start(freq_hz, duration_ms, level_0_255)`.
    - Level `[0, 255]` is mapped to a 16-bit signed amplitude.
  - Internal state is fully integer-based and allocation-free.
- High-level patterns:
  - Implemented as a small state machine that sequences `beep_start()` calls
    and silent gaps.
  - Patterns correspond to the `BeepPattern` enum:
    - Single, Double, Error, Alert.
  - See `lib/asap_audio/src/asap/audio/AudioEngine.cpp` for details.

## Mixer

- Sources:
  - Geiger channel: output of the Geiger engine (single click/tail at a time).
  - Beep channel: current beep or active pattern.
- Mix:
  - Mixed in a 32-bit accumulator: `geiger + beep`.
  - Hard-clipped to signed 16-bit:
    - Values are clamped to `[kMinSampleValue, kMaxSampleValue]`.
  - Exposed as:
    - C++: `asap::audio::getSample()`.
    - C: `asap_audio_get_sample()`.

## Native Snapshot Generation

The native test suite in `test/test_audio/test_audio.cpp` produces several
2.0 s WAV snapshots at 48 kHz:

- `geiger_single_click.wav`
  - A single Geiger click triggered at t = 0.
- `geiger_burst_3.wav`
  - A burst of exactly 3 clicks.
- `geiger_burst_5.wav`
  - A burst of exactly 5 clicks.
- `beep_single.wav`
  - A single beep (e.g., 1 kHz, 200 ms, full level).
- `beep_geiger_mix.wav`
  - A mixed scenario with a beep and a Geiger burst overlapping in time.
- `geiger_burst_randomized.wav`
  - Several bursts (for example, three bursts of 5 clicks) rendered into a
    single file to visualize the random intra-burst spacing.

Each snapshot uses the same pipeline:

1. Initialize the audio engine state.
2. Trigger the appropriate Geiger and/or beep events.
3. For each 48 kHz output sample:
   - Fetch a new 16 kHz engine sample every 3rd tick using
     `asap_audio_get_sample()`; hold the value in between (ZOH�3).
   - Normalize to `[-1, 1]` and run through the 2-pole RC filter
     (fc � 7.2 kHz).
   - Convert back to 16-bit PCM and write to the WAV buffer.

This snapshot flow acts as the audio UI contract across firmware and native
builds: when the engine or parameters change, updated WAVs can be diffed or
listened to for regression checks.
