# CLAUDE.md – Schimmer

Atmospheric reverb plugin (VST3 macOS/Windows, AU macOS, Standalone) by anschae. JUCE 8.0.15, CMake,
C++20, Catch2 v3. The owner (Andreas) is a guitarist and teacher, not a developer: he has no local
toolchain, uses Cubase (main) and Logic Pro, and gets every binary from GitHub Actions. Talk to him
in German; code, comments, docs and commit messages are in English.

**Keep this file up to date with every change.** Later sessions continue from it.

## Rules that must not be broken
- **Sound first.** Fewer features that sound outstanding beat many mediocre ones. Measure (tests,
  render tool, spectrograms) before calling a DSP change done.
- **Never rename or remove a parameter ID** (`src/Parameters.cpp`, table order = enum `P` order).
  New parameters are appended (table + enum + `tests/ProcessorTests.cpp` list) with a higher
  `kParamVersion`. `mode`, `shimMode`, note-value and other choice indices are stored in projects:
  only append choices.
- **Audio thread:** no allocation, locks (try-locks are OK), file I/O or logging. Smooth every
  continuous parameter. `Engine::setQuality` allocates: it is only called while processing is
  suspended (processor timer).
- **State compatibility:** root `SchimmerState` with `stateVersion`; parameters missing in an old
  state fall back to defaults (test "An older state ...").
- **Don't fake features.** If something isn't finished, say so in the README and to the owner.
- **Keep CI green** on Linux, macOS and Windows.

## Status (milestones)
- M0 research + architecture: done (`docs/RESEARCH.md`).
- M1 skeleton, CI, render tool: done (first push 2026-11-21).
- M2 reverb core: implemented and measured (RT60, NED, resonance, flutter, decorrelation,
  sample-rate invariance, level consistency, clicks); sonic tuning ongoing.
- M3 delay, M4 shimmer, M5 grains, M6 clear/freeze/bloom, M7 ducking: DSP implemented with tests.
- M8 UI: placeholder grid of all controls; Tail View / decay curve / ducking display still to do.
- M9 presets (25 factory presets), mix lock, morph (engine + state): done; randomize: to do.
- M10 polish: to do.

## Build / test (Linux)
```sh
sudo apt-get install -y ninja-build xvfb libasound2-dev libx11-dev libxext-dev libxrandr-dev \
  libxinerama-dev libxcursor-dev libxi-dev libfreetype-dev libfontconfig1-dev libgl1-mesa-dev
cmake -S . -B build-dev -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DSCH_BUILD_TOOLS=ON
cmake --build build-dev --target SchimmerTests SchimmerRender SchimmerSnapshot Schimmer_VST3
./build-dev/SchimmerTests_artefacts/RelWithDebInfo/SchimmerTests            # all except [.long]
./build-dev/SchimmerTests_artefacts/RelWithDebInfo/SchimmerTests "[.long]"  # 10-minute stability
xvfb-run -a build-dev/SchimmerRender_artefacts/RelWithDebInfo/SchimmerRender out --preset "Eno Shimmer" --signals impulse,pad
xvfb-run -a build-dev/SchimmerSnapshot_artefacts/RelWithDebInfo/SchimmerSnapshot out
```
- Fast DSP iteration without JUCE: `g++ -std=c++20 -O3 -funsafe-math-optimizations -fno-math-errno
  -o /tmp/devrender tools/dev/devrender.cpp src/dsp/Engine.cpp src/dsp/Tank.cpp`, then
  `/tmp/devrender out.wav impulse 10 mode=1 decay=4 sh=1 shamt=0.5` and
  `python3 tools/dev/analyze.py out.wav --png out.png` (RT60 per band, NED, resonances, flutter,
  correlation, spectrogram).
- To skip downloads, pass `-DFETCHCONTENT_SOURCE_DIR_JUCE=<juce checkout>` and
  `-DFETCHCONTENT_SOURCE_DIR_CATCH2=<catch2 checkout>`.
- The DSP sources (`src/dsp/*.cpp`) are compiled with `-O3 -funsafe-math-optimizations
  -fno-math-errno` (`/fp:fast` on MSVC): 2-3x faster. Never add `-ffinite-math-only` (NaN guard).

## Identity (never change after release)
Product `Schimmer`, target `Schimmer`, manufacturer `anschae`, manufacturer code `Ansc` (shared
with IRene and Prog Delay), plugin code `Schm`, bundle id `ch.anschae.schimmer`.

## File layout
```
src/Parameters.*        parameter table (IDs, ranges, defaults, tooltips, sections), formatting
src/PluginProcessor.*   APVTS, settings mapping (makeSettings), tempo sync, morph, clear trigger, state
src/PluginEditor.*      scales MainView (design size 1280x820, fixed aspect ratio)
src/dsp/                plain C++ (no JUCE): the whole signal path
  Engine.*              chain, chunking (32-sample parameter grid), clear tail, mode switching, meters
  Diffuser.h            8-ch multichannel allpass input diffuser
  Bloom.h               8-line FDN that releases energy slowly into the tank
  Tank.*                16/8-line modulated FDN with in-loop shimmer, absorption, filters, limiter
  GrainShifter.h        granular OLA pitch shifter (shimmer)
  Granulator.h          grains engine
  EchoDelay.h           delay in front of the reverb
  Ducker.h              ducking (from Prog Delay)
  RingBuffer.h          pow2 ring buffer with O(1) clear
  Modes.h               mode table (sizes, spreads, stages, early level, modulation, stereo)
src/model/              FactoryPresets (code), PresetManager (user presets *.schpreset, A/B)
src/ui/                 LookAndFeel/Theme/Widgets/DuckingDisplay (from Prog Delay), MainView
tests/                  Catch2: DspTests (measurements), ProcessorTests; Analysis.h = metrics
tools/RenderTool.cpp    offline renders: WAV + spectrogram PNG + INDEX.md (CI: audio-demos)
tools/Snapshot.cpp      editor -> PNG
tools/TestSignals.h     impulse, noise bursts, sine bursts, pad chord, snare, guitar arpeggio
tools/dev/              devrender (JUCE-free harness) and analyze.py
docs/RESEARCH.md        sources and DSP decisions; docs/PARITY.md ValhallaShimmer checklist
```

## Signal flow and DSP decisions (details in docs/RESEARCH.md)
in → lookahead (ducking) → input HP/LP → [duck pre] → echo delay → reverb in = in + echo·amount →
pre-delay (crossfaded on changes) → pre-shimmer (Placement "Pre") → Dark input LP (10 kHz, 8th
order) → **diffuser** (Diffusion: crossfade from dry below 30 %, allpass g = 0.72·smoothstep((d-0.3)/0.7))
→ early taps → **bloom** → freeze gate / clear gain → **tank** → taps + early·level → grains →
Dark output LP → tilt → width / bass mono → + echo direct → [duck post] → clear fade → equal-power
mix → output.

Tank details:
- Line length i = mean·scale·spread^(rel_i−0.5); spread shrinks to 1.05 when Diffusion → 0 (clear
  echoes). Size scale = 0.25·8^size, smoothed 0.35 s (gliding = tape pitch bend on fast sweeps).
- Modulation per line: LFO (rate × per-line factor 0.62..1.61) blended with smoothed random by
  Drift; depth = Mod Depth × mode max ms × √scale. Hermite reads (linear in Dark).
- In-loop allpass per line (integer length: linear interpolation inside an allpass loop is not
  allpass and caused HF ripple), g = 0.62·smoothstep((d−0.3)/0.7).
- **Output taps before the loop attenuation** (first pass at unity; level normalisation relies on
  energy = 1/(1−g²)).
- Absorption: gain gM = 10^(−3·len/T60) and first-order low/high shelves with gL/gM, gH/gM (exact
  per-band T60 at DC and Nyquist). Low/high cut one-poles, Dark 9 kHz one-pole, 4 Hz DC blocker.
  Freeze crossfades all of them to flat (DC blocker stays).
- Input gain = 2.7·sqrt((T60eff/2)^0.4 / E), E = 1/(1−mean g²): same loudness across modes and
  sizes, +~4 dB from 1 s to 30 s decay.
- Shimmer in loop: after the Hadamard mix, a fraction `amount` of the 16 lines (order interleaves
  L/R) is replaced by its pitch-shifted version (energy-neutral → stable at any amount). Dual:
  alternating up/down; Two-Voice: golden-ratio sequence assigns lines to voice B by voice mix.
  Shifter latency is added to the decay-gain loop length.
- Safety limiter: line RMS per 32-sample chunk above 0.7 reduces the loop gain (attack per chunk,
  release 300 ms). NaN guard resets the tank.
- Mode switch: fade 5 ms → clear reverb buffers → new mode (delay keeps running).
- Clear Tail: rising edge → 5 ms fade → O(1) buffer clear (`RingBuffer::clearFromNow`) → full gain.

## Parameters (APVTS, version hint 1)
mode size preDelay preDelaySync preDelayNote decay freeze freezeMode diffusion bloom modRate modDepth
modDrift lowCut highCut lowMult highMult lowXover highXover color warmth quality inHP inLP tilt width
bassMono mix mixLock output shimMode pitch pitchSnap fine pitch2 voiceMix shimAmount shimLowCut
shimHighCut shimSmooth shimPlace grainOn grainSource grainDest grainSize grainDensity grainPitch
grainQuant grainSpray grainPitchJit grainReverse grainScatter grainAmount delayOn delayTime delaySync
delayNote delayFeedback delayPingPong delayOffset delayHP delayLP delayMod delayToReverb delayLevel
clearTail clearTarget autoClear duckOn duckSource duckHP duckLP duckDetMode duckRms duckThresh
duckDepth duckFull duckKnee duckAttack duckHold duckRelease duckRelSync duckRelNote duckLook
duckTarget duckPos morphOn morph

## CI (`.github/workflows/build.yml`)
linux: tests + `[.long]` 10-minute stability + pluginval (strictness 10) + render tool → artifact
`audio-demos` (presets/, parity/, ui/). macos: universal build, tests, ad-hoc signing, pluginval,
`auval -strict -v aufx Schm Ansc`. windows: x64, static runtime check, tests, pluginval. package:
collects `Schimmer-macOS.zip` / `Schimmer-Windows.zip`; on `v*` tags attaches them to a release.

## Known issues / open points
- UI is a placeholder (all controls in a grid); visualisations missing.
- Randomize not implemented yet.
- Not yet tested by a person in Cubase / Logic.
- Quality "Ultra" currently behaves like "High".
- Chamber mode has lower modal density (allowed +1 dB more spectral ripple in the test), like
  ValhallaShimmer's Small mode.
