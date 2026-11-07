# RaceMenu Mirrorwright 1.0.0 - eighth check (combined): groups, race panel, camera data, the untested inputs

Build: `7. current test builds\Skyrim - RaceMenu Mirrorwright\RaceMenu Mirrorwright 1.0.0` (repackaged), Line-SE-AE16,
installed over `test RaceMenu Mirrorwright 1.0.0` inside the batch. RaceMenu + AMF on, Atelier and its patch off,
Apprentice on, throwaway save, camera facing the front. Tool `mirrorwright.control`.
Passed (runs 1-7): rail, icons, sliders applied, undo, filled boxes with no outline, finish and name flow, race change
with 0 changed afterwards, description panel, player dressed in RaceMenu.

## A. New in this build

1. Head (and Eyes, Hair): sliders sit under coloured group headers - **Shape** (morph sliders), **Parts**, **Colours**,
   **Paint**, **More** - each header in its own muted hue; colour rows (e.g. Eye Shadow, hair colour) show a small square
   in their colour. Screenshot. Settings > Group headers off: one plain list again.
2. `op=choose name=Redguard`: the panel below the grid shows the description, then `Health 50  Magicka 50  Stamina 50`
   (or the race's values), its skill bonuses (e.g. One-Handed +10, ...), and Abilities / Powers (Adrenaline Rush,
   Resist Poison ...). Screenshot. (The numbers come from the live race record.)

## B. Camera data (send the JSON; nothing to judge)

3. `op=members path=CharGen` - RaceMenu's Scaleform object.
4. `op=call path=CharGen.GetRaceSexCameraPos`, `CharGen.GetRaceSexCameraRot`, `CharGen.GetMeshCameraRadius`,
   `CharGen.GetPlayerPosition`, `CharGen.GetPlayerRotation`.
5. `op=members path=_root.RaceSexMenuBaseInstance.RaceSexPanelsInstance._zoomControl` and
   `..._lightControl`, and `op=members path=_root.RaceSexMenuBaseInstance.RaceSexPanelsInstance.cameraEditor`.

## C. Still untested inputs (what you can drive)

6. Mouse drag on the open space over the character: smooth turn; a slider drag does not turn.
7. Pad right stick turns; not doubled with Racemenu Rotation with Gamepad Support NG.
8. `op=done`, Enter, then Cancel on the game's finish box: RaceMenu's own interface; F4 brings the editor back.
9. Presets: Save `MW Test` (listed, .jslot in Overwrite), save again (replace question), Load (face returns; sliders
   follow after ~2 s?), Delete (gone).
10. Light (L / button) toggles RaceMenu's light.
11. No crash. Send the log, screenshots and the JSON of section B.
