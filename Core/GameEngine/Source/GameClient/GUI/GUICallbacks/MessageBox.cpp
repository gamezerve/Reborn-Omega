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

// FILE: MessageBox.cpp /////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
//
//                       Westwood Studios Pacific.
//
//                       Confidential Information
//                Copyright (C) 2001 - All Rights Reserved
//
//-----------------------------------------------------------------------------
//
// Project:   RTS3
//
// File name: MessageBox.cpp
//
// Created:   Chris Huybregts, June 2001
//
// Desc:      the Message Box control
//
//-----------------------------------------------------------------------------
///////////////////////////////////////////////////////////////////////////////

// SYSTEM INCLUDES ////////////////////////////////////////////////////////////

// USER INCLUDES //////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/GameEngine.h"
#include "Common/NameKeyGenerator.h"
#include "Common/PlayerTemplate.h" // Reborn
#include "GameClient/CampaignManager.h"
#include "GameClient/Display.h"
#include "GameClient/Gadget.h"
#include "GameClient/GadgetPushButton.h"
#include "GameClient/GameFont.h"
#include "GameClient/GameText.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/KeyDefs.h"
#include "GameClient/MessageBox.h"
#include "GameClient/Shell.h"
#include "GameClient/WindowLayout.h"

static Bool s_popupMessageUsesRebornLayout = FALSE;
static GameWindow* s_changeLogMessageBox = nullptr;
static Bool s_changeLogMessageBoxFromOptions = FALSE;
static Bool s_changeLogMessageBoxRestorePending = FALSE;

void SetPopupMessageUsesRebornLayout(Bool useReborn)
{
	s_popupMessageUsesRebornLayout = useReborn;
}

Bool GetPopupMessageUsesRebornLayout()
{
	return s_popupMessageUsesRebornLayout;
}

GameWindow *MessageBoxYesNo(UnicodeString titleString,UnicodeString bodyString,GameWinMsgBoxFunc yesCallback,GameWinMsgBoxFunc noCallback)  ///< convenience function for displaying a Message box with Yes and No buttons
{
	return TheWindowManager->gogoMessageBox(-1,-1,-1,-1,MSG_BOX_NO | MSG_BOX_YES , titleString, bodyString, yesCallback, noCallback, nullptr, nullptr);
}
GameWindow *QuitMessageBoxYesNo(UnicodeString titleString,UnicodeString bodyString,GameWinMsgBoxFunc yesCallback,GameWinMsgBoxFunc noCallback)  ///< convenience function for displaying a Message box with Yes and No buttons
{
	return TheWindowManager->gogoMessageBox(-1,-1,-1,-1,MSG_BOX_NO | MSG_BOX_YES , titleString, bodyString, yesCallback, noCallback, nullptr, nullptr, TRUE);
}

GameWindow* MessageBoxNoButtons(UnicodeString titleString, UnicodeString bodyString, bool bShowLogo)
{
	// Reborn: Zero means no message-box buttons in the existing window manager API.
	return TheWindowManager->gogoMessageBox(-1, -1, -1, -1, 0, titleString, bodyString, nullptr, nullptr, nullptr, nullptr, bShowLogo);
}


GameWindow *MessageBoxYesNoCancel(UnicodeString titleString,UnicodeString bodyString, GameWinMsgBoxFunc yesCallback, GameWinMsgBoxFunc noCallback, GameWinMsgBoxFunc cancelCallback)///< convenience function for displaying a Message box with Yes,No and Cancel buttons
{
	return TheWindowManager->gogoMessageBox(-1,-1,-1,-1,MSG_BOX_NO | MSG_BOX_YES | MSG_BOX_CANCEL , titleString, bodyString, yesCallback, noCallback, nullptr, cancelCallback);
}


GameWindow *MessageBoxOkCancel(UnicodeString titleString,UnicodeString bodyString,GameWinMsgBoxFunc okCallback,GameWinMsgBoxFunc cancelCallback)///< convenience function for displaying a Message box with Ok and Cancel buttons
{
	return TheWindowManager->gogoMessageBox(-1,-1,-1,-1,MSG_BOX_OK | MSG_BOX_CANCEL , titleString, bodyString, nullptr, nullptr, okCallback, cancelCallback);
}

GameWindow *MessageBoxOk(UnicodeString titleString,UnicodeString bodyString,GameWinMsgBoxFunc okCallback)///< convenience function for displaying a Message box with Ok button
{
	return TheWindowManager->gogoMessageBox(-1,-1,-1,-1,MSG_BOX_OK, titleString, bodyString, nullptr, nullptr, okCallback, nullptr);
}


GameWindow *MessageBoxCancel(UnicodeString titleString,UnicodeString bodyString,GameWinMsgBoxFunc cancelCallback)///< convenience function for displaying a Message box with Cancel button
{
	return TheWindowManager->gogoMessageBox(-1,-1,-1,-1, MSG_BOX_CANCEL, titleString, bodyString, nullptr, nullptr, nullptr, cancelCallback);
}

GameWindow* MessageBoxYesNoChangeLog(UnicodeString titleString,	UnicodeString bodyString,	GameWinMsgBoxFunc yesCallback, GameWinMsgBoxFunc noCallback, GameWinMsgBoxFunc changeLogCallback)
{
	GameWindow* messageBox =
		TheWindowManager->gogoMessageBox(
			-1,
			-1,
			-1,
			-1,
			MSG_BOX_NO | MSG_BOX_YES | MSG_BOX_CANCEL,
			titleString,
			bodyString,
			yesCallback,
			noCallback,
			nullptr,
			changeLogCallback);

	if (!messageBox)
		return nullptr;

	s_changeLogMessageBox = messageBox;

	const char* buttonName =
		s_popupMessageUsesRebornLayout
		? "MessageBoxGen.wnd:ButtonCancel"
		: "MessageBox.wnd:ButtonCancel";

	GameWindow* changeLogButton =
		TheWindowManager->winGetWindowFromId(
			messageBox,
			TheNameKeyGenerator->nameToKey(buttonName));

	const char* yesButtonName =
		s_popupMessageUsesRebornLayout
		? "MessageBoxGen.wnd:ButtonYes"
		: "MessageBox.wnd:ButtonYes";

	const char* noButtonName =
		s_popupMessageUsesRebornLayout
		? "MessageBoxGen.wnd:ButtonNo"
		: "MessageBox.wnd:ButtonNo";

	GameWindow* yesButton =
		TheWindowManager->winGetWindowFromId(
			messageBox,
			TheNameKeyGenerator->nameToKey(
				yesButtonName));

	GameWindow* noButton =
		TheWindowManager->winGetWindowFromId(
			messageBox,
			TheNameKeyGenerator->nameToKey(
				noButtonName));

	if (yesButton &&
		noButton &&
		changeLogButton)
	{
		Int changeLogFontSize =
			TheDisplay->getHeight() * 21 /
			1080;

		if (changeLogFontSize < 11)
			changeLogFontSize = 11;

		changeLogButton->winSetFont(
			TheFontLibrary->getFont(
				"Generals",
				changeLogFontSize,
				FALSE));

		GadgetButtonSetText(
			changeLogButton,
			TheGameText->fetch("GUI:ChangeLog"));

		Int leftX = 0;
		Int buttonY = 0;
		Int rightX = 0;
		Int rightY = 0;

		yesButton->winGetPosition(
			&leftX,
			&buttonY);

		changeLogButton->winGetPosition(
			&rightX,
			&rightY);

		Int middleX =
			leftX +
			(rightX - leftX) / 2;

		changeLogButton->winSetPosition(
			middleX,
			buttonY);

		noButton->winSetPosition(
			rightX,
			buttonY);
	}

	return messageBox;
}

GameWindow* MessageBoxOkChangeLog(
	UnicodeString titleString,
	UnicodeString bodyString,
	GameWinMsgBoxFunc okCallback,
	GameWinMsgBoxFunc changeLogCallback)
{
	GameWindow* messageBox =
		TheWindowManager->gogoMessageBox(
			-1,
			-1,
			-1,
			-1,
			MSG_BOX_OK | MSG_BOX_CANCEL,
			titleString,
			bodyString,
			nullptr,
			nullptr,
			okCallback,
			changeLogCallback);

	if (!messageBox)
		return nullptr;

	s_changeLogMessageBox =
		messageBox;

	const char* buttonName =
		s_popupMessageUsesRebornLayout
		? "MessageBoxGen.wnd:ButtonCancel"
		: "MessageBox.wnd:ButtonCancel";

	GameWindow* changeLogButton =
		TheWindowManager->winGetWindowFromId(
			messageBox,
			TheNameKeyGenerator->nameToKey(
				buttonName));

	if (changeLogButton)
	{
		Int changeLogFontSize =
			TheDisplay->getHeight() * 21 /
			1080;

		if (changeLogFontSize < 11)
			changeLogFontSize = 11;

		changeLogButton->winSetFont(
			TheFontLibrary->getFont(
				"Generals",
				changeLogFontSize,
				FALSE));

		GadgetButtonSetText(
			changeLogButton,
			TheGameText->fetch(
				"GUI:ChangeLog"));
	}

	return messageBox;
}

void SetChangeLogMessageBoxFromOptions(
	Bool fromOptions)
{
	s_changeLogMessageBoxFromOptions =
		fromOptions;
}

void RequestChangeLogMessageBoxRestore()
{
	s_changeLogMessageBoxRestorePending =
		TRUE;
}

Bool IsChangeLogMessageBoxRestorePending(
	Bool fromOptions)
{
	return
		s_changeLogMessageBoxRestorePending &&
		s_changeLogMessageBoxFromOptions ==
		fromOptions;
}

void RestoreChangeLogMessageBox()
{
	if (!s_changeLogMessageBox)
		return;

	s_changeLogMessageBoxRestorePending =
		FALSE;

	if (s_changeLogMessageBoxFromOptions)
	{
		GameWindow* optionsMenuWindow =
			TheWindowManager->winGetWindowFromId(
				nullptr,
				NAMEKEY("OptionsMenu.wnd:"));

		if (optionsMenuWindow)
		{
			optionsMenuWindow->winHide(FALSE);
			optionsMenuWindow->winEnable(TRUE);
			optionsMenuWindow->winBringToTop();
		}
	}

	s_changeLogMessageBox->winHide(FALSE);
	s_changeLogMessageBox->winEnable(TRUE);

	const char* parentName =
		s_popupMessageUsesRebornLayout
		? "MessageBoxGen.wnd:MessageBoxParent"
		: "MessageBox.wnd:MessageBoxParent";

	GameWindow* messageBoxParent =
		TheWindowManager->winGetWindowFromId(
			s_changeLogMessageBox,
			TheNameKeyGenerator->nameToKey(
				parentName));

	TheWindowManager->winSetModal(
		s_changeLogMessageBox);

	TheWindowManager->winSetFocus(nullptr);

	if (messageBoxParent)
	{
		TheWindowManager->winSetFocus(
			messageBoxParent);
	}

	s_changeLogMessageBox->winBringToTop();
}

Bool IsChangeLogMessageBoxFromOptions()
{
	return s_changeLogMessageBoxFromOptions;
}

void DiscardChangeLogMessageBox()
{
	s_changeLogMessageBoxRestorePending =
		FALSE;

	if (s_changeLogMessageBox)
	{
		TheWindowManager->winDestroy(
			s_changeLogMessageBox);

		s_changeLogMessageBox =
			nullptr;
	}

	s_changeLogMessageBoxFromOptions =
		FALSE;
}

// PRIVATE DATA ///////////////////////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------------------------------------
/** Message Box window system callback */
//-------------------------------------------------------------------------------------------------
WindowMsgHandledType MessageBoxSystem( GameWindow *window, UnsignedInt msg,
										 WindowMsgData mData1, WindowMsgData mData2 )
{


	switch( msg )
	{

		//---------------------------------------------------------------------------------------------
		case GWM_DESTROY:
		{
			if (window == s_changeLogMessageBox)
			{
				s_changeLogMessageBox = nullptr;
				s_changeLogMessageBoxRestorePending = FALSE;
			}

			delete (WindowMessageBoxData*)window->winGetUserData();
			window->winSetUserData(nullptr);
			break;
		}

		// --------------------------------------------------------------------------------------------
		case GWM_INPUT_FOCUS:
		{

			// if we're givin the opportunity to take the keyboard focus we must say we want it
			if( mData1 == TRUE )
				*(Bool *)mData2 = TRUE;

			break;

		}

		//---------------------------------------------------------------------------------------------
		case GBM_SELECTED:
		{
			GameWindow* control = (GameWindow*)mData1;
			Int controlID = control->winGetWindowId();

			const Bool useGen = s_popupMessageUsesRebornLayout;

			NameKeyType buttonOkID;
			NameKeyType buttonYesID;
			NameKeyType buttonNoID;
			NameKeyType buttonCancelID;

			if (useGen)
			{
				buttonOkID = TheNameKeyGenerator->nameToKey("MessageBoxGen.wnd:ButtonOk");
				buttonYesID = TheNameKeyGenerator->nameToKey("MessageBoxGen.wnd:ButtonYes");
				buttonNoID = TheNameKeyGenerator->nameToKey("MessageBoxGen.wnd:ButtonNo");
				buttonCancelID = TheNameKeyGenerator->nameToKey("MessageBoxGen.wnd:ButtonCancel");
			}
			else
			{
				buttonOkID = TheNameKeyGenerator->nameToKey("MessageBox.wnd:ButtonOk");
				buttonYesID = TheNameKeyGenerator->nameToKey("MessageBox.wnd:ButtonYes");
				buttonNoID = TheNameKeyGenerator->nameToKey("MessageBox.wnd:ButtonNo");
				buttonCancelID = TheNameKeyGenerator->nameToKey("MessageBox.wnd:ButtonCancel");
			}

			WindowMessageBoxData* MsgBoxCallbacks = (WindowMessageBoxData*)window->winGetUserData();

			if( controlID == buttonOkID )
			{
				//simple enough,if we have a callback, call it, if not, then just destroy the window
				if (MsgBoxCallbacks->okCallback)
					MsgBoxCallbacks->okCallback();

				TheWindowManager->winDestroy(window);

			}
			else if( controlID == buttonYesID )
			{
				if (MsgBoxCallbacks->yesCallback)
					MsgBoxCallbacks->yesCallback();
				TheWindowManager->winDestroy(window);
			}
			else if( controlID == buttonNoID )
			{
				if (MsgBoxCallbacks->noCallback)
					MsgBoxCallbacks->noCallback();
				TheWindowManager->winDestroy(window);
			}
			else if (controlID == buttonCancelID)
			{
				if (window == s_changeLogMessageBox)
				{
					window->winHide(TRUE);
					window->winEnable(FALSE);

					if (MsgBoxCallbacks->cancelCallback)
						MsgBoxCallbacks->cancelCallback();
				}
				else
				{
					if (MsgBoxCallbacks->cancelCallback)
						MsgBoxCallbacks->cancelCallback();

					TheWindowManager->winDestroy(window);
				}
			}

			break;

		}

		//---------------------------------------------------------------------------------------------
		default:
			return MSG_IGNORED;

	}

	return MSG_HANDLED;

}
//-------------------------------------------------------------------------------------------------
/** Message Box window system callback */
//-------------------------------------------------------------------------------------------------
WindowMsgHandledType QuitMessageBoxSystem( GameWindow *window, UnsignedInt msg,
										 WindowMsgData mData1, WindowMsgData mData2 )
{


	switch( msg )
	{

		//---------------------------------------------------------------------------------------------
		case GWM_DESTROY:
		{
			delete (WindowMessageBoxData *)window->winGetUserData();
			window->winSetUserData( nullptr );
			break;

		}

		// --------------------------------------------------------------------------------------------
		case GWM_INPUT_FOCUS:
		{

			// if we're givin the opportunity to take the keyboard focus we must say we want it
			if( mData1 == TRUE )
				*(Bool *)mData2 = TRUE;

			break;

		}

		//---------------------------------------------------------------------------------------------
		case GBM_SELECTED:
		{
			GameWindow *control = (GameWindow *)mData1;
			Int controlID = control->winGetWindowId();
			const Bool useGen = s_popupMessageUsesRebornLayout;

			const NameKeyType buttonOkID =
				TheNameKeyGenerator->nameToKey(useGen ? "QuitMessageBoxGen.wnd:ButtonOk"
					: "QuitMessageBox.wnd:ButtonOk");

			const NameKeyType buttonYesID =
				TheNameKeyGenerator->nameToKey(useGen ? "QuitMessageBoxGen.wnd:ButtonYes"
					: "QuitMessageBox.wnd:ButtonYes");

			const NameKeyType buttonNoID =
				TheNameKeyGenerator->nameToKey(useGen ? "QuitMessageBoxGen.wnd:ButtonNo"
					: "QuitMessageBox.wnd:ButtonNo");

			const NameKeyType buttonCancelID =
				TheNameKeyGenerator->nameToKey(useGen ? "QuitMessageBoxGen.wnd:ButtonCancel"
					: "QuitMessageBox.wnd:ButtonCancel");
			WindowMessageBoxData *MsgBoxCallbacks = (WindowMessageBoxData *)window->winGetUserData();

			if( controlID == buttonOkID )
			{
				//simple enough,if we have a callback, call it, if not, then just destroy the window
				if (MsgBoxCallbacks->okCallback)
					MsgBoxCallbacks->okCallback();

				TheWindowManager->winDestroy(window);

			}
			else if( controlID == buttonYesID )
			{
				if (MsgBoxCallbacks->yesCallback)
					MsgBoxCallbacks->yesCallback();
				TheWindowManager->winDestroy(window);
			}
			else if( controlID == buttonNoID )
			{
				if (MsgBoxCallbacks->noCallback)
					MsgBoxCallbacks->noCallback();
				TheWindowManager->winDestroy(window);
			}
			else if( controlID == buttonCancelID )
			{
				if (MsgBoxCallbacks->cancelCallback)
					MsgBoxCallbacks->cancelCallback();
				TheWindowManager->winDestroy(window);
			}

			break;

		}

		//---------------------------------------------------------------------------------------------
		default:
			return MSG_IGNORED;

	}

	return MSG_HANDLED;

}
