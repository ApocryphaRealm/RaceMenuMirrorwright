# RaceMenu Mirrorwright 1.0.0 - fourth short check (after the 20:49 run): Done box, undo, race change

Build: `7. current test builds\Skyrim - RaceMenu Mirrorwright\RaceMenu Mirrorwright 1.0.0` (repackaged), Line-SE-AE16,
installed over `test RaceMenu Mirrorwright 1.0.0` inside the batch (install-test-build.py; player INI kept - fine).
RaceMenu + AMF on, Atelier and its patch off, throwaway save, camera facing the front. Tool `mirrorwright.control`.
Earlier runs passed: icons, rail names, Head category with 0 changed, Scars / War Paint visibly applied, E turning.

1. `op=slider key="ChangeHeadPart|6" value=5`, then `op=slider key="ChangeMask|7" value=6`. EXPECTED: scar and war paint
   on the face, 2 changed, and **Undo is no longer greyed out**.
2. Ctrl+Z (or the Undo button) twice. EXPECTED: war paint goes, then the scar; 0 changed. Ctrl+Y / Redo brings them
   back. Then Reset > confirm (`op=confirm` works too). EXPECTED: 0 changed, face as opened.
3. `op=choose name=Redguard` (any race other than the current one). EXPECTED: the race changes on screen; log
   `choose: ...[i] through <owner>.onItemPress` and a new `probe` line about a second later. Screenshot.
4. Head > Nose: drag it with the mouse end to end. EXPECTED: the nose changes live (screenshot before / after).
5. `op=done`. EXPECTED: "Finish character creation?" with Finish highlighted; state `editor.question` = `##confirmDone`.
   Then `op=cancel`: the box closes (log `question ##confirmDone: cancelled (devbench)`).
6. `op=done` again, then press **Enter** through bench.input. EXPECTED: log `question ##confirmDone: answered yes (key)`
   and `Done: GameDelegate.call("ConfirmDone")`. If Enter still does nothing, use `op=confirm` instead and report that
   Enter did not reach the box (then AMF is not passing the spliced key to the window). Report what the game does
   after ConfirmDone (name box? menu closes?) and that gameplay input is normal afterwards.
7. Pad (if a pad route is available): X opens the Done box, B cancels it, A confirms it.
8. No crash. Send the log and screenshots.
