//
//	KrbManager
//

#include <windows.h>
#include <commctrl.h>
#include <winsock.h>
#include "krb.h"
#include "afxres.h"
#include "resource.h"

#define MAINWNDCLASS	"KrbManagerWndClass"
#define WINDOWNAME		"KrbManager"
#define NOTICKETCUE 	"(No tickets)"
#define MUTEXNAME		"KrbManagerAlreadyRunning"
#define UPDATEMESSAGE	"krb4-update-cache"
#define ID_LISTVIEW 	2000

HWND		hwndMain, hWndListView, hWndImageList;
HACCEL		haccl;
HINSTANCE	hInst;
static UINT update_cache;
static int	sortstate;

typedef struct _LV_TINFO { 
	char	start[64];
	char	end[64];
	char	principal[64];
	char	kvno[32];
} LV_TINFO;

static int InitApplication(int);
LRESULT CALLBACK MainWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK About(HWND hDlg, UINT message, UINT wParam, LONG lParam);
int CALLBACK CompFunc(LPARAM, LPARAM, LPARAM);
static void NotifyHandler(LPARAM);
static void SetTitle(char *);
static void AddColumn(int, LPSTR, int);
static int CreateListView(HWND);
static void UpdateCacheList();


int WINAPI
WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	MSG 	msg;
	HANDLE	hMutex;

	hMutex = CreateMutex(NULL, FALSE, MUTEXNAME);
#ifndef _DEBUG
	if (GetLastError() == ERROR_ALREADY_EXISTS)
	{
		ReleaseMutex(hMutex);
		return 0;
	}
#endif
	hInst = hInstance;

	if (!InitApplication(nCmdShow))
	{
		MessageBox(NULL, "Internal application error", WINDOWNAME, 16);
		ReleaseMutex(hMutex);
		return 0;
	}

	while (GetMessage(&msg, (HWND) NULL, 0, 0))
	{
		if (TranslateAccelerator(hwndMain, haccl, &msg))
			continue;
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	ReleaseMutex(hMutex);
	return msg.wParam;
}

static int
InitApplication(int nCmdShow)
{
	WNDCLASS	wc;
	RECT		rect;

	update_cache = RegisterWindowMessage(UPDATEMESSAGE);

	wc.style = (int)NULL;
	wc.lpfnWndProc = MainWndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInst;
	wc.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDR_MAINFRAME));
	wc.hCursor = NULL;
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wc.lpszMenuName = (HMENU)IDR_MAINFRAME;
	wc.lpszClassName = MAINWNDCLASS;

	if (!RegisterClass(&wc))
		return 0;

	SystemParametersInfo(SPI_GETWORKAREA, 0, &rect, 0);
	hwndMain = CreateWindowEx(WS_EX_CLIENTEDGE, MAINWNDCLASS, NOTICKETCUE " - " WINDOWNAME,
							  WS_OVERLAPPEDWINDOW, rect.left + 5, rect.bottom - 205, 600, 200,
							  HWND_DESKTOP, NULL, hInst, NULL);
	if (hwndMain == NULL)
		return 0;

	haccl = LoadAccelerators(hInst, MAKEINTRESOURCE(IDR_MAINFRAME));
	if (haccl == NULL)
		return 0;

	ShowWindow(hwndMain, nCmdShow);
	UpdateWindow(hwndMain);
	
	return TRUE;
}

LRESULT CALLBACK
MainWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	HMENU	hMenu;
	
	if (message == update_cache)
	{
		UpdateCacheList();
		return 0;
	}

	switch (message) 
	{
	case WM_CREATE:
		if (!CreateListView(hWnd))
			return (-1);
		break;

	case WM_NOTIFY:
		if (wParam == ID_LISTVIEW)
			NotifyHandler(lParam);
		break;

	case WM_ACTIVATE:
		UpdateCacheList();
		break;

	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
		case ID_FILE_DESTROY:
			dest_tkt();
			UpdateCacheList();
			break;

		/*case ID_FILE_LOGIN:
			break;
		case ID_EDIT_UNDO:
			break;
		case ID_EDIT_CUT:
			break;
		case ID_EDIT_COPY:
			break;
		case ID_EDIT_PASTE:
			break;*/

		case ID_APP_ABOUT:
			DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, (DLGPROC)About);
			break;

		case ID_APP_EXIT:
			PostMessage(hWnd, WM_CLOSE, 0, 0);
			break;

		default:
			return (DefWindowProc(hWnd, message, wParam, lParam));
		}
		break;

	case WM_INITMENU:
		hMenu = (HMENU)wParam;
		EnableMenuItem(hMenu, ID_EDIT_UNDO, MF_BYCOMMAND|MF_GRAYED);
		EnableMenuItem(hMenu, ID_EDIT_CUT, MF_BYCOMMAND|MF_GRAYED);
		EnableMenuItem(hMenu, ID_EDIT_COPY, MF_BYCOMMAND|MF_GRAYED);
		EnableMenuItem(hMenu, ID_EDIT_PASTE, MF_BYCOMMAND|MF_GRAYED);
		break;

	case WM_SIZE:
		MoveWindow(hWndListView, 0, 0, LOWORD(lParam),HIWORD(lParam),TRUE);
		break;

	case WM_DESTROY:
		if (hWndListView)
			DestroyWindow(hWndListView);
		if (hWndImageList)
			DestroyWindow(hWndImageList);
		PostQuitMessage(0);
		break;

	default:
		return (DefWindowProc(hWnd, message, wParam, lParam));
	}
	return 0;
}

LRESULT CALLBACK
About(HWND hDlg, UINT message, UINT wParam, LONG lParam)
{
	switch (message)
	{
	case WM_INITDIALOG:
		return TRUE;

	case WM_COMMAND:			  
		if (LOWORD(wParam) == IDOK)
			EndDialog(hDlg, TRUE);
		break;
	}
	return 0;	
}

int CALLBACK
CompFunc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
	int 		res;
	LV_TINFO	*tinfo1;
	LV_TINFO	*tinfo2;

	if (sortstate)
	{
		tinfo1 = (LV_TINFO *)lParam1;
		tinfo2 = (LV_TINFO *)lParam2;
	}
	else
	{
		tinfo2 = (LV_TINFO *)lParam1;
		tinfo1 = (LV_TINFO *)lParam2;
	}

	switch(lParamSort)
	{
	case 1:
		res = lstrcmpi(tinfo1->start, tinfo2->start);
		break;

	case 2:
		res = lstrcmpi(tinfo1->end, tinfo2->end);
		break;

	case 3:
		res = lstrcmpi(tinfo1->principal, tinfo2->principal);
		break;

	case 4:
		res = lstrcmpi(tinfo1->kvno, tinfo2->kvno);
		break;

	default:
		res = 0;
		break;
	}
	return res;
}

static void
NotifyHandler(LPARAM lParam)
{
	LV_DISPINFO *pLvdi = (LV_DISPINFO *)lParam;
	NM_LISTVIEW *pNm = (NM_LISTVIEW *)lParam;
	LV_TINFO *tinfo = (LV_TINFO *)pLvdi->item.lParam;

	switch(pLvdi->hdr.code)
	{
	case LVN_GETDISPINFO:
		switch (pLvdi->item.iSubItem)
		{
		case 1:
			strcpy(pLvdi->item.pszText, tinfo->start);
			break;

		case 2:
			strcpy(pLvdi->item.pszText, tinfo->end);
			break;

		case 3:
			strcpy(pLvdi->item.pszText, tinfo->principal);
			break;

		case 4:
			strcpy(pLvdi->item.pszText, tinfo->kvno);
			break;
		}
		break;

	case LVN_DELETEITEM:
		free((void *)pNm->lParam);
		break;
	case LVN_COLUMNCLICK:
		ListView_SortItems(pNm->hdr.hwndFrom, CompFunc, (LPARAM)(pNm->iSubItem));
		sortstate = !sortstate;
	}
}

static void
SetTitle(char *txt)
{
	char txt2[100];
	_snprintf(txt2, sizeof(txt2), "%s - " WINDOWNAME, txt);

	SetWindowText(hwndMain, txt2); 
}

static void
AddColumn(int index, LPSTR plsz, int width)
{
	LV_COLUMN col;
	col.mask = LVCF_TEXT | LVCF_WIDTH;
	col.fmt = LVCFMT_LEFT;
	if(width == -1)
		col.cx = ListView_GetStringWidth(hWndListView, plsz) + 15;
	else
		col.cx = width;
	col.pszText = plsz;
	col.cchTextMax = 0;
	col.iSubItem = index > 0 ? index : -1;
	if(col.iSubItem != -1)
		col.mask |= LVCF_SUBITEM;

	ListView_InsertColumn(hWndListView, index, &col);
}

static int
CreateListView(HWND hWnd) 
{
	RECT rcl;
	char time_str[64];
	time_t t = 0;
	int time_width;
	struct tm *tm = localtime(&t);

	strftime(time_str, sizeof(time_str), "%c", tm);

	InitCommonControls();

	GetClientRect(hWnd, &rcl);
	hWndListView = CreateWindowEx(0L, WC_LISTVIEW, "", WS_VISIBLE|WS_CHILD|LVS_REPORT,
								  0, 0, rcl.right - rcl.left, rcl.bottom - rcl.top,
								  hWnd, (HMENU)ID_LISTVIEW, hInst, NULL);
	if (hWndListView == NULL)
		return 0;

	time_width = ListView_GetStringWidth(hWndListView, time_str) + 15;
	AddColumn(0, "  ", -1);
	AddColumn(1, "Start time", time_width);
	AddColumn(2, "End time", time_width);
	AddColumn(3, "Principal", ListView_GetStringWidth(hWndListView, "krbtgt.XXXXXXXXXXXXX@XXXXXXXXXXXXX") + 15);
	AddColumn(4, "Kvno", -1);

	hWndImageList = ImageList_LoadBitmap(hInst, MAKEINTRESOURCE(IDB_BITMAP1), 16, 0, 0xffffff);
	if (hWndImageList == NULL)
		return 0;
	ListView_SetImageList(hWndListView, hWndImageList, LVSIL_SMALL);
	return TRUE;
}

static void
UpdateCacheList()
{
	int 		ret;
	int 		num = 0;
	CREDENTIALS c;
	LV_ITEM 	item;
	char		pname[ANAME_SZ], pinst[INST_SZ], prealm[REALM_SZ];
	struct tm  *tm;
	time_t		now, end;
	LV_TINFO   *tinfo;

	ListView_DeleteAllItems(hWndListView);

	time(&now);
	
	if (tf_init(NULL, R_TKT_FIL)
		|| tf_get_pname(pname)
		|| tf_get_pinst(pinst))
	{
		SetTitle(NOTICKETCUE);
		return;
	}

	while((ret = tf_get_cred(&c)) == KSUCCESS){
		if(num == 0)
			strcpy(prealm, c.realm);
		end = krb_life_to_time(c.issue_date, c.lifetime);
		item.mask = LVIF_TEXT|LVIF_IMAGE|LVIF_PARAM|LVIF_STATE;
		item.iItem = num;
		item.iSubItem = 0;
		item.state = 0;
		item.stateMask = 0;
		item.iImage = (now > end ? 1 : 0);
		item.lParam = (long)malloc(sizeof(LV_TINFO));
		tinfo = (LV_TINFO *)item.lParam;
		ListView_InsertItem(hWndListView, &item);

		tm = localtime((time_t*)&c.issue_date);
		strftime(tinfo->start, sizeof(tinfo->start), "%c", tm);
		item.iSubItem = 1;
		ListView_SetItem(hWndListView, &item);
		tm = localtime(&end);
		strftime(tinfo->end, sizeof(tinfo->end), "%c", tm);
		item.iSubItem = 2;
		ListView_SetItem(hWndListView, &item);
		item.iSubItem = 3;
		strcpy(tinfo->principal, krb_unparse_name_long(c.service, c.instance, c.realm));
		ListView_SetItem(hWndListView, &item);
		item.iSubItem = 4;
		_snprintf(tinfo->kvno, sizeof(tinfo->kvno), "%u", c.kvno);
		ListView_SetItem(hWndListView, &item);
		num++;
	}
	SetTitle(krb_unparse_name_long(pname, pinst, prealm));
}
