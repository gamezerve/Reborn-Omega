///////////////////////////////////////////////////////////////////////////////////////
// FILE: ObjectDeparserDialog.cpp
// Author: Gamezerve, September 2026
// Description: 
///////////////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
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

	const AsciiString output =
		ThingTemplateDeparser::deparse(thing);

	m_outputEdit.SetWindowText(output.str());
}
