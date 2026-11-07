# RaceMenu Mirrorwright 1.0.0 - tenth check: swatch back, close-up distance, scene panel layout

Build: `7. current test builds\Skyrim - RaceMenu Mirrorwright\RaceMenu Mirrorwright 1.0.0` (repackaged), Line-SE-AE16,
installed over `test RaceMenu Mirrorwright 1.0.0` inside the batch. RaceMenu + AMF on, Atelier and its patch off,
Apprentice on, throwaway save, camera facing the front. Tool `mirrorwright.control`.
Passed (runs 1-9): see run8\NOTES.md and run9; close-up on/off restores exactly; turning in close-up.

1. Hair: the Hair Color row has a small filled square in the hair's colour (blond for index 4). Log: `swatch Hair
   Color: hair colour form XXXXXXXX -> #RRGGBB` (or `no hair colour form ... stays`). Move Hair Color: the hair and,
   within about half a second, the square change. Eyes > Eye Shadow: square matches. Screenshot.
2. `op=closeup`. EXPECTED: the whole face from forehead to chin, eyes about the middle of the frame, no clipping;
   log `close-up on: camera (...) -> (...)` with the new camera about 50 units from the player's x/y and 9 higher.
   Again: back exactly. Screenshots. If too near or far, note it (the INI's fCloseUpDistance / fCloseUpRaise tune it).
3. Scene panel: Light and Close-up on the first row, Turn left and Turn right on the second; nothing cut off at the right
   edge. Screenshot.
4. Still for a real mouse when possible: mouse drag turning; presets save / replace / load / delete; the stick with
   Racemenu Rotation with Gamepad Support NG not doubled.
5. No crash. Send the log and screenshots.
