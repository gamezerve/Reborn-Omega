///////////////////////////////////////////////////////////////////////////////////////
// FILE: ObjectDeparserDialog.cpp
// Author: Gamezerve, September 2026
// Description: 
///////////////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "TextDiff.h"
#include "ObjectDeparser.h"
#include "ObjectDeparserDialog.h"

#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/ThingTemplateDeparser.h"

BEGIN_MESSAGE_MAP(CObjectDeparserDialog, CDialog)
	ON_WM_SIZE()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_MOUSEMOVE()
	ON_WM_SETCURSOR()
	ON_WM_PAINT()
	ON_WM_DRAWITEM()
	ON_WM_MEASUREITEM()
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
	m_secondSplitterRatio(0.60)
{
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
		GetSysColor(COLOR_3DSHADOW));

	dc.FillSolidRect(
		secondSplitter,
		GetSysColor(COLOR_3DSHADOW));
}

void CObjectDeparserDialog::clearCompareHighlight()
{
	CHARFORMAT2 format = {};
	format.cbSize = sizeof(format);
	format.dwMask = CFM_BACKCOLOR;
	format.dwEffects = CFE_AUTOBACKCOLOR;

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
	long oldStart;
	long oldEnd;

	edit.GetSel(oldStart, oldEnd);

	CHARFORMAT2 format = {};
	format.cbSize = sizeof(format);
	format.dwMask = CFM_BACKCOLOR;
	format.crBackColor = color;

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

void CObjectDeparserDialog::OnCompare()
{
	CString leftText;
	CString rightText;

	m_outputEdit.GetWindowText(leftText);
	m_workEdit.GetWindowText(rightText);

	CStringA leftAnsi(leftText);
	CStringA rightAnsi(rightText);

	clearCompareHighlight();

	const TextDiffResult result =
		TextDiff::compare(
			leftAnsi.GetString(),
			rightAnsi.GetString());

	highlightLines(
		m_outputEdit,
		result.leftChangedLines,
		RGB(255, 210, 210));

	highlightLines(
		m_workEdit,
		result.rightChangedLines,
		RGB(210, 255, 210));

	CString status;

	status.Format(
		"Compare: %d added, %d removed",
		result.addedCount,
		result.removedCount);

	m_reloadStatus.SetWindowText(status);
}

void CObjectDeparserDialog::updateCompareButtonState()
{
	m_compareButton.EnableWindow(
		m_outputEdit.GetWindowTextLength() > 0 &&
		m_workEdit.GetWindowTextLength() > 0);
}

void CObjectDeparserDialog::OnWorkingCopyChanged()
{
	clearCompareHighlight();
	updateCompareButtonState();
}

void CObjectDeparserDialog::OnTransfer()
{
	CString text;
	m_outputEdit.GetWindowText(text);

	m_workEdit.SetWindowText(text);
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
		GetSysColor(
			selected
			? COLOR_HIGHLIGHT
			: COLOR_WINDOW);

	COLORREF textColor;

	if (!isDefinitionImplemented(definition))
	{
		textColor =
			selected
			? RGB(255, 190, 190)
			: RGB(200, 0, 0);
	}
	else
	{
		textColor =
			GetSysColor(
				selected
				? COLOR_HIGHLIGHTTEXT
				: COLOR_WINDOWTEXT);
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

	const CString& loadTime =
		ObjectDeparserApp()->getLastObjectIniLoadTime();

	CStringA loadTimeAnsi(loadTime);

	output += "; INI Load Completed: ";
	output += loadTimeAnsi.GetString();
	output += "\r\n";

	std::string sourceFilename =
		definition->filename.str();

	std::replace(
		sourceFilename.begin(),
		sourceFilename.end(),
		'\\',
		'/');

	output += "; Source: ";
	output += sourceFilename;
	output += "\r\n\r\n";

	if (isDefinitionImplemented(definition))
	{
		const ThingTemplate* thing =
			TheThingFactory->findTemplate(
				definition->name,
				FALSE);

		if (thing)
		{
			output += ThingTemplateDeparser::deparse(thing);
		}
		else
		{
			output += definition->declaration.str();
			output += "\r\n\r\n";
			output += "; ThingTemplate was not found.\r\n";
		}
	}
	else
	{
		output += definition->declaration.str();
		output += "\r\n\r\n";
		output += "; Deparser for this definition type is not implemented yet.\r\n";
	}

	m_outputEdit.SetWindowText(output.c_str());

	m_transferButton.EnableWindow(TRUE);

	updateCompareButtonState();
}

void CObjectDeparserDialog::OnOK()
{
}

void CObjectDeparserDialog::OnCancel()
{
	const int result = MessageBox(
		"Are you sure you want to exit?",
		"Reborn Omega INI Deparser",
		MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);

	if (result == IDYES)
		CDialog::OnCancel();
}

void CObjectDeparserDialog::OnReloadINI()
{
	CString selectedName;

	const int selectedIndex = m_resultsList.GetCurSel();

	if (selectedIndex != LB_ERR)
		m_resultsList.GetText(selectedIndex, selectedName);

	m_deparseButton.EnableWindow(FALSE);
	m_transferButton.EnableWindow(FALSE);
	m_reloadButton.EnableWindow(FALSE);

	m_outputEdit.SetWindowText("");
	m_objectCount.SetWindowText("Reloading...");

	updateReloadProgress(
		0,
		"Starting reload...");

	const Bool success =
		ObjectDeparserApp()->reloadObjectDatabase(
			&CObjectDeparserDialog::reloadProgressCallback,
			this);

	buildDefinitionList();
	refreshDefinitionList();

	m_reloadButton.EnableWindow(TRUE);

	if (!success)
	{
		MessageBox(
			"Failed to reload Object INI files.",
			"Object Deparser",
			MB_OK | MB_ICONERROR);

		return;
	}

	if (!selectedName.IsEmpty())
		selectDefinitionByDeclaration(selectedName);
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
