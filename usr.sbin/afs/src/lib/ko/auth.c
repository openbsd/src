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

#include "ko_locl.h"
#include "auth.h"

RCSID("$arla: auth.c,v 1.7 2003/06/10 16:41:02 lha Exp $");

#ifdef KERBEROS

#define AFS_PRINCIPAL "afs"
#define AFS_INSTANCE  ""

/*
 * Try to fetch the token for `server' into `token'.
 * We should also fill in `client', but we don't.
 */

int
ktc_GetToken(const struct ktc_principal *server,
	     struct ktc_token *token,
	     int token_len,
	     struct ktc_principal *client)
{
    uint32_t i;
    unsigned char t[128];
    struct ViceIoctl parms;

    assert (sizeof(*token) == token_len);

    parms.in = (void *)&i;
    parms.in_size = sizeof(i);
    parms.out = (void *)t;
    parms.out_size = sizeof(t);

    for (i = 0; k_pioctl(NULL, VIOCGETTOK, &parms, 0) == 0; i++) {
	int32_t size_secret_tok, size_clear_tok;
	const unsigned char *r = t;
	const unsigned char *secret_token;
	struct ClearToken ct;
	const char *cell;

	memcpy (&size_secret_tok, r, sizeof(size_secret_tok));
	r += sizeof(size_secret_tok);
	secret_token = r;
	r += size_secret_tok;
	memcpy (&size_clear_tok, r, sizeof(size_clear_tok));
	r += sizeof(size_clear_tok);
	memcpy (&ct, r, size_clear_tok);
	r += size_clear_tok;
	/* there is a int32_t with length of cellname, but we dont read it */
	r += sizeof(int32_t);
	cell = (const char *)r;

	if (strcmp (cell, server->cell) == 0
	    && strcmp (server->name, AFS_PRINCIPAL) == 0
	    && strcmp (server->instance, AFS_INSTANCE) == 0) {
	    token->startTime = ct.BeginTimestamp;
	    token->endTime   = ct.EndTimestamp;
	    memcpy (token->sessionKey.data, ct.HandShakeKey, 8);
	    token->kvno = ct.AuthHandle;
	    token->ticketLen = size_secret_tok;
	    memcpy (token->ticket, secret_token, size_secret_tok);
	    memset (&ct, 0, sizeof(ct));
	    return 0;
	}
	memset (&ct, 0, sizeof(ct));
    }
    return -1;
}

/*
 * store the token in `token' for `server' into the kernel
 */

int
ktc_SetToken(const struct ktc_principal *server,
	     const struct ktc_token *token,
	     const struct ktc_principal *client,
	     int unknown)	/* XXX */
{
#ifdef HAVE_KRB4
    const char *cell;
    CREDENTIALS cred;
    int ret;
    char *p;
    uid_t uid = 0;

    if (strcmp(server->name, AFS_PRINCIPAL) != 0
	|| strcmp(server->instance, AFS_INSTANCE) != 0)
	return -1;
    cell = server->cell;
    strlcpy(cred.service, server->name, sizeof(cred.service));
    strlcpy(cred.instance, server->instance, sizeof(cred.instance));
    strlcpy(cred.realm, server->cell, sizeof(cred.realm));
    memcpy (cred.session, token->sessionKey.data, 8);
    cred.lifetime = krb_time_to_life (token->startTime, token->endTime);
    cred.kvno     = token->kvno;
    cred.ticket_st.length = token->ticketLen;
    memcpy (cred.ticket_st.dat, token->ticket, token->ticketLen);
    cred.issue_date = token->startTime;
    strlcpy(cred.pname, client->name, sizeof(cred.pname));
    strlcpy(cred.pinst, client->instance, sizeof(cred.pinst));
    p = strstr (client->name, "0123456789");
    if (p != NULL) {
	char *end;

	uid = strtol (p, &end, 0);
    }
    ret = kafs_settoken (cell, uid, &cred);
    memset (&cred, 0, sizeof(cred));
    return ret;
#elif defined(HAVE_KAFS_SETTOKEN_RXKAD)
    struct ClearToken ct;
    int ret;

    ct.AuthHandle = token.kvno;
    memcpy(&ct.HandShakeKey, &token.sessionKey, sizeof(ct.HandShakeKey));
    ct.BeginTimestamp = token.startTime;
    ct.EndTimestamp = token.endTime;
    
    ret = kafs_settoken_rxkad (server->cell, &ct,
			       token.ticket, token.ticketLen);
    memset(&ct, 0, sizeof(ct));
    return ret;
#endif
}

#endif /* KERBEROS */
