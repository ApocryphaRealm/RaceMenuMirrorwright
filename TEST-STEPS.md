# RaceMenu Mirrorwright 1.0.0 - thirteenth check: R3 zoom, hair swatch

Build: `7. current test builds\Skyrim - RaceMenu Mirrorwright\RaceMenu Mirrorwright 1.0.0` (repackaged), Line-SE-AE16,
installed over `test RaceMenu Mirrorwright 1.0.0` inside the batch. RaceMenu + AMF on, Atelier and its patch off,
Apprentice on, throwaway save, camera facing the front. Set `uLogLevel = 1` (debug) in the installed
RaceMenuMirrorwright.ini for this run (back to 2 afterwards). Tool `mirrorwright.control`.
Passed (runs 1-12): zoom by op=closeup and Z with the label following; rows wrap inside their panels; grey unused tints.

1. Pad R3 (gamelink right-thumb press, 0x0080). EXPECTED: RaceMenu's zoom toggles as with Z; log `zoom from R3 (game
   input)` or `zoom from R3 (editor window (ImGui R3))` followed by the `zoom: ...handleInput(keyDown 56) called -> true`
   line. Also in the log at debug: `pad button 0x0080 value 1.00 ...`. Press it twice: zoom out, zoom in, one zoom per
   press. If nothing happens, send every `pad button` line - they show whether the press reaches the editor at all.
2. Hair Color swatch: the same colour RaceMenu's own list shows for that row (F4 to compare). Log `swatch Hair Color:
   the row's own colour #...... (as RaceMenu's list shows it)`. Screenshots of both interfaces.
3. Settings > Panel width 0.20: nothing runs off any panel; rows wrap. Screenshot. Put it back to 0.30.
4. Still for a real mouse when possible: drag turning; presets save / replace / load / delete; the stick with Racemenu
   Rotation with Gamepad Support NG not doubled.
5. No crash. Send the log and screenshots.
