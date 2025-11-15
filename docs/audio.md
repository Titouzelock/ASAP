# ASAP Audio Architecture

This document describes the MCU-oriented audio path used by the ASAP detector
and the native test harness that simulates the analog RC filter and exports WAV
snapshots. The MCU code under `lib/asap_audio` is pure fixed-point C++17 and is
shared across embedded and native builds; all heavy simulation and tooling live
exclusively in the native test environment.

## Signal Chain Overview

On the hardware side, the audio path is:

- MCU timer → PWM @ 100 kHz
- Simple RC low-pass filter (first order)
- PAM8302 class-D amplifier
- Speaker / transducer

The firmware generates raw audio samples at 8 kHz. These samples are then
translated into a high-frequency PWM stream. The analog RC filter and PAM8302
convert this PWM into an audible analog voltage that drives the speaker.

### RC Filter Parameters

The RC filter is a first-order low-pass with the following values:

- `R = 5.1 kΩ`
- `C = 10 nF`
- Cutoff frequency: `fc ≈ 1 / (2πRC) ≈ 3.1 kHz`

The MCU produces **pre-filter** audio: the synthesized signal is what the PWM
represents before the RC filter, not what appears at the speaker terminals.
The analog RC shapes this signal in hardware, reducing PWM carrier components
and smoothing edges.

## MCU Audio Engine (lib/asap_audio)

The MCU-ready audio implementation lives under `lib/asap_audio/src/asap/audio`.
It is fixed-point, allocation-free and avoids heavy STL in the embedded path.
The public entry points are:

- `void asap_audio_init();`
- `int16_t asap_audio_get_sample();`
- `void geiger_trigger_click();`
- `void geiger_trigger_click_resonant_stalker();`
- `void geiger_trigger_click_resonant_metallic();`
- `void geiger_trigger_click_resonant_scifi();`
- `void geiger_trigger_click_resonant_bio();`
- `int16_t geiger_get_sample();`
- `void beep_start(uint16_t freqHz, uint16_t durationMs, uint8_t level0_255);`
- `int16_t beep_get_sample();`
- `void beep_pattern_start(uint8_t patternId);`

The native build also includes C++ namespaced APIs in
`asap::audio::AudioEngine.h`, but embedded code can treat the engine as a small
`extern "C"` module.

### Sample Rate

- Internal audio sample rate: **16 kHz**, fixed (`asap::audio::kSampleRateHz`).
- Every call to `asap_audio_get_sample()` advances all internal generators and
  returns one signed 16-bit sample.

### Geiger Click Synthesis

The geiger engine generates short broadband clicks using a 16-bit LFSR, a
high-frequency (HF) emphasis stage, and an exponential envelope. Multiple
click “flavors” are supported to approximate different Geiger aesthetics.

- Trigger: `geiger_trigger_click()` and its resonant variants arm a single
  click.
- Length: approximately **40 samples** (`kGeigerClickLengthSamples`), i.e.
  about 5 ms at 8 kHz.
- Noise source: a 16-bit maximal-length LFSR with taps at (16, 14, 13, 11),
  using the standard 0xB400 feedback polynomial.
- HF emphasis: a simple differentiator (`out = current - previous`) sharpens
  transients and suppresses low-frequency content.
- Envelope: exponential decay from `kGeigerMaxEnvelope = 255` toward 0 using
  an 8-bit fixed-point multiplier.

On each sample while a click is active:

1. The LFSR advances one step and produces a noise value.
2. The HF stage computes `hf = noise[n] - noise[n-1]` to emphasize high
   frequencies.
3. The envelope is updated with an exponential decay:
   - `envelope = (envelope * 200) >> 8` (~22% decay per sample at 8 kHz).
4. The HF noise is scaled by the envelope:
   - `scaled = (hf * envelope) >> 8`.
5. Optional resonant kernels are added for certain click types (see below).

When the envelope reaches zero or the maximum click length is exceeded, the
click automatically deactivates.

#### Geiger Click Types

The engine supports multiple click shapes via an enum
`asap::audio::GeigerClickType`:

- `Short` — baseline broadband click (HF noise + exponential envelope).
- `ResonantStalker` — slightly underdamped, “Stalker-style” tick.
- `ResonantMetallic` — brighter, more metallic resonance.
- `ResonantSciFi` — sharper, synthetic-style spark.
- `ResonantBio` — softer, more organic-sounding tick.

Each resonant type uses a tiny 8-sample “spark” kernel that is added on top of
the HF noise during the first few samples of the click. Kernels are small
`int8_t` arrays (8 bytes each), making them extremely MCU-friendly.

Example kernels:

- Stalker:  `{ 0, 100, -80, 60, -40, 20, -10, 0 }`
- Metallic: `{ 0, 110, -90, 70, -55, 40, -20, 0 }`
- SciFi:    `{ 0, 127, -95, 50, -20,  5,   0, 0 }`
- Bio:      `{ 0,  70, -60, 45, -30, 15,  -5, 0 }`

For resonant clicks, the kernel contribution is injected as:

- `scaled += spark8[type][position] << 7` (integer only, no floats).

This yields a short, shaped impulse response that blends with the HF noise and
decaying envelope, giving each click a distinct spectral character.

Usage on the MCU side:

- `geiger_trigger_click()` — baseline short click.
- `geiger_trigger_click_resonant_stalker()` — Stalker-style resonant click.
- `geiger_trigger_click_resonant_metallic()` — brighter metallic click.
- `geiger_trigger_click_resonant_scifi()` — synthetic spark-style click.
- `geiger_trigger_click_resonant_bio()` — softer, organic tick.

These functions are cheap to call and simply update state inside the geiger
engine; the actual audio work remains in the 8 kHz sample path.

#### 16 kHz Audio Engine Upgrade

The audio engine now runs at **16 kHz** rather than 8 kHz. This raises the
effective Nyquist frequency to 8 kHz, which better captures the “metallic”
resonance and spark content observed in real Geiger recordings while still
remaining light enough for the STM32F103.

- Timer configuration (detector):
  - TIM2: still drives PWM @ 100 kHz for PA0.
  - TIM3: now configured for 16 kHz update interrupts (e.g. PSC = 89, ARR = 49
    for a 72 MHz clock).
- Native snapshots:
  - Use zero-order hold from 16 kHz → 48 kHz with a factor of 3.
  - RC simulation remains at `fs = 48000 Hz` and `fc ≈ 7.2 kHz` per pole.

This keeps the test environment and MCU engine aligned while giving enough
bandwidth for bright, resonant clicks.

#### Real Click Model

In addition to procedural noise, the geiger engine includes a 32-sample kernel
captured from an analyzed Geiger waveform:

```cpp
static const int8_t kGeigerClickKernel32[32] = {
  -97,  -57,   95,  127,   25,  -77,  -53,   46,
  119,  109,   11, -115, -104,   20,   73,   33,
  -52, -112,   -3,   77,   12,  -58,  -34,   -8,
   12,    9,  -30,  -19,   25,   37,   -6,   -5
};
```

- Sampled at 16 kHz.
- Covers the first ~2 ms of the Geiger click.
- Centered around zero and scaled in fixed-point (shifted and then envelope
  scaled).

The engine applies this kernel over the first 32 samples of each click, then
relies on the envelope to drive the longer tail.

#### Two-Stage Envelope

The click envelope is split into a fast attack region and a long, slow tail
derived from real data:

- Initial envelope: `env = 255`.
- Attack region (`position < 64` samples, ~4 ms at 16 kHz):
  - Default decay: `env = (env * 241) >> 8` (~0.94 per sample).
- Tail region (`position >= 64` samples):
  - Default decay: `env = (env * 255) >> 8` (~0.995 per sample).
- Click lifetime:
  - `kGeigerClickTotalSamples = 1280` (≈80 ms at 16 kHz).
  - The click ends when `env` reaches 0 or `position` exceeds this length.

Attack shaping modes (`GeigerAttackShape`) adjust the decay factors to obtain
brighter or more percussive attacks while preserving this two-stage structure.

#### Burst Scheduler

To approximate the characteristic “tingy burst” patterns heard in many Geiger
recordings and game soundscapes, the engine includes a burst scheduler:

- `geiger_trigger_burst(count_min, count_max)`:
  - Picks a random number of clicks between `count_min` and `count_max`.
  - Schedules them with random gaps between ~4–60 ms.
- Internally:
  - A `BurstState` tracks whether a burst is active, how many clicks remain,
    and the sample countdown until the next trigger.
  - Each time `asap_audio_get_sample()` is called, the burst state is updated
    and a new resonant Stalker-style click is fired when the delay reaches 0.
  - Gaps are drawn using a lightweight 16-bit LFSR-based RNG.

The result is sequences of 3–5 short, resonant clicks with realistic spacing,
which are exported as dedicated snapshot WAVs for inspection and tuning.

#### Geiger Attack Shaping Modes

To further tune the character of resonant clicks, the engine supports several
attack shaping modes via `GeigerAttackShape`:

- `Default` — original behavior:
  - Envelope decay factor: `200/256` (~22% loss per sample).
  - No explicit overshoot; click length uses `kGeigerClickLengthSamples`
    (≈40 samples, ~5 ms).
- `Snappier1` — stronger decay:
  - Envelope decay factor: `180/256` (~30% loss per sample).
  - Same nominal click length as default.
- `Snappier2` — strongest decay + attack overshoot:
  - Envelope decay factor: `160/256` (~37% loss per sample).
  - Shorter effective click length:
    - Uses `kGeigerClickLengthSnappySamples` (currently 32 samples) instead of
      40.
  - Attack overshoot on the first two samples:
    - At `position == 0`: `scaled = scaled * 1.4` (+40%).
    - At `position == 1`: `scaled = scaled * 1.2` (+20%).

These shaping modes make resonant clicks brighter and more percussive, closer
to STALKER-style “ting” sounds while retaining deterministic, fixed-point
implementation.

Convenience APIs for resonant clicks:

- Default envelope:
  - `geiger_trigger_click_resonant_stalker()`
  - `geiger_trigger_click_resonant_metallic()`
  - `geiger_trigger_click_resonant_scifi()`
  - `geiger_trigger_click_resonant_bio()`
- Medium snappy (`Snappier1`):
  - `geiger_trigger_click_resonant_stalker_snappy_mid()`
  - `geiger_trigger_click_resonant_metallic_snappy_mid()`
  - `geiger_trigger_click_resonant_scifi_snappy_mid()`
  - `geiger_trigger_click_resonant_bio_snappy_mid()`
- Strong snappy (`Snappier2`):
  - `geiger_trigger_click_resonant_stalker_snappy_strong()`
  - `geiger_trigger_click_resonant_metallic_snappy_strong()`
  - `geiger_trigger_click_resonant_scifi_snappy_strong()`
  - `geiger_trigger_click_resonant_bio_snappy_strong()`

In the native test environment, snapshots are generated for all three attack
shapes so differences can be auditioned and compared visually in spectral
tools.

### Beep Synthesis (Phase Accumulator)

The beep engine produces tonal UI beeps using a simple phase-accumulator
square-wave:

- API: `beep_start(freqHz, durationMs, level0_255)` and `beep_get_sample()`.
- Frequency range: clamped to **200–3000 Hz** (`kBeepMinFreqHz` /
  `kBeepMaxFreqHz`).
- Level: 0–255 (`level0_255`), mapped to signed 16-bit amplitude via a fixed
  scaling factor.

Internally:

- A 32-bit phase accumulator is advanced each 8 kHz sample by:

  - `phaseStep = freqHz * 2^32 / sampleRate`.

- The top bit of the phase determines the square-wave polarity.
- A per-beep sample counter (`samplesRemaining`) is computed from
  `durationMs * sampleRate` and decremented each sample.

This yields a precise, deterministic, and allocation-free tone generator that
is inexpensive on STM32F103-class MCUs.

### Beep Patterns (State Machine)

High-level beep patterns are implemented as a non-blocking state machine on top
of the single-voice beep engine. Patterns are made of small steps:

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

- **Single**: one medium-length mid-frequency beep.
- **Double**: two mid-frequency beeps with a short gap.
- **Error**: a longer, lower-pitched tone.
- **Alert**: a short, higher-pitched tone.

Patterns are started with `beepPatternStart(BeepPattern)` on the C++ side or
`beep_pattern_start(patternId)` on the C wrapper. Each call to
`asap_audio_get_sample()` advances the pattern state machine and, when needed,
invokes `beepStart()` to schedule beeps. All sequencing is non-blocking; the
engine remains responsive to other events.

### Mixer Behavior

The mixer combines the geiger and beep streams into the final 16-bit sample
returned by `asap_audio_get_sample()`:

- Geiger sample: `geigerGetSample()`.
- Beep sample: `beepGetSample()`.
- Geiger attenuation: when a beep is active, the geiger channel is multiplied
  by `kGeigerAttenuationWhileBeep / 256`. The current default is:

  - `kGeigerAttenuationWhileBeep = 102` → ~0.4×.

- Mix: the two signed 16-bit streams are summed in 32-bit, then hard-clipped
  to the int16 range `[-32768, 32767]`.

This preserves the geiger texture while making UI beeps clearly audible.

## Native RC Simulation & WAV Snapshots (test/test_audio)

The native test environment implements a high-fidelity approximation of the
analog path and exports WAV files for inspection and regression testing. All
simulation, RC filtering, and file I/O live under `test/test_audio/` and are
only built in the `native` environment.

### Digital RC Filters

The analog RC low-pass is modelled as a discrete-time first-order filter:

- `y[n] = y[n-1] + α * (x[n] - y[n-1])`
- `α = (2π fc) / (2π fc + fs)`

Historically, the native test harness used a single digital RC pole with:

- `fc = 3000 Hz` (matching the original hardware RC).
- `fs = 48000 Hz` (Unity/native test audio rate).

This 1-pole filter is still kept in the code as a reference, but the snapshots
and Unity preview now use a 2-pole RC tuned for higher cutoff to preserve
resonant detail.

#### Two-Pole RC for Resonant Clicks

To better preserve the high-frequency “metallic” content of Geiger clicks, the
test environment uses a 2-pole RC with a higher cutoff:

- Goal:
  - Preserve resonant components in the 6–9 kHz region (spark kernels).
  - Reduce over-damping of “metallic” Geiger spectra.
  - Still provide strong attenuation for the 100 kHz PWM carrier when mapped
    onto the hardware PWM → RC → PAM8302 chain.

Analog reference:

- Each pole emulates `R = 2.2 kΩ`, `C = 10 nF`.
- Analog cutoff per pole:
  - `fc ≈ 1 / (2π * 2.2 kΩ * 10 nF) ≈ 7.2 kHz`.
- Two cascaded poles form an approximate 2nd-order filter with
  `-40 dB/dec` rolloff.

Digital implementation (per pole):

- First-order IIR:
  - `y[n] = y[n-1] + α * (x[n] - y[n-1])`
  - `α = (2π fc) / (2π fc + fs)`
  - For the tests:
    - `fc = 7200 Hz`
    - `fs = 48000 Hz`

Two poles are applied in sequence at 48 kHz:

- `y1[n] = pole1(x[n])`
- `y2[n] = pole2(y1[n])`
- Snapshot / Unity output is `y2[n]`.

This 2-pole RC preserves the “spark” content of the resonant Geiger kernels
while still smoothing the PWM-mapped waveform enough to resemble the analog
output after the detector’s RC + PAM8302 stages.

### Sample Rendering and Upsampling

The MCU engine runs at 16 kHz; Unity and WAV export use 48 kHz. The test
environment bridges these rates with a simple zero-order hold upsampler:

- For each 48 kHz output sample, the harness decides whether to advance the
  16 kHz engine:
  - Every 3rd output sample, it calls `asap_audio_get_sample()` to retrieve the
    next 16 kHz sample.
  - For the two intermediate samples, it holds the last 16 kHz value.

The resulting 48 kHz stream is then passed through the digital 2-pole RC filter and
scaled to a float range `[-1.0, 1.0]` for convenience.

### WAV Writer Utility

The test harness includes a self-contained WAV writer that emits mono,
16-bit PCM WAV files:

- Sample rate: **48 kHz**.
- Format: mono, 16-bit PCM.
- Header: standard RIFF/WAVE chunk layout, no external dependencies.
- Output directory: `snapshots/` at the repo root (shared with display PGM
  snapshots).

Each audio snapshot is about **2 seconds** long
(`asap::audio::kSnapshotDurationMs`), including some leading and trailing
silence. The test run creates the following files:

- `geiger_click.wav` — a single baseline geiger click.
- `geiger_resonant_stalker_default.wav` — Stalker-style click, default attack.
- `geiger_resonant_stalker_snappy_mid.wav` — Stalker-style click, Snappier1.
- `geiger_resonant_stalker_snappy_strong.wav` — Stalker-style click, Snappier2.
- `geiger_resonant_metallic_default.wav` — metallic click, default attack.
- `geiger_resonant_metallic_snappy_mid.wav` — metallic click, Snappier1.
- `geiger_resonant_metallic_snappy_strong.wav` — metallic click, Snappier2.
- `geiger_resonant_scifi_default.wav` — sci-fi click, default attack.
- `geiger_resonant_scifi_snappy_mid.wav` — sci-fi click, Snappier1.
- `geiger_resonant_scifi_snappy_strong.wav` — sci-fi click, Snappier2.
- `geiger_resonant_bio_default.wav` — bio click, default attack.
- `geiger_resonant_bio_snappy_mid.wav` — bio click, Snappier1.
- `geiger_resonant_bio_snappy_strong.wav` — bio click, Snappier2.
- `beep_single.wav` — a single pattern beep.
- `beep_double.wav` — the double-beep pattern.
- `beep_error.wav` — the low, long error tone.
- `beep_alert.wav` — the short, high alert tone.

These files act as audio snapshots analogous to the PGM display snapshots; they
can be inspected manually or diffed across revisions.

### Automatic Snapshot Generation

A dedicated Unity test case in `test/test_audio` drives the audio engine and
WAV writer:

- It calls `asap_audio_init()` to reset the engine.
- It triggers appropriate geiger/beep/pattern events at the start of a
  2-second window.
- It renders 2 seconds of 48 kHz filtered audio into an in-memory buffer.
- It writes all WAV files to `snapshots/` in one run.

The test is wired into the existing `native` Unity test harness so that
`pio test -e native` generates both display PGM and audio WAV snapshots.

## Unity Integration (OnAudioFilterRead)

For real-time preview, the native environment also exposes an `extern "C"`
function suitable for calling from Unity via `DllImport`:

- `float asap_audio_render_sample();`

This function internally:

- Runs the same 8 kHz → 48 kHz upsampling scheme.
- Applies the digital 2-pole RC filter with `fc ≈ 7.2 kHz` per pole and
  `fs = 48000 Hz`.
- Returns a single filtered audio sample as a float in `[-1.0, 1.0]`.

A C# script (e.g. `AsapAudioFilter.cs`) can then hook into Unity’s
`OnAudioFilterRead(float[] data, int channels)` callback:

- For each sample in `data`, it calls `asap_audio_render_sample()`.
- It writes the returned value into the buffer for each channel.

With the native shared library built from this project, Unity can play a
near-MCU-faithful rendition of the detector’s audio (geiger clicks and beep
patterns) in real time while using the same fixed-point engine as the STM32
firmware.
