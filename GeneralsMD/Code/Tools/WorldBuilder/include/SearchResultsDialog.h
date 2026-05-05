#pragma once


// SearchResultsDialog.h : header file for Reborn WorldBuilder search results dialog
//

#include "ScriptDialog.h"

class SearchResultsDialog : public CDialog
{
public:
	SearchResultsDialog(CWnd* pParent = nullptr);

	enum { IDD = IDD_SEARCH_RESULTS };

	struct SearchResultItem
	{
		CString playerText;
		CString folderText;
		CString scriptText;
		ListType playerSelection;
		ListType folderSelection;
		ListType itemSelection;
	};

	void AddResult(
		const CString& playerText,
		const CString& folderText,
		const CString& scriptText,
		const ListType& playerSel,
		const ListType& folderSel,
		const ListType& itemSel);

	bool HasSelection() const;
	ListType GetSelectedResult() const;

	void SetOwnerDialog(ScriptDialog* pOwner);
	void RunSearch();

	HTREEITEM FindChildItemByText(HTREEITEM hParent, const CString& text);
	void PopulateTreeFromResults();

protected:
	CTreeCtrl m_resultsTree;

	CEdit m_searchEdit;
	ScriptDialog* m_ownerDialog;

	CArray<SearchResultItem, SearchResultItem&> m_results;
	int m_selectedIndex;

	virtual void DoDataExchange(CDataExchange* pDX) override;
	virtual BOOL OnInitDialog() override;
	afx_msg void OnDblclkResultsTree(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnGoToResult();
	afx_msg void OnSearch();
	afx_msg void OnExpandAll();
	afx_msg void OnCollapseAll();
	virtual void OnOK() override;
	virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam) override;

	virtual void PostNcDestroy() override;
	virtual void OnCancel() override;
	virtual BOOL PreTranslateMessage(MSG* pMsg);

	void ExpandAllItems(HTREEITEM hItem);
	void CollapseAllItems(HTREEITEM hItem);

	DECLARE_MESSAGE_MAP()
};

