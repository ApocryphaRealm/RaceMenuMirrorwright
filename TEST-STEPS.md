# RaceMenu Mirrorwright 1.0.0 (Milestones 1 and 2) - test steps

Build: `7. current test builds\Skyrim - RaceMenu Mirrorwright\RaceMenu Mirrorwright 1.0.0` - install the **Line-SE-AE16**
line with install-test-build.py (`--line Line-SE-AE16`) into a new MO2 folder `test RaceMenu Mirrorwright 1.0.0`. Needs
RaceMenu and the Apocrypha Menu Framework enabled. RaceMenu Atelier and the RaceMenu Atelier patch must be **unticked**
(they draw over the same menu). Use a throwaway save: these steps change the character and write presets.

Log: `Documents\My Games\Skyrim Special Edition\SKSE\RaceMenuMirrorwright.log`. DevBench tool: `mirrorwright.control`.
Steps marked **[discovery]** answer questions only the game can answer - send their JSON back even when they pass.

## Start-up and opening

1. Start the game to the main menu. EXPECTED in the log: `RaceMenu Mirrorwright 1.0.0 loading`, `registered the editor
   window and the settings page with the menu framework`, `watching the RaceSex Menu`, `switch key sink installed
   (scan code 0x3E)`, `registered "mirrorwright.control" with DevBench`. No line saying the framework lacks an export.
2. Load a save, open the console, `showracemenu`, close the console with ~. EXPECTED: a dark panel titled **RaceMenu
   Mirrorwright** covers the left 30% of the screen (search box, a "N changed" count, a rail of categories, content,
   Undo / Redo / Reset / Done), a "Scene and presets" panel at the right, and RaceMenu's own panels are gone (only the
   character shows between them). Log: `RaceSex Menu opened`, `editor shown (RaceMenu opened)`, `RaceMenu's own panels
   hidden`, then `probe 1: N races, M sliders, K categories` with N > 0 and M > 0, and four `model:` lines.
3. **[discovery]** DevBench `mirrorwright.control` (no op), `op=model count=200`, `op=plugins`, and `op=list list=<raceList>`
   and `op=list list=<sliderList> count=50` (the indexes are in the state's `model`). EXPECTED: `model.raceList` and
   `model.sliderList` are not -1; `valueField`, `callbackField`, `flagField` are filled; `doneFunction` is named or
   empty. Send all five JSONs back - they confirm or correct the member names PLAN.md marks "confirm in the test run".

## The editor (Milestone 2)

4. Click **Race** in the rail. EXPECTED: a grid of race tiles with races only (with Apprentice installed, no classes or
   traits among them). Click another race (e.g. Nord -> Redguard). EXPECTED: the character changes race exactly as
   RaceMenu's own list does; log `choose: <path>[i] through <owner>.onItemPress` and, about a second later, a new
   `probe` line. Send the log line back. If the log says `no onItemPress found`, the race did not change: report it.
5. With Apprentice - A Class Overhaul installed: its classes and traits have their own rail entries after Race. Pick a
   class. EXPECTED: the race does NOT change; RaceMenu's bottom bar shows the class. (Skip without Apprentice.)
6. Click **Head** (or Face) and drag a slider. EXPECTED: the character's face changes live while dragging; the slider's
   label gets a `*` and the changed count goes up. Nudge a slider with the arrow keys: one step per press.
   **[discovery]** If the label moves but the face does NOT change, note which sliders, then set
   `bEngineSliderCalls = 1` in the INI, DevBench `op=reload`, and try again; report whether that made them work (or
   crashed). Put it back to 0 afterwards.
7. A body slider from a body mod (e.g. a CBBE/3BA/HIMBO slider in Body): drag it. EXPECTED: the body changes live.
8. Ctrl+Z. EXPECTED: the last slider goes back. Ctrl+Y: it comes back. Undo / Redo buttons do the same.
9. Type `nose` in the search box. EXPECTED: matching sliders from every category, grouped under category headers.
   Clear it: the chosen category comes back.
10. **Reset**. EXPECTED: a question; confirm. Every changed slider goes back to its value when the menu opened; the
    changed count is 0.
11. Switch key F4. EXPECTED: the editor disappears and RaceMenu's own interface is back and fully usable. F4 again:
    the editor and the hidden panels come back (log `RaceMenu's own panels shown` / `hidden`).

## Presets

12. In the scene panel type a name (e.g. `MW Test`), **Save**. EXPECTED: log `presets: CharGen.SavePreset("MW Test")
    dispatched`; after **Refresh the list** the name is listed; the file
    `Data\SKSE\Plugins\CharGen\Presets\MW Test.jslot` exists (in MO2's Overwrite). Save again with the same name:
    EXPECTED a "Replace it?" question.
13. Move a few sliders, select `MW Test`, **Load**, confirm. EXPECTED: log `CharGen.LoadPreset("MW Test") dispatched`; the
    character returns to the saved look. **[discovery]** Do the editor's sliders show the loaded values after about
    2 seconds (a new `probe` line)? Report yes/no.
14. Select `MW Test`, **Delete**, confirm. EXPECTED: it leaves the list and the .jslot file is gone.
15. **Light**. EXPECTED: RaceMenu's character light toggles (log `light: RSM_ToggleLight sent`).

## Finishing

16. **Done**. EXPECTED: "Finish character creation?" with **Finish** already highlighted; Enter (or A on a pad) finishes.
    EXPECTED: RaceMenu closes as with its own Done (log `Done: called <function>`). If instead the status says "Finish
    from RaceMenu's own interface", the editor switched to RaceMenu's interface - finish there with RaceMenu's Done and
    **send the `doneFunction` / function list from step 3** so the right function can be set.
17. After the menu closes: log `RaceSex Menu closed`; gameplay input normal (no stuck cursor, movement works).

## Settings page

18. Framework menu (F1) > RaceMenu Mirrorwright > Settings. EXPECTED: switches for each on/off setting, sliders that
    move 0.01 per arrow / D-pad nudge, the switch key shown as 0x3E, Save / Reload / Restore defaults. Set Panel width
    0.40, Save, `showracemenu`: the editor is wider; the installed RaceMenuMirrorwright.ini says `fPanelWidth = 0.40`.
    Put it back to 0.30 and Save.
19. Turn **Hide RaceMenu's own panels** off, Save, `showracemenu`. EXPECTED: RaceMenu's panels stay visible beside the
    editor. Turn it back on and Save.
20. Through every step: no crash, no freeze, no `error` lines in the log. Send RaceMenuMirrorwright.log and a screenshot
    of the editor over RaceMenu.
