# RaceMenu Mirrorwright 1.0.0 - twelfth check: the game's zoom, rows inside their panels, swatch colours

Build: `7. current test builds\Skyrim - RaceMenu Mirrorwright\RaceMenu Mirrorwright 1.0.0` (repackaged), Line-SE-AE16,
installed over `test RaceMenu Mirrorwright 1.0.0` inside the batch. RaceMenu + AMF on, Atelier and its patch off,
Apprentice on, throwaway save, camera facing the front. Tool `mirrorwright.control`.
Passed (runs 1-11): see the earlier run folders; swatches appear on all 15 colour rows (run 11).

1. Zoom: `op=closeup` (or Z, R3, the scene panel's zoom button). EXPECTED: the camera does what RaceMenu's own Left Alt
   does (the bottom bar's "LAlt Zoom Out"): it zooms out; again: it zooms back in. Log `zoom:
   _root...RaceSexPanelsInstance.handleInput(keyDown 56) called -> true|false` (or the base clip). The button reads
   "Zoom out" while zoomed in and "Close-up" while zoomed out. Screenshots of both states. If the log says "not there"
   for both clips, the zoom did nothing - report it. No camera inside the head.
2. Rows inside their parents: the top of the left panel - search box, "N changed", "RaceMenu's interface" - nothing cut
   off at the right edge; if the button does not fit it moves to the next line (or shows "RaceMenu's i..." with the full
   text as a tooltip). Same for the footer (Undo / Redo / Reset / Done), the scene panel (Light / zoom / Turn left /
   Turn right), the preset buttons, and the settings page's Save / Reload / Restore. Narrow the editor (Settings > Panel
   width 0.20) and check again: rows wrap, nothing runs off. Screenshots at 0.30 and 0.20.
3. Swatches: Hair Color's square is close to the hair on screen (light blond, not dark olive); log `swatch Hair Color:
   hair colour form ... raw #...... shown #......`. Unused tints (Dirt, War Paint, Eyeliner ...) show a dim grey square,
   not white. Screenshot of the Colors category.
4. Still for a real mouse when possible: drag turning; presets save / replace / load / delete; the stick with Racemenu
   Rotation with Gamepad Support NG not doubled.
5. No crash. Send the log and screenshots.
