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

// FILE: RebornLog.h ///////////////////////////////////////////////////////////
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
// File name:  RebornLog.h ////////////////////////////////////////////////////
//
// Created:    Gamezerve, July 2026
//
// Desc:       Debug Utilities
//
//-----------------------------------------------------------------------------
///////////////////////////////////////////////////////////////////////////////

#pragma once

namespace RebornLog
{
	// Reborn: Error logging uses fixed buffers and Win32 I/O, including during allocator failures.
	void WriteErrorCode(const char* sourceFile, int sourceLine, const char* functionName,
		const char* errorName, unsigned int errorCode, const char* context) noexcept;
	void Write(
		const char* sourceFile,
		int sourceLine,
		const char* functionName,
		const char* format,
		...);
}

#define REBORN_LOG(...) \
	RebornLog::Write(__FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)

// Reborn: Log before evaluation completes, preserving the original enum type and single-statement throw.
#define REBORN_LOG_ERROR_CODE(errorCode, context) \
	(RebornLog::WriteErrorCode(__FILE__, __LINE__, __FUNCTION__, #errorCode, \
		static_cast<unsigned int>(errorCode), context), (errorCode))
