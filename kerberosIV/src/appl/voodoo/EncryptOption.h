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

#ifndef __ENCRYPTOPTION_H__
#define __ENCRYPTOPTION_H__

#include "CharStream.h"
#include "Option.h"
#include "TelnetEngine.h"
#include "EmulatorEngine.h"


//#include "Negotiator.h"
//#include "KClient.h"

class CryptoEngine;

enum
{
    ENCRYPT_OPT		=		38,
    // ExecOption Commands
    ENCRYPT_ON		=		1,
    ENCRYPT_OFF		=		2
};

class EncryptOption : public Option{
public:
    EncryptOption(CharStream *stream, TelnetEngine *);
    ~EncryptOption();
	
    void NegotiateOption(unsigned char);
	
    BOOL InEffect() { return pTEngine->mDecrypt && pTEngine->mEncrypt; }
	
    void ExecOption(unsigned int Command, void* Data = NULL);

private:
    void sendIS_NULL(void);
    void recievedSUPPORT ();
    void recievedREPLY ();
    void recievedIS();
    void recievedREQSTART ();
    void recievedREQEND ();
    void recievedDEC_KEYID ();
    void recievedENC_KEYID ();
    void recievedSTART();
    void recievedEND();
    void sendSUPPORT ();
    void sendREPLY (int answer);
    void sendIS();
    void sendREQSTART ();
    void sendREQEND ();
    void sendDEC_KEYID (int answer);
    void sendENC_KEYID ();
    void sendSTART();
    void sendEND();
    void sendEncrypt(int option); // DO, DONT, WILL, WONT
    BOOL			mReadyToEncrypt;
    int 			mDecKeyId;
    CryptoEngine*   pCryptoEngine;
    unsigned char	mDecIV[8];
    unsigned char	mEncIV[8];
    int				mIVCount;
    // Encryption options
    enum{
	REQUEST_START = 5,
	REQUEST_END		=		6,
	ENC_KEYID		=		7,
	DEC_KEYID		=		8,
	DEF_KEYID		=		0,	// Default
		
	DES_CFB64		=		1,
	DES_OFB64		=		2,
	
	// CFB64 Suboption Commands
	CFB64_IV		= 		1,
	CFB64_IV_OK		=		2,
	CFB64_IV_BAD	=		3,
	CFB64_CHALLENGE	=		4,
	CFB64_RESPONSE	=		5
    };
};

#endif /* __ENCRYPTOPTION_H__ */
