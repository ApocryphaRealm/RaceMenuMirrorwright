# RaceMenu Mirrorwright 1.0.0 - fifteenth check: the four small fixes, and Milestone 5 probes

Build: `7. current test builds\Skyrim - RaceMenu Mirrorwright\RaceMenu Mirrorwright 1.0.0` (repackaged), Line-SE-AE16,
installed over `test RaceMenu Mirrorwright 1.0.0` inside the batch. RaceMenu + AMF on, Atelier and its patch off,
Apprentice on, throwaway save, camera facing the front. Keep `uLogLevel = 1`. Tool `mirrorwright.control`.
Passed (runs 1-14): see the run folders; run 14 - R3 zoom, narrow width, no hair square, white focus fill.

## A. Fixes

1. Open RaceMenu: EXPECTED no button looks focused (Light is not white) until a pad / arrow key moves the focus. Log
   `editor window takes the input focus`. Screenshot right after opening.
2. Move the pad focus to Light: white fill with clear room round the text (the "t" does not touch the edge). Screenshot.
3. Settings > Panel width 0.20: in the icon-only rail the selected item's highlight sits **behind** its icon (e.g. the
   scissors for Hair), not beside it. Screenshot. Back to 0.30.
4. `op=category name=Head`, then about 1.5 s later R3 (one press). EXPECTED: the zoom toggles at the FIRST press, log
   `zoom from R3 ...`. Repeat after `op=category name=Hair`.

## B. Milestone 5 probes (send the JSON; files are written - use a throwaway profile)

5. `op=call path=CharGen.ExportHead a0="MW Probe"`. EXPECTED: ok=true; then check whether
   `SKSE\Plugins\CharGen\MW Probe.nif` and `.dds` exist (Overwrite). Send the JSON and the file list.
6. `op=call path=CharGen.SavePreset a0="MW Probe"`. EXPECTED: ok=true; check whether
   `SKSE\Plugins\CharGen\Presets\MW Probe.jslot` exists. Send the JSON.
7. `op=call path=CharGen.GetSliderData`, `op=call path=CharGen.GetModName a0=0`. EXPECTED: ok=true; send the result
   shapes (for the remaining RaceMenu tools).

8. Still for a real keyboard / mouse when possible: F4 with pad focus; drag turning; presets in the panel.
9. No crash. Send the log, screenshots and the JSON.
