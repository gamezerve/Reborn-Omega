///////////////////////////////////////////////////////////////////////////////////////
// FILE: ObjectDeparser.cpp
// Author: Gamezerve, September 2026
// Description: 
///////////////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "ObjectDeparser.h"
#include "ObjectDeparserDialog.h"

CObjectDeparserApp theApp;

BEGIN_MESSAGE_MAP(CObjectDeparserApp, CWinApp)
END_MESSAGE_MAP()

CObjectDeparserApp::CObjectDeparserApp()
{
}

BOOL CObjectDeparserApp::InitInstance()
{
	CWinApp::InitInstance();

	AfxEnableControlContainer();

	CObjectDeparserDialog dialog;
	m_pMainWnd = &dialog;

	dialog.DoModal();

	return FALSE;
}
