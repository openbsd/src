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

/* $KTH: EmulatorEngine.cpp,v 1.3 1999/12/02 16:58:34 joda Exp $ */

// EmulatorEngine.cpp
#include <Windows.h>
#include <stdlib.h>
#include "TelnetApp.h"
#include "TelnetSession.h"
#include "TelnetEngine.h"
#include "EmulatorEngine.h"
#include "TerminalEngine.h"
#include "WinSizeOption.h"
#include "char_codes.h"
#include "TelnetCodes.h"

#define host mTelnetSession->mTelnetEngine

EmulatorEngine::EmulatorEngine(TelnetSession* thisTS)
: TerminalEngine(thisTS)
{
    Reset();
}

inline void EmulatorEngine::Reset(void)
{
    terminalMode.i = 0;
    homeRow = 1;
    homeCol = 1;
    scrlRegTop = 1;
    scrlRegBottom = 1;
    marginLeft = marginRight = 1;
    savedCursorPosX = 1;
    savedCursorPosY = 1;
    savedAttr = 0;
    currentAttr = 0;
    bufIndex = 0;
}

inline void EmulatorEngine::ConvVT100Coord(int *newRow, int *newCol, int row, int col)
{
    if(terminalMode.u.originMode)	{
	*newRow = row + scrlRegTop - 1;
	*newCol = col + marginLeft - 1;
	if(*newRow > scrlRegBottom)
	    *newRow = scrlRegBottom;
	if(*newCol > marginRight)
	    *newCol = marginRight;
    } else {
	*newRow = row;
	*newCol = col;
    }
}

inline void EmulatorEngine::ConvTermCoord(int *newRow, int*newCol, int row, int col)
{
    if(terminalMode.u.originMode)
	{
	    *newRow = row - scrlRegTop + 1;
	    *newCol = col - marginLeft + 1;
	}
    else
	{
	    *newRow = row;
	    *newCol = col;
	}
} 

void EmulatorEngine::SendExtended(WPARAM Key)
{
    ConvertSendToHost(Key, 1);	
}

void
EmulatorEngine::ConvertSendToHost(char c, char ExtendedCode)
{
    if(ExtendedCode) {
	switch(c) {
	case UpArrow:
	    if(terminalMode.u.cursorKeysApplMode)
		AnswerHost(true, " OA", 3);
	    else
		AnswerHost(true, " [A", 3);
	    break;
	case DownArrow:
	    if(terminalMode.u.cursorKeysApplMode)
		AnswerHost(true, " OB", 3);
	    else
		AnswerHost(true, " [B", 3);
	    break;
	case LeftArrow:
	    if(terminalMode.u.cursorKeysApplMode)
		AnswerHost(true, " OD", 3);
	    else
		AnswerHost(true, " [D", 3);
	    break;
	case RightArrow:
	    if(terminalMode.u.cursorKeysApplMode)
		AnswerHost(true, " OC", 3);
	    else
		AnswerHost(true, " [C", 3);
	    break;
	case VK_DELETE:
	    c = ForwardDelete;
	    AnswerHost(false, &c, 1);
	    break;
	case VK_F1:
	    AnswerHost(true, " OP", 3);
	    break;
	case VK_F2:
	    AnswerHost(true, " OQ", 3);
	    break;
	case VK_F3:
	    AnswerHost(true, " OR", 3);
	    break;
	case VK_F4:
	    AnswerHost(true, " OS", 3);
	    break;
	}
    } else {
	switch(c) {
	case 0x0d:
	    host->Send(&c,1);
	    c = 0x0a;
	    host->Send(&c,1);
	    break;
	case BS:
	    if(false) {
		c = ForwardDelete;
		host->Send(&c,1);
	    } else {
		host->Send(&c,1);
	    }
	    break;
	default:
	    host->Send(&c,1);
	}
    }
}

void EmulatorEngine::AnswerHost(BOOL escSeq, char *s, int len)
{	
    if(escSeq)
	s[0] = ESC;
    host->Send(s, len);
}

int EmulatorEngine::ComposeEscString(char *string,int *arguments, int argCount, char command)
{
    int i = 1, c;
    string[i++] = '[';
    for(c = 0; c < argCount; c++) {
	char ArgumentString[20];
	itoa(arguments[c], ArgumentString, 10); 
	strcat(string+i,ArgumentString);
	i += strlen(ArgumentString);
	string[i++] = ';';
	if(c==argCount) i--; // delete last ';'
    }
    string[i++] = command;
    return i;
}	

BOOL EmulatorEngine::MatchEscSequence(CharStream *stream, BOOL vt100, char *pat, int patLen)
{
    unsigned char c;
    int i;
    if(vt100) {
	if(!stream->GetChar(&c)) return false;
	if(c!=ESC) {
	    stream->RestoreToMark();
	    return false;
	}
    }
    for(i=0;i<patLen;i++) {
	if(!stream->GetChar(&c)) return false;
	if(pat[i]!=(char)c) {
	    stream->RestoreToMark();
	    return false;
	}
    }
    stream->MarkAsRead();
    return true;
}
	
void EmulatorEngine::ReceiveBuffer(unsigned char *buffer, int bufSZ)
{
    unsigned char *VT100buf;
    unsigned int VT100bufSZ;
    mRecStream->PutChars(buffer, bufSZ);
    if(mRecStream->SkipTo(ESC, &VT100buf, &VT100bufSZ)) {
	if(VT100bufSZ > 0)
	    DoWriteBfr(VT100buf,VT100bufSZ);
	ReceivedFromHost(mRecStream);
    } else {
	if(VT100bufSZ == 0)
	    return;
	DoWriteBfr(VT100buf,VT100bufSZ);
    }
}

void EmulatorEngine::DoWriteBfr(unsigned char* Buffer, unsigned int BufSZ)
{
    for(unsigned int i=0; i<BufSZ; i++) {
	if(Buffer[i]<SPACE)
	    DoVT100Command(Buffer[i]);
	else
	    DrawText(Buffer+i,1);
    }
}

void EmulatorEngine::DoVT100Command(char Command)
{
    switch(Command) {
    case Linefeed:

	int row, col;
	GetCursorPos(&row, &col);
	if(row == scrlRegBottom) {
	    //for(i=1;MatchEscSequence(stream, false, "\n", 1);i++){}
	    ScrollRows(scrlRegTop, scrlRegBottom, -1);
	    ClearLineFromCursor();
	    //stream->MarkAsRead();					
	} else {
	    DrawText((unsigned char*)&Command, 1);
	}
	break;
	
    default:
	DrawText((unsigned char*)&Command, 1);
    }
}

void EmulatorEngine::Send(char *buf, unsigned int buflen)
{
    for(unsigned int i = 0; i < buflen; i++)
	ConvertSendToHost(buf[i], 0);	
}

static inline int vt_value(int arc, int *arv, int pos, int def)
{
    if(pos >= arc || arv[pos] == 0)
	return def;
    return arv[pos];
}

BOOL EmulatorEngine::ReceivedFromHost(CharStream *stream)
{
    int row, col, i;
    unsigned char c;
    int arv[100], arc = 0;
	
    if(!stream->GetChar(&c)) return false;

    if(c == 255) {
	TelnetApp::Error(0, "EmulatorEngine: c == 255.");
	return false;
    }
	
    switch(c) {
    case ESC:
	if(!stream->GetChar(&c)) {
	    stream->RestoreToMark();
	    return false;
	}
	switch(c) {
	case '[':
	    if(!ReadArgs(stream, arv, &arc, &c)) { 
		stream->RestoreToMark();
		return false;
	    }
	    switch(c) {
	    case 'A':
	    case 'B':
	    case 'C':
	    case 'D':
		{
		    int num = 0;
		    if(arc)
			num = arv[0];
		    if(num == 0)
			num = 1;
		    switch(c) {
		    case 'A':
			MoveCursor(-num, 0); 
			break;
		    case 'B':
			MoveCursor(num, 0);
			break;
		    case 'C':
			MoveCursor(0, num);
			break;
		    case 'D':
			MoveCursor(0, -num);
			break;
		    }
		    break;
		}
	    case 'H':
	    case 'f': //cursor direct address
		{
		    row = vt_value(arc, arv, 0, 1);
		    col = vt_value(arc, arv, 1, 1);
		    if(row<=0) row=1; 
		    if(col<=0) col=1;
		    ConvVT100Coord(&row, &col, row, col);
		    PlaceCursor(row,col);
		    break;
		}
	    case 'n': //cursor pos report, answer ESC[pl;pcR
		{
		    char s[10];
		    int a[2], len;
		    GetCursorPos(&row,&col);
		    ConvTermCoord(a, a+1, row, col);
		    len = ComposeEscString(s,a,2,'R');
		    AnswerHost(true, s, len);
		    break;
		}
	    case 'm': //attributes
		{
		    SetAttr(ExtractArg(arv, arc));
		    break;
		}
	    case 'P': //delete char
		DeleteChars(vt_value(arc, arv, 0, 1));
		break;
	    case '@': //insert char
		InsertChars(vt_value(arc, arv, 0, 1));
		break;
	    case 'L': //insert line
		InsertLines(mCursor.y, scrlRegBottom, vt_value(arc, arv, 0, 1));
		break;
	    case 'M': //delete line
		DeleteLines(mCursor.y, scrlRegBottom, vt_value(arc, arv, 0, 1));
		break;
	    case 'J': //erase screen
		{
		    int alt = 0;
		    if(arc != 0) alt = arv[0];
		    switch(alt) {
		    case 0:
			ClearScreenFromCursor(); 
			break;
		    case 1:
			ClearScreenToCursor(); 
			break;
		    case 2:
			ClearScreen(); 
			break;
		    }
		    break;
		}
	    case 'K': // erase line
		{
		    int alt = 0;
		    if(arc != 0) alt = arv[0];
		    switch(alt) {
		    case 0:
			ClearLineFromCursor();
			break;
		    case 1:
			ClearLineToCursor();
			break;
		    case 2:
			ClearLineToCursor();
			ClearLineFromCursor();
			break;
		    }
		    break;
		}
	    case 'g': // clear tab
		{
		    switch(vt_value(arc, arv, 0, 0)) {
		    case 0:
			remove_tab(mCursor.x);
			break;
		    case 3:
			remove_all_tabs();
			break;
		    }
		    break;
		}
	    case 'r': // set scrolling region
		{
		    int width, height;
		    GetCharWindowSize(&width, &height);
		    int top=1, bottom=height;

		    if(arc==1)
			top=arv[0];
		    else if(arc==2) {
			top=arv[0];
			bottom=arv[1];
		    }
		    scrlRegTop = top;
		    scrlRegBottom = bottom;
		    ConvVT100Coord(&row, &col, 1, 1);
		    PlaceCursor(row, col);
		    break;
		}
	    case 'c': //what are you?
		{
		    // answer ESC[?1; ps c
		    // ps = 0 base vt100, 1 processor opt(stp), 2 adv video opt(avo),
		    // 4 graphics processor opt(gpo)
		    AnswerHost(true, " [?1;2c", 7); 
		    break;
		}
	    case '?': // Set, reset private modes 
		if(!ReadArgs(stream, arv, &arc, &c)) {
		    stream->RestoreToMark();
		    return false;
		}
		SetMode(c, 0, arv, arc);
		break;
	    case 'h': // Set, reset ansi modes
	    case 'l':
		SetMode(c, 1, arv, arc);
		break;
	    default: 
		UnrecognizedEscSequence(stream, c);
		break;
	    }
	    stream->MarkAsRead();
	    break;
	case ']':
	    {
		int num;
		if(!ReadNum(stream, '0', &num, &c)){
		    stream->RestoreToMark();
		    return false;
		}
		if(c != ';'){
		    stream->MarkAsRead();
		    return true;
		}
		char title[64];
		int pos = 0;
		while(stream->GetChar(&c)){
		    if(c == '\a')
			break;
		    title[pos++] = c;
		}
		if(c != '\a'){
		    stream->RestoreToMark();
		    return false;
		}
		title[pos] = 0;
		stream->MarkAsRead();
		if(num == 0 || num == 2)
		    SetWindowTitle(title);
		break;
	    }
	case 'D': // Cursor down at bottom of region scroll up
	    {
		GetCursorPos(&row, &col);
		if(row == scrlRegBottom)
		    {
			stream->MarkAsRead();
			for(i=1;MatchEscSequence(stream, true, "D", 1);i++){}
			ScrollRows(scrlRegTop, scrlRegBottom, -i);
			ClearLineFromCursor();
		    }
		else
		    MoveCursor(1,0);
		stream->MarkAsRead();
		break;
	    }
	case 'M': // Cursor up at top of region scroll down
	    {
		GetCursorPos(&row, &col);
		if(row == scrlRegTop)
		    {
			stream->MarkAsRead();
			for(i=1;MatchEscSequence(stream, true, "M", 1);i++){}
			ScrollRows(scrlRegTop, scrlRegBottom, i);
			ClearLineFromCursor();
		    }
		else
		    MoveCursor(-1,0);
		stream->MarkAsRead();
		break;
	    }
	case 'E': // next line, same as CR LF
	    {
		char buf[2]; buf[0] = Return; buf[1] = Linefeed;
		DoWriteBfr((unsigned char*)buf,2);
		stream->MarkAsRead();
		break;
	    }
	case 'Z': //what are you?
	    {
		// answer ESC[?1; ps c
		// ps = 0 base vt100, 1 processor opt(stp), 2 adv video opt(avo),
		// 4 graphics processor opt(gpo)
		AnswerHost(true, " [?1;2c", 7); 
		stream->MarkAsRead();
		break;
	    }
	case 'H': // set tab at column
	    set_tab(mCursor.x);
	    break;
	case '7': // save cursor and attributes
	    {
		GetCursorPos(&savedCursorPosX, &savedCursorPosY);
		savedAttr = currentAttr;
		stream->MarkAsRead();
		break;
	    }
	case '8': // restore cursor and attributes
	    {
		PlaceCursor(savedCursorPosX, savedCursorPosY);
		SetAttr(savedAttr);
		stream->MarkAsRead();
		break;
	    }
	case '(':
	    if(!stream->GetChar(&c)) {
		stream->RestoreToMark();
		return false;
	    }
	    stream->MarkAsRead();
	    UnimplementedEscSequence(stream, "Set character-set, Esc ( (A|B|0)");
	    switch(c) {
	    case 'A': break; // UK char set as G0
	    case 'B': break; // US cha set as G0
	    case '0': break; // Line char set as G0
	    default: UnrecognizedEscSequence(stream, c); break;
	    }
	    break;
	case ')':
	    if(!stream->GetChar(&c)) {
		stream->RestoreToMark();
		return false;
	    }
	    stream->MarkAsRead();
	    UnimplementedEscSequence(stream, "Set character-set, Esc ) (A|B|0)");
	    switch(c) {
	    case 'A': break; // UK char set as G1
	    case 'B': break; // US cha set as G1
	    case '0': break; // Line char set as G1
	    default: UnrecognizedEscSequence(stream, c); break;
	    }
	    break;
	case 'N': // Select G2 for next char 
	    UnimplementedEscSequence(stream, "Select G2 for next char, Esc N");
	    break;
	case 'O': // Select G3 for next char
	    UnimplementedEscSequence(stream, "Select G3 for next char, Esc O");
	    break;
	case '=': // Keypad application mode
	    SetMode(c, 0, 0, 0);
	    break;
	case '>': // Keypad numeric mode
	case '<': // ANSI mode
	    SetMode(c, 0, 0, 0);
	    break;
	case 'c': 
	    Reset();
	    break; //reset
	case '#':
	    if(!stream->GetChar(&c)){
		stream->RestoreToMark();
		return false;
	    }
	    stream->MarkAsRead();
	    if(c == '8')
		FillWithE();
	    else if(isdigit(c))
		    ;
	    else
		UnimplementedEscSequence(stream, "#");
	    break;

	default:
	    UnrecognizedEscSequence(stream, c);
	    break;
	}
	break;

    default:
	//unsigned char *data;
	//unsigned int size;
	//stream->UngetChar();
	//stream->SkipTo(ESC, &data, &size);
	DoWriteBfr(&c, 1);
	stream->MarkAsRead();
    }
    return true;

}


Boolean 
EmulatorEngine::ReadNum(CharStream *stream, char c, int *num, 
			unsigned char *lookAhead)
{
    char s[100];
    int i = 0;
    s[i++] = c;
    while(stream->GetChar(lookAhead)) {
	if(isdigit(*lookAhead)) {
	    if(i<100)
		s[i++] = *lookAhead;
	} else {
	    if(i<100) {
		s[i] = '\0';
		*num = atoi(s);
		return true;
	    } else {
		*num = 0;
		return true;
	    }
	}
    }
    return false;
}


Boolean
EmulatorEngine::ReadArgs(CharStream *stream, int *arv, int *arc, 
			 unsigned char *lookAhead)
{
    *arc = 0;
    while(stream->GetChar(lookAhead)) {
	if(isdigit(*lookAhead)) {
	    if(!ReadNum(stream, *lookAhead, arv+*arc, lookAhead)) return false;
	    (*arc)++;
	    if(*lookAhead != ';')
		return true;
	} else if(*lookAhead == ';') {
	    arv[(*arc)++] = 0;
	} else {
	    if(*arc > 0)
		arv[(*arc)++] = 0;
	    return true;
	}
    }
    return false;
}

AttributeWord EmulatorEngine::ExtractArg(int *arv, int arc)
{
    int i;
    AttributeWord res = 0;
    if(arc == 0) return attrOff;
    for(i=0;i<arc;i++) {
	if(arv[i] == Off)
	    res = attrOff;
	else if(arv[i] == Bold)
	    res |= BOLD;
	else if(arv[i] == Underscore)
	    res |= UNDERSCORE;
	else if(arv[i] == Blink)
	    res |= BLINK;
	else if(arv[i] == Reverse)
	    res |= REVERSE;
    }
    return res;
}

void EmulatorEngine::SetAttr(AttributeWord attr)
{
    if(attr&attrOff) {
	TerminalEngine::ClearAttribute(currentAttr);
	attr &= attrOff-1;
    }
    TerminalEngine::SetAttribute(attr);
    currentAttr = attr;
}

Boolean
EmulatorEngine::ReadModeArgs(CharStream *stream,
			     int *arv,
			     int *arc,
			     unsigned char *lookAhead)
{
    while(stream->GetChar(lookAhead)) {
	if(isdigit(*lookAhead))
	    arv[(*arc)++] = *lookAhead-'0';
	else 
	    return true;
    }
    return false;
}
	
void EmulatorEngine::SetMode(char c, int fooflag, int *arv, int arc)
{
    if(c == '=')
	terminalMode.u.keypadApplMode = 1;
    else if(c == '>')
	terminalMode.u.keypadApplMode = 0;
    else if(c == '<')
	terminalMode.u.ansiMode = 1;
    else if(c == 'h' || c == 'l'){
	for(int i=0; i<arc; i++) {
	    int set = (c == 'h');
	    switch(arv[i] + fooflag * 20){
	    case 1:
		terminalMode.u.cursorKeysApplMode = set;
		break;
	    case 2:
		terminalMode.u.ansiMode = set;
		break;
	    case 3:
		terminalMode.u.col132Mode = set;
		break;
	    case 4:
		terminalMode.u.smoothScrollMode = set;
		break;
	    case 6:
		terminalMode.u.originMode = set;
		break;
	    case 7:
		terminalMode.u.autoWrapMode = set;
		break;
	    case 8:
		terminalMode.u.autoRepeatMode = set;
		break;
	    case 9:
		terminalMode.u.interlaceMode = set;
		break;


	    case 24:
		terminalMode.u.insertMode = set;
		break;
	    case 32:
		terminalMode.u.newLineMode = set;
		break;
	    }
	}
	if(terminalMode.u.originMode) {
	    homeRow = scrlRegTop;
	    homeCol = marginLeft;
	} else {
	    homeRow = 1;
	    homeCol = 1;
	}
    }
}

BOOL EmulatorEngine::InsertMode(void)
{
    return terminalMode.u.insertMode;
}

BOOL EmulatorEngine::AutoWrapMode(void)
{
    return terminalMode.u.autoWrapMode;
}

void EmulatorEngine::UnimplementedEscSequence(CharStream *stream, char *msg)
{
    OutputDebugString("Unimplemented VT-100 command:\n");
    OutputDebugString(msg);
    OutputDebugString("\n");
    stream->MarkAsRead();
}

void EmulatorEngine::UnrecognizedEscSequence(CharStream *stream, char c)
{
    char *msg = "Unrecognized VT100-command:       \n";
    msg[30] = c;
    OutputDebugString(msg);
    stream->MarkAsRead();
}



int EmulatorEngine::SetWindowSize(int Width, int Height)
{
    int OldWidth, OldHeight;
    TerminalEngine::GetWindowSize(&OldWidth, &OldHeight);
    if(Width!=OldWidth||Height!=OldHeight) {
	TerminalEngine::SetWindowSize(Width, Height);
	SizeChanged = TRUE;
    }
    SetTimer(mWindow, 1, 100, NULL);
    return 0;
}

int EmulatorEngine::UpdateWindowSize(void)
{
    int Width, Height;
    if(SizeChanged) {
	TerminalEngine::GetCharWindowSize(&Width, &Height);
	if(Width < 0) Width = 0;
	if(Width > 1<<15) Width = 1<<15;
	if(Height < 0) Height = 0;
	if(Height > 1<<15) Height = 1<<15;
	unsigned short Data[2] = {Width, Height}; 
	mTelnetSession->InvokeCommand(WINSIZE_CHANGED, &Data);
	if(scrlRegTop == 1 && scrlRegBottom == windowRows)
	    scrlRegBottom = Height;
	windowRows = Height;
	windowCols = Width;
	SizeChanged = FALSE;
    }
    KillTimer(mWindow, 1);
    return 0;
}

void EmulatorEngine::Open(void)
{
    TerminalEngine::Open();
    int Width, Height;
    GetCharWindowSize(&Width, &Height);
    windowRows = Height;
    windowCols = Width;
    scrlRegTop = 1;
    scrlRegBottom = Height;
}
