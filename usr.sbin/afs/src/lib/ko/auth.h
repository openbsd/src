/*
 * Copyright (c) 1999 Kungliga Tekniska Högskolan
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

/* $arla: auth.h,v 1.4 2003/06/10 14:46:27 lha Exp $ */

#ifndef __AUTH_H
#define __AUTH_H 1

#ifdef KERBEROS

#include <sys/types.h>
#include <netinet/in.h>
#include <atypes.h>
#include <rx/rx.h>
#ifdef HAVE_KRB4
#include <krb.h>
#endif
#include <kafs.h>
#include <rxkad.h>

struct ktc_token {
    int32_t startTime;
    int32_t endTime;
    struct ktc_encryptionKey sessionKey;
    short kvno;
    int ticketLen;
    char ticket[MAXKTCTICKETLEN];
};

int
ktc_GetToken(const struct ktc_principal *server,
	     struct ktc_token *token,
	     int token_len,
	     struct ktc_principal *client);

int
ktc_SetToken(const struct ktc_principal *server,
	     const struct ktc_token *token,
	     const struct ktc_principal *client,
	     int unknown);	/* XXX */

#endif /* KERBEROS */

#endif /* __AUTH_H */
