# Universe X - Phase 1 skeleton

VST3 + Standalone, JUCE 6.1.6, 64-bit Windows, Windows 7 as minimum target.

**Status:** source code only. It has not been compiled or tested yet. Expect to fix small compile errors on the first build.

## What's in Phase 1
- `Parameters.*`: about 190 parameters in one list (oscillators, filters, 4 LFOs, 4 envelopes, 6-slot mod matrix, XY pad, 16-step sequencer, arp, effects, 8 macros, global).
- `Randomizer.*`: the Lazy Randomizer. 10 buttons (9 sections + Full Mix), section locks, amount (subtle to full), undo/redo (64 steps), safe boundaries (master volume, octave, polyphony, unison, glide are never randomized; cutoff never fully closes; the patch is never silent).
- `PluginProcessor.*`: parameter state, save/load, silent audio (the engine is Phase 2).
- `PluginEditor.*`: a working Lazy strip, not the final artwork.

## Build (Windows)
1. Install Visual Studio 2019 or 2022 with "Desktop development with C++". For Windows 7 support, also install the **v141 (VS 2017) toolset** component. Install CMake and Git.
2. From this folder:

```
cmake -B build -G "Visual Studio 16 2019" -A x64 -T v141
cmake --build build --config Release
```

(With Visual Studio 2022 use `-G "Visual Studio 17 2022"`.) Extract the JUCE 6.1.6 Windows zip into this folder first (you should get `JUCE\CMakeLists.txt`). If there is no `JUCE` folder, CMake downloads JUCE 6.1.6 instead.

3. Output: `build\UniverseX_artefacts\Release\VST3\Universe X.vst3` and the Standalone `.exe`. Copy the `.vst3` to `C:\Program Files\Common Files\VST3`.

## Before shipping
- Change `COMPANY_NAME` and `PLUGIN_MANUFACTURER_CODE` in `CMakeLists.txt`.
- Test on a real Windows 7 machine or VM. If JUCE 6.1.6 or the toolset gives trouble, report the error.

## Next phases
2. Audio engine: wavetable oscillators, filters, envelopes, LFOs, voices.
3. Mod matrix, macros, step sequencer, arpeggiator, effects.
4. Sampler, granular, spectral, 8 layers, preset browser.
5. Final Universe X interface, then Universe X 2.
