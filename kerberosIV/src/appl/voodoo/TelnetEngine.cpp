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

/* $KTH: TelnetEngine.cpp,v 1.6 1999/12/02 16:58:35 joda Exp $ */

// TelnetEngine.cpp
// Author: Jörgen Karlsson - d93-jka@nada.kth.se

#include <fstream.h>

#include <string.h>
#include <winsock.h>
#include "Option.h"
#include "WinSizeOption.h"
#include "YesNoOptions.h"
#include "EncryptOption.h"
#include "Telnet.h"
#include "TelnetApp.h"
#include "EmulatorEngine.h"
#include "AuthOption.h"
#include "CryptoEngine.h"
#include "TelnetEngine.h"
#include "TelnetSession.h"
#include "DenyAllOption.h"

BOOL TelnetEngine::Connect(char *hostname, char *username)
{
    extern char telnet_port_str[];
    char dbg_str[1024] = "";
    int sockerr;

    mNegotiator = new Negotiator(&mRecStream, this);
    mNegotiator->RegisterOption(new AuthOption(&mRecStream, this, 
					       hostname, username));
    mNegotiator->RegisterOption(new EncryptOption(&mRecStream, this));
    mNegotiator->RegisterOption(new WinSizeOption(&mRecStream, this));
    mNegotiator->RegisterOption(new EchoOption(&mRecStream, this));
    mNegotiator->RegisterOption(new SgaOption(&mRecStream, this));
    mNegotiator->RegisterOption(new TTOption(&mRecStream, this));

    PSERVENT serv;
    PHOSTENT host;
    SOCKADDR_IN remoteAddr;
    unsigned long in_addr;

    if((mSocket = socket(PF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
	OutputDebugString("TelnetEngine::Connect: Unable to create socket.");
    if ((remoteAddr.sin_port = htons(atoi(telnet_port_str))) == 0) {
      if ((serv = getservbyname(telnet_port_str, "tcp")) == NULL)
	serv = getservbyname("telnet","tcp");
      remoteAddr.sin_port = serv->s_port;
    }

    host = gethostbyname(hostname);
    if (host == NULL) {
        sockerr = WSAGetLastError();
        TelnetApp::Error(sockerr,
			 "Unable to find the hostname");
    }
    remoteAddr.sin_family = AF_INET;
    in_addr = *(unsigned long*)(host->h_addr_list[0]);
    remoteAddr.sin_addr.S_un.S_addr = in_addr;
    sockerr = connect(mSocket,(PSOCKADDR) &remoteAddr, sizeof(remoteAddr));
    if(sockerr == SOCKET_ERROR) {
	sockerr = WSAGetLastError();
	TelnetApp::Error(sockerr,
			 "Network Error - unable"
			 " to establish network connection"
			 " [TelnetEngine::Connect]");
    }

    WSAAsyncSelect(	mSocket, mTelnetSession->mTerminalEngine->mWindow,
			SM_READ, FD_READ|FD_CLOSE);
    return TRUE;
}

void TelnetEngine::TelnetNegotiate(CharStream *RecStream)
{
    unsigned char c;
	
    if(!RecStream->GetChar (&c)) return;
    switch (c) {
    case IAC:                   
	if(!RecStream->GetChar(&c)) return;
	switch (c) {            // Begin telnet commands
	case NOP:
	    RecStream->MarkAsRead ();
	    break;
	case DMARK:
	case BRK:       
	case IP:
	case AO: 
	case AYT: 
	case EC:  
	case EL:            
	case GA:
	    // Above commands not implemented yet so ignore them
	    RecStream->MarkAsRead ();
	    break;
	case SB:            // Begin options commands
	case DO:
	case WILL:
	case DONT:
	case WONT:
	    RecStream->UngetChar ();
	    mNegotiator->Negotiate();
	    break;
	case IAC:
	    RecStream->UngetChar (); // Char 255 doubled as IAC IAC
	default:
	    break;          // Shouldn't get here.
	}
	break;		
    default:
	RecStream->UngetChar ();
	break;
    }
}


void TelnetEngine::Send(char *buf, int bufSZ)
{
    pUChar cipherBuf = new unsigned char[bufSZ];

    if(mEncrypt) {
	mCryptoEngine->Encrypt((pUChar)buf, cipherBuf, bufSZ);
    } else {
	CopyMemory(cipherBuf, buf, bufSZ);
    }

    if(::send(mSocket, (const char*)cipherBuf, bufSZ, 0) == SOCKET_ERROR)
	TelnetApp::Error(WSAGetLastError(), "TelnetEngine::Send - Unable to send.\n");

    delete []cipherBuf;
}

void TelnetEngine::SendCommand(const int command)
{
    char buf[2] = {char(IAC)};
    buf[1] = command;
    Send(buf, 2);
}

unsigned long WINAPI TelnetEngine::RecvThread(void *thisTelnetEngine)
{
    ((TelnetEngine *)thisTelnetEngine)->ReadSocket();
    return 0;
}

void TelnetEngine::ReadSocket(void)
{
    int textBufSZ, sockErr;
    const int inbufSZ = 256;
    char inbuf[inbufSZ];
    if(textBufSZ = recv(mSocket, inbuf, inbufSZ, 0)) {
	if(textBufSZ == SOCKET_ERROR) {
	    sockErr = WSAGetLastError();
	    if(sockErr == WSAESHUTDOWN) {
		return;
	    } else {
		OutputDebugString("TelnetEngine::Receive: recv failed.\n");
		exit(sockErr);
	    }
	}
		
	//mRawStream.PutChars((unsigned char*)inbuf, textBufSZ);
	Receive(inbuf, textBufSZ);
    } else
	// Connection broken, ie closed by remote host.
	mTelnetSession->ConnectionBroken();
}

void TelnetEngine::Receive(char* Buffer, unsigned int Size)
{
    //ofstream file("decrypt", ios::out|ios::app|ios::binary);
    pUChar TextBuf = new unsigned char[Size];
    if(mDecrypt) {
	mCryptoEngine->Decrypt((pUChar)Buffer, TextBuf, Size);
    } else {
	CopyMemory(TextBuf, Buffer, Size);
    }

    //file.write(Buffer, Size);
    //file << ":--------------:";
    //file.write(TextBuf, Size);
    //file << "%***************%";
    mRecStream.PutChars(TextBuf, Size);
    mTelnetSession->RecCallback(&mRecStream);
    delete []TextBuf;	
}

void TelnetEngine::Close(void)
{
    // Close socket.
    if(closesocket(mSocket) == SOCKET_ERROR)
	TelnetApp::Error(WSAGetLastError(),
			 "TelnetEngine::Close: Failed to close socket. \n");
}

TelnetEngine::TelnetEngine(TelnetSession *thisTelnetSession)
{
    mTelnetSession = thisTelnetSession;
    mNegotiator = NULL;
    mCryptoEngine = NULL;
    mDecrypt = FALSE;
    mEncrypt = FALSE;
    mAuthenticated = TRUE;
    KClientInitSession(&mKClientSession, 0, 0, 0, 0);
}


void TelnetEngine::InvokeOption(unsigned char Opt, unsigned int Command, void* Data)
{
    Option *i;
    if(mNegotiator) {
	i = mNegotiator->FindOptionFor(Opt);
	i->ExecOption(Command, Data);
    }
}


void TelnetEngine::DeleteDoubleIAC(unsigned char* Buffer, unsigned int* Size)
{
    unsigned char *newBuf = new unsigned char[*Size];
    for(unsigned int i=0,j=0; i<*Size; i++,j++) {
	newBuf[i] = Buffer[i];
	if(Buffer[i] == IAC && Buffer[i+1] == IAC)
	    i++;
    }
    *Size = j;
    CopyMemory(Buffer,newBuf,*Size);
    delete[]newBuf;
}
