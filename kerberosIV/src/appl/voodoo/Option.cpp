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

/* $KTH: Option.cpp,v 1.3 1999/12/02 16:58:34 joda Exp $ */

//
//
// Option
//
//

#include <string.h>
#include "Telnet.h"
#include "TelnetCodes.h"
#include "Option.h"
#include "TelnetEngine.h"

// State variable values

Option::Option (CharStream *str, TelnetEngine* engine, unsigned char opt)
{
    option = opt;
    stream = str;
    pTEngine = engine;;
	
    // Initialize state variables
}

Option::~Option ()
{
}

/*short
Option::OptionFor (unsigned char opt)
{
return option == opt;
}
*/
void
Option::SendReply (void *buff, short len)
{
    pTEngine->Send ((char *)buff, len);
}

short
Option::ReadToIACSE ()
{
    unsigned char c;

    while (1) {
	if (!stream->GetChar (&c))
	    return 1;
	if (c == IAC) {
	    if (!stream->GetChar (&c))
		return 1;
	    if (c == SE)
		break;
	}
    }
	
    return 0;
}

void
Option::SendWILL ()
{
    unsigned char buff[3];
	
    buff[0] = IAC;
    buff[1] = WILL;
    buff[2] = option;
	
    SendReply (buff, 3);
}

void
Option::PadBinaryBuff (unsigned char *in, unsigned long *inLen,
					   unsigned char *out, unsigned long *outLen)
{
    unsigned long i;
    int len = 0;
	
    for (i = 0; i < *inLen; i++) {
	if (*in == IAC) {
	    *out++ = IAC;
	    len++;
	}
	*out++ = *in++;
    }
    *outLen = *inLen + len;
}	



Option::Option(void)
{

}

// Override in subclasses if needed.
//void Option::ExecOption(unsigned int Command) {}

