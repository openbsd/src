/*
 * Portions Copyright (C) 2001  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
 * INTERNET SOFTWARE CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* $ISC: BINDInstallDlg.h,v 1.3 2001/07/31 00:03:14 gson Exp $ */

/*
 * Copyright (c) 1999-2000 by Nortel Networks Corporation
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND NORTEL NETWORKS DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL NORTEL NETWORKS
 * BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES
 * OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
 * ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

#ifndef BINDINSTALLDLG_H
#define BINDINSTALLDLG_H

class CBINDInstallDlg : public CDialog
{
public:
	CBINDInstallDlg(CWnd* pParent = NULL);	// standard constructor

	//{{AFX_DATA(CBINDInstallDlg)
	enum { IDD = IDD_BINDINSTALL_DIALOG };
	CString	m_targetDir;
	CString	m_version;
	BOOL	m_autoStart;
	BOOL	m_keepFiles;
	CString	m_current;
	BOOL	m_startOnInstall;
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CBINDInstallDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support
	//}}AFX_VIRTUAL

protected:
	void StartBINDService();
	void StopBINDService();

	void InstallTags();
	void UninstallTags();

	void CreateDirs();
	void RemoveDirs(BOOL uninstall);

	void CopyFiles();
	void DeleteFiles(BOOL uninstall);

	void RegisterService();
	void UnregisterService(BOOL uninstall);

	void RegisterMessages();
	void UnregisterMessages(BOOL uninstall);
	
	void FailedInstall();
	void SetItemStatus(UINT nID, BOOL bSuccess = TRUE);

protected:
	CString DestDir(int destination);
	int MsgBox(int id,  ...);
	int MsgBox(int id, UINT type, ...);
	CString GetErrMessage(DWORD err = -1);
	BOOL CheckBINDService();
	void SetCurrent(int id, ...);
	void ProgramGroup(BOOL create = TRUE);
	
	HICON m_hIcon;
	CString m_defaultDir;
	CString m_etcDir;
	CString m_binDir;
	CString m_winSysDir;
	BOOL m_reboot;
	CString m_currentDir;

	// Generated message map functions
	//{{AFX_MSG(CBINDInstallDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnBrowse();
	afx_msg void OnChangeTargetdir();
	afx_msg void OnInstall();
	afx_msg void OnExit();
	afx_msg void OnUninstall();
	afx_msg void OnAutoStart();
	afx_msg void OnKeepFiles();
	afx_msg void OnStartOnInstall();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

#endif
