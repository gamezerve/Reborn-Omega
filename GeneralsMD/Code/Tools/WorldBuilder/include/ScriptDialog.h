/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

// ScriptDialog.h : header file
//

#include "GameLogic/SidesList.h"
#include <map>

class ListType  {
public:
	enum {BOGUS_TYPE = 0, PLAYER_TYPE = 1, GROUP_TYPE, SCRIPT_IN_PLAYER_TYPE, SCRIPT_IN_GROUP_TYPE};
	unsigned char m_objType;		 // 4 bits
	unsigned char m_playerIndex; // 4 bits
	unsigned short int m_groupIndex;	 // 12 bits
	unsigned short int m_scriptIndex; // 12 bits

	ListType() {m_objType=BOGUS_TYPE;m_playerIndex=0;m_groupIndex = 0; m_scriptIndex=0;}

	Int ListToInt() const { return((m_objType<<28)+(m_playerIndex<<24)+(m_groupIndex<<12)+m_scriptIndex);}

	void IntToList(int i) {m_objType = ((i)>>28)&0x0F; m_playerIndex = ((i)>>24)&0x0F; m_groupIndex = ((i)>>12)&0x0FFF; m_scriptIndex = (i)&0x0FFF;}
};

class ScriptList;
class ScriptGroup;
class Script;
class Parameter;
class SearchResultsDialog;

/** Class Definition for overridden Tree control that
    supports Right-click context sensitive menu.*/
class CSDTreeCtrl : public CTreeCtrl
{
	public:

	protected:
		virtual void OnRButtonDown(UINT nFlags, CPoint point);
		DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
// ScriptDialog dialog

class ScriptDialog : public CDialog
{
// Construction
public:
	ScriptDialog(CWnd* pParent = nullptr);   // standard constructor
	virtual ~ScriptDialog() override;   //  destructor

// Dialog Data
	//{{AFX_DATA(ScriptDialog)
	enum { IDD = IDD_ScriptDialog };
		// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(ScriptDialog)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX) override;    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
public:
	static void updateWarnings(Bool forceUpdate=false);
	static void updateScriptWarning(Script *pScript);

	static void patchScriptParametersForGC(Script *pScript);
	static void checkParametersForGC();

	void GoToSearchResult(const ListType& sel);
	void CollectSearchResults(const CString& text, SearchResultsDialog& dlg);

	void NotifySearchDialogDestroyed();
	void OpenSearchResultsDialog();

	/// To allow CSDTreeCtrl access to these member functions of ScriptDialog
	Script *friend_getCurScript();
	ScriptGroup *friend_getCurGroup();

protected:
	ListType	m_curSelection;
	CImageList m_imageList;
	SidesList	m_sides;
	static ScriptDialog *m_staticThis;
	CSDTreeCtrl *mTree;
	Bool			m_draggingTreeView;
	Bool m_autoUpdateWarnings;	///< flag whether we should updateWarnings on script editor actions.

	HTREEITEM m_dragItem;

	SearchResultsDialog* m_searchResultsDialog;

	MapObject *m_firstReadObject;
	PolygonTrigger *m_firstTrigger;
	Int							m_waypointBase;
	Int							m_maxWaypoint;

	AsciiString			m_readPlayerNames[MAX_PLAYER_COUNT];

	Int m_splitterY;
	Bool m_draggingSplitter;
	Int m_splitterHeight;
	Int m_minTopPaneHeight;
	Int m_minBottomPaneHeight;

	Int m_bottomSplitterX;
	Int m_lastClientWidth;
	Int m_lastClientHeight;
	Bool m_draggingBottomSplitter;
	Int m_minLeftBottomPaneWidth;
	Int m_minRightBottomPaneWidth;

	void RepositionControls(int cx, int cy);

protected:
	HTREEITEM addPlayer(Int playerIndx);
	void addScriptList(HTREEITEM hPlayer, Int playerIndex, ScriptList *pSL);
	void doDropOn(HTREEITEM hDrop, HTREEITEM hTarget);
	Script *getCurScript();
	ScriptGroup *getCurGroup();
	void reloadPlayer(Int playerIndex, ScriptList *pSL);
	HTREEITEM findItem(ListType sel, Bool failSafe = FALSE);
	void insertScript(Script *pNewScript);
	void scanForWaypointsAndTeams(Script *pScript, Bool doUnits, Bool doWaypoints, Bool doTriggers);
	void scanParmForWaypointsAndTeams(Parameter *pParm, Bool doUnits, Bool doWaypoints, Bool doTriggers);
	void updateSelection(ListType sel);
	void setIconScript(HTREEITEM item);
	void setIconGroup(HTREEITEM item);
	Bool updateIcons(HTREEITEM hItem);
	void markWaypoint(MapObject *pObj);
	void CloseSearchResultsDialog();

	void BuildSelectionsForItem(HTREEITEM hItem, ListType& playerSel, ListType& folderSel, ListType& itemSel);
	void BuildSearchableTextForItem(HTREEITEM hItem, CString& searchableText, CString& playerText, CString& folderText, CString& scriptText);

	void FindTreeItemsContains(HTREEITEM hItem, const CString& text, CString& results, int& matchCount);
	void CollectTreeItemsContains(HTREEITEM hItem, const CString& text, SearchResultsDialog& dlg);
	void GetItemDisplayParts(HTREEITEM hItem, CString& playerText, CString& folderText, CString& scriptText);

	static Bool ParseObjectsDataChunk(DataChunkInput &file, DataChunkInfo *info, void *userData);
	static Bool ParseObjectDataChunk(DataChunkInput &file, DataChunkInfo *info, void *userData);
	static Bool ParsePolygonTriggersDataChunk(DataChunkInput &file, DataChunkInfo *info, void *userData);
	static Bool ParseWaypointDataChunk(DataChunkInput &file, DataChunkInfo *info, void *userData);
	static Bool ParseTeamsDataChunk(DataChunkInput &file, DataChunkInfo *info, void *userData);
	static Bool ParsePlayersDataChunk(DataChunkInput &file, DataChunkInfo *info, void *userData);

	CRect m_initialClientRect;
	std::map<int, CRect> m_initialRects;


protected:

	// Generated message map functions
	//{{AFX_MSG(ScriptDialog)
	afx_msg void OnSelchangedScriptTree(NMHDR* pNMHDR, LRESULT* pResult);
	virtual BOOL OnInitDialog() override;
	afx_msg void OnNewFolder();
	afx_msg void OnNewScript();
	afx_msg void OnEditScript();
	afx_msg void OnCopyScript();
	afx_msg void OnDelete();
	afx_msg void OnVerify();
	afx_msg void OnPatchGC();
	afx_msg void OnSearch();
	afx_msg void OnAutoVerify();
	afx_msg void OnSave();
	afx_msg void OnLoad();
	afx_msg void OnDblclkScriptTree(NMHDR* pNMHDR, LRESULT* pResult);
	virtual void OnOK() override;
	virtual void OnCancel() override;
	afx_msg void OnBegindragScriptTree(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnScriptActivate();
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMove(int x, int y);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnGetMinMaxInfo(MINMAXINFO* lpMMI);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.
