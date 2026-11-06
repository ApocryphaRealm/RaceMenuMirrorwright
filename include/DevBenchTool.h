#pragma once

namespace DevBenchTool
{
	// Registers "likeness.control" with DevBench when present (rules 31 and 64). Call with false at kPostLoad and true at
	// kDataLoaded.
	void Init(bool a_lastAttempt);
}
