///////////////////////////////////////////////////////////////////////////////////////
// FILE: ObjectDeparserDialog.h
// Author: Gamezerve, September 2026
// Description: 
///////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "resource.h"

#include <vector>

class ThingTemplate;

class CObjectDeparserDialog : public CDialog
{
public:
	CObjectDeparserDialog(CWnd* parent = nullptr);

	enum { IDD = IDD_OBJECT_DEPARSER_DIALOG };

protected:
	virtual void DoDataExchange(CDataExchange* pDX) override;
	virtual BOOL OnInitDialog() override;

	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnSearchChanged();
	afx_msg void OnSelectionChanged();
	afx_msg void OnResultDoubleClicked();
	afx_msg void OnDeparseNow();

	DECLARE_MESSAGE_MAP()

private:
	void layoutControls();
	void buildTemplateList();
	void refreshTemplateList();

	CEdit m_searchEdit;
	CListBox m_resultsList;
	CButton m_deparseButton;
	CEdit m_outputEdit;
	CStatic m_objectCount;

	CFont m_outputFont;

	std::vector<const ThingTemplate*> m_templates;
};
