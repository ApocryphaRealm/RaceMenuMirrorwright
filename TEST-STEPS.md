# RaceMenu Mirrorwright 1.0.0 - sixteenth check: rail cell, first R3, safe probes, RaceMenu's own call shapes

Build: `7. current test builds\Skyrim - RaceMenu Mirrorwright\RaceMenu Mirrorwright 1.0.0` (repackaged), Line-SE-AE16,
installed over `test RaceMenu Mirrorwright 1.0.0` inside the batch. RaceMenu + AMF on, Atelier and its patch off,
Apprentice on, throwaway save, camera facing the front. Keep `uLogLevel = 1`. Tool `mirrorwright.control`.
Passed (runs 1-15): see the run folders; run 15 - no focus look on open, padded buttons, "..." at 0.20.
Do NOT call CharGen Save/Load/Export functions through op=call - run 15 crashed on that; the tool now refuses them.

1. Settings > Panel width 0.20: in the icon-only rail each icon sits in a square; the selected one (e.g. Head) has its
   highlight square BEHIND the icon, the same size, nothing beside it. Hover another: its square lights. Screenshot. Back
   to 0.30.
2. `op=category name=Head`, ~1.5 s later one R3 press. EXPECTED: the zoom toggles at the first press; log `zoom from R3
   (editor window (ImGui R3))`. Repeat after `op=category name=Hair`.
3. `op=call path=CharGen.SavePreset a0="x"`. EXPECTED: ok=false, "refused: ... no known signature"; the game does NOT
   crash; log `probe call ... refused`. `op=call path=CharGen.GetMeshCameraRadius`: EXPECTED ok=true (getters still work).
4. `op=spy`. EXPECTED: ok=true, `wrapped` lists SavePreset, LoadPreset, ReadPreset, ExportHead, ImportHead,
   LoadImportedHead; log `spy: watching CharGen's preset and export functions`.
   Then F4 to RaceMenu's OWN interface and, there, save a preset named `MW Spy` with RaceMenu's own preset controls, load
   it, and (if RaceMenu offers it) export the head. EXPECTED: RaceMenu behaves exactly as without the spy (the preset
   saves and loads), and the log has `spy: CharGen.SavePreset(N args): #0 ...` lines with the argument shapes and `spy:
   CharGen.SavePreset returned ...`. Send every `spy:` line - they give the signatures for Milestone 5.
5. Still for a real keyboard / mouse when possible: F4 with pad focus; drag turning; the editor's preset panel.
6. No crash. Send the log, screenshots and JSON.
