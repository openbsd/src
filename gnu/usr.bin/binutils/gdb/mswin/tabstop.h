
// tabstop.h : tab setting dialog
//
// This is a part of the Microsoft Foundation Classes C++ library.
// Copyright (C) 1992 Microsoft Corporation
// All rights reserved.
//
// This source code is only intended as a supplement to the
// Microsoft Foundation Classes Reference and Microsoft
// QuickHelp and/or WinHelp documentation provided with the library.
// See these sources for detailed information regarding the
// Microsoft Foundation Classes product.


/////////////////////////////////////////////////////////////////////////////
// CSetTabStops dialog

class CSetTabStops : public CDialog
{
	DECLARE_DYNAMIC(CSetTabStops)
// Construction
public:
	CSetTabStops(CWnd* pParent = NULL); // standard constructor

// Dialog Data
	//{{AFX_DATA(CSetTabStops)
	enum { IDD = ID_SYM_DIALOG_SET_TABSTOPS };
	UINT    m_nTabStops;
	//}}AFX_DATA

// Implementation
protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	//{{AFX_MSG(CSetTabStops)
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
