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

#ifndef __EMULATORENGINE_H__
#define __EMULATORENGINE_H__

#include "TerminalEngine.h"
#include "CharStream.h"

#ifndef ANSI_BOOLEAN
#define Boolean BOOL
#define true TRUE
#define false FALSE
#endif

const int bufSize = 100;
typedef		unsigned char	AttributeWord;

class EmulatorEngine  : public TerminalEngine {

public:
    void Open(void);
    int UpdateWindowSize(void);
    int SetWindowSize(int Width, int Height);
    void DoVT100Command(char Command);
    enum { class_ID = 'EMUL' };
    enum { attrOff = 16,
	   Off = 0,
	   Bold = 1,
	   Underscore = 4,
	   Blink = 5,
	   Reverse = 7 };

    union {
	int i;
	struct {
	    int cursorKeysApplMode:1;
	    int ansiMode:1;
	    int col132Mode:1;
	    int smoothScrollMode:1;
	    int originMode:1;
	    int autoWrapMode:1;
	    int autoRepeatMode:1;
	    int interlaceMode:1;
	    int keypadApplMode:1;
		
	    int insertMode:1;
	    int newLineMode:1;
	} u;
    } terminalMode;
	    
	    
    //TelnetEngine			*host;

    EmulatorEngine(TelnetSession*);						
    void ReceiveBuffer(unsigned char *buffer, int bufSZ);
    BOOL ReceivedFromHost(CharStream *);
	
    void SendExtended(WPARAM Key);
    void ConvertSendToHost(char c, char keyCode);
    void Send(char *buf, unsigned int buflen);
    BOOL AutoWrapMode(void);
    BOOL InsertMode(void);
private:
    void DoWriteBfr(unsigned char* Buffer, unsigned int BufSZ);
    int homeRow, homeCol, scrlRegTop, scrlRegBottom, windowRows, windowCols;
    int marginLeft, marginRight;
    int savedCursorPosX, savedCursorPosY, savedAttr, currentAttr;

    char buffer[bufSize];
    int bufIndex;


    void Reset(void);
    BOOL ReadModeArgs(	CharStream *stream,
			int *arv,
			int *arc,
			unsigned char *lookAhead);
			
    void SetMode(char c, int, int *arv, int arc);
    void AnswerHost(BOOL vt100, char *s, int len);
    int ComposeEscString(char *string,int *arguments, int argCount, char command);
    BOOL ReadNum(CharStream *stream, char c, int *num, unsigned char *lookAhead);

    BOOL MatchEscSequence(CharStream *stream, BOOL vt100, char *pat, int patLen);
    BOOL
    ReadArgs(CharStream *stream, int *arv, int *arc, unsigned char *lookAhead);
    AttributeWord ExtractArg(int *arv, int arc);
    void SetAttr(AttributeWord attr);
    inline void ConvVT100Coord(int *newRow, int *newCol, int row, int col);
    inline void ConvTermCoord(int *newRow, int *newCol, int row, int col);
    void UnimplementedEscSequence(CharStream *stream, char *msg);	
    void UnrecognizedEscSequence(CharStream *stream, char c);

};

#endif /* __EMULATORENGINE_H__ */
