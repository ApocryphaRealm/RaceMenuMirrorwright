# RaceMenu Mirrorwright 1.0.0 - fourteenth check: R3 by XInput, focus style, text fitting, F4

Build: `7. current test builds\Skyrim - RaceMenu Mirrorwright\RaceMenu Mirrorwright 1.0.0` (repackaged), Line-SE-AE16,
installed over `test RaceMenu Mirrorwright 1.0.0` inside the batch. RaceMenu + AMF on, Atelier and its patch off,
Apprentice on, throwaway save, camera facing the front. Keep `uLogLevel = 1`. Tool `mirrorwright.control`.
Passed (runs 1-13): zoom by op=closeup and Z; rows wrap; grey unused tints; pad A / D-pad reach ImGui.

1. Log at the first RaceMenu frame: `R3: reading the pad through xinput1_?.dll (already loaded)` (or `no XInput DLL is
   loaded`). Then pad R3 (gamelink right thumb). EXPECTED: `R3 down (XInput buttons 0x0080)`, `zoom from R3 (XInput
   poll)`, `zoom: ...handleInput(keyDown 56) called -> true`; the zoom toggles. Twice: out, then in - one per press.
2. Pad navigation onto a button (A / D-pad as in run 13): the focused button is filled white with dark text. EXPECTED: no
   blue rectangle. If a blue (or theme-coloured) frame is still drawn round it, report it with a screenshot - that frame
   comes from the menu framework itself (AMF draws its own highlight frame round the pad-focused item), not from this mod.
3. With pad focus on a button, press F4 (real keyboard if possible; bench.input as well). EXPECTED: the editor switches to
   RaceMenu's own interface; log `editor hidden (keyboard (OS))` or `(editor window)`. F4 again brings it back.
4. Settings > Panel width 0.20, `showracemenu`: the rail shows icons only (hover: the name); slider labels too long for
   the content end in "..." with the full name as a tooltip; the search hint is shortened with "..."; nothing is cut off
   mid-word. Preset names in the scene panel likewise. Screenshot, then back to 0.30.
5. Hair: the Hair Color row shows no square (no colour on the row) unless RaceMenu filled one in; log `swatch Hair Color:
   no colour on the row - no square` or `the row's own colour #......`. Other colour rows keep their squares.
6. Still for a real mouse when possible: drag turning; presets save / replace / load / delete.
7. No crash. Send the log and screenshots.
