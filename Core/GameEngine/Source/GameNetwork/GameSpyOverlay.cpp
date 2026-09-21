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

// FILE: wolscreens.cpp //////////////////////////////////////////////////////
// Westwood Online screen setup/teardown
// Author: Matthew D. Campbell, November 2001

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine
#include "Common/AudioEventRTS.h"

#include "GameClient/CampaignManager.h"
#include "GameClient/GadgetListBox.h"
#include "GameClient/GadgetPushButton.h"
#include "GameClient/GameText.h"
#include "GameClient/MessageBox.h"
#include "GameClient/ShellHooks.h"
//#include "GameNetwork/GameSpy.h"
//#include "GameNetwork/GameSpyGP.h"

#include "GameNetwork/GameSpyOverlay.h"
//#include "GameNetwork/GameSpy/PeerDefs.h"
#include "GameNetwork/GameSpy/BuddyThread.h"

#if defined(_MSC_VER)
#pragma comment(lib, "wininet.lib")
#endif

#if defined(GENERALS_ONLINE)
#include "GameNetwork/GeneralsOnline/NGMP_interfaces.h"
#endif

void deleteNotificationBox();
static void raiseOverlays();

// Message boxes -------------------------------------
static GameWindow *messageBoxWindow = nullptr;
static GameWinMsgBoxFunc okFunc = nullptr;
static GameWinMsgBoxFunc cancelFunc = nullptr;
static Bool reOpenPlayerInfoFlag = FALSE;
/**
	* messageBoxOK is called when a message box is destroyed
	* by way of an OK button, so we can clear our pointers to it.
	*/
static void messageBoxOK()
{
	DEBUG_ASSERTCRASH(messageBoxWindow, ("Message box window went away without being there in the first place!"));
	messageBoxWindow = nullptr;
	if (okFunc)
	{
		okFunc();
		okFunc = nullptr;
	}
}

#if defined(GENERALS_ONLINE)
void GameSpyCancelBuddyLoginInBackground();
#endif

/**
	* messageBoxCancel is called when a message box is destroyed
	* by way of a Cancel button, so we can clear our pointers to it.
	*/
static void messageBoxCancel()
{
	DEBUG_ASSERTCRASH(messageBoxWindow, ("Message box window went away without being there in the first place!"));
	messageBoxWindow = nullptr;
	if (cancelFunc)
	{
		cancelFunc();
		cancelFunc = nullptr;
	}

#if defined(GENERALS_ONLINE)
	GameSpyCancelBuddyLoginInBackground();
#endif
}

/**
	* clearGSMessageBoxes removes the current message box if
	* one is present.  This is usually done when putting up a
	* second messageBox.
	*/
void ClearGSMessageBoxes()
{
	if (messageBoxWindow)
	{
		TheWindowManager->winDestroy(messageBoxWindow);
		messageBoxWindow = nullptr;
	}

	if (okFunc)
	{
		okFunc = nullptr;
	}

	if (cancelFunc)
	{
		cancelFunc = nullptr;
	}
}

/**
	* GSMessageBoxOk puts up an OK dialog box and saves the
	* pointers to it and its callbacks.
	*/
void GSMessageBoxOk(UnicodeString title, UnicodeString message, GameWinMsgBoxFunc newOkFunc)
{
	ClearGSMessageBoxes();
	messageBoxWindow = MessageBoxOk(title, message, messageBoxOK);
	okFunc = newOkFunc;
}

/**
	* GSMessageBoxOkCancel puts up an OK/Cancel dialog box and saves the
	* pointers to it and its callbacks.
	*/
void GSMessageBoxOkCancel(UnicodeString title, UnicodeString message, GameWinMsgBoxFunc newOkFunc, GameWinMsgBoxFunc newCancelFunc)
{
	ClearGSMessageBoxes();
	messageBoxWindow = MessageBoxOkCancel(title, message, messageBoxOK, messageBoxCancel);
	okFunc = newOkFunc;
	cancelFunc = newCancelFunc;
}

void GSMessageBoxOkCancelWithLabels(
	UnicodeString title,
	UnicodeString message,
	UnicodeString okLabel,
	UnicodeString cancelLabel,
	GameWinMsgBoxFunc newOkFunc,
	GameWinMsgBoxFunc newCancelFunc)
{
	GSMessageBoxOkCancel(title, message, newOkFunc, newCancelFunc);

	if (messageBoxWindow == nullptr)
	{
		return;
	}

	GameWindow* buttonOk =
		TheWindowManager->winGetWindowFromId(
			messageBoxWindow,
			TheNameKeyGenerator->nameToKey("MessageBox.wnd:ButtonOk"));

	GameWindow* buttonCancel =
		TheWindowManager->winGetWindowFromId(
			messageBoxWindow,
			TheNameKeyGenerator->nameToKey("MessageBox.wnd:ButtonCancel"));

	if (buttonOk != nullptr)
	{
		GadgetButtonSetText(buttonOk, okLabel);
	}

	if (buttonCancel != nullptr)
	{
		GadgetButtonSetText(buttonCancel, cancelLabel);
	}
}

/**
	* GSMessageBoxYesNo puts up a Yes/No dialog box and saves the
	* pointers to it and its callbacks.
	*/
void GSMessageBoxYesNo(UnicodeString title, UnicodeString message, GameWinMsgBoxFunc newYesFunc, GameWinMsgBoxFunc newNoFunc)
{
	ClearGSMessageBoxes();
	messageBoxWindow = MessageBoxYesNo(title, message, messageBoxOK, messageBoxCancel);
	okFunc = newYesFunc;
	cancelFunc = newNoFunc;
}

/**
	* If the screen transitions underneath the dialog box, we
	* need to raise it to keep it visible.
	*/
void RaiseGSMessageBox()
{
	raiseOverlays();

	if (!messageBoxWindow)
		return;

	messageBoxWindow->winBringToTop();
}

void GSMessageBoxCancel(UnicodeString title, UnicodeString message, GameWinMsgBoxFunc cancelFunc)
{
	ClearGSMessageBoxes();
	messageBoxWindow = MessageBoxCancel(title, message, cancelFunc);
}

void GSMessageBoxNoButtons(UnicodeString title, UnicodeString message, bool bShowLogo)
{
	ClearGSMessageBoxes();
	messageBoxWindow = MessageBoxNoButtons(title, message, bShowLogo);
}

// Overlay screens -------------------------------------

/**
	* gsOverlays holds a list of the .wnd files used in GS overlays.
	* The entries *MUST* be in the same order as the GSOverlayType enum.
	*/
static const char * gsOverlays[GSOVERLAY_MAX] =
{
	"Menus/PopupPlayerInfo.wnd",	// Player info (right-click)
	"Menus/WOLMapSelectMenu.wnd",	// Map select
	"Menus/WOLBuddyOverlay.wnd",	// Buddy list
	"Menus/WOLPageOverlay.wnd",		// Find/page
	"Menus/PopupHostGame.wnd",		// Hosting options (game name, password, etc)
	"Menus/PopupJoinGame.wnd",		// Joining options (password, etc)
	"Menus/PopupLadderSelect.wnd",// LadderSelect
	"Menus/PopupLocaleSelect.wnd",// Prompt for user's locale
	"Menus/OptionsMenu.wnd",			// popup options
};

static WindowLayout *overlayLayouts[GSOVERLAY_MAX] =
{
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
};

static Bool buddyOverlayUsesGeneralsTheme = FALSE;

#if defined(GENERALS_ONLINE)
static Bool buddyLoginInProgress = FALSE;
static Bool buddyLoginCallbackRegistered = FALSE;
static UnsignedInt buddyLoginStartTime = 0;
static const UnsignedInt BUDDY_LOGIN_TIMEOUT = 120000;

static void deregisterBuddyLoginCallback()
{
	if (!buddyLoginCallbackRegistered)
		return;

	NGMP_OnlineServices_AuthInterface* authInterface =
		NGMP_OnlineServicesManager::GetInterface<NGMP_OnlineServices_AuthInterface>();

	if (authInterface != nullptr)
	{
		authInterface->DeregisterForLoginCallback();
	}

	buddyLoginCallbackRegistered = FALSE;
}

static void resetBuddyLoginAttempt()
{
	NGMP_OnlineServices_AuthInterface* authInterface =
		NGMP_OnlineServicesManager::GetInterface<NGMP_OnlineServices_AuthInterface>();

	if (authInterface != nullptr)
	{
		authInterface->DeregisterForLoginCallback();
	}

	buddyLoginInProgress = FALSE;
	buddyLoginStartTime = 0;
	ClearGSMessageBoxes();
}

static void stopBuddyLoginAttempt(Bool showFailure)
{
	if (!buddyLoginInProgress)
		return;

	NGMP_OnlineServices_AuthInterface* authInterface =
		NGMP_OnlineServicesManager::GetInterface<NGMP_OnlineServices_AuthInterface>();

	if (authInterface != nullptr)
	{
		authInterface->DeregisterForLoginCallback();
	}

	buddyLoginInProgress = FALSE;
	buddyLoginStartTime = 0;
	ClearGSMessageBoxes();

	if (showFailure)
	{
		GSMessageBoxOk(UnicodeString(L"Logging In"), UnicodeString(L"Login failed."), nullptr);
	}
}

static void buddyLoginComplete(ELoginResult loginResult);

void GameSpyContinueBuddyLoginInBackground()
{
	// Keep buddyLoginInProgress set: the auth interface must continue polling
	// the browser login code and will open the communicator when it succeeds.
}

void GameSpyCancelBuddyLoginInBackground()
{
	if (!buddyLoginInProgress)
		return;

	resetBuddyLoginAttempt();
}

static void buddyLoginComplete(ELoginResult loginResult)
{
	if (!buddyLoginInProgress)
		return;

	if (loginResult == ELoginResult::Success)
	{
		deregisterBuddyLoginCallback();
		ClearGSMessageBoxes();
		return;
	}

	if (loginResult == ELoginResult::Failed)
	{
		resetBuddyLoginAttempt();

		GSMessageBoxOk(
			UnicodeString(L"Logging In"),
			UnicodeString(L"Login failed."),
			nullptr);
	}
}

static Bool ensureBuddyLogin()
{
	NGMP_OnlineServicesManager::CreateInstance();
	NGMP_OnlineServicesManager *onlineServices = NGMP_OnlineServicesManager::GetInstance();
	if (onlineServices == nullptr || onlineServices->IsPendingFullTeardown())
	{
		return FALSE;
	}

	NGMP_OnlineServices_AuthInterface *authInterface =
		NGMP_OnlineServicesManager::GetInterface<NGMP_OnlineServices_AuthInterface>();
	if (authInterface == nullptr)
	{
		onlineServices->Init();
		authInterface = NGMP_OnlineServicesManager::GetInterface<NGMP_OnlineServices_AuthInterface>();
	}

	std::shared_ptr<WebSocket> webSocket = NGMP_OnlineServicesManager::GetWebSocket();
	if (authInterface != nullptr && authInterface->IsLoggedIn() &&
		webSocket != nullptr && webSocket->IsConnected())
	{
		return TRUE;
	}

	if (authInterface == nullptr || buddyLoginInProgress)
	{
		return FALSE;
	}

	buddyLoginInProgress = TRUE;
	buddyLoginStartTime = timeGetTime();
	ClearGSMessageBoxes();
	GSMessageBoxNoButtons(UnicodeString(L"Logging In"), UnicodeString(L"Please wait..."), true);
	authInterface->RegisterForLoginCallback(buddyLoginComplete);
	buddyLoginCallbackRegistered = TRUE;
	authInterface->BeginLogin(false);
	return FALSE;
}
#endif

#if !defined(GENERALS_ONLINE)
void GameSpyContinueBuddyLoginInBackground()
{
}

void GameSpyCancelBuddyLoginInBackground()
{
}
#endif

GSCommunicatorConnectionStatus GameSpyGetCommunicatorConnectionStatus()
{
#if defined(GENERALS_ONLINE)
	const UnsignedInt now = timeGetTime();

	NGMP_OnlineServicesManager* onlineServices = NGMP_OnlineServicesManager::GetInstance();
	if (onlineServices != nullptr && !onlineServices->IsPendingFullTeardown())
	{
		NGMP_OnlineServices_AuthInterface* authInterface =
			NGMP_OnlineServicesManager::GetInterface<NGMP_OnlineServices_AuthInterface>();

		std::shared_ptr<WebSocket> webSocket =
			NGMP_OnlineServicesManager::GetWebSocket();

		if (authInterface != nullptr &&
			authInterface->IsLoggedIn() &&
			webSocket != nullptr &&
			webSocket->IsConnected())
		{
			const Bool openBuddyOverlay = buddyLoginInProgress;

			if (buddyLoginInProgress)
			{
				resetBuddyLoginAttempt();
			}

			if (openBuddyOverlay && !GameSpyIsOverlayOpen(GSOVERLAY_BUDDY))
			{
				GameSpyOpenOverlay(GSOVERLAY_BUDDY);
			}

			return GSCOMMUNICATOR_CONNECTED;
		}
	}

	static UnsignedInt lastInternetCheck = 0;
	static Bool hasInternetConnection = FALSE;

	if (lastInternetCheck == 0 ||
		static_cast<UnsignedInt>(now - lastInternetCheck) >= 1000)
	{
		DWORD connectionState = 0;

		hasInternetConnection =
			InternetGetConnectedState(&connectionState, 0) &&
			!(connectionState & INTERNET_CONNECTION_MODEM_BUSY);

		lastInternetCheck = now;
	}

	if (!hasInternetConnection)
	{
		if (buddyLoginInProgress)
		{
			resetBuddyLoginAttempt();

			GSMessageBoxOk(
				TheGameText->fetch("GUI:GPErrorTitle"),
				TheGameText->fetch("GUI:GPDisconnected"),
				nullptr);
		}

		return GSCOMMUNICATOR_NO_INTERNET;
	}

	if (buddyLoginInProgress &&
		static_cast<UnsignedInt>(now - buddyLoginStartTime) >= BUDDY_LOGIN_TIMEOUT)
	{
		resetBuddyLoginAttempt();

		GSMessageBoxOk(
			UnicodeString(L"Logging In"),
			UnicodeString(L"Login timed out."),
			nullptr);

		return GSCOMMUNICATOR_DISCONNECTED;
	}

	if (buddyLoginInProgress)
	{
		return GSCOMMUNICATOR_CONNECTING;
	}
#endif

	return GSCOMMUNICATOR_DISCONNECTED;
}

static void buddyTryReconnect()
{
	BuddyRequest req;
	req.buddyRequestType = BuddyRequest::BUDDYREQUEST_RELOGIN;
	TheGameSpyBuddyMessageQueue->addRequest( req );
}

void GameSpyOpenOverlay( GSOverlayType overlay )
{
#if defined(GENERALS_ONLINE)
	if (overlay == GSOVERLAY_BUDDY && !ensureBuddyLogin())
	{
		return;
	}
#endif

	const Bool useGeneralsBuddyOverlay = overlay == GSOVERLAY_BUDDY && IsRebornCampaign();
	const char *overlayFilename = useGeneralsBuddyOverlay
		? "Menus/WOLBuddyOverlayGen.wnd"
		: gsOverlays[overlay];

	// Buddy overlays are cached between openings.  Recreate the cached layout
	// when a later game in the same process uses the other campaign UI theme.
	if (overlay == GSOVERLAY_BUDDY && overlayLayouts[overlay] &&
		buddyOverlayUsesGeneralsTheme != useGeneralsBuddyOverlay)
	{
		overlayLayouts[overlay]->runShutdown();
		overlayLayouts[overlay]->destroyWindows();
		deleteInstance(overlayLayouts[overlay]);
		overlayLayouts[overlay] = nullptr;
	}

	if (overlay == GSOVERLAY_BUDDY)
	{
#if !defined(GENERALS_ONLINE)
		if (!TheGameSpyBuddyMessageQueue->isConnected())
		{
			// not connected - is it because we were disconnected?
			if (TheGameSpyBuddyMessageQueue->getLocalProfileID())
			{
				// used to be connected
				GSMessageBoxYesNo(TheGameText->fetch("GUI:GPErrorTitle"), TheGameText->fetch("GUI:GPDisconnected"), buddyTryReconnect, nullptr);
			}
			else
			{
				// no profile
				GSMessageBoxOk(TheGameText->fetch("GUI:GPErrorTitle"), TheGameText->fetch("GUI:GPNoProfile"), nullptr);
			}
			return;
		}
#endif
		AudioEventRTS buttonClick("GUICommunicatorOpen");

		if( TheAudio )
		{
			TheAudio->addAudioEvent( &buttonClick );
		}
	}
	if (overlayLayouts[overlay])
	{
		overlayLayouts[overlay]->hide( FALSE );
		overlayLayouts[overlay]->bringForward();
	}
	else
	{
		overlayLayouts[overlay] = TheWindowManager->winCreateLayout(AsciiString(overlayFilename));
		if (!overlayLayouts[overlay])
		{
			DEBUG_LOG(("Unable to create GameSpy overlay layout '%s'", overlayFilename));
			return;
		}
		if (overlay == GSOVERLAY_BUDDY)
			buddyOverlayUsesGeneralsTheme = useGeneralsBuddyOverlay;

		overlayLayouts[overlay]->runInit();
		overlayLayouts[overlay]->hide( FALSE );
		overlayLayouts[overlay]->bringForward();
	}
}

void GameSpyCloseOverlay( GSOverlayType overlay )
{
	switch(overlay)
	{
		case GSOVERLAY_PLAYERINFO:
			DEBUG_LOG(("Closing overlay GSOVERLAY_PLAYERINFO"));
			break;
		case GSOVERLAY_MAPSELECT:
			DEBUG_LOG(("Closing overlay GSOVERLAY_MAPSELECT"));
			break;
		case GSOVERLAY_BUDDY:
			DEBUG_LOG(("Closing overlay GSOVERLAY_BUDDY"));
			break;
		case GSOVERLAY_PAGE:
			DEBUG_LOG(("Closing overlay GSOVERLAY_PAGE"));
			break;
		case GSOVERLAY_GAMEOPTIONS:
			DEBUG_LOG(("Closing overlay GSOVERLAY_GAMEOPTIONS"));
			break;
		case GSOVERLAY_GAMEPASSWORD:
			DEBUG_LOG(("Closing overlay GSOVERLAY_GAMEPASSWORD"));
			break;
		case GSOVERLAY_LADDERSELECT:
			DEBUG_LOG(("Closing overlay GSOVERLAY_LADDERSELECT"));
			break;
		case GSOVERLAY_OPTIONS:
			DEBUG_LOG(("Closing overlay GSOVERLAY_OPTIONS"));
			if( overlayLayouts[overlay] )
			{
				SignalUIInteraction(SHELL_SCRIPT_HOOK_OPTIONS_CLOSED);
			}
			break;
	}
	if( overlayLayouts[overlay] )
	{
		overlayLayouts[overlay]->runShutdown();
		overlayLayouts[overlay]->destroyWindows();
		deleteInstance(overlayLayouts[overlay]);
		overlayLayouts[overlay] = nullptr;
	}
}

Bool GameSpyIsOverlayOpen( GSOverlayType overlay )
{
	return (overlayLayouts[overlay] != nullptr);
}

void GameSpyToggleOverlay(GSOverlayType overlay)
{
#if defined(GENERALS_ONLINE)
	if (overlay == GSOVERLAY_BUDDY && buddyLoginInProgress)
	{
		GameSpyCancelBuddyLoginInBackground();
		return;
	}
#endif

	if (GameSpyIsOverlayOpen(overlay))
		GameSpyCloseOverlay(overlay);
	else
		GameSpyOpenOverlay(overlay);
}

void raiseOverlays()
{
	for (int i=0; i<GSOVERLAY_MAX; ++i)
	{
		if (overlayLayouts[(GSOverlayType)i])
		{
			overlayLayouts[(GSOverlayType)i]->bringForward();
		}
	}
}

void GameSpyCloseAllOverlays()
{
#if defined(GENERALS_ONLINE)
	if (buddyLoginInProgress)
	{
		GameSpyCancelBuddyLoginInBackground();
	}
#endif

	for (int i = 0; i < GSOVERLAY_MAX; ++i)
	{
		GameSpyCloseOverlay((GSOverlayType)i);
	}

	// if we're shutting down the rest, chances are we don't want this popping up.
	deleteNotificationBox();
}

void GameSpyUpdateOverlays()
{
	for (int i=0; i<GSOVERLAY_MAX; ++i)
	{
		if (overlayLayouts[(GSOverlayType)i])
		{
			overlayLayouts[(GSOverlayType)i]->runUpdate();
		}
	}
}

void ReOpenPlayerInfo()
{
	reOpenPlayerInfoFlag = TRUE;
}
void CheckReOpenPlayerInfo()
{
	if(!reOpenPlayerInfoFlag)
		return;

	GameSpyOpenOverlay(GSOVERLAY_PLAYERINFO);
	reOpenPlayerInfoFlag = FALSE;

}
