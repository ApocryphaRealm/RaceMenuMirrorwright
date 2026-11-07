# RaceMenu Mirrorwright 1.0.0 - nineteenth check: undress without TNG, visible on screen

Build: `7. current test builds\Skyrim - RaceMenu Mirrorwright\RaceMenu Mirrorwright 1.0.0` (repackaged), Line-SE-AE16,
installed over `test RaceMenu Mirrorwright 1.0.0` inside the batch. RaceMenu + AMF on, Atelier and its patch off,
Apprentice on, TNG as in the profile, throwaway save with the player clothed, camera facing the front. Keep
`uLogLevel = 1`. Tool `mirrorwright.control`. No screenshot may show explicit content: TNG's cover must stay on.
Settled in run 18: poses are not possible while the menu is up (the graph accepts events but the paused game never
advances them) - dropped from Milestone 5.

1. `op=undress` (or Undress in the scene panel). EXPECTED in the log: `undress: kept TNG GenitalCover (...): from The New
   Gentleman's plugins` (or "uses slot 52"), `undress: took off <Boots>`, `<Clothes>` ..., then `undress: 3D model update
   requested`. On screen: the boots and clothes are gone from the character within about a second; TNG's cover stays.
   Check the log BEFORE taking a screenshot; if any TNG item was taken off, do not screenshot - dress first and report.
2. `op=dress`. EXPECTED: the same boots and clothes back on screen; no "another copy used" line.
3. Undress, then Done (confirm) - EXPECTED `dress (Done)`, and back in the game the inventory shows the same items
   equipped (an enchanted / tempered one as the same item).
4. No crash. Send the log and (clean) screenshots.
5. Owner items still open (TEST-STEPS of the earlier runs and the to-do list): spy capture, real-pad first R3, mouse drag,
   the editor's own preset panel.
