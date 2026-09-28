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

// FILE: ChangeLogMenuRO.cpp ///////////////////////////////////////////////////////////////////////
// Author: Gamezerve, August 2026
// Desc: GUI callbacks for the Reborn Omega version changelog menu
///////////////////////////////////////////////////////////////////////////////////////////////////

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "BuildVersion.h"
#include "Common/NameKeyGenerator.h"
#include "GameClient/CampaignManager.h" // Reborn: Resolve the selected theme for Options-owned changelog overlays.
#include "GameClient/GadgetListBox.h"
#include "GameClient/GadgetPushButton.h"
#include "GameClient/GadgetStaticText.h"
#include "GameClient/GameText.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/GUICallbacks.h"
#include "GameClient/KeyDefs.h"
#include "GameClient/MessageBox.h"
#include "GameClient/RebornOmegaUpdater.h"
#include "GameClient/Shell.h"
#include "GameClient/WindowLayout.h"

#include <shellapi.h>
#include <string>
#include <vector>

static GameWindow* s_changeLogMenuROParent = nullptr;
static GameWindow* s_changeLogMenuROListBox = nullptr;
static Bool s_changeLogMenuROCloseRequested = FALSE;
static Bool s_changeLogMenuROOptionsOverlay = FALSE;
static Bool s_changeLogMenuRODownloadRequested = FALSE;
static Bool s_changeLogMenuRORestoreModalRequested = FALSE;
static Bool s_changeLogMenuROBringConfirmationToTopRequested = FALSE;

static GameWindow* s_changeLogMenuROVersionLabel = nullptr;
static GameWindow* s_changeLogMenuROPreviousButton = nullptr;
static GameWindow* s_changeLogMenuRONextButton = nullptr;
static GameWindow* s_changeLogMenuRODecreaseFontSizeButton = nullptr;
static GameWindow* s_changeLogMenuROIncreaseFontSizeButton = nullptr;
static GameWindow* s_changeLogMenuROInstallButton = nullptr;
static GameWindow* s_changeLogMenuROInstallConfirmationMessageBox = nullptr;
static GameWindow* s_changeLogMenuROInstallConfirmation = nullptr;
static GameWindow* s_changeLogMenuROVersionInfo = nullptr;
static GameWindow* s_changeLogMenuROVisitButton = nullptr;
static GameWindow* s_changeLogMenuRORefreshButton = nullptr;

static const Int s_changeLogMenuROMinimumFontSize = 10;
static const Int s_changeLogMenuROMaximumFontSize = 20;
static Int s_changeLogMenuROFontSize = 14;

static std::vector<RebornOmegaVersionInfo>
s_changeLogMenuROVersions;
static std::vector<std::string>
s_changeLogMenuROLinkUrls;
static std::string
s_changeLogMenuROPendingInstallUrl;

static Int s_changeLogMenuROSelectedVersion = -1;
static Int s_changeLogMenuRODownloadingVersion = -1;

static DWORD s_changeLogMenuROLastLoadingUpdate = 0;
static Int s_changeLogMenuROLoadingDots = 0;

static void UpdateChangeLogMenuRORefreshButton()
{
	if (!s_changeLogMenuRORefreshButton)
		return;

	Bool enabled = FALSE;

	if (
		s_changeLogMenuROSelectedVersion >= 0 &&
		s_changeLogMenuROSelectedVersion <
		static_cast<Int>(
			s_changeLogMenuROVersions.size()) &&
		GetRebornOmegaChangeLogDownloadState() !=
		REBORN_CHANGELOG_DOWNLOADING)
	{
		const RebornOmegaVersionInfo& version =
			s_changeLogMenuROVersions[
				s_changeLogMenuROSelectedVersion];

		enabled =
			version.changeLog.empty() &&
			!version.pageUrl.empty();
	}

	s_changeLogMenuRORefreshButton->
		winEnable(enabled);
}

static void UpdateChangeLogMenuROVisitButton()
{
	if (!s_changeLogMenuROVisitButton)
		return;

	Bool enabled = FALSE;

	if (
		s_changeLogMenuROSelectedVersion >= 0 &&
		s_changeLogMenuROSelectedVersion <
		static_cast<Int>(
			s_changeLogMenuROVersions.size()))
	{
		const std::string& url =
			s_changeLogMenuROVersions[
				s_changeLogMenuROSelectedVersion].
			pageUrl;

				enabled =
					url.compare(0, 8, "https://") == 0 ||
					url.compare(0, 7, "http://") == 0;
	}

	s_changeLogMenuROVisitButton->
		winEnable(enabled);
}

static void UpdateChangeLogMenuROInstallButton()
{
	if (!s_changeLogMenuROInstallButton)
		return;

	Bool enabled = FALSE;

	if (
		s_changeLogMenuROSelectedVersion >= 0 &&
		s_changeLogMenuROSelectedVersion <
		static_cast<Int>(
			s_changeLogMenuROVersions.size()))
	{
		const RebornOmegaVersionInfo& version =
			s_changeLogMenuROVersions[
				s_changeLogMenuROSelectedVersion];

		enabled =	version.buildRank >	REBORN_OMEGA_BUILD_RANK && !version.downloadStartUrl.empty();
		//enabled =	version.buildRank >	10399 && !version.downloadStartUrl.empty();
	}

	Int selectedRank = -1;
	Bool hasDownloadUrl = FALSE;

	if (
		s_changeLogMenuROSelectedVersion >= 0 &&
		s_changeLogMenuROSelectedVersion <
		static_cast<Int>(
			s_changeLogMenuROVersions.size()))
	{
		const RebornOmegaVersionInfo& selectedVersion =
			s_changeLogMenuROVersions[
				s_changeLogMenuROSelectedVersion];

		selectedRank =
			selectedVersion.buildRank;

		hasDownloadUrl =
			!selectedVersion.downloadStartUrl.empty();
	}

	DEBUG_LOG((
		"ChangeLog INSTALL update: enabled=%d index=%d selectedRank=%d installedRank=%d hasUrl=%d\n",
		enabled,
		s_changeLogMenuROSelectedVersion,
		selectedRank,
		REBORN_OMEGA_BUILD_RANK,
		hasDownloadUrl));

	s_changeLogMenuROInstallButton->
		winEnable(enabled);
}

static std::string CreateChangeLogDivider()
{
	if (!s_changeLogMenuROListBox)
		return std::string();

	Int width = 0;
	Int height = 0;

	s_changeLogMenuROListBox->winGetSize(
		&width,
		&height);

	Int characterCount = width / 8;

	if (characterCount < 1)
		characterCount = 1;

	return std::string(
		static_cast<size_t>(characterCount),
		'-');
}

static Bool RemoveChangeLogStyleMarker(
	std::string& text,
	const char* marker)
{
	const std::string markerText =
		marker;

	Bool styleEntireLine =
		text.find(markerText) == 0;

	size_t position = 0;

	while (
		(position =
			text.find(
				markerText,
				position)) !=
		std::string::npos)
	{
		text.erase(
			position,
			markerText.size());
	}

	return styleEntireLine;
}

static void UpdateChangeLogMenuROFontButtons()
{
	if (s_changeLogMenuRODecreaseFontSizeButton)
	{
		s_changeLogMenuRODecreaseFontSizeButton->
			winEnable(
				s_changeLogMenuROFontSize >
				s_changeLogMenuROMinimumFontSize);
	}

	if (s_changeLogMenuROIncreaseFontSizeButton)
	{
		s_changeLogMenuROIncreaseFontSizeButton->
			winEnable(
				s_changeLogMenuROFontSize <
				s_changeLogMenuROMaximumFontSize);
	}
}

static void PopulateChangeLogMenuRO(const char* changeLog)
{

	if (!s_changeLogMenuROListBox)
		return;

	GadgetListBoxReset(s_changeLogMenuROListBox);
	s_changeLogMenuROLinkUrls.clear();

	if (!changeLog || !*changeLog)
		return;

	std::string text = changeLog;

	const std::string dividerMarker =
		"REBORN_OMEGA_CHANGELOG_DIVIDER";

	size_t dividerPosition =
		text.find(dividerMarker);

	if (dividerPosition != std::string::npos)
	{
		text.replace(
			dividerPosition,
			dividerMarker.size(),
			CreateChangeLogDivider());
	}

	size_t position = 0;

	Bool previousLineEmpty = TRUE;
	Bool bulletPending = FALSE;

	while (position <= text.size())
	{
		size_t lineEnd = text.find('\n', position);

		std::string line =
			text.substr(
				position,
				lineEnd == std::string::npos
				? std::string::npos
				: lineEnd - position);

		if (!line.empty() && line[line.size() - 1] == '\r')
			line.erase(line.size() - 1);

		size_t textStart =
			line.find_first_not_of(" \t");

		if (textStart == std::string::npos)
		{
			line.clear();
		}
		else
		{
			size_t textEnd =
				line.find_last_not_of(" \t");

			line =
				line.substr(
					textStart,
					textEnd - textStart + 1);
		}

		Bool heading1 =
			RemoveChangeLogStyleMarker(
				line,
				"REBORN_OMEGA_STYLE_H1");

		Bool heading2 =
			RemoveChangeLogStyleMarker(
				line,
				"REBORN_OMEGA_STYLE_H2");

		Bool heading3 =
			RemoveChangeLogStyleMarker(
				line,
				"REBORN_OMEGA_STYLE_H3");

		Bool bold =
			RemoveChangeLogStyleMarker(
				line,
				"REBORN_OMEGA_STYLE_BOLD");

		if (line == "-")
		{
			bulletPending = TRUE;
			line.clear();
		}
		else if (bulletPending && !line.empty())
		{
			line.insert(0, "- ");
			bulletPending = FALSE;
		}

		Bool lineEmpty = line.empty();

		std::string linkUrl;

		const std::string linkBegin =
			"REBORN_OMEGA_LINK_BEGIN";

		const std::string linkTextMarker =
			"REBORN_OMEGA_LINK_TEXT";

		const std::string linkEnd =
			"REBORN_OMEGA_LINK_END";

		size_t linkBeginPosition =
			line.find(linkBegin);

		if (linkBeginPosition != std::string::npos)
		{
			size_t urlStart =
				linkBeginPosition +
				linkBegin.size();

			size_t linkTextPosition =
				line.find(
					linkTextMarker,
					urlStart);

			if (linkTextPosition != std::string::npos)
			{
				size_t linkEndPosition =
					line.find(
						linkEnd,
						linkTextPosition +
						linkTextMarker.size());

				if (linkEndPosition != std::string::npos)
				{
					linkUrl =
						line.substr(
							urlStart,
							linkTextPosition - urlStart);

					std::string visibleLinkText =
						line.substr(
							linkTextPosition +
							linkTextMarker.size(),
							linkEndPosition -
							linkTextPosition -
							linkTextMarker.size());

					line =
						line.substr(
							0,
							linkBeginPosition) +
						visibleLinkText +
						line.substr(
							linkEndPosition +
							linkEnd.size());
				}
			}
		}

		if (!lineEmpty || !previousLineEmpty)
		{
			UnicodeString translated;
			translated.translate(line.c_str());

			Color textColor =
				linkUrl.empty()
				? GameMakeColor(254, 254, 254, 255)
				: GameMakeColor(80, 160, 255, 255);

			GameFont* entryFont =
				TheFontLibrary->getFont(
					"Arial",
					s_changeLogMenuROFontSize,
					FALSE);

			if (heading1 || heading2)
			{
				entryFont =
					TheFontLibrary->getFont(
						"Arial",
						s_changeLogMenuROFontSize + 6,
						TRUE);
			}
			else if (heading3)
			{
				entryFont =
					TheFontLibrary->getFont(
						"Arial",
						s_changeLogMenuROFontSize + 2,
						TRUE);
			}
			else if (bold)
			{
				entryFont =
					TheFontLibrary->getFont(
						"Arial",
						s_changeLogMenuROFontSize,
						TRUE);
			}

			GadgetListBoxAddEntryTextWithFont(
				s_changeLogMenuROListBox,
				translated,
				textColor,
				entryFont,
				-1);

			s_changeLogMenuROLinkUrls.push_back(
				linkUrl);
		}

		previousLineEmpty = lineEmpty;

		if (lineEnd == std::string::npos)
			break;

		position = lineEnd + 1;
	}
}

static void SetChangeLogMenuROStatus(
	const UnicodeString& text)
{
	if (!s_changeLogMenuROListBox)
		return;

	GadgetListBoxReset(
		s_changeLogMenuROListBox);

	GadgetListBoxAddEntryText(
		s_changeLogMenuROListBox,
		text,
		GameMakeColor(254, 254, 254, 255),
		-1);
}

static void UpdateChangeLogMenuROVersionInfo()
{
	if (!s_changeLogMenuROVersionInfo)
		return;

	UnicodeString currentVersion =
		REBORN_OMEGA_VERSION_TEXT;

	UnicodeString latestVersion;
	latestVersion = L"-";

	const RebornOmegaVersionInfo* latest =
		nullptr;

	for (
		size_t index = 0;
		index <
		s_changeLogMenuROVersions.size();
		++index)
	{
		const RebornOmegaVersionInfo& version =
			s_changeLogMenuROVersions[index];

		if (
			!latest ||
			version.buildRank >
			latest->buildRank)
		{
			latest = &version;
		}
	}

	if (latest)
	{
		latestVersion.translate(
			latest->displayName.c_str());
	}

	UnicodeString text;

	text.format(
		L"%ls %ls\n%ls %ls",
		TheGameText->fetch(
			"GUI:CurrentVersion").str(),
		currentVersion.str(),
		TheGameText->fetch(
			"GUI:LatestAvailableVersion").str(),
		latestVersion.str());

	GadgetStaticTextSetText(
		s_changeLogMenuROVersionInfo,
		text);
}

static void UpdateChangeLogMenuRONavigation()
{
	Bool downloading =
		GetRebornOmegaChangeLogDownloadState() ==
		REBORN_CHANGELOG_DOWNLOADING;

	if (s_changeLogMenuROPreviousButton)
	{
		s_changeLogMenuROPreviousButton->winEnable(
			!downloading &&
			s_changeLogMenuROSelectedVersion > 0);
	}

	if (s_changeLogMenuRONextButton)
	{
		s_changeLogMenuRONextButton->winEnable(
			!downloading &&
			s_changeLogMenuROSelectedVersion >= 0 &&
			s_changeLogMenuROSelectedVersion <
			static_cast<Int>(
				s_changeLogMenuROVersions.size()) - 1);
	}

	UpdateChangeLogMenuRORefreshButton();
	UpdateChangeLogMenuROVisitButton();
	UpdateChangeLogMenuROInstallButton();

}

static void UpdateChangeLogMenuROLoadingText()
{
	UnicodeString loadingText =
		TheGameText->fetch(
			"GUI:RebornOmegaChangeLogLoading");

	switch (s_changeLogMenuROLoadingDots)
	{
	case 1:
		loadingText.concat(L".");
		break;

	case 2:
		loadingText.concat(L"..");
		break;

	case 3:
		loadingText.concat(L"...");
		break;
	}

	SetChangeLogMenuROStatus(
		loadingText);
}

static void ShowSelectedChangeLogMenuROVersion()
{
	if (s_changeLogMenuROSelectedVersion < 0 ||
		s_changeLogMenuROSelectedVersion >=
		static_cast<Int>(
			s_changeLogMenuROVersions.size()))
	{
		return;
	}

	RebornOmegaVersionInfo& version =
		s_changeLogMenuROVersions[
			s_changeLogMenuROSelectedVersion];

	if (s_changeLogMenuROVersionLabel)
	{
		UnicodeString versionText;
		versionText.translate(
			version.displayName.c_str());

		GadgetStaticTextSetText(
			s_changeLogMenuROVersionLabel,
			versionText);
	}

	if (!version.changeLog.empty())
	{
		PopulateChangeLogMenuRO(
			version.changeLog.c_str());

		UpdateChangeLogMenuRONavigation();
		return;
	}

	s_changeLogMenuROLoadingDots = 0;
	s_changeLogMenuROLastLoadingUpdate =
		timeGetTime();

	UpdateChangeLogMenuROLoadingText();

	s_changeLogMenuRODownloadingVersion =
		s_changeLogMenuROSelectedVersion;

	if (!StartRebornOmegaChangeLogDownload(
		version.pageUrl))
	{
		s_changeLogMenuRODownloadingVersion = -1;

		SetChangeLogMenuROStatus(
			TheGameText->fetch(
				"GUI:RebornOmegaChangeLogUnavailable"));
	}

	UpdateChangeLogMenuRONavigation();
}

static void AdjustChangeLogMenuROFontSize(Int adjustment)
{

	Int newFontSize =
		s_changeLogMenuROFontSize +
		adjustment;

	if (
		newFontSize <
		s_changeLogMenuROMinimumFontSize)
	{
		newFontSize =
			s_changeLogMenuROMinimumFontSize;
	}
	else if (
		newFontSize >
		s_changeLogMenuROMaximumFontSize)
	{
		newFontSize =
			s_changeLogMenuROMaximumFontSize;
	}

	if (
		newFontSize ==
		s_changeLogMenuROFontSize)
	{
		return;
	}

	Int topVisibleEntry = 0;

	if (s_changeLogMenuROListBox)
	{
		topVisibleEntry =
			GadgetListBoxGetTopVisibleEntry(
				s_changeLogMenuROListBox);
	}

	s_changeLogMenuROFontSize =
		newFontSize;

	ShowSelectedChangeLogMenuROVersion();

	if (s_changeLogMenuROListBox)
	{
		GadgetListBoxSetTopVisibleEntry(
			s_changeLogMenuROListBox,
			topVisibleEntry);
	}

	UpdateChangeLogMenuROFontButtons();
}

Bool IsChangeLogMenuROOptionsOverlayActive()
{
	return s_changeLogMenuROOptionsOverlay;
}

static void CloseChangeLogMenuRO()
{
	if (!s_changeLogMenuROParent)
		return;

	s_changeLogMenuROCloseRequested = FALSE;

	if (!s_changeLogMenuROOptionsOverlay)
	{
		RequestChangeLogMessageBoxRestore();
		TheShell->pop();
		return;
	}

	GameWindow* changeLogWindow =
		s_changeLogMenuROParent;

	s_changeLogMenuROOptionsOverlay = FALSE;
	s_changeLogMenuROListBox = nullptr;
	s_changeLogMenuROParent = nullptr;
	s_changeLogMenuROVersionLabel = nullptr;
	s_changeLogMenuROVersionInfo = nullptr;
	s_changeLogMenuROPreviousButton = nullptr;
	s_changeLogMenuRONextButton = nullptr;
	s_changeLogMenuROInstallButton = nullptr;
	s_changeLogMenuROVisitButton = nullptr;
	s_changeLogMenuRORefreshButton = nullptr;
	s_changeLogMenuROVersions.clear();
	s_changeLogMenuROSelectedVersion = -1;
	s_changeLogMenuRODownloadingVersion = -1;
	s_changeLogMenuROLastLoadingUpdate = 0;
	s_changeLogMenuROLoadingDots = 0;

	TheWindowManager->winDestroy(
		changeLogWindow);

	GameWindow* mainMenuWindow =
		TheWindowManager->winGetWindowFromId(
			nullptr,
			GetMainMenuWindowKey("MainMenuParent")); // Reborn: Follow the loaded Main Menu theme.

	if (mainMenuWindow)
	{
		mainMenuWindow->winHide(FALSE);
		mainMenuWindow->winEnable(TRUE);
	}

	RestoreChangeLogMessageBox();
}

void ShowChangeLogMenuRO()
{
	if (!IsChangeLogMessageBoxFromOptions())
	{
		TheShell->push(
			"Menus/ChangeLogMenuRO.wnd");

		return;
	}

	GameWindow* mainMenuWindow =
		TheWindowManager->winGetWindowFromId(
			nullptr,
			GetMainMenuWindowKey("MainMenuParent")); // Reborn: Follow the loaded Main Menu theme.

	if (mainMenuWindow)
	{
		mainMenuWindow->winHide(TRUE);
		mainMenuWindow->winEnable(FALSE);
	}

	s_changeLogMenuROOptionsOverlay = TRUE;

	GameWindow* changeLogWindow =
		TheWindowManager->winCreateFromScript(
			GetThemedWindowName("Menus/ChangeLogMenuRO.wnd")); // Reborn: Theme the Options-owned changelog overlay while retaining canonical child identities.

	if (!changeLogWindow)
	{
		s_changeLogMenuROOptionsOverlay = FALSE;

		if (mainMenuWindow)
		{
			mainMenuWindow->winHide(FALSE);
			mainMenuWindow->winEnable(TRUE);
		}

		RestoreChangeLogMessageBox();
		return;
	}

	changeLogWindow->winHide(FALSE);
	changeLogWindow->winEnable(TRUE);

	ChangeLogMenuROInit(
		nullptr,
		nullptr);

	changeLogWindow->winBringToTop();

	TheWindowManager->winSetModal(
		changeLogWindow);
}

void ChangeLogMenuROInit(
	WindowLayout* layout,
	void* userData)
{
	s_changeLogMenuROCloseRequested = FALSE;
	s_changeLogMenuRORestoreModalRequested = FALSE;
	s_changeLogMenuROInstallConfirmationMessageBox = nullptr;
	s_changeLogMenuROBringConfirmationToTopRequested = FALSE;
	s_changeLogMenuROPendingInstallUrl.clear();

	s_changeLogMenuROParent =
		TheWindowManager->winGetWindowFromId(
			nullptr,
			NAMEKEY("ChangeLogMenuRO.wnd:Parent"));

	s_changeLogMenuROListBox =
		TheWindowManager->winGetWindowFromId(
			s_changeLogMenuROParent,
			NAMEKEY("ChangeLogMenuRO.wnd:ListboxChangeLog"));

	s_changeLogMenuROVersionLabel =
		TheWindowManager->winGetWindowFromId(
			s_changeLogMenuROParent,
			NAMEKEY(
				"ChangeLogMenuRO.wnd:StaticTextVersion"));

	s_changeLogMenuROPreviousButton =
		TheWindowManager->winGetWindowFromId(
			s_changeLogMenuROParent,
			NAMEKEY(
				"ChangeLogMenuRO.wnd:ButtonPreviousVersion"));

	s_changeLogMenuRONextButton =
		TheWindowManager->winGetWindowFromId(
			s_changeLogMenuROParent,
			NAMEKEY(
				"ChangeLogMenuRO.wnd:ButtonNextVersion"));

	s_changeLogMenuRODecreaseFontSizeButton =
		TheWindowManager->winGetWindowFromId(
			s_changeLogMenuROParent,
			NAMEKEY(
				"ChangeLogMenuRO.wnd:"
				"ButtonDecreaseFontSize"));

	s_changeLogMenuROIncreaseFontSizeButton =
		TheWindowManager->winGetWindowFromId(
			s_changeLogMenuROParent,
			NAMEKEY(
				"ChangeLogMenuRO.wnd:"
				"ButtonIncreaseFontSize"));

	s_changeLogMenuROInstallButton =
		TheWindowManager->winGetWindowFromId(
			s_changeLogMenuROParent,
			NAMEKEY(
				"ChangeLogMenuRO.wnd:"
				"ButtonInstall"));

	if (s_changeLogMenuROInstallButton)
	{
		s_changeLogMenuROInstallButton->
			winEnable(FALSE);
	}

	s_changeLogMenuROVisitButton =
		TheWindowManager->winGetWindowFromId(
			s_changeLogMenuROParent,
			NAMEKEY(
				"ChangeLogMenuRO.wnd:"
				"ButtonVisit"));

	if (s_changeLogMenuROVisitButton)
	{
		s_changeLogMenuROVisitButton->
			winEnable(FALSE);
	}

	s_changeLogMenuROInstallConfirmation =
		TheWindowManager->winGetWindowFromId(
			s_changeLogMenuROParent,
			NAMEKEY(
				"ChangeLogMenuRO.wnd:"
				"InstallConfirmation"));

	s_changeLogMenuROVersionInfo =
		TheWindowManager->winGetWindowFromId(
			s_changeLogMenuROParent,
			NAMEKEY(
				"ChangeLogMenuRO.wnd:"
				"StaticTextVersionInfo"));

	if (s_changeLogMenuROVersionInfo)
	{
		s_changeLogMenuROVersionInfo->
			winSetFont(
				TheFontLibrary->getFont(
					"Arial",
					12,
					FALSE));
	}

	s_changeLogMenuRORefreshButton =
		TheWindowManager->winGetWindowFromId(
			s_changeLogMenuROParent,
			NAMEKEY(
				"ChangeLogMenuRO.wnd:"
				"ButtonRefresh"));

	if (s_changeLogMenuROInstallConfirmation)
	{
		s_changeLogMenuROInstallConfirmation->
			winHide(TRUE);

		s_changeLogMenuROInstallConfirmation->
			winEnable(FALSE);
	}

	if (s_changeLogMenuROListBox)
	{
		GadgetListBoxSetPreserveItemFonts(
			s_changeLogMenuROListBox,
			TRUE);
	}

	UpdateChangeLogMenuROFontButtons();

	s_changeLogMenuROVersions.clear();
	s_changeLogMenuROSelectedVersion = -1;
	s_changeLogMenuRODownloadingVersion = -1;

	RebornOmegaChangeLogDownloadState downloadState =
		GetRebornOmegaChangeLogDownloadState();

	if (downloadState ==
		REBORN_CHANGELOG_COMPLETED ||
		downloadState ==
		REBORN_CHANGELOG_FAILED)
	{
		FinishRebornOmegaChangeLogDownload();
	}

	if (!GetRebornOmegaVersionList(
		s_changeLogMenuROVersions))
	{
		if (s_changeLogMenuROVersionLabel)
		{
			GadgetStaticTextSetText(
				s_changeLogMenuROVersionLabel,
				TheGameText->fetch(
					"GUI:RebornOmegaChangeLogUnavailable"));
		}

		SetChangeLogMenuROStatus(
			TheGameText->fetch(
				"GUI:RebornOmegaChangeLogUnavailable"));

		UpdateChangeLogMenuRONavigation();
		return;
	}

	UpdateChangeLogMenuROVersionInfo();

	s_changeLogMenuROSelectedVersion =
		static_cast<Int>(
			s_changeLogMenuROVersions.size()) - 1;

	ShowSelectedChangeLogMenuROVersion();
	
}

void ChangeLogMenuROUpdate(
	WindowLayout* layout,
	void* userData)
{

	if (s_changeLogMenuRODownloadRequested)
	{
		s_changeLogMenuRODownloadRequested =
			FALSE;

		if (
			s_changeLogMenuROBringConfirmationToTopRequested &&
			s_changeLogMenuROInstallConfirmationMessageBox)
		{
			s_changeLogMenuROBringConfirmationToTopRequested =
				FALSE;

			TheWindowManager->winSetModal(
				s_changeLogMenuROInstallConfirmationMessageBox);

			TheWindowManager->winSetFocus(
				nullptr);

			s_changeLogMenuROInstallConfirmationMessageBox->
				winBringToTop();
		}

		if (s_changeLogMenuRORestoreModalRequested)
		{
			s_changeLogMenuRORestoreModalRequested =
				FALSE;

			if (s_changeLogMenuROParent)
			{
				s_changeLogMenuROParent->
					winEnable(TRUE);

				TheWindowManager->winSetModal(
					s_changeLogMenuROParent);

				TheWindowManager->winSetFocus(
					nullptr);

				s_changeLogMenuROParent->
					winBringToTop();
			}
		}

		GameWindow* mainMenuWindow =
			TheWindowManager->winGetWindowFromId(
				nullptr,
				GetMainMenuWindowKey("MainMenuParent")); // Reborn: Follow the loaded Main Menu theme.

		if (mainMenuWindow)
		{
			mainMenuWindow->winHide(TRUE);
			mainMenuWindow->winEnable(FALSE);
		}

		if (s_changeLogMenuROOptionsOverlay)
		{
			GameWindow* changeLogWindow =
				s_changeLogMenuROParent;

			s_changeLogMenuROOptionsOverlay =
				FALSE;

			s_changeLogMenuROParent = nullptr;
			s_changeLogMenuROListBox = nullptr;
			s_changeLogMenuROVersionLabel = nullptr;
			s_changeLogMenuROVersionInfo = nullptr;
			s_changeLogMenuROPreviousButton = nullptr;
			s_changeLogMenuRONextButton = nullptr;
			s_changeLogMenuROInstallButton = nullptr;
			s_changeLogMenuRODecreaseFontSizeButton = nullptr;
			s_changeLogMenuROIncreaseFontSizeButton = nullptr;
			s_changeLogMenuROVisitButton = nullptr;
			s_changeLogMenuRORefreshButton = nullptr;

			s_changeLogMenuROVersions.clear();
			s_changeLogMenuROLinkUrls.clear();
			s_changeLogMenuROSelectedVersion = -1;
			s_changeLogMenuRODownloadingVersion = -1;
			s_changeLogMenuROLastLoadingUpdate = 0;
			s_changeLogMenuROLoadingDots = 0;

			DiscardChangeLogMessageBox();

			TheWindowManager->winSetModal(
				nullptr);

			TheWindowManager->winSetFocus(
				nullptr);

			if (changeLogWindow)
			{
				TheWindowManager->winDestroy(
					changeLogWindow);
			}

			GameWindow* mainMenuWindow =
				TheWindowManager->winGetWindowFromId(
					nullptr,
					GetMainMenuWindowKey("MainMenuParent")); // Reborn: Follow the loaded Main Menu theme.

			if (mainMenuWindow)
			{
				mainMenuWindow->winHide(FALSE);
				mainMenuWindow->winEnable(TRUE);
			}

			return;
		}

		TheWindowManager->winSetModal(
			nullptr);

		TheWindowManager->winSetFocus(
			nullptr);

		mainMenuWindow =
			TheWindowManager->winGetWindowFromId(
				nullptr,
				GetMainMenuWindowKey("MainMenuParent")); // Reborn: Follow the loaded Main Menu theme.

		if (mainMenuWindow)
		{
			mainMenuWindow->winHide(TRUE);
			mainMenuWindow->winEnable(FALSE);
		}

		TheShell->popImmediate(
			TRUE);

		TheShell->push(
			"Menus/DownloadMenuRO.wnd");

		return;
  }

	if (s_changeLogMenuROCloseRequested)
	{
		CloseChangeLogMenuRO();
		return;
	}

	RebornOmegaChangeLogDownloadState state =
		GetRebornOmegaChangeLogDownloadState();

	if (state == REBORN_CHANGELOG_DOWNLOADING)
	{
		DWORD currentTime =
			timeGetTime();

		if (currentTime -
			s_changeLogMenuROLastLoadingUpdate >= 350)
		{
			s_changeLogMenuROLastLoadingUpdate =
				currentTime;

			s_changeLogMenuROLoadingDots =
				(s_changeLogMenuROLoadingDots + 1) % 4;

			UpdateChangeLogMenuROLoadingText();
		}

		return;
	}

	if (state == REBORN_CHANGELOG_COMPLETED)
	{
		std::string changeLog;

		if (GetRebornOmegaChangeLogDownloadResult(
			changeLog) &&
			s_changeLogMenuRODownloadingVersion >= 0 &&
			s_changeLogMenuRODownloadingVersion <
			static_cast<Int>(
				s_changeLogMenuROVersions.size()))
		{
			RebornOmegaVersionInfo& version =
				s_changeLogMenuROVersions[
					s_changeLogMenuRODownloadingVersion];

			version.changeLog =
				changeLog;

			CacheRebornOmegaVersionChangeLog(
				version.buildRank,
				changeLog);

			if (s_changeLogMenuROSelectedVersion ==
				s_changeLogMenuRODownloadingVersion)
			{
				PopulateChangeLogMenuRO(
					version.changeLog.c_str());
			}
		}

		s_changeLogMenuRODownloadingVersion = -1;

		FinishRebornOmegaChangeLogDownload();
		UpdateChangeLogMenuRONavigation();
		return;
	}

	if (state == REBORN_CHANGELOG_FAILED)
	{
		s_changeLogMenuRODownloadingVersion = -1;

		FinishRebornOmegaChangeLogDownload();

		SetChangeLogMenuROStatus(
			TheGameText->fetch(
				"GUI:RebornOmegaChangeLogUnavailable"));

		UpdateChangeLogMenuRONavigation();
	}
}

void ChangeLogMenuROShutdown(
	WindowLayout* layout,
	void* userData)
{
	if (layout)
		layout->hide(TRUE);

	s_changeLogMenuROParent = nullptr;
	s_changeLogMenuROListBox = nullptr;
	s_changeLogMenuROVersionLabel = nullptr;
	s_changeLogMenuROPreviousButton = nullptr;
	s_changeLogMenuRONextButton = nullptr;
	s_changeLogMenuRODecreaseFontSizeButton =	nullptr;
	s_changeLogMenuROIncreaseFontSizeButton =	nullptr;
	s_changeLogMenuROInstallButton = nullptr;
	s_changeLogMenuROInstallConfirmationMessageBox = nullptr;
	s_changeLogMenuROInstallConfirmation = nullptr;
	s_changeLogMenuROVisitButton = nullptr;
	s_changeLogMenuRORefreshButton = nullptr;
	s_changeLogMenuROVersionInfo = nullptr;
	s_changeLogMenuROPendingInstallUrl.clear();
	s_changeLogMenuROVersions.clear();
	s_changeLogMenuROSelectedVersion = -1;
	s_changeLogMenuRODownloadingVersion = -1;
	s_changeLogMenuROLastLoadingUpdate = 0;
	s_changeLogMenuROLoadingDots = 0;
	s_changeLogMenuROCloseRequested = FALSE;
	s_changeLogMenuRODownloadRequested = FALSE;
	s_changeLogMenuRORestoreModalRequested = FALSE;
	s_changeLogMenuROBringConfirmationToTopRequested = FALSE;
	s_changeLogMenuROLinkUrls.clear();
	s_changeLogMenuROPendingInstallUrl.clear();

	TheShell->shutdownComplete(layout);
}

static void ConfirmChangeLogMenuROInstall()
{

	s_changeLogMenuRORestoreModalRequested = FALSE;
	s_changeLogMenuROBringConfirmationToTopRequested = FALSE;
	s_changeLogMenuROInstallConfirmationMessageBox = nullptr;

	if (s_changeLogMenuROPendingInstallUrl.empty())
		return;

	std::string downloadStartUrl =
		s_changeLogMenuROPendingInstallUrl;

	s_changeLogMenuROPendingInstallUrl.clear();

	if (RebornOmegaUpdateAcceptedFromChangeLog(
		downloadStartUrl))
	{
		s_changeLogMenuRODownloadRequested =
			TRUE;
	}
}

static void DeclineChangeLogMenuROInstall()
{
	s_changeLogMenuROPendingInstallUrl.clear();

	s_changeLogMenuRORestoreModalRequested =
		TRUE;
}

WindowMsgHandledType ChangeLogMenuROSystem(
	GameWindow* window,
	UnsignedInt msg,
	WindowMsgData mData1,
	WindowMsgData mData2)
{

	if (
		reinterpret_cast<GameWindow*>(mData1) ==
		s_changeLogMenuROInstallButton)
	{
		DEBUG_LOG((
			"ChangeLog INSTALL message: msg=%u\n",
			msg));
	}

	if (msg == GLM_DOUBLE_CLICKED)
	{
		GameWindow* listbox =
			reinterpret_cast<GameWindow*>(mData1);

		if (!listbox)
			return MSG_HANDLED;

		if (listbox->winGetWindowId() !=
			NAMEKEY(
				"ChangeLogMenuRO.wnd:ListboxChangeLog"))
		{
			return MSG_HANDLED;
		}

		Int selectedIndex =
			static_cast<Int>(mData2);

		if (selectedIndex < 0)
		{
			GadgetListBoxGetSelected(
				listbox,
				&selectedIndex);
		}

		if (selectedIndex < 0 ||
			selectedIndex >=
			static_cast<Int>(
				s_changeLogMenuROLinkUrls.size()))
		{
			return MSG_HANDLED;
		}

		const std::string& url =
			s_changeLogMenuROLinkUrls[
				selectedIndex];

		if (
			url.compare(0, 8, "https://") != 0 &&
			url.compare(0, 7, "http://") != 0)
		{
			return MSG_HANDLED;
		}

		ShellExecuteA(
			nullptr,
			"open",
			url.c_str(),
			nullptr,
			nullptr,
			SW_SHOWNORMAL);

		return MSG_HANDLED;
	}

	if (msg != GBM_SELECTED)
		return MSG_IGNORED;

	GameWindow* control =
		reinterpret_cast<GameWindow*>(mData1);

	if (!control)
		return MSG_IGNORED;

	Int controlID = control->winGetWindowId();

	if (
		controlID ==
		NAMEKEY(
			"ChangeLogMenuRO.wnd:"
			"ButtonDecreaseFontSize"))
	{
		AdjustChangeLogMenuROFontSize(-1);
		return MSG_HANDLED;
	}

	DEBUG_LOG((
		"ChangeLog GBM_SELECTED: controlID=%d installID=%d\n",
		controlID,
		NAMEKEY(
			"ChangeLogMenuRO.wnd:"
			"ButtonInstall")));

	if (
		controlID ==
		NAMEKEY(
			"ChangeLogMenuRO.wnd:"
			"ButtonDecreaseFontSize"))
	{
		AdjustChangeLogMenuROFontSize(-1);
		return MSG_HANDLED;
	}

	if (
		controlID ==
		NAMEKEY(
			"ChangeLogMenuRO.wnd:"
			"ButtonIncreaseFontSize"))
	{
		AdjustChangeLogMenuROFontSize(1);
		return MSG_HANDLED;
	}

	if (
		controlID ==
		NAMEKEY(
			"ChangeLogMenuRO.wnd:"
			"ButtonVisit"))
	{
		if (
			s_changeLogMenuROSelectedVersion >= 0 &&
			s_changeLogMenuROSelectedVersion <
			static_cast<Int>(
				s_changeLogMenuROVersions.size()))
		{
			const RebornOmegaVersionInfo& version =
				s_changeLogMenuROVersions[
					s_changeLogMenuROSelectedVersion];

			const std::string& url =
				version.pageUrl;

			if (
				url.compare(0, 8, "https://") == 0 ||
				url.compare(0, 7, "http://") == 0)
			{
				ShellExecuteA(
					nullptr,
					"open",
					url.c_str(),
					nullptr,
					nullptr,
					SW_SHOWNORMAL);
			}
		}

		return MSG_HANDLED;
	}

	if (controlID ==
		NAMEKEY(
			"ChangeLogMenuRO.wnd:ButtonPreviousVersion"))
	{
		if (s_changeLogMenuROSelectedVersion > 0 &&
			GetRebornOmegaChangeLogDownloadState() !=
			REBORN_CHANGELOG_DOWNLOADING)
		{
			--s_changeLogMenuROSelectedVersion;
			ShowSelectedChangeLogMenuROVersion();
		}

		return MSG_HANDLED;
	}

	if (controlID ==
		NAMEKEY(
			"ChangeLogMenuRO.wnd:ButtonNextVersion"))
	{
		if (s_changeLogMenuROSelectedVersion >= 0 &&
			s_changeLogMenuROSelectedVersion <
			static_cast<Int>(
				s_changeLogMenuROVersions.size()) - 1 &&
			GetRebornOmegaChangeLogDownloadState() !=
			REBORN_CHANGELOG_DOWNLOADING)
		{
			++s_changeLogMenuROSelectedVersion;
			ShowSelectedChangeLogMenuROVersion();
		}

		return MSG_HANDLED;
	}

	if (
		controlID ==
		NAMEKEY(
			"ChangeLogMenuRO.wnd:"
			"ButtonInstall"))
	{
		if (
			s_changeLogMenuROSelectedVersion >= 0 &&
			s_changeLogMenuROSelectedVersion <
			static_cast<Int>(
				s_changeLogMenuROVersions.size()))
		{
			const RebornOmegaVersionInfo& version =
				s_changeLogMenuROVersions[
					s_changeLogMenuROSelectedVersion];

			if (version.buildRank > REBORN_OMEGA_BUILD_RANK)
			//if (version.buildRank > 10399)
			{
				s_changeLogMenuROPendingInstallUrl =
					version.downloadStartUrl;

				SetRebornOmegaDownloadDisplayName(
					version.displayName);

				if (s_changeLogMenuROInstallConfirmation)
				{
					s_changeLogMenuROInstallConfirmation->
						winHide(FALSE);

					s_changeLogMenuROInstallConfirmation->
						winEnable(TRUE);

					s_changeLogMenuROInstallConfirmation->
						winBringToTop();

					TheWindowManager->winSetModal(
						s_changeLogMenuROInstallConfirmation);
				}
			}
		}

		return MSG_HANDLED;
	}

	if (
		controlID ==
		NAMEKEY(
			"ChangeLogMenuRO.wnd:"
			"ButtonConfirmInstallYes"))
	{
		if (s_changeLogMenuROInstallConfirmation)
		{
			s_changeLogMenuROInstallConfirmation->
				winHide(TRUE);

			s_changeLogMenuROInstallConfirmation->
				winEnable(FALSE);
		}

		TheWindowManager->winSetModal(
			nullptr);

		TheWindowManager->winSetFocus(
			nullptr);

		if (
			!s_changeLogMenuROPendingInstallUrl.empty() &&
			RebornOmegaUpdateAcceptedFromChangeLog(
				s_changeLogMenuROPendingInstallUrl))
		{
			s_changeLogMenuRODownloadRequested =
				TRUE;
		}

		s_changeLogMenuROPendingInstallUrl.clear();

		return MSG_HANDLED;
	}

	if (
		controlID ==
		NAMEKEY(
			"ChangeLogMenuRO.wnd:"
			"ButtonConfirmInstallNo"))
	{
		s_changeLogMenuROPendingInstallUrl.clear();

		if (s_changeLogMenuROInstallConfirmation)
		{
			s_changeLogMenuROInstallConfirmation->
				winHide(TRUE);

			s_changeLogMenuROInstallConfirmation->
				winEnable(FALSE);
		}

		TheWindowManager->winSetModal(
			s_changeLogMenuROParent);

		s_changeLogMenuROParent->
			winBringToTop();

		return MSG_HANDLED;
	}

	if (controlID ==
		NAMEKEY("ChangeLogMenuRO.wnd:ButtonClose"))
	{
		s_changeLogMenuROCloseRequested = TRUE;
		return MSG_HANDLED;
	}

	if (
		controlID ==
		NAMEKEY(
			"ChangeLogMenuRO.wnd:"
			"ButtonRefresh"))
	{
		if (
			s_changeLogMenuROSelectedVersion < 0 ||
			s_changeLogMenuROSelectedVersion >=
			static_cast<Int>(
				s_changeLogMenuROVersions.size()))
		{
			UpdateChangeLogMenuRONavigation();
			return MSG_HANDLED;
		}

		RebornOmegaChangeLogDownloadState state =
			GetRebornOmegaChangeLogDownloadState();

		if (state == REBORN_CHANGELOG_DOWNLOADING)
			return MSG_HANDLED;

		if (
			state == REBORN_CHANGELOG_COMPLETED ||
			state == REBORN_CHANGELOG_FAILED)
		{
			FinishRebornOmegaChangeLogDownload();
		}

		ShowSelectedChangeLogMenuROVersion();
		return MSG_HANDLED;
	}

	return MSG_IGNORED;
}

WindowMsgHandledType ChangeLogMenuROInput(
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
		s_changeLogMenuROCloseRequested = TRUE;
		return MSG_HANDLED;
	}

	return MSG_IGNORED;
}
