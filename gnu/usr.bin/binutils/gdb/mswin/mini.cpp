// mini.cpp : implementation file
//

#include "stdafx.h"
#ifdef __MFC4__
    typedef struct * REFIID; 	// from wtypes.h
    #include <../src/afximpl.h>	/* FIXME: private header */
    struct _RTL_CRITICAL_SECTION _afxCS;
    struct _RTL_CRITICAL_SECTION* _afxCriticalSection=&_afxCS;
    //#define _afxCriticalSection CriticalSection	//in winuser...
    #define WS_EX_SMCAPTION WS_EX_MDICHILD	//???
#else
    #include <../src/afximpl.h>	
#endif

 
#ifdef _DEBUG 
#undef THIS_FILE 
static char THIS_FILE[] = __FILE__; 
#endif 

IMPLEMENT_DYNCREATE(CMiniMDIChildWnd, CMDIChildWnd) 
 
///////////////////////////////////////////////////////////////////////////// 
// CMiniMDIFrameWnd global data 
 
static HBITMAP hbmMiniSys; 
static HFONT hfontMiniSys; 
static SIZE sizeMiniSys; 

///////////////////////////////////////////////////////////////////////////// 
// CMiniMDIChildWnd 
 
BEGIN_MESSAGE_MAP(CMiniMDIChildWnd, CMDIChildWnd) 
	//{{AFX_MSG_MAP(CMiniMDIChildWnd) 
	ON_WM_NCPAINT() 
	ON_WM_NCACTIVATE() 
	ON_WM_NCCALCSIZE() 
	ON_WM_NCHITTEST() 
	ON_WM_NCLBUTTONDOWN() 
	ON_WM_MOUSEMOVE() 
	ON_WM_LBUTTONUP() 
	ON_WM_SYSCOMMAND() 
	ON_WM_GETMINMAXINFO() 
	ON_WM_NCCREATE() 
	ON_MESSAGE(WM_GETTEXT, OnGetText) 
	ON_MESSAGE(WM_GETTEXTLENGTH, OnGetTextLength) 
	ON_MESSAGE(WM_SETTEXT, OnSetText) 
	//}}AFX_MSG_MAP 
END_MESSAGE_MAP() 
 
#define new DEBUG_NEW 
 
///////////////////////////////////////////////////////////////////////////// 
// CMiniMDIChildWnd constructors 
 
CMiniMDIChildWnd::CMiniMDIChildWnd() 
{ 
	m_bActive = FALSE; 
 
	// rest of initialization not required if running latest Windows 
	if (afxData.bSmCaption) 
		return; 
 
	EnterCriticalSection(_afxCriticalSection); 
	if (hbmMiniSys == NULL) 
	{ 
		HINSTANCE hInst = AfxFindResourceHandle( 
			MAKEINTRESOURCE(AFX_IDB_MINIFRAME_MENU), RT_BITMAP); 
		VERIFY(hbmMiniSys = 
			LoadBitmap(hInst, MAKEINTRESOURCE(AFX_IDB_MINIFRAME_MENU))); 
		BITMAP bmStruct; 
		if (::GetObject(hbmMiniSys, sizeof(BITMAP), &bmStruct)) 
		{ 
			sizeMiniSys.cx = bmStruct.bmWidth; 
			sizeMiniSys.cy = bmStruct.bmHeight; 
		} 
	} 
	if (hfontMiniSys == NULL) 
	{ 
		LOGFONT logFont; memset(&logFont, 0, sizeof(LOGFONT)); 
		logFont.lfHeight = -(sizeMiniSys.cy-1); 
		logFont.lfCharSet = DEFAULT_CHARSET; 
		logFont.lfWeight = FW_NORMAL; 
		if (GetSystemMetrics(SM_DBCSENABLED)) 
			lstrcpy(logFont.lfFaceName, _T("Terminal")); 
		else 
			lstrcpy(logFont.lfFaceName, _T("Small Fonts")); 
		if (!AfxCustomLogFont(AFX_IDS_MINI_FONT, &logFont)) 
			logFont.lfPitchAndFamily = DEFAULT_PITCH | FF_SWISS; 
		hfontMiniSys = ::CreateFontIndirect(&logFont); 
	} 
 
	if (hfontMiniSys != NULL) 
	{ 
		CClientDC dc(NULL); 
		TEXTMETRIC tm; 
		HFONT hFontOld = (HFONT)dc.SelectObject(hfontMiniSys); 
		BOOL bResult = dc.GetTextMetrics(&tm); 
		dc.SelectObject(hFontOld); 
 
		if (!bResult || tm.tmHeight - tm.tmInternalLeading > sizeMiniSys.cy) 
			AfxDeleteObject((HGDIOBJ*)&hfontMiniSys); 
	} 
	LeaveCriticalSection(_afxCriticalSection); 
} 
 
CMiniMDIChildWnd::~CMiniMDIChildWnd() 
{ 
	DestroyWindow(); 
} 
 
BOOL CMiniMDIChildWnd::Create(LPCTSTR lpClassName, LPCTSTR lpszWindowName, 
	DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID) 
{ 
	m_strCaption = lpszWindowName; 
	return CreateEx(0, lpClassName ? lpClassName : 
		AfxRegisterWndClass(CS_DBLCLKS, ::LoadCursor(NULL, IDC_ARROW)), 
		lpszWindowName, dwStyle, rect.left, rect.top, rect.right - rect.left, 
		rect.bottom - rect.top, pParentWnd->GetSafeHwnd(), (HMENU)nID); 
} 
 
///////////////////////////////////////////////////////////////////////////// 
// CMiniMDIChildWnd message handlers 
 
BOOL CMiniMDIChildWnd::OnNcCreate(LPCREATESTRUCT lpcs) 
{ 
	if (!CMDIChildWnd::OnNcCreate(lpcs)) 
		return FALSE; 
 
	if (GetStyle() & MFS_SYNCACTIVE) 
	{ 
		// synchronize activation state with top level parent 
		CWnd* pParentWnd = GetTopLevelParent(); 
		ASSERT(pParentWnd != NULL); 
		CWnd* pActiveWnd = GetForegroundWindow(); 
		BOOL bActive = (pParentWnd == pActiveWnd) || 
			(pParentWnd->GetLastActivePopup() == pActiveWnd);

	} 
 
	return TRUE; 
} 
 
BOOL CMiniMDIChildWnd::PreCreateWindow(CREATESTRUCT& cs) 
{ 
	if (afxData.bSmCaption) 
	{ 
		// WS_4THICKFRAME and MFS_THICKFRAME imply WS_THICKFRAME 
		if (cs.style & (MFS_4THICKFRAME | MFS_THICKFRAME)) 
			cs.style |= WS_THICKFRAME; 
 
		// WS_CAPTION implies WS_EX_SMCAPTION 
		if (cs.style & WS_CAPTION) 
			cs.dwExStyle |= WS_EX_SMCAPTION; 
	} 
 
	VERIFY(CMDIChildWnd::PreCreateWindow(cs)); 
	cs.dwExStyle &= ~(WS_EX_CLIENTEDGE); 
 
	return TRUE; 
} 
 
void CMiniMDIChildWnd::OnGetMinMaxInfo(MINMAXINFO* pMMI) 
{ 
	// allow Windows to fill in the defaults 
	CMDIChildWnd::OnGetMinMaxInfo(pMMI); 
 
	// don't allow sizing smaller than the non-client area 
	CRect rectWindow, rectClient; 
	GetWindowRect(rectWindow); 
	GetClientRect(rectClient); 
	pMMI->ptMinTrackSize.x = rectWindow.Width() - rectClient.right; 
	pMMI->ptMinTrackSize.y = rectWindow.Height() - rectClient.bottom; 
} 
 
BOOL CMiniMDIChildWnd::OnNcActivate(BOOL bActive) 
{ 
	if ((GetStyle() & MFS_SYNCACTIVE) == 0) 
	{ 
		if (afxData.bSmCaption) 
			return Default(); 
 
		if (m_bActive != bActive) 
		{ 
			m_bActive = bActive; 
			SendMessage(WM_NCPAINT); 
		} 
	} 
	return TRUE; 
} 
 
void CMiniMDIChildWnd::OnNcCalcSize(BOOL, NCCALCSIZE_PARAMS *lpParams) 
{ 
	if (afxData.bSmCaption) 
	{ 
		Default(); 
		return; 
	} 
 
	// Modify the first rectangle in rgrc. 
	LONG dwStyle = GetStyle(); 
	if (dwStyle & (MFS_4THICKFRAME | MFS_THICKFRAME | WS_THICKFRAME)) 
	{ 
		::InflateRect(lpParams->rgrc, 
			-GetSystemMetrics(SM_CXFRAME), -GetSystemMetrics(SM_CYFRAME)); 
	} 
	else 
	{ 
		::InflateRect(lpParams->rgrc, 
			-GetSystemMetrics(SM_CXBORDER), -GetSystemMetrics(SM_CYBORDER)); 
	} 
	if (dwStyle & WS_CAPTION) 
		lpParams->rgrc[0].top += sizeMiniSys.cy; 
} 
 
UINT CMiniMDIChildWnd::OnNcHitTest(CPoint point) 
{ 
	DWORD dwStyle = GetStyle(); 
	CRect rectWindow; 
	GetWindowRect(&rectWindow); 
 
	CSize sizeFrame(GetSystemMetrics(SM_CXFRAME), 
		GetSystemMetrics(SM_CYFRAME)); 
 
	if (afxData.bSmCaption) 
	{ 
		UINT nHit = CMDIChildWnd::OnNcHitTest(point); 
 
		// MFS_BLOCKSYSMENU translates system menu hit to caption hit 
		if (afxData.bWin4 && (dwStyle & MFS_BLOCKSYSMENU)) 
		{ 
			if (nHit == HTSYSMENU) 
				nHit = HTCAPTION; 
			if (GetKeyState(VK_RBUTTON) < 0) 
				return HTNOWHERE; 
		} 
 
		if ((nHit < HTSIZEFIRST || nHit > HTSIZELAST) && nHit != HTGROWBOX) 
			return nHit; 
 
		// MFS_MOVEFRAME translates all size requests to move requests 
		if (dwStyle & MFS_MOVEFRAME) 
			return HTCAPTION; 
 
		// MFS_4THICKFRAME does not allow diagonal sizing 
		rectWindow.InflateRect(-sizeFrame.cx, -sizeFrame.cy); 
		if (dwStyle & MFS_4THICKFRAME) 
		{ 
			switch (nHit) 
			{ 
			case HTTOPLEFT: 
				return point.y < rectWindow.top ? HTTOP : HTLEFT; 
			case HTTOPRIGHT: 
				return point.y < rectWindow.top ? HTTOP : HTRIGHT; 
			case HTBOTTOMLEFT: 
				return point.y > rectWindow.bottom ? HTBOTTOM : HTLEFT; 
			case HTGROWBOX: 
			case HTBOTTOMRIGHT: 
				return point.y > rectWindow.bottom ? HTBOTTOM : HTRIGHT; 
			} 
		} 
 
		return nHit;    // no special translation 
	} 
 
	if (!rectWindow.PtInRect(point)) 
		return (UINT)HTNOWHERE; 
 
	CSize sizeBorder(GetSystemMetrics(SM_CXBORDER), 
		GetSystemMetrics(SM_CYBORDER)); 
 
	NCCALCSIZE_PARAMS Params; 
	Params.rgrc[0].top = rectWindow.top; 
	Params.rgrc[0].left = rectWindow.left; 
	Params.rgrc[0].bottom = rectWindow.bottom; 
	Params.rgrc[0].right = rectWindow.right; 
	OnNcCalcSize(FALSE, &Params); 
 
	CRect rcClient(Params.rgrc); 
	if (rcClient.PtInRect(point)) 
		return HTCLIENT; 
 
	if (dwStyle & (MFS_4THICKFRAME | MFS_THICKFRAME | WS_THICKFRAME)) 
	{ 
		UINT ht = HTNOWHERE; 
		CSize sizeOffset(sizeFrame.cx + sizeMiniSys.cx - sizeBorder.cx * 3, 
			sizeFrame.cy + sizeMiniSys.cy - sizeBorder.cy * 2); 
 
		if (point.y < rectWindow.top + sizeFrame.cy) 
		{ 
			if (dwStyle & MFS_4THICKFRAME) 
				ht = HTTOP; 
			else if (point.x <= rectWindow.left + sizeOffset.cx) 
				ht = HTTOPLEFT; 
			else if (point.x >= rectWindow.right - sizeOffset.cx) 
				ht = HTTOPRIGHT; 
			else 
				ht = HTTOP; 
		} 
		else if (point.y >= rectWindow.bottom - sizeFrame.cy) 
		{ 
			if (dwStyle & MFS_4THICKFRAME) 
				ht = HTBOTTOM; 
			else if (point.x <= rectWindow.left + sizeOffset.cx) 
				ht = HTBOTTOMLEFT; 
			else if (point.x >= rectWindow.right - sizeOffset.cx) 
				ht = HTBOTTOMRIGHT; 
			else 
				ht = HTBOTTOM; 
		} 
		else if (point.x < rectWindow.left + sizeFrame.cx) 
		{ 
			if (dwStyle & MFS_4THICKFRAME) 
				ht = HTLEFT; 
			else if (point.y <= rectWindow.top + sizeOffset.cy) 
				ht = HTTOPLEFT; 
			else if (point.y >= rectWindow.bottom - sizeOffset.cy) 
				ht = HTBOTTOMLEFT; 
			else 
				ht = HTLEFT; 
		} 
		else if (point.x >= rectWindow.right - sizeFrame.cx) 
		{ 
			if (dwStyle & MFS_4THICKFRAME) 
				ht = HTRIGHT; 
			else if (point.y <= rectWindow.top + sizeOffset.cy) 
				ht = HTTOPRIGHT; 
			else if (point.y >= rectWindow.bottom - sizeOffset.cy) 
				ht = HTBOTTOMRIGHT; 
			else 
				ht = HTRIGHT; 
		} 
 
		if (ht != HTNOWHERE) 
			return (dwStyle & MFS_MOVEFRAME) ? HTCAPTION : ht; 
 
		rectWindow.InflateRect(-sizeFrame.cx, -sizeFrame.cy); 
	} 
 
	rectWindow.bottom = rectWindow.top + sizeMiniSys.cy + sizeBorder.cy; 
	if (rectWindow.PtInRect(point)) 
	{ 
		if (point.x < rectWindow.left + (sizeMiniSys.cx - 2) && 
			(dwStyle & WS_SYSMENU)) 
			return HTSYSMENU; 
 
		return HTCAPTION; 
	} 
 
	return (UINT)HTERROR; 
} 
 
 
void CMiniMDIChildWnd::OnNcLButtonDown(UINT nHitTest, CPoint pt) 
{ 
	if (afxData.bSmCaption || nHitTest != HTSYSMENU) 
	{ 
		CMDIChildWnd::OnNcLButtonDown(nHitTest, pt); 
		return; 
	} 
 
	m_bSysTracking = TRUE; 
	m_bInSys = TRUE; 
	SetCapture(); 
	InvertSysMenu(); 
} 
 
void CMiniMDIChildWnd::OnMouseMove(UINT /*nFlags*/, CPoint pt) 
{ 
	if (!m_bSysTracking) 
	{ 
		Default(); 
		return; 
	} 
 
	ClientToScreen(&pt); 
	if (GetCapture() != this) 
	{ 
		m_bSysTracking = FALSE; 
		SendMessage(WM_NCPAINT); 
	} 
	else if ((OnNcHitTest(pt) == HTSYSMENU) != m_bInSys) 
	{ 
		m_bInSys = !m_bInSys; 
		InvertSysMenu(); 
	} 
} 
 
void CMiniMDIChildWnd::OnLButtonUp(UINT /*nFlags*/, CPoint pt) 
{ 
	if (!m_bSysTracking) 
	{ 
		Default(); 
		return; 
	} 
 
	ReleaseCapture(); 
	m_bSysTracking = FALSE; 
	ClientToScreen(&pt); 
 
	if (OnNcHitTest(pt) == HTSYSMENU) 
	{ 
		InvertSysMenu(); 
		SendMessage(WM_CLOSE); 
	} 
} 
 
void CMiniMDIChildWnd::InvertSysMenu() 
{ 
	CSize sizeBorder(GetSystemMetrics(SM_CXBORDER), 
		GetSystemMetrics(SM_CYBORDER)); 
	CSize sizeFrame(GetSystemMetrics(SM_CXFRAME), 
		GetSystemMetrics(SM_CYFRAME)); 
 
	CRect rect(sizeBorder.cx, sizeBorder.cy, 
		sizeMiniSys.cx - sizeBorder.cx, sizeMiniSys.cy); 
	if (GetStyle() & (MFS_4THICKFRAME | MFS_THICKFRAME | WS_THICKFRAME)) 
		rect.OffsetRect(sizeFrame.cx - sizeBorder.cx, sizeFrame.cy - sizeBorder.cy); 
 
	CDC* pDC = GetWindowDC(); 
	pDC->InvertRect(rect); 
	ReleaseDC(pDC); 
} 
 
 
 
 
// DrawFrame: [static] 
// Draws a frame in a given brush, with a given width for the lines. 
// Works like the doors to a cabinet: two tall stiles up and down the sides 
// and two short spacers across the top and bottom.  The thickness of the 
// lines are painted inside the rectangle. 
// 
static void DrawFrame(CDC* dc, LPCRECT lpRect, int nWidth, int nHeight, CBrush& br) 
{ 
	CRect rect; 
 
	// left stile 
	rect = *lpRect; 
	rect.right = rect.left + nWidth; 
	dc->FillRect(rect, &br); 
 
	// right stile 
	rect.right = lpRect->right; 
	rect.left = rect.right - nWidth; 
	dc->FillRect(rect, &br); 
 
	// top spacer 
	rect = *lpRect; 
	rect.bottom = rect.top + nHeight; 
	rect.left += nWidth; 
	rect.right -= nWidth; 
	dc->FillRect(rect, &br); 
 
	// bottom spacer 
	rect.bottom = lpRect->bottom; 
	rect.top = rect.bottom - nHeight; 
	dc->FillRect(rect, &br); 
} 
 
void CMiniMDIChildWnd::OnNcPaint() 
{ 
	if (afxData.bSmCaption) 
	{ 
		Default(); 
		return; 
	} 
 
	// Prepare for drawing the non-client area of the mini frame 
	CWindowDC dc(this); 
	CRect rect, rectCaption; 
	LONG dwStyle = GetStyle(); 
	GetWindowRect(&rect); 
	rect.OffsetRect(-rect.left, -rect.top); 
 
	// Create brushes we might need. 
	CBrush brFrame; 
	brFrame.CreateSolidBrush(::GetSysColor(COLOR_WINDOWFRAME)); 
	CBrush brBorder; 
	brBorder.CreateSolidBrush(::GetSysColor(m_bActive ? 
		COLOR_ACTIVEBORDER : COLOR_INACTIVEBORDER)); 
	CBrush brCaption; 
	brCaption.CreateSolidBrush(::GetSysColor(m_bActive ? 
		COLOR_ACTIVECAPTION : COLOR_INACTIVECAPTION)); 
 
	CSize sizeBorder(GetSystemMetrics(SM_CXBORDER), 
		GetSystemMetrics(SM_CYBORDER)); 
	CSize sizeFrame(GetSystemMetrics(SM_CXFRAME), 
		GetSystemMetrics(SM_CYFRAME)); 
 
	// Draw the thickframe.  Remove it from the rect. 
	if (dwStyle & (MFS_4THICKFRAME | WS_THICKFRAME | MFS_THICKFRAME)) 
	{ 
		DrawFrame(&dc, rect, sizeBorder.cx, sizeBorder.cy, brFrame); 
		rect.InflateRect(-sizeBorder.cx, -sizeBorder.cy); 
		DrawFrame(&dc, rect, sizeFrame.cx - sizeBorder.cx, 
			sizeFrame.cy - sizeBorder.cy, brBorder); 
 
		CSize sizeTick(sizeFrame.cx - sizeBorder.cx * 2, 
			sizeFrame.cy - sizeBorder.cy * 2); 
 
		if (!(dwStyle & MFS_4THICKFRAME)) 
		{ 
			CSize sizeOffset(sizeFrame.cx + sizeMiniSys.cx - sizeBorder.cx * 3, 
				sizeFrame.cy + sizeMiniSys.cy - sizeBorder.cy * 2); 
 
			dc.FillSolidRect(rect.left, rect.top + sizeOffset.cy, 
				sizeTick.cx, 1, RGB(0, 0, 0)); 
			dc.FillSolidRect(rect.left, rect.bottom - sizeOffset.cy, 
				sizeTick.cx, 1, RGB(0, 0, 0)); 
			dc.FillSolidRect(rect.right - sizeTick.cx, 
				rect.top + sizeOffset.cy, sizeTick.cx, 1, RGB(0, 0, 0)); 
			dc.FillSolidRect(rect.right - sizeTick.cx, 
				rect.bottom - sizeOffset.cy, sizeTick.cx, 1, RGB(0, 0, 0)); 
 
			dc.FillSolidRect(rect.left + sizeOffset.cx, rect.top, 
				1, sizeTick.cy, RGB(0, 0, 0)); 
			dc.FillSolidRect(rect.right - sizeOffset.cx, rect.top, 
				1, sizeTick.cy, RGB(0, 0, 0)); 
			dc.FillSolidRect(rect.left + sizeOffset.cx, 
				rect.bottom - sizeTick.cy, 1, sizeTick.cy, RGB(0, 0, 0)); 
			dc.FillSolidRect(rect.right - sizeOffset.cx, 
				rect.bottom - sizeTick.cy, 1, sizeTick.cy, RGB(0, 0, 0)); 
		} 
 
		rect.InflateRect(-sizeTick.cx, -sizeTick.cy); 
	} 
 
	// Draw the caption.  Remove it from rect. 
	if (dwStyle & WS_CAPTION) 
	{ 
		rectCaption = rect; 
		rectCaption.bottom = rectCaption.top + sizeMiniSys.cy + sizeBorder.cy; 
 
		DrawFrame(&dc, rectCaption, sizeBorder.cx, sizeBorder.cy, brFrame); 
		rectCaption.InflateRect(-sizeBorder.cx, -sizeBorder.cy); 
 
		dc.FillRect(&rectCaption, &brCaption); 
 
		// Draw the border around the client area. 
		// At this point, rc==rcClient.InflateRect(cxBorder, cyBorder). 
		// 
		DrawFrame(&dc, rect, sizeBorder.cx, sizeBorder.cy, brFrame); 
 
		if (hfontMiniSys != NULL) 
		{ 
			HFONT hFontOld = (HFONT)dc.SelectObject(hfontMiniSys); 
 
			CString strTitle; 
			GetWindowText(strTitle); 
			int xLeft = rectCaption.left + 
				(dwStyle & WS_SYSMENU ? sizeMiniSys.cx : 0); 
 
			CSize sizeText = dc.GetTextExtent(strTitle, strTitle.GetLength()); 
			if (sizeText.cx <= rectCaption.Width()) 
			{ 
				dc.SetTextAlign(TA_CENTER); 
				xLeft += (rectCaption.right - xLeft) / 2; 
			} 
 
			TEXTMETRIC tm; 
			VERIFY(dc.GetTextMetrics(&tm)); 
			int yHeight = tm.tmAscent + tm.tmDescent + tm.tmInternalLeading; 
			rectCaption.InflateRect(0, 1); 
			int yHeightDiff = (rectCaption.Height() - yHeight + 1) / 2; 
 
			dc.SetTextColor(::GetSysColor(m_bActive ? 
				COLOR_CAPTIONTEXT : COLOR_INACTIVECAPTIONTEXT)); 
			dc.SetBkMode(TRANSPARENT); 
			dc.ExtTextOut(xLeft, rectCaption.top + yHeightDiff, ETO_CLIPPED, 
				rectCaption, strTitle, strTitle.GetLength(), NULL); 
 
			dc.SelectObject(hFontOld); 
		} 
 
		// Draw the system menu. 
		if (dwStyle & WS_SYSMENU) 
		{ 
			CDC dcBitmap; 
			HBITMAP hBitmapOld; 
			if (!dcBitmap.CreateCompatibleDC(&dc)) 
				return; 
			hBitmapOld = (HBITMAP)dcBitmap.SelectObject(hbmMiniSys); 
			dc.BitBlt(rect.left, rect.top, sizeMiniSys.cx, sizeMiniSys.cy, 
				&dcBitmap, 0, 0, SRCCOPY); 
			dcBitmap.SelectObject(hBitmapOld); 
		} 
 
		rect.top = rectCaption.bottom; 
	} 
	else 
	{ 
		// Draw the border around the client area. 
		// At this point, rect == rcClient.InflateRect(cxBorder, cyBorder). 
		DrawFrame(&dc, rect, sizeBorder.cx, sizeBorder.cy, brFrame); 
	} 
} 
 
void CMiniMDIChildWnd::OnSysCommand(UINT nID, LPARAM lParam) 
{ 
	DWORD dwStyle = GetStyle(); 
	if ((dwStyle & WS_POPUP) && 
		((nID & 0xFFF0) != SC_CLOSE || 
		(GetKeyState(VK_F4) < 0 && GetKeyState(VK_MENU) < 0 && 
		(dwStyle & MFS_SYNCACTIVE)))) 
	{ 
		if (HandleFloatingSysCommand(nID, lParam)) 
			return; 
	} 
	CMDIChildWnd::OnSysCommand(nID, lParam); 
} 
 
void CMiniMDIChildWnd::CalcWindowRect(LPRECT lpClientRect, UINT nAdjustType) 
{ 
	if (afxData.bSmCaption) 
	{ 
		CMDIChildWnd::CalcWindowRect(lpClientRect, nAdjustType); 
		return; 
	} 
 
	LONG dwStyle = GetStyle(); 
	if (dwStyle & (MFS_4THICKFRAME | WS_THICKFRAME)) 
	{ 
		::InflateRect(lpClientRect, 
			GetSystemMetrics(SM_CXFRAME), GetSystemMetrics(SM_CYFRAME)); 
	} 
	else 
	{ 
		::InflateRect(lpClientRect, 
			GetSystemMetrics(SM_CXBORDER), GetSystemMetrics(SM_CYBORDER)); 
	} 
 
	if (dwStyle & WS_CAPTION) 
		lpClientRect->top -= sizeMiniSys.cy; 
} 
 
LRESULT CMiniMDIChildWnd::OnGetText(WPARAM wParam, LPARAM lParam) 
{ 
	if (afxData.bSmCaption) 
		return Default(); 
 
	lstrcpyn((LPTSTR)lParam, (LPCTSTR)m_strCaption, wParam); 
	if ((int)wParam > m_strCaption.GetLength()) 
		wParam = m_strCaption.GetLength(); 
	return wParam; 
} 
 
LRESULT CMiniMDIChildWnd::OnGetTextLength(WPARAM, LPARAM) 
{ 
	if (afxData.bSmCaption) 
		return Default(); 
 
	return m_strCaption.GetLength(); 
} 
 
// CMiniMDIChildWnd::OnSetText 
//  Windows will repaint just the caption, as a thick caption, if 
//  we don't override WM_SETTEXT.  NOTE: Unfortunately, this will never 
//  get called if you do a SetWindowText() on this window from another 
//  task.  Use SendMessage instead. 
LRESULT CMiniMDIChildWnd::OnSetText(WPARAM, LPARAM lParam) 
{ 
	if (afxData.bSmCaption) 
		return Default(); 
 
	TRY 
	{ 
		if (lParam == NULL) 
		{ 
			// NULL lParam means set caption to nothing 
			m_strCaption.Empty(); 
		} 
		else 
		{ 
			// non-NULL sets caption to that specified by lParam 
			lstrcpy(m_strCaption.GetBufferSetLength(lstrlen((LPCTSTR)lParam)), 
				(LPCTSTR)lParam); 
		} 
		SendMessage(WM_NCPAINT); 
	} 
	CATCH_ALL(e) 
	{ 
		// Note: DELETE_EXCEPTION(e) not required 
		return FALSE; 
	} 
	END_CATCH_ALL 
 
	return TRUE; 
} 

 

 
//////////////////////////////////////////////////////////////////////////// 

