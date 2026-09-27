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

// FILE: DownloadMenuRO.cpp /////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
//
//                       Electronic Arts Pacific.
//
//                       Confidential Information
//                Copyright (C) 2002 - All Rights Reserved
//
//-----------------------------------------------------------------------------
//
// Project:   RTS3
//
// File name: DownloadMenuRO.cpp
//
// Created:   Gamezerve, August 2026
//
// Desc:      Reborn Omega Mod Patch Download window control
//
//-----------------------------------------------------------------------------
///////////////////////////////////////////////////////////////////////////////

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/GameEngine.h"
#include "Common/NameKeyGenerator.h"
#include "GameClient/GadgetProgressBar.h"
#include "GameClient/GadgetPushButton.h"
#include "GameClient/GadgetStaticText.h"
#include "GameClient/GameText.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/GameWindowTransitions.h"
#include "GameClient/GUICallbacks.h"
#include "GameClient/KeyDefs.h"
#include "GameClient/MessageBox.h"
#include "GameClient/RebornOmegaUpdater.h"
#include "GameClient/Shell.h"
#include "GameClient/WindowLayout.h"

#include <shellapi.h>

// PRIVATE DATA ///////////////////////////////////////////////////////////////////////////////////
static GameWindow* s_downloadMenuROParent = nullptr;

static GameWindow* s_downloadMenuROProgress = nullptr;
static GameWindow* s_downloadMenuROSize = nullptr;
static GameWindow* s_downloadMenuROTime = nullptr;
static GameWindow* s_downloadMenuROFile = nullptr;
static GameWindow* s_downloadMenuROStatus = nullptr;
static GameWindow* s_downloadMenuROActionButton = nullptr;
static GameWindow* s_downloadMenuROResumeButton = nullptr;
static GameWindow* s_downloadMenuRODownloadedFileName = nullptr;
static Bool s_downloadMenuROCloseRequested = FALSE;

static DWORD s_downloadMenuROStartTime = 0;
static DWORD s_downloadMenuROPauseStartTime = 0;
static DWORD s_downloadMenuROTotalPausedTime = 0;
static DWORD s_downloadMenuROLastUIUpdateTime = 0;
static double s_downloadMenuRODisplayedSpeed = 0.0;
static DWORD s_downloadMenuRODisplayedRemainingTime = 0;

static void CloseDownloadMenuRO()
{
	if (!s_downloadMenuROParent)
		return;
	s_downloadMenuROCloseRequested = FALSE;
	TheShell->pop();
}

void DownloadMenuROInit(WindowLayout* layout, void* userData)
{

	s_downloadMenuROStartTime = timeGetTime();
	s_downloadMenuROPauseStartTime = 0;
	s_downloadMenuROTotalPausedTime = 0;
	s_downloadMenuROLastUIUpdateTime = 0;
	s_downloadMenuRODisplayedSpeed = 0.0;
	s_downloadMenuRODisplayedRemainingTime = 0;

	s_downloadMenuROParent =
		TheWindowManager->winGetWindowFromId(
			nullptr,
			NAMEKEY("DownloadMenuRO.wnd:ParentDownload"));

	s_downloadMenuROProgress =
		TheWindowManager->winGetWindowFromId(
			s_downloadMenuROParent,
			NAMEKEY("DownloadMenuRO.wnd:ProgressBarMunkee"));

	s_downloadMenuROSize =
		TheWindowManager->winGetWindowFromId(
			s_downloadMenuROParent,
			NAMEKEY("DownloadMenuRO.wnd:StaticTextSize"));

	s_downloadMenuRODownloadedFileName =
		TheWindowManager->winGetWindowFromId(
			s_downloadMenuROParent,
			NAMEKEY(
				"DownloadMenuRO.wnd:StaticTextDownloadedFileName"));

	s_downloadMenuROTime =
		TheWindowManager->winGetWindowFromId(
			s_downloadMenuROParent,
			NAMEKEY("DownloadMenuRO.wnd:StaticTextTime"));

	s_downloadMenuROFile =
		TheWindowManager->winGetWindowFromId(
			s_downloadMenuROParent,
			NAMEKEY("DownloadMenuRO.wnd:StaticTextFile"));

	s_downloadMenuROStatus =
		TheWindowManager->winGetWindowFromId(
			s_downloadMenuROParent,
			NAMEKEY("DownloadMenuRO.wnd:StaticTextStatus"));

	if (s_downloadMenuROProgress)
		GadgetProgressBarSetProgress(
			s_downloadMenuROProgress,
			0);

	if (s_downloadMenuROFile)
		GadgetStaticTextSetText(
			s_downloadMenuROFile,
			TheGameText->fetch("GUI:RebornOmegaInstallerFile"));

	if (s_downloadMenuROStatus)
		GadgetStaticTextSetText(
			s_downloadMenuROStatus,
			TheGameText->fetch("GUI:RebornOmegaPreparingDownload"));

	s_downloadMenuROActionButton =
		TheWindowManager->winGetWindowFromId(
			s_downloadMenuROParent,
			NAMEKEY("DownloadMenuRO.wnd:ButtonCancel"));

	s_downloadMenuROResumeButton =
		TheWindowManager->winGetWindowFromId(
			s_downloadMenuROParent,
			NAMEKEY("DownloadMenuRO.wnd:ButtonResume"));

	if (s_downloadMenuROResumeButton)
		s_downloadMenuROResumeButton->winHide(TRUE);

	if (s_downloadMenuROActionButton)
	{
		GadgetButtonSetText(
			s_downloadMenuROActionButton,
			TheGameText->fetch(
				"GUI:Cancel"));
	}

	if (s_downloadMenuRODownloadedFileName)
	{
		UnicodeString versionName;

		versionName.translate(
			GetRebornOmegaDownloadDisplayName().c_str());

		GadgetStaticTextSetText(
			s_downloadMenuRODownloadedFileName,
			versionName);
	}

}

void DownloadMenuROUpdate(WindowLayout* layout, void* userData)
{

	if (s_downloadMenuROCloseRequested)
	{
		CloseDownloadMenuRO();
		return;
	}

	RebornOmegaDownloadState state =
		GetRebornOmegaDownloadState();

	if (
		state ==
		REBORN_DOWNLOAD_DOWNLOADING &&
		s_downloadMenuROActionButton)
	{
		s_downloadMenuROActionButton->
			winEnable(TRUE);

		GadgetButtonSetText(
			s_downloadMenuROActionButton,
			TheGameText->fetch(
				"GUI:PauseDownload"));
	}

	static DWORD lastDownloadLogTime = 0;
	DWORD downloadLogTime = timeGetTime();

	if (downloadLogTime - lastDownloadLogTime >= 1000)
	{
		RebornOmegaDownloadProgress logProgress =
			GetRebornOmegaDownloadProgress();

		DEBUG_LOG((
			"DownloadMenuRO: state=%d downloaded=%lu total=%lu percent=%d paused=%lu elapsed=%lu\n",
			static_cast<Int>(state),
			logProgress.downloadedBytes,
			logProgress.totalBytes,
			logProgress.percent,
			s_downloadMenuROPauseStartTime,
			downloadLogTime - s_downloadMenuROStartTime));

		lastDownloadLogTime = downloadLogTime;
	}

	if (state == REBORN_DOWNLOAD_COMPLETED)
	{
		RebornOmegaDownloadProgress completedProgress =
			GetRebornOmegaDownloadProgress();

		if (s_downloadMenuROProgress)
		{
			GadgetProgressBarSetProgress(
				s_downloadMenuROProgress,
				100);
		}

		if (s_downloadMenuROSize)
		{
			UnicodeString sizeText;

			sizeText.format(
				L"%.1f MB / %.1f MB",
				completedProgress.totalBytes / 1048576.0,
				completedProgress.totalBytes / 1048576.0);

			GadgetStaticTextSetText(
				s_downloadMenuROSize,
				sizeText);
		}

		if (s_downloadMenuROStatus)
			s_downloadMenuROStatus->winHide(TRUE);

		if (s_downloadMenuROTime)
		{
			s_downloadMenuROTime->winHide(FALSE);

			GadgetStaticTextSetText(
				s_downloadMenuROTime,
				TheGameText->fetch(
					"GUI:RebornOmegaDownloadComplete"));
		}

		if (s_downloadMenuROResumeButton)
		{
			s_downloadMenuROResumeButton->winHide(FALSE);

			GadgetButtonSetText(
				s_downloadMenuROResumeButton,
				TheGameText->fetch(
					"GUI:InstallUpdate"));
		}

		if (s_downloadMenuROActionButton)
		{
			s_downloadMenuROActionButton->winHide(FALSE);

			GadgetButtonSetText(
				s_downloadMenuROActionButton,
				TheGameText->fetch(
					"GUI:Cancel"));
		}

		return;
	}

	if (state == REBORN_DOWNLOAD_PAUSED)
	{
		if (s_downloadMenuROPauseStartTime == 0)
			s_downloadMenuROPauseStartTime = timeGetTime();

		if (s_downloadMenuROStatus)
			s_downloadMenuROStatus->winHide(TRUE);

		if (s_downloadMenuROResumeButton)
		{
			s_downloadMenuROResumeButton->winHide(FALSE);

			GadgetButtonSetText(
				s_downloadMenuROResumeButton,
				TheGameText->fetch(
					"GUI:ResumeDownload"));
		}

		if (s_downloadMenuROActionButton)
		{
			GadgetButtonSetText(
				s_downloadMenuROActionButton,
				TheGameText->fetch("GUI:Cancel"));
		}

		RebornOmegaDownloadProgress pausedProgress =
			GetRebornOmegaDownloadProgress();

		if (s_downloadMenuROProgress)
		{
			GadgetProgressBarSetProgress(
				s_downloadMenuROProgress,
				pausedProgress.percent);
		}

		if (s_downloadMenuROSize)
		{
			UnicodeString sizeText;

			if (pausedProgress.totalBytes > 0)
			{
				sizeText.format(
					L"%.1f MB / %.1f MB",
					pausedProgress.downloadedBytes / 1048576.0,
					pausedProgress.totalBytes / 1048576.0);
			}
			else
			{
				sizeText.format(
					L"%.1f MB",
					pausedProgress.downloadedBytes / 1048576.0);
			}

			GadgetStaticTextSetText(
				s_downloadMenuROSize,
				sizeText);
		}

		DWORD pausedElapsedMilliseconds =
			s_downloadMenuROPauseStartTime -
			s_downloadMenuROStartTime -
			s_downloadMenuROTotalPausedTime;

		DWORD pausedElapsedTotalSeconds =
			pausedElapsedMilliseconds / 1000;

		Int pausedElapsedHours =
			pausedElapsedTotalSeconds / 3600;

		Int pausedElapsedMinutes =
			(pausedElapsedTotalSeconds % 3600) / 60;

		Int pausedElapsedRemainingSeconds =
			pausedElapsedTotalSeconds % 60;

		if (s_downloadMenuROTime)
		{
			UnicodeString pausedTimingText;

			pausedTimingText.format(
				TheGameText->fetch(
					"GUI:RebornOmegaDownloadPausedTiming").str(),
				pausedElapsedHours,
				pausedElapsedMinutes,
				pausedElapsedRemainingSeconds);

			GadgetStaticTextSetText(
				s_downloadMenuROTime,
				pausedTimingText);
		}

		return;
	}

	if (state == REBORN_DOWNLOAD_FAILED)
	{
		if (s_downloadMenuROStatus)
			s_downloadMenuROStatus->winHide(TRUE);

		if (s_downloadMenuROTime)
		{
			GadgetStaticTextSetText(
				s_downloadMenuROTime,
				TheGameText->fetch(
					"GUI:RebornOmegaDownloadFailed"));
		}

		if (s_downloadMenuROResumeButton)
		{
			s_downloadMenuROResumeButton->winHide(FALSE);

			GadgetButtonSetText(
				s_downloadMenuROResumeButton,
				TheGameText->fetch(
					"GUI:RetryDownload"));
		}

		if (s_downloadMenuROActionButton)
		{
			GadgetButtonSetText(
				s_downloadMenuROActionButton,
				TheGameText->fetch("GUI:Cancel"));
		}

		return;
	}

	if (state != REBORN_DOWNLOAD_STARTING &&
		state != REBORN_DOWNLOAD_DOWNLOADING)
	{
		return;
	}

	static DWORD lastUIUpdateTime = 0;

	if (state == REBORN_DOWNLOAD_DOWNLOADING)
	{
		DWORD now = timeGetTime();

		if (s_downloadMenuROPauseStartTime != 0)
		{
			s_downloadMenuROTotalPausedTime +=
				now - s_downloadMenuROPauseStartTime;

			s_downloadMenuROPauseStartTime = 0;
		}

		if (s_downloadMenuROStatus)
			s_downloadMenuROStatus->winHide(FALSE);

		if (s_downloadMenuROResumeButton)
			s_downloadMenuROResumeButton->winHide(TRUE);

		if (s_downloadMenuROActionButton)
		{
			GadgetButtonSetText(
				s_downloadMenuROActionButton,
				TheGameText->fetch("GUI:PauseDownload"));
		}
	}

	RebornOmegaDownloadProgress progress =
		GetRebornOmegaDownloadProgress();

	DWORD now = timeGetTime();

	DWORD elapsedMilliseconds =
		now -
		s_downloadMenuROStartTime -
		s_downloadMenuROTotalPausedTime;

	double elapsedSeconds =
		elapsedMilliseconds / 1000.0;

	if (now - s_downloadMenuROLastUIUpdateTime >= 1000 ||
		s_downloadMenuROLastUIUpdateTime == 0)
	{
		s_downloadMenuRODisplayedSpeed = 0.0;
		s_downloadMenuRODisplayedRemainingTime = 0;

		if (elapsedSeconds > 0.0)
		{
			s_downloadMenuRODisplayedSpeed =
				progress.downloadedBytes /
				elapsedSeconds;
		}

		if (s_downloadMenuRODisplayedSpeed > 0.0 &&
			progress.totalBytes >
			progress.downloadedBytes)
		{
			s_downloadMenuRODisplayedRemainingTime =
				static_cast<DWORD>(
					(progress.totalBytes -
						progress.downloadedBytes) /
					s_downloadMenuRODisplayedSpeed);
		}

		s_downloadMenuROLastUIUpdateTime = now;
	}

	double bytesPerSecond =
		s_downloadMenuRODisplayedSpeed;

	DWORD remainingSeconds =
		s_downloadMenuRODisplayedRemainingTime;

	if (s_downloadMenuROProgress)
	{
		GadgetProgressBarSetProgress(
			s_downloadMenuROProgress,
			progress.percent);
	}

	if (s_downloadMenuROTime)
	{
		DWORD elapsedTotalSeconds =
			elapsedMilliseconds / 1000;

		Int elapsedHours =
			elapsedTotalSeconds / 3600;

		Int elapsedMinutes =
			(elapsedTotalSeconds % 3600) / 60;

		Int elapsedRemainingSeconds =
			elapsedTotalSeconds % 60;

		Int remainingHours =
			remainingSeconds / 3600;

		Int remainingMinutes =
			(remainingSeconds % 3600) / 60;

		Int remainingRemainingSeconds =
			remainingSeconds % 60;

		UnicodeString timingText;

		timingText.format(
			TheGameText->fetch(
				"GUI:RebornOmegaDownloadTiming").str(),
			bytesPerSecond / 1048576.0,
			remainingHours,
			remainingMinutes,
			remainingRemainingSeconds,
			elapsedHours,
			elapsedMinutes,
			elapsedRemainingSeconds);

		GadgetStaticTextSetText(
			s_downloadMenuROTime,
			timingText);
	}

	if (s_downloadMenuROSize)
	{
		UnicodeString sizeText;

		if (progress.totalBytes > 0)
		{
			sizeText.format(
				L"%.1f MB / %.1f MB",
				progress.downloadedBytes / 1048576.0,
				progress.totalBytes / 1048576.0);
		}
		else
		{
			sizeText.format(
				L"%.1f MB",
				progress.downloadedBytes / 1048576.0);
		}

		GadgetStaticTextSetText(
			s_downloadMenuROSize,
			sizeText);
	}

	if (s_downloadMenuROStatus)
	{
		UnicodeString statusText;

		statusText.format(
			TheGameText->fetch(
				"GUI:RebornOmegaDownloading").str(),
			progress.percent);

		GadgetStaticTextSetText(
			s_downloadMenuROStatus,
			statusText);
	}
}

void DownloadMenuROShutdown(
	WindowLayout* layout,
	void* userData)
{
	if (layout)
		layout->hide(TRUE);

	s_downloadMenuROProgress = nullptr;
	s_downloadMenuROSize = nullptr;
	s_downloadMenuROTime = nullptr;
	s_downloadMenuROFile = nullptr;
	s_downloadMenuROStatus = nullptr;
	s_downloadMenuROActionButton = nullptr;
	s_downloadMenuROResumeButton = nullptr;
	s_downloadMenuROParent = nullptr;
	s_downloadMenuRODownloadedFileName = nullptr;
	s_downloadMenuROCloseRequested = FALSE;

	TheWindowManager->winSetModal(nullptr);
	TheWindowManager->winSetFocus(nullptr);

	GameWindow* mainMenuWindow =
		TheWindowManager->winGetWindowFromId(
			nullptr,
			GetMainMenuWindowKey("MainMenuParent")); // Reborn: Follow the loaded Main Menu theme.

	if (mainMenuWindow)
	{
		mainMenuWindow->winHide(FALSE);
		mainMenuWindow->winEnable(TRUE);
	}

	TheShell->shutdownComplete(layout);
}

WindowMsgHandledType DownloadMenuROSystem(
	GameWindow* window,
	UnsignedInt msg,
	WindowMsgData mData1,
	WindowMsgData mData2)
{
	if (msg == GBM_SELECTED)
	{
		GameWindow* control =
			reinterpret_cast<GameWindow*>(mData1);

		if (!control)
			return MSG_IGNORED;

		Int controlID =
			control->winGetWindowId();

		if (controlID ==
			NAMEKEY("DownloadMenuRO.wnd:ButtonResume"))
		{
			RebornOmegaDownloadState state =
				GetRebornOmegaDownloadState();

			if (s_downloadMenuRODownloadedFileName)
			{
				UnicodeString versionName;

				versionName.translate(
					GetRebornOmegaDownloadDisplayName().c_str());

				GadgetStaticTextSetText(
					s_downloadMenuRODownloadedFileName,
					versionName);
			}

			if (state == REBORN_DOWNLOAD_COMPLETED)
			{
				const std::string& installerPath =
					GetRebornOmegaDownloadedInstallerPath();

				if (installerPath.empty() ||
					GetFileAttributesA(installerPath.c_str()) ==
					INVALID_FILE_ATTRIBUTES)
				{
					MessageBoxOk(
						TheGameText->fetch(
							"GUI:CheckForUpdates"),
						TheGameText->fetch(
							"GUI:RebornOmegaInstallerLaunchFailed"),
						nullptr);

					return MSG_HANDLED;
				}

				HINSTANCE result = ShellExecuteA(
					nullptr,
					"open",
					installerPath.c_str(),
					nullptr,
					nullptr,
					SW_SHOWNORMAL);

				if (reinterpret_cast<INT_PTR>(result) <= 32)
				{
					MessageBoxOk(
						TheGameText->fetch(
							"GUI:CheckForUpdates"),
						TheGameText->fetch(
							"GUI:RebornOmegaInstallerLaunchFailed"),
						nullptr);

					return MSG_HANDLED;
				}

				TheGameEngine->setQuitting(TRUE);
				return MSG_HANDLED;
			}

			DEBUG_LOG((
				"DownloadMenuRO: Resume button selected state=%d\n",
				static_cast<Int>(state)));

			if (state == REBORN_DOWNLOAD_PAUSED)
			{
				ResumeRebornOmegaInstallerDownload();
				return MSG_HANDLED;
			}

			if (state == REBORN_DOWNLOAD_FAILED)
			{
				if (RetryRebornOmegaInstallerDownload())
				{
					s_downloadMenuROStartTime =
						timeGetTime();

					s_downloadMenuROPauseStartTime = 0;
					s_downloadMenuROTotalPausedTime = 0;
					s_downloadMenuROLastUIUpdateTime = 0;
					s_downloadMenuRODisplayedSpeed = 0.0;
					s_downloadMenuRODisplayedRemainingTime = 0;

					if (s_downloadMenuROProgress)
					{
						GadgetProgressBarSetProgress(
							s_downloadMenuROProgress,
							0);
					}

					if (s_downloadMenuROSize)
					{
						GadgetStaticTextSetText(
							s_downloadMenuROSize,
							L"0.0 MB");
					}
				}

				return MSG_HANDLED;
			}

			if (state == REBORN_DOWNLOAD_COMPLETED)
			{
				s_downloadMenuROCloseRequested = TRUE;
				return MSG_HANDLED;
			}

		}

		if (controlID ==
			NAMEKEY("DownloadMenuRO.wnd:ButtonCancel"))
		{
			RebornOmegaDownloadState state =
				GetRebornOmegaDownloadState();

			if (state == REBORN_DOWNLOAD_COMPLETED)
			{
				const std::string& installerPath =
					GetRebornOmegaDownloadedInstallerPath();

				if (!installerPath.empty())
					DeleteFileA(installerPath.c_str());

				FinishRebornOmegaInstallerDownload();

				s_downloadMenuROCloseRequested = TRUE;
				return MSG_HANDLED;
			}

			if (state == REBORN_DOWNLOAD_DOWNLOADING)
			{
				PauseRebornOmegaInstallerDownload();
				return MSG_HANDLED;
			}

			if (state == REBORN_DOWNLOAD_PAUSED)
			{
				CancelRebornOmegaInstallerDownload();
				s_downloadMenuROCloseRequested = TRUE;
				return MSG_HANDLED;
			}

			if (state == REBORN_DOWNLOAD_FAILED)
			{
				FinishRebornOmegaInstallerDownload();
				s_downloadMenuROCloseRequested = TRUE;
				return MSG_HANDLED;
			}

			if (state == REBORN_DOWNLOAD_STARTING)
			{
				CancelRebornOmegaInstallerDownload();

				s_downloadMenuROCloseRequested =
					TRUE;

				return MSG_HANDLED;
			}

		}
	}

	return MSG_IGNORED;
}

WindowMsgHandledType DownloadMenuROInput(
	GameWindow* window,
	UnsignedInt msg,
	WindowMsgData mData1,
	WindowMsgData mData2)
{
	if (msg == GWM_CHAR &&
		static_cast<UnsignedByte>(mData1) == KEY_ESC &&
		BitIsSet(
			static_cast<UnsignedByte>(mData2),
			KEY_STATE_UP))
	{
		CancelRebornOmegaInstallerDownload();
		CloseDownloadMenuRO();
		return MSG_HANDLED;
	}

	return MSG_IGNORED;
}
