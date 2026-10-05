# MIDI Composer

Cross-platform MIDI composition app (C++20, JUCE 8, CMake) with scale-based Chord Input Mode.

## Build
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build            # core unit tests (no GUI/audio/plugins)
```
JUCE is used from `external/JUCE` if present, otherwise fetched (tag 8.0.4).
`-DMC_BUILD_APP=OFF` builds only the core library + tests (no JUCE needed).
Linux deps: libasound2-dev libfreetype-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libgtk-3-dev.

## Layout
- `src/core` – pure C++ (no JUCE): `theory` (Scale/Chord Engine), `model` (Track/MidiTrack/AudioTrack, Project, undoable Commands),
  `sequencer` (timing, playback snapshots), `input` (ChordInputProcessor, modifier mapping, MIDI Learn), `io` (JSON project format v1, SMF import/export).
- `src/app` – JUCE: `audio` (realtime engine), `midi` (device manager), `recording`, `plugins` (InstrumentPlugin interface; Internal, JUCE [VST3/AU/LV2], CLAP stub hosts), `settings`, `gui`.
- `tests` – unit tests.

## Usage
- Piano roll: click = add note (or chord in Chord Mode), drag = move, drag right edge = resize, right-click = delete,
  Ctrl/Shift-drag = rubber band, Del, Ctrl+A/C/X/V, arrows transpose/nudge, Ctrl+wheel zoom H, Alt+wheel zoom V.
- Timeline: click = set position, Shift/right-drag = loop range.
- Keys: Space play/stop, R record, L loop, C chord mode, Home return, Ctrl+Z/Y undo/redo, Ctrl+S/O/N.
- Default modifier keys: C1=1st inv, D1=2nd inv, C#1=7, D#1=9 (change/MIDI Learn in Options > Settings).
