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

/* $KTH: TerminalEngine.cpp,v 1.4 1999/12/02 16:58:35 joda Exp $ */

// TerminalEngine.cpp
// Author: Jörgen Karlsson - d93-jka@nada.kth.se

#include <Windows.h>
#include "Resource.h"
#include "TelnetApp.h"
#include "TelnetEngine.h"
#include "EmulatorEngine.h"
#include "TerminalEngine.h"
#include "char_codes.h"

void
TerminalEngine::init_tabs(void)
{
    for(int i = 0; i < mBufSZ.x; ++i)
	if(i % 8 == 0)
	    tabs[i] = 1;
	else
	    tabs[i] = 0;
}

void
TerminalEngine::remove_tab(int col)
{
    tabs[col - 1] = 0;
}

void
TerminalEngine::remove_all_tabs(void)
{
    for(int i = 0; i < mBufSZ.x; ++i)
	tabs[i] = 0;
}

void
TerminalEngine::set_tab(int col)
{
    tabs[col - 1] = 1;
}

void TerminalEngine::Open(void)
{
    HINSTANCE hinstance = theApp->AppInstance;
    WNDCLASSEX wcx; 
    TEXTMETRIC tm;

    /* 
     * Fill in the window class structure with parameters 
     * that describe the main window. 
     */ 
    wcx.cbSize = sizeof(wcx);          /* size of structure      */ 
    wcx.style = CS_OWNDC;//|CS_HREDRAW|CS_VREDRAW;
    /* redraw if size changes */
    // Get own DC
    wcx.lpfnWndProc = TerminalEngine::WindowProc; /* points to window proc. */ 
    wcx.cbClsExtra = 0;                /* no extra class memory  */ 
    wcx.cbWndExtra = 0;                /* no extra window memory */ 
    wcx.hInstance = hinstance;         /* handle of instance     */ 
    wcx.hIcon = LoadIcon(NULL, 
			 IDI_APPLICATION);              /* predefined app. icon   */ 
    wcx.hCursor = LoadCursor(NULL, 
			     IDC_ARROW);                    /* predefined arrow       */ 
    wcx.hbrBackground = GetStockObject( 
				       WHITE_BRUSH);                  /* white background brush */ 
    wcx.lpszMenuName =  MAKEINTRESOURCE(IDR_MENU_TELNET);/* name of menu resource  */ 
    wcx.lpszClassName = "MainWClass";  /* name of window class   */ 
    wcx.hIconSm = LoadImage(hinstance, /* small class icon       */ 
			    MAKEINTRESOURCE(5),
			    IMAGE_ICON,
			    GetSystemMetrics(SM_CXSMICON), 
			    GetSystemMetrics(SM_CYSMICON), 
			    LR_DEFAULTCOLOR); 
 
    /* Register the window class. */ 
 
    RegisterClassEx(&wcx); 
	
    mWindow = CreateWindow("MainWClass",           /* class name */ 
			   "Telnet Session",       /* window name */ 
			   WS_OVERLAPPEDWINDOW |   /* overlapped window */ 
			   WS_VSCROLL,         /* vertical scroll bar */ 
			   CW_USEDEFAULT,          /* default
						      horizontal position */
			   CW_USEDEFAULT,          /* default vertical
						      position */
			   17,        /* default width */ 
			   17,       /* default height */ 
			   (HWND) NULL,            /* no parent or
						      owner window */
			   (HMENU) NULL,           /* class menu used */ 
			   hinstance,              /* instance handle */ 
			   mTelnetSession);     /* send session as
						   window creation data */
 
    if (!mWindow) 
	exit(-1);
	
    mHDC = GetDC(mWindow);
    SetMapMode(mHDC, MM_TEXT);

    /* Create fonts needed */	
    LOGFONT  lplf[1] = {
	16,8,0,0,
	/*LONG lfHeight;LONG lfWidth;
	  LONG lfEscapement;LONG lfOrientation;*/
	FW_NORMAL,0,0,0,
	/*LONG lfWeight;BYTE lfItalic; 
	  BYTE lfUnderline;BYTE lfStrikeOut;*/
	ANSI_CHARSET,OUT_DEFAULT_PRECIS,
	CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,
	/*BYTE lfCharSet;BYTE lfOutPrecision; 
	  BYTE lfClipPrecision;BYTE lfQuality;*/ 
	DEFAULT_PITCH|FF_MODERN,0};
    /*BYTE lfPitchAndFamily;TCHAR lfFaceName[LF_FACESIZE]*/ 
    stdFont = CreateFontIndirect(lplf);
	
    LOGFONT lplfb[1] = {
	16,8,0,0,
	/*LONG lfHeight;LONG lfWidth;
	  LONG lfEscapement;LONG lfOrientation;*/
	FW_BOLD,0,0,0,
	/*LONG lfWeight;BYTE lfItalic; 
	  BYTE lfUnderline;BYTE lfStrikeOut;*/
	DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,
	CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,
	/*BYTE lfCharSet;BYTE lfOutPrecision; 
	  BYTE lfClipPrecision;BYTE lfQuality;*/ 
	DEFAULT_PITCH|FF_MODERN,0};
    /*BYTE lfPitchAndFamily;TCHAR lfFaceName[LF_FACESIZE]*/ 
	
    boldFont = CreateFontIndirect(lplfb);
	
    LOGFONT lplfu[1] = {
	16,8,0,0,
	/*LONG lfHeight;LONG lfWidth;
	  LONG lfEscapement;LONG lfOrientation;*/
	FW_NORMAL,0,1,0,
	/*LONG lfWeight;BYTE lfItalic; 
	  BYTE lfUnderline;BYTE lfStrikeOut;*/
	DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,
	CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,
	/*BYTE lfCharSet;BYTE lfOutPrecision; 
	  BYTE lfClipPrecision;BYTE lfQuality;*/ 
	DEFAULT_PITCH|FF_MODERN,0};
    /*BYTE lfPitchAndFamily;TCHAR lfFaceName[LF_FACESIZE]*/ 
	
    uscoreFont = CreateFontIndirect(lplfu);

    LOGFONT lplfub[1] = {
	16,8,0,0,
	/*LONG lfHeight;LONG lfWidth;
	  LONG lfEscapement;LONG lfOrientation;*/
	FW_BOLD,0,1,0,
	/*LONG lfWeight;BYTE lfItalic; 
	  BYTE lfUnderline;BYTE lfStrikeOut;*/
	DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,
	CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,
	/*BYTE lfCharSet;BYTE lfOutPrecision; 
	  BYTE lfClipPrecision;BYTE lfQuality;*/ 
	DEFAULT_PITCH|FF_MODERN,0};
    /*BYTE lfPitchAndFamily;TCHAR lfFaceName[LF_FACESIZE]*/ 
	
    bolduscoreFont = CreateFontIndirect(lplfub);
  
    // Select standard font.
    SelectObject(mHDC, stdFont); 
	
    // Save the avg. width and height of characters.
    GetTextMetrics(mHDC, &tm);
    mCharWidth = tm.tmAveCharWidth; 
    mCharHeight = tm.tmHeight;

    // Save text and background colors
    mTextColor = GetTextColor(mHDC);
    mBkColor = GetBkColor(mHDC);
    
    int fh = 2 * GetSystemMetrics(SM_CYFRAME) + 
	GetSystemMetrics(SM_CYCAPTION) + 
	GetSystemMetrics(SM_CYMENU) + 3;
    int fw = 2 * GetSystemMetrics(SM_CXFRAME) + 
	GetSystemMetrics(SM_CXVSCROLL);

    SetWindowPos(mWindow, NULL, 0, 0, 80 * mCharWidth + fw, 24 * mCharHeight + fh, 
		 SWP_NOZORDER | SWP_NOMOVE);
    RECT bounds;
    GetWindowRect(mWindow, &bounds);
    mWindowHeight = bounds.bottom - bounds.top;
    mWindowWidth = bounds.right - bounds.left;


    // Create and initialize buffer
    mBufSZ.x = GetSystemMetrics(SM_CXFULLSCREEN)/mCharWidth;	// Width of buffer in characters.
    mBufSZ.y = 3*GetSystemMetrics(SM_CYFULLSCREEN)/mCharHeight;	// Height of buffer in characters
    mBuffer = new char[mBufSZ.x*mBufSZ.y];
    mAttrBuffer = new unsigned char[mBufSZ.x*mBufSZ.y];
    tabs = new int[mBufSZ.x];
    init_tabs();
    mOrigin.x = mOrigin.y = mCursor.x = mCursor.y = 1;


    // Initialize scrollbar
    SCROLLINFO s = {sizeof(SCROLLINFO),SIF_POS|SIF_RANGE,0,
		    mBufSZ.y*mCharHeight-mWindowHeight,
		    0,0,0};
    SetScrollInfo(mWindow, SB_VERT, &s, FALSE);

    // Show the window 
    Clear(SPACE);
    ShowWindow(mWindow, SW_SHOWDEFAULT);
}

LRESULT CALLBACK TerminalEngine::WindowProc(HWND window, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static TelnetSession *theSession;
    PAINTSTRUCT paint;
    switch(msg) {
	
    case WM_CREATE:
	theSession = (TelnetSession *)(((CREATESTRUCT*)lParam)->lpCreateParams);
	break;
	
    case WM_DESTROY:
	CloseWindow((HWND)wParam);
	PostQuitMessage(0); break;
	
    case WM_CHAR:
	theSession->mTerminalEngine->Send((char*)&wParam, 1);
	break;

    case WM_KEYDOWN: 
	switch (wParam) {
	case VK_LEFT:   /* LEFT ARROW */ 
	case VK_RIGHT:  /* RIGHT ARROW */ 
	case VK_UP:     /* UP ARROW   */ 
	case VK_DOWN:   /* DOWN ARROW */
	case VK_DELETE:	// DELETE KEY
	case VK_F1:
	case VK_F2:
	case VK_F3:
	case VK_F4:
	    theSession->mTerminalEngine->SendExtended(wParam);
	    break;
	case VK_HOME:   /* HOME */ 
	case VK_END:    /* END */
	    break;
	}
	break;
	
    case WM_PAINT:
	/*		RECT test;
			if(GetUpdateRect(theSession->mTerminalEngine->mWindow, &test, FALSE))
			*/		{
	BeginPaint(theSession->mTerminalEngine->mWindow, &paint);
	theSession->mTerminalEngine->Redraw(paint.rcPaint);
	EndPaint(theSession->mTerminalEngine->mWindow, &paint);
    }
    break;

    case WM_TIMER:
	theSession->mTerminalEngine->UpdateWindowSize();
	break;

    case WM_COMMAND:
	if(HIWORD(wParam) == 0) // Menu selected.
	    theSession->InvokeCommand(LOWORD(wParam));
	break;
	
    case WM_SETFOCUS:
	if(!CreateCaret(theSession->mTerminalEngine->mWindow,0,
			0,
			theSession->mTerminalEngine->mCharHeight))
	    TelnetApp::Error(GetLastError(),
			     "TerminalEngine::WindowProc: Unable to create caret.\n");
	theSession->mTerminalEngine->UpdateCaret();
	ShowCaret(theSession->mTerminalEngine->mWindow);
	break;
	
    case WM_KILLFOCUS:
	HideCaret(theSession->mTerminalEngine->mWindow);
	DestroyCaret();
	break;
		
    case WM_VSCROLL:
	switch (LOWORD(wParam)) {
	case SB_LINEUP:
	case SB_LINEDOWN:
	case SB_PAGEUP:
	case SB_PAGEDOWN:
	    theSession->mTerminalEngine->SetOriginRelative(LOWORD(wParam));
	    break;

	case SB_THUMBPOSITION:
	case SB_THUMBTRACK:
	    int nPos = HIWORD(wParam);
	    theSession->mTerminalEngine->SetOrigin(0, nPos);
	    break;
	}
	break;
	
    case WM_SIZE: 
	theSession->mTerminalEngine->SetWindowSize(LOWORD(lParam), HIWORD(lParam));
	break;

    case SM_READ:
	theSession->mTelnetEngine->ReadSocket();
	break;

    default: return DefWindowProc(window, msg, wParam, lParam);
    }
    return 0;
}

void TerminalEngine::DrawText(unsigned char *buf, unsigned int buflen)
{
    unsigned int i;
    unsigned int invTopRow = mCursor.y;

    for(i=0;i<buflen;i++) {
	switch(buf[i]) {
	case NUL: break;
		
	case Linefeed:	// Line	Feed
	    InvalidateRows(mCursor.y, mCursor.y);
	    mCursor.y++;
	    break;
		
	case Return:	// Carrige Return
	    mCursor.x = 1;
	    break;

	case BS:		// Back Space
	    mCursor.x--;
	    break;

	case BEL:		// Bell
	    MessageBeep(MB_ICONHAND);
	    continue;
	    break;
		
	case HT: {		// Horizontal Tab
	    int q;

	    for(q = mCursor.x; tabs[q] == 0; ++q)
		;
	    mCursor.x = q + 1;
	    break;
	}
	case 0x01:
	case 0x02:
	case 0x03:
	case 0x04:
	case 0x05:
	case 0x06:
	case 0x0b:
	case 0x0c:
	case 0x0e:
	case 0x0f:
	case 0x10:
	case 0x11:
	case 0x12:
	case 0x13:
	case 0x14:
	case 0x15:
	case 0x16:
	case 0x17:
	case 0x18:
	case 0x19:
	case 0x1a:
	case 0x1b:
	case 0x1c:
	case 0x1d:
	case 0x1e:
	case 0x1f:
	    break;

	default:		// Printable character
	    int x = mCursor.x, y = 0;
	    LCtoBC(&y,&x);
	    if(x <= mBufSZ.x) {
		int h, w;
		GetCharWindowSize(&w, &h);

		if(AutoWrapMode() && mCursor.x == w + 1) {
		    mCursor.x = 1;
		    InvalidateRows(mCursor.y, mCursor.y);
		    mCursor.y++;
		}
		if(InsertMode())
		    InsertChars(1);
		*Cur2Addr(mBuffer, mCursor.y, mCursor.x) = buf[i];
		*Cur2Addr(mAttrBuffer, mCursor.y, mCursor.x) = mCurrentAttr;
		mCursor.x++;
	    }
	}
	int y = mCursor.y, x = 0;
	LCtoBC(&y,&x);
	if(y > mBufSZ.y) Scroll(-1);
    }

    unsigned int invBottomRow = mCursor.y;
    BOOL erase = AdjustOrigin();
    InvalidateRows(invTopRow, invBottomRow);
}

void TerminalEngine::RedrawAll(void)
{
#if 0
    int err, i;
    int RowsToDraw, StartRow;
    RECT bounds;
    POINT origin;


    GetClientRect(mWindow, &bounds);
    GetWindowOrgEx(mHDC, &origin);
    StartRow = origin.y/mCharHeight;
    RowsToDraw = bounds.bottom/mCharHeight+1;
    for(i=0;i<RowsToDraw;i++) {
	if(StartRow+i>=mBufSZ.y) break;
	bounds.top = mCharHeight * (StartRow+i);
	bounds.bottom =	mCharHeight * (StartRow+i+1);
	err = ::DrawText(mHDC, mBuffer+mBufSZ.x*(StartRow+i), -1,
			 &bounds, 0);
    }
    SetCaretPos(mCharWidth*(mCursor.x-1), mCharHeight*(mCursor.y-1));
    ValidateRect(mWindow, 0);
#endif
}


void TerminalEngine::Close(void)
{
    ReleaseDC(mWindow, mHDC);
    PostMessage(mWindow,WM_DESTROY,0,0);
}

void TerminalEngine::Scroll(int Rows)
{
    mCursor.y += Rows;
    mCursor.x = 1;
    ScrollRows(1, mBufSZ.y, Rows);
    //AdjustOrigin();
}

void TerminalEngine::Clear(char c)
{
    FillMemory(mBuffer, mBufSZ.x*mBufSZ.y, c);
    FillMemory(mAttrBuffer, mBufSZ.x*mBufSZ.y, 0);
}

// Adjust window origin so that caret i visible.
// Return TRUE if origin is moved, FALSE otherwise.
BOOL TerminalEngine::AdjustOrigin(void)
{
    BOOL wasMoved = FALSE;
    POINT caret, origin, bottomright;
    int x = mCursor.x;
    int y = mCursor.y;
    LCtoBC(&y, &x);
    caret.x = mCharWidth*(x-1);
    caret.y = mCharHeight*(y-1);

    bottomright.y = mWindowHeight;
    DPtoLP(mHDC, &bottomright, 1);

    // Get current origin.
    GetWindowOrgEx(mHDC, &origin);

    if(bottomright.y<caret.y+mCharHeight-mCharHeight/4 || // Caret below bottom 
       caret.y+mCharHeight/4 < origin.y){ // Caret above origin
	origin.y = (mOrigin.y-1)*mCharHeight;
	SetOrigin(0, origin.y);
	wasMoved = TRUE;
    }
    return wasMoved;
}

// Adjust origin so that only buffer is visible.
int TerminalEngine::AdjustCharOrigin(void)
{
    int x,y;
    GetCharWindowSize(&x,&y);
	
    if(mCursor.y > y) {
	mOrigin.y += mCursor.y - y;
	mCursor.y = y;
    }

    LCtoBC(&y,&x);
    if(y>mBufSZ.y) {
	int move = y - mBufSZ.y;
	mOrigin.y -= move;
	mCursor.y += move;
    }
	
    return 0;
}

TerminalEngine::TerminalEngine(TelnetSession *thisTelnetSession)
{
    mTelnetSession = thisTelnetSession;
    mRecStream = new CharStream();
    mCurrentAttr = 0;
    mBuffer = 0;
    mAttrBuffer = 0;
    tabs = 0;
}

TerminalEngine::~TerminalEngine(void)
{
    if(mBuffer)
	delete []mBuffer;
    if(mAttrBuffer)
	delete []mAttrBuffer;
    if(tabs)
	delete []tabs;
    delete mRecStream;
}

void TerminalEngine::MoveCursor(int drow, int dcol)
{
    int x,y;
    GetCharWindowSize(&x,&y);
    mCursor.y += drow;
    if(mCursor.y < 1) mCursor.y = 1;
    if(mCursor.y > y) mCursor.y = y;

    mCursor.x += dcol;
    if(mCursor.x < 1) mCursor.x = 1;
    if(mCursor.x > x) mCursor.x = x;

    UpdateCaret();
}

void TerminalEngine::PlaceCursor(int row, int col)
{
    int x,y;
    GetCharWindowSize(&x,&y);
    mCursor.y = row;
    if(mCursor.y < 1) mCursor.y = 1;
    if(mCursor.y > y) mCursor.y = y;

    mCursor.x = col;
    if(mCursor.x < 1) mCursor.x = 1;
    if(mCursor.x > x) mCursor.x = x;

    UpdateCaret();
}

void TerminalEngine::GetCursorPos(int *row, int *col)
{
    *row = mCursor.y;
    *col = mCursor.x;
}

void TerminalEngine::ScrollRows(int ScrRegTop, int ScrRegBot, int Rows)
{

    // Redraw invalid part of window, to make sure window is ok to scroll.
    UpdateWindow(mWindow);

    int Width, Height;
    GetCharWindowSize(&Width, &Height);
    if(ScrRegTop == 1 && ScrRegBot == Height && Rows < 0) {
	// Whole screen being scrolled.
	LCtoBC(&Height, &Width);
		
	// Check if it's posible just to move origin.
	if(Height < mBufSZ.y) {
	    Rows *= -1;
	    int RowsToScroll = mBufSZ.y-Height;
	    if(RowsToScroll > Rows) RowsToScroll = Rows;
	    int RowsLeft = Rows - (mBufSZ.y-Height);
	    if(RowsLeft < 0) RowsLeft = 0;
	    mOrigin.y += RowsToScroll;
	    if(RowsLeft) ScrollRows(ScrRegTop, ScrRegBot, -RowsLeft);
	    return;
	}
    }
	
	
    // Scroll into buffer if possible.
    int ScrRegTopBuf;
    if(ScrRegTop == 1) 
	ScrRegTopBuf = ScrRegTop - (mOrigin.y-1);
    else 
	ScrRegTopBuf = ScrRegTop;
	
    // Scroll contents of buffer.
    int memrow = Rows;
    if(Rows < 0) {
	Rows *= -1;
	MoveMemory(Cur2Addr(mBuffer, ScrRegTopBuf,1),Cur2Addr(mBuffer, ScrRegTopBuf+Rows,1)
		   ,mBufSZ.x*(ScrRegBot-ScrRegTopBuf+1-Rows));
	FillMemory(Cur2Addr(mBuffer, ScrRegBot+1-Rows,1),mBufSZ.x*Rows, SPACE);
//	MoveMemory(Cur2Addr(mAttrBuffer, ScrRegTopBuf,1),Cur2Addr(mAttrBuffer, ScrRegTopBuf+Rows,1)
//		   ,mBufSZ.x*(ScrRegBot-ScrRegTopBuf+1-Rows));
//	FillMemory(Cur2Addr(mAttrBuffer, ScrRegBot+1-Rows,1),mBufSZ.x*Rows, 0);
	
	//ScrollRect.top += (ScrRegBot-ScrRegTop-Rows)*mCharHeight;
    } else {
	MoveMemory(Cur2Addr(mBuffer, ScrRegTop+Rows,1),Cur2Addr(mBuffer, ScrRegTop,1)
		   ,mBufSZ.x*(ScrRegBot-ScrRegTop+1-Rows));
	FillMemory(Cur2Addr(mBuffer, ScrRegTop, 1),mBufSZ.x*Rows, SPACE);
	MoveMemory(Cur2Addr(mAttrBuffer, ScrRegTop+Rows,1),Cur2Addr(mAttrBuffer, ScrRegTop,1)
		   ,mBufSZ.x*(ScrRegBot-ScrRegTop+1-Rows));
	FillMemory(Cur2Addr(mAttrBuffer, ScrRegTop, 1),mBufSZ.x*Rows, 0);
	
	//ScrollRect.bottom -= (ScrRegBot-ScrRegTop-Rows)*mCharHeight;
    }
	

    // Scroll contents of window
    HideCaret(mWindow);
	
    RECT ScrollRect;
    ScrollRect.top = (ScrRegTop-1)*mCharHeight;
    ScrollRect.bottom = ScrRegBot*mCharHeight;
    ScrollRect.left = 0;
    ScrollRect.right = mWindowWidth;
	
    //GetClientRect(mWindow,(RECT*)&ScrollRect);	// Always scroll all of window.
    // Should only scroll the specified rows.
    ScrollWindowEx(	mWindow, 0, mCharHeight*memrow, &ScrollRect,&ScrollRect,
			0, 0, SW_ERASE|SW_INVALIDATE);

    ShowCaret(mWindow);
}

void TerminalEngine::ClearLineFromCursor(void)
{
    FillMemory(Cur2Addr(mBuffer, mCursor.y, mCursor.x), mBufSZ.x-mCursor.x+1, SPACE);
    FillMemory(Cur2Addr(mAttrBuffer, mCursor.y, mCursor.x), mBufSZ.x-mCursor.x+1, 0);
    InvalidateRows(mCursor.y, mCursor.y);
}

void TerminalEngine::ClearLineToCursor(void)
{
    FillMemory(Cur2Addr(mBuffer, mCursor.y, 1), mCursor.x, SPACE);
    FillMemory(Cur2Addr(mAttrBuffer, mCursor.y, 1), mCursor.x, 0);
    InvalidateRows(mCursor.y, mCursor.y);
}

void TerminalEngine::ClearScreenFromCursor(void)
{
    size_t start, len;
    start = Cur2Addr(mBuffer, mCursor.y, mCursor.x) - mBuffer;
    len = mBufSZ.x * mBufSZ.y - start;
    FillMemory(mBuffer + start, len, SPACE);
    FillMemory(mAttrBuffer + start, len, 0);
    InvalidateRows(mCursor.y, mBufSZ.y);
}

void TerminalEngine::ClearScreenToCursor(void)
{
    int start, len;
    start = Cur2Addr(mBuffer, 1, 1) - mBuffer;
    len = Cur2Addr(mBuffer, mCursor.y, mCursor.x) - mBuffer - start + 1;
    FillMemory(mBuffer + start, len, SPACE);
    FillMemory(mAttrBuffer + start, len, 0);
    InvalidateRows(1, mCursor.y);
}

void TerminalEngine::ClearScreen(void)
{
    Clear(SPACE);
    InvalidateRect(mWindow, 0, TRUE);
}

void TerminalEngine::FillWithE(void)
{
    Clear('E');
    InvalidateRect(mWindow, 0, TRUE);
}


void TerminalEngine::DeleteChars(int num)
{
    int start, end;
    int width, height;
    GetCharWindowSize(&width, &height);
    if(mCursor.x > width)
	width = mCursor.x;
    start = Cur2Addr(mBuffer, mCursor.y, mCursor.x) - mBuffer;
    end = Cur2Addr(mBuffer, mCursor.y, width) - mBuffer + 1;
    if(num > end - start)
	num = end - start;
    memmove(mBuffer + start, mBuffer + start + num, end - start - num);
    memset(mBuffer + end - num, SPACE, num);
    memmove(mAttrBuffer + start, mAttrBuffer + start + num, end - start - num);
    memset(mAttrBuffer + end - num, 0, num);
    InvalidateRows(mCursor.y, mCursor.y);
}

void TerminalEngine::InsertChars(int num)
{
    int start, end;
    int width, height;
    GetCharWindowSize(&width, &height);
    if(mCursor.x > width)
	width = mCursor.x;
    start = Cur2Addr(mBuffer, mCursor.y, mCursor.x) - mBuffer;
    end = Cur2Addr(mBuffer, mCursor.y, width) - mBuffer + 1;
    if(num > end - start)
	num = end - start;
    memmove(mBuffer + start + num, mBuffer + start, end - start - num);
    memset(mBuffer + start, SPACE, num);
    memmove(mAttrBuffer + start + num, mAttrBuffer + start, end - start - num);
    memset(mAttrBuffer + start, 0, num);
    InvalidateRows(mCursor.y, mCursor.y);
}

void TerminalEngine::DeleteLines(int fromline, int endline, int num)
{
    int start, end;
    endline++;
    start = Cur2Addr(mBuffer, fromline, 1) - mBuffer;
    end = Cur2Addr(mBuffer, endline, 1) - mBuffer;
    if(num > endline - fromline)
	num = endline - fromline;
    num *= mBufSZ.x;
    memmove(mBuffer + start, mBuffer + start + num, end - start - num);
    memset(mBuffer + end - num, SPACE, num);
    memmove(mAttrBuffer + start, mAttrBuffer + start + num, 
	    end - start - num);
    memset(mAttrBuffer + end - num, 0, num);
    InvalidateRows(fromline, endline);
}

void TerminalEngine::InsertLines(int fromline, int endline, int num)
{
    int start, end;
    endline++;
    start = Cur2Addr(mBuffer, fromline, 1) - mBuffer;
    end = Cur2Addr(mBuffer, endline, 1) - mBuffer;
    if(num > endline - fromline)
	num = endline - fromline;
    num *= mBufSZ.x;
    memmove(mBuffer + start + num, mBuffer + start, end - start - num);
    memset(mBuffer + start, SPACE, num);
    memmove(mAttrBuffer + start + num, mAttrBuffer + start, 
	    end - start - num);
    memset(mAttrBuffer + start, 0, num);
    InvalidateRows(fromline, endline);
}

void TerminalEngine::SetOrigin(int x, int y)
{
    int oldy;
    POINT oldorg;

    // Check range
    if(y<0) y=0;
    int maxOrg = mBufSZ.y*mCharHeight-mWindowHeight;
    if(y>maxOrg) y=maxOrg;

    // Get old origin
    GetWindowOrgEx(mHDC, &oldorg);
    oldy = oldorg.y;
	
    if(y != oldy) {
	// Update scrollbar
	SCROLLINFO scri = { sizeof(SCROLLINFO), SIF_POS, 0,0,0, 0,0 };
	scri.nPos = y;
	SetScrollInfo(mWindow, SB_VERT, &scri, TRUE);
	
	UpdateWindow(mWindow);
		
	// Set window to show right part of buffer
	HideCaret(mWindow);
	SetWindowOrgEx(mHDC, 0, y, NULL);
		
	// Scroll contents of window
	RECT r;
	GetClientRect(mWindow,(RECT*)&r);
	ScrollWindowEx(	mWindow, 0, oldy-y, 0,(const RECT*)&r,
			0,0, SW_ERASE|SW_INVALIDATE);
	ShowCaret(mWindow);
    }
}

void
TerminalEngine::SetWindowSize(int x, int y)
{
    BOOL setSize = FALSE;
    int maxX = GetSystemMetrics(SM_CXSCREEN);
    int maxY = GetSystemMetrics(SM_CYSCREEN);
	
    // Check range
    if(x<0){x=0; setSize = TRUE;}
    if(x>maxX){x=maxX; setSize = TRUE;}

    if(y<0){y=0; setSize = TRUE;}
    if(y>maxY){y=maxY; setSize = TRUE;}

    // If size out of range, make sure window gets a valid size
    if(setSize)
	SetWindowPos(mWindow,0,0,0,		// No x,y,z position
		     x,					// width
		     y,					// height
		     SWP_NOMOVE|SWP_NOZORDER);	// window-positioning flags

    // Invalidate window if necessary.
    if(x > mWindowWidth) {
	RECT invRect = {mWindowWidth ,0, x, y};
	InvalidateRect(mWindow, &invRect, TRUE);
    }
    if(y > mWindowHeight) {
	RECT invRect = {0, mWindowHeight, x, y};
	InvalidateRect(mWindow, &invRect, TRUE);
    }
	
    // width of client area
    mWindowWidth = x;
    // height of client area 
    mWindowHeight = y;
	
    // Update size of scrollbar
    SCROLLINFO s = {sizeof(SCROLLINFO), SIF_RANGE,0,0,0,0,0};
    s.nMax = mBufSZ.y*mCharHeight-mWindowHeight;
    SetScrollInfo(mWindow, SB_VERT, &s, TRUE);

    // Correct origin to new size.
    s.fMask = SIF_POS;
    GetScrollInfo(mWindow, SB_VERT, &s);
    SetOrigin(0, s.nPos);
    AdjustCharOrigin();
}

void TerminalEngine::Redraw(RECT invRect)
{
    int outLen;
    int FirstRow, LastRow, CurRow;
    const int tmpBufSZ = 200;
    char tmpBuf[tmpBufSZ];
    AttributeWord CurAttr;
    RECT bounds = invRect;

    FirstRow = invRect.top/mCharHeight+1;
    LastRow = invRect.bottom/mCharHeight+1;

    if(FirstRow == LastRow)
	FirstRow = 1;

    for(CurRow=FirstRow;CurRow<=LastRow;CurRow++) {
	if(CurRow>mBufSZ.y) break;
	bounds.top = mCharHeight * (CurRow-1);
	bounds.bottom =	mCharHeight * CurRow;
	while(BuildPrintable(CurRow, &bounds, tmpBuf, tmpBufSZ, 
			     &outLen, &CurAttr)) {
	    if(CurAttr&BOLD && !(CurAttr&UNDERSCORE))
		SelectObject(mHDC, boldFont);
	    else if(!(CurAttr&BOLD) && CurAttr&UNDERSCORE)
		SelectObject(mHDC, uscoreFont);
	    else if(CurAttr&BOLD && CurAttr&UNDERSCORE)
		SelectObject(mHDC, bolduscoreFont);
	    else
		SelectObject(mHDC, stdFont);

	    if(CurAttr&REVERSE || CurAttr&BLINK) {
		SetTextColor(mHDC, mBkColor);
		SetBkColor(mHDC, mTextColor);
	    } else {
		SetTextColor(mHDC, mTextColor);
		SetBkColor(mHDC, mBkColor);
	    }

	    ::DrawText(mHDC, tmpBuf, outLen, &bounds, DT_NOPREFIX);
	}
    }
    UpdateCaret();
}

// ---------------------------------------------------------------------------
//		SetAttribute
// ---------------------------------------------------------------------------
void
TerminalEngine::SetAttribute(AttributeWord attr)
{
    mCurrentAttr |= attr;
}

// ---------------------------------------------------------------------------
//		ClearAttribute
// ---------------------------------------------------------------------------
void
TerminalEngine::ClearAttribute(AttributeWord attr)
{
    mCurrentAttr ^= attr & mCurrentAttr;
}

BOOL
TerminalEngine::BuildPrintable(int curRow, RECT *bounds, char *Buf,
			       int maxBufLen, int *BufLen,
			       AttributeWord *Attr)
{
    static int lastRow = 0, LastCol = 0;
    static BOOL RowDone = FALSE;
    int CurCol;

    if(curRow != lastRow) {
	lastRow	= curRow;
	LastCol = CurCol = 1;
	RowDone = FALSE;
    } else {
	CurCol = LastCol;
    }

    *BufLen = 0;
    FillMemory(Buf, maxBufLen, SPACE);

    bounds->left = (CurCol-1)*mCharWidth;
    bounds->right = mWindowWidth;
	
    if(RowDone || bounds->left >= bounds->right) {
	bounds->left = 0;
	bounds->right = 0;
	lastRow = 0;
	return FALSE;
    }

	
    *Attr = *LCtoAddr(mAttrBuffer, curRow, CurCol);
    while(*Attr == (AttributeWord)*LCtoAddr(mAttrBuffer, curRow, CurCol)) {
	if(CurCol > mBufSZ.x) {RowDone = TRUE; break;}
	Buf[*BufLen] = *LCtoAddr(mBuffer, curRow, CurCol);
	CurCol++;

	(*BufLen)++;
	if(*BufLen == maxBufLen) break;
    }
    LastCol = CurCol;
    return TRUE;
}

void TerminalEngine::InvalidateRows(unsigned int topRow, unsigned int bottomRow)
{
    // Calculate compensation because origin is not always on char boundaries.
    POINT oldorg;
    GetWindowOrgEx(mHDC, &oldorg);
    int orgDiff = oldorg.y - (mOrigin.y-1)*mCharHeight;

    // Calculate invalid rectangle.
    RECT invRect = {0,0,0,0};
    invRect.right = mWindowWidth;
    invRect.top = mCharHeight*(topRow-1)- orgDiff;
    invRect.bottom = mCharHeight*bottomRow - orgDiff;
	
    InvalidateRect(mWindow, &invRect, TRUE);
}

void TerminalEngine::UpdateCaret(void)
{
    int x = mCursor.x, y = mCursor.y;
    LCtoBC(&y,&x);
    SetCaretPos(mCharWidth*(x-1), mCharHeight*(y-1));
}


int TerminalEngine::GetWindowSize(int* Width, int* Height)
{
    *Width = mWindowWidth;
    *Height = mWindowHeight;
    return 0;
}

int TerminalEngine::GetCharWindowSize(int *Cols, int* Rows)
{
    GetWindowSize(Cols, Rows);
    *Cols = (*Cols)/mCharWidth;
    *Rows = (*Rows)/mCharHeight;
    return 0;
}

// Logical Coordinates to Buffer Coordinates.
int TerminalEngine::LCtoBC(int* Row, int* Col)
{
    *Row += mOrigin.y-1;
    *Col += mOrigin.x-1;
    return 0;
}

char* TerminalEngine::LCtoAddr(void* BaseAddr, int row, int col)
{
    return (char*)BaseAddr+mBufSZ.x*(row-1)+col-1;
}

char* TerminalEngine::Cur2Addr(void* BaseAddr, int row, int col)
{
    LCtoBC(&row, &col);
    return (char*)BaseAddr+mBufSZ.x*(row-1)+col-1;
}


void TerminalEngine::SetOriginRelative(int How)
{
    int nPos;
    switch (How) {
    case SB_LINEUP:
	nPos = -mCharHeight;
	break;
    case SB_LINEDOWN:
	nPos = mCharHeight;
	break;
	
    case SB_PAGEUP:
	nPos = -mWindowHeight;
	break;
    case SB_PAGEDOWN:
	nPos = mWindowHeight;
	break;
    }
    // Get old origin
    POINT oldorg;
    GetWindowOrgEx(mHDC, &oldorg);
    SetOrigin(0, oldorg.y+nPos);
}
