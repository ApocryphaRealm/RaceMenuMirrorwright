# RaceMenu Mirrorwright 1.0.0 - seventh check (combined): fixes from run 6, untested parts, data probes

Build: `7. current test builds\Skyrim - RaceMenu Mirrorwright\RaceMenu Mirrorwright 1.0.0` (repackaged), Line-SE-AE16,
installed over `test RaceMenu Mirrorwright 1.0.0` inside the batch. RaceMenu + AMF on, Atelier and its patch off,
Apprentice on, throwaway save, camera facing the front. Tool `mirrorwright.control`.
Passed (runs 1-6): names/order/icons, Head sliders, Scars/War Paint applied, E turning, undo of tool changes, filled
confirm boxes and their focus, the whole finish and name flow, race change by name (default route).

## A. Fixes from run 6

1. `op=choose name=Redguard`. EXPECTED: Redguard on screen; log `probe: new starting values after the race change`;
   state `bridge.model.changed` = **0**; Undo greyed (nothing to undo). The panel below the grid shows "Redguard" and
   its description (or "No description."). Screenshot.
2. Then `op=slider key="ChangeHeadPart|6" value=5`: changed = 1, Undo enabled; Undo: changed = 0.
3. `op=done`, Right arrow, then Esc: **no blue outline** anywhere (box, Light button, rail); the focused choice is
   only the white fill. Screenshot.

## B. Never run yet

4. Mouse drag on the open space over the character: smooth turn, stops on release; dragging a slider does not turn.
5. Pad right stick turns; with Racemenu Rotation with Gamepad Support NG on, not doubled. Q / E and the Turn buttons.
6. `op=done`, Enter, then **Cancel** on the game's finish box: back in RaceMenu's own interface; F4 brings the editor
   back.
7. Presets: Save `MW Test` (listed, .jslot in Overwrite\SKSE\Plugins\CharGen\Presets; saving again asks to replace),
   move a slider, Load `MW Test` (face returns; do the sliders show the loaded values after ~2 s?), Delete (gone).
8. Light (L / button): RaceMenu's light toggles.

## C. Data probes for Milestones 3-5 - just run them and send the JSON back (nothing to judge)

9. `op=model count=5` - note the `i` of the first slider row (its index in the item list), then
   `op=members path=_root.RaceSexMenuBaseInstance.RaceSexPanelsInstance.racePanel.itemList._entryList.<i>` for that row
   and for one Body row and one Hair row.
10. `op=call path=_global.skse.plugins.CharGen.GetRaceSexCameraPos`, `...GetRaceSexCameraRot`, `...GetMeshCameraRadius`,
    `...GetPlayerPosition`.
11. `op=members path=_root.RaceSexMenuBaseInstance` and `op=members path=_root.RaceSexMenuBaseInstance.RaceSexPanelsInstance`.
12. Before `showracemenu`: note what the player wears; in RaceMenu (F4 to see the body): is the player still dressed?

13. No crash. Send the log, the screenshots and the JSON of section C.
