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

/* $KTH: WinSizeOption.cpp,v 1.3 1999/12/02 16:58:35 joda Exp $ */

#include <string.h>
#include "WinSizeOption.h"
#include "EmulatorEngine.h"

void
WinSizeOption::NegotiateOption (unsigned char command)
{
	YesNoOption::NegotiateOption (command);
	negotiated = true;
	int width, height;
	pTEngine->mTelnetSession->mTerminalEngine->GetCharWindowSize(&width, &height);
	SendWinSize (width, height);
}

void WinSizeOption::ExecOption(unsigned int Command, void* Data)
{
	if(Command == WINSIZE_CHANGED)
		SendWinSize(((unsigned short*)Data)[0], ((unsigned short*)Data)[1]);
}

void
WinSizeOption::SendWinSize (unsigned short width, unsigned short height)
{
	unsigned char buff[9];
	if (negotiated) {
		buff[0] = IAC;
		buff[1] = SB;
		buff[2] = NAWS;
		buff[3] = width>>8;
		buff[4] = width&0xff;
		buff[5] = height>>8;
		buff[6] = height&0xff;
		buff[7] = IAC;
		buff[8] = SE;
		SendReply (buff, 9);
	}
}			