///////////////////////////////////////////////////////////////////////////////////////
// FILE: ObjectDeparserDialog.cpp
// Author: Gamezerve, September 2026
// Description: 
///////////////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "TextDiff.h"
#include "ObjectDeparser.h"
#include "ObjectDeparserDialog.h"
#include "DefinitionReferenceWindow.h"

#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/ThingTemplateDeparser.h"

#include "GameLogic/Weapon.h"
#include "GameLogic/WeaponTemplateDeparser.h"

#include <tom.h>
#include <cctype>

class ScopedRichEditUndoSuspend
{
public:
	explicit ScopedRichEditUndoSuspend(CRichEditCtrl& edit)
		: m_document(nullptr)
	{
		IUnknown* ole = nullptr;

		if (!edit.SendMessage(
			EM_GETOLEINTERFACE,
			0,
			reinterpret_cast<LPARAM>(&ole)) || !ole)
			return;

		const HRESULT result = ole->QueryInterface(
			__uuidof(ITextDocument),
			reinterpret_cast<void**>(&m_document));

		ole->Release();

		if (FAILED(result) || !m_document)
		{
			m_document = nullptr;
			return;
		}

		if (FAILED(m_document->Undo(tomSuspend, nullptr)))
		{
			m_document->Release();
			m_document = nullptr;
		}
	}

	~ScopedRichEditUndoSuspend()
	{
		if (m_document)
		{
			m_document->Undo(tomResume, nullptr);
			m_document->Release();
		}
	}

	bool isActive() const
	{
		return m_document != nullptr;
	}

	ScopedRichEditUndoSuspend(
		const ScopedRichEditUndoSuspend&) = delete;

	ScopedRichEditUndoSuspend& operator=(
		const ScopedRichEditUndoSuspend&) = delete;

private:
	ITextDocument* m_document;
};


struct ReloadCaptureContext
{
	ParsedDefinitionCatalog* catalog;
	CObjectDeparserDialog* dialog;
};


BEGIN_MESSAGE_MAP(CObjectDeparserDialog, CDialog)
	ON_WM_SIZE()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_MOUSEMOVE()
	ON_WM_SETCURSOR()
	ON_WM_PAINT()
	ON_WM_DRAWITEM()
	ON_WM_MEASUREITEM()
	ON_WM_CTLCOLOR()
	ON_EN_CHANGE(IDC_OUTPUT_EDIT, OnOutputChanged)
	// Reborn: Receive clicks for catalog-backed definition links in both RichEdit controls.
	ON_NOTIFY(EN_LINK, IDC_OUTPUT_EDIT, OnDefinitionLink)
	ON_NOTIFY(EN_LINK, IDC_WORK_EDIT, OnDefinitionLink)
	ON_WM_TIMER()
	ON_WM_CLOSE()
	ON_EN_CHANGE(IDC_SEARCH_EDIT, OnSearchChanged)
	ON_LBN_SELCHANGE(IDC_RESULTS_LIST, OnSelectionChanged)
	ON_LBN_DBLCLK(IDC_RESULTS_LIST, OnResultDoubleClicked)
	ON_BN_CLICKED(IDC_DEPARSE_NOW, OnDeparseNow)
	ON_BN_CLICKED(IDC_TRANSFER, OnTransfer)
	ON_BN_CLICKED(IDC_RELOAD_INI, OnReloadINI)
	ON_BN_CLICKED(IDC_COMPARE, OnCompare)
	ON_EN_CHANGE(IDC_WORK_EDIT, OnWorkingCopyChanged)
END_MESSAGE_MAP()

CObjectDeparserDialog::CObjectDeparserDialog(CWnd* parent)
	: CDialog(IDD_OBJECT_DEPARSER_DIALOG, parent),
	m_draggingSplitter(FALSE),
	m_activeSplitter(0),
	m_firstSplitterRatio(0.20),
	m_secondSplitterRatio(0.60),
	m_compareMode(FALSE),
	m_compareUpdating(FALSE),
	m_lastOutputScroll(0, 0),
	m_lastWorkScroll(0, 0),
	m_reloadInProgress(FALSE),
	m_pumpingReloadMessages(FALSE),
	m_updatingDefinitionLinks(FALSE),
	m_lastReloadPumpTick(0),
	m_backgroundColor(RGB(37, 37, 38)),
	m_panelColor(RGB(30, 30, 30)),
	m_textColor(RGB(212, 212, 212)),
	m_secondaryTextColor(RGB(160, 160, 160)),
	m_borderColor(RGB(63, 63, 70)),
	m_unsupportedColor(RGB(240, 106, 106)),
	m_selectionColor(RGB(9, 71, 113))
{
	m_backgroundBrush.CreateSolidBrush(m_backgroundColor);
	m_editBrush.CreateSolidBrush(m_panelColor);

}

void CObjectDeparserDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);

	DDX_Control(pDX, IDC_SEARCH_EDIT, m_searchEdit);
	DDX_Control(pDX, IDC_RESULTS_LIST, m_resultsList);
	DDX_Control(pDX, IDC_DEPARSE_NOW, m_deparseButton);
	DDX_Control(pDX, IDC_TRANSFER, m_transferButton);
	DDX_Control(pDX, IDC_RELOAD_INI, m_reloadButton);
	DDX_Control(pDX, IDC_OUTPUT_EDIT, m_outputEdit);
	DDX_Control(pDX, IDC_WORK_EDIT, m_workEdit);
	DDX_Control(pDX, IDC_OBJECT_COUNT, m_objectCount);
	DDX_Control(pDX, IDC_RELOAD_PROGRESS, m_reloadProgress);
	DDX_Control(pDX, IDC_RELOAD_STATUS, m_reloadStatus);
	DDX_Control(pDX, IDC_COMPARE, m_compareButton);
}

HBRUSH CObjectDeparserDialog::OnCtlColor(
	CDC* pDC,
	CWnd* pWnd,
	UINT nCtlColor)
{

	switch (nCtlColor)
	{
	case CTLCOLOR_DLG:
		pDC->SetBkColor(m_backgroundColor);
		return static_cast<HBRUSH>(
			m_backgroundBrush.GetSafeHandle());

	case CTLCOLOR_STATIC:
		pDC->SetTextColor(m_textColor);
		pDC->SetBkColor(m_backgroundColor);
		pDC->SetBkMode(TRANSPARENT);

		return static_cast<HBRUSH>(
			m_backgroundBrush.GetSafeHandle());

	case CTLCOLOR_EDIT:
		pDC->SetTextColor(m_textColor);
		pDC->SetBkColor(m_panelColor);

		return static_cast<HBRUSH>(
			m_editBrush.GetSafeHandle());

	case CTLCOLOR_LISTBOX:
		pDC->SetTextColor(m_textColor);
		pDC->SetBkColor(m_panelColor);

		return static_cast<HBRUSH>(
			m_editBrush.GetSafeHandle());
	}

	return CDialog::OnCtlColor(
		pDC,
		pWnd,
		nCtlColor);
}

void CObjectDeparserDialog::calculatePaneGeometry(
	CRect& leftPane,
	CRect& firstSplitter,
	CRect& middlePane,
	CRect& secondSplitter,
	CRect& rightPane) const
{
	CRect client;
	GetClientRect(&client);

	const int margin = 10;
	const int gap = 10;
	const int searchHeight = 24;
	const int labelHeight = 18;
	const int statusHeight = 20;
	const int splitterWidth = 6;
	const int minPaneWidth = 160;

	const int panelLabelTop =
		margin + searchHeight + gap;

	const int panelTop =
		panelLabelTop + labelHeight;

	const int panelBottom =
		client.Height() - margin - statusHeight;

	const int contentLeft = margin;
	const int contentRight = client.Width() - margin;
	const int contentWidth = max(1, contentRight - contentLeft);

	int firstX =
		contentLeft +
		static_cast<int>(
			contentWidth * m_firstSplitterRatio);

	int secondX =
		contentLeft +
		static_cast<int>(
			contentWidth * m_secondSplitterRatio);

	firstX = max(
		contentLeft + minPaneWidth,
		min(
			firstX,
			contentRight -
			minPaneWidth * 2 -
			splitterWidth * 2));

	secondX = max(
		firstX + splitterWidth + minPaneWidth,
		min(
			secondX,
			contentRight -
			minPaneWidth -
			splitterWidth));

	leftPane.SetRect(
		contentLeft,
		panelTop,
		firstX,
		panelBottom);

	firstSplitter.SetRect(
		firstX,
		panelLabelTop,
		firstX + splitterWidth,
		panelBottom - 1);

	middlePane.SetRect(
		firstX + splitterWidth,
		panelTop,
		secondX,
		panelBottom);

	secondSplitter.SetRect(
		secondX,
		panelLabelTop,
		secondX + splitterWidth,
		panelBottom - 1);

	rightPane.SetRect(
		secondX + splitterWidth,
		panelTop,
		contentRight,
		panelBottom);
}

void CObjectDeparserDialog::buildDefinitionList()
{
	m_definitions.clear();

	const std::vector<ParsedDefinition>& definitions =
		ObjectDeparserApp()->getDefinitionCatalog().getDefinitions();

	for (const ParsedDefinition& definition : definitions)
		m_definitions.push_back(&definition);

	std::sort(
		m_definitions.begin(),
		m_definitions.end(),
		[](const ParsedDefinition* a, const ParsedDefinition* b)
		{
			return _stricmp(
				a->declaration.str(),
				b->declaration.str()) < 0;
		});
}

void CObjectDeparserDialog::refreshDefinitionList()
{
	CString filter;
	m_searchEdit.GetWindowText(filter);
	filter.MakeLower();

	m_resultsList.SetRedraw(FALSE);
	m_resultsList.ResetContent();

	for (const ParsedDefinition* definition : m_definitions)
	{
		CString declaration(definition->declaration.str());
		CString lowerDeclaration(declaration);

		lowerDeclaration.MakeLower();

		if (!filter.IsEmpty() &&
			lowerDeclaration.Find(filter) == -1)
		{
			continue;
		}

		const int index =
			m_resultsList.AddString(declaration);

		m_resultsList.SetItemDataPtr(
			index,
			const_cast<ParsedDefinition*>(definition));
	}

	CString countText;
	countText.Format(
		"%d definitions",
		m_resultsList.GetCount());

	m_objectCount.SetWindowText(countText);

	m_resultsList.SetRedraw(TRUE);
	m_resultsList.Invalidate();

	m_deparseButton.EnableWindow(FALSE);
}

BOOL CObjectDeparserDialog::OnInitDialog()
{
	CDialog::OnInitDialog();

	m_compareButton.EnableWindow(FALSE);

	SetWindowText("Reborn Omega INI Deparser");

	m_outputFont.CreatePointFont(95, "Consolas");

	m_outputEdit.SetFont(&m_outputFont);
	m_outputEdit.SendMessage(EM_EXLIMITTEXT, 0, 0x7fffffff);

	m_workEdit.SetFont(&m_outputFont);
	m_workEdit.SendMessage(EM_EXLIMITTEXT, 0, 0x7fffffff);

	const DWORD outputEventMask =
		static_cast<DWORD>(
			m_outputEdit.SendMessage(EM_GETEVENTMASK));

	m_outputEdit.SendMessage(
		EM_SETEVENTMASK,
		0,
		// Reborn: Preserve edit notifications and add native link activation notifications.
		outputEventMask | ENM_CHANGE | ENM_LINK);

	const DWORD workEventMask =
		static_cast<DWORD>(
			m_workEdit.SendMessage(EM_GETEVENTMASK));

	m_workEdit.SendMessage(
		EM_SETEVENTMASK,
		0,
		// Reborn: Preserve edit notifications and add native link activation notifications.
		workEventMask | ENM_CHANGE | ENM_LINK);

	m_outputEdit.SetBackgroundColor(
		FALSE,
		m_panelColor);

	m_workEdit.SetBackgroundColor(
		FALSE,
		m_panelColor);

	CHARFORMAT2 textFormat = {};
	textFormat.cbSize = sizeof(textFormat);
	textFormat.dwMask = CFM_COLOR;
	textFormat.crTextColor = m_textColor;

	m_outputEdit.SetDefaultCharFormat(textFormat);
	m_workEdit.SetDefaultCharFormat(textFormat);

	m_reloadProgress.SendMessage(
		PBM_SETBKCOLOR,
		0,
		m_panelColor);

	m_reloadProgress.SendMessage(
		PBM_SETBARCOLOR,
		0,
		RGB(0, 122, 204));

	m_deparseButton.EnableWindow(FALSE);
	m_transferButton.EnableWindow(FALSE);

	m_reloadProgress.SetRange(0, 100);
	m_reloadProgress.SetPos(100);

	m_reloadStatus.SetWindowText(
		"Initial load complete.");

	buildDefinitionList();
	refreshDefinitionList();

	m_deparseButton.EnableWindow(FALSE);

	layoutControls();

	ShowWindow(SW_RESTORE);
	BringWindowToTop();
	SetActiveWindow();
	::SetForegroundWindow(GetSafeHwnd());

	m_searchEdit.SetFocus();

	return FALSE;
}

void CObjectDeparserDialog::OnSize(UINT nType, int cx, int cy)
{
	CDialog::OnSize(nType, cx, cy);

	if (!GetSafeHwnd())
		return;

	layoutControls();

	RedrawWindow(
		nullptr,
		nullptr,
		RDW_INVALIDATE |
		RDW_ERASE |
		RDW_ALLCHILDREN);
}

void CObjectDeparserDialog::reloadProgressCallback(
	Int progress,
	const char* status,
	void* userData)
{
	CObjectDeparserDialog* dialog =
		static_cast<CObjectDeparserDialog*>(userData);

	if (!dialog)
		return;

	dialog->updateReloadProgress(
		progress,
		status);
}

void CObjectDeparserDialog::updateReloadProgress(
	Int progress,
	const char* status)
{
	m_reloadProgress.SetPos(progress);
	m_reloadStatus.SetWindowText(status);

	m_reloadProgress.UpdateWindow();
	m_reloadStatus.UpdateWindow();
	UpdateWindow();

	pumpReloadMessages();

}


void CObjectDeparserDialog::OnClose()
{
	if (m_reloadInProgress)
		return;

	CDialog::OnClose();
}


void CObjectDeparserDialog::layoutControls()
{
	if (!m_searchEdit.GetSafeHwnd())
		return;

	CRect client;
	GetClientRect(&client);

	const int margin = 10;
	const int gap = 10;
	const int labelHeight = 18;
	const int searchHeight = 24;
	const int searchLabelWidth = 50;
	const int deparseWidth = 110;
	const int transferWidth = 100;
	const int reloadWidth = 100;
	const int statusHeight = 20;
	const int compareWidth = 90;

	const int searchTop = margin;

	CWnd* searchLabel = GetDlgItem(IDC_STATIC_SEARCH);
	CWnd* objectsLabel = GetDlgItem(IDC_STATIC_OBJECTS);
	CWnd* outputLabel = GetDlgItem(IDC_STATIC_OUTPUT);
	CWnd* workLabel = GetDlgItem(IDC_STATIC_WORK);

	if (searchLabel)
		searchLabel->MoveWindow(
			margin,
			searchTop + 4,
			searchLabelWidth,
			labelHeight);

	const int searchLeft =
		margin + searchLabelWidth;

	int buttonX =
		client.Width() - margin - reloadWidth;

	m_reloadButton.MoveWindow(
		buttonX,
		searchTop,
		reloadWidth,
		searchHeight);

	buttonX -= gap + compareWidth;

	m_compareButton.MoveWindow(
		buttonX,
		searchTop,
		compareWidth,
		searchHeight);

	buttonX -= gap + transferWidth;

	m_transferButton.MoveWindow(
		buttonX,
		searchTop,
		transferWidth,
		searchHeight);

	buttonX -= gap + deparseWidth;

	m_deparseButton.MoveWindow(
		buttonX,
		searchTop,
		deparseWidth,
		searchHeight);

	const int searchWidth =
		max(
			50,
			buttonX - gap - searchLeft);

	m_searchEdit.MoveWindow(
		searchLeft,
		searchTop,
		searchWidth,
		searchHeight);

	const int panelLabelTop =
		searchTop + searchHeight + gap;
	
	const int panelBottom =
		client.Height() - margin - statusHeight;

	CRect leftPane;
	CRect firstSplitter;
	CRect middlePane;
	CRect secondSplitter;
	CRect rightPane;

	calculatePaneGeometry(
		leftPane,
		firstSplitter,
		middlePane,
		secondSplitter,
		rightPane);

	if (objectsLabel)
	{
		objectsLabel->SetWindowPos(
			&wndTop,
			leftPane.left + 2,
			panelLabelTop,
			max(1, leftPane.Width() - 4),
			labelHeight,
			SWP_SHOWWINDOW);
	}

	if (outputLabel)
	{
		outputLabel->SetWindowPos(
			&wndTop,
			middlePane.left + 2,
			panelLabelTop,
			max(1, middlePane.Width() - 4),
			labelHeight,
			SWP_SHOWWINDOW);
	}

	if (workLabel)
	{
		workLabel->SetWindowText("Working Copy");

		workLabel->SetWindowPos(
			&wndTop,
			rightPane.left + 2,
			panelLabelTop,
			max(1, rightPane.Width() - 4),
			labelHeight,
			SWP_SHOWWINDOW);
	}

	m_resultsList.MoveWindow(
		leftPane.left,
		leftPane.top,
		leftPane.Width(),
		leftPane.Height());

	m_outputEdit.MoveWindow(
		middlePane.left,
		middlePane.top,
		middlePane.Width(),
		middlePane.Height());

	m_workEdit.MoveWindow(
		rightPane.left,
		rightPane.top,
		rightPane.Width(),
		rightPane.Height());

	m_objectCount.MoveWindow(
		leftPane.left,
		panelBottom + 4,
		leftPane.Width(),
		statusHeight);

	const int footerY =
		panelBottom + 3;

	const int reloadStatusWidth = 180;

	m_reloadProgress.MoveWindow(
		middlePane.left,
		footerY,
		rightPane.right -
		middlePane.left -
		reloadStatusWidth -
		gap,
		16);

	m_reloadStatus.MoveWindow(
		rightPane.right -
		reloadStatusWidth,
		footerY,
		reloadStatusWidth,
		16);
}

void CObjectDeparserDialog::OnLButtonDown(
	UINT nFlags,
	CPoint point)
{
	CRect leftPane;
	CRect firstSplitter;
	CRect middlePane;
	CRect secondSplitter;
	CRect rightPane;

	calculatePaneGeometry(
		leftPane,
		firstSplitter,
		middlePane,
		secondSplitter,
		rightPane);

	if (firstSplitter.PtInRect(point))
	{
		m_draggingSplitter = TRUE;
		m_activeSplitter = 1;
		SetCapture();
		return;
	}

	if (secondSplitter.PtInRect(point))
	{
		m_draggingSplitter = TRUE;
		m_activeSplitter = 2;
		SetCapture();
		return;
	}

	CDialog::OnLButtonDown(
		nFlags,
		point);
}

void CObjectDeparserDialog::OnLButtonUp(
	UINT nFlags,
	CPoint point)
{
	if (m_draggingSplitter)
	{
		m_draggingSplitter = FALSE;
		m_activeSplitter = 0;

		if (GetCapture() == this)
			ReleaseCapture();

		return;
	}

	CDialog::OnLButtonUp(
		nFlags,
		point);
}

void CObjectDeparserDialog::OnMouseMove(
	UINT nFlags,
	CPoint point)
{
	if (!m_draggingSplitter)
	{
		CDialog::OnMouseMove(
			nFlags,
			point);

		return;
	}

	CRect client;
	GetClientRect(&client);

	const int margin = 10;
	const int splitterWidth = 6;
	const int minPaneWidth = 160;

	const int contentLeft = margin;
	const int contentRight =
		client.Width() - margin;

	const int contentWidth =
		max(
			1,
			contentRight - contentLeft);

	CRect leftPane;
	CRect firstSplitter;
	CRect middlePane;
	CRect secondSplitter;
	CRect rightPane;

	calculatePaneGeometry(
		leftPane,
		firstSplitter,
		middlePane,
		secondSplitter,
		rightPane);

	const CRect oldFirstSplitter = firstSplitter;
	const CRect oldSecondSplitter = secondSplitter;

	if (m_activeSplitter == 1)
	{
		const int minimumX =
			contentLeft + minPaneWidth;

		const int maximumX =
			secondSplitter.left -
			splitterWidth -
			minPaneWidth;

		const int pointX = static_cast<int>(point.x);

		const int newX =
			pointX < minimumX
			? minimumX
			: pointX > maximumX
			? maximumX
			: pointX;

		m_firstSplitterRatio =
			static_cast<double>(
				newX - contentLeft) /
			static_cast<double>(
				contentWidth);
	}
	else if (m_activeSplitter == 2)
	{
		const int minimumX =
			firstSplitter.right +
			minPaneWidth;

		const int maximumX =
			contentRight -
			splitterWidth -
			minPaneWidth;

		const int pointX = static_cast<int>(point.x);

		const int newX =
			pointX < minimumX
			? minimumX
			: pointX > maximumX
			? maximumX
			: pointX;

		m_secondSplitterRatio =
			static_cast<double>(
				newX - contentLeft) /
			static_cast<double>(
				contentWidth);
	}

	layoutControls();

	CRect newLeftPane;
	CRect newFirstSplitter;
	CRect newMiddlePane;
	CRect newSecondSplitter;
	CRect newRightPane;

	calculatePaneGeometry(
		newLeftPane,
		newFirstSplitter,
		newMiddlePane,
		newSecondSplitter,
		newRightPane);

	InvalidateRect(
		&oldFirstSplitter,
		TRUE);

	InvalidateRect(
		&oldSecondSplitter,
		TRUE);

	InvalidateRect(
		&newFirstSplitter,
		TRUE);

	InvalidateRect(
		&newSecondSplitter,
		TRUE);

	if (m_activeSplitter == 1)
	{
		m_resultsList.Invalidate(FALSE);
		m_resultsList.UpdateWindow();
	}

	UpdateWindow();
}

BOOL CObjectDeparserDialog::OnSetCursor(
	CWnd* pWnd,
	UINT nHitTest,
	UINT message)
{
	CPoint point;
	GetCursorPos(&point);
	ScreenToClient(&point);

	CRect leftPane;
	CRect firstSplitter;
	CRect middlePane;
	CRect secondSplitter;
	CRect rightPane;

	calculatePaneGeometry(
		leftPane,
		firstSplitter,
		middlePane,
		secondSplitter,
		rightPane);

	if (m_draggingSplitter ||
		firstSplitter.PtInRect(point) ||
		secondSplitter.PtInRect(point))
	{
		::SetCursor(
			AfxGetApp()->LoadStandardCursor(
				IDC_SIZEWE));

		return TRUE;
	}

	return CDialog::OnSetCursor(
		pWnd,
		nHitTest,
		message);
}

void CObjectDeparserDialog::OnPaint()
{
	CDialog::OnPaint();

	CRect leftPane;
	CRect firstSplitter;
	CRect middlePane;
	CRect secondSplitter;
	CRect rightPane;

	calculatePaneGeometry(
		leftPane,
		firstSplitter,
		middlePane,
		secondSplitter,
		rightPane);

	CClientDC dc(this);

	dc.FillSolidRect(
		firstSplitter,
		m_borderColor);

	dc.FillSolidRect(
		secondSplitter,
		m_borderColor);
}


void CObjectDeparserDialog::clearCompareHighlight()
{
	ScopedRichEditUndoSuspend outputUndo(m_outputEdit);
	ScopedRichEditUndoSuspend workUndo(m_workEdit);

	if (!outputUndo.isActive() || !workUndo.isActive())
		return;

	CHARFORMAT2 format = {};
	format.cbSize = sizeof(format);
	format.dwMask = CFM_BACKCOLOR | CFM_COLOR;
	format.dwEffects = CFE_AUTOBACKCOLOR;
	format.crTextColor = m_textColor;

	long start;
	long end;

	m_outputEdit.GetSel(start, end);
	m_outputEdit.SetSel(0, -1);
	m_outputEdit.SetSelectionCharFormat(format);
	m_outputEdit.SetSel(start, end);

	m_workEdit.GetSel(start, end);
	m_workEdit.SetSel(0, -1);
	m_workEdit.SetSelectionCharFormat(format);
	m_workEdit.SetSel(start, end);
}



void CObjectDeparserDialog::highlightLines(
	CRichEditCtrl& edit,
	const std::vector<Int>& lines,
	COLORREF color)
{
	ScopedRichEditUndoSuspend undo(edit);

	if (!undo.isActive())
		return;

	long oldStart;
	long oldEnd;

	edit.GetSel(oldStart, oldEnd);

	CHARFORMAT2 format = {};
	format.cbSize = sizeof(format);
	format.dwMask = CFM_BACKCOLOR | CFM_COLOR;
	format.crBackColor = color;
	format.crTextColor = RGB(0, 0, 0);

	const int lineCount = edit.GetLineCount();

	for (Int line : lines)
	{
		if (line < 0 || line >= lineCount)
			continue;

		const long start = edit.LineIndex(line);

		const long end =
			line + 1 < lineCount
			? edit.LineIndex(line + 1)
			: edit.GetTextLength();

		if (start < 0)
			continue;

		edit.SetSel(start, end);
		edit.SetSelectionCharFormat(format);
	}

	edit.SetSel(oldStart, oldEnd);
}


static CPoint getEditorScroll(
	CRichEditCtrl& edit)
{
	POINT position = {};

	edit.SendMessage(
		EM_GETSCROLLPOS,
		0,
		reinterpret_cast<LPARAM>(&position));

	return CPoint(
		position.x,
		position.y);
}

void CObjectDeparserDialog::OnCompare()
{
	if (m_compareMode)
	{
		stopCompareMode();
		return;
	}

	if (m_outputEdit.GetWindowTextLength() == 0 ||
		m_workEdit.GetWindowTextLength() == 0)
	{
		return;
	}

	m_compareMode = TRUE;

	m_compareButton.SetWindowText(
		"Stop Compare");

	performCompare();

	CPoint top(0, 0);

	m_outputEdit.SendMessage(
		EM_SETSCROLLPOS,
		0,
		reinterpret_cast<LPARAM>(&top));

	m_workEdit.SendMessage(
		EM_SETSCROLLPOS,
		0,
		reinterpret_cast<LPARAM>(&top));

	m_lastOutputScroll =
		getEditorScroll(m_outputEdit);

	m_lastWorkScroll =
		getEditorScroll(m_workEdit);

	SetTimer(
		TIMER_COMPARE_SCROLL,
		30,
		nullptr);

	updateCompareButtonState();
}

void CObjectDeparserDialog::stopCompareMode()
{
	if (!m_compareMode)
		return;

	KillTimer(TIMER_COMPARE_SCROLL);
	KillTimer(TIMER_COMPARE_DEBOUNCE);

	m_compareMode = FALSE;
	m_compareUpdating = TRUE;

	const CPoint outputScroll =
		getEditorScroll(m_outputEdit);

	const CPoint workScroll =
		getEditorScroll(m_workEdit);

	m_outputEdit.SetRedraw(FALSE);
	m_workEdit.SetRedraw(FALSE);

	clearCompareHighlight();

	m_outputEdit.SendMessage(
		EM_SETSCROLLPOS,
		0,
		reinterpret_cast<LPARAM>(&outputScroll));

	m_workEdit.SendMessage(
		EM_SETSCROLLPOS,
		0,
		reinterpret_cast<LPARAM>(&workScroll));

	m_outputEdit.SetRedraw(TRUE);
	m_workEdit.SetRedraw(TRUE);

	m_outputEdit.Invalidate(FALSE);
	m_workEdit.Invalidate(FALSE);

	m_compareUpdating = FALSE;

	m_compareButton.SetWindowText(
		"Compare");

	m_reloadStatus.SetWindowText(
		"Compare mode off.");

	updateCompareButtonState();
}

void CObjectDeparserDialog::restartCompareDebounce()
{
	if (!m_compareMode || m_compareUpdating)
		return;

	KillTimer(TIMER_COMPARE_DEBOUNCE);

	SetTimer(
		TIMER_COMPARE_DEBOUNCE,
		500,
		nullptr);
}

void CObjectDeparserDialog::updateCompareButtonState()
{
	m_compareButton.EnableWindow(
		m_compareMode ||
		(m_outputEdit.GetWindowTextLength() > 0 &&
			m_workEdit.GetWindowTextLength() > 0));
}

void CObjectDeparserDialog::OnWorkingCopyChanged()
{
	if (m_compareUpdating || m_updatingDefinitionLinks)
		return;

	// Reborn: Delay catalog rescanning until the user pauses typing.
	KillTimer(TIMER_DEFINITION_LINKS);
	SetTimer(TIMER_DEFINITION_LINKS, 180, nullptr);

	restartCompareDebounce();
	updateCompareButtonState();
}

void CObjectDeparserDialog::OnOutputChanged()
{
	if (m_compareUpdating || m_updatingDefinitionLinks)
		return;

	// Reborn: Delay link formatting until programmatic output replacement has settled.
	KillTimer(TIMER_DEFINITION_LINKS);
	SetTimer(TIMER_DEFINITION_LINKS, 180, nullptr);

	restartCompareDebounce();
	updateCompareButtonState();
}

void CObjectDeparserDialog::OnTimer(
	UINT_PTR nIDEvent)
{
	if (nIDEvent == TIMER_COMPARE_SCROLL)
	{
		synchronizeCompareScroll();
		return;
	}

	if (nIDEvent == TIMER_COMPARE_DEBOUNCE)
	{
		KillTimer(TIMER_COMPARE_DEBOUNCE);

		if (m_compareMode)
			performCompare();

		return;
	}

	if (nIDEvent == TIMER_DEFINITION_LINKS)
	{
		KillTimer(TIMER_DEFINITION_LINKS);

		// Reborn: Never consult a catalog while reload is replacing its backing entries.
		if (!m_reloadInProgress)
		{
			updateDefinitionLinks(m_outputEdit);
			updateDefinitionLinks(m_workEdit);
		}

		return;
	}

	CDialog::OnTimer(nIDEvent);
}

void CObjectDeparserDialog::OnTransfer()
{
	CString text;
	m_outputEdit.GetWindowText(text);

	m_workEdit.SetWindowText(text);

	if (m_compareMode)
		restartCompareDebounce();

	m_workEdit.SetFocus();

	clearCompareHighlight();
	updateCompareButtonState();
}

void CObjectDeparserDialog::OnSearchChanged()
{
	refreshDefinitionList();
}

void CObjectDeparserDialog::OnSelectionChanged()
{
	m_deparseButton.EnableWindow(
		m_resultsList.GetCurSel() != LB_ERR);
}

void CObjectDeparserDialog::OnResultDoubleClicked()
{
	OnDeparseNow();
}

Bool CObjectDeparserDialog::isDefinitionImplemented(
	const ParsedDefinition* definition) const
{
	if (!definition)
		return FALSE;

	return
		definition->blockType.compareNoCase("Object") == 0 ||
		definition->blockType.compareNoCase("ObjectInherit") == 0 ||
		definition->blockType.compareNoCase("ObjectReskin") == 0;
}

void CObjectDeparserDialog::OnMeasureItem(
	int nIDCtl,
	LPMEASUREITEMSTRUCT lpMeasureItemStruct)
{
	if (nIDCtl == IDC_RESULTS_LIST)
	{
		lpMeasureItemStruct->itemHeight = 18;
		return;
	}

	CDialog::OnMeasureItem(
		nIDCtl,
		lpMeasureItemStruct);
}

void CObjectDeparserDialog::OnDrawItem(
	int nIDCtl,
	LPDRAWITEMSTRUCT lpDrawItemStruct)
{
	if (nIDCtl != IDC_RESULTS_LIST)
	{
		CDialog::OnDrawItem(
			nIDCtl,
			lpDrawItemStruct);

		return;
	}

	if (lpDrawItemStruct->itemID == static_cast<UINT>(-1))
		return;

	CDC dc;
	dc.Attach(lpDrawItemStruct->hDC);

	CFont* oldFont = dc.SelectObject(
		m_resultsList.GetFont());

	CString text;
	m_resultsList.GetText(
		lpDrawItemStruct->itemID,
		text);

	const ParsedDefinition* definition =
		static_cast<const ParsedDefinition*>(
			m_resultsList.GetItemDataPtr(
				lpDrawItemStruct->itemID));

	const Bool selected =
		(lpDrawItemStruct->itemState & ODS_SELECTED) != 0;

	const COLORREF backgroundColor =
		selected
		? m_selectionColor
		: m_panelColor;

	COLORREF textColor;

	if (!isDefinitionImplemented(definition))
	{
		textColor =
			selected
			? RGB(255, 190, 190)
			: m_unsupportedColor;
	}
	else
	{
		textColor = m_textColor;
	}

	dc.FillSolidRect(
		&lpDrawItemStruct->rcItem,
		backgroundColor);

	dc.SetBkMode(TRANSPARENT);
	dc.SetTextColor(textColor);

	CRect textRect =
		lpDrawItemStruct->rcItem;

	textRect.left += 3;

	dc.DrawText(
		text,
		&textRect,
		DT_SINGLELINE |
		DT_VCENTER |
		DT_NOPREFIX |
		DT_END_ELLIPSIS);

	if (lpDrawItemStruct->itemState & ODS_FOCUS)
		dc.DrawFocusRect(
			&lpDrawItemStruct->rcItem);

	if (oldFont)
		dc.SelectObject(oldFont);

	dc.Detach();
}

// Reborn: Treat the characters used by registered INI identifiers as one clickable token.
static Bool isDefinitionReferenceCharacter(
	TCHAR character)
{
	return _istalnum(character) ||
		character == '_' ||
		character == '-' ||
		character == '.' ||
		character == ':';
}

// Reborn: Numeric scalars can share names with Rank definitions but are never identifier references.
static Bool isPotentialDefinitionReferenceToken(
	const CString& token)
{
	if (token.IsEmpty())
		return FALSE;

	const TCHAR firstCharacter = token.GetAt(0);
	return _istalpha(firstCharacter) || firstCharacter == '_';
}

// Reborn: Collapse CRLF to RichEdit's single internal paragraph mark for exact selection indices.
static void getRichEditIndexText(
	CRichEditCtrl& edit,
	CString& indexText)
{
	CString windowText;
	edit.GetWindowText(windowText);

	indexText.Empty();
	LPTSTR output = indexText.GetBuffer(windowText.GetLength());
	Int outputLength = 0;

	for (Int index = 0; index < windowText.GetLength(); ++index)
	{
		const TCHAR character = windowText.GetAt(index);

		if (character == '\n' &&
			index > 0 &&
			windowText.GetAt(index - 1) == '\r')
		{
			continue;
		}

		output[outputLength++] = character;
	}

	indexText.ReleaseBuffer(outputLength);
}

// Reborn: Find the definition declared by one editor without relying on the other editor's selection.
const ParsedDefinition* CObjectDeparserDialog::resolveEditorDefinition(
	const CString& text) const
{
	const ParsedDefinitionCatalog& catalog =
		ObjectDeparserApp()->getDefinitionCatalog();

	long lineStart = 0;
	const long textLength = text.GetLength();

	while (lineStart < textLength)
	{
		long lineEnd = lineStart;

		while (lineEnd < textLength &&
			text.GetAt(lineEnd) != '\r' &&
			text.GetAt(lineEnd) != '\n')
		{
			++lineEnd;
		}

		CString line =
			text.Mid(lineStart, lineEnd - lineStart);
		line.Trim();

		if (!line.IsEmpty() && line.GetAt(0) != ';')
		{
			long typeStart = 0;

			while (typeStart < line.GetLength() &&
				!isDefinitionReferenceCharacter(line.GetAt(typeStart)))
			{
				++typeStart;
			}

			long typeEnd = typeStart;

			while (typeEnd < line.GetLength() &&
				isDefinitionReferenceCharacter(line.GetAt(typeEnd)))
			{
				++typeEnd;
			}

			long nameStart = typeEnd;

			while (nameStart < line.GetLength() &&
				!isDefinitionReferenceCharacter(line.GetAt(nameStart)))
			{
				++nameStart;
			}

			long nameEnd = nameStart;

			while (nameEnd < line.GetLength() &&
				isDefinitionReferenceCharacter(line.GetAt(nameEnd)))
			{
				++nameEnd;
			}

			if (typeEnd > typeStart && nameEnd > nameStart)
			{
				const CString type =
					line.Mid(typeStart, typeEnd - typeStart);
				const CString name =
					line.Mid(nameStart, nameEnd - nameStart);

				const CStringA typeAnsi(type);
				const CStringA nameAnsi(name);

				const ParsedDefinition* definition =
					catalog.findDefinition(
						nameAnsi.GetString(),
						typeAnsi.GetString());

				if (definition)
					return definition;
			}
		}

		lineStart = lineEnd + 1;
	}

	return nullptr;
}

// Reborn: Read the assignment key or declaration keyword that supplies a safe type hint.
AsciiString CObjectDeparserDialog::getReferenceTypeHint(
	const CString& text,
	long tokenStart) const
{
	long lineStart = tokenStart;

	while (lineStart > 0 &&
		text.GetAt(lineStart - 1) != '\r' &&
		text.GetAt(lineStart - 1) != '\n')
	{
		--lineStart;
	}

	long hintEnd = -1;

	for (long index = lineStart; index < tokenStart; ++index)
	{
		if (text.GetAt(index) == '=')
			hintEnd = index;
	}

	long hintStart = lineStart;

	if (hintEnd >= 0)
	{
		while (hintEnd > lineStart &&
			!isDefinitionReferenceCharacter(text.GetAt(hintEnd - 1)))
		{
			--hintEnd;
		}

		hintStart = hintEnd;

		while (hintStart > lineStart &&
			isDefinitionReferenceCharacter(text.GetAt(hintStart - 1)))
		{
			--hintStart;
		}
	}
	else
	{
		while (hintStart < tokenStart &&
			!isDefinitionReferenceCharacter(text.GetAt(hintStart)))
		{
			++hintStart;
		}

		hintEnd = hintStart;

		while (hintEnd < tokenStart &&
			isDefinitionReferenceCharacter(text.GetAt(hintEnd)))
		{
			++hintEnd;
		}
	}

	if (hintEnd <= hintStart || hintStart == tokenStart)
		return AsciiString::TheEmptyString;

	const CString hint =
		text.Mid(hintStart, hintEnd - hintStart);

	const CStringA hintAnsi(hint);
	return hintAnsi.GetString();
}

// Reborn: Mark only references that resolve to exactly one current catalog name/type target.
void CObjectDeparserDialog::updateDefinitionLinks(
	CRichEditCtrl& edit)
{
	if (m_reloadInProgress ||
		m_updatingDefinitionLinks ||
		!::IsWindow(edit.GetSafeHwnd()))
	{
		return;
	}

	CString text;
	// Reborn: Calculate every CFE_LINK range in the same coordinate space used by SetSel.
	getRichEditIndexText(edit, text);

	long oldStart = 0;
	long oldEnd = 0;
	edit.GetSel(oldStart, oldEnd);

	const CPoint oldScroll = getEditorScroll(edit);
	ScopedRichEditUndoSuspend undo(edit);

	if (!undo.isActive())
		return;

	m_updatingDefinitionLinks = TRUE;
	edit.SetRedraw(FALSE);

	CHARFORMAT2 linkFormat = {};
	linkFormat.cbSize = sizeof(linkFormat);
	linkFormat.dwMask = CFM_LINK;
	linkFormat.dwEffects = 0;

	edit.SetSel(0, -1);
	edit.SetSelectionCharFormat(linkFormat);

	const ParsedDefinitionCatalog& catalog =
		ObjectDeparserApp()->getDefinitionCatalog();
	// Reborn: Self-link filtering belongs to this editor's declaration, not the main list selection.
	const ParsedDefinition* editorDefinition =
		resolveEditorDefinition(text);

	long tokenStart = 0;
	const long textLength = text.GetLength();

	while (tokenStart < textLength)
	{
		while (tokenStart < textLength &&
			!isDefinitionReferenceCharacter(text.GetAt(tokenStart)))
		{
			++tokenStart;
		}

		long tokenEnd = tokenStart;

		while (tokenEnd < textLength &&
			isDefinitionReferenceCharacter(text.GetAt(tokenEnd)))
		{
			++tokenEnd;
		}

		if (tokenEnd > tokenStart)
		{
			const CString token =
				text.Mid(tokenStart, tokenEnd - tokenStart);

			if (isPotentialDefinitionReferenceToken(token))
			{
				const CStringA tokenAnsi(token);
				const AsciiString typeHint =
					getReferenceTypeHint(text, tokenStart);

				const ParsedDefinition* definition =
					catalog.resolveReference(
						tokenAnsi.GetString(),
						typeHint);

				if (definition && definition != editorDefinition)
				{
					linkFormat.dwEffects = CFE_LINK;
					edit.SetSel(tokenStart, tokenEnd);
					edit.SetSelectionCharFormat(linkFormat);
					linkFormat.dwEffects = 0;
				}
			}
		}

		tokenStart = tokenEnd + 1;
	}

	edit.SetSel(oldStart, oldEnd);
	edit.SendMessage(
		EM_SETSCROLLPOS,
		0,
		reinterpret_cast<LPARAM>(&oldScroll));

	edit.SetRedraw(TRUE);
	edit.Invalidate(FALSE);
	m_updatingDefinitionLinks = FALSE;
}

// Reborn: Re-resolve clicked text so delayed edits or reloads can never use stale link targets.
const ParsedDefinition* CObjectDeparserDialog::resolveDefinitionLink(
	CRichEditCtrl& edit,
	const CHARRANGE& range) const
{
	if (range.cpMin < 0 || range.cpMax <= range.cpMin)
		return nullptr;

	CString text;
	// Reborn: EN_LINK ranges use RichEdit coordinates, so resolve from the normalized index text.
	getRichEditIndexText(edit, text);

	if (range.cpMax > text.GetLength())
		return nullptr;

	const CString token =
		text.Mid(range.cpMin, range.cpMax - range.cpMin);

	// Reborn: Reject stale or externally formatted numeric links at activation time as well.
	if (!isPotentialDefinitionReferenceToken(token))
		return nullptr;

	const CStringA tokenAnsi(token);
	const AsciiString typeHint =
		getReferenceTypeHint(text, range.cpMin);

	const ParsedDefinition* definition =
		ObjectDeparserApp()->getDefinitionCatalog().resolveReference(
		tokenAnsi.GetString(),
		typeHint);

	const ParsedDefinition* editorDefinition =
		resolveEditorDefinition(text);

	return definition == editorDefinition
		? nullptr
		: definition;
}

// Reborn: Route a RichEdit link click to a fresh, independent modeless reference viewer.
void CObjectDeparserDialog::OnDefinitionLink(
	NMHDR* notifyHeader,
	LRESULT* result)
{
	*result = 0;

	if (m_reloadInProgress || !notifyHeader)
		return;

	const ENLINK* link =
		reinterpret_cast<const ENLINK*>(notifyHeader);

	// Reborn: RichEdit reliably reports linked text activation on button-down, as in WorldBuilder.
	if (link->msg != WM_LBUTTONDOWN)
		return;

	CRichEditCtrl* edit =
		notifyHeader->idFrom == IDC_OUTPUT_EDIT
		? &m_outputEdit
		: &m_workEdit;

	const ParsedDefinition* definition =
		resolveDefinitionLink(*edit, link->chrg);

	if (!definition)
	{
		m_reloadStatus.SetWindowText(
			"Reference is ambiguous or no longer available.");
		return;
	}

	openDefinitionReference(*definition);
}

// Reborn: Give every click a separately allocated owned window with freshly generated text.
void CObjectDeparserDialog::openDefinitionReference(
	const ParsedDefinition& definition)
{
	std::string output;

	if (!buildDeparsedDefinitionText(definition, output))
		return;

	CDefinitionReferenceWindow* window =
		new CDefinitionReferenceWindow(
			this,
			definition.blockType,
			definition.name);

	if (!window->createWindow(this))
	{
		// Reborn: CFrameWnd releases itself through PostNcDestroy when creation fails.
		return;
	}

	m_referenceWindows.push_back(window);
	window->setContent(CString(output.c_str()));
	window->ShowWindow(SW_SHOW);
	window->BringWindowToTop();
}

// Reborn: Remove a closed self-deleting viewer while leaving every sibling window intact.
void CObjectDeparserDialog::onDefinitionReferenceWindowDestroyed(
	CDefinitionReferenceWindow* window)
{
	m_referenceWindows.erase(
		std::remove(
			m_referenceWindows.begin(),
			m_referenceWindows.end(),
			window),
		m_referenceWindows.end());
}

// Reborn: Clear all child deparse snapshots before their source engine objects are replaced.
void CObjectDeparserDialog::setReferenceWindowsReloading()
{
	for (CDefinitionReferenceWindow* window : m_referenceWindows)
	{
		if (window && ::IsWindow(window->GetSafeHwnd()))
			window->setReloading();
	}
}

// Reborn: Refresh children from stable name/type identities after the new catalog is complete.
void CObjectDeparserDialog::refreshReferenceWindows(
	Bool reloadSucceeded)
{
	const ParsedDefinitionCatalog& catalog =
		ObjectDeparserApp()->getDefinitionCatalog();

	for (CDefinitionReferenceWindow* window : m_referenceWindows)
	{
		if (!window || !::IsWindow(window->GetSafeHwnd()))
			continue;

		if (!reloadSucceeded)
		{
			window->setContent(
				"; Reload failed. Reference content was not regenerated.\r\n");
			continue;
		}

		const ParsedDefinition* definition =
			catalog.findDefinition(
				window->getDefinitionName(),
				window->getBlockType());

		if (!definition)
		{
			window->setContent(
				"; This definition no longer exists after reload.\r\n");
			continue;
		}

		std::string output;
		buildDeparsedDefinitionText(*definition, output);
		window->setContent(CString(output.c_str()));
	}
}

// Reborn: Produce deparse text from a short-lived catalog lookup shared by main and child views.
Bool CObjectDeparserDialog::buildDeparsedDefinitionText(
	const ParsedDefinition& definition,
	std::string& output) const
{
	output.clear();

	const CString& loadTime =
		ObjectDeparserApp()->getLastObjectIniLoadTime();

	CStringA loadTimeAnsi(loadTime);

	output += "; INI Load Completed: ";
	output += loadTimeAnsi.GetString();
	output += "\r\n";

	std::string sourceFilename =
		definition.filename.str();

	std::replace(
		sourceFilename.begin(),
		sourceFilename.end(),
		'\\',
		'/');

	output += "; Source: ";
	output += sourceFilename;
	output += "\r\n\r\n";

	if (isDefinitionImplemented(&definition))
	{
		const ThingTemplate* thing =
			TheThingFactory->findTemplate(
				definition.name,
				FALSE);

		if (thing)
		{
			output += ThingTemplateDeparser::deparse(thing);
		}
		else
		{
			output += definition.declaration.str();
			output += "\r\n\r\n";
			output += "; ThingTemplate was not found.\r\n";
		}
	}
	else if (definition.blockType.compareNoCase("Weapon") == 0)
	{
		const WeaponTemplate* weapon =
			TheWeaponStore->findWeaponTemplate(
				definition.name);

		if (weapon)
		{
			output += WeaponTemplateDeparser::deparse(
				weapon);
		}
		else
		{
			output += definition.declaration.str();
			output += "\r\n\r\n";
			output += "; WeaponTemplate was not found.\r\n";
		}
	}
	else
	{
		output += definition.declaration.str();
		output += "\r\n\r\n";
		output += "; Deparser for this definition type is not implemented yet.\r\n";
	}

	return TRUE;
}

// Reborn: Render the selected definition and immediately expose its safe catalog references.
void CObjectDeparserDialog::OnDeparseNow()
{
	clearCompareHighlight();

	const int index = m_resultsList.GetCurSel();

	if (index == LB_ERR)
		return;

	const ParsedDefinition* definition =
		static_cast<const ParsedDefinition*>(
			m_resultsList.GetItemDataPtr(index));

	if (!definition)
		return;

	std::string output;

	// Reborn: Use the same fresh deparse path used by modeless reference windows.
	if (!buildDeparsedDefinitionText(*definition, output))
		return;

	m_outputEdit.SetWindowText(output.c_str());
	updateDefinitionLinks(m_outputEdit);

	if (m_compareMode)
		restartCompareDebounce();

	m_transferButton.EnableWindow(TRUE);

	updateCompareButtonState();
}

void CObjectDeparserDialog::OnOK()
{
}

void CObjectDeparserDialog::OnCancel()
{

	if (m_reloadInProgress)
		return;

	const int result = MessageBox(
		"Are you sure you want to exit?",
		"Reborn Omega INI Deparser",
		MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);

	if (result == IDYES)
		CDialog::OnCancel();
}


void CObjectDeparserDialog::OnReloadINI()
{
	if (m_reloadInProgress)
		return;

	stopCompareMode();

	CString selectedName;

	const int selectedIndex =
		m_resultsList.GetCurSel();

	if (selectedIndex != LB_ERR)
	{
		m_resultsList.GetText(
			selectedIndex,
			selectedName);
	}

	m_reloadInProgress = TRUE;
	m_pumpingReloadMessages = FALSE;
	m_lastReloadPumpTick = ::GetTickCount() - 30;
	// Reborn: Stop pending link scans and clear child snapshots before engine replacement starts.
	KillTimer(TIMER_DEFINITION_LINKS);
	setReferenceWindowsReloading();

	m_deparseButton.EnableWindow(FALSE);
	m_transferButton.EnableWindow(FALSE);
	m_compareButton.EnableWindow(FALSE);
	m_reloadButton.EnableWindow(FALSE);
	m_searchEdit.EnableWindow(FALSE);
	m_resultsList.EnableWindow(FALSE);
	m_workEdit.SetReadOnly(TRUE);

	m_resultsList.SetRedraw(FALSE);
	m_resultsList.ResetContent();
	m_definitions.clear();
	m_resultsList.SetRedraw(TRUE);
	m_resultsList.Invalidate(FALSE);

	m_outputEdit.SetWindowText("");
	m_objectCount.SetWindowText("Reloading...");

	updateReloadProgress(
		0,
		"Starting reload...");

	ReloadCaptureContext context = {};
	context.catalog =
		&ObjectDeparserApp()->getDefinitionCatalog();
	context.dialog = this;

	INI::setBlockParsedProc(
		&CObjectDeparserDialog::reloadBlockParsedCallback,
		&context);

	const Bool success =
		ObjectDeparserApp()->reloadObjectDatabase(
			&CObjectDeparserDialog::reloadProgressCallback,
			this);

	INI::setBlockParsedProc(
		&ParsedDefinitionCatalog::capture,
		context.catalog);

	buildDefinitionList();
	refreshDefinitionList();

	m_reloadInProgress = FALSE;
	m_pumpingReloadMessages = FALSE;

	m_searchEdit.EnableWindow(TRUE);
	m_resultsList.EnableWindow(TRUE);
	m_workEdit.SetReadOnly(FALSE);
	m_reloadButton.EnableWindow(TRUE);

	updateCompareButtonState();
	// Reborn: Child windows now resolve identity against only the newly completed catalog.
	refreshReferenceWindows(success);
	updateDefinitionLinks(m_outputEdit);
	updateDefinitionLinks(m_workEdit);

	if (!success)
	{
		MessageBox(
			"Failed to reload Object INI files.",
			"Object Deparser",
			MB_OK | MB_ICONERROR);

		return;
	}

	if (!selectedName.IsEmpty())
	{
		selectDefinitionByDeclaration(
			selectedName);
	}
}


void CObjectDeparserDialog::selectDefinitionByDeclaration(const CString& declaration)
{
	for (int i = 0; i < m_resultsList.GetCount(); ++i)
	{
		CString currentDeclaration;
		m_resultsList.GetText(i, currentDeclaration);

		if (currentDeclaration.CompareNoCase(declaration) != 0)
			continue;

		m_resultsList.SetCurSel(i);

		OnSelectionChanged();
		OnDeparseNow();

		return;
	}
}

static void setEditorVerticalScroll(
	CRichEditCtrl& edit,
	LONG y)
{
	CPoint position =
		getEditorScroll(edit);

	position.y = y;

	edit.SendMessage(
		EM_SETSCROLLPOS,
		0,
		reinterpret_cast<LPARAM>(&position));
}

void CObjectDeparserDialog::synchronizeCompareScroll()
{
	if (!m_compareMode || m_compareUpdating)
		return;

	const CPoint outputScroll =
		getEditorScroll(m_outputEdit);

	const CPoint workScroll =
		getEditorScroll(m_workEdit);

	const Bool outputChanged =
		outputScroll.y != m_lastOutputScroll.y;

	const Bool workChanged =
		workScroll.y != m_lastWorkScroll.y;

	if (!outputChanged && !workChanged)
		return;

	if (outputChanged &&
		(!workChanged ||
			::GetFocus() != m_workEdit.GetSafeHwnd()))
	{
		setEditorVerticalScroll(
			m_workEdit,
			outputScroll.y);
	}
	else
	{
		setEditorVerticalScroll(
			m_outputEdit,
			workScroll.y);
	}

	m_lastOutputScroll =
		getEditorScroll(m_outputEdit);

	m_lastWorkScroll =
		getEditorScroll(m_workEdit);
}

void CObjectDeparserDialog::performCompare()
{
	if (!m_compareMode)
		return;

	const CPoint outputScroll =
		getEditorScroll(m_outputEdit);

	const CPoint workScroll =
		getEditorScroll(m_workEdit);

	const LONG commonY =
		::GetFocus() == m_workEdit.GetSafeHwnd()
		? workScroll.y
		: outputScroll.y;

	CString leftText;
	CString rightText;

	m_outputEdit.GetWindowText(leftText);
	m_workEdit.GetWindowText(rightText);

	CStringA leftAnsi(leftText);
	CStringA rightAnsi(rightText);

	const TextDiffResult result =
		TextDiff::compare(
			leftAnsi.GetString(),
			rightAnsi.GetString());

	m_compareUpdating = TRUE;

	m_outputEdit.SetRedraw(FALSE);
	m_workEdit.SetRedraw(FALSE);

	clearCompareHighlight();

	highlightLines(
		m_outputEdit,
		result.leftChangedLines,
		RGB(255, 210, 210));

	highlightLines(
		m_workEdit,
		result.rightChangedLines,
		RGB(210, 255, 210));

	setEditorVerticalScroll(
		m_outputEdit,
		commonY);

	setEditorVerticalScroll(
		m_workEdit,
		commonY);

	m_outputEdit.SetRedraw(TRUE);
	m_workEdit.SetRedraw(TRUE);

	m_outputEdit.Invalidate(FALSE);
	m_workEdit.Invalidate(FALSE);

	m_lastOutputScroll =
		getEditorScroll(m_outputEdit);

	m_lastWorkScroll =
		getEditorScroll(m_workEdit);

	m_compareUpdating = FALSE;

	CString status;

	status.Format(
		"Compare: %d added, %d removed",
		result.addedCount,
		result.removedCount);

	m_reloadStatus.SetWindowText(status);
}


void CObjectDeparserDialog::reloadBlockParsedCallback(
	const AsciiString& declaration,
	const AsciiString& blockType,
	const AsciiString& filename,
	UnsignedInt line,
	INILoadType loadType,
	void* userData)
{
	ReloadCaptureContext* context =
		static_cast<ReloadCaptureContext*>(userData);

	if (!context)
		return;

	ParsedDefinitionCatalog::capture(
		declaration,
		blockType,
		filename,
		line,
		loadType,
		context->catalog);

	context->dialog->pumpReloadMessages();
}


void CObjectDeparserDialog::pumpReloadMessages()
{
	if (!m_reloadInProgress ||
		m_pumpingReloadMessages)
	{
		return;
	}

	const DWORD now = ::GetTickCount();

	if (now - m_lastReloadPumpTick < 30)
		return;

	m_lastReloadPumpTick = now;
	m_pumpingReloadMessages = TRUE;

	MSG message = {};
	Int processed = 0;

	while (processed < 32 &&
		::PeekMessage(
			&message,
			nullptr,
			0,
			0,
			PM_REMOVE))
	{
		if (message.message == WM_QUIT)
		{
			::PostQuitMessage(
				static_cast<Int>(message.wParam));

			break;
		}

		if (!AfxGetApp()->PreTranslateMessage(&message))
		{
			::TranslateMessage(&message);
			::DispatchMessage(&message);
		}

		++processed;
	}

	m_pumpingReloadMessages = FALSE;
}
