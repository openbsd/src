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


//
//
// YesNoOption
//
// Base class for simple Yes/No options such as ECHO
// Do NOT instantiate from this class!!
//

#ifndef __YESNOOPTIONS_H__
#define __YESNOOPTIONS_H__


#include "CharStream.h"
#include "TelnetCodes.h"
#include "Option.h"


class YesNoOption : public Option {
    BOOL will, doo;
public:
    YesNoOption (CharStream* stream, TelnetEngine *engine, unsigned char option)
	: Option (stream, engine, option)
    {
    }
    void NegotiateOption (unsigned char);
    BOOL InEffect() { return will || doo; }
};


//
//
// EchoOption
//
class EchoOption : public YesNoOption {
public:
	EchoOption (CharStream *stream, TelnetEngine *engine)
	: YesNoOption (stream, engine, ECHO)
	{}
};


//
//
// SgaOption
//
class SgaOption : public YesNoOption {
public:
	SgaOption (CharStream *stream, TelnetEngine *engine)
	: YesNoOption (stream, engine, SGA)
	{}
};

// Terminal type

class TTOption : public YesNoOption {
private:
    enum { TELQUAL_IS = 0, TELQUAL_SEND = 1 };
public:
    TTOption (CharStream* stream, 
	      TelnetEngine *engine) : YesNoOption (stream, engine, TTYPE)
    {
    }

    void  NegotiateOption (unsigned char command)
    {
	if(command == SB){
	    unsigned char c;
	    if(!stream->GetChar(&c))
		return;
	    if (ReadToIACSE ())
		return;
	    stream->MarkAsRead ();
	    if(c == SEND){
		unsigned char mBuf[] = { IAC, SB, TTYPE, IS, 
					 'x', 't', 'e', 'r', 'm', 
					 IAC, SE };
		SendReply(mBuf, sizeof(mBuf));
	    }
	}else
	    YesNoOption::NegotiateOption(command);
    }
};

#endif /* __YESNOOPTIONS_H__ */
