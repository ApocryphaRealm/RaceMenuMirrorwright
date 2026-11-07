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
- Font Awesome icons on the rail, Undo / Redo / Reset / Done, the preset buttons, Light, Turn, search and the switch.
- Turning turns the player's own heading (RaceMenu's Scaleform rotation gave no value in the second run); held keys and the stick add up into one turn per frame.
- The INI key for passing sliders to the game is now bGameSliderCalls (default 1): an installed INI from the first build kept bEngineSliderCalls = 0, which switched the sliders off.
- The confirmation boxes (Done, Reset, Load, replace, delete) answer Enter, Space and the pad's A, and Esc and the pad's B, read by the box itself; the press that opened a box never answers it.
- DevBench: slider changes go through the editor's own path, so they can be undone; op=confirm / op=cancel answer the open question; op=choose name=<race>.
- After Done, the editor steps aside so the game's own "Finish and name your character?" box and the name entry get the keys; the editor also hides while any game message box is up during RaceMenu and comes back when it closes. F4 brings it back after a cancelled finish.
- A race is changed through the game's own ChangeRace call with the race's ID (onItemPress with the list position changed nothing); other mods' choices still go through onItemPress, with the entry's own itemIndex.
- Apprentice's Class and Trait categories sit directly after Race in the rail, labelled Class and Trait; any all-capitals category name shows in title case.
- Turning by dragging the mouse on the open space over the character, beside the stick, Q / E and the Turn buttons - all the editor's own rotation.
- Every confirmation box (Done, Reset, Load, replace, delete) draws its two choices as filled boxes: grey with light text, the focused one white with dark text. Focus starts on the confirming choice; Left / Right, the D-pad and Tab move it, the mouse focuses what it hovers; Enter / Space / A take the focused choice, Esc / B cancel.
- A race tile is pressed through RaceMenu's own item-press handler on its panels clip first (run 5: the game's ChangeRace with the race's ID reloaded the sliders on top of the old ones and left the character as it was); sRaceRoute in the INI and route= in DevBench choose the route while it is confirmed. The log names each entry's raceID, filteredIndex and itemIndex.
- Below the race and choice grids, the description of the race or choice last hovered or focused.
- A race change is a new starting point: the sliders it sets do not count as changes, and Undo starts over (run 6 showed "28 changed" with Undo greyed out).
- No keyboard-navigation outline around the focused control; the white fill is the focus.
- The race description panel follows keyboard and pad focus and DevBench choices, not only the mouse.
- DevBench probes op=members and op=call, to read slider rows and RaceMenu's camera functions in the next run.
- Group headers in each category, each in its own muted hue: Shape, Parts, Colours, Paint, More (from what each row does; switchable with bGroupHeaders). Colour rows show a swatch of their colour.
- The race panel adds the live race record's starting Health / Magicka / Stamina, skill bonuses and spells by kind (abilities, powers, lesser powers, shouts, spells).
- DevBench op=call and op=members accept CharGen.<function> for RaceMenu's Scaleform object.
- Colour swatches show the character's live colour: the hair colour from the player's hair colour, a tint row from the player's tint of that kind (run 8: the row's own value showed near-black for red-orange hair); they follow a colour change.
- Face close-up: Z, the pad's R3, the Close-up button or DevBench op=closeup move RaceMenu's camera halfway to the face and back (CharGen camera position).
- The colour swatch is drawn as a filled square on the window (run 9: the icon-font square drew nothing); the hair colour read is logged, and a row with no colour from the character keeps its own.
- The close-up sits fCloseUpDistance (50) from the head along the original view and fCloseUpRaise (9) higher, so the eyes are centred (run 9: halfway was ~20 units, nose clipping).
- The scene panel's buttons sit in two rows, so Turn right is no longer cut off.
- Colour rows are recognised by their colour, their tint type or a colour callback, and every candidate row is logged with why it got a swatch or not; up to 64 members of a row are kept (24 could drop the colour member). Run 10 showed no swatch at all.
- The close-up always moves in: to min(fCloseUpDistance = 28, 0.6 x the current distance) from the head point (feet + 123 + fCloseUpRaise), on the line to the camera, so the face stays centred (run 10: a fixed 50 moved the camera back).
- Rows stay inside their parent (the owner's rule): the top bar, the footer, the scene panel, the preset buttons and the settings buttons measure each item and wrap to the next line when it does not fit; a label too long for a whole line is shortened with "..." and shown in full in a tooltip.
- Zoom: Z, R3, the button and op=closeup press the game's own zoom (the menu's input handler with the zoom control's key) instead of moving the camera; the button reads "Zoom out" or "Close-up" by state. fCloseUpDistance and fCloseUpRaise are gone (runs 9-11: the computed camera clipped into the head).
- Swatches: the hair colour is shown doubled (the hair shader brightens it; raw and shown values logged); an unused tint shows a dim grey square instead of white.
- The pad's R3 zooms through the game's own input event as well as the editor's own read (one zoom per press); every pad button the editor sees is logged at debug level.
- Hair Color's swatch shows the row's own colour, as RaceMenu's list does (the hair colour form's tint is not the colour the hair renders).
- R3 is read from XInput directly (through the XInput DLL the game already loaded; nothing new is loaded) and the switch key from the keyboard while the game is in front, so neither depends on who holds ImGui's focus.
- The focused button is drawn white with dark text, like the confirmation boxes.
- Every text item fits its parent: rail names, slider labels, the search hint and preset names are shortened with "..." and shown in full in a tooltip; a narrow rail shows icons only.
- A colour row with no known colour draws no square (no grey placeholder).
- An icon-only rail draws the icon as the item itself, so the highlight sits behind it; buttons have more room round their text; no button looks focused until the player navigates; the editor window takes the input focus when it opens, is shown, or changes category.
- The icon-only rail cell is drawn by hand - a square the size of the icon, its highlight behind it, the icon centred.
- R3 is read as its own edge on the key state every frame, so the first press after a category change counts.
- DevBench op=call refuses RaceMenu plugin functions other than argument-less getters (a bad call to CharGen.SavePreset crashed the game in run 15); op=spy logs RaceMenu's own calls to its preset and export functions with their arguments.
- Every pad key edge the editor sees through the framework, and every XInput R3 edge, is logged at debug level with the frame and how many frames since the editor took the focus; an R3 edge that is ignored says why.
- Discovery > "Record RaceMenu's calls" arms the same recording as DevBench op=spy, for a player sending a bug report.
- Undress / Dress button in the scene panel (and DevBench op=undress / op=dress): takes off every armour the player wears, quietly, and puts exactly those back on the second press, on Done, when RaceMenu closes and on a game load; never saved, TNG untouched.
- DevBench op=pose event=<animation event>: sends one event to the player and reports whether the animation graph took it (does the menu's character animate at all).
- Undress never touches The New Gentleman: it takes off body clothing and armour on slots 30-61 only, never slot 52, nothing from TNG's plugins (TNG puts its keywords on ordinary clothing, so a keyword rule kept the clothes on); every item kept on is logged with the reason. The character's model is rebuilt after undressing and dressing, so the change shows in RaceMenu.
- Renamed RaceMenu Mirrorwright (working name Likeness): DLL, INI, log and translation files are RaceMenuMirrorwright.*, the DevBench tool is mirrorwright.control.
