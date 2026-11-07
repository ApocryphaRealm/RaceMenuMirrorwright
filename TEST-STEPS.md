# RaceMenu Mirrorwright 1.0.0 - short check after the 2026-11-06 20:26 run (fixes + Milestone 3 controls)

Build: `7. current test builds\Skyrim - RaceMenu Mirrorwright\RaceMenu Mirrorwright 1.0.0` (repackaged), **Line-SE-AE16**,
into `test RaceMenu Mirrorwright 1.0.0` with install-test-build.py. RaceMenu and AMF on; RaceMenu Atelier and its patch
off. A throwaway save. Log `SKSE\RaceMenuMirrorwright.log`, tool `mirrorwright.control`.
The first run's evidence: `4. plans\racemenu-rebuild\test-run-20261106\`.

## A. What the first run found wrong (each must now pass)

1. `showracemenu` (console open), then close the console with ~. EXPECTED: the editor appears only after the console
   closes (log `RaceMenu opened under the console; the editor waits` then `editor shown (console closed)`).
2. Log: `probe N: ... races, ... sliders, ... categories` with **no** `node limit reached`, and no warning that
   `...racePanel.itemList._entryList` or `...categoryList._entryList` is missing.
3. Rail. EXPECTED: names from RaceMenu's own category list in RaceMenu's order (Race, Body, Head, Face, Eyes, Brow,
   Mouth, Hair, ... - shown without the `$`, e.g. "Head"), no "$ALL", no "Choices <number>" entries (with Apprentice:
   its categories under their own names), then Discovery. EXPECTED: the rail does not grow when RaceMenu is read again
   (Discovery > Read RaceMenu again, twice).
4. Race. EXPECTED: race tiles only, **no empty tiles** after the last race.
5. `mirrorwright.control op=category name=Head`. EXPECTED: the rail moves to Head and the content shows Head sliders.
   EXPECTED: `bridge.model.changed` is **0** (no slider touched). Send the state JSON.
6. `op=model count=40`. EXPECTED: each slider has a different `key` (e.g. `ChangeDoubleMorph|12`); send the JSON - it
   shows whether the entries have a `sliderID`.

## B. Edits (not tried in the first run)

7. Head: drag one slider. EXPECTED: the face changes live, the slider gets a `*`, changed = 1. Log at trace level
   (`uLogLevel = 0`) shows `slider <label> (<callback>) = <value>`. If the face does NOT change: report it, and send
   `op=model count=40` - the game handler is called as GameDelegate.call(callback, [value, sliderID]).
   If the game crashes here, set `bEngineSliderCalls = 0` in the INI and report the crash log.
8. Ctrl+Z / Ctrl+Y: the slider goes back / comes back. Reset (confirm): changed = 0.
9. Race: click another race. EXPECTED: the race changes; log `choose: ...[i] through <owner>.onItemPress`.
10. Presets: Save `MW Test`, Refresh (listed, file in Overwrite\SKSE\Plugins\CharGen\Presets), Load it (character
    returns), Delete it (file gone). Log `CharGen.SavePreset` / `LoadPreset ... dispatched`.

## C. Milestone 3 controls

11. Keyboard (not typing in a box): Q / E turn the character; log once `turn: rotation A -> B (first turn ...)`. If
    instead `turn: CharGen.GetPlayerRotation gave no number`, report it. Scene panel Turn left / Turn right: 15 degrees.
12. L toggles RaceMenu's light (log `light: RSM_ToggleLight sent`).
13. Pad (if available): right stick turns; D-pad moves through rail and content; X opens the Done question.
14. Done (button, R, or X): "Finish character creation?" with Finish focused; confirm. EXPECTED: log
    `Done: GameDelegate.call("ConfirmDone")` and RaceMenu finishes as with its own Done (the game's next step, e.g.
    the name box, follows). Report what happened after it.
15. F4 twice: editor off (RaceMenu's own panels back), editor on (panels hidden again).
16. No crash; send the log and a screenshot of the Head category.
