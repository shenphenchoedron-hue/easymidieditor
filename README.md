# Easy MIDI Composer

**Easy Midi Composer** is a lightweight, cross-platform MIDI composition app built with C++20, JUCE 8 and CMake.

It is designed for quickly creating MIDI files, musical ideas and complete MIDI compositions without having to open a large and complex DAW.

The idea is simple: compose and edit MIDI in a focused environment, export the result as a standard MIDI file, and then import it into your DAW for instrumentation, production, mixing and further arrangement.

Easy Midi Composer is not intended to replace a DAW. Instead, it provides a fast and uncomplicated workspace for the part that often comes before the DAW: writing the music.

The application includes a piano-roll editor, scale-aware composition tools and a Chord Input Mode designed to make melodies, harmonies and chord progressions quick to create.

## Why Easy Midi Composer?

Sometimes you just want to create a melody, chord progression or MIDI arrangement without loading an entire production environment.

Easy Midi Composer focuses specifically on MIDI composition:

- Quickly sketch melodies and musical ideas.
- Create chords directly in the piano roll.
- Compose using scales and scale-aware chord tools.
- Build MIDI arrangements without configuring a full DAW project.
- Export standard MIDI files for use in your preferred DAW.
- Keep composition separate from sound design, mixing and production.

The goal is to make MIDI composition feel immediate: open the application, write the music, export the MIDI file, and continue working with it wherever you want.

**Open → Compose → Export MIDI → Import into DAW.**

## Built-in instruments
Easy Midi Composer makes sound without any plugins: the most common band
instruments are built in, grouped as **Rock**, **Pop** and **Jazz** under
*Internal* in each track's instrument menu (drums, guitars, basses, piano,
electric piano, organ, ukulele, strings, sax, trumpet, vibraphone).

Want more? Copy SoundFont files (`.sf2`) into the Sounds folder
(*Options → Open Sounds folder*), then *Options → Rescan plugins*. Every preset
appears under *SoundFonts*.

Credits: built-in sounds are **GeneralUser GS** by S. Christian Collins
(https://www.schristiancollins.com/generaluser, license shipped as
`GeneralUser-GS-LICENSE.txt`). SoundFont playback uses **TinySoundFont** by
Bernhard Schelling (MIT). The SoundFont is downloaded at configure time
(`-DMC_BUNDLE_SOUNDFONT=OFF` to skip).

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
