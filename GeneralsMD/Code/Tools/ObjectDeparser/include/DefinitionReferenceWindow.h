///////////////////////////////////////////////////////////////////////////////////////
// FILE: DefinitionReferenceWindow.h
// Author: Gamezerve, September 2026
// Description: Reborn: Modeless viewer for a referenced parsed definition.
///////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "ParsedDefinitionCatalog.h"

class CObjectDeparserDialog;

class CDefinitionReferenceWindow : public CFrameWnd
{
	DECLARE_DYNAMIC(CDefinitionReferenceWindow)

public:
	// Reborn: Keep only stable definition identity values so reload cannot leave engine pointers here.
	CDefinitionReferenceWindow(
		CObjectDeparserDialog* owner,
		const AsciiString& blockType,
		const AsciiString& name);

	// Reborn: Create one independent owned top-level window for every reference click.
	Bool createWindow(CWnd* owner);

	// Reborn: Replace the viewer text with freshly generated content from the current engine state.
	void setContent(const CString& content);

	// Reborn: Remove stale deparse text before the engine database is replaced.
	void setReloading();

	const AsciiString& getBlockType() const
	{
		return m_blockType;
	}

	const AsciiString& getDefinitionName() const
	{
		return m_name;
	}

protected:
	afx_msg int OnCreate(LPCREATESTRUCT createStruct);
	afx_msg void OnSize(UINT type, int width, int height);
	afx_msg void OnClose();
	virtual void PostNcDestroy() override;

	DECLARE_MESSAGE_MAP()

private:
	CObjectDeparserDialog* m_owner;
	AsciiString m_blockType;
	AsciiString m_name;
	CRichEditCtrl m_contentEdit;
	CFont m_contentFont;
	COLORREF m_backgroundColor;
	COLORREF m_textColor;
};
