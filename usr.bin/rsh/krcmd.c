/*	$OpenBSD: krcmd.c,v 1.2 2002/07/15 22:54:26 deraadt Exp $	*/
/*	$NetBSD: krcmd.c,v 1.2 1995/03/21 07:58:36 cgd Exp $	*/

/*
 * Copyright (c) 1989, 1993
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
 */

#ifndef lint
#if 0
static char sccsid[] = "@(#)krcmd.c	8.1 (Berkeley) 6/6/93";
#else
static char rcsid[] = "$OpenBSD: krcmd.c,v 1.2 2002/07/15 22:54:26 deraadt Exp $";
#endif
#endif /* not lint */

/*
 *	$Header: /mit/kerberos/ucb/mit/kcmd/RCS/krcmd.c,v 5.1
 *		89/07/25 15:38:44 kfall Exp Locker: kfall 
 */

#ifdef KERBEROS
#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>

#include <des.h>
#include <kerberosIV/krb.h>

#include <stdio.h>

#define	SERVICE_NAME	"rcmd"

int	kcmd(int *, char **, u_short, char *, char *, char *, int *,
	    KTEXT, char *, char *, CREDENTIALS *, Key_schedule, MSG_DAT *,
	    struct sockaddr_in *, struct sockaddr_in *, long);
int	krcmd(char **, u_short, char *, char *, int *, char *);
int	krcmd_mutual(char **, u_short, char *, char *, int *, char *,
	    CREDENTIALS *, Key_schedule);

/*
 * krcmd: simplified version of Athena's "kcmd"
 *	returns a socket attached to the destination, -1 or krb error on error 
 *	if fd2p is non-NULL, another socket is filled in for it
 */
int
krcmd(char **ahost, u_short rport, char *remuser, char *cmd, int *fd2p,
    char *realm)
{
	int sock = -1, err = 0;
	char myrealm[REALM_SZ];
	long authopts = 0L;
	KTEXT_ST ticket;

	err = kcmd(&sock, ahost, rport, NULL, remuser, cmd, fd2p,
	    &ticket, SERVICE_NAME, realm, (CREDENTIALS *) NULL,
	    (void *) NULL, (MSG_DAT *) NULL, (struct sockaddr_in *) NULL,
	    (struct sockaddr_in *) NULL, authopts);

	if (err > KSUCCESS && err < MAX_KRB_ERRORS) {
		if (krb_get_lrealm(myrealm, 0) == KSUCCESS)
			fprintf(stderr, "krcmd: %s\n", krb_err_txt[err]);
		return(-1);
	}
	if (err < 0)
		return(-1);
	return(sock);
}

int
krcmd_mutual(char **ahost, u_short rport, char *remuser, char *cmd, int *fd2p,
    char *realm, CREDENTIALS *cred, Key_schedule sched)
{
	struct sockaddr_in laddr, faddr;
	long authopts = KOPT_DO_MUTUAL;
	char myrealm[REALM_SZ];
	KTEXT_ST ticket;
	MSG_DAT msg_dat;
	int sock, err;

	err = kcmd(&sock, ahost, rport, NULL, remuser, cmd, fd2p,
	    &ticket, SERVICE_NAME, realm, cred, sched, &msg_dat,
	    &laddr, &faddr, authopts);

	if (err > KSUCCESS && err < MAX_KRB_ERRORS) {
		if (krb_get_lrealm(myrealm, 0) == KSUCCESS)
			fprintf(stderr, "krcmd_mutual: %s\n", krb_err_txt[err]);
		return(-1);
	}

	if (err < 0)
		return (-1);
	return(sock);
}
#endif /* KERBEROS */
