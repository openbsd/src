
// pageset.h : page setup dialog interface

// This is a part of the Microsoft Foundation Classes C++ library.
// Copyright (C) 1992 Microsoft Corporation
// All rights reserved.
//
// This source code is only intended as a supplement to the
// Microsoft Foundation Classes Reference and Microsoft
// QuickHelp and/or WinHelp documentation provided with the library.
// See these sources for detailed information regarding the
// Microsoft Foundation Classes product.


#ifndef _PAGESET_H_
#define _PAGESET_H_

/////////////////////////////////////////////////////////////////////////////
// CPageSetupDlg dialog

class CPageSetupDlg : public CDialog
{
	DECLARE_DYNAMIC(CPageSetupDlg)
// Construction
public:
	CPageSetupDlg(CWnd* pParent = NULL);    // standard constructor
	void Initialize();
	void Terminate();

// Dialog Data
	//{{AFX_DATA(CPageSetupDlg)
	enum { IDD = ID_SYM_DIALOG_PAGE_SETUP };
	CString m_strFooter;
	CString m_strHeader;
	int     m_iFooterTime;
	int     m_iHeaderTime;
	//}}AFX_DATA

	CString m_strFooterOld;
	CString m_strHeaderOld;
	int     m_iFooterTimeOld;
	int     m_iHeaderTimeOld;

// Operations
	void FormatHeader(CString& strHeader, CTime& time,
		const char* pszFileName, UINT nPage);
	void FormatFooter(CString& strFooter, CTime& time,
		const char* pszFileName, UINT nPage);

// Implementation
protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	static void FormatFilePage(
		CString& strFormat, const char* pszFileName, UINT nPage);

	// Generated message map functions
	//{{AFX_MSG(CPageSetupDlg)
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

#endif // _PAGESET_H_

/////////////////////////////////////////////////////////////////////////////
