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
#include "ParsedDefinitionCatalog.h"

typedef void (*ObjectReloadProgressProc)(Int progress, const char* status, void* userData);

class CObjectDeparserApp : public CWinApp
{
public:
	CObjectDeparserApp();

	virtual BOOL InitInstance() override;
	virtual int ExitInstance() override;

	Bool reloadObjectDatabase(ObjectReloadProgressProc progressProc, void* userData);

	const CString& getLastObjectIniLoadTime() const
	{
		return m_lastObjectIniLoadTime;
	}

	ParsedDefinitionCatalog& getDefinitionCatalog()
	{
		return m_definitionCatalog;
	}

	const ParsedDefinitionCatalog& getDefinitionCatalog() const
	{
		return m_definitionCatalog;
	}

	DECLARE_MESSAGE_MAP()

private:
	void updateObjectIniLoadTimestamp();

	CString m_lastObjectIniLoadTime;
	ParsedDefinitionCatalog m_definitionCatalog;
};

inline CObjectDeparserApp* ObjectDeparserApp()
{
	return static_cast<CObjectDeparserApp*>(AfxGetApp());
}
