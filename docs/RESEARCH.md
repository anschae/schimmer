# Schimmer – Research and DSP decisions

This document collects the research done before writing code (milestone M0) and the decisions that
came out of it. It is the reference for *why* the DSP looks the way it does. When a decision changes,
update the relevant section and add a line to "Change log" at the end.

Sound goal (from the spec): huge, dreamy, unreal spaces; dense and smooth tails without metallic
ringing, flutter or discrete echoes; decays from 0.5 s to 60 s and infinite without mud; a very wide,
decorrelated stereo image that still survives a mono fold-down; shimmer, grains and modulation.
Hard requirement: everything ValhallaShimmer can do (see `docs/PARITY.md`).

---

## 1. Sources

Primary literature and articles (all read or consulted for this document):

| # | Source | Used for |
|---|--------|----------|
| S1 | J. Dattorro, "Effect Design, Part 1: Reverberator and Other Filters", JAES 45(9), 1997 ([PDF](https://ccrma.stanford.edu/~dattorro/EffectDesignPart1.pdf)) | Figure-of-eight plate tank, input diffusers, modulated allpasses, allpass interpolation |
| S2 | J.-M. Jot, A. Chaigne, "Digital Delay Networks for Designing Artificial Reverberators", AES 90th Conv., preprint 3030, 1991 | FDN with unitary matrix, per-line absorption filters, tonal correction |
| S3 | J. Stautner, M. Puckette, "Designing Multi-Channel Reverberators", CMJ 6(1), 1982 | 4-line FDN, stability condition |
| S4 | G. Luff (Signalsmith Audio), "Let's Write A Reverb", 2021 ([article](https://signalsmith-audio.co.uk/writing/2021/lets-write-a-reverb/)), ADC 2021 talk | Multichannel diffuser (delay → shuffle/flip → Hadamard), Householder loop, separation of diffusion and decay |
| S5 | S. Costello, Valhalla DSP blog: "Eno/Lanois Shimmer Sound: How it is made", "Shimmer: Modulation, auto-correlation, and decorrelation", "Reverbs: Diffusion, allpass delays, and metallic artifacts", "Modulation in reverbs: reality and unreality", "ValhallaShimmer: The Controls", "a bit of history", Tips & Tricks ([valhalladsp.com](https://valhalladsp.com/?p=553)) | Shimmer topology, randomized "de-glitched" shifter sound, diffusion/coloration trade-off, ValhallaShimmer controls |
| S6 | ValhallaShimmer manual notes ([PDF](https://www.valhalladsp.com/shimmer/ValhallaShimmerNotes.pdf)) | Parity checklist |
| S7 | D. Griesinger, "Practical Processors and Programs for Digital Reverberation", AES 7th Int. Conf., 1989 ([PDF](https://freeverb3-vst.sourceforge.io/doc/10.1.1.72.5367.pdf)) | Lexicon approach: allpass loops, random time variation against coloration |
| S8 | J. S. Abel, P. Huang, "A Simple, Robust Measure of Reverberation Echo Density", AES 121st Conv., 2006; Huang, Abel et al., AES 123rd/125th Conv. | Normalized echo density (NED) used in the automated checks |
| S9 | S. J. Schlecht, E. A. P. Habets, "Feedback Delay Networks: Echo Density and Mixing Time", IEEE/ACM TASLP 2017; "Accurate Reverberation Time Control in FDNs", DAFx 2017; "Scattering in FDNs", 2020 | Delay length choice vs. echo density, attenuation proportional to delay length |
| S10 | J. O. Smith, *Physical Audio Signal Processing*, "Choice of Delay Lengths", "History of FDNs" ([CCRMA](https://ccrma.stanford.edu/~jos/pasp05/History_FDNs_Artificial_Reverberation.html)) | Mode density rules of thumb, mutually prime lengths |
| S11 | K. Barr, Spin Semiconductor FV-1 notes and forum posts; "Analyzing the Spin Mini Reverb" (SpinCAD docs) | Allpass-loop ("ring") topology |
| S12 | Valhalla VintageVerb mode/color descriptions (1970s = 10 kHz bandwidth + internal downsampling; 80s = full rate, noisy modulation) | "Dark" color mode |
| S13 | Strymon BigSky manual (Bloom, Cloud, Shimmer machines) | Bloom = diffusion/feedback section feeding a tank; Cloud = cascaded input diffusion |
| S14 | M. R. Schroeder, "Natural Sounding Artificial Reverberation", JAES 10(3), 1962; "Colorless Artificial Reverberation", JAES 9(3), 1961 | Allpass diffusion, coloration |
| S15 | E. Moulines, F. Charpentier, PSOLA/TD-SOLA (1990); J. Laroche, M. Dolson, "Improved Phase Vocoder Time-Scale Modification", 1999 | Pitch shifter comparison |
| S16 | C. Roads, *Microsound*, 2001; R. Bencina, "Implementing Real-Time Granular Synthesis", 2001 | Granular engine |
| S17 | V. Välimäki, J. Parker, L. Savioja, J. O. Smith, J. Abel, "Fifty Years of Artificial Reverberation", IEEE TASLP 20(5), 2012 | Overview, velvet noise, modulated FDNs |

Sonic references (character only, no code or proprietary algorithm copied): Valhalla Supermassive,
Shimmer and VintageVerb; Strymon BigSky (Cloud, Bloom, Shimmer); Eventide Blackhole; Lexicon 224/480 halls.

---

## 2. Reverb topologies compared

### 2.1 Feedback delay network (Stautner/Puckette, Jot)
N delay lines whose outputs are mixed by a unitary (energy preserving) matrix and fed back. Decay is
set by a gain per line `g_i = 10^(-3 d_i / (T60 fs))` (S2, S9): because the attenuation is
proportional to each line's length, every line decays at the same rate in dB/s and the decay is
free of beating between lines. Frequency-dependent decay: replace the gain by a low-order
"absorption" filter per line designed the same way per band.

* Pros: decay time is an exact, independent parameter; stability by construction (unitary matrix,
  gains < 1); mode density = sum of delay lengths, freely scalable; parallel lines vectorise well
  (SIMD over lines); many decorrelated output taps for wide stereo.
* Cons: echo density builds up slowly at the start if nothing diffuses the input ("sputter",
  S4, S8); short delays sound metallic (low mode density, S10); a strongly mixing matrix can lock
  lines together and colour long tails (S4).

**Channel count.** 8 lines: OK for small rooms, audible fluttery texture on long tails. 16 lines:
the usual high-quality choice, mode density for 60 s decays is reached with 50–300 ms lines.
32 lines: marginal improvement for twice the cost; better spent on modulation and diffusion.
→ **16 lines** in the tank (8 in the Eco quality level).

**Matrix.** Hadamard (all entries ±1/√N) mixes maximally: every line feeds every line with equal
weight, echo count ×N per pass. Householder `I - (2/N) 1 1ᵀ` mixes weakly for large N (diagonal
0.875 for N = 16): cheap (O(N)) and avoids coloured "locking" (S4). Random orthogonal matrices
behave like Hadamard but cost O(N²). For a *huge* reverb with an input diffuser in front, the echo
density at the start comes from the diffuser; the loop matrix then mostly decides how fast the tail
becomes Gaussian. Decision: **Hadamard (fast Walsh–Hadamard transform, O(N log N)) combined with a
fixed permutation and sign pattern**. A pure Hadamard loop with equal-ish delays can produce
periodic "pulsing"; the permutation breaks the symmetry. This is verified by measurement
(NED, tail spectral flatness) rather than assumed; Householder stays available as a compile-time
option for comparison (`tests/` measure both).

**Delay lengths.** Mutually prime, spread geometrically over a ratio of ~1:2.5 (S9: larger spread
increases echo density growth, the mean length sets mixing time and "size" perception). Lengths are
generated per mode from a base length, a spread and a fixed random seed (S4: random within equal
sub-ranges), then rounded to the next prime at the current sample rate so every sample rate gets
the same times in seconds.

### 2.2 Dattorro plate / figure-of-eight tank
Input bandwidth filter → 4 series allpasses (input diffusion) → a tank of two cross-coupled
branches, each with a modulated allpass, a delay, damping, decay gain, a second allpass and a delay.
Output is taken from 7 taps per channel (S1).

* Pros: very fast echo density (the allpasses are *inside* the loop), lush with modulation, cheap.
* Cons: only two loops: decay control is approximate (one gain for paths of different lengths);
  lengths are tuned for 29.8 kHz; long decays (> 10 s) become audibly periodic and metallic unless
  modulated heavily; stereo taps are fixed.

### 2.3 Allpass loops (Keith Barr / FV-1, Lexicon)
A ring of 2–4 sections "allpass, allpass, delay, damping" with the output of the last section fed
back to the first (S11). Lexicon halls are related: allpass-rich loops with random time variation
(S7). Very dense and lush for the cost; the sound depends on hand tuning; decay is coupled to
allpass coefficients and loop length.

### 2.4 Multichannel diffuser (Signalsmith)
A chain of steps "N delays → shuffle + polarity flips → Hadamard", N ≥ 8. Each step multiplies the
echo count by N; 4–5 steps reach 2000–4000 echoes/s, where echoes merge into noise (S4, S8).
Delays doubling per step give a smooth onset. Lossless and colourless (no feedback, just scattering).

### 2.5 Decision: hybrid
**Input diffuser (multichannel, Signalsmith style, but with an allpass coefficient) → optional
"bloom" network → 16-line modulated FDN tank with in-loop allpass diffusion, absorption filters,
low/high cut, shimmer, saturation and a safety limiter.** This takes the exact decay control and
decorrelation of the FDN (S2), the instant echo density of the multichannel diffuser (S4) and the
lushness of in-loop allpasses and random modulation (S1, S7, S11).

The input diffuser stages are *multichannel allpasses*: `w = x + g·U·D(w)`, `y = U·D(w) − g·w`,
with `U` orthogonal (Hadamard with permutation/flips) and `D` the per-channel delays. This is the
Möbius transform of a paraunitary system and therefore lossless for any |g| < 1. With g = 0 it is
exactly the Signalsmith step; raising g adds recirculation inside each stage, i.e. a slow fade-in
and a long diffusion tail "even with little feedback" – which is exactly how ValhallaShimmer's
Diffusion behaves (S5: 0.5–0.618 slow fade-in, 0.9 long decay at zero feedback).

---

## 3. Diffusion, echo density and coloration

* Series allpasses at the input sound metallic on impulsive material (S5 "Reverbs: Diffusion,
  allpass delays, and metallic artifacts"). Mitigations used here: (1) multichannel allpasses with
  a Hadamard scattering spread every echo over 8 channels, so no single short loop dominates;
  (2) stage delays are long-ish (doubling 6 → 96 ms × size) instead of 1–10 ms; (3) every stage
  delay is slightly and randomly modulated; (4) Diffusion scales the coefficient (0 → 0.72 at the
  input, 0 → 0.6 in the loop) instead of switching allpasses on/off.
* **Diffusion = 0** must turn the tank into "a plain FDN with clear echoes". With 16 different
  delays one would still get a cloud of echoes, not a clear echo. Decision: the *spread* of the
  tank delay lengths is also controlled by Diffusion below 25 %: at 0 the 16 lines cluster within
  ±2.5 % of the base length, so the tank behaves like one slightly chorused recirculating delay –
  usable for pitch-shifted echoes and chorus (ValhallaShimmer's "Diffusion < 0.5 sounds closer to a
  pitch shifted echo", S5). The spread is smoothed slowly (0.4 s), so sweeping Diffusion glides
  instead of clicking.
* Echo density target: NED (S8) reaching ≥ 0.9 within 150 ms of the first wet arrival for the
  large modes at Diffusion ≥ 70 % (perceived as smooth above ~20 000 echoes/s, S8).

## 4. Modulation (Griesinger, Costello)

Time variation breaks up the fixed mode pattern of a static network: modes are smeared in
frequency, so no resonance can build up over a long decay (S7, S5 "Modulation in reverbs").
Lexicon used random, smoothed modulation ("wander") plus slow LFOs ("spin").

Decision:
* Every tank line has its own modulator: a sine LFO whose rate is the Mod Rate × a per-line factor
  (0.62…1.61, golden-ratio spread so phases never align) **plus** a smoothed random walk (Drift).
  Depth is in milliseconds (0–4 ms × size); Drift blends from pure LFO (chorus-like, audible pitch
  wobble) to pure random (decorrelating, no audible vibrato).
* The in-loop allpasses and input diffuser stages get a fixed small random modulation (≤ 0.3 ms)
  that is independent of the user controls, purely against metallic coloration.
* **Interpolation.** Linear: cheap, low-pass filters by a varying amount → noisy HF "breathing"
  (the 1970s/80s character, S12). Allpass (S1): flat magnitude but state-dependent, transients when
  the delay jumps; bad for fast modulation. Lagrange-3 / cubic Hermite: near-flat to ~0.25 fs,
  no state, well behaved under modulation. → **Cubic Hermite** in Bright, **linear** in Dark (part of
  its character).

## 5. Frequency-dependent decay and tone

* Per-line absorption: a first-order low shelf and a first-order high shelf, designed per line so
  the decay time in the low band = Decay × Low Mult and in the high band = Decay × High Mult with
  adjustable crossovers (Jot's method, S2, S9). First order keeps phase smooth; per-band T60 is
  exact at DC and Nyquist and interpolates smoothly in between.
* In-loop Low Cut and High Cut (as in ValhallaShimmer): one-pole HP and LP per line. Applied on
  every pass they compound: the effective decay above High Cut shortens rapidly – this is the
  classic "darkening tail" and the main anti-mud control. A fixed 4 Hz DC blocker is always in the
  loop (also in freeze) to stop DC creep.
* Output tilt EQ (±6 dB around 800 Hz) and input HP/LP filters against mud on long decays.
* A "tonal correction" (S2) is not needed for a non-realistic reverb; the level stays steady
  because the tank input is normalised by √(1 − g²)-style energy factors where relevant.

## 6. Stereo and decorrelation

* Outputs: L and R are two **orthogonal** ±1 sign vectors over the 16 lines (from the Hadamard
  rows), scaled by 1/√16. Uncorrelated lines + orthogonal taps → L/R correlation ≈ 0, and the mono
  sum keeps the full energy (no comb-filter collapse): |L+R|² = |L|² + |R|².
* Stereo modes inject L into the even and R into the odd diffuser channels (true stereo: a hard
  left source stays left-weighted in the early part). The Mono mode sums the input and feeds all
  channels (mono-in/stereo-out, maximal size).
* Width 0–200 % in M/S; bass mono below an adjustable frequency (second-order HP on the side signal).

## 7. Pitch shifting in the loop (shimmer)

### 7.1 Placement
* **In the loop** (Eno/Lanois, S5): every recirculation is shifted again → cascading harmonics
  (+12: octave, two octaves, …). This *is* the shimmer sound.
* **Before the tank** ("Pre"): the input is shifted once; the reverb then decays normally. Cleaner,
  a "pitched reverb" rather than a cascade.
* Global loop "reverb output → shifter → reverb input" (the original hardware patch) is hard to keep
  stable because the reverb's energy gain is ≫ 1 for long decays.

Decision for In Loop: the shifter replaces the signal of a subset of the 16 tank lines **after the
mixing matrix**. Amount = fraction of lines that are shifted (crossfaded line by line). Since the
shifter preserves energy on average and replaces (does not add to) the signal, the loop gain is
unchanged → stable at any amount, and the per-pass shifted fraction is exactly the Amount. At 100 %
every recirculation is shifted, which matches ValhallaShimmer, where the whole feedback passes the
shifter. Dual mode = half of the shifted lines up, half down (no extra CPU, no extra energy).
Two-Voice = the shifted lines are split between voice A and voice B according to the voice mix.

### 7.2 Shifter algorithms

| Method | Quality | Artifacts | CPU | Latency |
|---|---|---|---|---|
| Two-tap delay crossfade ("rotating head", Eventide H910 style) | OK on noise, poor on tones | Amplitude/phase modulation at the window rate (buzzy at short windows, echoey at long) | very low | ½ window |
| Granular OLA, 4 overlapping Hann grains + jitter | Good on reverb tails (noise-like) | Noisy sidebands, controllable via jitter → "orchestral rosin" (S5) | low | ~grain |
| Reversed grains | Smooth, organ-like (S5/S6) | Slight smearing, no chattering | low | ~grain |
| PSOLA / correlation-based splicing ("de-glitching") | Excellent on monophonic tones | Chaos in a decorrelated, time-varying reverb loop (S5) | medium | ~2 periods |
| Phase vocoder / spectral | Very clean on stationary polyphonic signals | Phasiness, transient smear; Costello found a cleaner shifter in the loop sounds "more boring" (S5 KVR) | high | FFT size (≥ 2048) |

Decision: **granular overlap-add shifter**, 4 Hann grains per voice, Hermite interpolation,
grain length 40–120 ms depending on Smoothness, per-grain random jitter of length and start
position (Smoothness 100 % = no jitter, long grains; 0 % = heavy jitter, short grains → noisy
"orchestral" sidebands). Reverse modes read each grain backwards. The pitch ratio is applied per
grain start and smoothed, so continuous sweeps from −24 to +24 st glide without glitches. The
latency (up to (r−1)·grain for upward shifts) sits inside the loop and is included in the decay-gain
calculation so RT60 stays correct.

### 7.3 Keeping shimmer glossy
* Anti-aliasing: the shifted path is low-passed at `min(Shimmer Tone LP, 0.42 fs / ratio)` before
  shifting up, so octave-up of 15 kHz content never folds.
* Shimmer Tone HP/LP only on the shifted lines → the cascade is darkened at each pass, which keeps
  it from turning harsh and limits how many octaves build up.
* Normalisation for uncorrelated grain overlap (Hann, 4× overlap → Σw² = 1.5 → ×1/√1.5).
* In-loop safety limiter (below) catches the coherent case (tonal input + small detune can sum
  grains in phase).

## 8. Granular synthesis coupled with the reverb

Real-time granular synthesis (S16): a capture ring buffer (4 s stereo), a scheduler producing
grains at a mean density (1–80 grains/s) with exponential (Poisson) inter-onset jitter, each grain
with a raised-cosine (Tukey 50 %) window, a read position (now − delay − spray·random), pitch
(ratio, optionally quantised to semitones, plus jitter), direction (reverse probability), pan
(stereo scatter). Up to 48 simultaneous grains, pre-allocated. Output level normalised by
√(density × size) so Amount is level-independent.

Coupling: source = input or tank output; destination = into the tank (grains get reverberated:
clouds) or parallel to the wet output (glitter over the tail). Tank → grains → tank forms a second
feedback path; its gain is ≤ Amount × 0.5 and passes the safety limiter.

## 9. Stability, freeze, gain staging, denormals

* Unitary matrix + all gains ≤ 1 → stable by construction for the linear part.
* **Freeze/Infinite**: decay gain → 1, absorption shelves and low/high cut crossfaded to flat over
  80 ms, DC blocker stays. Input either muted ("Freeze") or kept ("Freeze + Add" – energy can grow;
  the limiter holds it).
* **Safety limiter**: the RMS over all lines is measured per 32-sample chunk; above +3 dB re. a
  nominal level the loop gain is reduced smoothly (attack 5 ms, release 300 ms). Never audible in
  normal operation; it only holds runaway cases (Freeze+Add, coherent shimmer, grains feedback).
* **Warmth** (optional): `tanh`-based soft saturation inside each line, drive 0–12 dB with level
  compensation; adds harmonics on loud tails and acts as a soft limiter.
* NaN/Inf guard: if a chunk produces a non-finite value, the tank is cleared (never observed in
  tests; it is a last line of defence so a host never receives NaNs).
* Denormals: FTZ/DAZ (`juce::ScopedNoDenormals`) in `processBlock`, plus the DC blocker.

## 10. Vintage digital reverb character ("Dark")

Late 1970s units (EMT 250, Lexicon 224) ran at low internal rates (≈ 20–34 kHz) with steep analog
anti-aliasing filters (≈ 10 kHz bandwidth), linear interpolation and fixed-point modulation noise
(S12). Decision for **Dark**: steep 10 kHz low-pass (8th order, Butterworth cascade) on the tank
input and output, an extra in-loop one-pole at 9 kHz, linear interpolation for the modulated reads,
and slightly deeper default modulation. A true lower internal rate was considered; it costs
resampling filters and latency and the audible part of it is the bandwidth and interpolation noise,
which this reproduces. Kept as an option for a later "1970s" color.

## 11. Size changes

Changing all delay lengths at once can either crossfade between two tank instances (no pitch
change, 2× CPU, smeared transition) or glide the read positions (Doppler pitch shift, "tape/rocket").
Decision: **glide**, smoothed with a 0.35 s one-pole, which gives a gentle tape-like pitch bend
during fast sweeps and is completely click-free (no discontinuity in any read position). This is
the musically useful behaviour for an experimental reverb (and what ValhallaShimmer does); slow
automation is practically pitch-neutral. Decay gains are recomputed from the current lengths so
RT60 stays constant during a sweep.

## 12. Bloom

Strymon's Bloom feeds a long diffusion/feedback section into the tank (S13). A first version used one
8-line FDN in front of the tank: the tank then integrates a decaying input, and the envelope of two
cascaded exponential decays always peaks early (measured: ~0.5 s whatever the Bloom setting).
Decision: **three cascaded 8-line feedback stages**, each energy-normalised (gain √(1−g²)), with a
per-stage energy time constant τ = Bloom / 2.2. Their combined release is a gamma density
t²·e^(−t/τ), which rises smoothly and peaks around the Bloom time; the tank integrates it with its
own decay. Measured: Bloom 1/3/5 s → envelope peak at 1.5/3.0/5.2 s (test "Bloom swells the tail
in ..."). The direct diffuser ("early") output is faded out as Bloom rises, so there is no initial
blip before the swell. Unlike an envelope follower this works for plucks as well as pads. The Tail
View draws the same model (numeric convolution of the gamma density with the tank decay).

## 13. Measurement methods (milestone checks)

* **RT60**: Schroeder backward integration of the impulse response energy, linear fit of −5…−35 dB
  (T30), per octave band for the decay-vs-frequency curve.
* **Echo density**: NED (S8) with a 20 ms sliding window.
* **Resonances / metallic ringing**: in the late tail (after mixing time), the spectrum (averaged
  periodograms, 1/3-octave-smoothed reference) must not show peaks > 9 dB above the smoothed
  envelope; also a spectral flatness check per band.
* **Flutter**: the envelope of the band-passed tail must not show periodic modulation (autocorrelation
  of the envelope below 0.3 at lags 20–200 ms).
* **Decorrelation**: normalised L/R cross-correlation of the tail |ρ| < 0.2; mono energy within
  ±1.5 dB of L+R energy sum.
* **Stability**: 10+ min of freeze/extreme settings: finite, bounded peak, DC < −60 dBFS.
* **Clicks**: high-pass (≥ 8 kHz) residual against a smoothed version must not spike during
  parameter sweeps and Clear Tail.
* **Sample rate invariance**: RT60, NED onset and band levels within tolerance at 44.1/48/96 kHz.

## 14. Architecture summary (signal flow)

```
in (mono/stereo) ─► input HP/LP ─► [Duck Pre] ─┬─► Delay (sync, ping-pong, fb HP/LP, mod) ─┬─► delay→reverb amt ─┐
                                               │                                           └─► delay→out level ──┼─► wet sum
                                               └────────────────────────────────────────────────────────────────┐ │
                                                    Pre-delay ◄── reverb in = input + delay·amt ◄──────────────┘ │
      Pre-shimmer (optional) ─► Input diffuser (8 ch, 5 multichannel allpass stages, Diffusion)                  │
      ─► Bloom network (8 ch FDN, Bloom) ─► Tank (16-line FDN):                                                 │
             read (modulated, Hermite/linear) → in-loop allpass (Diffusion) → absorption shelves (Decay, Low/High │
             mult) → low cut / high cut → DC block → warmth → Hadamard+perm mix → shimmer on k lines → limiter    │
      ─► L/R orthogonal taps ─► Dark color LP ─► Grains (parallel or into tank) ─► tilt EQ ─► width/bass mono ──┘
wet sum ─► Ducking (target: all / reverb / delay; Post) ─► Clear-tail fade ─► Mix (equal power) with dry ─► Output
```

---

## Change log
- 2026-11-21: initial version (M0).
- 2026-11-21: Bloom redesigned (cascade of three stages); shimmer amount taper (amount²) and an
  energy-matching AGC in the granular shifter (overlap-add gain depends on grain coherence).
