///////////////////////////////////////////////////////////////////////////////////////
// FILE: ObjectDeparserLoadingDialog.h
// Author: Gamezerve, September 2026
// Description: 
///////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "resource.h"

class CObjectDeparserLoadingDialog : public CDialog
{
public:
	CObjectDeparserLoadingDialog(CWnd* parent = nullptr);

	enum { IDD = IDD_OBJECT_DEPARSER_LOADING };

	void setProgress(Int progress, const char* status);

protected:
	virtual void DoDataExchange(CDataExchange* pDX) override;
	virtual BOOL OnInitDialog() override;

	DECLARE_MESSAGE_MAP()

private:
	CProgressCtrl m_progress;
	CStatic m_status;
};
