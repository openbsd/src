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

/* $KTH: AuthOption.cpp,v 1.5 1999/12/02 16:58:34 joda Exp $ */

//
//
// AuthOption
//
//

extern "C" {
#include <time.h>
#include <stdlib.h>
}

#include <Windows.h>
#include <winsock.h>
#include "TelnetApp.h"
#include "Negotiator.h"
#include "AuthOption.h"
#include "des.h"
#include "KClient.h"
#include "krb.h"
#include "string.h"
#include "TelnetEngine.h"

AuthOption::AuthOption (CharStream *stream, 
			TelnetEngine *engine, 
			const char *hostname,
			const char *username)
: Option (stream, engine, AUTHENTICATION)
{
    memset (host, 0, sizeof (host));
    strncpy (host, (char*)hostname, strlen(hostname));
    memset (user, 0, sizeof (user));
    strncpy (user, (char*)username, strlen(username));
}

AuthOption::~AuthOption ()
{
    /*if (KClientStatus () == KClientLoggedIn)
      KClientLogout ();
      */
}

BOOL
AuthOption::KerbInit ()
{
    int res;
    res = KClientInitSession(&pTEngine->mKClientSession, 0, 0, 0, 0);
    return res;
}

void
AuthOption::NegotiateOption (unsigned char command)
{
    unsigned char c;
	
    switch (command) {
    case DO:
	unsigned char buff[3];
	buff[0] = IAC;
	if (KerbInit ())
		{
	    buff[1] = WONT;
//flag
//			unsigned char msg[] = "[ WARNING WONT use KERBEROS4 authentication or encrypt. WARNING ].\n";
//			TelenetSession::mTerminalEngine->ReceiveBuffer(msg, sizeof(msg));			
//flag
		}
	else
		{
	    buff[1] = WILL;
//flag
//			unsigned char msg[] = "[ Trying mutual KERBEROS4 ... ]\n";
//			mTerminalEngine->ReceiveBuffer(msg, sizeof(msg));
//flag
		}
	buff[2] = option;
	SendReply (buff, 3);
	stream->MarkAsRead ();
	break;

    case SB:
	if (!stream->GetChar (&c))
	    return;
	switch (c) {
	case SEND:
	    NegotiateSend ();
	    break;
	case REPLY:
	    NegotiateReply ();
	    break;
	default:
	    break;
	}		
	break;
    default:
	break;
    }
}


void
AuthOption::SendNoKerb4 ()
{
    static unsigned char buff[8];

    buff[0] = IAC;
    buff[1] = SB;
    buff[2] = AUTHENTICATION;
    buff[3] = IS;
    buff[4] = NULL;
    buff[5] = 0;
    buff[6] = IAC;
    buff[7] = SE;
    SendReply (buff, 8);
}

void
AuthOption::NegotiateSend ()
{
    unsigned char        pair[2], c = 0, padKBuff[2048];
    char                 kBuff[2048], *pKey, principal[255], hostname[255];
    unsigned long        kBuffLen = sizeof (kBuff);
    Kerr                result;
		
    pair[0] = SE; // dummy init
	
    while (pair[0] != KERBEROS_V4 && pair[0] != IAC) {
	if (!stream->GetChar (pair))
	    return;
	if (!stream->GetChar (pair + 1))
	    return;
    }
	
    while (pair[0] == KERBEROS_V4) {
	if (pair[1] == AUTH_HOW_MUTUAL)
	    break;
	if (!stream->GetChar (pair)) 
	    return;
	if (!stream->GetChar (pair + 1))
	    return;
    }
	
    if (pair[0] == IAC) {
	if (!stream->GetChar (&c))  // Get SE
	    return;
    }
    else
	if (ReadToIACSE ())
	    return;

    stream->MarkAsRead ();
		
    if (pair[0] != KERBEROS_V4) {  // Cannot do kerb4, one-way -> Send NULL authpair
	SendNoKerb4 ();
	return;
    }
	

    // Get Kerberos ticket
    principal[0] = '\0';
    strcat (principal, "rcmd.");
    strcpy(hostname,host);
    char *p = strchr(hostname, '.');
    if(p)
	*p = 0;
    strcat (principal, hostname);
    strcat(principal,"@");
    strcat (principal, krb_realmofhost(host));
    result = KClientGetTicketForService (&pTEngine->mKClientSession,
					 principal, kBuff, &kBuffLen);
    if (result) {
	char error[255];
	KClientErrorText(result, error); 
	TelnetApp::Error(result, error);
	SendNoKerb4 ();
	return;
    }
	
    // username

    if(*user == '\0') {
	result = KClientGetUserName(user);
	if(result) {
	    char error[255];
	    KClientErrorText(result, error);
	    TelnetApp::Error(result, error);
	    SendNoKerb4 ();
	    return;
	}
    }

    // Send name
    unsigned char nameBuff[4];
    unsigned char iacseBuff[2];
    nameBuff[0] = IAC;
    nameBuff[1] = SB;
    nameBuff[2] = AUTHENTICATION;
    nameBuff[3] = NAME;
    SendReply (nameBuff, 4);
    SendReply (user, strlen (user));
    iacseBuff[0] = IAC;
    iacseBuff[1] = SE;
    SendReply (iacseBuff, 2);
		
    // Check if kBufflen in beginning of kBuff
    if (*((unsigned long *)kBuff) == kBuffLen - sizeof (unsigned long)) {
	pKey = kBuff + sizeof (unsigned long);
	kBuffLen -= sizeof (unsigned long);
    }
    else
	pKey = kBuff;
	
    // Send ticket
    unsigned char tktBuff[7];
    tktBuff[0] = IAC;
    tktBuff[1] = SB;
    tktBuff[2] = AUTHENTICATION;
    tktBuff[3] = IS;
    tktBuff[4] = KERBEROS_V4;
    tktBuff[5] = AUTH_CLIENT_TO_SERVER | AUTH_HOW_MUTUAL;
    tktBuff[6] = AUTH;
    SendReply (tktBuff, 7);
	
    unsigned long padKBuffLen;
    PadBinaryBuff ((unsigned char*)pKey, &kBuffLen, (unsigned char*)padKBuff, &padKBuffLen); 
    SendReply(padKBuff, padKBuffLen);
    SendReply(iacseBuff, 2);
}

void
AuthOption::SendChallenge()
{
    int i;
    unsigned char reply1[] = { IAC, SB, AUTHENTICATION, IS, KERBEROS_V4, AUTH_CLIENT_TO_SERVER | AUTH_HOW_MUTUAL, CHALLENGE };
    unsigned char reply2[] = { IAC, SE };
    unsigned char msg[16];
    unsigned long len, padlen;
    des_cblock session;
    des_key_schedule schedule;

    srand(time(0));
    for(i = 0; i < 8; i++)
	pTEngine->enckey[i] = rand() % 256; // XXX
    des_set_odd_parity((des_cblock *)pTEngine->enckey);
    KClientGetSessionKey(&pTEngine->mKClientSession, (KClientKey*)&session);
    des_set_key(&session, schedule);
    des_ecb_encrypt(&pTEngine->enckey, &challenge, schedule, DES_DECRYPT);

    len = 8;
    PadBinaryBuff(challenge, &len, msg, &padlen);
	
    SendReply(reply1, sizeof(reply1));
    SendReply(msg, padlen);
    SendReply(reply2, sizeof(reply2));

    des_ecb_encrypt(&challenge, &challenge, schedule, DES_DECRYPT);

    for(i = 7; i >= 0; i--)
	if(++(challenge[i]))
	    break;
    des_ecb_encrypt(&challenge, &challenge, schedule, DES_ENCRYPT);

    memset(&session, 0, sizeof(des_cblock));
    memset(&msg, 0, sizeof(msg));
    memset(&schedule, 0, sizeof(des_key_schedule));
}


void
AuthOption::NegotiateReply ()
{
    unsigned char pair[2];
    unsigned char reply;
    unsigned char reason[80];
    unsigned char c = 0;
    short i = 0;
	
    if (!stream->GetChar (pair) || !stream->GetChar (pair + 1)) // Get auth-pair
	return;
	
    if (!stream->GetChar (&reply))
	return;
	
    switch (reply) {
    case REJECT:
			
	while (c != IAC) {
	    if (!stream->GetChar (&c))
		return;
	    reason[i++] = c;
	}
	reason[i]  = '\0';
	TelnetApp::Error(GetLastError(),(char*) reason);
	stream->UngetChar ();
	break;	
    case ACCEPT:
	if(pair[1] & AUTH_HOW_MUTUAL)
		{
//flag	
//			unsigned char msg[] = "[ Kerberos V4 accepts you ]\n";
//			mTerminalEngine->ReceiveBuffer(msg, sizeof(msg));
//flag
	    SendChallenge();
		}
	break;
    case RESPONSE:
	des_cblock session;
	des_key_schedule schedule;
	des_cblock response;
	// Get encrypted response
	for(i = 0; i < 8; i++)
	    if(!stream->GetChar(response + i))
		return;
	for(i = 0; i < 8; i++)
	    if(response[i] != challenge[i])
		break; //return;
//flag	
//			unsigned char msg[] = "[ Kerberos V4 challenge successful ]\n";
//			mTerminalEngine->ReceiveBuffer(msg, sizeof(msg));
//flag
	pTEngine->mAuthenticated = TRUE;
	NegotiateEncryption();
	break;
    default:
	break;
    }
	
    if (ReadToIACSE ())
	return;
    else
	stream->MarkAsRead ();
}

void
AuthOption::NegotiateEncryption()
{
    unsigned char mBuf[] = {IAC, DO, ENCRYPT_OPT};
    SendReply(mBuf, 3);
    mBuf[1] = WILL;
    SendReply(mBuf, 3);
}

void AuthOption::ExecOption(unsigned int Command)
{

}
