# ValhallaShimmer parity checklist

Every ValhallaShimmer control and mode (from Valhalla DSP's "ValhallaShimmer: The Controls" and the
manual notes) and its equivalent in Schimmer. The CI artifact `audio-demos/parity/` contains a
render for each row (file name in the last column, `.wav` + spectrogram `.png`).

| ValhallaShimmer | What it does there | Schimmer equivalent | Demo |
|---|---|---|---|
| **Mix** | dry/wet, power-complementary | **Mix** (equal-power crossfade, 0-100 %) | `parity_mix_25`, `parity_mix_100` |
| **Pitch** (-12..+12 st) | shift amount in the feedback loop | **Pitch** (-24..+24 st, Snap, Fine ±50 ct); sweeps glide | `parity_pitch_plus12`, `parity_pitch_minus12`, `parity_pitch_plus7` |
| **Feedback** | recirculation around diffusor/shifter; pitch only audible > 0 | **Decay** (0.3-120 s RT60) + **Freeze** (infinite); shimmer audible whenever Shimmer > 0 | `parity_feedback_short_2s`, `parity_feedback_long_30s`, `parity_feedback_infinite_freeze` |
| **Diffusion** | 0 = straight delay, 0.5-0.618 slow fade-in, 0.9 long decay at zero feedback | **Diffusion** 0-100 %: 0 = clear recirculating echoes, ~55 % slow fade-in, 100 % dense/long | `parity_diffusion_0_echoes`, `parity_diffusion_55_fadein`, `parity_diffusion_100` |
| **Size** | delay lengths | **Size** (0.25x-2x of the mode's base size) | `parity_size_small`, `parity_size_large` |
| **Low Cut** | high-pass in the feedback path | **Low Cut** (in loop, 10 Hz = off); plus Input Low Cut | `parity_lowcut_400` |
| **High Cut** | low-pass in the feedback path | **High Cut** (in loop, 20 kHz = off); plus Input High Cut, High Decay | `parity_highcut_2k` |
| **Mod Rate** | speed of many random modulators | **Mod Rate** (0.01-5 Hz) + **Drift** (LFO ↔ random) | `parity_mod_slow_deep`, `parity_mod_fast_chorus` |
| **Mod Depth** | modulation depth in samples | **Mod Depth** (0-100 % of the mode's maximum) | `parity_mod_slow_deep` |
| Reverb mode **Mono** | mono-in/stereo-out, very large, high density | **Nebula** | `parity_mode_mono_nebula` |
| Reverb mode **Big Stereo** | very large, cathedral-scale, slow swells | **Cathedral** | `parity_mode_big_cathedral` |
| Reverb mode **Medium Stereo** | halls, fast harmonic build-up | **Hall** | `parity_mode_medium_hall` |
| Reverb mode **Small Stereo** | small rooms, chorused ambiences, more colored | **Chamber** | `parity_mode_small_chamber` |
| Pitch mode **Single** | up or down, randomised (noisy "orchestral" sidebands) | **Single** (Smoothness low = noisy/orchestral, high = clean) | `parity_pitchmode_single` |
| Pitch mode **Dual** | up and down at once | **Dual** | `parity_pitchmode_dual` |
| Pitch mode **SingleReverse** | reversed grains, smoother, organ-like | **Single Reverse** | `parity_pitchmode_single_reverse` |
| Pitch mode **DualReverse** | reversed grains up + down, pipe organ | **Dual Reverse** | `parity_pitchmode_dual_reverse` |
| Pitch mode **Bypass** | no shifting | **Bypass** | `parity_pitchmode_bypass` |
| Color **Bright** | full bandwidth, hi-fi | **Bright** | `parity_color_bright` |
| Color **Dark** | 70s/80s digital reverb, steep cut above 10 kHz | **Dark** (8th-order 10 kHz LP on input and output, linear interpolation, in-loop 9 kHz) | `parity_color_dark` |
| (beyond Valhalla) | | **Two-Voice** (independent intervals + voice mix), Placement Pre/In Loop, Shimmer tone, Bloom, Grains, Delay, Ducking, Clear Tail | `parity_pitchmode_two_voice` |

Notes
- ValhallaShimmer's "Feedback" sets a loop gain; Schimmer sets the decay time directly (RT60), which
  stays correct when Size changes. Both reach infinite: Feedback 1.0 there, Freeze here.
- In ValhallaShimmer the whole feedback passes the shifter; Schimmer's Shimmer amount at 100 % does
  the same, lower values shift only part of each recirculation.
