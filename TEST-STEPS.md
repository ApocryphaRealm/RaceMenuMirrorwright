# RaceMenu Mirrorwright 1.0.0 - fifth short check (after the 20:57 run): finish flow, race change, rail order, turning

Build: `7. current test builds\Skyrim - RaceMenu Mirrorwright\RaceMenu Mirrorwright 1.0.0` (repackaged), Line-SE-AE16,
installed over `test RaceMenu Mirrorwright 1.0.0` inside the batch (install-test-build.py). RaceMenu + AMF on, Atelier
and its patch off, Apprentice on, throwaway save, camera facing the front. Tool `mirrorwright.control`.

1. Rail. EXPECTED: Race, then **Class** and **Trait** (not "Apptrait"), then Body, Head, Face, ... with category names
   in title case (Race, Body, Head - not RACE, BODY). Screenshot.
2. `op=choose name=Redguard` (a race other than the current one). EXPECTED: the character becomes a Redguard on screen;
   log `choose: ...[i] race through GameDelegate.call("ChangeRace", [<id>])` and a new `probe` line. Screenshot.
   Then pick a race by clicking its tile: same result.
3. Class: pick a class tile. EXPECTED: the race does NOT change; log `choose: ... through <owner>.onItemPress`; RaceMenu's
   bottom bar (F4 to see it) shows the class.
4. Turning, camera facing the front:
   a. Mouse: press the left button on the open space over the character (not on a panel) and drag left / right.
      EXPECTED: the character turns smoothly with the drag and stops when released; dragging a slider does NOT turn.
   b. Pad right stick left / right: turns smoothly.
   c. Q / E held, and the Turn left / Turn right buttons (15 degrees): still turn.
   d. With Racemenu Rotation with Gamepad Support NG enabled (it is in the list): the stick turns at one speed, not
      doubled or jerking. If it doubles, report it.
5. Done flow: `op=done`, Enter (or `op=confirm`). EXPECTED: log `answered yes`, `Done: GameDelegate.call("ConfirmDone")`,
   `editor hidden (Done: the game's own finish)`; RaceMenu's own panels come back; the game's "Finish and name your
   character?" box answers Enter / its OK; then the name entry takes typing; the menu closes and gameplay input is
   normal. Report each stage.
6. Repeat 5 but press Cancel on the game's box. EXPECTED: back in RaceMenu with its own interface; F4 brings the editor
   back.
7. Confirmation box look (Reset with a slider changed, or `op=done`): two filled boxes side by side - **Finish** white
   with dark text (focused), **Cancel** dark grey with light text; no outline-only box. Right arrow / D-pad right / Tab:
   the white fill moves to Cancel; Left: back. Hover Cancel with the mouse: it turns white. Enter with Cancel focused
   closes the box without finishing (log `cancelled (key)`); Esc / B always cancel. Screenshot both focus states.
   Same look on Load, Save-over-existing and Delete in the presets panel.
8. No crash. Send the log and screenshots.
