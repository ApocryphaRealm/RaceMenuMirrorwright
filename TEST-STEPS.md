# RaceMenu Mirrorwright 1.0.0 - eleventh check: swatches, close-up moving in

Build: `7. current test builds\Skyrim - RaceMenu Mirrorwright\RaceMenu Mirrorwright 1.0.0` (repackaged), Line-SE-AE16,
installed over `test RaceMenu Mirrorwright 1.0.0` inside the batch. RaceMenu + AMF on, Atelier and its patch off,
Apprentice on, throwaway save, camera facing the front. Tool `mirrorwright.control`.
Passed (runs 1-10): see run8\NOTES.md, run9, run10 (scene panel rows fit).

Before starting: if the installed RaceMenuMirrorwright.ini has `fCloseUpDistance` / `fCloseUpRaise` lines, set them to
28.00 and 0.00 (or delete them) - the compiled defaults changed.

1. Log after `showracemenu`: one `swatch row <name> (<callback>): fillColor=... tintType=... colourCallback=... -> swatch
   true|false` line per colour row (Skin Tone, Hair Color, Eye Shadow, Lip Color, ...). EXPECTED: `-> swatch true` for
   them. Send those lines.
2. Colors and Hair: each colour row has a small filled square in its colour. EXPECTED: Hair Color matches the hair
   (blond for index 4); move it - the square follows within about half a second. Screenshot.
3. `op=closeup`. EXPECTED: log `close-up: N units from the head point before, M after` with M smaller than N (about 26 for
   N = 44), then `close-up on: ...`. On screen the face is LARGER and still centred. Screenshot. Again: back exactly.
4. Close-up on, turn with the right stick or E: the face turns and stays framed.
5. Still for a real mouse when possible: mouse drag turning; presets save / replace / load / delete; the stick with
   Racemenu Rotation with Gamepad Support NG not doubled.
6. No crash. Send the log and screenshots.
