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

/* $KTH: TelnetSession.cpp,v 1.6 1999/12/02 16:58:35 joda Exp $ */

// TelnetSession.cpp
// Author: Jörgen Karlsson - d93-jka@nada.kth.se

#include <string.h>
#include <winsock.h>
#include "Telnet.h"
#include "Resource.h"
#include "TelnetApp.h"
#include "TelnetEngine.h"
#include "EmulatorEngine.h"
//#include "TerminalEngine.h"
#include "TelnetSession.h"
#include "WinSizeOption.h"
#include "EncryptOption.h"

TelnetSession::TelnetSession(void)
{
    mTelnetEngine	=		new TelnetEngine(this);
    mTerminalEngine	=		new EmulatorEngine(this);
    //	mCryptoEngine	=		new CryptoEngine(&mTelnetEngine->enckey);
}

BOOL TelnetSession::Connect(char *hostname, char *username)
{
    mTerminalEngine->Open();
    mTerminalEngine->SetWindowTitle(hostname);
    mTelnetEngine->Connect(hostname, username);

    // XXX - Save in registry?

    return TRUE;
}

void TelnetSession::RecCallback(CharStream *RecStream)
{
    unsigned char c;
    
    while(RecStream->GetChar(&c)) {
	RecStream->UngetChar();
	if(c == IAC) {
	    mTelnetEngine->TelnetNegotiate(RecStream);
	} else {
	    if(!mTerminalEngine->ReceivedFromHost(RecStream))
		return;
	}
		
    }
}

void TelnetSession::ConnectionBroken(void)
{
//    unsigned char msg[] = "Connection closed by remote host.\n";
//    mTerminalEngine->ReceiveBuffer(msg, sizeof(msg));
    TelnetApp::Message("Connection closed by foreign host.");
    Close();
}

void TelnetSession::Close(void)
{
    mTelnetEngine->Close();
    mTerminalEngine->Close();
    theApp->SessionClosed(this);
}

void TelnetSession::InvokeCommand(unsigned short command, void* Data)
{
    char *msg_text;
    switch(command) {
	// Connection menu items.	
    case ID_CONNECTION_CLOSE:
	Close();
	break;

	// Telnet menu items
    case ID_TELNET_SEND_AYT:
	mTelnetEngine->SendCommand(AYT);
	break;
	
    case ID_TELNET_SEND_BRK:
	mTelnetEngine->SendCommand(BRK);
	break;
	
    case ID_TELNET_SEND_IP:
	mTelnetEngine->SendCommand(IP);
	break;
	
    case ID_TELNET_SEND_AO:
	mTelnetEngine->SendCommand(AO);
	break;
	
    case ID_TELNET_SEND_EC:
	mTelnetEngine->SendCommand(EC);
	break;

    case ID_TELNET_SEND_EL:
	mTelnetEngine->SendCommand(EL);
	break;

	// Encryption menu items.
    case ID_ENCRYPT_OPTION:
/*
	removed by flag
	mTelnetEngine->InvokeOption(ENCRYPT_OPT, !mTerminalEngine->GetEncryptFlag());
*/
	break;

	// About menu items.
    case ID_SHOWVERSION:
	msg_text = "Voodoo version 1.1.1\n"
	    "Copyright © 1996 - 1998 Kungliga Tekniska Högskolan";
	MessageBox(mTerminalEngine->mWindow, msg_text,
		   "About Voodoo Telnet", MB_OK|MB_ICONINFORMATION);
	break;
		
	// Window size changed.
    case WINSIZE_CHANGED:
	mTelnetEngine->InvokeOption(NAWS, WINSIZE_CHANGED, Data);
	break;
    }
}

