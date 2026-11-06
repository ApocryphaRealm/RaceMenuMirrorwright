#pragma once

// Likeness - settings. Plain-file INI (never the Win32 profile API, which MO2's VFS does not redirect - rule 16).
// Every compiled default here equals the value in dist\SKSE\Plugins\Likeness.ini.

#include <cstdint>
#include <string>

namespace settings
{
	// [General]
	inline bool enabled = true;             // bEnabled - draw the editor over RaceMenu (0: RaceMenu's own interface only)
	inline bool startInEditor = true;       // bStartInEditor - the editor is up when RaceMenu opens

	// [Layout]
	inline float scale = 1.0F;              // fScale - size of the editor, 0.50 - 2.00
	inline float panelWidth = 0.30F;        // fPanelWidth - share of the screen width, 0.20 - 0.50
	inline bool scenePanel = true;          // bScenePanel - the scene and presets panel

	// [Editor]
	inline bool askBeforeDone = true;       // bAskBeforeDone
	inline bool groupHeaders = true;        // bGroupHeaders
	inline bool hideOtherBodySliders = true;// bHideOtherBodySliders
	inline bool allowPastRange = false;     // bAllowPastRange - typed values may pass a custom morph's range
	inline bool showInternalNames = false;  // bShowInternalNames

	// [Camera]
	inline float turnSpeed = 0.35F;         // fTurnSpeed - degrees per pixel of drag, 0.05 - 2.00

	// [Controls]
	inline std::uint32_t switchKey = 0x3E;  // iSwitchKey - DirectInput scan code; F4 (DEFAULT-KEYS.md). 0 = unbound

	// [Debug]
	inline std::uint32_t logLevel = 2;      // uLogLevel - 2 = info (rule 14)

	void Init(const char* a_iniFileName);
	bool Reload();
	bool Save();
	void RestoreDefaults();
	void ApplyLogLevel();
	const std::string& GetIniPath();
}
