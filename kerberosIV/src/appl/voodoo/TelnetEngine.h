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

// Author: Jörgen Karlsson - d93-jka@nada.kth.se

#ifndef __TELNETENGINE_H__
#define __TELNETENGINE_H__

#include <winsock.h>
#include "CharStream.h"
#include "Negotiator.h"
#include "CryptoEngine.h"
#include <kclient.h>

class TelnetSession;

enum	{ SM_READ = WM_USER,	// Socket Message Read,
	  // data available for reading.
};

class TelnetEngine
{
public:
    void DeleteDoubleIAC(unsigned char* Buffer, unsigned int* Size);
    void ReadSocket(void);
    void InvokeOption(unsigned char Option, unsigned int Command, void* Data = NULL);
    void SendCommand(const int command);
    HANDLE mRecEvent;
    HANDLE mRecThread;

    BOOL mDecrypt;
    BOOL mEncrypt;
    BOOL mAuthenticated;
    unsigned char enckey[8];
    TelnetEngine(TelnetSession *thisTelnetSession);
    TelnetSession* mTelnetSession;
    void Close(void);
    void Receive(char* buffer, unsigned int buf_size);
    static unsigned long WINAPI TelnetEngine::RecvThread(
							 void *thisTelnetEngine);
    Negotiator *mNegotiator;
    void Send(char *buf, int bufSZ);
    void TelnetNegotiate(CharStream *RecStream);
	
	 
    CharStream		mRawStream,	// Used	by receive thread to store incoming data.
	mRecStream;	// Sent to TelnetSession.
    BOOL Connect(char *hostname, char *username);
    SOCKET mSocket;
    KClientSessionInfo mKClientSession;

    CryptoEngine* mCryptoEngine;

private:
};

#endif /* __TELNETENGINE_H__ */
