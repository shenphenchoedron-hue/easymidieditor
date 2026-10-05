#!/usr/bin/env bash
# Packages the Release build as a .deb (Debian/Ubuntu).
# Usage: scripts/make-deb.sh <version> <output-dir>
set -euo pipefail
VERSION="${1#v}"
OUT="${2:-dist}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="$ROOT/build/src/app/MidiComposer_artefacts/Release/MIDI Composer"
ARCH="$(dpkg --print-architecture)"
PKG="$(mktemp -d)/midi-composer"

install -Dm755 "$BIN" "$PKG/usr/lib/midi-composer/midi-composer"
mkdir -p "$PKG/usr/bin"
ln -s ../lib/midi-composer/midi-composer "$PKG/usr/bin/midi-composer"
install -Dm644 "$ROOT/resources/icon.png" "$PKG/usr/share/icons/hicolor/512x512/apps/midi-composer.png"
install -Dm644 "$ROOT/LICENSE" "$PKG/usr/share/doc/midi-composer/copyright"

mkdir -p "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/midi-composer.desktop" <<DESK
[Desktop Entry]
Type=Application
Name=MIDI Composer
Comment=MIDI composition with scale-based chord input
Exec=midi-composer %f
Icon=midi-composer
Terminal=false
Categories=AudioVideo;Audio;Midi;Music;
MimeType=audio/midi;audio/x-midi;
DESK

mkdir -p "$PKG/DEBIAN"
cat > "$PKG/DEBIAN/control" <<CTRL
Package: midi-composer
Version: $VERSION
Section: sound
Priority: optional
Architecture: $ARCH
Depends: libasound2 | libasound2t64, libfreetype6, libfontconfig1, libx11-6, libxext6, libxrandr2, libxinerama1, libxcursor1, libgtk-3-0 | libgtk-3-0t64
Maintainer: shenphenchoedron-hue <shenphenchoedron-hue@users.noreply.github.com>
Homepage: https://github.com/shenphenchoedron-hue/easymidieditor
Description: MIDI composer with scale-based chord input
 Multi-track MIDI editor with piano roll, MIDI keyboard input, recording,
 instrument plugins (VST3/LV2) and diatonic chord input with inversions
 and 7th/9th chords.
CTRL

mkdir -p "$OUT"
dpkg-deb --build --root-owner-group "$PKG" "$OUT/midi-composer_${VERSION}_${ARCH}.deb"
