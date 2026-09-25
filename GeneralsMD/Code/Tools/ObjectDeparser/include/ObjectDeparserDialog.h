///////////////////////////////////////////////////////////////////////////////////////
// FILE: ObjectDeparserDialog.h 
// Author: Gamezerve, September 2026
// Description: 
///////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "resource.h"
#include "ParsedDefinitionCatalog.h"

#include <vector>

class CObjectDeparserDialog : public CDialog
{
public:
	CObjectDeparserDialog(CWnd* parent = nullptr);

	enum { IDD = IDD_OBJECT_DEPARSER_DIALOG };

protected:
	virtual void DoDataExchange(CDataExchange* pDX) override;
	virtual BOOL OnInitDialog() override;

	virtual void OnOK() override;
	virtual void OnCancel() override;

	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnSearchChanged();
	afx_msg void OnSelectionChanged();
	afx_msg void OnResultDoubleClicked();
	afx_msg void OnDeparseNow();
	afx_msg void OnTransfer();
	afx_msg void OnReloadINI();
	afx_msg void OnCompare();
	afx_msg void OnWorkingCopyChanged();
	afx_msg void OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct);
	afx_msg void OnMeasureItem(int nIDCtl, LPMEASUREITEMSTRUCT lpMeasureItemStruct);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
	afx_msg void OnPaint();
	afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
	afx_msg void OnOutputChanged();
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnClose();


	DECLARE_MESSAGE_MAP()

private:
	void layoutControls();
	void buildDefinitionList();
	void refreshDefinitionList();
	void selectDefinitionByDeclaration(const CString& declaration);
	static void reloadProgressCallback(Int progress, const char* status, void* userData);
	void updateReloadProgress(Int progress, const char* status);
	void clearCompareHighlight();
	void highlightLines(CRichEditCtrl& edit, const std::vector<Int>& lines, COLORREF color);
	void updateCompareButtonState();
	Bool isDefinitionImplemented(const ParsedDefinition* definition) const;
	void calculatePaneGeometry(CRect& leftPane,	CRect& firstSplitter,	CRect& middlePane, CRect& secondSplitter, CRect& rightPane) const;
	Bool m_draggingSplitter;
	Int m_activeSplitter;
	double m_firstSplitterRatio;
	double m_secondSplitterRatio;
	void performCompare();
	void stopCompareMode();
	void synchronizeCompareScroll();
	void restartCompareDebounce();
	static void reloadBlockParsedCallback(
		const AsciiString& declaration,
		const AsciiString& blockType,
		const AsciiString& filename,
		UnsignedInt line,
		INILoadType loadType,
		void* userData);

	void pumpReloadMessages();

	Bool m_reloadInProgress;
	Bool m_pumpingReloadMessages;
	DWORD m_lastReloadPumpTick;


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
	CBrush m_backgroundBrush;
	CBrush m_editBrush;
	COLORREF m_backgroundColor;
	COLORREF m_panelColor;
	COLORREF m_textColor;
	COLORREF m_secondaryTextColor;
	COLORREF m_borderColor;
	COLORREF m_unsupportedColor;
	COLORREF m_selectionColor;

	std::vector<const ParsedDefinition*> m_definitions;

	enum
	{
		TIMER_COMPARE_SCROLL = 2001,
		TIMER_COMPARE_DEBOUNCE = 2002
	};

	Bool m_compareMode;
	Bool m_compareUpdating;

	CPoint m_lastOutputScroll;
	CPoint m_lastWorkScroll;

};
