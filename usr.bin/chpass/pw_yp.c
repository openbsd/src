/*	$OpenBSD: pw_yp.c,v 1.8 1998/03/30 06:59:32 deraadt Exp $	*/
/*	$NetBSD: pw_yp.c,v 1.5 1995/03/26 04:55:33 glass Exp $	*/

/*
 * Copyright (c) 1988 The Regents of the University of California.
 * All rights reserved.
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
static char sccsid[] = "@(#)pw_yp.c	1.0 2/2/93";
#else
static char rcsid[] = "$OpenBSD: pw_yp.c,v 1.8 1998/03/30 06:59:32 deraadt Exp $";
#endif
#endif /* not lint */

#ifdef	YP

#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <time.h>
#include <pwd.h>
#include <err.h>
#include <errno.h>
#include <stdlib.h>
#include <rpc/rpc.h>
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>
#define passwd yp_passwd_rec
#include <rpcsvc/yppasswd.h>
#undef passwd

extern char *progname;

static char *domain;

int
pw_yp(pw, uid)
	struct passwd *pw;
	uid_t uid;
{
	char *master;
	char *p;
	char buf[10];
	int r, rpcport, status, alen;
	struct yppasswd yppasswd;
	struct timeval tv;
	CLIENT *client;
	extern char *getpass();
	
	/*
	 * Get local domain
	 */
	if (!domain && (r = yp_get_default_domain(&domain))) {
		fprintf(stderr, "%s: can't get local YP domain. Reason: %s\n",
		    progname, yperr_string(r));
		return(0);
	}

	/*
	 * Find the host for the passwd map; it should be running
	 * the daemon.
	 */
	if ((r = yp_master(domain, "passwd.byname", &master)) != 0) {
		fprintf(stderr,
		    "%s: can't find the master YP server. Reason: %s\n",
		    progname, yperr_string(r));
		return(0);
	}

	/*
	 * Ask the portmapper for the port of the daemon.
	 */
	if ((rpcport = getrpcport(master, YPPASSWDPROG, YPPASSWDPROC_UPDATE,
	    IPPROTO_UDP)) == 0) {
		fprintf(stderr,
		    "%s: master YP server not running yppasswd daemon.\n",
		    progname);
		fprintf(stderr,	"\tCan't change password.\n");
		return(0);
	}

	/*
	 * Be sure the port is priviledged
	 */
	if (rpcport >= IPPORT_RESERVED) {
		(void)fprintf(stderr,
		    "%s: yppasswd daemon running on an invalid port.\n",
		    progname);
		return(0);
	}

	/* prompt for old password */
	bzero(&yppasswd, sizeof yppasswd);
	yppasswd.oldpass = "none";
	yppasswd.oldpass = getpass("Old password:");
	if (!yppasswd.oldpass) {
		(void)fprintf(stderr, "Cancelled.\n");
		return(0);
	}
	
	for (alen = 0, p = pw->pw_gecos; *p; p++)
		if (*p == '&')
			alen = alen + strlen(pw->pw_name) - 1;
	if (strlen(pw->pw_name) + 1 + strlen(pw->pw_passwd) + 1 +
	    strlen((sprintf(buf, "%d", pw->pw_uid), buf)) + 1 +
	    strlen((sprintf(buf, "%d", pw->pw_gid), buf)) + 1 +
	    strlen(pw->pw_gecos) + alen + 1 + strlen(pw->pw_dir) + 1 +
	    strlen(pw->pw_shell) >= 1023) {
		warnx("entries too long");
		return (0);
	}

	/* tell rpc.yppasswdd */
	yppasswd.newpw.pw_name	= pw->pw_name;
	yppasswd.newpw.pw_passwd= pw->pw_passwd;
	yppasswd.newpw.pw_uid 	= pw->pw_uid;
	yppasswd.newpw.pw_gid	= pw->pw_gid;
	yppasswd.newpw.pw_gecos = pw->pw_gecos;
	yppasswd.newpw.pw_dir	= pw->pw_dir;
	yppasswd.newpw.pw_shell	= pw->pw_shell;

	client = clnt_create(master, YPPASSWDPROG, YPPASSWDVERS, "udp");
	if (client==NULL) {
		fprintf(stderr, "can't contact yppasswdd on %s: Reason: %s\n",
		    master, yperr_string(YPERR_YPBIND));
		return(1);
	}
	client->cl_auth = authunix_create_default();
	tv.tv_sec = 5;
	tv.tv_usec = 0;
	r = clnt_call(client, YPPASSWDPROC_UPDATE,
	    xdr_yppasswd, &yppasswd, xdr_int, &status, tv);
	if (r) {
		fprintf(stderr, "%s: rpc to yppasswdd failed. %d\n", progname, r);
		clnt_destroy(client);
		return(1);
	} else if (status) {
		printf("Couldn't change YP password information.\n");
		clnt_destroy(client);
		return(1);
	}
	printf("The YP password information has been changed on %s, the master YP passwd server.\n", master);

	clnt_destroy(client);
	return(0);
}

static char *
pwskip(p)
	register char *p;
{
	while (*p && *p != ':' && *p != '\n')
		++p;
	if (*p)
		*p++ = 0;
	return (p);
}

static struct passwd *
interpret(pwent, line)
	struct passwd *pwent;
	char *line;
{
	register char	*p = line;

	pwent->pw_passwd = "*";
	pwent->pw_uid = 0;
	pwent->pw_gid = 0;
	pwent->pw_gecos = "";
	pwent->pw_dir = "";
	pwent->pw_shell = "";
	pwent->pw_change = 0;
	pwent->pw_expire = 0;
	pwent->pw_class = "";
	
	/* line without colon separators is no good, so ignore it */
	if(!strchr(p,':'))
		return(NULL);

	pwent->pw_name = p;
	p = pwskip(p);
	pwent->pw_passwd = p;
	p = pwskip(p);
	pwent->pw_uid = (uid_t)strtoul(p, NULL, 10);
	p = pwskip(p);
	pwent->pw_gid = (gid_t)strtoul(p, NULL, 10);
	p = pwskip(p);
	pwent->pw_gecos = p;
	p = pwskip(p);
	pwent->pw_dir = p;
	p = pwskip(p);
	pwent->pw_shell = p;
	while (*p && *p != '\n')
		p++;
	*p = '\0';
	return (pwent);
}

static char *__yplin;

struct passwd *
ypgetpwnam(nam)
	char *nam;
{
	static struct passwd pwent;
	char *val;
	int reason, vallen;
	
	/*
	 * Get local domain
	 */
	if (!domain && (reason = yp_get_default_domain(&domain))) {
		fprintf(stderr, "%s: can't get local YP domain. Reason: %s\n",
		    progname, yperr_string(reason));
		exit(1);
	}

	reason = yp_match(domain, "passwd.byname", nam, strlen(nam),
	    &val, &vallen);
	switch(reason) {
	case 0:
		break;
	default:
		return (NULL);
		break;
	}
	val[vallen] = '\0';
	if (__yplin)
		free(__yplin);
	__yplin = (char *)malloc(vallen + 1);
	strcpy(__yplin, val);
	free(val);

	return(interpret(&pwent, __yplin));
}

struct passwd *
ypgetpwuid(uid)
	uid_t uid;
{
	static struct passwd pwent;
	char *val;
	int reason, vallen;
	char namebuf[16];
	
	if (!domain && (reason = yp_get_default_domain(&domain))) {
		fprintf(stderr, "%s: can't get local YP domain. Reason: %s\n",
		    progname, yperr_string(reason));
		exit(1);
	}

	sprintf(namebuf, "%d", uid);
	reason = yp_match(domain, "passwd.byuid", namebuf, strlen(namebuf),
	    &val, &vallen);
	switch(reason) {
	case 0:
		break;
	default:
		return (NULL);
		break;
	}
	val[vallen] = '\0';
	if (__yplin)
		free(__yplin);
	__yplin = (char *)malloc(vallen + 1);
	strcpy(__yplin, val);
	free(val);

	return(interpret(&pwent, __yplin));
}

#endif	/* YP */
