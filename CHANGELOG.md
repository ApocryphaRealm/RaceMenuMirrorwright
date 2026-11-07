# RaceMenu Mirrorwright - changelog

Written as changes happen, not reconstructed afterwards (rule 61). Each version carries its
**version-ledger status**, so this file cannot quietly claim more than the ledger does:

* **working** - observed running in game
* **untested** - built and packaged, not yet confirmed
* **failed** - built but crashed or malfunctioned; the number was reclaimed
* **scratch** - a hypothesis-test build that never held a real number

## 1.0.0 - 2026-11-06 - untested

### Added
- Milestone 1 (foundation and discovery): an editor window drawn through Apocrypha Menu Framework (or SKSE Menu Framework 3) over the RaceSex Menu, shown when RaceMenu opens; the switch key (F4) or a button switches between it and RaceMenu's own interface.
- Reads what RaceMenu's movie holds - every list with its path, size and entries, and RaceMenu's Scaleform functions - and shows the lists in a rail with their entries, read-only in this milestone.
- Settings page inside the menu framework (switches, precise sliders, Save / Reload / Restore defaults), an INI with every setting, eleven languages.
- DevBench tool likeness.control: state, probe, list, plugins, show, hide, switch, reload.
- Milestone 2 (the playable minimum): a category rail (RaceMenu's categories, Race first, with another mod's choices filed in the race list - Apprentice's classes and traits - as their own entries), a race grid that changes race the way RaceMenu's own list does, every slider of the chosen category applied live, Undo / Redo (Ctrl+Z / Ctrl+Y), Reset to the values the menu opened with, a changed count, search across every slider (Ctrl+F), Done with a confirmation that opens with Finish focused, and RaceMenu's own panels hidden while the editor is up.
- Scene and presets panel: RaceMenu's presets listed by their real names (a name RaceMenu cannot open on this Windows is greyed out), Load, Save (asks before replacing) and Delete (asks first), and RaceMenu's light.
- From the first in-game run: the rail takes its names and order from RaceMenu's own category list (no raw flag numbers, no "$ALL"), the race grid has no empty tiles, sliders are told apart by callback and slider ID (the false "266 changed" came from many sliders sharing one game callback), the rail no longer grows on a re-read, and the menu's lists are read from their known paths before the walk (the walk had hit its node limit).
- Sliders handled by the game are passed to it through the menu's GameDelegate with their slider ID; Done calls the game's ConfirmDone the same way.
- Controls: Q / E and the right stick turn the character (RaceMenu's CharGen rotation), Turn left / right buttons, L toggles the light, R or the pad's X presses Done; opened from the console, the editor waits until the console closes.
- Renamed RaceMenu Mirrorwright (working name Likeness): DLL, INI, log and translation files are RaceMenuMirrorwright.*, the DevBench tool is mirrorwright.control.
