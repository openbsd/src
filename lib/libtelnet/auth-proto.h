/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)auth-proto.h	8.1 (Berkeley) 6/4/93
 *	$Id: auth-proto.h,v 1.1.1.1 1995/10/18 08:43:11 deraadt Exp $
 */

/*
 * Copyright (C) 1990 by the Massachusetts Institute of Technology
 *
 * Export of this software from the United States of America is assumed
 * to require a specific license from the United States Government.
 * It is the responsibility of any person or organization contemplating
 * export to obtain such a license before exporting.
 * 
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

#include <sys/cdefs.h>

#if	defined(AUTHENTICATION)
Authenticator *findauthenticator __P((int, int));

void auth_init __P((char *, int));
int auth_cmd __P((int, char **));
void auth_request __P((void));
void auth_send __P((unsigned char *, int));
void auth_send_retry __P((void));
void auth_is __P((unsigned char *, int));
void auth_reply __P((unsigned char *, int));
void auth_finished __P((Authenticator *, int));
int auth_wait __P((char *));
void auth_disable_name __P((char *));
void auth_gen_printsub __P((unsigned char *, int, unsigned char *, int));

int getauthmask __P((char *, int *));
int auth_enable __P((char *));
int auth_disable __P((char *));
int auth_onoff __P((char *, int));
int auth_togdebug __P((int));
int auth_status __P((void));
void auth_name __P((unsigned char *, int));
int auth_sendname __P((unsigned char *, int));
void auth_finished __P((Authenticator *, int));
int auth_wait __P((char *));
void auth_debug __P((int));
void auth_printsub __P((unsigned char *, int, unsigned char *, int));

#ifdef	KRB4
int kerberos4_init __P((Authenticator *, int));
int kerberos4_send __P((Authenticator *));
void kerberos4_is __P((Authenticator *, unsigned char *, int));
void kerberos4_reply __P((Authenticator *, unsigned char *, int));
int kerberos4_status __P((Authenticator *, char *, int));
void kerberos4_printsub __P((unsigned char *, int, unsigned char *, int));
#endif

#ifdef	KRB5
int kerberos5_init __P((Authenticator *, int));
int kerberos5_send __P((Authenticator *));
void kerberos5_is __P((Authenticator *, unsigned char *, int));
void kerberos5_reply __P((Authenticator *, unsigned char *, int));
int kerberos5_status __P((Authenticator *, char *, int));
void kerberos5_printsub __P((unsigned char *, int, unsigned char *, int));
#endif
#endif
