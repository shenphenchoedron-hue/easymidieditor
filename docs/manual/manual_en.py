"""English user manual content. Rendered by make_manual.py (helpers are passed in as `m`).
Keep in sync with content_da() in make_manual.py."""


def content_en(m):
    h1, h2, p, bullets, steps, code, tip, table = m.h1, m.h2, m.p, m.bullets, m.steps, m.code, m.tip, m.table
    story, st, mm = m.story, m.st, m.mm
    m.TOC_TITLE[0] = "Contents"

    # ---- cover
    story.append(m.Spacer(1, 45 * mm))
    if m.os.path.exists(m.ICON):
        story.append(m.Image(m.ICON, width=38 * mm, height=38 * mm))
    story.append(m.Spacer(1, 10 * mm))
    story.append(m.Paragraph("MIDI Composer", st["title"]))
    story.append(m.Spacer(1, 4 * mm))
    story.append(m.Paragraph("User Manual", st["subtitle"]))
    story.append(m.Spacer(1, 3 * mm))
    story.append(m.Paragraph("Easy Midi Composer – compose with chords, scales and built-in instruments", st["subtitle"]))
    story.append(m.PageBreak())

    # ---- toc
    story.append(m.Paragraph(m.TOC_TITLE[0], st["h1"]))
    toc = m.TableOfContents()
    toc.levelStyles = [st["toc1"]]
    toc.dotsMinLevel = 0
    story.append(toc)
    story.append(m.PageBreak())

    # ---- 1
    h1("1. Welcome")
    p("MIDI Composer is a lightweight program for writing music with MIDI – ideas, chord progressions and complete "
      "arrangements – without having to set up a big, complicated DAW project. You can record from a MIDI keyboard, "
      "draw notes with the mouse, let the program build chords from a scale, and hear everything with the built-in "
      "instruments or your own plugins.")
    p("When you are done, you can export a MIDI file that every music program can open, or MusicXML to view and print "
      "the score in, for example, MuseScore.")
    h2("What it can do")
    bullets([
        "Multiple tracks, each with its own instrument, colour, volume, pan and MIDI channel.",
        "A piano roll for drawing, moving, lengthening, copying and deleting notes.",
        "<b>Chord mode</b>: one key plays a whole chord in the selected scale – with inversions, 7ths and 9ths.",
        "Next-chord suggestions based on the circle of fifths.",
        "Recording from a MIDI keyboard, also in a loop.",
        "21 built-in band instruments (Rock, Pop, Jazz) – plus your own SoundFont files (.sf2).",
        "Plugins: VST3, Audio Unit (Mac), LV2 (Linux) and CLAP.",
        "Import and export of MIDI and MusicXML (MuseScore, Sibelius, Finale, Dorico).",
        "Undo/redo for everything.",
    ])
    p("Buttons and menus are written in <b>bold</b> in this manual, exactly as they appear in the program. A Danish "
      "version of this manual can be downloaded from the project's Releases page on GitHub.")

    # ---- 2
    h1("2. Installation")
    h2("Mac")
    steps([
        "Unzip <b>MIDI-Composer-macOS-Universal.zip</b>. It runs on both Intel and Apple Silicon Macs.",
        "Move <b>MIDI Composer.app</b> to your <b>Applications</b> folder.",
        "The program is not signed by Apple, so macOS blocks it the first time. Open Terminal and run:",
    ])
    code('xattr -dr com.apple.quarantine "/Applications/MIDI Composer.app"')
    p("If macOS still says the program cannot be opened, or that it \"may harm your computer\", allow it in System "
      "Settings:")
    steps([
        "Try to open <b>MIDI Composer</b> once and close the warning (choose <b>OK</b> or <b>Done</b>, not "
        "<b>Move to Trash</b>).",
        "Open <b>System Settings → Privacy &amp; Security</b>.",
        "Scroll down to the <b>Security</b> section. It says that \"MIDI Composer\" was blocked. Click <b>Open Anyway</b>.",
        "Confirm with your password or Touch ID, then click <b>Open</b> in the next dialog.",
    ])
    p("You only need to do this once per version of the program. The <b>Open Anyway</b> button is only shown for about "
      "an hour after you tried to open the program – if you cannot see it, try opening the program again first.")
    tip("On older macOS versions (14 and earlier) you can instead right-click the program, choose <b>Open</b> and "
        "confirm. From macOS 15 on this no longer works – use System Settings as described above.")
    h2("Windows")
    p("Unzip <b>MIDI-Composer-Windows-x64.zip</b> into a folder and start <b>MIDI Composer.exe</b>. The file "
      "<b>GeneralUser-GS.sf2</b> must stay in the same folder as the program – it contains the built-in instruments.")
    h2("Debian and Ubuntu")
    code("sudo apt install ./midi-composer_<version>_amd64.deb")
    p("The program then appears in the application menu as <b>MIDI Composer</b>, or can be started with the command "
      "<b>midi-composer</b>.")

    # ---- 3
    h1("3. First start")
    steps([
        "The first time, the program asks whether to <b>scan for instrument plugins</b>. If you have plugins installed "
        "(e.g. Kontakt), choose <b>Scan</b>. You can always do it later via <b>Options → Rescan plugins</b>.",
        "Open <b>Options → Settings (audio, MIDI, modifier keys)</b> and choose your audio device and MIDI keyboard "
        "(see chapter 13).",
        "A new project has one track with <b>Grand Piano</b>. Play your keyboard, or click in the piano roll to add notes.",
    ])
    tip("No sound? Check that the right audio device is selected in Settings, that the track is not muted (M), and "
        "that the track volume is not all the way down.")

    # ---- 4
    h1("4. The window")
    p("The window has these parts, from top to bottom:")
    table(["Part", "What it does"], [
        ["Menu bar", "<b>File</b> (projects, import/export), <b>Edit</b> (undo, copy, tracks) and <b>Options</b> "
                     "(settings, plugins, sounds, manual, about). On a Mac the menu is at the top of the screen."],
        ["Transport bar", "Play, stop, record, loop, tempo, time signature, note grid and undo."],
        ["Chord panel", "Root, scale, chord mode, modifier buttons, the current chord and next-chord suggestions."],
        ["Track list (left)", "All tracks with name, colour, mute/solo, volume, pan, channel and instrument."],
        ["Zoom bar", "Above the piano roll: the <b>Zoom</b> buttons (chapter 8)."],
        ["Piano roll (right)", "The notes of the active track, with the timeline on top, the keyboard on the left and "
                               "the velocity lane at the bottom."],
        ["Step sequencers (bottom)", "Every step sequencer track, below the piano roll and always visible: the setup card "
                                     "on the left, the step rows aligned with the piano roll grid (chapter 9)."],
    ], [38, 127])

    # ---- 5
    h1("5. Transport, tempo and loop")
    table(["Button / field", "Function"], [
        ["<b>|&lt;</b> (Return to start)", "Jumps to the start – or to the loop start if loop is on. The piano roll follows."],
        ["<b>Play</b>", "Starts playback from the playhead."],
        ["<b>Stop</b>", "Stops. Press <b>Stop</b> again while stopped to jump back to the start."],
        ["<b>Rec</b>", "Starts recording on the active track (see chapter 11)."],
        ["<b>Loop</b>", "Turns the loop on/off. Start and end bar are set in the <b>Loop bars</b> fields."],
        ["Position display", "Shows bar, beat and time of the playhead."],
        ["<b>BPM</b>", "Tempo in beats per minute."],
        ["<b>Sig</b>", "Time signature, e.g. 4/4, 3/4 or 6/8."],
        ["<b>Grid</b>", "Note grid: 1/4, 1/8, 1/16, 1/32 and triplets (1/8 T, 1/16 T)."],
        ["<b>Snap</b>", "When on, new and moved notes snap to the grid."],
        ["<b>Undo / Redo</b>", "Undo and redo."],
    ], [45, 120])
    h2("The timeline")
    bullets([
        "<b>Click</b> in the timeline above the piano roll to move the playhead.",
        "<b>Shift+drag</b> or <b>right-drag</b> in the timeline to set a loop range.",
    ])

    # ---- 6
    h1("6. Tracks")
    h2("Adding and deleting")
    p("Click <b>+ Track</b> and choose <b>Piano roll</b> or <b>Step sequencer</b> (also in the Edit menu). "
      "<b>- Track</b> or <b>Edit → Delete active track</b> deletes the active piano roll track. Click a track in the track list to make it "
      "active – that is the track you see and edit in the piano roll, and the one your MIDI keyboard plays.")
    h2("Track settings")
    table(["Element", "Function"], [
        ["Colour button", "The small square next to the name. Opens the colour picker (see below)."],
        ["Name", "Double-click and type to rename the track."],
        ["<b>M</b> / <b>S</b>", "Mute (silence the track) and Solo (hear only this track)."],
        ["Channel", "MIDI channel 1–16. Used for export and by plugins that listen on specific channels."],
        ["Volume / Pan", "The sliders for volume and panning (left/right)."],
        ["Instrument button", "Shows the instrument's name. Click it to choose an instrument (see chapter 7)."],
        ["<b>E</b> (editor)", "Opens the plugin's own window, if it has one (e.g. Kontakt)."],
    ], [38, 127])
    h2("Colours")
    p("Click a track's colour button to open the <b>colour picker</b>:")
    bullets([
        "Drag in the colour field and sliders, or type a hex value. The track changes colour immediately.",
        "<b>Presets</b>: ten fixed colours.",
        "<b>Saved</b>: your own swatches. Click <b>Save swatch</b> to store the current colour. They are kept in your "
        "settings and can be used in every project. Right-click a swatch to remove it.",
        "<b>Default</b> resets to the default colour.",
        "Click outside the box to close it. The colour change can be undone in one step.",
    ])
    p("The active track is shown in a strong colour – both in the track list and in the piano roll. The other tracks "
      "are muted, and their notes shine faintly through the piano roll in their own colour, like looking through "
      "tracing paper. You can see them, but not edit them.")

    # ---- 7
    h1("7. Instruments")
    p("Click a track's instrument button. The menu has these groups:")
    table(["Menu", "Contents"], [
        ["<b>Internal → Rock / Pop / Jazz</b>", "The built-in band instruments (table below)."],
        ["<b>Internal → Basic Synth</b>", "A simple, lightweight synth without samples."],
        ["<b>SoundFonts</b>", "Your own .sf2 files – one sub-menu per file – and <b>Open Sounds folder</b>."],
        ["<b>VST3, AudioUnit, LV2, CLAP</b>", "Your installed plugins, once scanned."],
        ["<b>Browse plugins...</b>", "A window with search and filtering by format."],
        ["<b>Bypass</b> / <b>Remove instrument</b>", "Temporarily disable the instrument, or remove it."],
    ], [55, 110])
    h2("Built-in instruments")
    table(["Rock", "Pop", "Jazz"], [
        ["Rock Drums", "Pop Drums", "Jazz Drums"],
        ["Distortion Guitar", "Grand Piano", "Brush Drums"],
        ["Overdrive Guitar", "Electric Piano", "Jazz Piano"],
        ["Clean Electric Guitar", "Acoustic Guitar", "Upright Bass"],
        ["Electric Bass (Pick)", "Ukulele", "Jazz Guitar"],
        ["Rock Organ", "Electric Bass (Finger)", "Tenor Sax"],
        ["", "Strings", "Trumpet"],
        ["", "", "Vibraphone"],
    ], [55, 55, 55])
    h2("Drum kits")
    p("The drum kits follow the General MIDI standard, so drum tracks from other programs fit directly. The most "
      "important keys:")
    table(["Key", "Sound", "Key", "Sound"], [
        ["C1", "Bass drum", "F♯1", "Closed hi-hat"],
        ["C♯1", "Side stick", "G♯1", "Pedal hi-hat"],
        ["D1", "Snare", "A♯1", "Open hi-hat"],
        ["D♯1", "Hand clap", "C♯2", "Crash cymbal"],
        ["E1", "Snare 2", "D♯2", "Ride cymbal"],
        ["F1, G1, A1, B1, C2, D2", "Toms (low → high)", "F2", "Ride bell"],
    ], [28, 54, 28, 55])
    tip("Turn chord mode off on drum tracks – otherwise one key becomes three drums. With the default modifier keys "
        "C1/D1 (see chapter 10), those keys are exactly the bass drum and snare.")
    h2("Your own sounds (SoundFonts)")
    steps([
        "Choose <b>Options → Open Sounds folder</b>. The folder opens in Finder/Explorer.",
        "Copy one or more <b>.sf2</b> files into the folder. Many free SoundFonts are available online.",
        "Choose <b>Options → Rescan plugins</b>.",
        "The sounds now appear under <b>SoundFonts</b> in the instrument menu, sorted by file name.",
    ])
    p("Projects remember which file and sound each track uses. If you move or delete the file, the track is shown as "
      "missing until the file is back.")
    h2("Plugins")
    p("The program can use VST3 plugins on all systems, Audio Units on Mac, LV2 on Linux, and CLAP. After installing "
      "new plugins: <b>Options → Rescan plugins</b>. If a plugin crashes during the scan, it is automatically "
      "blacklisted and skipped next time.")
    p("If a plugin used by a project is missing, the track shows <b>Plugin missing</b>. The notes are "
      "still there – choose another instrument, or reinstall the plugin.")

    # ---- 8
    h1("8. Piano roll")
    h2("Notes")
    table(["Action", "How"], [
        ["Add a note", "<b>Click</b> an empty spot. Keep the button down and drag right to set the length. "
                       "In chord mode a whole chord is inserted."],
        ["Move", "Drag a note. Up/down changes pitch, sideways changes time."],
        ["Change length", "Drag the note's <b>right edge</b> to change where it ends, or its <b>left edge</b> to change "
                          "where it starts. Over an edge the mouse pointer changes to a ↔ resize arrow."],
        ["Select", "Click a note. <b>Shift+click</b> adds to/removes from the selection."],
        ["Select several", "<b>⌘/Ctrl+drag</b> or <b>Shift+drag</b> on an empty spot draws a selection box. "
                           "<b>⌘/Ctrl+A</b> selects everything."],
        ["Delete one note", "<b>Right-click</b> the note."],
        ["Delete many", "Hold the <b>right mouse button</b> and drag a <b>red box</b> over the notes. Every note the "
                        "box touches is deleted when you release. Or select them and press <b>Delete</b>/<b>Backspace</b>."],
        ["Transpose", "<b>↑ / ↓</b> moves selected notes by a semitone. <b>Shift+↑ / ↓</b> moves them an octave."],
        ["Copy / cut / paste", "<b>⌘/Ctrl+C</b>, <b>⌘/Ctrl+X</b>, <b>⌘/Ctrl+V</b>. Pastes at the playhead."],
    ], [38, 127])
    p("On a Mac trackpad, a right-click is a two-finger click or Ctrl+click.")
    h2("Velocity")
    p("The lane at the bottom shows each note's velocity as a vertical line. Click and drag across the lines to change "
      "it. If some notes are selected, only the selected notes change. Soft notes are drawn slightly darker in the "
      "piano roll.")
    h2("Zoom and scroll")
    table(["Mouse / trackpad", "Effect"], [
        ["Scroll", "Scroll up/down (and sideways with a trackpad)."],
        ["Shift+scroll", "Scroll horizontally in time."],
        ["⌘/Ctrl+scroll", "Zoom in/out in time."],
        ["Alt/⌥+scroll", "Zoom vertically (taller/shorter rows)."],
        ["Scroll in the timeline", "Zoom in time."],
        ["<b>Zoom out / Zoom in</b>", "Toolbar buttons: zoom in time. The <b>+</b> and <b>-</b> keys do the same."],
        ["<b>Lower / Taller</b>", "Toolbar buttons: smaller or taller note rows. The keyboard follows the rows."],
    ], [45, 120])
    h2("The keyboard")
    p("Click the keys on the left to hear the notes. The notes of the selected scale are highlighted as lighter rows "
      "in the piano roll, so you can easily see which notes fit.")

    # ---- 9
    h1("9. Step sequencer")
    p("The step sequencer is a quick way to build drum beats and other repeating patterns. A step sequencer track is "
      "built from <b>lines</b>. Each line plays one note with its own instrument, e.g. kick, snare and hi-hat, or a "
      "bass note on a bass instrument.")
    p("All step sequencers are shown <b>below the piano roll</b> and are always visible. Their step rows follow the "
      "piano roll grid – same bars, scroll and zoom – so you can see exactly where the rhythm falls under your notes. "
      "The piano roll keeps its size: the first step sequencer is shown just below it, and further step sequencers "
      "follow underneath – use the scrollbar on the far right of the window to scroll down to them.")
    h2("Getting started")
    steps([
        "Click <b>+ Track</b> in the track list and choose <b>Step sequencer</b> (or <b>Edit → Add step sequencer "
        "track</b>). It appears below the piano roll with a <b>Kick</b> line on Pop Drums.",
        "Click <b>+ Line</b> on its card to add more lines. New lines suggest Snare, Closed Hat, Open Hat, Clap and so "
        "on, and use the same instrument as the line above.",
        "Click the steps to switch them on. Press <b>Space</b> to play – the red playhead runs through the piano roll "
        "and the step rows together.",
    ])
    h2("The card")
    p("The setup of each step sequencer is on its card in the left column:")
    table(["Setting", "Effect"], [
        ["<b>▾ / ▸</b>", "Collapses the step sequencer to its top row (or expands it again)."],
        ["Colour dot", "Click to choose the colour of the step sequencer – handy when you have several."],
        ["<b>+ Line</b>", "Adds a line."],
        ["<b>X</b> (top)", "Deletes the whole step sequencer with all lines (can be undone)."],
        ["<b>Step</b>", "Length of each step: 1/4, 1/8, 1/16, 1/32 or triplets. With 1/16 a 4/4 bar has 16 steps. "
                        "Changing it keeps your rhythm where possible."],
        ["<b>Bars</b>", "Length of the step sequencer in bars. <b>Every bar has its own steps</b> – change one bar "
                        "without affecting the others. New bars start empty."],
        ["<b>Start</b>", "The bar of the song where the step sequencer starts."],
    ], [38, 127])
    h2("Working with bars")
    p("The strip above the step rows shows each bar (<b>Bar 1</b>, <b>Bar 2</b> ...). <b>Right-click</b> a bar there "
      "for these commands (they apply to all lines):")
    table(["Command", "Effect"], [
        ["<b>Copy to next bar</b>", "Copies the bar onto the following bar – a quick start for a variation."],
        ["<b>Duplicate bar</b>", "Inserts a copy of the bar right after it."],
        ["<b>Add empty bar after</b>", "Inserts an empty bar after it."],
        ["<b>Clear bar</b>", "Switches off all steps in the bar."],
        ["<b>Delete bar</b>", "Removes the bar; the following bars move left."],
    ], [45, 120])
    h2("A line")
    table(["Part", "What it does"], [
        ["Name", "Double-click to rename the line."],
        ["Instrument button", "Choose the line's instrument – the same menu as in the track list (chapter 7)."],
        ["<b>M</b> / <b>X</b>", "Mute the line / delete the line (can be undone)."],
        ["Note", "Under the piano keyboard: the note the line plays. For drum kits: C2 kick, D2 snare, F♯2 closed "
                 "hi-hat, A♯2 open hi-hat."],
        ["Steps", "<b>Click</b> = on/off. <b>Shift+click</b> = accent (louder). <b>Right-click</b> = off. "
                  "<b>Drag</b> across several steps to switch them all on or off."],
    ], [38, 127])
    tip("The notes are generated from the pattern, so they play and export to MIDI like any other notes. "
        "<b>⌘/Ctrl+scroll</b> over the step rows zooms the time axis, <b>Shift+scroll</b> scrolls it – the piano roll follows.")

    # ---- 10
    h1("10. Chord mode")
    p("In chord mode, every key – on the MIDI keyboard or in the piano roll – plays a whole chord that fits the "
      "selected scale. Play C in C major and you get a C major chord. Play D and you get D minor, and so on.")
    h2("Turning it on")
    steps([
        "Choose the root in <b>Root</b> and the scale in <b>Scale</b> (e.g. A Natural Minor).",
        "Click <b>Chord Mode</b> so it shows ON – or press <b>C</b> on the computer keyboard.",
        "Play or click a note in the scale. Notes outside the scale are played as single notes.",
    ])
    p("Scales: Major, Natural Minor, Harmonic Minor, Melodic Minor, Dorian, Phrygian, Lydian, Mixolydian and Locrian.")
    h2("Modifiers: inversions, 7ths and 9ths")
    p("The modifier buttons in the chord panel change the chord. You can click the buttons (they stay on), or hold the "
      "corresponding keys on your keyboard:")
    table(["Modifier", "Default key", "Effect"], [
        ["<b>1st inversion</b>", "C1", "The top note moves down below the note you play."],
        ["<b>2nd inversion</b>", "D1", "The two top notes move down below the note you play."],
        ["<b>7</b>", "C♯1", "Adds the seventh (e.g. Am → Am7, G → G7)."],
        ["<b>9</b>", "D♯1", "Adds the seventh and ninth (e.g. Am → Am9)."],
    ], [35, 32, 98])
    p("The note you play always stays where you put it. Example with C major played from C3:")
    table(["", "Notes"], [
        ["Root position", "C3 – E3 – G3"],
        ["1st inversion", "G2 – C3 – E3"],
        ["2nd inversion", "E2 – G2 – C3"],
    ], [45, 120])
    p("The modifier keys do not make a sound themselves in chord mode. You can choose other keys in Settings "
      "(chapter 13). The current chord is shown in the chord panel, e.g. <b>Am7 – 1st inversion</b>.")
    h2("Next-chord suggestions")
    p("After you play a chord, the chord panel shows up to four suggestions next to <b>Next</b>. Click a suggestion to "
      "hear it. Hover over it to see why it is suggested. The suggestions follow the circle of fifths within the "
      "scale, strongest first:")
    table(["Priority", "Root movement", "Example in C major"], [
        ["1", "Down a fifth (strongest resolution)", "G → C, Dm → G"],
        ["2", "Up a fifth", "C → G"],
        ["3", "Down a third (two common notes)", "C → Am"],
        ["4", "Back to the tonic", "→ C"],
        ["5", "One step up", "C → Dm"],
    ], [22, 78, 65])

    # ---- 10
    h1("11. Recording")
    steps([
        "Select the track you want to record on.",
        "Press <b>Rec</b> (or <b>R</b>). Playback starts, and what you play is shown in red.",
        "Press <b>Stop</b> (or <b>space</b>). The notes are added to the track in one step, which can be undone.",
    ])
    p("If <b>Loop</b> is on, recording keeps going around the loop, and everything you play on each pass is kept. That "
      "way you can build up layers, e.g. first the bass and then the chords. Chord mode and modifiers also work while "
      "recording.")

    # ---- 11
    h1("12. Files, import and export")
    table(["Menu", "Function"], [
        ["<b>File → New / Open / Save / Save as</b>", "Projects are saved as <b>.mcproj</b>. Tracks, notes, instruments "
                                                        "with their settings, colours, tempo and loop are included."],
        ["<b>File → Import MIDI file...</b>", "Adds the tracks of a .mid file to the project."],
        ["<b>File → Export MIDI file...</b>", "Saves all tracks as a standard MIDI file that any music program can open."],
        ["<b>File → Import MusicXML (MuseScore)...</b>", "Loads .musicxml, .xml or .mxl. Each part becomes a track."],
        ["<b>File → Export MusicXML (MuseScore)...</b>", "Saves a score that opens in MuseScore, Sibelius, Finale and Dorico."],
    ], [62, 103])
    h2("MusicXML – to and from sheet music")
    p("<b>Export</b>: the key (from Root/Scale), time signature and tempo are included. Notes are rounded to the nearest "
      "1/16, notes across bar lines are tied, and chords are written as chords. Piano tracks with a wide range get a "
      "grand staff.")
    p("<b>Import</b>: notes, chords, rests, ties, pickup bars and multiple voices are included, and the program sets "
      "Root/Scale from the score's key signature. Repeats are not unrolled, and grace notes and dynamics are skipped.")
    p("In MuseScore, use <b>File → Export → MusicXML</b> (not the normal .mscz file) to bring the score into MIDI Composer.")

    # ---- 12
    h1("13. Settings")
    p("<b>Options → Settings (audio, MIDI, modifier keys)</b> contains:")
    bullets([
        "<b>Audio device</b>: output, sample rate and <b>buffer size</b>. A small buffer (128–256 samples) gives a short "
        "delay when you play. If you hear crackles, choose a larger one.",
        "<b>MIDI input</b>: your MIDI keyboard. The program reconnects automatically if the keyboard is unplugged and "
        "plugged in again.",
        "<b>Chord modifier keys</b>: which keys act as 1st inversion, 2nd inversion, 7 and 9. Click <b>MIDI Learn</b> "
        "and press the key you want to use. <b>Defaults</b> restores the default keys C1, D1, C♯1 and D♯1.",
    ])
    p("<b>Options → User manual</b> opens this manual. <b>Options → About MIDI Composer</b> shows the version and credits.")

    # ---- 13
    h1("14. Keyboard shortcuts")
    table(["Key", "Function"], [
        ["Space", "Play / stop"],
        ["Home", "To start (or loop start)"],
        ["R", "Record on/off"],
        ["L", "Loop on/off"],
        ["C", "Chord mode on/off"],
        ["+ / -", "Zoom in / out in time (piano roll)"],
        ["⌘/Ctrl+Z", "Undo"],
        ["⌘/Ctrl+Shift+Z or ⌘/Ctrl+Y", "Redo"],
        ["⌘/Ctrl+N / O / S", "New project / Open / Save"],
        ["⌘/Ctrl+Shift+S", "Save as"],
        ["⌘/Ctrl+A", "Select all notes"],
        ["⌘/Ctrl+C / X / V", "Copy / cut / paste notes"],
        ["Delete / Backspace", "Delete selected notes"],
        ["↑ / ↓", "Transpose selected notes by a semitone"],
        ["Shift+↑ / ↓", "Transpose selected notes by an octave"],
    ], [65, 100])
    p("⌘ applies on Mac, Ctrl on Windows and Linux.")

    # ---- 14
    h1("15. Troubleshooting")
    table(["Problem", "Solution"], [
        ["No sound", "Check the audio device in Settings, mute/solo and volume on the track, and that the track has an instrument."],
        ["The sound comes late", "Choose a smaller buffer size in Settings (e.g. 256 or 128 samples)."],
        ["The sound crackles", "Choose a larger buffer size, or use fewer heavy plugins at once."],
        ["The MIDI keyboard does nothing", "Select it under MIDI input in Settings. Check that the right track is active."],
        ["One key plays three notes", "Chord mode is on – press <b>C</b> or click <b>Chord Mode</b>."],
        ["A key plays no sound", "It is set as a modifier key in chord mode. Turn chord mode off, or choose other "
                                 "modifier keys in Settings."],
        ["A plugin is missing from the list", "Run <b>Options → Rescan plugins</b>. Plugins that crashed during the scan are skipped."],
        ["My own .sf2 sounds do not appear", "The file must be in the Sounds folder and end in .sf2. Then run <b>Rescan plugins</b>."],
        ["Mac: \"cannot be opened\"", "Run the command from chapter 2 (xattr), or go to <b>System Settings → Privacy "
                                      "&amp; Security</b> and click <b>Open Anyway</b> (see chapter 2)."],
        ["The built-in instruments are missing", "The file GeneralUser-GS.sf2 must be next to the program "
                                                 "(Windows/Linux) or inside the app (Mac). Reinstall the program."],
    ], [55, 110])

    # ---- 15
    h1("16. Credits and licenses")
    bullets([
        "<b>GeneralUser GS</b> – the built-in instruments – by S. Christian Collins. "
        "https://www.schristiancollins.com/generaluser. The license is included as GeneralUser-GS-LICENSE.txt.",
        "<b>TinySoundFont</b> – SoundFont playback – by Bernhard Schelling (MIT license). "
        "https://github.com/schellingb/TinySoundFont",
        "<b>JUCE</b> – the framework MIDI Composer is built with. https://juce.com",
        "Source code: https://github.com/shenphenchoedron-hue/easymidieditor",
    ])
