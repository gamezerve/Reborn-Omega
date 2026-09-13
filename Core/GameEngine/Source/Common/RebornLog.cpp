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

// FILE: RebornLog.cpp /////////////////////////////////////////////////////////
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
// File name:  RebornLog.cpp ///////////////////////////////////////////////////
//
// Created:    Gamezerve, July 2026
//
// Desc:       Debug Utilities
//
//-----------------------------------------------------------------------------
///////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"
#include "Common/RebornLog.h"

#include <windows.h>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <algorithm>
#include <string>
#include <vector>

#ifndef REBORN_OMEGA_DISPLAY_NAME
#define REBORN_OMEGA_DISPLAY_NAME "Reborn Omega Unknown"
#endif

namespace
{
	std::once_flag g_logInitFlag;
	std::mutex g_logMutex;
	char g_logsDirectory[MAX_PATH] = {};
	char g_logPath[MAX_PATH] = {};

	void initializeLogDirectory()
	{
		char exePath[MAX_PATH] = {};
		GetModuleFileNameA(nullptr, exePath, MAX_PATH);

		char* separator = strrchr(exePath, '\\');
		if (separator)
			*separator = '\0';

		char statusDirectory[MAX_PATH] = {};

		snprintf(
			statusDirectory,
			MAX_PATH,
			"%s\\RebornOmegaStatus",
			exePath);

		snprintf(
			g_logsDirectory,
			MAX_PATH,
			"%s\\RebornOmegaStatus\\Logs",
			exePath);

		CreateDirectoryA(statusDirectory, nullptr);
		CreateDirectoryA(g_logsDirectory, nullptr);
	}

	void buildLogPath(char* path, size_t pathSize, const SYSTEMTIME& time)
	{
		snprintf(
			path,
			pathSize,
			"%s\\RebornOmegaLog_%04u-%02u-%02u_%02u-%02u-%02u-%03u.txt",
			g_logsDirectory,
			time.wYear,
			time.wMonth,
			time.wDay,
			time.wHour,
			time.wMinute,
			time.wSecond,
			time.wMilliseconds);
	}

	const char* getFilename(const char* path)
	{
		if (!path)
			return "Unknown";

		const char* slash = strrchr(path, '\\');

		if (!slash)
			slash = strrchr(path, '/');

		return slash ? slash + 1 : path;
	}
}

struct LogFileInfo
{
	std::string path;
	FILETIME lastWriteTime;
};

void deleteOldLogs()
{
	char searchPath[MAX_PATH] = {};

	snprintf(
		searchPath,
		ARRAY_SIZE(searchPath),
		"%s\\RebornOmegaLog_*.txt",
		g_logsDirectory);

	WIN32_FIND_DATAA findData = {};
	HANDLE findHandle = FindFirstFileA(searchPath, &findData);

	if (findHandle == INVALID_HANDLE_VALUE)
		return;

	std::vector<LogFileInfo> logFiles;

	do
	{
		if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
		{
			char filePath[MAX_PATH] = {};

			snprintf(
				filePath,
				ARRAY_SIZE(filePath),
				"%s\\%s",
				g_logsDirectory,
				findData.cFileName);

			LogFileInfo info;
			info.path = filePath;
			info.lastWriteTime = findData.ftLastWriteTime;
			logFiles.push_back(info);
		}
	} while (FindNextFileA(findHandle, &findData));

	FindClose(findHandle);

	if (logFiles.size() <= 10)
		return;

	std::sort(
		logFiles.begin(),
		logFiles.end(),
		[](const LogFileInfo& left, const LogFileInfo& right)
		{
			ULARGE_INTEGER leftTime;
			leftTime.LowPart = left.lastWriteTime.dwLowDateTime;
			leftTime.HighPart = left.lastWriteTime.dwHighDateTime;

			ULARGE_INTEGER rightTime;
			rightTime.LowPart = right.lastWriteTime.dwLowDateTime;
			rightTime.HighPart = right.lastWriteTime.dwHighDateTime;

			return leftTime.QuadPart < rightTime.QuadPart;
		});

	const size_t deleteCount = logFiles.size() - 10;

	for (size_t i = 0; i < deleteCount; ++i)
	{
		DeleteFileA(logFiles[i].path.c_str());
	}
}

void RebornLog::Write(
	const char* sourceFile,
	int sourceLine,
	const char* functionName,
	const char* format,
	...)
{
	std::call_once(g_logInitFlag, initializeLogDirectory);
	std::lock_guard<std::mutex> lock(g_logMutex);

	SYSTEMTIME time;
	GetLocalTime(&time);

	if (g_logPath[0] == '\0')
	{
		buildLogPath(g_logPath, ARRAY_SIZE(g_logPath), time);
		deleteOldLogs();
	}

	char message[4096] = {};

	va_list args;
	va_start(args, format);
	vsnprintf(message, sizeof(message), format, args);
	va_end(args);



	char output[8192] = {};

	snprintf(
		output,
		sizeof(output),
		"[%04u-%02u-%02u %02u:%02u:%02u.%03u] File=%s Line=%d Function=%s Version=%s\n%s\n\n",
		time.wYear,
		time.wMonth,
		time.wDay,
		time.wHour,
		time.wMinute,
		time.wSecond,
		time.wMilliseconds,
		getFilename(sourceFile),
		sourceLine,
		functionName ? functionName : "Unknown",
		REBORN_OMEGA_DISPLAY_NAME,
		message);

	HANDLE file = CreateFileA(
		g_logPath,
		FILE_APPEND_DATA,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		nullptr,
		OPEN_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		nullptr);

	if (file == INVALID_HANDLE_VALUE)
		return;

	DWORD bytesWritten = 0;

	WriteFile(
		file,
		output,
		static_cast<DWORD>(strlen(output)),
		&bytesWritten,
		nullptr);

	FlushFileBuffers(file);
	CloseHandle(file);
}
