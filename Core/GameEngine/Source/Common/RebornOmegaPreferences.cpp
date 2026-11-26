/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
////////////////////////////////////////////////////////////////////////////////

// FILE: RebornOmegaPreferences.cpp /////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
//
//                       Westwood Studios Pacific.
//
//                       Confidential Information
//                Copyright (C) 2001 - All Rights Reserved
//
//-----------------------------------------------------------------------------
//
// Project:    Reborn Omega
//
// File name:  RebornOmegaPreferences.cpp ///////////////////////////////////////////////////
//
// Created:    Gamezerve, August 2026
//
// Desc:       Debug Utilities
//
//-----------------------------------------------------------------------------
///////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"

#include "Common/GlobalData.h"
#include "Common/RebornOmegaPreferences.h"
#include "Common/UserPreferences.h"

static const char* REBORN_OMEGA_OPTIONS_FILE =
"RebornOmegaOptions\\RebornOmegaOptions.ini";

Bool LoadRebornOmegaPreferences(UserPreferences& preferences)
{
	return preferences.load(REBORN_OMEGA_OPTIONS_FILE);
}

Bool WriteRebornOmegaPreferences(UserPreferences& preferences)
{
	AsciiString directory = TheGlobalData->getPath_UserData();
	directory.concat("RebornOmegaOptions");

	CreateDirectoryA(directory.str(), nullptr);

	return preferences.write();
}

// Reborn: Existing installations continue showing the counters until the player turns them off.
static Bool s_showHudClocks = TRUE;
static Bool s_showHudPerformance = TRUE;

//-------------------------------------------------------------------------------------------------
/** Reborn: Return the accepted visibility for the system clock and elapsed game time. */
//-------------------------------------------------------------------------------------------------
Bool GetShowHudClocks() { return s_showHudClocks; }

//-------------------------------------------------------------------------------------------------
/** Reborn: Return the accepted visibility for FPS and network run-ahead. */
//-------------------------------------------------------------------------------------------------
Bool GetShowHudPerformance() { return s_showHudPerformance; }

//-------------------------------------------------------------------------------------------------
/** Reborn: Apply HUD visibility immediately without recreating display strings or changing their fonts. */
//-------------------------------------------------------------------------------------------------
void SetHudCounterVisibility(Bool clocks, Bool performance)
{
	s_showHudClocks = clocks;
	s_showHudPerformance = performance;
}
