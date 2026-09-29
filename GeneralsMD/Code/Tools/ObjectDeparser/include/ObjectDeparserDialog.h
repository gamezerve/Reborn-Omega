///////////////////////////////////////////////////////////////////////////////////////
// FILE: ObjectDeparserDialog.h 
// Author: Gamezerve, September 2026
// Description: 
///////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "resource.h"
#include "ParsedDefinitionCatalog.h"
#include "TextDiff.h"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class CDefinitionReferenceWindow;

class CObjectDeparserDialog : public CDialog
{
public:
	CObjectDeparserDialog(CWnd* parent = nullptr);

	// Reborn: Let self-deleting modeless viewers remove only their own tracking entry.
	void onDefinitionReferenceWindowDestroyed(
		CDefinitionReferenceWindow* window);

	enum { IDD = IDD_OBJECT_DEPARSER_DIALOG };

protected:
	virtual void DoDataExchange(CDataExchange* pDX) override;
	virtual BOOL OnInitDialog() override;
	// Reborn: Route editor find shortcuts before the dialog consumes Enter and Escape.
	virtual BOOL PreTranslateMessage(MSG* message) override;

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
	// Reborn: Navigate the active editor's incremental find results in either direction.
	afx_msg void OnEditorFindChanged();
	afx_msg void OnEditorFindPrevious();
	afx_msg void OnEditorFindNext();
	afx_msg void OnEditorFindClose();
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
	// Reborn: Handle RichEdit link activation in either primary editor.
	afx_msg void OnDefinitionLink(NMHDR* notifyHeader, LRESULT* result);


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
	// Reborn: Paint line numbers and compact compare-state symbols beside one editor.
	void drawCompareGutter(
		LPDRAWITEMSTRUCT drawItem,
		CRichEditCtrl& edit,
		const std::unordered_map<Int, Int>& markers);
	// Reborn: Replace both gutter marker sets atomically from the latest diff result.
	void updateCompareGutterMarkers(const TextDiffResult& result);
	// Reborn: Repaint gutters only when their editor scroll positions actually change.
	void refreshCompareGutters();
	// Reborn: Align diff hunks visually through paragraph spacing without changing editor text.
	void applyCompareLineGaps(CRichEditCtrl& edit, const std::vector<TextDiffLineGap>& gaps);
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
	// Reborn: Show one find bar bound to the editor that owned the Ctrl+F shortcut.
	void showEditorFindBar(CRichEditCtrl& edit);
	void hideEditorFindBar();
	void findInActiveEditor(Bool backwards, Bool startFromSelection);
	// Reborn: Mark every exact occurrence of the custom double-click token without editing text.
	void highlightTokenOccurrences(
		CRichEditCtrl& edit,
		const CString& token,
		long selectedStart,
		long selectedEnd);
	void clearTokenOccurrenceHighlights();
	// Reborn: Handle only an actual editor double-click without subscribing to every mouse move.
	Bool selectEditorTokenAtPoint(CRichEditCtrl& edit, CPoint point);
	static void reloadBlockParsedCallback(
		const AsciiString& declaration,
		const AsciiString& blockType,
		const AsciiString& filename,
		UnsignedInt line,
		INILoadType loadType,
		void* userData);

	void pumpReloadMessages();
	// Reborn: Share current-state deparse generation between the main and reference views.
	Bool buildDeparsedDefinitionText(
		const ParsedDefinition& definition,
		std::string& output) const;
	// Reborn: Apply safe catalog-backed link formatting without changing editor text or undo history.
	void updateDefinitionLinks(CRichEditCtrl& edit);
	// Reborn: Infer a type family from the current INI line to disambiguate same-name definitions.
	AsciiString getReferenceTypeHint(
		const CString& text,
		long tokenStart) const;
	// Reborn: Resolve the clicked character range against the current catalog only.
	const ParsedDefinition* resolveDefinitionLink(
		CRichEditCtrl& edit,
		const CHARRANGE& range) const;
	// Reborn: Resolve a clicked Generals.str label independently from parsed INI definitions.
	Bool resolveGameTextLink(
		CRichEditCtrl& edit,
		const CHARRANGE& range,
		AsciiString& label) const;
	// Reborn: Fetch current localized text directly from the initialized game string subsystem.
	Bool buildGameTextDefinition(
		const AsciiString& label,
		CString& output) const;
	// Reborn: Resolve each editor's own declaration so Deparsed and Working Copy filter independently.
	const ParsedDefinition* resolveEditorDefinition(
		const CString& text) const;
	// Reborn: Open one independent modeless window for every successful reference click.
	void openDefinitionReference(
		const ParsedDefinition& definition);
	// Reborn: Open one independent viewer for a resolved Generals.str label.
	void openGameTextReference(const AsciiString& label);
	// Reborn: Clear child content before reload replaces engine-owned definition objects.
	void setReferenceWindowsReloading();
	// Reborn: Rebuild every open child from current catalog and engine state after reload.
	void refreshReferenceWindows(Bool reloadSucceeded);

	Bool m_reloadInProgress;
	Bool m_pumpingReloadMessages;
	Bool m_updatingDefinitionLinks;
	// Reborn: Suppress edit/debounce work for background-only token highlighting.
	Bool m_updatingOccurrenceHighlights;
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
	CEdit m_editorFindEdit;
	CButton m_editorFindPrevious;
	CButton m_editorFindNext;
	CButton m_editorFindClose;
	// Reborn: Owner-drawn gutters provide persistent line numbers and compare operation markers.
	CStatic m_outputGutter;
	CStatic m_workGutter;
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
	// Reborn: Cache live Generals.str resolutions so repeated labels do not refetch during link scans.
	mutable std::unordered_map<std::string, CString> m_gameTextDefinitionCache;
	mutable std::unordered_set<std::string> m_missingGameTextLabels;
	// Reborn: Track modeless windows for reload refresh; each window owns and deletes itself.
	std::vector<CDefinitionReferenceWindow*> m_referenceWindows;
	// Reborn: Store one precedence-resolved compare marker code per physical editor line.
	std::unordered_map<Int, Int> m_outputGutterMarkers;
	std::unordered_map<Int, Int> m_workGutterMarkers;

	enum
	{
		TIMER_COMPARE_SCROLL = 2001,
		TIMER_COMPARE_DEBOUNCE = 2002,
		// Reborn: Debounce link rescans while the working copy is being edited.
		TIMER_DEFINITION_LINKS = 2003,
		// Reborn: Track independent editor scrolling for line-number gutter repainting.
		TIMER_GUTTER_REFRESH = 2004
	};

	Bool m_compareMode;
	Bool m_compareUpdating;
	// Reborn: Keep search state independent from definition-list filtering.
	Bool m_editorFindVisible;
	CRichEditCtrl* m_editorFindTarget;
	// Reborn: Remember which editor owns transient same-token highlighting.
	CRichEditCtrl* m_occurrenceHighlightEdit;
	struct OccurrenceHighlightFormat
	{
		CHARRANGE range;
		COLORREF backgroundColor;
		Bool automaticBackground;
	};
	// Reborn: Restore exact pre-highlight backgrounds without recomputing the complete diff.
	std::vector<OccurrenceHighlightFormat> m_occurrenceHighlightFormats;

	CPoint m_lastOutputScroll;
	CPoint m_lastWorkScroll;
	CPoint m_lastOutputGutterScroll;
	CPoint m_lastWorkGutterScroll;
	// Reborn: Detect structural line insertions/removals even if a RichEdit change notification is delayed.
	Int m_lastComparedOutputLineCount;
	Int m_lastComparedWorkLineCount;

};
