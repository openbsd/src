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
// Option
//
//

#ifndef __OPTION_H__
#define __OPTION_H__

// Define some ansi C++ keywords.
#define ANSI_BOOLEAN
#define Boolean bool
#define bool	BOOL
#define true	TRUE
#define false	FALSE

#include "CharStream.h"
#include "TelnetCodes.h"

class TelnetEngine;

//
//
// Option
//
// Base class for options
//
class Option {
public:
    // Constructors and destructors
    Option(void);
    ~Option ();
    Option (CharStream* str, TelnetEngine *engine, unsigned char option);

    // NegotiateOption, override in subclasses.
    virtual void  NegotiateOption (unsigned char command) = 0;

    // ExecOption, override in subclasses if needed.
    virtual void ExecOption(unsigned int Command, void* Data = NULL) {}
	
    // OptionFor, returns true if object negotiates opt
    // Overridden in DenyAllOption only (where it returns true for all opt:s)
    // virtual short OptionFor (unsigned char opt);

    virtual BOOL InEffect(void) = 0;

    unsigned char option;

protected:
    void          SendReply (void* buff, short len);
    void          SendWILL ();
    short         ReadToIACSE ();
    void          PadBinaryBuff (unsigned char *in, unsigned long *inLen,
				 unsigned char *out, unsigned long *outLen);
	
	
	
    // Variables
    CharStream    *stream;
    TelnetEngine  *pTEngine;
    // State variables as in RFC 1143
    short         us, usq;
    short         him, himq;
};

#endif /* __OPTION_H__ */
