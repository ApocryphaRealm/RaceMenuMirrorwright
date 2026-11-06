# Prior art for Likeness (searched 2026-11-06, rule 24)

Wanted: a new character editor drawn over RaceMenu - a category rail, sliders, race and choice tiles, presets, search,
undo, a scene panel, full controller support - with RaceMenu still doing the real work underneath.

Searched Nexus (web search: "RaceMenu new interface replacement UI", "character creation overhaul", names "Likeness",
"Facesmith", "Looking Glass") and GitHub (`gh api`: expired6978/SKSE64Plugins for RaceMenu's own licence and exposed
interfaces; the RaceMenu Atelier repository by name only, not its source).

| Mod | Where | What it is | Why it is not this |
|---|---|---|---|
| **RaceMenu Atelier** (RustyOldSword, formerly emberchain) | Nexus 193865, GPL-3.0 | A redesigned front end for RaceMenu drawn through SKSE Menu Framework: category rail, sliders, search, undo, scene and presets panel, F4 to RaceMenu's own interface. | **The mod whose behaviour this rebuilds.** We forked it as "RaceMenu Atelier - Apprentice Patch" (our page 197089); Nexus moderators removed that page because it linked the source of an author banned from Nexus. So we need our own implementation that owes nothing to that codebase. Likeness is a clean-room rebuild: the spec (`4. plans\racemenu-rebuild\SPEC.md`) comes from Atelier's public description, its settings file names and observed behaviour; no code, ActionScript, movie, art or file layout is taken from it or from our fork. |
| RaceMenu (expired6978) | Nexus 19080 | The character editor itself (RaceSex Menu movie, sliders, presets, overlays, morphs). | Required underneath; Likeness drives it and never replaces it. Its repo carries no licence, so nothing of it is copied. |
| RaceMenu skins: Untarnished UI patch, Nordic UI, Horizons UI, Dear Diary Dark Mode | Nexus (various), Dynamic Interface Patcher patches | Recolour or reskin RaceMenu's own movie. | Same layout and controls as RaceMenu; no new editor. They keep working under Likeness. |
| Face sculptor's RaceMenu | Nexus | A limited RaceMenu that locks race and sex. | Restricts RaceMenu; no new interface. |
| Racemenu Rotation with Gamepad Support NG | Nexus | Turning the character with a pad in RaceMenu. | One control; Likeness's controller layout covers it. |

Nothing found other than RaceMenu Atelier draws a new editor over RaceMenu.
