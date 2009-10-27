/*	$OpenBSD: pw_yp.c,v 1.23 2009/10/27 23:59:36 deraadt Exp $	*/
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
 * 3. Neither the name of the University nor the names of its contributors
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

#ifdef	YP

#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <time.h>
#include <pwd.h>
#include <err.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <rpc/rpc.h>
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>
#define passwd yp_passwd_rec
#include <rpcsvc/yppasswd.h>
#undef passwd
#include "chpass.h"

extern char *__progname;

static char *domain;

int
pw_yp(struct passwd *pw, uid_t uid)
{
	char uidbuf[20], gidbuf[20], *master, *p;
	int r, rpcport, status, alen;
	struct yppasswd yppasswd;
	struct timeval tv;
	CLIENT *client;

	/*
	 * Get local domain
	 */
	if (!domain && (r = yp_get_default_domain(&domain))) {
		fprintf(stderr, "%s: can't get local YP domain. Reason: %s\n",
		    __progname, yperr_string(r));
		return(0);
	}

	/*
	 * Find the host for the passwd map; it should be running
	 * the daemon.
	 */
	if ((r = yp_master(domain, "passwd.byname", &master)) != 0) {
		fprintf(stderr,
		    "%s: can't find the master YP server. Reason: %s\n",
		    __progname, yperr_string(r));
		return(0);
	}

	/*
	 * Ask the portmapper for the port of the daemon.
	 */
	if ((rpcport = getrpcport(master, YPPASSWDPROG, YPPASSWDPROC_UPDATE,
	    IPPROTO_UDP)) == 0) {
		fprintf(stderr,
		    "%s: master YP server not running yppasswd daemon.\n",
		    __progname);
		fprintf(stderr,	"\tCan't change password.\n");
		return(0);
	}

	/*
	 * Be sure the port is privileged
	 */
	if (rpcport >= IPPORT_RESERVED) {
		(void)fprintf(stderr,
		    "%s: yppasswd daemon running on an invalid port.\n",
		    __progname);
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
	(void)snprintf(uidbuf, sizeof uidbuf, "%u", pw->pw_uid);
	(void)snprintf(gidbuf, sizeof gidbuf, "%u", pw->pw_gid);

	if (strlen(pw->pw_name) + 1 + strlen(pw->pw_passwd) + 1 +
	    strlen(uidbuf) + 1 + strlen(gidbuf) + 1 +
	    strlen(pw->pw_gecos) + alen + 1 + strlen(pw->pw_dir) + 1 +
	    strlen(pw->pw_shell) >= 1023) {
		warnx("entries too long");
		return (0);
	}

	/* tell rpc.yppasswdd */
	yppasswd.newpw.pw_name	= pw->pw_name;
	yppasswd.newpw.pw_passwd= pw->pw_passwd;
	yppasswd.newpw.pw_uid	= pw->pw_uid;
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
		fprintf(stderr, "%s: rpc to yppasswdd failed. %d\n",
		    __progname, r);
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
pwskip(char *p)
{
	while (*p && *p != ':' && *p != '\n')
		++p;
	if (*p)
		*p++ = 0;
	return (p);
}

static struct passwd *
interpret(struct passwd *pwent, char *line, const int secure)
{
	char	*p = line;

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
	if (!strchr(p,':'))
		return(NULL);

	pwent->pw_name = p;
	p = pwskip(p);
	pwent->pw_passwd = p;
	p = pwskip(p);
	pwent->pw_uid = (uid_t)strtoul(p, NULL, 10);
	p = pwskip(p);
	pwent->pw_gid = (gid_t)strtoul(p, NULL, 10);
	p = pwskip(p);
	if (secure == 1) {
		pwent->pw_class = p;
		p = pwskip(p);
		pwent->pw_change = (time_t)strtoul(p, NULL, 10);
		p = pwskip(p);
		pwent->pw_expire = (time_t)strtoul(p, NULL, 10);
		p = pwskip(p);
	}
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
ypgetpwnam(char *nam)
{
	static struct passwd pwent;
	int reason, vallen, secure = 0;
	char *val;

	/*
	 * Get local domain
	 */
	if (!domain && (reason = yp_get_default_domain(&domain))) {
		fprintf(stderr, "%s: can't get local YP domain. Reason: %s\n",
		    __progname, yperr_string(reason));
		exit(1);
	}

	if (!yp_match(domain, "master.passwd.byname", nam, strlen(nam),
	    &val, &vallen))
		secure = 1;
	else if (yp_match(domain, "passwd.byname", nam, strlen(nam),
	    &val, &vallen))
		return (NULL);

	val[vallen] = '\0';
	if (__yplin)
		free(__yplin);
	if (!(__yplin = malloc(vallen + 1)))
		err(1, NULL);
	strlcpy(__yplin, val, vallen + 1);
	free(val);

	return(interpret(&pwent, __yplin, secure));
}

struct passwd *
ypgetpwuid(uid_t uid)
{
	static struct passwd pwent;
	int reason, vallen, secure = 0;
	char namebuf[16], *val;

	if (!domain && (reason = yp_get_default_domain(&domain))) {
		fprintf(stderr, "%s: can't get local YP domain. Reason: %s\n",
		    __progname, yperr_string(reason));
		exit(1);
	}

	snprintf(namebuf, sizeof namebuf, "%u", (u_int)uid);
	if (!yp_match(domain, "master.passwd.byuid", namebuf, strlen(namebuf),
	    &val, &vallen))
		secure = 1;
	else if (yp_match(domain, "passwd.byuid", namebuf, strlen(namebuf),
	    &val, &vallen))
		return (NULL);

	val[vallen] = '\0';
	if (__yplin)
		free(__yplin);
	if (!(__yplin = malloc(vallen + 1)))
		err(1, NULL);
	strlcpy(__yplin, val, vallen + 1);
	free(val);

	return(interpret(&pwent, __yplin, secure));
}

#endif	/* YP */
