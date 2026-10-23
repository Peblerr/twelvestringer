# TwelveStringer

A VST3 / AU effect that makes a 6-string guitar sound like a 12-string. Works in FL Studio (Windows and Mac) and any other VST3/AU host.

## How it works

A real 12-string has six *courses* of paired strings. The four low courses (E A D G) pair each string with a thin string tuned an octave higher, and the two high courses (B e) pair each string with a unison twin that is never perfectly in tune. TwelveStringer rebuilds those partner strings from your 6-string signal:

| Section | What it adds |
|---|---|
| **Octave strings** | A phase-vocoder pitch shifter adds a +12-semitone copy of the low/mid register, thinned and brightened like a light-gauge octave string. |
| **Unison strings** | Two slightly detuned copies (one sharp, one flat), panned apart, for the shimmering chorus of the paired high strings. |
| **Feel** | A short pick-timing offset, slow random drift so the pairs never lock together, and stereo width. |

## Controls

| Knob | Range | What it does |
|---|---|---|
| Octave · Mix | 0–100 % | Level of the octave strings. |
| Octave · Range | 300 Hz–8 kHz | How far up the neck octaves are added. Lower = octaves only on the bass strings (more authentic); higher = octaves on everything (brighter, more "jangle"). |
| Octave · Sparkle | −6 to +12 dB | Brightness of the octave strings. |
| Unison · Mix | 0–100 % | Level of the detuned unison twins. |
| Unison · Detune | 0–25 cents | How far out of tune the twins are. 4–8 is realistic; above 12 gets seasick. |
| Unison · Drift | 0–100 % | Slow random tuning wander, like a real 12-string going slightly out. |
| Strum | 0–20 ms | How late the partner strings are picked relative to the main string. |
| Width | 0–100 % | Stereo spread of the partner strings. |
| Dry | 0–100 % | Level of your original guitar. Turn it down to hear only the added strings. |
| Output | −24 to +12 dB | Overall level. |
| **LIVE** | on / off | **Off (HQ)**: cleanest octave; adds ~40 ms of latency that is reported to FL Studio so it is compensated automatically. **On (Live)**: zero latency for monitoring while you record, with a rougher octave on low notes. Track with LIVE on, mix with it off. |

Presets: Classic 12-String, Jangle Pop, Folk Strummer, Subtle Doubler, Wide Shimmer, Octaves Only.

## Getting the plugin file

The plugin has to be compiled for your computer. Two ways:

### Option A — let GitHub build it (no coding tools needed)

1. Create a free account at github.com and make a new repository (e.g. `twelvestringer`).
2. Upload the contents of this folder to it, **including the hidden `.github` folder** (dragging the folder into the web uploader works; if `.github` gets skipped, use GitHub Desktop).
3. Open the repository's **Actions** tab. The "Build TwelveStringer" workflow starts automatically (or click **Run workflow**). It takes about 10 minutes.
4. When it finishes, open the run and download **TwelveStringer-Windows** (or **TwelveStringer-macOS**) from the Artifacts section, then unzip it.

### Option B — build it on your own computer

**Windows:** install [Visual Studio 2022 Community](https://visualstudio.microsoft.com/vs/community/) with the *Desktop development with C++* workload (it includes CMake), and [Git](https://git-scm.com/). Then open *Developer PowerShell for VS 2022* in this folder and run:

```
cmake -B build
cmake --build build --config Release --target TwelveStringer_VST3
```

**macOS:** install Xcode (or the Command Line Tools) and CMake, then:

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target TwelveStringer_VST3 TwelveStringer_AU
```

The first build downloads JUCE (the audio framework) automatically. The plugin ends up in `build/TwelveStringer_artefacts/Release/VST3/`.

## Installing in FL Studio

1. Copy `TwelveStringer.vst3` to your VST3 folder:
   - Windows: `C:\Program Files\Common Files\VST3\`
   - Mac: `~/Library/Audio/Plug-Ins/VST3/` (and `TwelveStringer.component` to `~/Library/Audio/Plug-Ins/Components/` if you want the AU)
2. In FL Studio: **Options → Manage plugins → Find installed plugins**. TwelveStringer appears under effects.
3. Route your guitar to a mixer track and load TwelveStringer in an insert slot. Pick a preset from the dropdown in the plugin window.

On a Mac, if FL Studio refuses to load a plugin downloaded from GitHub, run this once in Terminal:

```
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/TwelveStringer.vst3
```

## Tips

- Feed it a clean or lightly compressed guitar; put distortion *after* it.
- Strummed open chords sell the effect most. For single-note lines, try **Subtle Doubler** or lower the Octave Range.
- For the classic 60s jangle, use **Jangle Pop**, then add compression after it.

## Project layout

```
Source/TwelveStringEngine.h   all of the DSP (plain C++, no framework)
Source/PluginProcessor.*      parameters, presets, state, latency reporting
Source/PluginEditor.*         the plugin window
tests/render_demo.cpp         renders a before/after demo with a synthetic guitar
tests/host/                   loads the built VST3 like a DAW and checks it
.github/workflows/build.yml   cloud build for Windows and macOS
```

Built with [JUCE](https://juce.com) 8.0.9. Personal use is fine; JUCE is dual-licensed (AGPLv3, or JUCE's own licences including a free Starter tier), so check juce.com/legal before distributing or selling a plugin built with it.
