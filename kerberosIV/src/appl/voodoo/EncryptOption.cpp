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

/* $KTH: EncryptOption.cpp,v 1.4 1999/12/02 16:58:34 joda Exp $ */

#include <Windows.h>
#include "EncryptOption.h"
#include "CryptoEngine.h"
#include "TelnetEngine.h"

EncryptOption::EncryptOption(CharStream *stream, TelnetEngine* engine)
: Option(stream, engine, ENCRYPT_OPT)
{
    mReadyToEncrypt = FALSE;
    mIVCount = 0;
    pCryptoEngine = pTEngine->mCryptoEngine;
}
	
EncryptOption::~EncryptOption()
{

}

void
EncryptOption::NegotiateOption(unsigned char command)
{
    unsigned char c;
    
    switch (command) {
    case DO:
	if(pTEngine->mAuthenticated){
	    if (!pTEngine->mCryptoEngine){
		pCryptoEngine = pTEngine->mCryptoEngine = new CryptoEngine(&pTEngine->enckey);
	    }	
	    sendEncrypt(WILL);
	} else {
	    sendEncrypt(WONT);
	}
	stream->MarkAsRead();
	break;

    case WILL:
	if(pTEngine->mAuthenticated){
	    if (!pTEngine->mCryptoEngine){
		pCryptoEngine = pTEngine->mCryptoEngine = new CryptoEngine(&pTEngine->enckey);
	    }
	    sendEncrypt(DO);
	    sendREQSTART();
	    sendSUPPORT();
	} else {
	    sendEncrypt(DONT);
	}			
	stream->MarkAsRead();
	break;

    case SB:
	if (!stream->GetChar (&c))
	    return;
	switch (c) {
	case SUPPORT:
	    recievedSUPPORT ();
	    break;
	case REPLY:
	    recievedREPLY ();
	    break;
	case IS:
	    recievedIS();
	    break;
	case REQUEST_START:
	    break;
	case REQUEST_END:
	    recievedREQEND ();
	    break;
	case DEC_KEYID:
	    recievedDEC_KEYID ();
	    break;
	case ENC_KEYID:
	    recievedENC_KEYID ();
	    break;
	case START:
	    recievedSTART ();
	    break;
	case END:
	    recievedEND ();
	    break;
	default:
	    break;
	}
	break;

    case WONT:
	sendEncrypt (DONT);
	stream->MarkAsRead();
	break;

    case DONT:
	sendEncrypt(WONT);
	stream->MarkAsRead();
	break;
		
    default:
	break;						
    }
}

void
EncryptOption::recievedSUPPORT()
{
    unsigned char 	c;

    if (!stream->GetChar(&c))
	return;
    while (TRUE) {
	switch (c){
	case DES_CFB64:
	    if (ReadToIACSE ())
		return;
	    stream->MarkAsRead();		
	    sendIS();
	    return;
			
	case IAC:
	    if (!stream->GetChar(&c))	// Get SE
		return;
	    stream->MarkAsRead();
	    sendIS_NULL();
	    sendEncrypt(WONT);
	    return;
				
	case DES_OFB64:					// in the future
			
	default:
	    if (!stream->GetChar(&c)) /* read next encryption type and
					 hope for better luck */
		return;
	    break;
	}
    }
}

void
EncryptOption::recievedREPLY()
{
    unsigned char c;
    BOOL			mBuffFull = FALSE;
    int				count = 0;
    int i = 0;

    if (!stream->GetChar(&c))
	return;
    if (c != DES_CFB64){
	if(!ReadToIACSE ())
	    return;
	stream->MarkAsRead ();
	return;
    }
	
    // c == DES_CFB64
    if (!stream->GetChar(&c))
	return;
    switch (c){
    case CFB64_IV:
	for (i = 0; !mBuffFull; i++) {
	    if (!stream->GetChar(&c))
		return;
	    if(c == IAC)
		mEncIV[i++] = IAC;
	    mEncIV[i] = c;
	    if (++count >= 8)
		mBuffFull = TRUE;
				
	}
	if (ReadToIACSE ())
	    return;
	break;		
    case CFB64_IV_OK:
	if (ReadToIACSE ())
	    return;
	stream->MarkAsRead ();
	sendENC_KEYID ();
	break;
    default:
	sendEncrypt(WONT);
	break;
    }
}
void
EncryptOption::recievedIS()
{

    unsigned char 	c;
    BOOL			mBuffFull = FALSE;
    int				count = 0;
    if (!stream->GetChar(&c))
	return;
    while (TRUE) {
	int i = 0;
	switch (c){
	case DES_CFB64:
	    if (!stream->GetChar (&c))
		return;
	    switch (c){
	    case CFB64_IV:	
		// Get Initvec and start negotiate KEYID
		for (; !mBuffFull; i++) {
		    if (!stream->GetChar(&c))
			return;
		    if(c == IAC)
			if(!stream->GetChar(&c))
			    return;
		    pCryptoEngine->mDecInitVec[i] = c;
		    if (++count >= 8)
			mBuffFull = TRUE;
							
		}
		mIVCount = count;
		if (ReadToIACSE ())
		    return;
		stream->MarkAsRead ();
		sendREPLY(CFB64_IV_OK);
		return;	
					
	    case CFB64_IV_OK:
		sendENC_KEYID();
		if (!ReadToIACSE ())
		    return;
		stream->MarkAsRead ();
		return;
	    default:
		//	Encryptrion negotiation failed
		sendEncrypt (WONT);
		break;
	    }
	case IAC:
	    if (!stream->GetChar(&c)) 	// Get SE
		return;
	    stream->MarkAsRead();	
	    sendEncrypt(WONT);
	    return;
	    break;
	case DES_OFB64:					// in the future
			
	default:
	    if (!stream->GetChar(&c))   // read next encryption type and hope for better luck
		return;
	    break;
	}
    }
}



void
EncryptOption::recievedENC_KEYID ()
{
    unsigned char c;
    if(!stream->GetChar(&c))
	return;
    if (c == DEF_KEYID){
	sendDEC_KEYID(DEF_KEYID);
	sendREQSTART();
    }
    else 
	sendDEC_KEYID(-1);
    if(ReadToIACSE ())
	return;
    stream->MarkAsRead();				
}

void 
EncryptOption::recievedDEC_KEYID()
{
    unsigned char c;
    if (!stream->GetChar(&c))
	return;
    if (c != DEF_KEYID) {
	sendEncrypt(WONT);
	if (c == IAC)
	    if (!stream->GetChar (&c)) //Get SE
		return;
	if (ReadToIACSE ())
	    return;
    } else {
	sendSTART();
	if (ReadToIACSE ())
	    return;
    }
    stream->MarkAsRead ();
}

void 
EncryptOption::recievedREQSTART ()
{
    stream->MarkAsRead();
}

void
EncryptOption::recievedREQEND ()
{
    sendEND ();
    pTEngine->mEncrypt = FALSE;
    stream->MarkAsRead();
}

void
EncryptOption::recievedSTART ()
{
    unsigned char* data;
    unsigned int size;
	
    // If encryption is not already active, decrypt rest of stream
    // since that have not been done yet.
    if(!pTEngine->mDecrypt) {
	if(ReadToIACSE()) return;
	stream->MarkAsRead();
	while(stream->GetBuffer(&data, &size))
	    pTEngine->mCryptoEngine->Decrypt(data, data, size);
	stream->RestoreToMark();
    } else {
	if(ReadToIACSE()) return;
	stream->MarkAsRead();
    }
    pTEngine->mDecrypt = TRUE;
    pTEngine->mTelnetSession->mTerminalEngine->SetEncryptFlag(pTEngine->mDecrypt); // XXX
    sendSTART();
}

void
EncryptOption::recievedEND ()
{
    unsigned char* data;
    unsigned int size;
	
    // If encryption is active, undo decryption of rest of stream
    // since that have been decrypted.
    if(pTEngine->mDecrypt) {
	if(ReadToIACSE()) return;
	stream->MarkAsRead();
	if(stream->GetBuffer(&data, &size))
	    pTEngine->mCryptoEngine->UndoDecrypt(data, size);
	stream->RestoreToMark();
    } else {
	if(ReadToIACSE()) return;
	stream->MarkAsRead();
    }

    pTEngine->mDecrypt = FALSE;
}

void EncryptOption::sendEncrypt (int command)
{
    unsigned char mBuf[3];
    mBuf[0] = IAC;
    mBuf[1] = command;
    mBuf[2] = ENCRYPT_OPT;
    SendReply(mBuf, 3);
}

void EncryptOption::sendSUPPORT ()
{
    unsigned char mBuf[] = {IAC, SB, ENCRYPT_OPT, SUPPORT, DES_CFB64, IAC, SE};
    SendReply(mBuf, 7);
}


void EncryptOption::sendREPLY (int answer)
{
    unsigned char mBuf[8];
    mBuf[0] = IAC;
    mBuf[1] = SB;
    mBuf[2] = ENCRYPT_OPT;
    mBuf[3] = REPLY;
    mBuf[4] = DES_CFB64;
    mBuf[5] = answer;
    mBuf[6] = IAC;
    mBuf[7] = SE;
    SendReply(mBuf, 8);
}


void EncryptOption::sendIS ()
{
    int i = 0;
    unsigned char 	mBuf[24];
    unsigned char 	mRandBuf[8];
    int				mRandCount = 0;
	
    mBuf[0] = IAC;
    mBuf[1] = SB;
    mBuf[2] = ENCRYPT_OPT;
    mBuf[3] = IS;
    mBuf[4] = DES_CFB64;
    mBuf[5] = CFB64_IV;
    pCryptoEngine->RandData (mRandBuf, 8); // Generates ivec for cfb64
    for (int j = 0; j < 8; j++)
	pCryptoEngine->mEncInitVec[j] = mRandBuf[j];
    for(j = 0; mRandCount < 8; i++, j++) {
	if (mRandBuf[j] == IAC)
	    mBuf[6 + i++] = IAC;
	mBuf[i + 6] =  mRandBuf[j];	
	mRandCount++;
    }
    mBuf[6 + i++] = IAC;
    mBuf[6 + i++] = SE;
    SendReply(mBuf, 6 + i);
		
}


void EncryptOption::sendREQSTART ()
{
    unsigned char mBuf[] = {IAC, SB, ENCRYPT_OPT, REQUEST_START, IAC, SE};
    SendReply(mBuf, 6);
}


void EncryptOption::sendREQEND ()
{
    unsigned char mBuf[] = {IAC, SB, ENCRYPT_OPT, REQUEST_END, IAC, SE};
    SendReply(mBuf, 6);
}


void EncryptOption::sendDEC_KEYID (int option)
{
    unsigned char mBuf[7], *p = mBuf;
	
    *p++ = IAC;
    *p++ = SB;
    *p++ = ENCRYPT_OPT;
    *p++ = DEC_KEYID;
    if(option >= 0)
	*p++ = option;
    *p++ = IAC;
    *p++ = SE;
    SendReply(mBuf, p - mBuf);
}


void EncryptOption::sendENC_KEYID ()
{
    unsigned char mBuf[] = {IAC, SB, ENCRYPT_OPT, ENC_KEYID, DEF_KEYID, IAC, SE};
    SendReply(mBuf, 7);	
}


void EncryptOption::sendSTART()
{
    unsigned char mBuf[] = {IAC, SB, ENCRYPT_OPT, START, IAC, SE};
    SendReply(mBuf, 6);	
    pTEngine->mEncrypt = TRUE;
}


void EncryptOption::sendEND ()
{
    unsigned char mBuf[] = {IAC, SB, ENCRYPT_OPT, END, IAC, SE};
    SendReply(mBuf, 6);
    pTEngine->mEncrypt = FALSE;
}

void EncryptOption::sendIS_NULL(void)
{
    unsigned char 	mBuf[7];
    mBuf[0] = IAC;
    mBuf[1] = SB;
    mBuf[2] = ENCRYPT_OPT;
    mBuf[3] = IS;
    mBuf[4] = NULL;
    mBuf[5] = IAC;
    mBuf[6] = SE;
    SendReply(mBuf, 7);
}

void EncryptOption::ExecOption(unsigned int Command, void* Data)
{
    switch(Command) {
    case ENCRYPT_ON:
	sendREQSTART();
	sendSTART();
	break;

    case ENCRYPT_OFF:
	sendREQEND();
	sendEND();
	break;
    }
}
