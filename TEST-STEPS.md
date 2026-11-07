# RaceMenu Mirrorwright 1.0.0 - eighteenth check: undress toggle, pose check (Milestone 5)

Build: `7. current test builds\Skyrim - RaceMenu Mirrorwright\RaceMenu Mirrorwright 1.0.0` (repackaged), Line-SE-AE16,
installed over `test RaceMenu Mirrorwright 1.0.0` inside the batch. RaceMenu + AMF on, Atelier and its patch off,
Apprentice on, throwaway save, camera facing the front. Keep `uLogLevel = 1`. Tool `mirrorwright.control`.
Settled in run 17: only the session's first pad press is lost, before it reaches the editor - AMF's input translation
drops the press that switches the device to the controller (Input.cpp ~1403 / 1529-1566, reported to Main Agent).

## A. Undress toggle (Main Agent)

1. Note what the player wears (the clothes are visible in RaceMenu). Scene panel **Undress** (or `op=undress`).
   EXPECTED: the clothes come off, quietly; the button now reads **Dress**; log `undress: took off <item> (<id>)` per item
   and `undress: N item(s) off ...`; `op=undress` answers `"undressed":true` with the item names. Screenshot.
2. **Dress** (or `op=dress`). EXPECTED: exactly the same items go back on; log `dress (button): put on <item>`. Screenshot.
3. Undress again, then `op=done` + confirm (or close the menu any way). EXPECTED: log `dress (Done)` (or `dress (RaceMenu
   closed)`) and, back in the game, the player wears everything they wore before (open the inventory: the same items
   are equipped, enchanted / tempered ones included).
4. Undress, then load a save from the console's menu / quickload. EXPECTED: log `dress (game load)`; nothing is lost
   (the loaded save is unaffected; the editor's list is cleared).

## B. Pose check

5. `op=pose` (default event IdleForceDefaultState), then `op=pose event=IdleWave` (or another vanilla idle event).
   EXPECTED: ok=true; send the JSON (`accepted`) and whether the character visibly moved. This decides whether poses
   can be built on the menu's character at all.

## C. For the owner with a real pad (not testable through the pad splice)

6. After using only the keyboard / mouse in the editor, press R3 on the real pad: EXPECTED the zoom toggles at the first
   press (log `zoom from R3 (XInput poll)`). Known: through AMF alone the first pad press is dropped (AMF fix pending).
7. Spy capture, as in the previous steps: Discovery > Record RaceMenu's calls, F4, in RaceMenu's own interface save
   `MW Spy`, load it, export the head; send the log.

8. No crash. Send the log, screenshots and JSON.
