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
	afx_msg void OnTransfer();
	afx_msg void OnReloadINI();
	afx_msg void OnCompare();
	afx_msg void OnWorkingCopyChanged();

	DECLARE_MESSAGE_MAP()

private:
	void layoutControls();
	void buildTemplateList();
	void refreshTemplateList();
	void selectTemplateByName(const CString& name);
	static void reloadProgressCallback(Int progress, const char* status, void* userData);
	void updateReloadProgress(Int progress, const char* status);
	void clearCompareHighlight();
	void highlightLines(CRichEditCtrl& edit, const std::vector<Int>& lines, COLORREF color);
	void updateCompareButtonState();

	CEdit m_searchEdit;
	CListBox m_resultsList;
	CButton m_deparseButton;
	CButton m_transferButton;
	CButton m_reloadButton;
	CRichEditCtrl m_outputEdit;
	CRichEditCtrl m_workEdit;
	CStatic m_objectCount;
	CProgressCtrl m_reloadProgress;
	CStatic m_reloadStatus;
	CButton m_compareButton;

	CFont m_outputFont;

	std::vector<const ThingTemplate*> m_templates;
};
