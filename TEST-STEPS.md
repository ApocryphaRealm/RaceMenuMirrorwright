# RaceMenu Mirrorwright 1.0.0 - sixth check (combined): race change routes, turning, cancel, boxes, presets

Build: `7. current test builds\Skyrim - RaceMenu Mirrorwright\RaceMenu Mirrorwright 1.0.0` (repackaged), Line-SE-AE16,
installed over `test RaceMenu Mirrorwright 1.0.0` inside the batch. RaceMenu + AMF on, Atelier and its patch off,
Apprentice on, throwaway save, camera facing the front. Tool `mirrorwright.control`. Passed already (runs 1-5): rail
names and order, icons, Head sliders, Scars / War Paint on the face, E turning, undo, filled confirm boxes, the whole
finish and name flow.

## Race change (the one open fault) - try the routes in this order, a fresh `showracemenu` for each

1. `op=choose name=Redguard` (default route). EXPECTED: the character becomes a Redguard on screen. Log: one line
   `choose: ...[i] raceID=.. filteredIndex=.. itemIndex=.. route='auto'` then `choose: ... through <owner>.onItemPress`.
   Send both lines and a screenshot. If it worked, skip to step 4.
2. If not: `op=choose name=Redguard route=_root.RaceSexMenuBaseInstance.RaceSexPanelsInstance.racePanel`. Same EXPECTED.
3. If not: `op=choose name=Redguard route=changerace`. Same EXPECTED; note the slider count in the state before / after.
   Send the log lines of all three; the route that worked becomes the default.
4. Click a race tile with the mouse: same route, same result. Hover a race tile: its description shows below the grid.
5. Class: click a class tile. EXPECTED: the race does NOT change; RaceMenu's bottom bar (F4) shows the class.

## Turning (not run in run 5)

6. Mouse: press on the open space over the character and drag left / right: smooth turn, stops on release. Dragging a
   slider does not turn.
7. Pad right stick: turns. With Racemenu Rotation with Gamepad Support NG on: one speed, not doubled.
8. Q / E and the Turn buttons: still turn.

## Boxes and the cancelled finish

9. `op=done`: Finish white, Cancel grey. Right arrow / Tab: white moves to Cancel; Enter: the box closes without finishing
   (`cancelled (key)`). Hover Finish: white again.
10. `op=done`, Enter, then **Cancel** on the game's "Finish and name your character?" box. EXPECTED: back in RaceMenu's
    own interface; F4 brings the editor back.

## Presets (never run yet)

11. Type `MW Test`, Save (icon floppy). Refresh: listed; the .jslot is in Overwrite\SKSE\Plugins\CharGen\Presets.
    Save again: the replace question (filled boxes).
12. Move a slider, select `MW Test`, Load, confirm: the face returns. Note whether the sliders show the loaded values
    after about 2 seconds (a new `probe` line).
13. Delete `MW Test`, confirm: gone from the list and the folder.
14. Light (L or the button): RaceMenu's light toggles.

15. No crash. Send the log and screenshots.
