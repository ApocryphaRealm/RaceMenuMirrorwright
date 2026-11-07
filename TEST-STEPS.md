# RaceMenu Mirrorwright 1.0.0 - seventeenth check: the first R3 press (diagnosed), and the spy capture

Build: `7. current test builds\Skyrim - RaceMenu Mirrorwright\RaceMenu Mirrorwright 1.0.0` (repackaged), Line-SE-AE16,
installed over `test RaceMenu Mirrorwright 1.0.0` inside the batch. RaceMenu + AMF on, Atelier and its patch off,
Apprentice on, throwaway save, camera facing the front. Keep `uLogLevel = 1`. Tool `mirrorwright.control`.
Passed (runs 1-16): see the run folders; run 16 - rail cells, the call guard, op=spy wrapping, later R3 presses.

## A. The first R3 press (Main Agent)

1. `op=category name=Head`, wait ~1.5 s, press R3 once, wait 2 s, press R3 again. EXPECTED: both presses zoom. Whatever
   happens, send these debug lines in order: `editor window takes the input focus (frame N)`, every `ImGui pad key ...
   down/up (frame M, focus taken K frames ago)`, every `XInput R3 down/up`, every `zoom from R3` and every `R3 edge ...
   ignored`. They show whether the first press reaches the editor at all (no `ImGui pad key` line for it = it never
   arrived; a line followed by `ignored` = the guard; a line followed by `zoom from R3` but no zoom = the menu ignored it).
2. Same with A instead of R3 after the category change (does the first A press reach ImGui?), and with R3 without any
   category change for 5 s first.

## B. The spy capture - needs the owner at the PC (RaceMenu's own preset save asks for a typed name)

`op=switch` (or the editor's "RaceMenu's interface" button) gets to RaceMenu's interface without a key; the typed name
still needs a real keyboard. For the owner, in game with the test profile:
3. Open RaceMenu (showracemenu). In the editor's rail pick **Discovery**, press **Record RaceMenu's calls** (or DevBench
   `op=spy`). EXPECTED: log `spy: watching CharGen's preset and export functions (...)`.
4. Press **F4** (or the "RaceMenu's interface" button). In RaceMenu's own interface: save a preset named `MW Spy`, load
   `MW Spy`, and export the head if RaceMenu offers it. EXPECTED: RaceMenu behaves exactly as usual; the log gets lines
   `spy: CharGen.<Function>(N args): #0 ...` and `spy: CharGen.<Function> returned ...` for each.
5. Close the game and send RaceMenuMirrorwright.log (Documents\My Games\Skyrim Special Edition\SKSE). Those lines give
   the argument shapes for Milestone 5 (export head, presets through RaceMenu's own functions).

6. No crash. Send the log.
