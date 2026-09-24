///////////////////////////////////////////////////////////////////////////////////////
// FILE: ObjectDeparserLoadingDialog.cpp
// Author: Gamezerve, September 2026
// Description: 
///////////////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "ObjectDeparserLoadingDialog.h"

BEGIN_MESSAGE_MAP(CObjectDeparserLoadingDialog, CDialog)
END_MESSAGE_MAP()

CObjectDeparserLoadingDialog::CObjectDeparserLoadingDialog(CWnd* parent)
	: CDialog(IDD_OBJECT_DEPARSER_LOADING, parent)
{
}

void CObjectDeparserLoadingDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);

	DDX_Control(pDX, IDC_LOADING_PROGRESS, m_progress);
	DDX_Control(pDX, IDC_LOADING_STATUS, m_status);
}

BOOL CObjectDeparserLoadingDialog::OnInitDialog()
{
	CDialog::OnInitDialog();

	m_progress.SetRange(0, 100);
	m_progress.SetPos(0);

	m_status.SetWindowText("Starting...");

	CenterWindow();

	return TRUE;
}

void CObjectDeparserLoadingDialog::setProgress(
	Int progress,
	const char* status)
{
	m_progress.SetPos(progress);
	m_status.SetWindowText(status);

	m_progress.UpdateWindow();
	m_status.UpdateWindow();
	UpdateWindow();

	MSG message;

	while (PeekMessage(
		&message,
		nullptr,
		0,
		0,
		PM_REMOVE))
	{
		TranslateMessage(&message);
		DispatchMessage(&message);
	}
}
