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
// AuthOption
//
// Authenticates user with Kerberos 4
//

#ifndef __AUTHOPTION_H__
#define __AUTHOPTION_H__

#include <winsock.h>
#include "Negotiator.h"
extern "C" {
#include "KClient.h"
}
//#include "des.h"

class AuthOption : public Option {
public:
	AuthOption (CharStream *stream, TelnetEngine *engine, const char *, const char *);
	~AuthOption ();
	void NegotiateOption (unsigned char);
	BOOL InEffect() { return FALSE; }
	void ExecOption(unsigned int Command);
private:
	char               	host[255];
	char			user[64];
//	des_cblock			challenge;
	unsigned char challenge[8];
	void NegotiateSend ();
	void NegotiateReply ();
	void NegotiateEncryption();
	void SendChallenge();
	void SendNoKerb4 ();
	BOOL KerbInit ();
	// Authentication option, commands, modifiers etc...
	enum{
		AUTHENTICATION 	= 37,
		ENCRYPT_OPT		= 38,
	
	//       Authentication types
	    KERBEROS_V4   =   1,
	    KERBEROS_V5   =   2,
	    SPX           =   3,
	    RSA           =   6,
	    LOKI          =  10,
	
	//       Modifiers
	    AUTH_WHO_MASK  			=   1,
	    AUTH_CLIENT_TO_SERVER 	=   0,
	    AUTH_SERVER_TO_CLIENT  	=  	1,
	    AUTH_HOW_MASK        	=	2,
	    AUTH_HOW_ONE_WAY     	=   0,
	    AUTH_HOW_MUTUAL      	=   2,
	
	// Kerberos suboption commands
	    AUTH			=	0,
		REJECT 			=	1,
	 	ACCEPT			=	2,
		CHALLENGE		=	3,
	 	RESPONSE		=	4
	 };
};

#endif /* __AUTHOPTION_H__ */
