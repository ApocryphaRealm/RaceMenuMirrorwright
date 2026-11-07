# RaceMenu Mirrorwright 1.0.0 - ninth check: swatch colour, face close-up (Milestone 3), what is left

Build: `7. current test builds\Skyrim - RaceMenu Mirrorwright\RaceMenu Mirrorwright 1.0.0` (repackaged), Line-SE-AE16,
installed over `test RaceMenu Mirrorwright 1.0.0` inside the batch. RaceMenu + AMF on, Atelier and its patch off,
Apprentice on, throwaway save, camera facing the front. Tool `mirrorwright.control`.
Passed (runs 1-8): everything listed in run8\NOTES.md plus the earlier runs.

## A. Fixes and the close-up

1. Hair: the Hair Color row's square shows the hair's colour on screen (red-orange for index 5, not near-black). Move
   the Hair Color slider: EXPECTED: the hair changes and, within about half a second, the square takes the new colour.
   Eyes > Eye Shadow (a tint row): its square matches the eye shadow colour. Screenshot.
2. `op=closeup` (or Z, or the pad's R3, or the Close-up button in the scene panel). EXPECTED: the camera moves in to
   about half the distance, onto the face; log `close-up on: camera (x, y, z) -> (x', y', z')`. Again: back to where it
   was (`close-up off`). Screenshots of both. If the camera does not move but the log line is there, report it (then
   SetRaceSexCameraPos wants another argument shape). If the game crashes, send the crash log.
3. Close-up on, then turn with E: the face turns in close-up. Close the menu and reopen: the camera starts normal.

## B. Still untested (real mouse needed)

4. Mouse drag on the open space over the character: smooth turn; a slider drag does not turn.
5. Presets: Save `MW Test` (listed; .jslot in Overwrite\SKSE\Plugins\CharGen\Presets), save again (replace question),
   Load (face returns; sliders follow after ~2 s?), Delete (gone).
6. With Racemenu Rotation with Gamepad Support NG on: the stick turn is not doubled.
7. No crash. Send the log and screenshots.
