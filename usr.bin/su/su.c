/*	$OpenBSD: su.c,v 1.41 2001/09/18 04:41:09 millert Exp $	*/

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
static const char copyright[] =
"@(#) Copyright (c) 1988 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static const char sccsid[] = "from: @(#)su.c	5.26 (Berkeley) 7/6/91";
#else
static const char rcsid[] = "$OpenBSD: su.c,v 1.41 2001/09/18 04:41:09 millert Exp $";
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <err.h>
#include <errno.h>
#include <grp.h>
#include <login_cap.h>
#include <paths.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif
#include <bsd_auth.h>

char   *ontty __P((void));
int	chshell __P((char *));
void	usage __P((void));
void	auth_err __P((auth_session_t *, int, const char *, ...));
void	auth_errx __P((auth_session_t *, int, const char *, ...));

int
main(argc, argv)
	int argc;
	char **argv;
{
	extern char **environ;
	enum { UNSET, YES, NO } iscsh;
	struct passwd *pwd;
	struct group *gr;
	uid_t ruid;
	login_cap_t *lc;
	auth_session_t *as;
	int asme, asthem, authok, ch, fastlogin, prio;
	char *class, *style, *p, **g;
	char *user, *shell, *avshell, *username, **np, *fullname;
	char avshellbuf[MAXPATHLEN];

	iscsh = UNSET;
	class = shell = style = NULL;
	asme = asthem = fastlogin = 0;
	while ((ch = getopt(argc, argv, "-a:c:fKlm")) != -1)
		switch(ch) {
		case 'a':
			if (style)
				usage();
			style = optarg;
			break;
		case 'c':
			if (class)
				usage();
			class = optarg;
			break;
		case 'f':
			fastlogin = 1;
			break;
		case 'K':
			if (style)
				usage();
			style = "passwd";
			break;
		case '-':
		case 'l':
			asme = 0;
			asthem = 1;
			break;
		case 'm':
			asme = 1;
			asthem = 0;
			break;
		case '?':
		default:
			usage();
		}
	argv += optind;

	errno = 0;
	prio = getpriority(PRIO_PROCESS, 0);
	if (errno)
		prio = 0;
	(void)setpriority(PRIO_PROCESS, 0, -2);
	openlog("su", LOG_CONS, 0);

	if ((as = auth_open()) == NULL) {
		syslog(LOG_ERR, "auth_open: %m");
		err(1, "unable to initialize BSD authentication");
	}
	auth_setoption(as, "login", "yes");

	/* get current login name and shell */
	ruid = getuid();
	username = getlogin();

	if (username != NULL)
		auth_setoption(as, "invokinguser", username);

	if (username == NULL || (pwd = getpwnam(username)) == NULL ||
	    pwd->pw_uid != ruid)
		pwd = getpwuid(ruid);
	if (pwd == NULL)
		auth_errx(as, 1, "who are you?");
	if ((username = strdup(pwd->pw_name)) == NULL)
		auth_errx(as, 1, "can't allocate memory");
	if (asme) {
		if (pwd->pw_shell && *pwd->pw_shell) {
			shell = strdup(pwd->pw_shell);
		} else {
			shell = _PATH_BSHELL;
			iscsh = NO;
		}
	}

	/* get target login information, default to root */
	user = *argv ? *argv : "root";
	np = *argv ? argv : argv - 1;

	if ((pwd = getpwnam(user)) == NULL)
		auth_errx(as, 1, "unknown login %s", user);
	if ((pwd = pw_dup(pwd)) == NULL)
		auth_errx(as, 1, "can't allocate memory");
	user = pwd->pw_name;

	/* If the user specified a login class and we are root, use it */
	if (ruid && class)
		auth_errx(as, 1, "only the superuser may specify a login class");
	if (class)
		pwd->pw_class = class;
	if ((lc = login_getclass(pwd->pw_class)) == NULL)
		auth_errx(as, 1, "no such login class: %s",
		    class ? class : LOGIN_DEFCLASS);

	if (ruid) {
		/*
		 * If we are trying to become root and the default style
		 * is being used, don't bother to look it up (we might be
		 * be su'ing up to fix /etc/login.conf)
		 */
		if ((pwd->pw_uid || !style || strcmp(style, LOGIN_DEFSTYLE)) &&
		    (style = login_getstyle(lc, style, "auth-su")) == NULL)
			auth_errx(as, 1, "invalid authentication type");
		fullname = user;
		/*
		 * Let the authentication program know whether they are
		 * in group wheel or not (if trying to become super user)
		 */
		if (pwd->pw_uid == 0 && (gr = getgrgid((gid_t)0))
		    && gr->gr_mem && *(gr->gr_mem)) {
			for (g = gr->gr_mem; *g; ++g) {
				if (strcmp(username, *g) == 0) {
					auth_setoption(as, "wheel", "yes");
					break;
				}
			}
			if (!*g)
				auth_setoption(as, "wheel", "no");
		}

		auth_verify(as, style, fullname, lc->lc_class, NULL);
		authok = auth_getstate(as);
		if ((authok & AUTH_ALLOW) == 0) {
			if ((p = auth_getvalue(as, "errormsg")) != NULL)
				fprintf(stderr, "%s\n", p);
			fprintf(stderr, "Sorry\n");
			syslog(LOG_AUTH|LOG_WARNING,
				"BAD SU %s to %s%s", username, user, ontty());
			auth_close(as);
			exit(1);
		}
	}

	if (asme) {
		/* if asme and non-standard target shell, must be root */
		if (!chshell(pwd->pw_shell) && ruid)
			auth_errx(as, 1, "permission denied (shell).");
	} else if (pwd->pw_shell && *pwd->pw_shell) {
		shell = pwd->pw_shell;
		iscsh = UNSET;
	} else {
		shell = _PATH_BSHELL;
		iscsh = NO;
	}

	if ((p = strrchr(shell, '/')))
		avshell = p+1;
	else
		avshell = shell;

	/* if we're forking a csh, we want to slightly muck the args */
	if (iscsh == UNSET)
		iscsh = strcmp(avshell, "csh") ? NO : YES;

	if (!asme) {
		if (asthem) {
			p = getenv("TERM");
			if ((environ = calloc(1, sizeof (char *))) == NULL)
				auth_errx(as, 1, "calloc");
			if (setusercontext(lc, pwd, pwd->pw_uid, LOGIN_SETPATH))
				auth_err(as, 1, "unable to set user context");
			if (p && setenv("TERM", p, 1) == -1)
				auth_err(as, 1, "unable to set environment");

			seteuid(pwd->pw_uid);
			setegid(pwd->pw_gid);
			if (chdir(pwd->pw_dir) < 0)
				auth_err(as, 1, "%s", pwd->pw_dir);
			seteuid(0);
			setegid(0);	/* XXX use a saved gid instead? */
		} else if (pwd->pw_uid == 0) {
			if (setusercontext(lc,
			    pwd, pwd->pw_uid, LOGIN_SETPATH|LOGIN_SETUMASK))
				auth_err(as, 1, "unable to set user context");
		}
		if (asthem || pwd->pw_uid) {
			if (setenv("LOGNAME", pwd->pw_name, 1) == -1 ||
			    setenv("USER", pwd->pw_name, 1) == -1)
				auth_err(as, 1, "unable to set environment");
		}
		if (setenv("HOME", pwd->pw_dir, 1) == -1 ||
		    setenv("SHELL", shell, 1) == -1)
			auth_err(as, 1, "unable to set environment");
	}

	if (iscsh == YES) {
		if (fastlogin)
			*np-- = "-f";
		if (asme)
			*np-- = "-m";
	}

	if (asthem) {
		avshellbuf[0] = '-';
		strlcpy(avshellbuf+1, avshell, sizeof(avshellbuf) - 1);
		avshell = avshellbuf;
	} else if (iscsh == YES) {
		/* csh strips the first character... */
		avshellbuf[0] = '_';
		strlcpy(avshellbuf+1, avshell, sizeof(avshellbuf) - 1);
		avshell = avshellbuf;
	}
			
	*np = avshell;

	if (ruid != 0)
		syslog(LOG_NOTICE|LOG_AUTH, "%s to %s%s",
		    username, user, ontty());

	(void)setpriority(PRIO_PROCESS, 0, prio);
	if (setusercontext(lc, pwd, pwd->pw_uid,
	    (asthem ? (LOGIN_SETPRIORITY | LOGIN_SETUMASK) : 0) |
	    LOGIN_SETRESOURCES | LOGIN_SETGROUP | LOGIN_SETUSER))
		auth_err(as, 1, "unable to set user context");
	if (pwd->pw_uid && auth_approval(as, lc, pwd->pw_name, "su") <= 0)
		auth_err(as, 1, "approval failure");
	auth_close(as);

	execv(shell, np);
	err(1, "%s", shell);
}

int
chshell(sh)
	char *sh;
{
	char *cp;

	while ((cp = getusershell()) != NULL)
		if (strcmp(cp, sh) == 0)
			return (1);
	return (0);
}

char *
ontty()
{
	static char buf[MAXPATHLEN + 4];
	char *p;

	buf[0] = 0;
	if ((p = ttyname(STDERR_FILENO)))
		snprintf(buf, sizeof(buf), " on %s", p);
	return (buf);
}

void
usage()
{
	extern char *__progname;

	fprintf(stderr, "usage: %s [-fKlm] [-a auth-type] %s ", __progname,
	    "[-c login-class] [login [shell arguments]]\n");
	exit(1);
}

void
#ifdef __STDC__
auth_err(auth_session_t *as, int eval, const char *fmt, ...)
#else
auth_err(va_alist)
	va_dcl
#endif
{
	va_list ap;
#ifdef __STDC__
	va_start(ap, fmt);
#else
	auth_session_t *as;
	int eval;
	const char *fmt;

	va_start(ap);
	as = va_arg(ap, auth_session_t *);
	eval = va_arg(ap, int);
	fmt = va_arg(ap, const char *);
#endif
	verr(eval, fmt, ap);
	auth_close(as);
	va_end(ap);
}

void
#ifdef __STDC__
auth_errx(auth_session_t *as, int eval, const char *fmt, ...)
#else
auth_errx(va_alist)
	va_dcl
#endif
{
	va_list ap;
#ifdef __STDC__
	va_start(ap, fmt);
#else
	auth_session_t *as;
	int eval;
	const char *fmt;

	va_start(ap);
	as = va_arg(ap, auth_session_t *);
	eval = va_arg(ap, int);
	fmt = va_arg(ap, const char *);
#endif
	verrx(eval, fmt, ap);
	auth_close(as);
	va_end(ap);
}
