# ASAP Audio Engine

This document describes the rebuilt ASAP audio engine used both on the STM32 firmware and in the native snapshot tests.

## Sample Rates

- Core engine sample rate: `16 kHz` (MCU and native).
- Snapshot output sample rate: `48 kHz` mono, 16‑bit PCM.
- Native snapshots upsample the 16 kHz engine output using zero‑order hold ×3, then pass the 48 kHz stream through a 2‑pole RC low‑pass filter before writing WAV files.

Signal chain for snapshots:

`engine (16 kHz) → ZOH×3 (48 kHz) → 2‑pole RC (fc ≈ 7.2 kHz) → WAV`

## Geiger Engine

- Core model:
  - 16 kHz engine with a hybrid click composed of:
    - A **real 64‑sample attack** (`4 ms @ 16 kHz`) taken from a recorded Geiger click.
    - A **synthetic tail**: damped 440 Hz tone with an exponential envelope.
  - Multiple overlapping clicks are supported (up to 4 concurrent `GeigerClick` instances).

- Attack:
  - Stored as `int16_t kGeigerAttack64[64]`, replayed directly for the first 64 samples of a click.
  - No extra envelope is applied during the attack; the recorded data encodes the transient.

- Tail:
  - Generated using a 256‑entry `int16` sine LUT, full‑scale in `[-32767, 32767]`.
  - Tail frequency: ≈`440 Hz` at `16 kHz` using a fixed phase step in a 16‑bit accumulator.
  - Amplitude is shaped by an 8‑bit envelope (`0..255`) that decays multiplicatively with a fixed integer factor each sample (no floating point).
  - Tail life is limited by both:
    - Maximum tail length (`≈80 ms`), and
    - Envelope reaching zero.

- Bursts:
  - A `GeigerBurstState` schedules a small number of clicks:
    - Burst size: random count in `[minCount, maxCount]` (typically 3–5).
    - Inter‑click spacing: random delay in the range `2..32 ms` (computed in samples).
  - Each scheduled click picks a free slot in the `GeigerClick` pool; if all slots are busy, new clicks may be dropped.

## Beep Engine

- The beep engine is restored from the original ASAP implementation:
  - Square‑wave generator using a 32‑bit phase accumulator.
  - Frequency, duration and level parameters:
    - `beep_start(freq_hz, duration_ms, level_0_255)`.
    - Level maps `[0, 255]` to a 16‑bit amplitude range.
  - Internal state is fully integer‑based and allocation‑free.
- High‑level patterns are implemented as a small state machine that sequences `beep_start()` calls and silent gaps (e.g., single, double, error, alert). See `lib/asap_audio/src/asap/audio/AudioEngine.cpp` for exact details.

## Mixer

- The mixer combines:
  - Geiger channel: sum of all active `GeigerClick` instances.
  - Beep channel: current beep or pattern output.
- The mixed output:
  - Is computed as a 32‑bit accumulator and hard‑clipped to signed 16‑bit.
  - Is returned one sample at a time by `getSample()` and exposed to C via `asap_audio_get_sample()`.

## Native Snapshot Generation

The native test suite in `test/test_audio/test_audio.cpp` produces several 2.0 s WAV snapshots at 48 kHz:

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

Each snapshot uses the same pipeline:

1. Initialize the audio engine state.
2. Trigger the appropriate Geiger and/or beep events at t = 0.
3. For each 48 kHz output sample:
   - Fetch a new 16 kHz engine sample every 3rd tick using `asap_audio_get_sample()`; hold in between (ZOH ×3).
   - Normalize to `[-1, 1]` and run through the 2‑pole RC filter (fc ≈ 7.2 kHz).
   - Convert back to 16‑bit PCM and write to the WAV buffer.

This snapshot flow acts as the audio UI contract across firmware and native builds: when the engine or parameters change, updated WAVs can be diffed or listened to for regression checks.

