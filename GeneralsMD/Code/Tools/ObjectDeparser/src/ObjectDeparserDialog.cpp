///////////////////////////////////////////////////////////////////////////////////////
// FILE: ObjectDeparserDialog.cpp
// Author: Gamezerve, September 2026
// Description: 
///////////////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "ObjectDeparser.h"
#include "ObjectDeparserDialog.h"

#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/ThingTemplateDeparser.h"

BEGIN_MESSAGE_MAP(CObjectDeparserDialog, CDialog)
	ON_WM_SIZE()
	ON_EN_CHANGE(IDC_SEARCH_EDIT, OnSearchChanged)
	ON_LBN_SELCHANGE(IDC_RESULTS_LIST, OnSelectionChanged)
	ON_LBN_DBLCLK(IDC_RESULTS_LIST, OnResultDoubleClicked)
	ON_BN_CLICKED(IDC_DEPARSE_NOW, OnDeparseNow)
	ON_BN_CLICKED(IDC_TRANSFER, OnTransfer)
	ON_BN_CLICKED(IDC_RELOAD_INI, OnReloadINI)
END_MESSAGE_MAP()

CObjectDeparserDialog::CObjectDeparserDialog(CWnd* parent)
	: CDialog(IDD_OBJECT_DEPARSER_DIALOG, parent)
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
}

void CObjectDeparserDialog::buildTemplateList()
{
	m_templates.clear();

	if (!TheThingFactory)
		return;

	for (const ThingTemplate* thing = TheThingFactory->firstTemplate();
		thing != nullptr;
		thing = thing->friend_getNextTemplate())
	{
		m_templates.push_back(thing);
	}

	std::sort(
		m_templates.begin(),
		m_templates.end(),
		[](const ThingTemplate* a, const ThingTemplate* b)
		{
			return _stricmp(
				a->getName().str(),
				b->getName().str()) < 0;
		});
}

void CObjectDeparserDialog::refreshTemplateList()
{
	CString filter;
	m_searchEdit.GetWindowText(filter);
	filter.MakeLower();

	m_resultsList.SetRedraw(FALSE);
	m_resultsList.ResetContent();

	for (const ThingTemplate* thing : m_templates)
	{
		CString name(thing->getName().str());
		CString lowerName(name);

		lowerName.MakeLower();

		if (!filter.IsEmpty() && lowerName.Find(filter) == -1)
			continue;

		const int index = m_resultsList.AddString(name);

		m_resultsList.SetItemDataPtr(
			index,
			const_cast<ThingTemplate*>(thing));
	}

	CString countText;
	countText.Format(
		"%d objects",
		m_resultsList.GetCount());

	m_objectCount.SetWindowText(countText);

	m_resultsList.SetRedraw(TRUE);
	m_resultsList.Invalidate();

	m_deparseButton.EnableWindow(FALSE);
}

BOOL CObjectDeparserDialog::OnInitDialog()
{
	CDialog::OnInitDialog();

	SetWindowText("Reborn Omega Object Deparser");

	m_outputFont.CreatePointFont(95, "Consolas");

	m_outputEdit.SetFont(&m_outputFont);
	m_outputEdit.SetLimitText(0x7fffffff);

	m_workEdit.SetFont(&m_outputFont);
	m_workEdit.SetLimitText(0x7fffffff);

	m_deparseButton.EnableWindow(FALSE);
	m_transferButton.EnableWindow(FALSE);

	m_reloadProgress.SetRange(0, 100);
	m_reloadProgress.SetPos(100);

	m_reloadStatus.SetWindowText(
		"Initial load complete.");

	buildTemplateList();
	refreshTemplateList();

	m_deparseButton.EnableWindow(FALSE);

	layoutControls();

	return TRUE;
}

void CObjectDeparserDialog::OnSize(UINT nType, int cx, int cy)
{
	CDialog::OnSize(nType, cx, cy);

	if (GetSafeHwnd())
		layoutControls();
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

	const int buttonsWidth =
		deparseWidth +
		transferWidth +
		reloadWidth +
		gap * 3;

	const int searchLeft =
		margin + searchLabelWidth;

	const int searchWidth =
		max(
			100,
			client.Width() -
			searchLeft -
			margin -
			buttonsWidth);

	m_searchEdit.MoveWindow(
		searchLeft,
		searchTop,
		searchWidth,
		searchHeight);

	int buttonX =
		searchLeft + searchWidth + gap;

	m_deparseButton.MoveWindow(
		buttonX,
		searchTop,
		deparseWidth,
		searchHeight);

	buttonX += deparseWidth + gap;

	m_transferButton.MoveWindow(
		buttonX,
		searchTop,
		transferWidth,
		searchHeight);

	buttonX += transferWidth + gap;

	m_reloadButton.MoveWindow(
		buttonX,
		searchTop,
		reloadWidth,
		searchHeight);

	const int panelLabelTop =
		searchTop + searchHeight + gap;

	const int panelTop =
		panelLabelTop + labelHeight;

	const int panelBottom =
		client.Height() - margin - statusHeight;

	const int panelHeight =
		max(50, panelBottom - panelTop);

	const int leftWidth =
		max(180, client.Width() / 5);

	const int availableWidth =
		client.Width() -
		margin * 2 -
		gap * 2 -
		leftWidth;

	const int middleWidth =
		availableWidth / 2;

	const int rightWidth =
		availableWidth - middleWidth;

	const int middleX =
		margin + leftWidth + gap;

	const int rightX =
		middleX + middleWidth + gap;

	if (objectsLabel)
		objectsLabel->MoveWindow(
			margin,
			panelLabelTop,
			leftWidth,
			labelHeight);

	if (outputLabel)
		outputLabel->MoveWindow(
			middleX,
			panelLabelTop,
			middleWidth,
			labelHeight);

	if (workLabel)
		workLabel->MoveWindow(
			rightX,
			panelLabelTop,
			rightWidth,
			labelHeight);

	m_resultsList.MoveWindow(
		margin,
		panelTop,
		leftWidth,
		panelHeight);

	m_outputEdit.MoveWindow(
		middleX,
		panelTop,
		middleWidth,
		panelHeight);

	m_workEdit.MoveWindow(
		rightX,
		panelTop,
		rightWidth,
		panelHeight);

	m_objectCount.MoveWindow(
		margin,
		panelBottom + 4,
		leftWidth,
		statusHeight);

	const int footerY =
		panelBottom + 3;

	const int reloadStatusWidth = 180;

	m_reloadProgress.MoveWindow(
		middleX,
		footerY,
		middleWidth + gap + rightWidth -
		reloadStatusWidth - gap,
		16);

	m_reloadStatus.MoveWindow(
		client.Width() -
		margin -
		reloadStatusWidth,
		footerY,
		reloadStatusWidth,
		16);
}

void CObjectDeparserDialog::OnTransfer()
{
	CString text;
	m_outputEdit.GetWindowText(text);

	m_workEdit.SetWindowText(text);
	m_workEdit.SetFocus();
}

void CObjectDeparserDialog::OnSearchChanged()
{
	refreshTemplateList();
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

void CObjectDeparserDialog::OnDeparseNow()
{
	const int index = m_resultsList.GetCurSel();

	if (index == LB_ERR)
		return;

	const ThingTemplate* thing =
		static_cast<const ThingTemplate*>(
			m_resultsList.GetItemDataPtr(index));

	if (!thing)
		return;

	std::string output;

	const CString& loadTime =
		ObjectDeparserApp()->getLastObjectIniLoadTime();

	CStringA loadTimeAnsi(loadTime);

	output += "; Object INI Load Completed: ";
	output += loadTimeAnsi.GetString();
	output += "\r\n";

	output +=
		ThingTemplateDeparser::deparse(thing);

	m_outputEdit.SetWindowText(
		output.c_str());

	m_transferButton.EnableWindow(TRUE);

	m_transferButton.EnableWindow(TRUE);
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

	buildTemplateList();
	refreshTemplateList();

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
		selectTemplateByName(selectedName);
}

void CObjectDeparserDialog::selectTemplateByName(const CString& name)
{
	for (int i = 0; i < m_resultsList.GetCount(); ++i)
	{
		CString currentName;
		m_resultsList.GetText(i, currentName);

		if (currentName.CompareNoCase(name) != 0)
			continue;

		m_resultsList.SetCurSel(i);

		OnSelectionChanged();
		OnDeparseNow();

		return;
	}
}
