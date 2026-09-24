///////////////////////////////////////////////////////////////////////////////////////
// FILE: ObjectDeparserDialog.cpp
// Author: Gamezerve, September 2026
// Description: 
///////////////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "ObjectDeparserDialog.h"

BEGIN_MESSAGE_MAP(CObjectDeparserDialog, CDialog)
	ON_WM_SIZE()
	ON_EN_CHANGE(IDC_SEARCH_EDIT, OnSearchChanged)
	ON_LBN_SELCHANGE(IDC_RESULTS_LIST, OnSelectionChanged)
	ON_LBN_DBLCLK(IDC_RESULTS_LIST, OnResultDoubleClicked)
	ON_BN_CLICKED(IDC_DEPARSE_NOW, OnDeparseNow)
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
	DDX_Control(pDX, IDC_OUTPUT_EDIT, m_outputEdit);
	DDX_Control(pDX, IDC_OBJECT_COUNT, m_objectCount);
}

BOOL CObjectDeparserDialog::OnInitDialog()
{
	CDialog::OnInitDialog();

	SetWindowText("Reborn Omega Object Deparser");

	m_outputFont.CreatePointFont(95, "Consolas");
	m_outputEdit.SetFont(&m_outputFont);
	m_outputEdit.SetLimitText(0x7fffffff);

	m_outputEdit.SetWindowText(
		"Object database is not loaded yet.\r\n"
		"\r\n"
		"The UI is ready.");

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

void CObjectDeparserDialog::layoutControls()
{
	if (!m_searchEdit.GetSafeHwnd())
		return;

	CRect client;
	GetClientRect(&client);

	const int margin = 10;
	const int labelHeight = 18;
	const int searchHeight = 24;
	const int buttonWidth = 110;
	const int gap = 10;
	const int leftWidth = max(220, client.Width() / 4);
	const int statusHeight = 20;

	const int searchTop = margin;
	const int searchLabelWidth = 50;

	CWnd* searchLabel = GetDlgItem(IDC_STATIC_SEARCH);
	CWnd* objectsLabel = GetDlgItem(IDC_STATIC_OBJECTS);
	CWnd* outputLabel = GetDlgItem(IDC_STATIC_OUTPUT);

	if (searchLabel)
		searchLabel->MoveWindow(
			margin,
			searchTop + 4,
			searchLabelWidth,
			labelHeight);

	m_searchEdit.MoveWindow(
		margin + searchLabelWidth,
		searchTop,
		client.Width() - margin * 3 - searchLabelWidth - buttonWidth,
		searchHeight);

	m_deparseButton.MoveWindow(
		client.Width() - margin - buttonWidth,
		searchTop,
		buttonWidth,
		searchHeight);

	const int panelLabelTop = searchTop + searchHeight + gap;
	const int panelTop = panelLabelTop + labelHeight;
	const int panelBottom = client.Height() - margin - statusHeight;
	const int panelHeight = max(50, panelBottom - panelTop);

	if (objectsLabel)
		objectsLabel->MoveWindow(
			margin,
			panelLabelTop,
			leftWidth,
			labelHeight);

	if (outputLabel)
		outputLabel->MoveWindow(
			margin + leftWidth + gap,
			panelLabelTop,
			client.Width() - margin * 2 - leftWidth - gap,
			labelHeight);

	m_resultsList.MoveWindow(
		margin,
		panelTop,
		leftWidth,
		panelHeight);

	m_outputEdit.MoveWindow(
		margin + leftWidth + gap,
		panelTop,
		client.Width() - margin * 2 - leftWidth - gap,
		panelHeight);

	m_objectCount.MoveWindow(
		margin,
		panelBottom + 4,
		leftWidth,
		statusHeight);
}

void CObjectDeparserDialog::OnSearchChanged()
{
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
}
