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
#include "GameClient/GameText.h"

#include <tom.h>
#include <cctype>

class ScopedRichEditUndoSuspend
{
public:
	explicit ScopedRichEditUndoSuspend(CRichEditCtrl& edit)
		: m_document(nullptr),
		m_frozen(FALSE)
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
		else
		{
			// Reborn: Batch formatting without recalculating RichEdit layout for every selected range.
			LONG freezeCount = 0;
			m_frozen = SUCCEEDED(m_document->Freeze(&freezeCount));
		}
	}

	~ScopedRichEditUndoSuspend()
	{
		if (m_document)
		{
			if (m_frozen)
			{
				LONG freezeCount = 0;
				m_document->Unfreeze(&freezeCount);
			}
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
	Bool m_frozen;
};

//-------------------------------------------------------------------------------------------------
/** Reborn: Temporarily permit programmatic paragraph formatting on a read-only RichEdit control. */
//-------------------------------------------------------------------------------------------------
class ScopedRichEditWritable
{
public:
	explicit ScopedRichEditWritable(CRichEditCtrl& edit)
		: m_edit(edit),
		m_restoreReadOnly((edit.GetStyle() & ES_READONLY) != 0)
	{
		if (m_restoreReadOnly)
			m_edit.SendMessage(EM_SETREADONLY, FALSE);
	}

	~ScopedRichEditWritable()
	{
		if (m_restoreReadOnly && ::IsWindow(m_edit.GetSafeHwnd()))
			m_edit.SendMessage(EM_SETREADONLY, TRUE);
	}

	ScopedRichEditWritable(const ScopedRichEditWritable&) = delete;
	ScopedRichEditWritable& operator=(const ScopedRichEditWritable&) = delete;

private:
	CRichEditCtrl& m_edit;
	Bool m_restoreReadOnly;
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
	// Reborn: Keep editor searching separate from the definition-list search control.
	ON_EN_CHANGE(IDC_EDITOR_FIND_EDIT, OnEditorFindChanged)
	ON_BN_CLICKED(IDC_EDITOR_FIND_PREVIOUS, OnEditorFindPrevious)
	ON_BN_CLICKED(IDC_EDITOR_FIND_NEXT, OnEditorFindNext)
	ON_BN_CLICKED(IDC_EDITOR_FIND_CLOSE, OnEditorFindClose)
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
	m_editorFindVisible(FALSE),
	m_editorFindTarget(nullptr),
	m_occurrenceHighlightEdit(nullptr),
	m_lastOutputScroll(0, 0),
	m_lastWorkScroll(0, 0),
	m_lastOutputGutterScroll(-1, -1),
	m_lastWorkGutterScroll(-1, -1),
	m_lastComparedOutputLineCount(-1),
	m_lastComparedWorkLineCount(-1),
	m_reloadInProgress(FALSE),
	m_pumpingReloadMessages(FALSE),
	m_updatingDefinitionLinks(FALSE),
	m_updatingOccurrenceHighlights(FALSE),
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
	DDX_Control(pDX, IDC_EDITOR_FIND_EDIT, m_editorFindEdit);
	DDX_Control(pDX, IDC_EDITOR_FIND_PREVIOUS, m_editorFindPrevious);
	DDX_Control(pDX, IDC_EDITOR_FIND_NEXT, m_editorFindNext);
	DDX_Control(pDX, IDC_EDITOR_FIND_CLOSE, m_editorFindClose);
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

	// Reborn: Create lightweight owner-drawn gutters without adding editable resource controls.
	const CRect initialGutterRect(0, 0, 1, 1);
	m_outputGutter.Create("", WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
		initialGutterRect, this, IDC_OUTPUT_GUTTER);
	m_workGutter.Create("", WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
		initialGutterRect, this, IDC_WORK_GUTTER);
	m_outputGutter.SetFont(&m_outputFont);
	m_workGutter.SetFont(&m_outputFont);
	SetTimer(TIMER_GUTTER_REFRESH, 50, nullptr);

	// Reborn: The editor find bar is revealed only by Ctrl+F in one of the two text panes.
	m_editorFindEdit.ShowWindow(SW_HIDE);
	m_editorFindPrevious.ShowWindow(SW_HIDE);
	m_editorFindNext.ShowWindow(SW_HIDE);
	m_editorFindClose.ShowWindow(SW_HIDE);
	m_editorFindEdit.SendMessage(EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"Find in editor"));

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

	CRect outputEditRect(middlePane);
	CRect workEditRect(rightPane);

	if (m_editorFindVisible && m_editorFindTarget)
	{
		// Reborn: Place the shared find bar above only the editor that invoked Ctrl+F.
		const int findBarHeight = 24;
		const int findControlGap = 4;
		const int previousWidth = 72;
		const int nextWidth = 50;
		const int closeWidth = 28;
		CRect findPane = m_editorFindTarget == &m_outputEdit ? middlePane : rightPane;

		const int closeLeft = findPane.right - closeWidth;
		const int nextLeft = closeLeft - findControlGap - nextWidth;
		const int previousLeft = nextLeft - findControlGap - previousWidth;
		// Reborn: Normalize MFC LONG coordinates before using the engine's strongly typed max helper.
		const int findEditWidth = max(40, previousLeft - findControlGap - static_cast<int>(findPane.left));

		m_editorFindEdit.MoveWindow(findPane.left, findPane.top, findEditWidth, findBarHeight);
		m_editorFindPrevious.MoveWindow(previousLeft, findPane.top, previousWidth, findBarHeight);
		m_editorFindNext.MoveWindow(nextLeft, findPane.top, nextWidth, findBarHeight);
		m_editorFindClose.MoveWindow(closeLeft, findPane.top, closeWidth, findBarHeight);

		m_editorFindEdit.ShowWindow(SW_SHOW);
		m_editorFindPrevious.ShowWindow(SW_SHOW);
		m_editorFindNext.ShowWindow(SW_SHOW);
		m_editorFindClose.ShowWindow(SW_SHOW);

		// Reborn: Reserve the same vertical strip above both editors so Compare rows remain aligned.
		outputEditRect.top += findBarHeight + findControlGap;
		workEditRect.top += findBarHeight + findControlGap;
	}
	else
	{
		m_editorFindEdit.ShowWindow(SW_HIDE);
		m_editorFindPrevious.ShowWindow(SW_HIDE);
		m_editorFindNext.ShowWindow(SW_HIDE);
		m_editorFindClose.ShowWindow(SW_HIDE);
	}


	// Reborn: Reserve a fixed marker and line-number column without reducing scrollable text coordinates.
	const int gutterWidth = 58;
	m_outputGutter.MoveWindow(
		outputEditRect.left,
		outputEditRect.top,
		gutterWidth,
		outputEditRect.Height());
	m_workGutter.MoveWindow(
		workEditRect.left,
		workEditRect.top,
		gutterWidth,
		workEditRect.Height());
	outputEditRect.left += gutterWidth;
	workEditRect.left += gutterWidth;

	m_outputEdit.MoveWindow(outputEditRect);
	m_workEdit.MoveWindow(workEditRect);

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
	// Reborn: Discard transient token ranges before compare/deparse formatting replaces their positions.
	clearTokenOccurrenceHighlights();

	// Reborn: Read-only output needs temporary write access for reliable paragraph-gap removal.
	ScopedRichEditWritable outputWritable(m_outputEdit);
	ScopedRichEditUndoSuspend outputUndo(m_outputEdit);
	ScopedRichEditUndoSuspend workUndo(m_workEdit);

	if (!outputUndo.isActive() || !workUndo.isActive())
		return;

	CHARFORMAT2 format = {};
	format.cbSize = sizeof(format);
	format.dwMask = CFM_BACKCOLOR | CFM_COLOR;
	format.dwEffects = CFE_AUTOBACKCOLOR;
	format.crTextColor = m_textColor;

	long outputStart;
	long outputEnd;
	long workStart;
	long workEnd;

	m_outputEdit.GetSel(outputStart, outputEnd);
	m_outputEdit.SetSel(0, -1);
	m_outputEdit.SetSelectionCharFormat(format);
	m_outputEdit.SetSel(outputStart, outputEnd);

	m_workEdit.GetSel(workStart, workEnd);
	m_workEdit.SetSel(0, -1);
	m_workEdit.SetSelectionCharFormat(format);
	m_workEdit.SetSel(workStart, workEnd);

	// Reborn: Remove visual diff padding while leaving both editor buffers and undo histories untouched.
	PARAFORMAT2 paragraphFormat = {};
	paragraphFormat.cbSize = sizeof(paragraphFormat);
	paragraphFormat.dwMask = PFM_SPACEBEFORE | PFM_SPACEAFTER;
	paragraphFormat.dySpaceBefore = 0;
	paragraphFormat.dySpaceAfter = 0;

	m_outputEdit.SetSel(0, -1);
	m_outputEdit.SendMessage(EM_SETPARAFORMAT, 0, reinterpret_cast<LPARAM>(&paragraphFormat));
	m_outputEdit.SetSel(outputStart, outputEnd);

	m_workEdit.SetSel(0, -1);
	m_workEdit.SendMessage(EM_SETPARAFORMAT, 0, reinterpret_cast<LPARAM>(&paragraphFormat));
	m_workEdit.SetSel(workStart, workEnd);
}



void CObjectDeparserDialog::highlightLines(
	CRichEditCtrl& edit,
	const std::vector<Int>& lines,
	COLORREF color)
{
	if (lines.empty())
		return;

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

	const Int lineCount = edit.GetLineCount();
	std::vector<Int> sortedLines = lines;
	std::sort(sortedLines.begin(), sortedLines.end());
	sortedLines.erase(
		std::unique(sortedLines.begin(), sortedLines.end()),
		sortedLines.end());

	for (size_t index = 0; index < sortedLines.size();)
	{
		const Int firstLine = sortedLines[index];
		if (firstLine < 0 || firstLine >= lineCount)
		{
			++index;
			continue;
		}

		Int lastLine = firstLine;
		while (index + 1 < sortedLines.size() &&
			sortedLines[index + 1] == lastLine + 1 &&
			sortedLines[index + 1] < lineCount)
		{
			++index;
			lastLine = sortedLines[index];
		}

		const long start = edit.LineIndex(firstLine);
		const long lastStart = edit.LineIndex(lastLine);

		if (start < 0 || lastStart < 0)
		{
			++index;
			continue;
		}

		// Reborn: Format one contiguous marker run and stop before its final CR/LF to prevent color leakage.
		const long end = lastStart + max(0, static_cast<Int>(edit.LineLength(lastStart)));

		edit.SetSel(start, end);
		edit.SetSelectionCharFormat(format);
		++index;
	}

	edit.SetSel(oldStart, oldEnd);
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Add Notepad++-style visual alignment gaps without inserting synthetic blank lines. */
//-------------------------------------------------------------------------------------------------
void CObjectDeparserDialog::applyCompareLineGaps(
	CRichEditCtrl& edit,
	const std::vector<TextDiffLineGap>& gaps)
{
	if (gaps.empty())
		return;

	// Reborn: Apply the same paragraph-spacing path to read-only Deparsed and editable Working panes.
	ScopedRichEditWritable writable(edit);
	ScopedRichEditUndoSuspend undo(edit);
	if (!undo.isActive())
		return;

	long oldStart;
	long oldEnd;
	edit.GetSel(oldStart, oldEnd);

	CClientDC dc(&edit);
	CFont* oldFont = nullptr;
	if (edit.GetFont())
		oldFont = dc.SelectObject(edit.GetFont());

	TEXTMETRIC metrics = {};
	dc.GetTextMetrics(&metrics);
	const Int measuredDpiY = dc.GetDeviceCaps(LOGPIXELSY);
	Int lineHeightPixels = metrics.tmHeight + metrics.tmExternalLeading;
	const Int dpiY = measuredDpiY > 0 ? measuredDpiY : 1;

	if (oldFont)
		dc.SelectObject(oldFont);

	const Int lineCount = edit.GetLineCount();
	if (lineCount > 1)
	{
		// Reborn: Measure RichEdit's actual rendered baseline advance to prevent cumulative gap drift.
		const long firstIndex = edit.LineIndex(0);
		const long secondIndex = edit.LineIndex(1);
		if (firstIndex >= 0 && secondIndex >= 0)
		{
			const CPoint firstPosition = edit.PosFromChar(firstIndex);
			const CPoint secondPosition = edit.PosFromChar(secondIndex);
			const Int renderedAdvance = secondPosition.y - firstPosition.y;
			if (renderedAdvance > 0)
				lineHeightPixels = renderedAdvance;
		}
	}
	lineHeightPixels = max(1, lineHeightPixels);

	std::vector<Int> boundaryGapCounts(static_cast<size_t>(lineCount + 1), 0);
	for (const TextDiffLineGap& gap : gaps)
	{
		if (gap.count <= 0)
			continue;
		const Int boundary = gap.afterLine ? gap.line + 1 : gap.line;
		if (boundary >= 0 && boundary <= lineCount)
			boundaryGapCounts[boundary] += gap.count;
	}

	for (Int boundary = 0; boundary <= lineCount; ++boundary)
	{
		const Int gapCount = boundaryGapCounts[boundary];
		if (gapCount <= 0)
			continue;

		const LONG totalSpacing = MulDiv(lineHeightPixels * gapCount, 1440, dpiY);
		const LONG safeParagraphSpacing = 31500;
		LONG beforeSpacing = boundary < lineCount
			? min(totalSpacing, safeParagraphSpacing)
			: 0;
		LONG afterSpacing = boundary > 0
			? totalSpacing - beforeSpacing
			: 0;
		if (afterSpacing > safeParagraphSpacing && boundary < lineCount)
		{
			afterSpacing = safeParagraphSpacing;
			beforeSpacing = totalSpacing - afterSpacing;
		}

		if (afterSpacing > 0 && boundary > 0)
		{
			// Reborn: Put overflow on the preceding paragraph at the same visual boundary.
			const long previousStart = edit.LineIndex(boundary - 1);
			if (previousStart >= 0)
			{
				PARAFORMAT2 afterFormat = {};
				afterFormat.cbSize = sizeof(afterFormat);
				afterFormat.dwMask = PFM_SPACEAFTER;
				afterFormat.dySpaceAfter = afterSpacing;
				edit.SetSel(previousStart, previousStart);
				edit.SendMessage(EM_SETPARAFORMAT, 0,
					reinterpret_cast<LPARAM>(&afterFormat));
			}
		}

		if (beforeSpacing > 0 && boundary < lineCount)
		{
			// Reborn: Keep the remaining height before the next real line so the gap stays contiguous.
			const long nextStart = edit.LineIndex(boundary);
			if (nextStart >= 0)
			{
				PARAFORMAT2 beforeFormat = {};
				beforeFormat.cbSize = sizeof(beforeFormat);
				beforeFormat.dwMask = PFM_SPACEBEFORE;
				beforeFormat.dySpaceBefore = beforeSpacing;
				edit.SetSel(nextStart, nextStart);
				edit.SendMessage(EM_SETPARAFORMAT, 0,
					reinterpret_cast<LPARAM>(&beforeFormat));
			}
		}
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
	// Reborn: Compare exclusively owns editor presentation until the user stops it.
	KillTimer(TIMER_DEFINITION_LINKS);

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
	m_outputGutterMarkers.clear();
	m_workGutterMarkers.clear();
	m_outputGutter.Invalidate(FALSE);
	m_workGutter.Invalidate(FALSE);

	m_compareUpdating = FALSE;
	// Reborn: Rebuild links once after compare releases all background and paragraph formatting.
	updateDefinitionLinks(m_outputEdit);
	updateDefinitionLinks(m_workEdit);

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
	if (m_compareUpdating || m_updatingDefinitionLinks || m_updatingOccurrenceHighlights)
		return;

	// Reborn: Defer link formatting while Compare exclusively owns editor presentation.
	KillTimer(TIMER_DEFINITION_LINKS);
	if (!m_compareMode)
		SetTimer(TIMER_DEFINITION_LINKS, 180, nullptr);

	restartCompareDebounce();
	updateCompareButtonState();
	m_workGutter.Invalidate(FALSE);
}

void CObjectDeparserDialog::OnOutputChanged()
{
	if (m_compareUpdating || m_updatingDefinitionLinks || m_updatingOccurrenceHighlights)
		return;

	// Reborn: Defer link formatting while Compare exclusively owns editor presentation.
	KillTimer(TIMER_DEFINITION_LINKS);
	if (!m_compareMode)
		SetTimer(TIMER_DEFINITION_LINKS, 180, nullptr);

	restartCompareDebounce();
	updateCompareButtonState();
	m_outputGutter.Invalidate(FALSE);
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

		// Reborn: Never let delayed link formatting mutate an active compare presentation.
		if (!m_reloadInProgress && !m_compareMode)
		{
			updateDefinitionLinks(m_outputEdit);
			updateDefinitionLinks(m_workEdit);
		}

		return;
	}

	if (nIDEvent == TIMER_GUTTER_REFRESH)
	{
		refreshCompareGutters();
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

//-------------------------------------------------------------------------------------------------
/** Reborn: Paint physical line numbers at RichEdit-rendered positions with compact diff symbols. */
//-------------------------------------------------------------------------------------------------
void CObjectDeparserDialog::drawCompareGutter(
	LPDRAWITEMSTRUCT drawItem,
	CRichEditCtrl& edit,
	const std::unordered_map<Int, Int>& markers)
{
	CDC dc;
	dc.Attach(drawItem->hDC);
	const CRect gutterRect(drawItem->rcItem);
	dc.FillSolidRect(&gutterRect, m_backgroundColor);
	dc.SetBkMode(TRANSPARENT);

	CFont* oldFont = dc.SelectObject(&m_outputFont);
	TEXTMETRIC metrics = {};
	dc.GetTextMetrics(&metrics);
	const Int lineHeight = max(1,
		static_cast<Int>(metrics.tmHeight + metrics.tmExternalLeading));
	const Int firstVisible = max(0, static_cast<Int>(edit.SendMessage(EM_GETFIRSTVISIBLELINE)) - 1);
	const Int lineCount = edit.GetLineCount();

	for (Int line = firstVisible; line < lineCount; ++line)
	{
		const long character = edit.LineIndex(line);
		if (character < 0)
			continue;

		const CPoint position = edit.PosFromChar(character);
		if (position.y > drawItem->rcItem.bottom)
			break;
		if (position.y + lineHeight < drawItem->rcItem.top)
			continue;

		CRect numberRect(17, position.y,
			drawItem->rcItem.right - 3, position.y + lineHeight);
		CString number;
		number.Format("%d", line + 1);
		dc.SetTextColor(m_secondaryTextColor);
		dc.DrawText(number, &numberRect,
			DT_SINGLELINE | DT_RIGHT | DT_VCENTER | DT_NOPREFIX);

		const auto marker = markers.find(line);
		if (marker == markers.end())
			continue;

		COLORREF markerColor = m_secondaryTextColor;
		CString symbol;
		switch (marker->second)
		{
		case 1:
			markerColor = RGB(220, 80, 80);
			symbol = "-";
			break;
		case 2:
			markerColor = RGB(80, 180, 100);
			symbol = "+";
			break;
		case 3:
			markerColor = RGB(220, 175, 70);
			symbol = "~";
			break;
		case 4:
			markerColor = RGB(70, 140, 220);
			symbol = ">";
			break;
		}

		CRect markerRect(1, position.y, 15, position.y + lineHeight);
		dc.FillSolidRect(&markerRect, markerColor);
		dc.SetTextColor(RGB(15, 15, 15));
		dc.DrawText(symbol, &markerRect,
			DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX);
	}

	dc.FillSolidRect(gutterRect.right - 1, gutterRect.top,
		1, gutterRect.Height(), m_borderColor);
	if (oldFont)
		dc.SelectObject(oldFont);
	dc.Detach();
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Resolve result precedence into one visible operation marker per physical line. */
//-------------------------------------------------------------------------------------------------
void CObjectDeparserDialog::updateCompareGutterMarkers(const TextDiffResult& result)
{
	m_outputGutterMarkers.clear();
	m_workGutterMarkers.clear();

	for (Int line : result.leftRemovedLines)
		m_outputGutterMarkers[line] = 1;
	for (Int line : result.rightAddedLines)
		m_workGutterMarkers[line] = 2;
	for (Int line : result.leftModifiedLines)
		m_outputGutterMarkers[line] = 3;
	for (Int line : result.rightModifiedLines)
		m_workGutterMarkers[line] = 3;
	for (Int line : result.leftMovedLines)
		m_outputGutterMarkers[line] = 4;
	for (Int line : result.rightMovedLines)
		m_workGutterMarkers[line] = 4;

	m_outputGutter.Invalidate(FALSE);
	m_workGutter.Invalidate(FALSE);
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Keep owner-drawn line numbers attached to independently scrolling RichEdit controls. */
//-------------------------------------------------------------------------------------------------
void CObjectDeparserDialog::refreshCompareGutters()
{
	if (!m_outputEdit.GetSafeHwnd() || !m_workEdit.GetSafeHwnd())
		return;

	if (m_compareMode && !m_compareUpdating &&
		(m_outputEdit.GetLineCount() != m_lastComparedOutputLineCount ||
			m_workEdit.GetLineCount() != m_lastComparedWorkLineCount))
	{
		// Reborn: Apply new alignment rows immediately instead of waiting behind the heavier link scan.
		performCompare();
		return;
	}

	const CPoint outputScroll = getEditorScroll(m_outputEdit);
	const CPoint workScroll = getEditorScroll(m_workEdit);
	if (outputScroll != m_lastOutputGutterScroll)
	{
		m_lastOutputGutterScroll = outputScroll;
		m_outputGutter.Invalidate(FALSE);
	}
	if (workScroll != m_lastWorkGutterScroll)
	{
		m_lastWorkGutterScroll = workScroll;
		m_workGutter.Invalidate(FALSE);
	}
}

void CObjectDeparserDialog::OnDrawItem(
	int nIDCtl,
	LPDRAWITEMSTRUCT lpDrawItemStruct)
{
	if (nIDCtl == IDC_OUTPUT_GUTTER)
	{
		drawCompareGutter(lpDrawItemStruct, m_outputEdit, m_outputGutterMarkers);
		return;
	}
	if (nIDCtl == IDC_WORK_GUTTER)
	{
		drawCompareGutter(lpDrawItemStruct, m_workEdit, m_workGutterMarkers);
		return;
	}

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

// Reborn: Treat underscores as part of editor words while punctuation remains a double-click boundary.
static Bool isEditorWordCharacter(TCHAR character)
{
	return _istalnum(character) || character == '_';
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

//-------------------------------------------------------------------------------------------------
/** Reborn: Restore only the ranges touched by the previous token overlay. */
//-------------------------------------------------------------------------------------------------
void CObjectDeparserDialog::clearTokenOccurrenceHighlights()
{
	CRichEditCtrl* edit = m_occurrenceHighlightEdit;
	m_occurrenceHighlightEdit = nullptr;
	if (!edit || !::IsWindow(edit->GetSafeHwnd()))
	{
		m_occurrenceHighlightFormats.clear();
		return;
	}

	ScopedRichEditUndoSuspend undo(*edit);
	if (!undo.isActive())
		return;

	long oldStart = 0;
	long oldEnd = 0;
	edit->GetSel(oldStart, oldEnd);
	const CPoint oldScroll = getEditorScroll(*edit);

	m_updatingOccurrenceHighlights = TRUE;
	edit->SetRedraw(FALSE);
	for (const OccurrenceHighlightFormat& previous : m_occurrenceHighlightFormats)
	{
		CHARFORMAT2 format = {};
		format.cbSize = sizeof(format);
		format.dwMask = CFM_BACKCOLOR;
		format.dwEffects = previous.automaticBackground ? CFE_AUTOBACKCOLOR : 0;
		format.crBackColor = previous.backgroundColor;
		edit->SetSel(previous.range.cpMin, previous.range.cpMax);
		edit->SetSelectionCharFormat(format);
	}
	m_occurrenceHighlightFormats.clear();
	edit->SetSel(oldStart, oldEnd);
	edit->SendMessage(EM_SETSCROLLPOS, 0, reinterpret_cast<LPARAM>(&oldScroll));
	edit->SetRedraw(TRUE);
	edit->Invalidate(FALSE);
	m_updatingOccurrenceHighlights = FALSE;
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Highlight every boundary-safe occurrence and retain the originally double-clicked range. */
//-------------------------------------------------------------------------------------------------
void CObjectDeparserDialog::highlightTokenOccurrences(
	CRichEditCtrl& edit,
	const CString& token,
	long selectedStart,
	long selectedEnd)
{
	clearTokenOccurrenceHighlights();
	if (token.IsEmpty())
		return;

	CString text;
	getRichEditIndexText(edit, text);
	ScopedRichEditUndoSuspend undo(edit);
	if (!undo.isActive())
		return;

	CHARFORMAT2 format = {};
	format.cbSize = sizeof(format);
	format.dwMask = CFM_BACKCOLOR;
	format.crBackColor = RGB(105, 85, 25);

	const CPoint oldScroll = getEditorScroll(edit);
	m_updatingOccurrenceHighlights = TRUE;
	edit.SetRedraw(FALSE);
	Int searchStart = 0;
	while (searchStart < text.GetLength())
	{
		const Int occurrence = text.Find(token, searchStart);
		if (occurrence < 0)
			break;

		const Int occurrenceEnd = occurrence + token.GetLength();
		const Bool leftBoundary = occurrence == 0 || !isEditorWordCharacter(text.GetAt(occurrence - 1));
		const Bool rightBoundary = occurrenceEnd >= text.GetLength() ||
			!isEditorWordCharacter(text.GetAt(occurrenceEnd));

		if (leftBoundary && rightBoundary)
		{
			edit.SetSel(occurrence, occurrenceEnd);

			OccurrenceHighlightFormat previous = {};
			previous.range.cpMin = occurrence;
			previous.range.cpMax = occurrenceEnd;
			previous.automaticBackground = TRUE;

			if (m_compareMode)
			{
				// Reborn: Read prior colors only when Compare may have supplied a non-default background.
				CHARFORMAT2 previousFormat = {};
				previousFormat.cbSize = sizeof(previousFormat);
				edit.GetSelectionCharFormat(previousFormat);
				previous.backgroundColor = previousFormat.crBackColor;
				previous.automaticBackground =
					(previousFormat.dwEffects & CFE_AUTOBACKCOLOR) != 0;
			}
			m_occurrenceHighlightFormats.push_back(previous);

			edit.SetSelectionCharFormat(format);
		}

		searchStart = occurrence + 1;
	}

	m_occurrenceHighlightEdit = &edit;
	edit.SetSel(selectedStart, selectedEnd);
	// Reborn: Formatting selections must never navigate to the last matching token.
	edit.SendMessage(EM_SETSCROLLPOS, 0, reinterpret_cast<LPARAM>(&oldScroll));
	edit.SetRedraw(TRUE);
	edit.Invalidate(FALSE);
	m_updatingOccurrenceHighlights = FALSE;
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Expand RichEdit double-clicks across underscores and highlight identical whole tokens. */
//-------------------------------------------------------------------------------------------------
Bool CObjectDeparserDialog::selectEditorTokenAtPoint(
	CRichEditCtrl& edit,
	CPoint point)
{
	long character = edit.CharFromPos(point);

	CString text;
	getRichEditIndexText(edit, text);
	if (character < 0 || character >= text.GetLength() || !isEditorWordCharacter(text.GetAt(character)))
		return FALSE;

	long wordStart = character;
	long wordEnd = character + 1;
	while (wordStart > 0 && isEditorWordCharacter(text.GetAt(wordStart - 1)))
		--wordStart;
	while (wordEnd < text.GetLength() && isEditorWordCharacter(text.GetAt(wordEnd)))
		++wordEnd;

	const CString token = text.Mid(wordStart, wordEnd - wordStart);
	highlightTokenOccurrences(edit, token, wordStart, wordEnd);
	return TRUE;
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Show the shared find bar over the editor that received Ctrl+F. */
//-------------------------------------------------------------------------------------------------
void CObjectDeparserDialog::showEditorFindBar(CRichEditCtrl& edit)
{
	long selectionStart = 0;
	long selectionEnd = 0;
	edit.GetSel(selectionStart, selectionEnd);

	CString selectedText;
	if (selectionEnd > selectionStart)
	{
		CString indexText;
		getRichEditIndexText(edit, indexText);
		if (selectionStart >= 0 && selectionEnd <= indexText.GetLength())
			selectedText = indexText.Mid(selectionStart, selectionEnd - selectionStart);
	}

	m_editorFindTarget = &edit;
	m_editorFindVisible = TRUE;
	layoutControls();

	// Reborn: Seed Ctrl+F from the current single-line editor selection when one exists.
	if (!selectedText.IsEmpty() && selectedText.Find('\r') < 0 && selectedText.Find('\n') < 0)
		m_editorFindEdit.SetWindowText(selectedText);

	m_editorFindEdit.SetFocus();
	m_editorFindEdit.SetSel(0, -1);
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Close editor search and return keyboard focus to its originating text pane. */
//-------------------------------------------------------------------------------------------------
void CObjectDeparserDialog::hideEditorFindBar()
{
	CRichEditCtrl* target = m_editorFindTarget;
	m_editorFindVisible = FALSE;
	layoutControls();

	if (target && target->GetSafeHwnd())
		target->SetFocus();

	if (m_compareMode)
		performCompare();
	else
		m_reloadStatus.SetWindowText("Find closed.");
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Locate the next or previous case-insensitive match with wraparound in the active editor. */
//-------------------------------------------------------------------------------------------------
void CObjectDeparserDialog::findInActiveEditor(Bool backwards, Bool startFromSelection)
{
	if (!m_editorFindTarget || !m_editorFindTarget->GetSafeHwnd())
		return;

	CString needle;
	m_editorFindEdit.GetWindowText(needle);
	if (needle.IsEmpty())
		return;

	CString text;
	getRichEditIndexText(*m_editorFindTarget, text);
	CString lowerText(text);
	CString lowerNeedle(needle);
	lowerText.MakeLower();
	lowerNeedle.MakeLower();

	long selectionStart;
	long selectionEnd;
	m_editorFindTarget->GetSel(selectionStart, selectionEnd);
	Int match = -1;

	if (!backwards)
	{
		const Int start = startFromSelection ? static_cast<Int>(selectionEnd) : 0;
		match = lowerText.Find(lowerNeedle, start);
		if (match < 0 && startFromSelection)
			match = lowerText.Find(lowerNeedle, 0);
	}
	else
	{
		const Int lastPossible = max(0, lowerText.GetLength() - lowerNeedle.GetLength());
		const Int limit = startFromSelection ? static_cast<Int>(selectionStart) - 1 : lastPossible;
		Int scan = 0;

		while (scan <= limit)
		{
			const Int candidate = lowerText.Find(lowerNeedle, scan);
			if (candidate < 0 || candidate > limit)
				break;
			match = candidate;
			scan = candidate + 1;
		}

		if (match < 0 && startFromSelection)
		{
			scan = 0;
			while (scan <= lastPossible)
			{
				const Int candidate = lowerText.Find(lowerNeedle, scan);
				if (candidate < 0)
					break;
				match = candidate;
				scan = candidate + 1;
			}
		}
	}

	if (match < 0)
	{
		CString status;
		status.Format("Find: no match for '%s'.", needle.GetString());
		m_reloadStatus.SetWindowText(status);
		return;
	}

	m_editorFindTarget->SetSel(match, match + needle.GetLength());
	// Reborn: Reveal the match first, then center its rendered pixel position even across Compare gaps.
	m_editorFindTarget->SendMessage(EM_SCROLLCARET);
	CRect clientRect;
	m_editorFindTarget->GetClientRect(&clientRect);
	const CPoint matchPosition = m_editorFindTarget->PosFromChar(match);
	CPoint centeredScroll = getEditorScroll(*m_editorFindTarget);
	centeredScroll.y = max(
		static_cast<LONG>(0),
		centeredScroll.y + matchPosition.y - clientRect.Height() / 2);
	m_editorFindTarget->SendMessage(
		EM_SETSCROLLPOS,
		0,
		reinterpret_cast<LPARAM>(&centeredScroll));
	m_reloadStatus.SetWindowText(backwards ? "Find: previous match." : "Find: next match.");
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Update the active editor selection while the user refines the find text. */
//-------------------------------------------------------------------------------------------------
void CObjectDeparserDialog::OnEditorFindChanged()
{
	if (m_editorFindVisible)
		findInActiveEditor(FALSE, FALSE);
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Select the preceding editor match, wrapping at the beginning. */
//-------------------------------------------------------------------------------------------------
void CObjectDeparserDialog::OnEditorFindPrevious()
{
	findInActiveEditor(TRUE, TRUE);
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Select the following editor match, wrapping at the end. */
//-------------------------------------------------------------------------------------------------
void CObjectDeparserDialog::OnEditorFindNext()
{
	findInActiveEditor(FALSE, TRUE);
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Close the editor find bar from its dedicated button. */
//-------------------------------------------------------------------------------------------------
void CObjectDeparserDialog::OnEditorFindClose()
{
	hideEditorFindBar();
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Provide Ctrl+F, Enter, F3, Shift navigation and Escape for editor-local search. */
//-------------------------------------------------------------------------------------------------
BOOL CObjectDeparserDialog::PreTranslateMessage(MSG* message)
{
	if (message && message->message == WM_LBUTTONDBLCLK)
	{
		CRichEditCtrl* edit = nullptr;
		if (message->hwnd == m_outputEdit.GetSafeHwnd())
			edit = &m_outputEdit;
		else if (message->hwnd == m_workEdit.GetSafeHwnd())
			edit = &m_workEdit;

		if (edit)
		{
			// Reborn: Filter only the double-click itself; ordinary mouse movement stays on the fast path.
			const CPoint point(
				static_cast<short>(LOWORD(message->lParam)),
				static_cast<short>(HIWORD(message->lParam)));
			if (selectEditorTokenAtPoint(*edit, point))
				return TRUE;
		}
	}

	if (message && message->message == WM_KEYDOWN)
	{
		const Bool controlDown = (::GetKeyState(VK_CONTROL) & 0x8000) != 0;
		const Bool shiftDown = (::GetKeyState(VK_SHIFT) & 0x8000) != 0;
		const HWND focus = ::GetFocus();

		if (controlDown && message->wParam == 'F')
		{
			if (focus == m_outputEdit.GetSafeHwnd())
				showEditorFindBar(m_outputEdit);
			else if (focus == m_workEdit.GetSafeHwnd())
				showEditorFindBar(m_workEdit);
			else if (m_editorFindVisible && m_editorFindTarget)
				showEditorFindBar(*m_editorFindTarget);
			else
				return CDialog::PreTranslateMessage(message);
			return TRUE;
		}

		if (m_editorFindVisible && focus == m_editorFindEdit.GetSafeHwnd() && message->wParam == VK_RETURN)
		{
			findInActiveEditor(shiftDown, TRUE);
			return TRUE;
		}

		if (m_editorFindVisible && message->wParam == VK_F3)
		{
			findInActiveEditor(shiftDown, TRUE);
			return TRUE;
		}

		if (m_editorFindVisible && message->wParam == VK_ESCAPE)
		{
			hideEditorFindBar();
			return TRUE;
		}
	}

	return CDialog::PreTranslateMessage(message);
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

//-------------------------------------------------------------------------------------------------
/** Reborn: Format one existing Generals.str entry through the game's live localized text store. */
//-------------------------------------------------------------------------------------------------
Bool CObjectDeparserDialog::buildGameTextDefinition(
	const AsciiString& label,
	CString& output) const
{
	output.Empty();
	if (!TheGameText || label.isEmpty() || !strchr(label.str(), ':'))
		return FALSE;

	const std::string cacheKey(label.str());
	const auto cached = m_gameTextDefinitionCache.find(cacheKey);
	if (cached != m_gameTextDefinitionCache.end())
	{
		output = cached->second;
		return TRUE;
	}
	if (m_missingGameTextLabels.find(cacheKey) != m_missingGameTextLabels.end())
		return FALSE;

	Bool exists = FALSE;
	const UnicodeString localized = TheGameText->fetch(label, &exists);
	if (!exists)
	{
		// Reborn: Remember negative lookups because arbitrary colon tokens can repeat many times.
		m_missingGameTextLabels.insert(cacheKey);
		return FALSE;
	}

	const Int bufferLength = ::WideCharToMultiByte(
		CP_ACP,
		0,
		localized.str(),
		-1,
		nullptr,
		0,
		nullptr,
		nullptr);

	if (bufferLength <= 0)
		return FALSE;

	std::vector<Char> translated(static_cast<size_t>(bufferLength));
	::WideCharToMultiByte(
		CP_ACP,
		0,
		localized.str(),
		-1,
		translated.data(),
		bufferLength,
		nullptr,
		nullptr);

	// Reborn: Mirror the source Generals.str entry shape requested by the reference viewer.
	CStringA formatted;
	formatted.Format(
		"%s\r\n\"%s\"\r\nEND\r\n",
		label.str(),
		translated.data());
	output = CString(formatted);
	// Reborn: Store formatted viewer content as the existence/value cache for later scans and clicks.
	m_gameTextDefinitionCache.emplace(cacheKey, output);
	return TRUE;
}

// Reborn: Mark only references that resolve to exactly one current catalog name/type target.
void CObjectDeparserDialog::updateDefinitionLinks(
	CRichEditCtrl& edit)
{
	if (m_reloadInProgress ||
		m_compareMode ||
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

				Bool shouldLink = definition && definition != editorDefinition;
				if (!shouldLink && token.Find(':') > 0)
				{
					// Reborn: Link only labels that the live Generals.str subsystem confirms exist.
					CString gameTextDefinition;
					const AsciiString label(tokenAnsi.GetString());
					shouldLink = buildGameTextDefinition(label, gameTextDefinition);
				}

				if (shouldLink)
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

//-------------------------------------------------------------------------------------------------
/** Reborn: Revalidate a clicked string label against the current Generals.str store. */
//-------------------------------------------------------------------------------------------------
Bool CObjectDeparserDialog::resolveGameTextLink(
	CRichEditCtrl& edit,
	const CHARRANGE& range,
	AsciiString& label) const
{
	label.clear();
	if (range.cpMin < 0 || range.cpMax <= range.cpMin)
		return FALSE;

	CString text;
	getRichEditIndexText(edit, text);
	if (range.cpMax > text.GetLength())
		return FALSE;

	const CString token = text.Mid(range.cpMin, range.cpMax - range.cpMin);
	if (token.Find(':') <= 0)
		return FALSE;

	const CStringA tokenAnsi(token);
	label = tokenAnsi.GetString();
	CString output;
	if (!buildGameTextDefinition(label, output))
	{
		label.clear();
		return FALSE;
	}

	return TRUE;
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

	if (definition)
	{
		openDefinitionReference(*definition);
		return;
	}

	AsciiString gameTextLabel;
	if (resolveGameTextLink(*edit, link->chrg, gameTextLabel))
	{
		openGameTextReference(gameTextLabel);
		return;
	}

	if (!definition)
	{
		m_reloadStatus.SetWindowText(
			"Reference is ambiguous or no longer available.");
		return;
	}
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

//-------------------------------------------------------------------------------------------------
/** Reborn: Open a fresh modeless child containing one live Generals.str entry. */
//-------------------------------------------------------------------------------------------------
void CObjectDeparserDialog::openGameTextReference(
	const AsciiString& label)
{
	CString output;
	if (!buildGameTextDefinition(label, output))
		return;

	CDefinitionReferenceWindow* window =
		new CDefinitionReferenceWindow(
			this,
			"GameText",
			label);

	if (!window->createWindow(this))
		return;

	m_referenceWindows.push_back(window);
	window->setContent(output);
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

		if (window->getBlockType().compareNoCase("GameText") == 0)
		{
			// Reborn: String windows retain only their label and refetch after every reload boundary.
			CString output;
			if (buildGameTextDefinition(window->getDefinitionName(), output))
				window->setContent(output);
			else
				window->setContent("; This Generals.str entry is no longer available.\r\n");
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

	// Reborn: An immediate structural refresh supersedes any older delayed text refresh.
	KillTimer(TIMER_COMPARE_DEBOUNCE);
	KillTimer(TIMER_DEFINITION_LINKS);

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

	// Reborn: Publish operation markers from the same immutable result used for highlights and gaps.
	updateCompareGutterMarkers(result);

	m_compareUpdating = TRUE;

	m_outputEdit.SetRedraw(FALSE);
	m_workEdit.SetRedraw(FALSE);

	clearCompareHighlight();

	applyCompareLineGaps(
		m_outputEdit,
		result.leftGaps);

	applyCompareLineGaps(
		m_workEdit,
		result.rightGaps);

	highlightLines(
		m_outputEdit,
		result.leftRemovedLines,
		RGB(255, 195, 195));

	highlightLines(
		m_workEdit,
		result.rightAddedLines,
		RGB(195, 255, 195));

	highlightLines(
		m_outputEdit,
		result.leftModifiedLines,
		RGB(255, 238, 170));

	highlightLines(
		m_workEdit,
		result.rightModifiedLines,
		RGB(255, 238, 170));

	highlightLines(
		m_outputEdit,
		result.leftMovedLines,
		RGB(190, 220, 255));

	highlightLines(
		m_workEdit,
		result.rightMovedLines,
		RGB(190, 220, 255));

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
	m_outputGutter.Invalidate(FALSE);
	m_workGutter.Invalidate(FALSE);

	m_lastOutputScroll =
		getEditorScroll(m_outputEdit);

	m_lastWorkScroll =
		getEditorScroll(m_workEdit);

	m_lastComparedOutputLineCount = m_outputEdit.GetLineCount();
	m_lastComparedWorkLineCount = m_workEdit.GetLineCount();

	m_compareUpdating = FALSE;
	// Reborn: Discard formatting notifications queued by this completed compare transaction.
	KillTimer(TIMER_COMPARE_DEBOUNCE);

	CString status;

	status.Format(
		"Compare: %d added, %d removed, %d changed, %d moved",
		result.addedCount,
		result.removedCount,
		result.modifiedCount,
		result.movedCount);

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
