# RaceMenu Mirrorwright 1.0.0 - third short check (after the 20:41 run): turning, sliders reaching the face

Build: `7. current test builds\Skyrim - RaceMenu Mirrorwright\RaceMenu Mirrorwright 1.0.0` (repackaged), Line-SE-AE16, in
`test RaceMenu Mirrorwright 1.0.0` (installed by the session that packaged it; check the DLL hash in the log of
install-test-build). RaceMenu + AMF on, RaceMenu Atelier and its patch off, throwaway save. Evidence of the earlier
runs: `4. plans\racemenu-rebuild\test-run-20261106\`. Open RaceMenu with the camera **facing the character's front**.

1. Start-up log line `settings read from ...` then `... gameSliderCalls=true ...`. EXPECTED: true (the key is now
   bGameSliderCalls; the old bEngineSliderCalls line in an old INI is ignored).
2. `showracemenu`, close the console. Hold Q for about 1.5 s, then E. EXPECTED: the character turns left, then right,
   smoothly; log once `turn: player heading A -> B rad (Actor::SetHeading; first turn of the session)`. If the
   character snaps back at once, report it (the menu may hold its own rotation). Scene panel Turn left / Turn right:
   15 degrees each. Pad right stick: turns.
3. Turn the face to the camera. Head > drag **Nose** (or any Head morph slider) end to end. EXPECTED: the nose changes
   live; the row gets `*`, the count says 1 changed. Screenshot before and after.
4. `op=slider key="ChangeHeadPart|6" value=5` (Scars) and `op=slider key="ChangeMask|7" value=6` (War Paint), as in the
   last run. EXPECTED: a scar and war paint appear on the face. Screenshot.
5. Ctrl+Z twice: both go away again. Reset (confirm): 0 changed, the face back as it opened.
6. Race: click another race. EXPECTED: the race changes; log `choose: ...through <owner>.onItemPress`.
7. Done (button or R): Finish focused, confirm. EXPECTED: log `Done: GameDelegate.call("ConfirmDone")`; RaceMenu
   finishes (report what follows). Then gameplay input is normal.
8. No crash. Send the log and the screenshots.
