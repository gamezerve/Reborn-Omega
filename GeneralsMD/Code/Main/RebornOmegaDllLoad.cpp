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

#include "RebornOmegaDllLoad.h"

namespace {

constexpr const wchar_t *REBORN_OMEGA_RUNTIME_DIRECTORY = L"RebornOmegaData\\";
constexpr const wchar_t *REBORN_OMEGA_RUNTIME_FILES[] = {
	L"GameNetworkingSockets.dll",
	L"libcurl.dll",
	L"libprotobuf.dll",
	L"libcrypto-3.dll",
	L"libssl-3.dll",
	L"abseil_dll.dll",
	L"zlib1.dll",
	L"discord-rpc.dll",
};

//-------------------------------------------------------------------------------------------------
/** Reborn: Build an absolute path inside the private runtime directory. */
//-------------------------------------------------------------------------------------------------
static bool buildRebornOmegaRuntimePath(const wchar_t *fileName, wchar_t *path, size_t pathCapacity)
{
	if (fileName == nullptr || path == nullptr || pathCapacity == 0)
		return false;

	DWORD modulePathLength = GetModuleFileNameW(nullptr, path, static_cast<DWORD>(pathCapacity));
	if (modulePathLength == 0 || modulePathLength >= pathCapacity)
		return false;

	wchar_t *executableName = wcsrchr(path, L'\\');
	if (executableName == nullptr)
		return false;

	*(executableName + 1) = 0;
	if (wcslen(path) + wcslen(REBORN_OMEGA_RUNTIME_DIRECTORY) + wcslen(fileName) >= pathCapacity)
		return false;

	wcscat_s(path, pathCapacity, REBORN_OMEGA_RUNTIME_DIRECTORY);
	wcscat_s(path, pathCapacity, fileName);
	return true;
}

} // namespace

//-------------------------------------------------------------------------------------------------
/** Reborn: Report every missing private runtime DLL before starting the executable. */
//-------------------------------------------------------------------------------------------------
bool validateRebornOmegaRuntime()
{
	wchar_t message[2048] = L"Reborn Omega cannot start because the following required files are missing:\n\n";
	bool missingFile = false;

	for (const wchar_t *fileName : REBORN_OMEGA_RUNTIME_FILES)
	{
		wchar_t filePath[MAX_PATH] = {};
		DWORD fileAttributes = INVALID_FILE_ATTRIBUTES;
		if (buildRebornOmegaRuntimePath(fileName, filePath, sizeof(filePath) / sizeof(filePath[0])))
			fileAttributes = GetFileAttributesW(filePath);

		if (fileAttributes == INVALID_FILE_ATTRIBUTES || (fileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
		{
			missingFile = true;
			wcscat_s(message, L"  RebornOmegaData\\");
			wcscat_s(message, fileName);
			wcscat_s(message, L"\n");
		}
	}

	if (!missingFile)
		return true;

	wcscat_s(message, L"\nPlease repair or reinstall Reborn Omega.");
	MessageBoxW(nullptr, message, L"Reborn Omega - Missing Runtime Files", MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
	return false;
}

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
	const size_t modulePathCapacity = sizeof(modulePath) / sizeof(modulePath[0]);
	const size_t dllNameCapacity = sizeof(dllName) / sizeof(dllName[0]);

	if (MultiByteToWideChar(CP_ACP, 0, delayLoadInfo->szDll, -1, dllName, dllNameCapacity) == 0)
		goto loadFailure;

	if (!buildRebornOmegaRuntimePath(dllName, modulePath, modulePathCapacity))
		goto loadFailure;

	if (HMODULE module = LoadLibraryExW(modulePath, nullptr,
			LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32))
	{
		return reinterpret_cast<FARPROC>(module);
	}

loadFailure:
	// Reborn: Never fall back to retail GO DLLs beside the executable when the private runtime fails.
	wchar_t errorMessage[1024] = {};
	_snwprintf_s(errorMessage, sizeof(errorMessage) / sizeof(errorMessage[0]), _TRUNCATE,
		L"Reborn Omega could not load RebornOmegaData\\%hs.\n\nWindows error: %lu\n\n"
		L"The file may be damaged, incompatible, or one of its dependencies may be invalid. "
		L"Please repair or reinstall Reborn Omega.",
		delayLoadInfo->szDll, GetLastError());
	MessageBoxW(nullptr, errorMessage, L"Reborn Omega - Runtime Load Error",
		MB_OK | MB_ICONERROR | MB_SETFOREGROUND);

	ULONG_PTR exceptionArgument = reinterpret_cast<ULONG_PTR>(delayLoadInfo);
	RaiseException(VcppException(ERROR_SEVERITY_ERROR, ERROR_MOD_NOT_FOUND), 0, 1, &exceptionArgument);
	return nullptr;
}

extern "C" const PfnDliHook __pfnDliNotifyHook2 = rebornOmegaDelayLoadHook;
