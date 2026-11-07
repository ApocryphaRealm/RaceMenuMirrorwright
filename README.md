# RaceMenu Mirrorwright

A character editor drawn over RaceMenu for Skyrim Special Edition / Anniversary Edition. RaceMenu stays installed and
loaded underneath and does all the real work - every slider, preset, overlay and morph is still RaceMenu's - while
the editor draws its own editor over the RaceSex Menu through the Apocrypha Menu Framework. A key (F4) switches to
RaceMenu's own interface and back.

Status: **Milestones 1 and 2** built (untested in game): race grid, live sliders, presets, Done. What it reads from
RaceMenu is chosen at run time; see PLAN.md 7c. Plan and spec:
`D:\Claude output\4. plans\racemenu-rebuild\` (`SPEC.md`, `PLAN.md`).

## Requirements

SKSE64, Address Library for SKSE Plugins, RaceMenu, and Apocrypha Menu Framework (or SKSE Menu Framework 3).

## Settings

`SKSE\Plugins\RaceMenuMirrorwright.ini`; most also on the framework's RaceMenu Mirrorwright > Settings page.

## Testing hooks

DevBench tool `mirrorwright.control` (ops: state, probe, list, plugins, model, slider, choose, category, done, presets, show, hide, switch, reload). Log:
`Documents\My Games\Skyrim Special Edition\SKSE\RaceMenuMirrorwright.log`.

## Building

Visual Studio 2022 (C++), `VCPKG_ROOT` set. `configure.bat` then `build.bat` (SE 1.5.97 / AE 1.6 line);
`configure17.bat` then `build17.bat` (Skyrim 1.7.x line). Translations: `py -3 tools/make_translations.py`.

## Licence

GPL-3.0-or-later (`LICENSE`, `NOTICE.md`). Written from scratch; see `NOTICE.md` for the clean-room statement and
`PRIOR-ART.md` for what came before.
