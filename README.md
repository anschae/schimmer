# Schimmer

Schimmer is an atmospheric reverb plugin (VST3 for macOS and Windows, AU for macOS) by anschae.
It is built for huge, dreamy, "larger than life" spaces rather than realistic rooms: pads, drones,
ambient guitar, synths and textures. It covers everything ValhallaShimmer does (see
[docs/PARITY.md](docs/PARITY.md)) and adds Bloom, Grains, a tempo-synced delay in front of the
reverb, ducking, Clear Tail, Freeze, Randomize and Morph.

**Status: in development (v0.1).** All features are implemented and measured automatically; the
plugin has not yet been tried by a person in Cubase or Logic. Please report anything odd.

## Download

Builds come from GitHub Actions, not from a local machine:

1. Open the [Actions tab](https://github.com/anschae/schimmer/actions) and click the newest green
   run of **Build** on `main`.
2. At the bottom ("Artifacts") download
   - `Schimmer-macOS.zip` (VST3 + AU + Standalone, universal: Apple Silicon and Intel),
   - `Schimmer-Windows.zip` (VST3 + Standalone, x64),
   - `audio-demos` (WAV renders and spectrograms of every factory preset and the ValhallaShimmer
     parity demos, plus a screenshot of the UI).

You need to be logged in to GitHub to download artifacts. Artifacts expire after 90 days; tagged
releases (Releases page) keep their files.

## Installation

### macOS
1. Unzip `Schimmer-macOS.zip`.
2. Copy `Schimmer.vst3` to `~/Library/Audio/Plug-Ins/VST3/` (Cubase) and `Schimmer.component` to
   `~/Library/Audio/Plug-Ins/Components/` (Logic). In Finder: Go > Go to Folder… and paste the path.
3. The plugin is not notarized. If macOS refuses to load it ("damaged" / "cannot be verified"),
   remove the quarantine flag in Terminal:
   ```sh
   xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/Schimmer.vst3
   xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/Schimmer.component
   ```
4. Cubase: Studio > VST Plug-in Manager > rescan. Logic: restart Logic (it validates new AUs at start;
   if Schimmer is listed as failed, use Logic Pro > Settings > Plug-in Manager > Reset & Rescan).

### Windows
1. Unzip `Schimmer-Windows.zip`.
2. Copy the folder `Schimmer.vst3` to `C:\Program Files\Common Files\VST3\`.
3. Rescan in Cubase (Studio > VST Plug-in Manager).

## Quick start
- Pick a preset with the arrows or the preset menu (categories: Pads, Shimmer, Organ, Guitar,
  Grains, Drones, Vintage, Ambience, Send).
- As a send effect, use Mix 100 % (the "Send" presets do that).
- Drag the handles in the **Tail View** (pre-delay, decay, delay time) and in the **Decay /
  Frequency** curve (low/mid/high decay, crossovers, Low Cut, High Cut).
- **Clear Tail** (button or automation of the "Clear Tail" parameter: a short 0→1 spike) silences
  the reverb instantly, e.g. before a quiet section. **Auto Clear** does it on transport start.
- **Freeze** holds the tail forever; "Freeze + Add" keeps layering new input on top.

## Parameters

| Section | Controls |
|---|---|
| Reverb | **Mode** (Nebula: mono-in, enormous, slow fade-in · Cathedral: huge stereo · Hall: medium · Chamber: small, chorus-like ambiences), **Size**, **Decay** (0.3–120 s), **Diffusion** (0 % clear echoes … 100 % dense), **Bloom** (slow swell, 0–5 s), **Pre-Delay** (ms or synced), **Mod Rate / Mod Depth / Drift** (LFO ↔ random wander), **Color** (Bright / Dark = late-70s digital), **Quality** (Eco / High / Ultra) |
| Tone & Stereo | in-loop **Low Cut / High Cut**, **Low / High Decay** multipliers with **crossovers**, **Warmth** (saturation in the loop), **Input Low/High Cut**, **Tilt**, **Width** (0–200 %), **Bass Mono** |
| Shimmer | **Shimmer Mode** (Bypass, Single, Dual, Single Reverse, Dual Reverse, Two-Voice), **Pitch** (−24…+24 st, Snap, Fine), **Pitch 2** + **Voice Mix** (Two-Voice), **Shimmer** amount, **Smoothness** (clean ↔ noisy "orchestral"), **Shimmer Low/High Cut**, **Placement** (In Loop = cascading, Pre = once) |
| Grains | on/off, **Source** (input / tank), **Destination** (parallel / into tank), **Size**, **Density**, **Pitch** (+ Quantize), **Pitch Jitter**, **Spray**, **Reverse** probability, **Scatter**, **Amount** |
| Delay | on/off, **Time** (ms or synced note, dotted, triplet), **Feedback** (to 98 %), **Ping-Pong** + **Stereo Offset**, **Low/High Cut** in the feedback, **Mod**, **Delay > Reverb**, **Delay Direct** (to the output, without reverb) |
| Ducking | as in Prog Delay: source (input / sidechain), threshold, depth (or full), knee, attack, hold, release (or synced), detector HP/LP, Peak/RMS, lookahead (latency reported), **target** (all wet / reverb / delay), **position** (Post / Pre) |
| Global | **Mix** (equal power), **Output**, **Mix Lock**, **Clear Tail**, **Clear Target**, **Auto Clear**, **Freeze**, **Freeze Mode**, **Morph** (A/B snapshots, automatable), **Randomize** with section locks |

## Sound design notes
See [docs/RESEARCH.md](docs/RESEARCH.md) for the algorithms (multichannel diffuser, 16-line modulated
feedback delay network, granular in-loop pitch shifting) and the measurements that check them.

## Building (developers)
See [CLAUDE.md](CLAUDE.md).
