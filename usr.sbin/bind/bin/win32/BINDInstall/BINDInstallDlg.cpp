/*
 * Portions Copyright (C) 2001, 2002  Internet Software Consortium.
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

/* $ISC: BINDInstallDlg.cpp,v 1.6.2.3 2002/02/20 02:17:22 marka Exp $ */

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

/*
 * Define this to make a standalone installer that will copy msvcrt.dll
 * and/or msvcrtd.dll during the install
 */
// #define BINARIES_INSTALL

/*
 * msvcrt.dll is the release c-runtime library for MSVC.  msvcrtd.dll is the debug
 * c-runtime library for MSVC.  If you have debug binaries you want to have DEBUG_BINARIES
 * defined.  If you have release binaries you want to have RELEASE_BINARIES defined.
 * If you have both, then define them both.
 * Of course, you need msvcrt[d].dll present to install it!
 */
#ifdef BINARIES_INSTALL
// #  define DEBUG_BINARIES
// #  define RELEASE_BINARIES
#endif

#include "stdafx.h"
#include "BINDInstall.h"
#include "BINDInstallDlg.h"
#include "DirBrowse.h"
#include <winsvc.h>
#include <named/ntservice.h>
#include <isc/bind_registry.h>
#include <direct.h>
#include "versioninfo.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

typedef struct _exception
{
	_exception(UINT string, ...);
	
	CString resString;
} Exception;

_exception::_exception(UINT string, ...)
{
	CString format;
	va_list va;

	format.LoadString(string);

	va_start(va, string);
	resString.FormatV(format, va);
	va_end(va);
}

typedef struct _filedata
{
	enum FileDestinations {TargetDir, BinDir, EtcDir, WinSystem};
	enum FileImportance {Trivial, Normal, Critical};

	char *filename;
	int destination;
	int importance;
	BOOL checkVer;

} FileData;

const FileData installFiles[] =
{
#ifdef BINARIES_INSTALL
#  ifdef DEBUG_BINARIES
	{"msvcrtd.dll", FileData::WinSystem, FileData::Critical, TRUE},
#  endif
#  ifdef RELEASE_BINARIES
	{"msvcrt.dll", FileData::WinSystem, FileData::Critical, TRUE},
#  endif
#endif
	{"bindevt.dll", FileData::WinSystem, FileData::Normal, FALSE},
	{"libisc.dll", FileData::WinSystem, FileData::Critical, FALSE},
	{"libisccfg.dll", FileData::WinSystem, FileData::Critical, FALSE},
	{"libisccc.dll", FileData::WinSystem, FileData::Critical, FALSE},
	{"libdns.dll", FileData::WinSystem, FileData::Critical, FALSE},
	{"liblwres.dll", FileData::WinSystem, FileData::Critical, FALSE},
	{"libeay32.dll", FileData::WinSystem, FileData::Critical, FALSE},
	{"named.exe", FileData::BinDir, FileData::Critical, FALSE},
	{"nsupdate.exe", FileData::BinDir, FileData::Normal, FALSE},
	{"BINDInstall.exe", FileData::BinDir, FileData::Normal, FALSE},
	{"rndc.exe", FileData::BinDir, FileData::Normal, FALSE},
	{"dig.exe", FileData::BinDir, FileData::Normal, FALSE},
	{"host.exe", FileData::BinDir, FileData::Normal, FALSE},
	{"nslookup.exe", FileData::BinDir, FileData::Normal, FALSE},
	{"rndc-confgen.exe", FileData::BinDir, FileData::Normal, FALSE},
	{"dnssec-keygen.exe", FileData::BinDir, FileData::Normal, FALSE},
	{"dnssec-makekeyset.exe", FileData::BinDir, FileData::Normal, FALSE},
	{"dnssec-signkey.exe", FileData::BinDir, FileData::Normal, FALSE},
	{"dnssec-signzone.exe", FileData::BinDir, FileData::Normal, FALSE},
	{"named-checkconf.exe", FileData::BinDir, FileData::Normal, FALSE},
	{"named-checkzone.exe", FileData::BinDir, FileData::Normal, FALSE},
	{"readme1st.txt", FileData::BinDir, FileData::Trivial, FALSE},
	{NULL, -1, -1}
};

/////////////////////////////////////////////////////////////////////////////
// CBINDInstallDlg dialog

CBINDInstallDlg::CBINDInstallDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CBINDInstallDlg::IDD, pParent)
{
	char buf[MAX_PATH];

	//{{AFX_DATA_INIT(CBINDInstallDlg)
	m_targetDir = _T("");
	m_version = _T("");
	m_autoStart = FALSE;
	m_keepFiles = FALSE;
	m_current = _T("");
	m_startOnInstall = FALSE;
	//}}AFX_DATA_INIT
	// Note that LoadIcon does not require a subsequent DestroyIcon in Win32
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
	m_reboot = FALSE;

	GetSystemDirectory(buf, MAX_PATH);
	m_winSysDir = buf;
	m_defaultDir = buf;
	m_defaultDir += "\\dns";
}

void CBINDInstallDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CBINDInstallDlg)
	DDX_Text(pDX, IDC_TARGETDIR, m_targetDir);
	DDX_Text(pDX, IDC_VERSION, m_version);
	DDX_Check(pDX, IDC_AUTO_START, m_autoStart);
	DDX_Check(pDX, IDC_KEEP_FILES, m_keepFiles);
	DDX_Text(pDX, IDC_CURRENT, m_current);
	DDX_Check(pDX, IDC_START, m_startOnInstall);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CBINDInstallDlg, CDialog)
	//{{AFX_MSG_MAP(CBINDInstallDlg)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_BROWSE, OnBrowse)
	ON_BN_CLICKED(IDC_INSTALL, OnInstall)
	ON_BN_CLICKED(IDC_EXIT, OnExit)
	ON_BN_CLICKED(IDC_UNINSTALL, OnUninstall)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CBINDInstallDlg message handlers

BOOL CBINDInstallDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	char filename[MAX_PATH];
	char dirname[MAX_PATH];
	char *fptr = &filename[0];
	GetModuleFileName(NULL, filename, MAX_PATH);
	char *dptr = strrchr(filename,'\\');
	int index = dptr - fptr;
	strncpy(dirname, filename, index);
	dirname[index] = '\0';
	CString Dirname(dirname);
	m_currentDir = Dirname;

	
	CVersionInfo bindInst(filename);
	if(bindInst.IsValid())
		m_version.Format(IDS_VERSION, bindInst.GetFileVersionString());
	else
		m_version.LoadString(IDS_NO_VERSION);

	DWORD dwBufLen = MAX_PATH;
	char buf[MAX_PATH];
	HKEY hKey;

	m_startOnInstall = CheckBINDService();

	/* See if we are installed already */
	if(RegOpenKeyEx(HKEY_LOCAL_MACHINE, BIND_SUBKEY, 0, KEY_READ, &hKey) == ERROR_SUCCESS)
	{
		memset(buf, 0, MAX_PATH);
		// Get the install directory
		if(RegQueryValueEx(hKey, "InstallDir", NULL, NULL, (LPBYTE)buf, &dwBufLen) == ERROR_SUCCESS)
			if(strcmp(buf, ""))
				m_defaultDir = buf;
		
		RegCloseKey(hKey);
	}
	m_targetDir = m_defaultDir;

	// Set checkbox defaults
	m_autoStart = TRUE;
	m_keepFiles = TRUE;

	UpdateData(FALSE);

	return(TRUE);  // return(TRUE  unless you set the focus to a control
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CBINDInstallDlg::OnPaint() 
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, (WPARAM) dc.GetSafeHdc(), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}

// The system calls this to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CBINDInstallDlg::OnQueryDragIcon()
{
	return((HCURSOR)m_hIcon);
}

void CBINDInstallDlg::OnBrowse() 
{

	CDirBrowse browse;

	if(browse.DoModal() == IDOK)
	{
		//m_targetDir = browse.m_selectedDir;
		UpdateData(FALSE);
	}
}

/*
 * User pressed the exit button
 */
void CBINDInstallDlg::OnExit() 
{
	EndDialog(0);	
}

/*
 * User pressed the uninstall button.  Make it go.
 */
void CBINDInstallDlg::OnUninstall() 
{
	UpdateData();	

	if(MsgBox(IDS_UNINSTALL, MB_YESNO) == IDYES)
	{
		if(CheckBINDService())
			StopBINDService();

		HANDLE hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
		if(!hSCManager)
		{
			MsgBox(IDS_ERR_OPEN_SCM, GetErrMessage());
			return;
		}
		
		HANDLE hService = OpenService(hSCManager, BIND_SERVICE_NAME, SERVICE_ALL_ACCESS);
		if(!hService && GetLastError() != ERROR_SERVICE_DOES_NOT_EXIST)
		{
			MsgBox(IDS_ERR_OPEN_SERVICE, GetErrMessage());
			return;
		}

		SERVICE_STATUS ss;
		QueryServiceStatus(hService, &ss);
		if(ss.dwCurrentState == SERVICE_RUNNING)
		{
			BOOL rc = ControlService(hService, SERVICE_CONTROL_STOP, &ss);
			if(rc == FALSE || ss.dwCurrentState != SERVICE_STOPPED)
			{
				MsgBox(IDS_ERR_STOP_SERVICE, GetErrMessage());
				return;
			}

		}
		CloseServiceHandle(hService);
		CloseServiceHandle(hSCManager);
		
		// Directories
		m_etcDir = m_targetDir + "\\etc";
		m_binDir = m_targetDir + "\\bin";

		UninstallTags();
		UnregisterMessages(TRUE);
		UnregisterService(TRUE);
		DeleteFiles(TRUE);
		if(m_keepFiles == FALSE)
			RemoveDirs(TRUE);
		else
			GetDlgItem(IDC_CREATE_DIR)->SetWindowText("Not Removed");

		
		// Delete registry keys for named
		RegDeleteKey(HKEY_LOCAL_MACHINE, BIND_SESSION_SUBKEY);
		RegDeleteKey(HKEY_LOCAL_MACHINE, BIND_SUBKEY);
		RegDeleteKey(HKEY_LOCAL_MACHINE, BIND_UNINSTALL_SUBKEY);
	
		ProgramGroup(FALSE);

		SetCurrent(IDS_UNINSTALL_DONE);
		MsgBox(IDS_UNINSTALL_DONE);
	}
}

/*
 * User pressed the install button.  Make it go.
 */
void CBINDInstallDlg::OnInstall() 
{
	BOOL success = FALSE;

	if(CheckBINDService())
		StopBINDService();

	InstallTags();

	UpdateData();

	// Directories
	m_etcDir = m_targetDir + "\\etc";
	m_binDir = m_targetDir + "\\bin";

	if(m_defaultDir != m_targetDir)
	{
		if(GetFileAttributes(m_targetDir) != 0xFFFFFFFF)
		{
			int install = MsgBox(IDS_DIREXIST, MB_YESNO | MB_ICONQUESTION, m_targetDir);
			if(install == IDNO)
				return;
		}
		else
		{
			int createDir = MsgBox(IDS_CREATEDIR, MB_YESNO | MB_ICONQUESTION, m_targetDir);
			if(createDir == IDNO)
				return;
		}
	}

	try
	{
		CreateDirs();
 		CopyFiles();
		RegisterService();
		RegisterMessages();

		HKEY hKey;

		/* Create a new key for named */
		SetCurrent(IDS_CREATE_KEY);
		if(RegCreateKey(HKEY_LOCAL_MACHINE, BIND_SUBKEY, &hKey) == ERROR_SUCCESS)
		{
			// Get the install directory
			RegSetValueEx(hKey, "InstallDir", 0, REG_SZ, (LPBYTE)(LPCTSTR)m_targetDir, m_targetDir.GetLength());
			RegCloseKey(hKey);
		}

		
		SetCurrent(IDS_ADD_REMOVE);
		if(RegCreateKey(HKEY_LOCAL_MACHINE, BIND_UNINSTALL_SUBKEY, &hKey) == ERROR_SUCCESS)
		{
			char winDir[MAX_PATH];
			CString buf(BIND_DISPLAY_NAME);
			GetWindowsDirectory(winDir, MAX_PATH);

			RegSetValueEx(hKey, "DisplayName", 0, REG_SZ, (LPBYTE)(LPCTSTR)buf, buf.GetLength());

			buf.Format("%s\\BINDInstall.exe", winDir);
			RegSetValueEx(hKey, "UninstallString", 0, REG_SZ, (LPBYTE)(LPCTSTR)buf, buf.GetLength());
			RegCloseKey(hKey);
		}
	
		if(m_startOnInstall && !m_reboot)
			StartBINDService();
	}
	catch(Exception e)
	{
		MessageBox(e.resString);
		SetCurrent(IDS_CLEANUP);
		FailedInstall();
		MsgBox(IDS_FAIL);
		return;
	}
	catch(DWORD dw)
	{
		CString msg;
		msg.Format("A fatal error occured\n(%s)", GetErrMessage(dw));
		MessageBox(msg);
		SetCurrent(IDS_CLEANUP);
		FailedInstall();
		MsgBox(IDS_FAIL);
		return;
	}

	SetCurrent(IDS_INSTALL_DONE);
	MsgBox(IDS_SUCCESS);
	if(m_reboot)
	{
		if(MsgBox(IDS_REBOOT, MB_YESNO) == IDYES)
		{
			InitiateSystemShutdown(NULL, NULL, 0, TRUE, TRUE);
		}
	}
}

/*
 * Methods to do the work
 */
void CBINDInstallDlg::CreateDirs()
{
	/* s'OK if the directories already exist */
	SetCurrent(IDS_CREATE_DIR, m_targetDir);
	if(!CreateDirectory(m_targetDir, NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
		throw(Exception(IDS_ERR_CREATE_DIR, m_targetDir, GetErrMessage()));

	SetCurrent(IDS_CREATE_DIR, m_etcDir);
	if(!CreateDirectory(m_etcDir, NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
		throw(Exception(IDS_ERR_CREATE_DIR, m_etcDir, GetErrMessage()));

	SetCurrent(IDS_CREATE_DIR, m_binDir);
	if(!CreateDirectory(m_binDir, NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
		throw(Exception(IDS_ERR_CREATE_DIR, m_binDir, GetErrMessage()));
		
	SetItemStatus(IDC_CREATE_DIR);
}

void CBINDInstallDlg::RemoveDirs(BOOL uninstall)
{
	if(!m_keepFiles)
	{
		SetCurrent(IDS_REMOVE_DIR, m_binDir);
		// Check for existence then remove if present
		if(GetFileAttributes(m_binDir) != 0xFFFFFFFF)
			RemoveDirectory(m_binDir);

		SetCurrent(IDS_REMOVE_DIR, m_etcDir);
		if(GetFileAttributes(m_etcDir) != 0xFFFFFFFF)
			RemoveDirectory(m_etcDir);

		SetCurrent(IDS_REMOVE_DIR, m_targetDir);
		if(GetFileAttributes(m_targetDir) != 0xFFFFFFFF)
			RemoveDirectory(m_targetDir);
	}

	if(uninstall)
		SetItemStatus(IDC_CREATE_DIR, TRUE);
}

void CBINDInstallDlg::CopyFiles()
{
	CString destFile;

	for(int i = 0; installFiles[i].filename; i++)
	{
		SetCurrent(IDS_COPY_FILE, installFiles[i].filename);

		destFile = DestDir(installFiles[i].destination) + "\\" + installFiles[i].filename;
		CString filespec = m_currentDir + "\\" + installFiles[i].filename;
		CVersionInfo bindFile(destFile);		/* This file doesn't have to exist */
		
		CVersionInfo origFile(filespec);
		if(!origFile.IsValid() && installFiles[i].checkVer)
		{
			if(MsgBox(IDS_FILE_BAD, MB_YESNO, installFiles[i].filename) == IDNO)
				throw(Exception(IDS_ERR_COPY_FILE, installFiles[i].filename, GetErrMessage()));
		}
		
		try
		{
/* Ignore Version checking.  We need to make sure that all files get copied regardless
   of whether or not they are earlier or later versions since we cannot guarantee
   that we have either backward or forward compatibility between versions.
*/
			bindFile.CopyFileNoVersion(origFile);
		}
		catch(...)
		{
			if(installFiles[i].importance != FileData::Trivial)
			{
				if(installFiles[i].importance == FileData::Critical ||
					MsgBox(IDS_ERR_NONCRIT_FILE, MB_YESNO, installFiles[i].filename, GetErrMessage()) == IDNO)
				{
					SetItemStatus(IDC_COPY_FILE, FALSE);
					throw(Exception(IDS_ERR_COPY_FILE, installFiles[i].filename, GetErrMessage()));
				}
			}
		}
	}

	SetItemStatus(IDC_COPY_FILE);
}

void CBINDInstallDlg::DeleteFiles(BOOL uninstall)
{
	CString destFile;

	for(int i = 0; installFiles[i].filename; i++)
	{
		if(installFiles[i].checkVer)
			continue;

		destFile = DestDir(installFiles[i].destination) + "\\" + installFiles[i].filename;
	
		if(uninstall)
			SetCurrent(IDS_DELETE_FILE, installFiles[i].filename);
		
		DeleteFile(destFile);
	}

	if(!m_keepFiles)
	{
		WIN32_FIND_DATA findData;
		CString file = m_etcDir + "\\*.*";
		BOOL rc;
		HANDLE hFile;
			
		hFile = FindFirstFile(file, &findData);
		rc = hFile != INVALID_HANDLE_VALUE;
		
		while(rc == TRUE)
		{
			if(strcmp(findData.cFileName, ".") && strcmp(findData.cFileName, ".."))
			{
				file = m_etcDir + "\\" + findData.cFileName;
				SetCurrent(IDS_DELETE_FILE, file);
				DeleteFile(file);
			}
			rc = FindNextFile(hFile, &findData);
		}
		FindClose(hFile);
	}

	if(uninstall)
		SetItemStatus(IDC_COPY_FILE, TRUE);
}	


void CBINDInstallDlg::RegisterService()
{
	HANDLE hSCManager;
	HANDLE hService;

	SetCurrent(IDS_OPEN_SCM);
	hSCManager= OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if(!hSCManager)
		throw(Exception(IDS_ERR_OPEN_SCM, GetErrMessage()));

	DWORD dwStart = SERVICE_DEMAND_START;
	if(m_autoStart)
		dwStart = SERVICE_AUTO_START;

	DWORD dwServiceType = SERVICE_WIN32_OWN_PROCESS;

	CString namedLoc;
	namedLoc.Format("%s\\bin\\named.exe", m_targetDir);

	SetCurrent(IDS_CREATE_SERVICE);
	hService = CreateService(hSCManager, BIND_SERVICE_NAME, BIND_DISPLAY_NAME, SERVICE_ALL_ACCESS, dwServiceType, dwStart,
		SERVICE_ERROR_NORMAL, namedLoc, NULL, NULL, NULL, NULL, NULL);	
	
	if(!hService && GetLastError() != ERROR_SERVICE_EXISTS)
		throw(Exception(IDS_ERR_CREATE_SERVICE, GetErrMessage()));

	if(hSCManager)
		CloseServiceHandle(hSCManager);

	if(hService)
		CloseServiceHandle(hService);

	SetItemStatus(IDC_REG_SERVICE);
}

void CBINDInstallDlg::UnregisterService(BOOL uninstall)
{
	BOOL rc = FALSE;
	HANDLE hSCManager;
	HANDLE hService;

	while(1)
	{
		SetCurrent(IDS_OPEN_SCM);
		hSCManager= OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
		if(!hSCManager && uninstall == TRUE)
		{
			MsgBox(IDS_ERR_OPEN_SCM, GetErrMessage());
			break;
		}

		SetCurrent(IDS_OPEN_SERVICE);
		hService = OpenService(hSCManager, BIND_SERVICE_NAME, STANDARD_RIGHTS_REQUIRED);
		if(!hService && uninstall == TRUE)
		{
			if(GetLastError() != ERROR_SERVICE_DOES_NOT_EXIST)
			{
				MsgBox(IDS_ERR_OPEN_SERVICE, GetErrMessage());
				break;
			}
		}
		else
		{
			SetCurrent(IDS_REMOVE_SERVICE);
			if(!DeleteService(hService) && uninstall == TRUE)
			{
				DWORD err = GetLastError();
				if(err != ERROR_SERVICE_MARKED_FOR_DELETE && err != ERROR_SERVICE_DOES_NOT_EXIST)
				{
					MsgBox(IDS_ERR_REMOVE_SERVICE, GetErrMessage());
					break;
				}
			}
		}

		rc = TRUE;
		break;
	}

	if(hSCManager)
		CloseServiceHandle(hSCManager);

	if(hService)
		CloseServiceHandle(hService);

	if(uninstall)
		SetItemStatus(IDC_REG_SERVICE, rc);
}

void CBINDInstallDlg::RegisterMessages()
{
	HKEY hKey;
	DWORD dwData;
	char pszMsgDLL[MAX_PATH], buf[MAX_PATH];

	GetSystemDirectory(buf, MAX_PATH);
	sprintf(pszMsgDLL, "%s\\%s", buf, "bindevt.dll");

	SetCurrent(IDS_REGISTER_MESSAGES);
	/* Create a new key for named */
	if(RegCreateKey(HKEY_LOCAL_MACHINE, BIND_MESSAGE_SUBKEY, &hKey) != ERROR_SUCCESS)
		throw(Exception(IDS_ERR_CREATE_KEY, GetErrMessage()));
	
	/* Add the Event-ID message-file name to the subkey. */
	if(RegSetValueEx(hKey, "EventMessageFile", 0, REG_EXPAND_SZ, (LPBYTE)pszMsgDLL, strlen(pszMsgDLL) + 1) != ERROR_SUCCESS)
		throw(Exception(IDS_ERR_SET_VALUE, GetErrMessage()));

	/* Set the supported types flags and addit to the subkey. */
	dwData = EVENTLOG_ERROR_TYPE | EVENTLOG_WARNING_TYPE | EVENTLOG_INFORMATION_TYPE;
	if(RegSetValueEx(hKey, "TypesSupported", 0, REG_DWORD, (LPBYTE)&dwData, sizeof(DWORD)) != ERROR_SUCCESS)
		throw(Exception(IDS_ERR_SET_VALUE, GetErrMessage()));

	RegCloseKey(hKey);
			
	SetItemStatus(IDC_REG_MESSAGE);
}

void CBINDInstallDlg::UnregisterMessages(BOOL uninstall)
{
	BOOL rc = FALSE;
	HKEY hKey = NULL;

	while(1)
	{
		SetCurrent(IDS_UNREGISTER_MESSAGES);
		/* Open key for Application Event Log */
		if(RegOpenKey(HKEY_LOCAL_MACHINE, EVENTLOG_APP_SUBKEY, &hKey) != ERROR_SUCCESS)
			break;

		/* Remove named from the list of messages sources */
		if(RegDeleteKey(hKey, BIND_MESSAGE_NAME) != ERROR_SUCCESS)
			break;
		
		rc = TRUE;
		break;
	}

	if(hKey)
		RegCloseKey(hKey);

	if(uninstall)
		SetItemStatus(IDC_REG_MESSAGE, rc);
}

/*
 * Install failed - clean up quietly
 */
void CBINDInstallDlg::FailedInstall()
{
	UnregisterMessages(FALSE);
	UnregisterService(FALSE);
	DeleteFiles(FALSE);
	RemoveDirs(FALSE);
}

/*
 * Set the checklist tags for install
 */
void CBINDInstallDlg::InstallTags()
{
	CString tag;
	
	tag.LoadString(IDS_INSTALL_FILE);
	GetDlgItem(IDC_COPY_TAG)->SetWindowText(tag);
	GetDlgItem(IDC_COPY_FILE)->SetWindowText("");

	tag.LoadString(IDS_INSTALL_DIR);
	GetDlgItem(IDC_DIR_TAG)->SetWindowText(tag);
	GetDlgItem(IDC_CREATE_DIR)->SetWindowText("");
	GetDlgItem(IDC_REG_SERVICE)->SetWindowText("");

	tag.LoadString(IDS_INSTALL_SERVICE);
	GetDlgItem(IDC_SERVICE_TAG)->SetWindowText(tag);
	
	tag.LoadString(IDS_INSTALL_MESSAGE);
	GetDlgItem(IDC_MESSAGE_TAG)->SetWindowText(tag);
	GetDlgItem(IDC_REG_MESSAGE)->SetWindowText("");
}

/*
 * Set the checklist tags for uninstall
 */
void CBINDInstallDlg::UninstallTags()
{
	CString tag;
	
	tag.LoadString(IDS_UNINSTALL_FILES);
	GetDlgItem(IDC_COPY_TAG)->SetWindowText(tag);
	GetDlgItem(IDC_COPY_FILE)->SetWindowText("");

	tag.LoadString(IDS_UNINSTALL_DIR);
	GetDlgItem(IDC_DIR_TAG)->SetWindowText(tag);
	GetDlgItem(IDC_CREATE_DIR)->SetWindowText("");

	tag.LoadString(IDS_UNINSTALL_SERVICE);
	GetDlgItem(IDC_SERVICE_TAG)->SetWindowText(tag);
	GetDlgItem(IDC_REG_SERVICE)->SetWindowText("");
	
	tag.LoadString(IDS_UNINSTALL_MESSAGE);
	GetDlgItem(IDC_MESSAGE_TAG)->SetWindowText(tag);
	GetDlgItem(IDC_REG_MESSAGE)->SetWindowText("");
}

void CBINDInstallDlg::SetItemStatus(UINT nID, BOOL bSuccess)
{
	GetDlgItem(nID)->SetWindowText(bSuccess == TRUE ? "Done" : "Failed");
}


/*
 * Set the text in the current operation field - use a string table string
 */
void CBINDInstallDlg::SetCurrent(int id, ...)
{
	CString format;
	va_list va;
	char buf[128];

	format.LoadString(id);
	memset(buf, 0, 128);

	va_start(va, id);
	vsprintf(buf, format, va);
	va_end(va);
	
	m_current.Format("%s", buf);
	UpdateData(FALSE);
}

/*
 * Stop the BIND service
 */
void CBINDInstallDlg::StopBINDService()
{
	SERVICE_STATUS svcStatus;
	
	SetCurrent(IDS_STOP_SERVICE);

	HANDLE hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if(!hSCManager)
	{
		MsgBox(IDS_ERR_OPEN_SCM, GetErrMessage());
	}
	
	HANDLE hBINDSvc = OpenService(hSCManager, BIND_SERVICE_NAME, SERVICE_ALL_ACCESS);
	if(!hBINDSvc)
	{
		MsgBox(IDS_ERR_OPEN_SERVICE, GetErrMessage());
	}
	
	BOOL rc = ControlService(hBINDSvc, SERVICE_CONTROL_STOP, &svcStatus);
}

/*
 * Start the BIND service
 */
void CBINDInstallDlg::StartBINDService()
{
	SetCurrent(IDS_START_SERVICE);

	HANDLE hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if(!hSCManager)
	{
		MsgBox(IDS_ERR_OPEN_SCM, GetErrMessage());
	}
	
	HANDLE hBINDSvc = OpenService(hSCManager, BIND_SERVICE_NAME, SERVICE_ALL_ACCESS);
	if(!hBINDSvc)
	{
		MsgBox(IDS_ERR_OPEN_SERVICE, GetErrMessage());
	}
	BOOL rc = StartService(hBINDSvc, 0, NULL);
}

/*
 * Check to see if the BIND service is running or not
 */
BOOL CBINDInstallDlg::CheckBINDService()
{
	SERVICE_STATUS svcStatus;

	HANDLE hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if(hSCManager)
	{
		HANDLE hBINDSvc = OpenService(hSCManager, BIND_SERVICE_NAME, SERVICE_ALL_ACCESS);
		if(hBINDSvc)
		{
			BOOL rc = ControlService(hBINDSvc, SERVICE_CONTROL_INTERROGATE, &svcStatus);
			if(!rc)
				DWORD err = GetLastError();

			return(svcStatus.dwCurrentState == SERVICE_RUNNING);
		}
	}
	return(FALSE);
}

/*
 * Display message boxes with variable args, using string table strings for the format specifiers 
 */
int CBINDInstallDlg::MsgBox(int id, ...)
{
	CString format;
	va_list va;
	char buf[BUFSIZ];

	format.LoadString(id);
	memset(buf, 0, BUFSIZ);

	va_start(va, id);
	vsprintf(buf, format, va);
	va_end(va);

	return(MessageBox(buf));
}

int CBINDInstallDlg::MsgBox(int id, UINT type, ...)
{
	CString format;
	va_list va;
	char buf[BUFSIZ];

	format.LoadString(id);
	memset(buf, 0, BUFSIZ);

	va_start(va, type);
	vsprintf(buf, format, va);
	va_end(va);

	return(MessageBox(buf, NULL, type));
}

/*
 * Call GetLastError(), retrieve the message associated with the error
 */
CString CBINDInstallDlg::GetErrMessage(DWORD err)
{	
	LPVOID msgBuf;
	static char buf[BUFSIZ];
	
	DWORD len = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, err == -1 ? GetLastError() : err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR) &msgBuf, 0, NULL );


	strcpy(buf, (LPTSTR)msgBuf);
	LocalFree(msgBuf);
	/* Strip off the period and the \n */
	buf[len - 3] = 0;
	return(buf);
}

void CBINDInstallDlg::ProgramGroup(BOOL create)
{
	TCHAR path[MAX_PATH], commonPath[MAX_PATH], fileloc[MAX_PATH], linkpath[MAX_PATH];
	HRESULT hres; 
	IShellLink *psl = NULL; 
	LPMALLOC pMalloc = NULL;
	ITEMIDLIST *itemList = NULL;

	HRESULT hr = SHGetMalloc(&pMalloc);
	if(hr != NOERROR)
	{
		MessageBox("Could not get a handle to Shell memory object");
		return;
	}

	hr = SHGetSpecialFolderLocation(m_hWnd, CSIDL_COMMON_PROGRAMS, &itemList);
	if(hr != NOERROR)
	{
		MessageBox("Could not get a handle to the Common Programs folder");
		if(itemList)
		{
			pMalloc->Free(itemList);
		}
		return;
	}
	
	hr = SHGetPathFromIDList(itemList, commonPath);
	pMalloc->Free(itemList);

	if(create)
	{
		sprintf(path, "%s\\ISC", commonPath);
		CreateDirectory(path, NULL);
		
		sprintf(path, "%s\\ISC\\BIND", commonPath);
		CreateDirectory(path, NULL);

		hres = CoInitialize(NULL);

		if (SUCCEEDED(hres))
		{ 
			// Get a pointer to the IShellLink interface. 
			hres = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (LPVOID *)&psl); 
			if (SUCCEEDED(hres))
			{ 
				IPersistFile* ppf; 
				sprintf(linkpath, "%s\\BINDCtrl.lnk", path);
				sprintf(fileloc, "%s\\BINDCtrl.exe", m_binDir);
			
				psl->SetPath(fileloc); 
				psl->SetDescription("BIND Control Panel"); 

				hres = psl->QueryInterface(IID_IPersistFile, (void **)&ppf); 
				if (SUCCEEDED(hres))
				{ 
					WCHAR wsz[MAX_PATH]; 

					MultiByteToWideChar(CP_ACP, 0, linkpath, -1, wsz, MAX_PATH); 
					hres = ppf->Save(wsz, TRUE); 
					ppf->Release(); 
				} 

				if(GetFileAttributes("readme.txt") != -1)
				{
					sprintf(fileloc, "%s\\Readme.txt", m_targetDir);
					sprintf(linkpath, "%s\\Readme.lnk", path);

					psl->SetPath(fileloc); 
					psl->SetDescription("BIND Readme"); 

					hres = psl->QueryInterface(IID_IPersistFile, (void **)&ppf); 
					if (SUCCEEDED(hres))
					{ 
						WCHAR wsz[MAX_PATH]; 

						MultiByteToWideChar(CP_ACP, 0, linkpath, -1, wsz, MAX_PATH); 
						hres = ppf->Save(wsz, TRUE); 
						ppf->Release(); 
					} 
					psl->Release(); 
				}
			}
			CoUninitialize();
		} 
	}
	else
	{
		TCHAR filename[MAX_PATH];
		WIN32_FIND_DATA fd;

		sprintf(path, "%s\\ISC\\BIND", commonPath);

		sprintf(filename, "%s\\*.*", path);
		HANDLE hFind = FindFirstFile(filename, &fd);
		if(hFind != INVALID_HANDLE_VALUE)
		{
			do
			{
				if(strcmp(fd.cFileName, ".") && strcmp(fd.cFileName, ".."))
				{
					sprintf(filename, "%s\\%s", path, fd.cFileName);
					DeleteFile(filename);
				}
			} while(FindNextFile(hFind, &fd));
			FindClose(hFind);
		}
		RemoveDirectory(path);
		sprintf(path, "%s\\ISC", commonPath);
		RemoveDirectory(path);
	}
}

CString CBINDInstallDlg::DestDir(int destination)
{
	switch(destination)
	{
		case FileData::TargetDir:
			return m_targetDir;
		case FileData::BinDir:
			return m_binDir;
		case FileData::EtcDir:
			return m_etcDir;
		case FileData::WinSystem:
			return m_winSysDir;
	}
	return("");
}
