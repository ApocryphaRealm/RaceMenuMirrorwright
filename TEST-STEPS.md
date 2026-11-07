# Likeness 1.0.0 (Milestone 1) - test steps

Build: `7. current test builds\Skyrim - Likeness\Likeness 1.0.0` - install the **Line-SE-AE16** line with
install-test-build.py (`--line Line-SE-AE16`) into a new MO2 folder `test Likeness 1.0.0`. Needs RaceMenu and the
Apocrypha Menu Framework enabled (both are in Njordlinger). RaceMenu Atelier and the RaceMenu Atelier patch must be
**unticked** for this run (they draw over the same menu). Nothing in this build edits the character - it only reads
RaceMenu and shows what it found.

Log: `Documents\My Games\Skyrim Special Edition\SKSE\Likeness.log`.

1. Start the game to the main menu. EXPECTED in Likeness.log: `Likeness 1.0.0 loading`, `Build line: SE 1.5.97 / AE 1.6.x`,
   `registered the editor window and the settings page with the menu framework`, `watching the RaceSex Menu`,
   `switch key sink installed (scan code 0x3E)`, and `registered "likeness.control" with DevBench`. No line saying the
   framework lacks an export.
2. Load a save (or coc Riverwood from a new game) and open the console: `showracemenu`, then close the console with ~.
   EXPECTED: RaceMenu opens and a dark panel titled **Likeness** covers the left 30% of the screen, full height, with a
   status line, two buttons (Read RaceMenu again, RaceMenu's interface), a left list and a right list. Log:
   `RaceSex Menu opened`, `editor shown (RaceMenu opened)`, then within about 2 seconds `probe 1: N lists, M entries,
   K plugin objects, ...` with N > 0.
3. DevBench: `likeness.control` with no op. EXPECTED: `editor.shown` true, `bridge.menuOpen` true, `bridge.lists`
   non-empty; one of the lists has RaceMenu's races in it (its `first` is a race name such as Argonian / Breton) and
   one or more hold slider rows. Send back the whole JSON.
4. DevBench: `likeness.control op=plugins`. EXPECTED: an entry for RaceMenu's Scaleform object (its name and member
   list, e.g. functions about presets). Send back the whole JSON - Milestone 2 is built on it.
5. DevBench: `likeness.control op=list list=<index of the race list> count=50`, and the same for the largest slider
   list. EXPECTED: labels match what RaceMenu shows on its Race tab and on that slider tab. Send back both.
6. In the panel, click entries of the left list. EXPECTED: the right list changes to that list's entries; hovering an
   entry shows its numbers. Nothing on the character changes.
7. Press F4. EXPECTED: the Likeness panel disappears and RaceMenu's own interface is fully usable (sliders move the
   character). Log: `editor hidden (editor window)`. Press F4 again. EXPECTED: the panel comes back
   (`editor shown (game input)`), one switch per press (no flicker back).
8. Press the panel's **RaceMenu's interface** button. EXPECTED: same as the first F4 press (`editor hidden (button)`).
9. Finish RaceMenu (its own Done). EXPECTED: the panel is gone with the menu; log `RaceSex Menu closed`; gameplay input
   normal (the cursor is not stuck, movement works).
10. Open the framework menu (F1) > Likeness > Settings. EXPECTED: switches (not tick boxes) for each on/off setting, two
    sliders that move 0.01 per arrow / D-pad nudge, the switch key shown as 0x3E, Save / Reload / Restore defaults.
    Set Panel width to 0.40, press Save, `showracemenu` again. EXPECTED: the panel is wider (40%); the installed
    Likeness.ini now says `fPanelWidth = 0.40` (diff it against the packaged one). Put it back to 0.30 and Save.
11. With `Start in Likeness` off (Save), `showracemenu`. EXPECTED: RaceMenu opens in its own interface; F4 brings Likeness
    up. Turn it back on and Save.
12. Through every step: no crash, no freeze, and the log has no `error` lines. Send back Likeness.log.
