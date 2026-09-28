///////////////////////////////////////////////////////////////////////////////////////
// FILE: DefinitionReferenceWindow.cpp
// Author: Gamezerve, September 2026
// Description: Reborn: Modeless viewer for a referenced parsed definition.
///////////////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "DefinitionReferenceWindow.h"
#include "ObjectDeparserDialog.h"

IMPLEMENT_DYNAMIC(CDefinitionReferenceWindow, CFrameWnd)

BEGIN_MESSAGE_MAP(CDefinitionReferenceWindow, CFrameWnd)
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_WM_CLOSE()
END_MESSAGE_MAP()

// Reborn: Store definition identity strings rather than catalog or engine object addresses.
CDefinitionReferenceWindow::CDefinitionReferenceWindow(
	CObjectDeparserDialog* owner,
	const AsciiString& blockType,
	const AsciiString& name)
	: m_owner(owner),
	m_blockType(blockType),
	m_name(name),
	m_backgroundColor(RGB(30, 30, 30)),
	m_textColor(RGB(212, 212, 212))
{
}

// Reborn: Create an unlimited modeless owned frame for this single reference activation.
Bool CDefinitionReferenceWindow::createWindow(CWnd* owner)
{
	CString title;
	title.Format(
		"%s %s - Definition Reference",
		m_blockType.str(),
		m_name.str());

	CRect windowRect(0, 0, 680, 500);

	if (owner && ::IsWindow(owner->GetSafeHwnd()))
	{
		owner->GetWindowRect(&windowRect);
		windowRect.OffsetRect(36, 36);
		windowRect.right = windowRect.left + 680;
		windowRect.bottom = windowRect.top + 500;
	}

	const CString windowClass = AfxRegisterWndClass(
		CS_DBLCLKS,
		::LoadCursor(nullptr, IDC_ARROW),
		static_cast<HBRUSH>(::GetStockObject(BLACK_BRUSH)),
		nullptr);

	return CreateEx(
		WS_EX_TOOLWINDOW,
		windowClass,
		title,
		WS_OVERLAPPEDWINDOW | WS_VISIBLE,
		windowRect,
		owner,
		0) != FALSE;
}

// Reborn: Build the read-only RichEdit surface after the owned frame exists.
int CDefinitionReferenceWindow::OnCreate(
	LPCREATESTRUCT createStruct)
{
	if (CFrameWnd::OnCreate(createStruct) == -1)
		return -1;

	if (!m_contentEdit.Create(
		WS_CHILD |
		WS_VISIBLE |
		WS_VSCROLL |
		WS_HSCROLL |
		ES_MULTILINE |
		ES_AUTOVSCROLL |
		ES_AUTOHSCROLL |
		ES_READONLY |
		ES_NOHIDESEL,
		CRect(0, 0, 0, 0),
		this,
		1))
	{
		return -1;
	}

	m_contentFont.CreatePointFont(95, "Consolas");
	m_contentEdit.SetFont(&m_contentFont);
	m_contentEdit.SendMessage(EM_EXLIMITTEXT, 0, 0x7fffffff);
	m_contentEdit.SetBackgroundColor(FALSE, m_backgroundColor);

	CHARFORMAT2 format = {};
	format.cbSize = sizeof(format);
	format.dwMask = CFM_COLOR;
	format.crTextColor = m_textColor;
	m_contentEdit.SetDefaultCharFormat(format);

	return 0;
}

// Reborn: Keep the reference viewer editor fitted to its independently resizable frame.
void CDefinitionReferenceWindow::OnSize(
	UINT type,
	int width,
	int height)
{
	CFrameWnd::OnSize(type, width, height);

	if (::IsWindow(m_contentEdit.GetSafeHwnd()))
		m_contentEdit.MoveWindow(0, 0, width, height);
}

// Reborn: Show current deparse text without preserving any temporary engine pointer.
void CDefinitionReferenceWindow::setContent(
	const CString& content)
{
	if (!::IsWindow(m_contentEdit.GetSafeHwnd()))
		return;

	m_contentEdit.SetWindowText(content);
	m_contentEdit.SetSel(0, 0);
	m_contentEdit.SendMessage(WM_VSCROLL, SB_TOP, 0);
}

// Reborn: Clear content before reload can invalidate objects used to produce the old text.
void CDefinitionReferenceWindow::setReloading()
{
	setContent(
		"; Reloading INI data...\r\n"
		"; This reference will refresh from the new database when reload completes.\r\n");
}

// Reborn: Destroy this one modeless viewer without affecting sibling reference windows.
void CDefinitionReferenceWindow::OnClose()
{
	DestroyWindow();
}

// Reborn: Unregister the self-owned frame before releasing its C++ object.
void CDefinitionReferenceWindow::PostNcDestroy()
{
	if (m_owner)
		m_owner->onDefinitionReferenceWindowDestroyed(this);

	// Reborn: CFrameWnd performs its standard heap self-destruction here.
	CFrameWnd::PostNcDestroy();
}
