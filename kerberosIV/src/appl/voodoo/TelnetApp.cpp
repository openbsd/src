/*
 * Copyright (c) 1995, 1996, 1997 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* $KTH: TelnetApp.cpp,v 1.7 1999/12/02 16:58:34 joda Exp $ */

// TelnetApp.cpp
// Author: Jörgen Karlsson - d93-jka@nada.kth.se

#include <Windows.h>
#include <winsock.h>
#include "TelnetApp.h"
#include "TelnetEngine.h"
#include "TerminalEngine.h"
#include "TelnetSession.h"
#include "Resource.h"

#include "KClient.h"

TelnetApp *theApp;
TelnetSession *theSession;
HANDLE theEvent[1];
int EventCount;

const int hostnameSZ = 64;
char hostname[hostnameSZ] = "";
char username[64] = "";
int  telnet_port;
char telnet_port_str[64] = "";

BOOL CALLBACK
host_dialog_proc(HWND  hwndDlg, UINT  uMsg, WPARAM  wParam, LPARAM  lParam)
{
    switch(uMsg) {
    case WM_INITDIALOG:
	SetDlgItemText(hwndDlg, IDC_EDIT1, hostname);
	SetDlgItemText(hwndDlg, IDC_EDIT2, username);
	SetDlgItemText(hwndDlg, IDC_EDIT3, "telnet");
	return TRUE;
	break;
    case WM_COMMAND: 
	switch(wParam) {
	case IDOK:
	    if(!GetDlgItemText(hwndDlg,IDC_EDIT1,
			       hostname ,hostnameSZ))
		EndDialog(hwndDlg, IDCANCEL);
	    GetDlgItemText(hwndDlg, IDC_EDIT2,
			   username, sizeof(username));
	    GetDlgItemText(hwndDlg, IDC_EDIT3,
			   telnet_port_str, sizeof(telnet_port_str));
	case IDCANCEL:
	    EndDialog(hwndDlg, wParam);
	    return TRUE;
	    break;
	}
    }
    return FALSE;
}


int WINAPI
WinMain(HINSTANCE  hInst, HINSTANCE  hPrevInst,
	LPSTR  CmdLine, int  nShowCmd)
{
    MSG  msg;
    TelnetApp app(hInst, CmdLine);

    while(TRUE) {
	GetMessage(&msg, 0,0,0);
	switch(msg.message) {
	case WM_QUIT:
	    return msg.wParam;

	case WM_ENDSESSION:
	    theApp->CloseAllSessions();
	    return 0;
	    break;

	default:
	    TranslateMessage(&msg); 
	    DispatchMessage(&msg);
	}
    }
}


TelnetApp::TelnetApp(HINSTANCE hInst, char *hostname)
{
    WORD version;	
    WSADATA data; 
    TelnetSession *session;

    AppInstance = hInst; 
    theApp = this;
    version = MAKEWORD(1, 1); 
    if(WSAStartup(version, &data) != 0)
	exit(1);

    session = new TelnetSession();

    KClientGetUserName(username);

    HKEY key;

    RegCreateKeyEx(HKEY_CURRENT_USER,
		   "voodoo",
		   0,
		   "hostname",
		   REG_OPTION_NON_VOLATILE,
		   KEY_ALL_ACCESS,
		   NULL,
		   &key,
		   NULL);

    char rhost[64];
    if(*hostname == '\0') {
	DWORD pcbData = sizeof(rhost);

	if(RegQueryValueEx(key,
			   "hostname",
			   0,
			   NULL,
			   (unsigned char*)rhost,
			   &pcbData) == 0) {
	    hostname = rhost;
	}
	hostname = GetHostName(hostname);
    }

    if(hostname != NULL) {
	strncpy(rhost, hostname, sizeof(rhost));
	rhost[sizeof(rhost) - 1] = 0;
	hostname = rhost;
	RegSetValueEx(key,
		      "hostname",
		      0,
		      REG_SZ,
		      (unsigned char *)hostname,
		      strlen(hostname) + 1);

	session = new TelnetSession();
	if(session->Connect(hostname, username))
	    theSession = session;		
    } else
	SessionClosed(0);
    RegCloseKey(key);
}	
	
TelnetApp::~TelnetApp(void)
{
    if(WSACleanup() == SOCKET_ERROR)
	exit(1);
}

void TelnetApp::Error(int ErrorCode, char *ErrorString)
{
    Message(ErrorString);
    exit(ErrorCode);
}

void TelnetApp::SessionClosed(TelnetSession *thisTelnetSession)
{
	PostQuitMessage(0);
}

char* TelnetApp::GetHostName(char* defaultHost)
{
    PHOSTENT host;

    strcpy(hostname, defaultHost);
askforhost:
    HWND wnd = GetActiveWindow();
    HANDLE hInst = theApp->AppInstance;
    switch(DialogBox(hInst,MAKEINTRESOURCE(IDD_DIALOG1),wnd,(DLGPROC)host_dialog_proc)) {
    case IDOK:
	if(!(host = gethostbyname(hostname))) {
	    TelnetApp::Message("Unknown host.");
	    goto askforhost;
	}
	return host->h_name;
    case IDCANCEL:
    default:
	return NULL;
    }
}


void TelnetApp::RegisterRecEvent(HANDLE RecEvent)
{
    theEvent[0] = RecEvent;
    EventCount = 1;
}

void TelnetApp::CloseAllSessions(void)
{
    theSession->Close();
}

void TelnetApp::Message(char *text)
{
    HWND wnd = GetActiveWindow();
    MessageBox(wnd, text, "Voodoo message", MB_ICONERROR|MB_OK|MB_APPLMODAL);
}
