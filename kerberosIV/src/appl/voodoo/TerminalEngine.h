/* -*- C++ -*- */
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
// TerminalEngine.h
// Author: Jörgen Karlsson - d93-jka@nada.kth.se

#ifndef __TERMINALENGINE_H__
#define __TERMINALENGINE_H__

#include <Windows.h>
#include "resource.h"
#include <iostream.h>

#include "TelnetSession.h"

typedef		unsigned char		AttributeWord;
enum
{	
    BOLD	=	1<<0,
    UNDERSCORE	=	1<<1,
    BLINK 	=	1<<2,
    REVERSE	=	1<<3,
};

class TerminalEngine
{
public:
    void SetOriginRelative(int How);
    int AdjustCharOrigin(void);
    char* LCtoAddr(void* BaseAddr, int row, int col);
    int LCtoBC(int *Row, int* Col);
    BOOL SizeChanged;
    int GetCharWindowSize(int *Cols, int* Rows);
    int GetWindowSize(int* Width, int* Height);
    void UpdateCaret(void);
    void InvalidateRows(unsigned int topRow, unsigned int bottomRow);
    BOOL BuildPrintable(int curRow, RECT *bounds,
			char *Buf, int maxBufLen,
			int *BufLen, AttributeWord *Attr);

	

    void SetAttribute(AttributeWord AttrWord);
    void ClearAttribute(AttributeWord AttrWord);
    void Redraw(RECT invRect);
	
    HWND mWindow;
    HDC mHDC;
	
    void SetWindowSize(int x, int y);
    void SetWindowTitle(const char *title) 
    {
	SetWindowText(mWindow, title);
    }
    BOOL GetEncryptFlag()
    {
	HMENU menu = GetMenu(mWindow);
	return GetMenuState(menu, ID_ENCRYPT_OPTION, 0) & MF_CHECKED;
    }
    void SetEncryptFlag(BOOL on)
    {
	HMENU menu = GetMenu(mWindow);
	CheckMenuItem(menu, ID_ENCRYPT_OPTION, on ? MF_CHECKED : MF_UNCHECKED);
	DrawMenuBar(menu);
    }
    void SetOrigin(int x, int y);
    char* Cur2Addr(void* BaseAddr, int row, int col);
    void ClearScreen(void);
    void FillWithE(void);
    void ClearScreenToCursor(void);
    void ClearScreenFromCursor(void);
    void ClearLineToCursor(void);
    void ClearLineFromCursor(void);
    void DeleteChars(int num);
    void InsertChars(int num);
    void DeleteLines(int, int, int);
    void InsertLines(int, int, int);
    void ScrollRows(int ScrRegTop, int ScrRegBot, int rows);
    void GetCursorPos(int *x, int *y);
    void PlaceCursor(int x, int y);
    void MoveCursor(int x, int y);
    virtual BOOL AutoWrapMode(void) { return FALSE; }
    virtual BOOL InsertMode(void) { return FALSE; }
    TelnetSession * mTelnetSession;
    ~TerminalEngine(void);
    TerminalEngine(TelnetSession* );
    CharStream * mRecStream;
    BOOL AdjustOrigin(void);
    void Clear(char);
    void Scroll(int Rows);
    int mWindowHeight;
    int mWindowWidth;
    void Close(void);
    void RedrawAll(void);
    void DrawText(unsigned char *buf, unsigned int buflen);
    void Open(void);
    static LRESULT CALLBACK WindowProc(HWND window, UINT msg, WPARAM wParam, LPARAM lParam);

    void init_tabs(void);
    void remove_tab(int col);
    void remove_all_tabs(void);
    void set_tab(int col);
    int mCharHeight;
    int mCharWidth;
    POINT mCursor, mOrigin;

private:
    POINT mBufSZ;
    char *mBuffer;
    AttributeWord		mCurrentAttr,	*mAttrBuffer;				

    HFONT stdFont;
    HFONT boldFont; 
    HFONT uscoreFont, bolduscoreFont;

    COLORREF mTextColor;
    COLORREF mBkColor;
    int *tabs;
};

#endif /* __TERMINALENGINE_H__ */
