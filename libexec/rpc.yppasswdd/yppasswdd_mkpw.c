/*	$OpenBSD: yppasswdd_mkpw.c,v 1.27 2004/04/20 23:21:23 millert Exp $	*/

/*
 * Copyright (c) 1994 Mats O Jansson <moj@stacken.kth.se>
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef LINT
static char rcsid[] = "$OpenBSD: yppasswdd_mkpw.c,v 1.27 2004/04/20 23:21:23 millert Exp $";
#endif

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <fcntl.h>
#include <rpc/rpc.h>
#include <rpcsvc/yppasswd.h>
#include <db.h>
#include <pwd.h>
#include <stdlib.h>
#include <unistd.h>
#include <util.h>
#include <ctype.h>
#include <string.h>
#include <syslog.h>

extern int noshell;
extern int nogecos;
extern int nopw;
extern int make;
extern char make_arg[];
extern char *dir;

char *ok_shell(char *);
int badchars(char *);
int subst(char *, char, char);
int make_passwd(yppasswd *);

char *
ok_shell(char *name)
{
	char *p, *sh;

	setusershell();
	while ((sh = getusershell())) {
		if (!strcmp(name, sh))
			return (name);
		/* allow just shell name, but use "real" path */
		if ((p = strrchr(sh, '/')) && strcmp(name, p + 1) == 0)
			return (sh);
	}
	return (NULL);
}

int
badchars(char *base)
{
	int ampr = 0;
	char *s;

	for (s = base; *s; s++) {
		if (*s == '&')
			ampr++;
		if (!isprint(*s))
			return 1;
		if (strchr(":\n\t\r", *s))
			return 1;
	}
	if (ampr > 10)
		return 1;
	return 0;
}

int
subst(char *s, char from, char to)
{
	int	n = 0;

	while (*s) {
		if (*s == from) {
			*s = to;
			n++;
		}
		s++;
	}
	return (n);
}

int
make_passwd(yppasswd *argp)
{
	struct passwd pw;
	int     pfd = -1, tfd;
	char	buf[11], *bp = NULL, *p, *t;
	int	n;
	ssize_t cnt;
	size_t	resid;
	struct stat st;
	char *master;

	pw_init();
	if (dir)
		pw_setdir(dir);
	master = pw_file(_PATH_MASTERPASSWD);
	if (!master)
		return (1);
	pfd = open(master, O_RDONLY);
	if (pfd < 0)
		goto fail;
	if (fstat(pfd, &st))
		goto fail;
	p = bp = malloc((resid = st.st_size) + 1);
	if (bp == NULL)
		goto fail;
	do {
		cnt = read(pfd, p, resid);
		if (cnt < 0)
			goto fail;
		p += cnt;
		resid -= cnt;
	} while (resid > 0);
	close(pfd);
	pfd = -1;
	*p = '\0';		/* Buf oflow prevention */

	p = bp;
	subst(p, '\n', '\0');
	for (n = 1; p < bp + st.st_size; n++, p = t) {
		t = strchr(p, '\0') + 1;
		cnt = subst(p, ':', '\0');
		if (cnt != 9) {
			syslog(LOG_WARNING, "bad entry at line %d of %s", n,
			    master);
			continue;
		}

		if (strcmp(p, argp->newpw.pw_name) == 0)
			break;
	}
	if (p >= bp + st.st_size)
		goto fail;

#define	EXPAND(e)	e = p; while (*p++);
	EXPAND(pw.pw_name);
	EXPAND(pw.pw_passwd);
	pw.pw_uid = atoi(p); EXPAND(t);
	pw.pw_gid = atoi(p); EXPAND(t);
	EXPAND(pw.pw_class);
	pw.pw_change = (time_t)atol(p); EXPAND(t);
	pw.pw_expire = (time_t)atol(p); EXPAND(t);
	EXPAND(pw.pw_gecos);
	EXPAND(pw.pw_dir);
	EXPAND(pw.pw_shell);

	if (strcmp(crypt(argp->oldpass, pw.pw_passwd), pw.pw_passwd) != 0)
		goto fail;

	if (!nopw && badchars(argp->newpw.pw_passwd))
		goto fail;
	if (!nogecos && badchars(argp->newpw.pw_gecos))
		goto fail;
	if (!nogecos && badchars(argp->newpw.pw_shell))
		goto fail;
	if (!ok_shell(argp->newpw.pw_shell) || !ok_shell(pw.pw_shell))
		goto fail;

	/*
	 * Get the new password.  Reset passwd change time to zero; when
	 * classes are implemented, go and get the "offset" value for this
	 * class and reset the timer.
	 */
	if (!nopw) {
		pw.pw_passwd = argp->newpw.pw_passwd;
		pw.pw_change = 0;
	}
	if (!nogecos)
		pw.pw_gecos = argp->newpw.pw_gecos;
	if (!noshell)
		pw.pw_shell = argp->newpw.pw_shell;

	for (n = 0, p = pw.pw_gecos; *p; p++) {
		if (*p == '&')
			n = n + strlen(pw.pw_name) - 1;
	}
	if (strlen(pw.pw_name) + 1 + strlen(pw.pw_passwd) + 1 +
	    strlen((snprintf(buf, sizeof buf, "%u", pw.pw_uid), buf)) + 1 +
	    strlen((snprintf(buf, sizeof buf, "%u", pw.pw_gid), buf)) + 1 +
	    strlen(pw.pw_gecos) + n + 1 + strlen(pw.pw_dir) + 1 +
	    strlen(pw.pw_shell) >= 1023)
		goto fail;

	pfd = open(master, O_RDONLY, 0);
	if (pfd < 0) {
		syslog(LOG_ERR, "cannot open %s", master);
		goto fail;
	}

	tfd = pw_lock(0);
	if (tfd < 0)
		goto fail;

	pw_copy(pfd, tfd, &pw, NULL);
	pw_mkdb(pw.pw_name, 0);
	free(bp);

	if (fork() == 0) {
		chdir("/var/yp");
		(void)umask(022);
		system(make_arg);
		exit(0);
	}
	return (0);

fail:
	if (bp)
		free(bp);
	if (pfd >= 0)
		close(pfd);
	free(master);
	return (1);
}
