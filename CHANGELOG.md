# Likeness - changelog

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
