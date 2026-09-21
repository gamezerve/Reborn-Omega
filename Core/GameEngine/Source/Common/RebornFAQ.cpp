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

// FILE: RebornFAQ.cpp ////////////////////////////////////////////////////////////
//
// Reborn Omega FAQ support implementation
//
// Author: Gamezerve, September 2026
//
///////////////////////////////////////////////////////////////////////////////

// FILE: RebornFAQ.cpp ////////////////////////////////////////////////////////
//
// Reborn Omega FAQ support implementation
//
// Author: Gamezerve, September 2026
//
///////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"

#include "Common/RebornFAQ.h"
#include "Common/Debug.h"

#include <Windows.h>
#include <cctype>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

struct RebornFAQEntry
{
	std::string question;
	std::string answer;
};

struct RebornFAQ
{
	std::string name;
	std::string title;
	std::vector<RebornFAQEntry> entries;
};

struct RebornFAQWindowContext
{
	std::string title;
	std::string text;
	HWND edit;
	HWND closeButton;
};

static const char* RebornFAQWindowClassName = "RebornOmegaFAQWindow";


// trimFAQString ==============================================================
/** Reborn: Trim leading and trailing whitespace from an FAQ parser string. */
//=============================================================================
static std::string trimFAQString(const std::string& value)
{
	size_t first = 0;
	size_t last = value.length();

	while (first < last && isspace(static_cast<unsigned char>(value[first])))
		first++;

	while (last > first && isspace(static_cast<unsigned char>(value[last - 1])))
		last--;

	return value.substr(first, last - first);
}


// getRebornFAQPath ===========================================================
/** Reborn: Build the FAQ data path based on whether the game is running under a debugger. */
//=============================================================================
static std::string getRebornFAQPath()
{
	char exePath[MAX_PATH] = {};

	DWORD length = GetModuleFileNameA(
		nullptr,
		exePath,
		ARRAY_SIZE(exePath));

	if (length == 0 || length >= ARRAY_SIZE(exePath))
	{
		if (IsDebuggerPresent())
			return "Data\\FAQ.ini";

		return "RebornOmegaData\\FAQ.ini";
	}

	char* lastSlash = strrchr(exePath, '\\');

	if (lastSlash)
		*(lastSlash + 1) = '\0';

	// Reborn: Use the development Data directory while running through Visual Studio or another debugger.
	if (IsDebuggerPresent())
		return std::string(exePath) + "Data\\FAQ.ini";

	return std::string(exePath) + "RebornOmegaData\\FAQ.ini";
}


// parseFAQStringValue ========================================================
/** Reborn: Parse a quoted string value from an FAQ field. */
//=============================================================================
static Bool parseFAQStringValue(
	const std::string& line,
	const char* expectedField,
	std::string& value)
{
	size_t equals = line.find('=');

	if (equals == std::string::npos)
		return FALSE;

	std::string field = trimFAQString(line.substr(0, equals));

	if (field != expectedField)
		return FALSE;

	std::string source = trimFAQString(line.substr(equals + 1));

	if (source.empty() || source[0] != '"')
		return FALSE;

	value.clear();

	Bool escaped = FALSE;
	Bool closed = FALSE;
	size_t index;

	for (index = 1; index < source.length(); index++)
	{
		char ch = source[index];

		if (escaped)
		{
			switch (ch)
			{
			case 'n':
				value += '\n';
				break;

			case 'r':
				value += '\r';
				break;

			case 't':
				value += '\t';
				break;

			case '"':
				value += '"';
				break;

			case '\\':
				value += '\\';
				break;

			default:
				return FALSE;
			}

			escaped = FALSE;
			continue;
		}

		if (ch == '\\')
		{
			escaped = TRUE;
			continue;
		}

		if (ch == '"')
		{
			closed = TRUE;
			index++;
			break;
		}

		value += ch;
	}

	if (!closed || escaped)
		return FALSE;

	if (!trimFAQString(source.substr(index)).empty())
		return FALSE;

	return TRUE;
}


// loadRebornFAQ ==============================================================
/** Reborn: Load and parse a named FAQ block from the external FAQ data file. */
//=============================================================================
static Bool loadRebornFAQ(const char* faqName, RebornFAQ& faq)
{
	std::string filename = getRebornFAQPath();
	std::ifstream file(filename.c_str());

	if (!file.is_open())
	{
		REBORN_LOG(
			"FAQ_LOAD_FAILED: Failed to open FAQ file '%s'.",
			filename.c_str());

		return FALSE;
	}

	faq.name.clear();
	faq.title.clear();
	faq.entries.clear();

	std::string line;
	Int lineNumber = 0;
	Bool inFAQ = FALSE;
	Bool targetFAQ = FALSE;
	Bool inEntry = FALSE;
	RebornFAQEntry entry;

	while (std::getline(file, line))
	{
		lineNumber++;

		if (lineNumber == 1 &&
			line.length() >= 3 &&
			static_cast<unsigned char>(line[0]) == 0xEF &&
			static_cast<unsigned char>(line[1]) == 0xBB &&
			static_cast<unsigned char>(line[2]) == 0xBF)
		{
			line.erase(0, 3);
		}

		line = trimFAQString(line);

		if (line.empty())
			continue;

		if (line[0] == ';' || line[0] == '#')
			continue;

		if (!inFAQ)
		{
			if (line.compare(0, 4, "FAQ ") != 0)
			{
				REBORN_LOG(
					"FAQ_INVALID_DATA: Expected FAQ block but found '%s'. FAQFile='%s', FAQLine=%d.",
					line.c_str(),
					filename.c_str(),
					lineNumber);

				return FALSE;
			}

			std::string currentFAQName =
				trimFAQString(line.substr(4));

			if (currentFAQName.empty())
			{
				REBORN_LOG(
					"FAQ_INVALID_DATA: FAQ block is missing a name. FAQFile='%s', FAQLine=%d.",
					filename.c_str(),
					lineNumber);

				return FALSE;
			}

			inFAQ = TRUE;
			targetFAQ = currentFAQName == faqName;

			if (targetFAQ)
				faq.name = currentFAQName;

			continue;
		}

		if (line == "Entry")
		{
			if (inEntry)
			{
				if (targetFAQ)
				{
					REBORN_LOG(
						"FAQ_INVALID_DATA: Nested Entry block found in FAQ '%s'. FAQFile='%s', FAQLine=%d.",
						faqName,
						filename.c_str(),
						lineNumber);

					return FALSE;
				}

				continue;
			}

			inEntry = TRUE;

			if (targetFAQ)
				entry = RebornFAQEntry();

			continue;
		}

		if (line == "End")
		{
			if (inEntry)
			{
				if (targetFAQ)
				{
					if (entry.question.empty())
					{
						REBORN_LOG(
							"FAQ_INVALID_DATA: Entry is missing Question in FAQ '%s'. FAQFile='%s', FAQLine=%d.",
							faqName,
							filename.c_str(),
							lineNumber);

						return FALSE;
					}

					if (entry.answer.empty())
					{
						REBORN_LOG(
							"FAQ_INVALID_DATA: Entry is missing Answer in FAQ '%s'. FAQFile='%s', FAQLine=%d.",
							faqName,
							filename.c_str(),
							lineNumber);

						return FALSE;
					}

					faq.entries.push_back(entry);
				}

				inEntry = FALSE;
				continue;
			}

			if (targetFAQ)
			{
				if (faq.title.empty())
				{
					REBORN_LOG(
						"FAQ_INVALID_DATA: FAQ '%s' is missing Title. FAQFile='%s', FAQLine=%d.",
						faqName,
						filename.c_str(),
						lineNumber);

					return FALSE;
				}

				if (faq.entries.empty())
				{
					REBORN_LOG(
						"FAQ_INVALID_DATA: FAQ '%s' contains no Entry blocks. FAQFile='%s', FAQLine=%d.",
						faqName,
						filename.c_str(),
						lineNumber);

					return FALSE;
				}

				return TRUE;
			}

			inFAQ = FALSE;
			targetFAQ = FALSE;
			continue;
		}

		if (!targetFAQ)
			continue;

		if (inEntry)
		{
			if (line.compare(0, 8, "Question") == 0)
			{
				if (!entry.question.empty())
				{
					REBORN_LOG(
						"FAQ_INVALID_DATA: Duplicate Question in FAQ '%s'. FAQFile='%s', FAQLine=%d.",
						faqName,
						filename.c_str(),
						lineNumber);

					return FALSE;
				}

				if (!parseFAQStringValue(
					line,
					"Question",
					entry.question))
				{
					REBORN_LOG(
						"FAQ_INVALID_DATA: Invalid Question syntax in FAQ '%s'. FAQFile='%s', FAQLine=%d.",
						faqName,
						filename.c_str(),
						lineNumber);

					return FALSE;
				}

				continue;
			}

			if (line.compare(0, 6, "Answer") == 0)
			{
				if (!entry.answer.empty())
				{
					REBORN_LOG(
						"FAQ_INVALID_DATA: Duplicate Answer in FAQ '%s'. FAQFile='%s', FAQLine=%d.",
						faqName,
						filename.c_str(),
						lineNumber);

					return FALSE;
				}

				if (!parseFAQStringValue(
					line,
					"Answer",
					entry.answer))
				{
					REBORN_LOG(
						"FAQ_INVALID_DATA: Invalid Answer syntax in FAQ '%s'. FAQFile='%s', FAQLine=%d.",
						faqName,
						filename.c_str(),
						lineNumber);

					return FALSE;
				}

				continue;
			}

			REBORN_LOG(
				"FAQ_INVALID_DATA: Unexpected token '%s' inside Entry block of FAQ '%s'. FAQFile='%s', FAQLine=%d.",
				line.c_str(),
				faqName,
				filename.c_str(),
				lineNumber);

			return FALSE;
		}

		if (line.compare(0, 5, "Title") == 0)
		{
			if (!faq.title.empty())
			{
				REBORN_LOG(
					"FAQ_INVALID_DATA: Duplicate Title in FAQ '%s'. FAQFile='%s', FAQLine=%d.",
					faqName,
					filename.c_str(),
					lineNumber);

				return FALSE;
			}

			if (!parseFAQStringValue(
				line,
				"Title",
				faq.title))
			{
				REBORN_LOG(
					"FAQ_INVALID_DATA: Invalid Title syntax in FAQ '%s'. FAQFile='%s', FAQLine=%d.",
					faqName,
					filename.c_str(),
					lineNumber);

				return FALSE;
			}

			continue;
		}

		REBORN_LOG(
			"FAQ_INVALID_DATA: Unexpected token '%s' inside FAQ '%s'. FAQFile='%s', FAQLine=%d.",
			line.c_str(),
			faqName,
			filename.c_str(),
			lineNumber);

		return FALSE;
	}

	if (targetFAQ)
	{
		REBORN_LOG(
			"FAQ_INVALID_DATA: FAQ '%s' was not terminated with End. FAQFile='%s', FAQLine=%d.",
			faqName,
			filename.c_str(),
			lineNumber);

		return FALSE;
	}

	REBORN_LOG(
		"FAQ_NOT_FOUND: FAQ '%s' was not found in '%s'.",
		faqName,
		filename.c_str());

	return FALSE;
}


// buildRebornFAQText =========================================================
/** Reborn: Build the text displayed in the scrollable FAQ window. */
//=============================================================================
static std::string buildRebornFAQText(const RebornFAQ& faq)
{
	std::string text;

	for (size_t i = 0; i < faq.entries.size(); i++)
	{
		if (!text.empty())
			text += "\r\n\r\n";

		text += "Q: ";
		text += faq.entries[i].question;
		text += "\r\n";
		text += "A: ";
		text += faq.entries[i].answer;
	}

	return text;
}


// resizeRebornFAQWindow ======================================================
/** Reborn: Resize the FAQ controls to fit the current FAQ window client area. */
//=============================================================================
static void resizeRebornFAQWindow(HWND hwnd)
{
	RebornFAQWindowContext* context =
		reinterpret_cast<RebornFAQWindowContext*>(
			GetWindowLongPtrA(hwnd, GWLP_USERDATA));

	if (context == nullptr)
		return;

	RECT clientRect;
	GetClientRect(hwnd, &clientRect);

	Int width = clientRect.right - clientRect.left;
	Int height = clientRect.bottom - clientRect.top;

	const Int margin = 16;
	const Int buttonWidth = 100;
	const Int buttonHeight = 30;
	const Int buttonSpacing = 12;

	Int editWidth = width - margin * 2;
	Int editHeight =
		height -
		margin * 2 -
		buttonHeight -
		buttonSpacing;

	if (editWidth < 1)
		editWidth = 1;

	if (editHeight < 1)
		editHeight = 1;

	if (context->edit)
	{
		MoveWindow(
			context->edit,
			margin,
			margin,
			editWidth,
			editHeight,
			TRUE);
	}

	if (context->closeButton)
	{
		MoveWindow(
			context->closeButton,
			width - margin - buttonWidth,
			height - margin - buttonHeight,
			buttonWidth,
			buttonHeight,
			TRUE);
	}
}


// rebornFAQWindowProc ========================================================
/** Reborn: Process messages for the scrollable Reborn Omega FAQ window. */
//=============================================================================
static LRESULT CALLBACK rebornFAQWindowProc(
	HWND hwnd,
	UINT message,
	WPARAM wParam,
	LPARAM lParam)
{
	RebornFAQWindowContext* context =
		reinterpret_cast<RebornFAQWindowContext*>(
			GetWindowLongPtrA(hwnd, GWLP_USERDATA));

	switch (message)
	{
	case WM_NCCREATE:
	{
		CREATESTRUCTA* createStruct =
			reinterpret_cast<CREATESTRUCTA*>(lParam);

		context =
			reinterpret_cast<RebornFAQWindowContext*>(
				createStruct->lpCreateParams);

		SetWindowLongPtrA(
			hwnd,
			GWLP_USERDATA,
			reinterpret_cast<LONG_PTR>(context));

		return TRUE;
	}

	case WM_CREATE:
	{
		if (context == nullptr)
			return -1;

		HFONT font =
			reinterpret_cast<HFONT>(
				GetStockObject(DEFAULT_GUI_FONT));

		context->edit = CreateWindowExA(
			WS_EX_CLIENTEDGE,
			"EDIT",
			context->text.c_str(),
			WS_CHILD |
			WS_VISIBLE |
			WS_TABSTOP |
			WS_VSCROLL |
			ES_LEFT |
			ES_MULTILINE |
			ES_READONLY |
			ES_AUTOVSCROLL,
			0,
			0,
			0,
			0,
			hwnd,
			nullptr,
			GetModuleHandleA(nullptr),
			nullptr);

		context->closeButton = CreateWindowExA(
			0,
			"BUTTON",
			"Close",
			WS_CHILD |
			WS_VISIBLE |
			WS_TABSTOP |
			BS_DEFPUSHBUTTON,
			0,
			0,
			0,
			0,
			hwnd,
			reinterpret_cast<HMENU>(1),
			GetModuleHandleA(nullptr),
			nullptr);

		if (context->edit == nullptr ||
			context->closeButton == nullptr)
		{
			REBORN_LOG(
				"FAQ_WINDOW_CREATE_FAILED: Failed to create FAQ window controls.");

			return -1;
		}

		SendMessageA(
			context->edit,
			WM_SETFONT,
			reinterpret_cast<WPARAM>(font),
			TRUE);

		SendMessageA(
			context->closeButton,
			WM_SETFONT,
			reinterpret_cast<WPARAM>(font),
			TRUE);

		resizeRebornFAQWindow(hwnd);

		return 0;
	}

	case WM_SIZE:
	{
		resizeRebornFAQWindow(hwnd);
		return 0;
	}

	case WM_GETMINMAXINFO:
	{
		MINMAXINFO* minMaxInfo =
			reinterpret_cast<MINMAXINFO*>(lParam);

		minMaxInfo->ptMinTrackSize.x = 480;
		minMaxInfo->ptMinTrackSize.y = 320;

		return 0;
	}

	case WM_COMMAND:
	{
		if (LOWORD(wParam) == 1)
		{
			DestroyWindow(hwnd);
			return 0;
		}

		break;
	}

	case WM_CLOSE:
	{
		DestroyWindow(hwnd);
		return 0;
	}

	case WM_DESTROY:
	{
		return 0;
	}
	}

	return DefWindowProcA(
		hwnd,
		message,
		wParam,
		lParam);
}


// registerRebornFAQWindowClass ===============================================
/** Reborn: Register the native window class used by Reborn Omega FAQ windows. */
//=============================================================================
static Bool registerRebornFAQWindowClass()
{
	static Bool registered = FALSE;

	if (registered)
		return TRUE;

	WNDCLASSEXA windowClass = {};
	windowClass.cbSize = sizeof(windowClass);
	windowClass.style = CS_HREDRAW | CS_VREDRAW;
	windowClass.lpfnWndProc = rebornFAQWindowProc;
	windowClass.hInstance = GetModuleHandleA(nullptr);
	windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
	windowClass.hbrBackground =
		reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
	windowClass.lpszClassName = RebornFAQWindowClassName;

	if (RegisterClassExA(&windowClass) == 0)
	{
		DWORD error = GetLastError();

		if (error != ERROR_CLASS_ALREADY_EXISTS)
		{
			REBORN_LOG(
				"FAQ_WINDOW_CREATE_FAILED: Failed to register FAQ window class. Error=%lu.",
				error);

			return FALSE;
		}
	}

	registered = TRUE;

	return TRUE;
}


// centerRebornFAQWindow ======================================================
/** Reborn: Center an FAQ window over its owner or the desktop work area. */
//=============================================================================
static void centerRebornFAQWindow(HWND hwnd, HWND owner)
{
	RECT windowRect;
	GetWindowRect(hwnd, &windowRect);

	Int width =
		windowRect.right -
		windowRect.left;

	Int height =
		windowRect.bottom -
		windowRect.top;

	RECT targetRect;

	if (owner && IsWindow(owner))
	{
		GetWindowRect(
			owner,
			&targetRect);
	}
	else
	{
		SystemParametersInfoA(
			SPI_GETWORKAREA,
			0,
			&targetRect,
			0);
	}

	Int x =
		targetRect.left +
		(targetRect.right - targetRect.left - width) / 2;

	Int y =
		targetRect.top +
		(targetRect.bottom - targetRect.top - height) / 2;

	SetWindowPos(
		hwnd,
		HWND_TOP,
		x,
		y,
		0,
		0,
		SWP_NOSIZE);
}


// ShowRebornFAQ ==============================================================
/** Reborn: Load and display a named FAQ in a scrollable modal window. */
//=============================================================================
void ShowRebornFAQ(const char* faqName)
{
	RebornFAQ faq;

	if (!loadRebornFAQ(faqName, faq))
	{
		MessageBoxA(
			GetActiveWindow(),
			"FAQ information could not be loaded.\n\nSee the Reborn Omega log for more information.",
			"Reborn Omega",
			MB_OK |
			MB_ICONERROR |
			MB_SETFOREGROUND |
			MB_TOPMOST);

		return;
	}

	if (!registerRebornFAQWindowClass())
		return;

	RebornFAQWindowContext context;
	context.title = faq.title;
	context.text = buildRebornFAQText(faq);
	context.edit = nullptr;
	context.closeButton = nullptr;

	HWND owner = GetActiveWindow();

	HWND hwnd = CreateWindowExA(
		WS_EX_DLGMODALFRAME,
		RebornFAQWindowClassName,
		context.title.c_str(),
		WS_OVERLAPPED |
		WS_CAPTION |
		WS_SYSMENU |
		WS_THICKFRAME,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		700,
		500,
		owner,
		nullptr,
		GetModuleHandleA(nullptr),
		&context);

	if (hwnd == nullptr)
	{
		REBORN_LOG(
			"FAQ_WINDOW_CREATE_FAILED: Failed to create FAQ window for FAQ '%s'. Error=%lu.",
			faqName,
			GetLastError());

		return;
	}

	centerRebornFAQWindow(
		hwnd,
		owner);

	Bool ownerWasEnabled = FALSE;

	if (owner && IsWindow(owner))
	{
		ownerWasEnabled =
			IsWindowEnabled(owner) != FALSE;

		if (ownerWasEnabled)
			EnableWindow(owner, FALSE);
	}

	ShowWindow(
		hwnd,
		SW_SHOW);

	UpdateWindow(
		hwnd);

	MSG message;

	while (IsWindow(hwnd))
	{
		BOOL result = GetMessageA(
			&message,
			nullptr,
			0,
			0);

		if (result == -1)
			break;

		if (result == 0)
		{
			PostQuitMessage(
				static_cast<int>(message.wParam));
			break;
		}

		if (!IsDialogMessageA(hwnd, &message))
		{
			TranslateMessage(&message);
			DispatchMessageA(&message);
		}
	}

	if (owner &&
		IsWindow(owner) &&
		ownerWasEnabled)
	{
		EnableWindow(
			owner,
			TRUE);

		SetForegroundWindow(
			owner);
	}
}
