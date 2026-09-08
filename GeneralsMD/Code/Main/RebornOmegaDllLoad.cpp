/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2026 TheSuperHackers
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

// RebornOmegaDLLLoad.cpp ///////////////////////////////////////////////////////////////////////////////////
// "RebornOmegaDLLLoad" - Seperate Retail and Generals Online DLLs from Reborn Omega 
// Author: Gamezerve, September 2026
///////////////////////////////////////////////////////////////////////////////////////////////////

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <delayimp.h>
#include <string.h>
#include <wchar.h>

//-------------------------------------------------------------------------------------------------
/** Reborn: Load the private Generals Online runtime without consulting DLLs beside the executable. */
//-------------------------------------------------------------------------------------------------
static FARPROC WINAPI rebornOmegaDelayLoadHook(unsigned dliNotify, PDelayLoadInfo delayLoadInfo)
{
	if (dliNotify != dliNotePreLoadLibrary || delayLoadInfo == nullptr || delayLoadInfo->szDll == nullptr)
		return nullptr;

	if (_stricmp(delayLoadInfo->szDll, "GameNetworkingSockets.dll") != 0 &&
			_stricmp(delayLoadInfo->szDll, "libcurl.dll") != 0)
	{
		return nullptr;
	}

	wchar_t modulePath[MAX_PATH] = {};
	wchar_t dllName[MAX_PATH] = {};
	wchar_t *fileName = nullptr;
	const size_t modulePathCapacity = sizeof(modulePath) / sizeof(modulePath[0]);
	const size_t dllNameCapacity = sizeof(dllName) / sizeof(dllName[0]);

	DWORD modulePathLength = GetModuleFileNameW(nullptr, modulePath, modulePathCapacity);
	if (modulePathLength == 0 || modulePathLength >= modulePathCapacity)
		goto loadFailure;

	fileName = wcsrchr(modulePath, L'\\');
	if (fileName == nullptr)
		goto loadFailure;

	*(fileName + 1) = 0;

	if (MultiByteToWideChar(CP_ACP, 0, delayLoadInfo->szDll, -1, dllName, dllNameCapacity) == 0)
		goto loadFailure;

	if (wcslen(modulePath) + wcslen(L"RebornOmegaData\\") + wcslen(dllName) >= modulePathCapacity)
		goto loadFailure;

	wcscat_s(modulePath, L"RebornOmegaData\\");
	wcscat_s(modulePath, dllName);

	if (HMODULE module = LoadLibraryExW(modulePath, nullptr,
			LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32))
	{
		return reinterpret_cast<FARPROC>(module);
	}

loadFailure:
	// Reborn: Never fall back to retail GO DLLs beside the executable when the private runtime fails.
	ULONG_PTR exceptionArgument = reinterpret_cast<ULONG_PTR>(delayLoadInfo);
	RaiseException(VcppException(ERROR_SEVERITY_ERROR, ERROR_MOD_NOT_FOUND), 0, 1, &exceptionArgument);
	return nullptr;
}

extern "C" const PfnDliHook __pfnDliNotifyHook2 = rebornOmegaDelayLoadHook;
