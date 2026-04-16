


// SearchResultsDialog.cpp : implementation of the Reborn WorldBuildr Search Functionality Dialog
//

#include "StdAfx.h"
#include "WorldBuilder.h"
#include "SearchResultsDialog.h"

SearchResultsDialog::SearchResultsDialog(CWnd* pParent)
	: CDialog(SearchResultsDialog::IDD, pParent)
{
	m_selectedIndex = -1;
	m_ownerDialog = nullptr;
}

void SearchResultsDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_RESULTS_TREE, m_resultsTree);
	DDX_Control(pDX, IDC_SEARCH_TEXT, m_searchEdit);
}

BEGIN_MESSAGE_MAP(SearchResultsDialog, CDialog)
	ON_NOTIFY(NM_DBLCLK, IDC_RESULTS_TREE, OnDblclkResultsTree)
	ON_BN_CLICKED(IDC_SEARCH, OnSearch)
	ON_BN_CLICKED(IDC_EXPAND_ALL, OnExpandAll)
	ON_BN_CLICKED(IDC_COLLAPSE_ALL, OnCollapseAll)
END_MESSAGE_MAP()

BOOL SearchResultsDialog::OnInitDialog()
{
	CDialog::OnInitDialog();
	return TRUE;
}

void SearchResultsDialog::SetOwnerDialog(ScriptDialog* pOwner)
{
	m_ownerDialog = pOwner;
}

void SearchResultsDialog::RunSearch()
{
	if (m_ownerDialog == nullptr)
		return;

	CString searchText;
	m_searchEdit.GetWindowText(searchText);
	searchText.Trim();

	m_results.RemoveAll();
	m_resultsTree.DeleteAllItems();
	m_selectedIndex = -1;

	if (searchText.IsEmpty())
		return;

	m_ownerDialog->CollectSearchResults(searchText, *this);
	PopulateTreeFromResults();

	if (m_results.GetSize() == 0)
	{
		HTREEITEM hRoot = m_resultsTree.InsertItem("No results found for: " + searchText, TVI_ROOT, TVI_LAST);
		m_resultsTree.SetItemData(hRoot, 0);
		return;
	}
}

HTREEITEM SearchResultsDialog::FindChildItemByText(HTREEITEM hParent, const CString& text)
{
	HTREEITEM hChild = m_resultsTree.GetChildItem(hParent);
	while (hChild != nullptr)
	{
		if (m_resultsTree.GetItemText(hChild) == text)
			return hChild;

		hChild = m_resultsTree.GetNextSiblingItem(hChild);
	}

	return nullptr;
}

void SearchResultsDialog::PopulateTreeFromResults()
{
	for (int i = 0; i < m_results.GetSize(); ++i)
	{
		const SearchResultItem& item = m_results[i];

		HTREEITEM hPlayer = FindChildItemByText(TVI_ROOT, item.playerText);
		if (hPlayer == nullptr)
		{
			hPlayer = m_resultsTree.InsertItem(item.playerText, TVI_ROOT, TVI_LAST);
			m_resultsTree.SetItemData(hPlayer, item.playerSelection.ListToInt());
		}

		if (item.folderText.IsEmpty() && item.scriptText.IsEmpty())
		{
			continue;
		}

		HTREEITEM hFolder = hPlayer;
		if (!item.folderText.IsEmpty())
		{
			hFolder = FindChildItemByText(hPlayer, item.folderText);
			if (hFolder == nullptr)
			{
				hFolder = m_resultsTree.InsertItem(item.folderText, hPlayer, TVI_LAST);
				m_resultsTree.SetItemData(hFolder, item.folderSelection.ListToInt());
			}
		}

		if (!item.scriptText.IsEmpty())
		{
			HTREEITEM hScript = m_resultsTree.InsertItem(item.scriptText, hFolder, TVI_LAST);
			m_resultsTree.SetItemData(hScript, item.itemSelection.ListToInt());
		}
	}

	HTREEITEM hRoot = m_resultsTree.GetRootItem();
	while (hRoot != nullptr)
	{
		m_resultsTree.Expand(hRoot, TVE_EXPAND);
		hRoot = m_resultsTree.GetNextSiblingItem(hRoot);
	}
}

void SearchResultsDialog::ExpandAllItems(HTREEITEM hItem)
{
	while (hItem != nullptr)
	{
		m_resultsTree.Expand(hItem, TVE_EXPAND);

		HTREEITEM hChild = m_resultsTree.GetChildItem(hItem);
		if (hChild != nullptr)
		{
			ExpandAllItems(hChild);
		}

		hItem = m_resultsTree.GetNextSiblingItem(hItem);
	}
}

void SearchResultsDialog::CollapseAllItems(HTREEITEM hItem)
{
	while (hItem != nullptr)
	{
		HTREEITEM hChild = m_resultsTree.GetChildItem(hItem);
		if (hChild != nullptr)
		{
			CollapseAllItems(hChild);
		}

		m_resultsTree.Expand(hItem, TVE_COLLAPSE);
		hItem = m_resultsTree.GetNextSiblingItem(hItem);
	}
}

void SearchResultsDialog::OnExpandAll()
{
	HTREEITEM hRoot = m_resultsTree.GetRootItem();
	if (hRoot != nullptr)
	{
		ExpandAllItems(hRoot);
	}
}

void SearchResultsDialog::OnCollapseAll()
{
	HTREEITEM hRoot = m_resultsTree.GetRootItem();
	if (hRoot != nullptr)
	{
		CollapseAllItems(hRoot);
	}
}

void SearchResultsDialog::OnSearch()
{
	RunSearch();
}

void SearchResultsDialog::AddResult(
	const CString& playerText,
	const CString& folderText,
	const CString& scriptText,
	const ListType& playerSel,
	const ListType& folderSel,
	const ListType& itemSel)
{
	SearchResultItem item;
	item.playerText = playerText;
	item.folderText = folderText;
	item.scriptText = scriptText;
	item.playerSelection = playerSel;
	item.folderSelection = folderSel;
	item.itemSelection = itemSel;
	m_results.Add(item);
}



void SearchResultsDialog::OnDblclkResultsTree(NMHDR* pNMHDR, LRESULT* pResult)
{
	OnGoToResult();
	*pResult = 0;
}

void SearchResultsDialog::OnGoToResult()
{
	HTREEITEM hSelected = m_resultsTree.GetSelectedItem();
	if (hSelected == nullptr)
		return;

	ListType sel;
	sel.IntToList((int)m_resultsTree.GetItemData(hSelected));

	if (m_ownerDialog != nullptr)
	{
		m_ownerDialog->GoToSearchResult(sel);
	}
}

void SearchResultsDialog::OnOK()
{
}

BOOL SearchResultsDialog::OnCommand(WPARAM wParam, LPARAM lParam)
{
	if (LOWORD(wParam) == IDC_GO_TO_RESULT)
	{
		OnGoToResult();
		return TRUE;
	}

	return CDialog::OnCommand(wParam, lParam);
}

void SearchResultsDialog::OnCancel()
{
	DestroyWindow();
}

void SearchResultsDialog::PostNcDestroy()
{
	if (m_ownerDialog != nullptr)
	{
		m_ownerDialog->NotifySearchDialogDestroyed();
	}

	CDialog::PostNcDestroy();
	delete this;
}
