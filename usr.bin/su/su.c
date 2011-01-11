/*	$OpenBSD: su.c,v 1.65 2011/01/11 10:07:56 robert Exp $	*/

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
#include <utmp.h>
#include <stdarg.h>
#include <bsd_auth.h>

char   *getloginname(void);
char   *ontty(void);
int	chshell(const char *);
int	verify_user(char *, struct passwd *, char *, login_cap_t *,
	    auth_session_t *);
void	usage(void);
void	auth_err(auth_session_t *, int, const char *, ...);
void	auth_errx(auth_session_t *, int, const char *, ...);

int
main(int argc, char **argv)
{
	int asme = 0, asthem = 0, ch, fastlogin = 0, emlogin = 0, prio;
	int altshell = 0, homeless = 0;
	char *user, *shell = NULL, *avshell, *username, **np;
	char *class = NULL, *style = NULL, *p;
	enum { UNSET, YES, NO } iscsh = UNSET;
	char avshellbuf[MAXPATHLEN];
	extern char **environ;
	auth_session_t *as;
	struct passwd *pwd;
	login_cap_t *lc;
	uid_t ruid;
	u_int flags;

	while ((ch = getopt(argc, argv, "a:c:fKLlms:-")) != -1)
		switch (ch) {
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
		case 'L':
			emlogin = 1;
			break;
		case 'l':
		case '-':
			asme = 0;
			asthem = 1;
			break;
		case 'm':
			asme = 1;
			asthem = 0;
			break;
		case 's':
			altshell = 1;
			shell = optarg;
			break;
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

	if (ruid && class)
		auth_errx(as, 1, "only the superuser may specify a login class");

	if (ruid && altshell)
		auth_errx(as, 1, "only the superuser may specify a login shell");

	if (username != NULL)
		auth_setoption(as, "invokinguser", username);

	if (username == NULL || (pwd = getpwnam(username)) == NULL ||
	    pwd->pw_uid != ruid)
		pwd = getpwuid(ruid);
	if (pwd == NULL)
		auth_errx(as, 1, "who are you?");
	if ((username = strdup(pwd->pw_name)) == NULL)
		auth_errx(as, 1, "can't allocate memory");
	if (asme && !altshell) {
		if (pwd->pw_shell && *pwd->pw_shell) {
			if ((shell = strdup(pwd->pw_shell)) == NULL)
				auth_errx(as, 1, "can't allocate memory");
		} else {
			shell = _PATH_BSHELL;
			iscsh = NO;
		}
	}

	for (;;) {
		/* get target user, default to root unless in -L mode */
		if (*argv) {
			user = *argv;
		} else if (emlogin) {
			if ((user = getloginname()) == NULL) {
				auth_close(as);
				exit(1);
			}
		} else {
			user = "root";
		}
		/* style may be specified as part of the username */
		if ((p = strchr(user, ':')) != NULL) {
			*p++ = '\0';
			style = p;	/* XXX overrides -a flag */
		}

		/*
		 * Clean and setup our current authentication session.
		 * Note that options *are* not cleared.
		 */
		auth_clean(as);
		if (auth_setitem(as, AUTHV_INTERACTIVE, "True") != 0 ||
		    auth_setitem(as, AUTHV_NAME, user) != 0)
			auth_errx(as, 1, "can't allocate memory");
		if ((user = auth_getitem(as, AUTHV_NAME)) == NULL)
			auth_errx(as, 1, "internal error");
		if (auth_setpwd(as, NULL) || (pwd = auth_getpwd(as)) == NULL) {
			if (emlogin)
				pwd = NULL;
			else
				auth_errx(as, 1, "unknown login %s", user);
		}

		/* If the user specified a login class, use it */
		if (!class && pwd && pwd->pw_class && pwd->pw_class[0] != '\0')
			class = strdup(pwd->pw_class);
		if ((lc = login_getclass(class)) == NULL)
			auth_errx(as, 1, "no such login class: %s",
			    class ? class : LOGIN_DEFCLASS);

		if ((ruid == 0 && !emlogin) ||
		    verify_user(username, pwd, style, lc, as) == 0)
			break;
		syslog(LOG_AUTH|LOG_WARNING, "BAD SU %s to %s%s",
		    username, user, ontty());
		if (!emlogin) {
			fprintf(stderr, "Sorry\n");
			auth_close(as);
			exit(1);
		}
		fprintf(stderr, "Login incorrect\n");
	}

	if (!altshell) {
		if (asme) {
			/* if asme and non-std target shell, must be root */
			if (ruid && !chshell(shell))
				auth_errx(as, 1, "permission denied (shell).");
		} else if (pwd->pw_shell && *pwd->pw_shell) {
			if ((shell = strdup(pwd->pw_shell)) == NULL)
				auth_errx(as, 1, "can't allocate memory");
			iscsh = UNSET;
		} else {
			shell = _PATH_BSHELL;
			iscsh = NO;
		}
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

			setegid(pwd->pw_gid);
			seteuid(pwd->pw_uid);

			homeless = chdir(pwd->pw_dir);
			if (homeless) {
				if (login_getcapbool(lc, "requirehome", 0)) {
					auth_err(as, 1, "%s", pwd->pw_dir);
				} else {
					(void)printf("No home directory %s!\n", pwd->pw_dir);
					(void)printf("Logging in with home = \"/\".\n");
					if (chdir("/") < 0)
						auth_err(as, 1, "/");
				}
			}
			setegid(0);	/* XXX use a saved gid instead? */
			seteuid(0);
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
		if (setenv("HOME", homeless ? "/" : pwd->pw_dir, 1) == -1 ||
		    setenv("SHELL", shell, 1) == -1)
			auth_err(as, 1, "unable to set environment");
	} else if (altshell) {
		if (setenv("SHELL", shell, 1) == -1)
			auth_err(as, 1, "unable to set environment");
	}

	np = *argv ? argv : argv - 1;
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
	if (emlogin) {
		flags = LOGIN_SETALL & ~LOGIN_SETPATH;
		/*
		 * Only call setlogin() if this process is a session leader.
		 * In practice, this means the login name will be set only if
		 * we are exec'd by a shell.  This is important because
		 * otherwise the parent shell's login name would change too.
		 */
		if (getsid(0) != getpid())
			flags &= ~LOGIN_SETLOGIN;
	} else {
		flags = LOGIN_SETRESOURCES|LOGIN_SETGROUP|LOGIN_SETUSER;
		if (asthem)
			flags |= LOGIN_SETENV|LOGIN_SETPRIORITY|LOGIN_SETUMASK;
	}
	if (setusercontext(lc, pwd, pwd->pw_uid, flags) != 0)
		auth_err(as, 1, "unable to set user context");
	if (pwd->pw_uid && auth_approval(as, lc, pwd->pw_name, "su") <= 0)
		auth_err(as, 1, "approval failure");
	auth_close(as);

	execv(shell, np);
	err(1, "%s", shell);
}

int
verify_user(char *from, struct passwd *pwd, char *style,
    login_cap_t *lc, auth_session_t *as)
{
	struct group *gr;
	char **g, *cp;
	int authok;

	/*
	 * If we are trying to become root and the default style
	 * is being used, don't bother to look it up (we might be
	 * be su'ing up to fix /etc/login.conf)
	 */
	if ((pwd == NULL || pwd->pw_uid != 0 || style == NULL ||
	    strcmp(style, LOGIN_DEFSTYLE) != 0) &&
	    (style = login_getstyle(lc, style, "auth-su")) == NULL)
		auth_errx(as, 1, "invalid authentication type");

	/*
	 * Let the authentication program know whether they are
	 * in group wheel or not (if trying to become super user)
	 */
	if (pwd != NULL && pwd->pw_uid == 0 && (gr = getgrgid(0)) != NULL &&
	    gr->gr_mem != NULL && *(gr->gr_mem) != NULL) {
		for (g = gr->gr_mem; *g; ++g) {
			if (strcmp(from, *g) == 0) {
				auth_setoption(as, "wheel", "yes");
				break;
			}
		}
		if (!*g)
			auth_setoption(as, "wheel", "no");
	}

	auth_verify(as, style, NULL, lc->lc_class, (char *)NULL);
	authok = auth_getstate(as);
	if ((authok & AUTH_ALLOW) == 0) {
		if ((cp = auth_getvalue(as, "errormsg")) != NULL)
			fprintf(stderr, "%s\n", cp);
		return(1);
	}
	return(0);
}

int
chshell(const char *sh)
{
	char *cp;
	int found = 0;

	setusershell();
	while ((cp = getusershell()) != NULL) {
		if (strcmp(cp, sh) == 0) {
			found = 1;
			break;
		}
	}
	endusershell();
	return (found);
}

char *
ontty(void)
{
	static char buf[MAXPATHLEN + 4];
	char *p;

	buf[0] = 0;
	if ((p = ttyname(STDERR_FILENO)))
		snprintf(buf, sizeof(buf), " on %s", p);
	return (buf);
}

/*
 * Allow for a '.' and 16 characters for any instance as well as
 * space for a ':' and 16 characters defining the authentication type.
 */
#define NBUFSIZ		(UT_NAMESIZE + 1 + 16 + 1 + 16)

char *
getloginname(void)
{
	static char nbuf[NBUFSIZ], *p;
	int ch;

	for (;;) {
		(void)printf("login: ");
		for (p = nbuf; (ch = getchar()) != '\n'; ) {
			if (ch == EOF)
				return (NULL);
			if (p < nbuf + (NBUFSIZ - 1))
				*p++ = ch;
		}
		if (p > nbuf) {
			if (nbuf[0] == '-') {
				(void)fprintf(stderr,
				    "login names may not start with '-'.\n");
			} else {
				*p = '\0';
				break;
			}
		}
	}
	return (nbuf);
}

void
usage(void)
{
	extern char *__progname;

	fprintf(stderr, "usage: %s [-fKLlm] [-a auth-type] [-c login-class] "
	    "[-s login-shell]\n"
	    "%-*s[login [shell arguments]]\n", __progname,
	    (int)strlen(__progname) + 8, "");
	exit(1);
}

void
auth_err(auth_session_t *as, int eval, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vwarn(fmt, ap);
	va_end(ap);
	auth_close(as);
	exit(eval);
}

void
auth_errx(auth_session_t *as, int eval, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vwarnx(fmt, ap);
	va_end(ap);
	auth_close(as);
	exit(eval);
}
