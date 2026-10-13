#pragma once

// Reborn: Keep online timestamps on the original clock where supported, without requiring ICU on older Windows.
#include "Common/RebornLog.h"
#include <chrono>
#include <cstdint>
#include <system_error>
#include <windows.h>

namespace RebornOnlineClock
{
//-------------------------------------------------------------------------------------------------
/** Reborn: Select one clock for all online timers; only missing system dependencies enable the compatibility fallback. */
//-------------------------------------------------------------------------------------------------
inline int64_t nowMilliseconds()
{
	// Reborn: Thread-safe initialization keeps request callbacks and the main thread on the same time base.
	static const bool useSystemClock = []()
	{
		try
		{
			std::chrono::utc_clock::now();
			return false;
		}
		catch (const std::system_error& error)
		{
			if (error.code() != std::error_code(ERROR_MOD_NOT_FOUND, std::system_category()) &&
				error.code() != std::error_code(ERROR_PROC_NOT_FOUND, std::system_category()))
				throw; // Reborn: Unrelated failures retain their original handling.
			REBORN_LOG("CLOCK COMPATIBILITY: Online UTC timer unavailable (%d: %s); using system_clock for online timers",
				error.code().value(), error.what());
			return true;
		}
	}();
	if (useSystemClock)
		return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::utc_clock::now().time_since_epoch()).count();
}
}
