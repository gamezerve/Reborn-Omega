///////////////////////////////////////////////////////////////////////////////////////
// FILE: ObjectDeparser.h
// Author: Gamezerve, September 2026
// Description: 
///////////////////////////////////////////////////////////////////////////////////////

#pragma once

#ifndef __AFXWIN_H__
#error include 'StdAfx.h' before including this file
#endif

#include "resource.h"

class CObjectDeparserApp : public CWinApp
{
public:
	CObjectDeparserApp();

	virtual BOOL InitInstance() override;

	DECLARE_MESSAGE_MAP()
};
