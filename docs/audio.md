# ASAP Audio Architecture

This document describes the MCU‑oriented audio path used by the ASAP detector
and the native test harness that simulates the analog RC filter and exports WAV
snapshots. The MCU code under `lib/asap_audio` is pure fixed‑point C++17 and is
shared across embedded and native builds; all heavy simulation and tooling live
exclusively in the native test environment.

## Signal Chain Overview

On the hardware side, the audio path is:

- MCU timer → PWM @ 100 kHz
- Simple RC low‑pass filter (first order)
- PAM8302 class‑D amplifier
- Speaker / transducer

The firmware generates raw audio samples at 8 kHz. These samples are then
translated into a high‑frequency PWM stream. The analog RC filter and PAM8302
convert this PWM into an audible analog voltage that drives the speaker.

### RC Filter Parameters

The RC filter is a first‑order low‑pass with the following values:

- `R = 5.1 kΩ`
- `C = 10 nF`
- Cutoff frequency: `fc ≈ 1 / (2πRC) ≈ 3.1 kHz`

The MCU produces **pre‑filter** audio: the synthesized signal is what the PWM
represents before the RC filter, not what appears at the speaker terminals.
The analog RC shapes this signal in hardware, reducing PWM carrier components
and smoothing edges.

## MCU Audio Engine (lib/asap_audio)

The MCU‑ready audio implementation lives under `lib/asap_audio/src/asap/audio`.
It is fixed‑point, allocation‑free and avoids heavy STL in the embedded path.
The public entry points are:

- `void asap_audio_init();`
- `int16_t asap_audio_get_sample();`
- `void geiger_trigger_click();`
- `int16_t geiger_get_sample();`
- `void beep_start(uint16_t freqHz, uint16_t durationMs, uint8_t level0_255);`
- `int16_t beep_get_sample();`
- `void beep_pattern_start(uint8_t patternId);`

The native build also includes C++ namespaced APIs in
`asap::audio::AudioEngine.h`, but embedded code can treat the engine as a small
`extern "C"` module.

### Sample Rate

- Internal audio sample rate: **8 kHz**, fixed (`asap::audio::kSampleRateHz`).
- Every call to `asap_audio_get_sample()` advances all internal generators and
  returns one signed 16‑bit sample.

### Geiger Click Synthesis

The geiger engine generates short broadband clicks using a 16‑bit LFSR and a
simple envelope:

- Trigger: `geiger_trigger_click()` arms a single click.
- Length: approximately **40 samples** (`kGeigerClickLengthSamples`), i.e.
  about 5 ms at 8 kHz.
- Noise source: a 16‑bit maximal‑length LFSR with taps at (16, 14, 13, 11),
  using the standard 0xB400 feedback polynomial.
- Envelope: linear decay from `kGeigerMaxEnvelope = 255` down to 0 over the
  click length.
- Output: signed 16‑bit sample derived from the LFSR value scaled by the
  envelope.

On each sample while a click is active:

1. The LFSR advances one step and produces a noise value.
2. A per‑sample envelope factor is computed from 255 → 0 across 40 samples.
3. The noise value is scaled by this envelope and returned as a 16‑bit sample.

When the envelope completes, the click automatically deactivates.

### Beep Synthesis (Phase Accumulator)

The beep engine produces tonal UI beeps using a simple phase‑accumulator
square‑wave:

- API: `beep_start(freqHz, durationMs, level0_255)` and `beep_get_sample()`.
- Frequency range: clamped to **200–3000 Hz** (`kBeepMinFreqHz` /
  `kBeepMaxFreqHz`).
- Level: 0–255 (`level0_255`), mapped to signed 16‑bit amplitude via a fixed
  scaling factor.

Internally:

- A 32‑bit phase accumulator is advanced each 8 kHz sample by:

  - `phaseStep = freqHz * 2^32 / sampleRate`.

- The top bit of the phase determines the square‑wave polarity.
- A per‑beep sample counter (`samplesRemaining`) is computed from
  `durationMs * sampleRate` and decremented each sample.

This yields a precise, deterministic, and allocation‑free tone generator that
is inexpensive on STM32F103‑class MCUs.

### Beep Patterns (State Machine)

High‑level beep patterns are implemented as a non‑blocking state machine on top
of the single‑voice beep engine. Patterns are made of small steps:

- Each step describes:
  - Whether it is a beep or a silent gap.
  - Frequency in Hz.
  - Duration in ms.
  - Level (0–255).

The engine exposes an enum and configuration constants in
`AudioConfig.h` (`asap::audio`):

- `enum class BeepPattern { Single, Double, Error, Alert };`
- `kBeepSingleFreqHz`, `kBeepSingleDurationMs`, `kBeepSingleLevel`
- `kBeepDoubleFreqHz`, `kBeepDoubleToneDurationMs`, `kBeepDoubleGapDurationMs`,
  `kBeepDoubleLevel`
- `kBeepErrorFreqHz`, `kBeepErrorDurationMs`, `kBeepErrorLevel`
- `kBeepAlertFreqHz`, `kBeepAlertDurationMs`, `kBeepAlertLevel`

These values can be tuned by humans without touching implementation code.

The patterns are:

- **Single**: one medium‑length mid‑frequency beep.
- **Double**: two mid‑frequency beeps with a short gap.
- **Error**: a longer, lower‑pitched tone.
- **Alert**: a short, higher‑pitched tone.

Patterns are started with `beepPatternStart(BeepPattern)` on the C++ side or
`beep_pattern_start(patternId)` on the C wrapper. Each call to
`asap_audio_get_sample()` advances the pattern state machine and, when needed,
invokes `beepStart()` to schedule beeps. All sequencing is non‑blocking; the
engine remains responsive to other events.

### Mixer Behavior

The mixer combines the geiger and beep streams into the final 16‑bit sample
returned by `asap_audio_get_sample()`:

- Geiger sample: `geigerGetSample()`.
- Beep sample: `beepGetSample()`.
- Geiger attenuation: when a beep is active, the geiger channel is multiplied
  by `kGeigerAttenuationWhileBeep / 256`. The current default is:

  - `kGeigerAttenuationWhileBeep = 102` → ~0.4×.

- Mix: the two signed 16‑bit streams are summed in 32‑bit, then hard‑clipped
  to the int16 range `[-32768, 32767]`.

This preserves the geiger texture while making UI beeps clearly audible.

## Native RC Simulation & WAV Snapshots (test/test_audio)

The native test environment implements a high‑fidelity approximation of the
analog path and exports WAV files for inspection and regression testing. All
simulation, RC filtering, and file I/O live under `test/test_audio/` and are
only built in the `native` environment.

### Digital RC Filter

The analog RC low‑pass is modelled as a discrete‑time first‑order filter:

- `y[n] = y[n-1] + α * (x[n] - y[n-1])`
- `α = (2π fc) / (2π fc + fs)`

For audio snapshots and Unity preview, we use:

- `fc = 3000 Hz` (matching the hardware RC).
- `fs = 48000 Hz` (Unity/native test audio rate).

The filter is implemented in fixed or floating point in the test harness and
fed with the MCU pre‑filter audio generated by `asap_audio_get_sample()`.

### Sample Rendering and Upsampling

The MCU engine runs at 8 kHz; Unity and WAV export use 48 kHz. The test
environment bridges these rates with a simple zero‑order hold upsampler:

- For each 48 kHz output sample, the harness decides whether to advance the
  8 kHz engine:
  - Every 6th output sample, it calls `asap_audio_get_sample()` to retrieve the
    next 8 kHz sample.
  - For the five intermediate samples, it holds the last 8 kHz value.

The resulting 48 kHz stream is then passed through the digital RC filter and
scaled to a float range `[-1.0, 1.0]` for convenience.

### WAV Writer Utility

The test harness includes a self‑contained WAV writer that emits mono,
16‑bit PCM or 32‑bit float WAV files:

- Sample rate: **48 kHz**.
- Format: mono, 16‑bit PCM.
- Header: standard RIFF/WAVE chunk layout, no external dependencies.
- Output directory: `snapshots/` at the repo root (shared with display PGM
  snapshots).

Each audio snapshot is about **2 seconds** long
(`asap::audio::kSnapshotDurationMs`), including some leading and trailing
silence. The test run creates the following files:

- `geiger_click.wav` — a single geiger click.
- `beep_single.wav` — a single pattern beep.
- `beep_double.wav` — the double‑beep pattern.
- `beep_error.wav` — the low, long error tone.
- `beep_alert.wav` — the short, high alert tone.

These files act as audio snapshots analogous to the PGM display snapshots; they
can be inspected manually or diffed across revisions.

### Automatic Snapshot Generation

A dedicated Unity test case in `test/test_audio` drives the audio engine and
WAV writer:

- It calls `asap_audio_init()` to reset the engine.
- It triggers appropriate geiger/beep/pattern events at the start of a
  2‑second window.
- It renders 2 seconds of 48 kHz filtered audio into an in‑memory buffer.
- It writes all five WAV files to `snapshots/` in one run.

The test is wired into the existing `native` Unity test harness so that
`pio test -e native` generates both display PGM and audio WAV snapshots.

## Unity Integration (OnAudioFilterRead)

For real‑time preview, the native environment also exposes an `extern "C"`
function suitable for calling from Unity via `DllImport`:

- `float asap_audio_render_sample();`

This function internally:

- Runs the same 8 kHz → 48 kHz upsampling scheme.
- Applies the digital RC filter with `fc = 3000 Hz` and `fs = 48000 Hz`.
- Returns a single filtered audio sample as a float in `[-1.0, 1.0]`.

A C# script (e.g. `AsapAudioFilter.cs`) can then hook into Unity’s
`OnAudioFilterRead(float[] data, int channels)` callback:

- For each sample in `data`, it calls `asap_audio_render_sample()`.
- It writes the returned value into the buffer for each channel.

With the native shared library built from this project, Unity can play a
near‑MCU‑faithful rendition of the detector’s audio (geiger clicks and beep
patterns) in real time while using the same fixed‑point engine as the STM32
firmware.

