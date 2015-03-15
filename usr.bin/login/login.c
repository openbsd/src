/*	$OpenBSD: login.c,v 1.64 2015/03/15 00:41:28 millert Exp $	*/
/*	$NetBSD: login.c,v 1.13 1996/05/15 23:50:16 jtc Exp $	*/

/*-
 * Copyright (c) 1980, 1987, 1988, 1991, 1993, 1994
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
/*-
 * Copyright (c) 1995 Berkeley Software Design, Inc. All rights reserved.
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
 *      This product includes software developed by Berkeley Software Design,
 *      Inc.
 * 4. The name of Berkeley Software Design, Inc.  may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BERKELEY SOFTWARE DESIGN, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL BERKELEY SOFTWARE DESIGN, INC. BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	BSDI $From: login.c,v 2.28 1999/09/08 22:35:36 prb Exp $
 */

/*
 * login [ name ]
 * login -h hostname	(for telnetd, etc.)
 * login -f name	(for pre-authenticated login: datakit, xterm, etc.)
 * login -p		(preserve existing environment; for getty)
 */

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <login_cap.h>
#include <netdb.h>
#include <pwd.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <ttyent.h>
#include <unistd.h>
#include <limits.h>
#include <utmp.h>
#include <util.h>
#include <bsd_auth.h>

#include "pathnames.h"

void	 badlogin(char *);
void	 dolastlog(int);
void	 getloginname(void);
void	 motd(void);
void	 quickexit(int);
int	 rootterm(char *);
void	 sigint(int);
void	 sighup(int);
void	 sleepexit(int);
char	*stypeof(char *);
void	 timedout(int);
int	 main(int, char **);

extern int check_failedlogin(uid_t);
extern void log_failedlogin(uid_t, char *, char *, char *);

#define	TTYGRPNAME	"tty"		/* name of group to own ttys */

#define	SECSPERDAY	(24 * 60 * 60)
#define	TWOWEEKS	(2 * 7 * SECSPERDAY)

/*
 * This bounds the time given to login; may be overridden by /etc/login.conf.
 */
u_int		timeout = 300;

struct passwd	*pwd;
login_cap_t	*lc = NULL;
auth_session_t	*as = NULL;
int		failures;
int		needbanner = 1;
char		term[64], *hostname, *tty;
char		*style;
char		*username = NULL, *rusername = NULL;

extern char **environ;

int
main(int argc, char *argv[])
{
	char *domain, *p, *ttyn, *shell, *fullname, *instance;
	char *lipaddr, *script, *ripaddr, *style, *type, *fqdn;
	char tbuf[PATH_MAX + 2], tname[sizeof(_PATH_TTY) + 10];
	char localhost[HOST_NAME_MAX+1], *copyright;
	char mail[sizeof(_PATH_MAILDIR) + 1 + NAME_MAX];
	int ask, ch, cnt, fflag, pflag, quietlog, rootlogin, lastchance;
	int error, homeless, needto, authok, tries, backoff;
	struct addrinfo *ai, hints;
	struct rlimit cds, scds;
	quad_t expire, warning;
	struct utmp utmp;
	struct group *gr;
	struct stat st;
	uid_t uid;

	openlog("login", LOG_ODELAY, LOG_AUTH);

	fqdn = lipaddr = ripaddr = fullname = type = NULL;
	authok = 0;
	tries = 10;
	backoff = 3;

	domain = NULL;
	if (gethostname(localhost, sizeof(localhost)) < 0) {
		syslog(LOG_ERR, "couldn't get local hostname: %m");
		strlcpy(localhost, "localhost", sizeof(localhost));
	} else if ((domain = strchr(localhost, '.'))) {
		domain++;
		if (*domain && strchr(domain, '.') == NULL)
			domain = localhost;
	}

	if ((as = auth_open()) == NULL) {
		syslog(LOG_ERR, "auth_open: %m");
		err(1, "unable to initialize BSD authentication");
	}
	auth_setoption(as, "login", "yes");

	/*
	 * -p is used by getty to tell login not to destroy the environment
	 * -f is used to skip a second login authentication
	 * -h is used by other servers to pass the name of the remote
	 *    host to login so that it may be placed in utmp and wtmp
	 */
	fflag = pflag = 0;
	uid = getuid();
	while ((ch = getopt(argc, argv, "fh:pu:L:R:")) != -1)
		switch (ch) {
		case 'f':
			fflag = 1;
			break;
		case 'h':
			if (uid) {
				warnc(EPERM, "-h option");
				quickexit(1);
			}
			free(fqdn);
			if ((fqdn = strdup(optarg)) == NULL) {
				warn(NULL);
				quickexit(1);
			}
			auth_setoption(as, "fqdn", fqdn);
			if (domain && (p = strchr(optarg, '.')) &&
			    strcasecmp(p+1, domain) == 0)
				*p = 0;
			hostname = optarg;
			auth_setoption(as, "hostname", hostname);
			break;
		case 'L':
			if (uid) {
				warnc(EPERM, "-L option");
				quickexit(1);
			}
			if (lipaddr) {
				warnx("duplicate -L option");
				quickexit(1);
			}
			lipaddr = optarg;
			memset(&hints, 0, sizeof(hints));
			hints.ai_family = PF_UNSPEC;
			hints.ai_flags = AI_CANONNAME;
			error = getaddrinfo(lipaddr, NULL, &hints, &ai);
			if (!error) {
				strlcpy(localhost, ai->ai_canonname,
				    sizeof(localhost));
				freeaddrinfo(ai);
			} else
				strlcpy(localhost, lipaddr, sizeof(localhost));
			auth_setoption(as, "local_addr", lipaddr);
			break;
		case 'p':
			pflag = 1;
			break;
		case 'R':
			if (uid) {
				warnc(EPERM, "-R option");
				quickexit(1);
			}
			if (ripaddr) {
				warnx("duplicate -R option");
				quickexit(1);
			}
			ripaddr = optarg;
			auth_setoption(as, "remote_addr", ripaddr);
			break;
		case 'u':
			if (uid) {
				warnc(EPERM, "-u option");
				quickexit(1);
			}
			rusername = optarg;
			break;
		default:
			if (!uid)
				syslog(LOG_ERR, "invalid flag %c", ch);
			(void)fprintf(stderr,
			    "usage: login [-fp] [-h hostname] [-L local-addr] "
			    "[-R remote-addr] [-u username]\n\t[user]\n");
			quickexit(1);
		}
	argc -= optind;
	argv += optind;

	if (*argv) {
		username = *argv;
		ask = 0;
	} else
		ask = 1;

	/*
	 * If effective user is not root, just run su(1) to emulate login(1).
	 */
	if (geteuid() != 0) {
		char *av[5], **ap;

		auth_close(as);
		closelog();
		closefrom(STDERR_FILENO + 1);

		ap = av;
		*ap++ = _PATH_SU;
		*ap++ = "-L";
		if (!pflag)
			*ap++ = "-l";
		if (!ask)
			*ap++ = username;
		*ap = NULL;
		execv(_PATH_SU, av);
		warn("unable to exec %s", _PATH_SU);
		_exit(1);
	}

	ttyn = ttyname(STDIN_FILENO);
	if (ttyn == NULL || *ttyn == '\0') {
		(void)snprintf(tname, sizeof(tname), "%s??", _PATH_TTY);
		ttyn = tname;
	}
	if ((tty = strrchr(ttyn, '/')))
		++tty;
	else
		tty = ttyn;

	/*
	 * Since login deals with sensitive information, turn off coredumps.
	 */
	if (getrlimit(RLIMIT_CORE, &scds) < 0) {
		syslog(LOG_ERR, "couldn't get core dump size: %m");
		scds.rlim_cur = scds.rlim_max = QUAD_MIN;
	}
	cds.rlim_cur = cds.rlim_max = 0;
	if (setrlimit(RLIMIT_CORE, &cds) < 0) {
		syslog(LOG_ERR, "couldn't set core dump size to 0: %m");
		scds.rlim_cur = scds.rlim_max = QUAD_MIN;
	}

	(void)signal(SIGALRM, timedout);
	if (argc > 1) {
		needto = 0;
		(void)alarm(timeout);
	} else
		needto = 1;
	(void)signal(SIGQUIT, SIG_IGN);
	(void)signal(SIGINT, SIG_IGN);
	(void)signal(SIGHUP, SIG_IGN);
	(void)setpriority(PRIO_PROCESS, 0, 0);

#ifdef notyet
	/* XXX - we don't (yet) support per-tty auth stuff */
	/* BSDi uses a ttys.conf file but we could just overload /etc/ttys */
	/*
	 * Classify the attempt.
	 * By default we use the value in the ttys file.
	 * If there is a classify script we run that as
	 *
	 *	classify [-f] [username]
	 */
	if (type = getttyauth(tty))
		auth_setoption(as, "auth_type", type);
#endif

	/* get the default login class */
	if ((lc = login_getclass(0)) == NULL) { /* get the default class */
		warnx("Failure to retrieve default class");
		quickexit(1);
	}
	timeout = (u_int)login_getcapnum(lc, "login-timeout", 300, 300);
	if ((script = login_getcapstr(lc, "classify", NULL, NULL)) != NULL) {
		unsetenv("AUTH_TYPE");
		unsetenv("REMOTE_NAME");
		if (script[0] != '/') {
			syslog(LOG_ERR, "Invalid classify script: %s", script);
			warnx("Classification failure");
			quickexit(1);
		}
		shell = strrchr(script, '/') + 1;
		auth_setstate(as, AUTH_OKAY);
		auth_call(as, script, shell,
		    fflag ? "-f" : username, fflag ? username : 0, (char *)0);
		if (!(auth_getstate(as) & AUTH_ALLOW))
			quickexit(1);
		auth_setenv(as);
		if ((p = getenv("AUTH_TYPE")) != NULL &&
		    strncmp(p, "auth-", 5) == 0)
			type = p;
		if ((p = getenv("REMOTE_NAME")) != NULL)
			hostname = p;
		/*
		 * we may have changed some values, reset them
		 */
		auth_clroptions(as);
		if (type)
			auth_setoption(as, "auth_type", type);
		if (fqdn)
			auth_setoption(as, "fqdn", fqdn);
		if (hostname)
			auth_setoption(as, "hostname", hostname);
		if (lipaddr)
			auth_setoption(as, "local_addr", lipaddr);
		if (ripaddr)
			auth_setoption(as, "remote_addr", ripaddr);
	}

	/*
	 * Request the things like the approval script print things
	 * to stdout (in particular, the nologins files)
	 */
	auth_setitem(as, AUTHV_INTERACTIVE, "True");

	for (cnt = 0;; ask = 1) {
		/*
		 * Clean up our current authentication session.
		 * Options are not cleared so we need to clear any
		 * we might set below.
		 */
		auth_clean(as);
		auth_clroption(as, "style");
		auth_clroption(as, "lastchance");

		lastchance = 0;

		if (ask) {
			fflag = 0;
			getloginname();
		}
		if (needto) {
			needto = 0;
			alarm(timeout);
		}
		if ((style = strchr(username, ':')) != NULL)
			*style++ = '\0';
		if (fullname)
			free(fullname);
		if (auth_setitem(as, AUTHV_NAME, username) < 0 ||
		    (fullname = strdup(username)) == NULL) {
			syslog(LOG_ERR, "%m");
			warn(NULL);
			quickexit(1);
		}
		rootlogin = 0;
		if ((instance = strchr(username, '/')) != NULL) {
			if (strncmp(instance + 1, "root", 4) == 0)
				rootlogin = 1;
			*instance++ = '\0';
		} else
			instance = "";

		if (strlen(username) > UT_NAMESIZE)
			username[UT_NAMESIZE] = '\0';

		/*
		 * Note if trying multiple user names; log failures for
		 * previous user name, but don't bother logging one failure
		 * for nonexistent name (mistyped username).
		 */
		if (failures && strcmp(tbuf, username)) {
			if (failures > (pwd ? 0 : 1))
				badlogin(tbuf);
			failures = 0;
		}
		(void)strlcpy(tbuf, username, sizeof(tbuf));

		if ((pwd = getpwnam(username)) != NULL &&
		    auth_setpwd(as, pwd) < 0) {
			syslog(LOG_ERR, "%m");
			warn(NULL);
			quickexit(1);
		}

		lc = login_getclass(pwd ? pwd->pw_class : NULL);
		if (!lc)
			goto failed;

		style = login_getstyle(lc, style, type);
		if (!style)
			goto failed;

		/*
		 * We allow "login-tries" attempts to login but start
		 * slowing down after "login-backoff" attempts.
		 */
		tries = (int)login_getcapnum(lc, "login-tries", 10, 10);
		backoff = (int)login_getcapnum(lc, "login-backoff", 3, 3);

		/*
		 * Turn off the fflag if we have an invalid user
		 * or we are not root and we are trying to change uids.
		 */
		if (!pwd || (uid && uid != pwd->pw_uid))
			fflag = 0;

		if (pwd && pwd->pw_uid == 0)
			rootlogin = 1;

		/*
		 * If we do not have the force flag authenticate the user
		 */
		if (!fflag) {
			lastchance =
			    login_getcaptime(lc, "password-dead", 0, 0) != 0;
			if (lastchance)
				auth_setoption(as, "lastchance", "yes");
			/*
			 * Once we start asking for a password
			 *  we want to log a failure on a hup.
			 */
			signal(SIGHUP, sighup);
			auth_verify(as, style, NULL, lc->lc_class, NULL);
			authok = auth_getstate(as);
			/*
			 * If their password expired and it has not been
			 * too long since then, give the user one last
			 * chance to change their password
			 */
			if ((authok & AUTH_PWEXPIRED) && lastchance) {
				authok = AUTH_OKAY;
			} else
				lastchance = 0;
			if ((authok & AUTH_ALLOW) == 0)
				goto failed;
			if (auth_setoption(as, "style", style) < 0) {
				syslog(LOG_ERR, "%m");
				warn(NULL);
				quickexit(1);
			}
		}
		/*
		 * explicitly reject users without password file entries
		 */
		if (pwd == NULL)
			goto failed;

		/*
		 * If trying to log in as root on an insecure terminal,
		 * refuse the login attempt unless the authentication
		 * style explicitly says a root login is okay.
		 */
		if (pwd && rootlogin && !rootterm(tty))
			goto failed;

		if (fflag) {
			type = 0;
			style = "forced";
		}
		break;

failed:
		if (authok & AUTH_SILENT)
			quickexit(0);
		if (rootlogin && !rootterm(tty)) {
			warnx("%s login refused on this terminal.",
			    fullname);
			if (hostname)
				syslog(LOG_NOTICE,
				    "LOGIN %s REFUSED FROM %s%s%s ON TTY %s",
				    fullname, rusername ? rusername : "",
				    rusername ? "@" : "", hostname, tty);
			else
				syslog(LOG_NOTICE,
				    "LOGIN %s REFUSED ON TTY %s",
				    fullname, tty);
		} else {
			if (!as || (p = auth_getvalue(as, "errormsg")) == NULL)
				p = "Login incorrect";
			(void)printf("%s\n", p);
		}
		failures++;
		if (pwd)
			log_failedlogin(pwd->pw_uid, hostname, rusername, tty);
		/*
		 * By default, we allow 10 tries, but after 3 we start
		 * backing off to slow down password guessers.
		 */
		if (++cnt > backoff) {
			if (cnt >= tries) {
				badlogin(username);
				sleepexit(1);
			}
			sleep((u_int)((cnt - backoff) * tries / 2));
		}
	}

	/* committed to login -- turn off timeout */
	(void)alarm(0);

	endpwent();

	shell = login_getcapstr(lc, "shell", pwd->pw_shell, pwd->pw_shell);
	if (*shell == '\0')
		shell = _PATH_BSHELL;
	else if (strlen(shell) >= PATH_MAX) {
		syslog(LOG_ERR, "shell path too long: %s", shell);
		warnx("invalid shell");
		quickexit(1);
	}

	/* Destroy environment unless user has requested its preservation. */
	if (!pflag) {
		if ((environ = calloc(1, sizeof (char *))) == NULL)
			err(1, "calloc");
	} else {
		char **cpp, **cpp2;

		for (cpp2 = cpp = environ; *cpp; cpp++) {
			if (strncmp(*cpp, "LD_", 3) &&
			    strncmp(*cpp, "ENV=", 4) &&
			    strncmp(*cpp, "BASH_ENV=", 9) &&
			    strncmp(*cpp, "IFS=", 4))
				*cpp2++ = *cpp;
		}
		*cpp2 = 0;
	}
	/* Note: setusercontext(3) will set PATH */
	if (setenv("HOME", pwd->pw_dir, 1) == -1 ||
	    setenv("SHELL", pwd->pw_shell, 1) == -1) {
		warn("unable to setenv()");
		quickexit(1);
	}
	if (term[0] == '\0')
		(void)strlcpy(term, stypeof(tty), sizeof(term));
	(void)snprintf(mail, sizeof(mail), "%s/%s", _PATH_MAILDIR,
		pwd->pw_name);
	if (setenv("TERM", term, 0) == -1 ||
	    setenv("LOGNAME", pwd->pw_name, 1) == -1 ||
	    setenv("USER", pwd->pw_name, 1) == -1 ||
	    setenv("MAIL", mail, 1) == -1) {
		warn("unable to setenv()");
		quickexit(1);
	}
	if (hostname) {
		if (setenv("REMOTEHOST", hostname, 1) == -1) {
			warn("unable to setenv()");
			quickexit(1);
		}
	}
	if (rusername) {
		if (setenv("REMOTEUSER", rusername, 1) == -1) {
			warn("unable to setenv()");
			quickexit(1);
		}
	}

	if (setusercontext(lc, pwd, pwd->pw_uid, LOGIN_SETPATH)) {
		warn("unable to set user context");
		quickexit(1);
	}
	auth_setenv(as);

	/* if user not super-user, check for disabled logins */
	if (!rootlogin)
		auth_checknologin(lc);

	setegid(pwd->pw_gid);
	seteuid(pwd->pw_uid);

	homeless = chdir(pwd->pw_dir);
	if (homeless) {
		if (login_getcapbool(lc, "requirehome", 0)) {
			(void)printf("No home directory %s!\n", pwd->pw_dir);
			quickexit(1);
		}
		if (chdir("/"))
			quickexit(0);
	}

	quietlog = ((strcmp(pwd->pw_shell, "/sbin/nologin") == 0) ||
	    login_getcapbool(lc, "hushlogin", 0) ||
	    (access(_PATH_HUSHLOGIN, F_OK) == 0));

	seteuid(0);
	setegid(0);	/* XXX use a saved gid instead? */

	if ((p = auth_getvalue(as, "warnmsg")) != NULL)
		(void)printf("WARNING: %s\n\n", p);

	expire = auth_check_expire(as);
	if (expire < 0) {
		(void)printf("Sorry -- your account has expired.\n");
		quickexit(1);
	} else if (expire > 0 && !quietlog) {
		warning = login_getcaptime(lc, "expire-warn",
		    TWOWEEKS, TWOWEEKS);
		if (expire < warning)
			(void)printf("Warning: your account expires on %s",
			    ctime(&pwd->pw_expire));
	}

	/* Nothing else left to fail -- really log in. */
	(void)signal(SIGHUP, SIG_DFL);
	memset(&utmp, 0, sizeof(utmp));
	(void)time(&utmp.ut_time);
	(void)strncpy(utmp.ut_name, username, sizeof(utmp.ut_name));
	if (hostname)
		(void)strncpy(utmp.ut_host, hostname, sizeof(utmp.ut_host));
	(void)strncpy(utmp.ut_line, tty, sizeof(utmp.ut_line));
	login(&utmp);

	if (!quietlog)
		(void)check_failedlogin(pwd->pw_uid);
	dolastlog(quietlog);

	login_fbtab(tty, pwd->pw_uid, pwd->pw_gid);

	(void)chown(ttyn, pwd->pw_uid,
	    (gr = getgrnam(TTYGRPNAME)) ? gr->gr_gid : pwd->pw_gid);

	/* If fflag is on, assume caller/authenticator has logged root login. */
	if (rootlogin && fflag == 0) {
		if (hostname)
			syslog(LOG_NOTICE, "ROOT LOGIN (%s) ON %s FROM %s%s%s",
			    username, tty, rusername ? rusername : "",
			    rusername ? "@" : "", hostname);
		else
			syslog(LOG_NOTICE, "ROOT LOGIN (%s) ON %s", username, tty);
	}

	if (!quietlog) {
		if ((copyright =
		    login_getcapstr(lc, "copyright", NULL, NULL)) != NULL)
			auth_cat(copyright);
		motd();
		if (stat(mail, &st) == 0 && st.st_size != 0)
			(void)printf("You have %smail.\n",
			    (st.st_mtime > st.st_atime) ? "new " : "");
	}

	(void)signal(SIGALRM, SIG_DFL);
	(void)signal(SIGQUIT, SIG_DFL);
	(void)signal(SIGHUP, SIG_DFL);
	(void)signal(SIGINT, SIG_DFL);
	(void)signal(SIGTSTP, SIG_IGN);

	tbuf[0] = '-';
	(void)strlcpy(tbuf + 1, (p = strrchr(shell, '/')) ?
	    p + 1 : shell, sizeof(tbuf) - 1);

	if ((scds.rlim_cur != QUAD_MIN || scds.rlim_max != QUAD_MIN) &&
	    setrlimit(RLIMIT_CORE, &scds) < 0)
		syslog(LOG_ERR, "couldn't reset core dump size: %m");

	if (lastchance)
		(void)printf("WARNING: Your password has expired."
		    "  You must change your password, now!\n");

	if (setusercontext(lc, pwd, rootlogin ? 0 : pwd->pw_uid,
	    LOGIN_SETALL & ~LOGIN_SETPATH) < 0) {
		warn("unable to set user context");
		quickexit(1);
	}

	if (homeless) {
		(void)printf("No home directory %s!\n", pwd->pw_dir);
		(void)printf("Logging in with home = \"/\".\n");
		(void)setenv("HOME", "/", 1);
	}

	if (auth_approval(as, lc, NULL, "login") == 0) {
		if (auth_getstate(as) & AUTH_EXPIRED)
			(void)printf("Sorry -- your account has expired.\n");
		else
			(void)printf("approval failure\n");
		quickexit(1);
	}

	/*
	 * The last thing we do is discard all of the open file descriptors.
	 * Last because the C library may have some open.
	 */
	closefrom(STDERR_FILENO + 1);

	/*
	 * Close the authentication session, make sure it is marked
	 * as okay so no files are removed.
	 */
	auth_setstate(as, AUTH_OKAY);
	auth_close(as);

	execlp(shell, tbuf, (char *)NULL);
	err(1, "%s", shell);
}

/*
 * Allow for a '.' and 16 characters for any instance as well as
 * space for a ':' and 16 characters defining the authentication type.
 */
#define NBUFSIZ		(UT_NAMESIZE + 1 + 16 + 1 + 16)

void
getloginname(void)
{
	static char nbuf[NBUFSIZ], *p;
	int ch;

	for (;;) {
		(void)printf("login: ");
		for (p = nbuf; (ch = getchar()) != '\n'; ) {
			if (ch == EOF) {
				badlogin(username);
				quickexit(0);
			}
			if (p < nbuf + (NBUFSIZ - 1))
				*p++ = ch;
		}
		if (p > nbuf) {
			if (nbuf[0] == '-')
				(void)fprintf(stderr,
				    "login names may not start with '-'.\n");
			else {
				*p = '\0';
				username = nbuf;
				break;
			}
		}
	}
}

int
rootterm(char *ttyn)
{
	struct ttyent *t;

	/* XXX - stash output of getttynam() elsewhere */
	return ((t = getttynam(ttyn)) && t->ty_status & TTY_SECURE);
}

void
motd(void)
{
	char tbuf[8192], *motd;
	int fd, nchars;
	struct sigaction sa, osa;

	motd = login_getcapstr(lc, "welcome", _PATH_MOTDFILE, _PATH_MOTDFILE);

	if ((fd = open(motd, O_RDONLY, 0)) < 0)
		return;

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = sigint;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;		/* don't set SA_RESTART */
	(void)sigaction(SIGINT, &sa, &osa);

	/* read and spew motd until EOF, error, or SIGINT */
	while ((nchars = read(fd, tbuf, sizeof(tbuf))) > 0 &&
	    write(STDOUT_FILENO, tbuf, nchars) == nchars)
		;

	(void)sigaction(SIGINT, &osa, NULL);
	(void)close(fd);
}

/* ARGSUSED */
void
sigint(int signo)
{
	return;			/* just interrupt syscall */
}

/* ARGSUSED */
void
timedout(int signo)
{
	char warn[1024];

	snprintf(warn, sizeof warn,
	    "Login timed out after %d seconds\n", timeout);
	write(STDERR_FILENO, warn, strlen(warn));
	if (username)
		badlogin(username);
	_exit(0);
}

void
dolastlog(int quiet)
{
	struct lastlog ll;
	int fd;

	if ((fd = open(_PATH_LASTLOG, O_RDWR, 0)) >= 0) {
		(void)lseek(fd, (off_t)pwd->pw_uid * sizeof(ll), SEEK_SET);
		if (!quiet) {
			if (read(fd, (char *)&ll, sizeof(ll)) == sizeof(ll) &&
			    ll.ll_time != 0) {
				(void)printf("Last login: %.*s ",
				    24-5, (char *)ctime(&ll.ll_time));
				(void)printf("on %.*s",
				    (int)sizeof(ll.ll_line),
				    ll.ll_line);
				if (*ll.ll_host != '\0')
					(void)printf(" from %.*s",
					    (int)sizeof(ll.ll_host),
					    ll.ll_host);
				(void)putchar('\n');
			}
			(void)lseek(fd, (off_t)pwd->pw_uid * sizeof(ll),
			    SEEK_SET);
		}
		memset((void *)&ll, 0, sizeof(ll));
		(void)time(&ll.ll_time);
		(void)strncpy(ll.ll_line, tty, sizeof(ll.ll_line));
		if (hostname)
			(void)strncpy(ll.ll_host, hostname, sizeof(ll.ll_host));
		(void)write(fd, (char *)&ll, sizeof(ll));
		(void)close(fd);
	}
}

void
badlogin(char *name)
{
	struct syslog_data sdata = SYSLOG_DATA_INIT;

	if (failures == 0)
		return;
	if (hostname) {
		syslog_r(LOG_NOTICE, &sdata,
		    "%d LOGIN FAILURE%s FROM %s%s%s",
		    failures, failures > 1 ? "S" : "",
		    rusername ? rusername : "", rusername ? "@" : "", hostname);
		syslog_r(LOG_AUTHPRIV|LOG_NOTICE, &sdata,
		    "%d LOGIN FAILURE%s FROM %s%s%s, %s",
		    failures, failures > 1 ? "S" : "",
		    rusername ? rusername : "", rusername ? "@" : "",
		    hostname, name);
	} else {
		syslog_r(LOG_NOTICE, &sdata,
		    "%d LOGIN FAILURE%s ON %s",
		    failures, failures > 1 ? "S" : "", tty);
		syslog_r(LOG_AUTHPRIV|LOG_NOTICE, &sdata,
		    "%d LOGIN FAILURE%s ON %s, %s",
		    failures, failures > 1 ? "S" : "", tty, name);
	}
}

#undef	UNKNOWN
#define	UNKNOWN	"su"

char *
stypeof(char *ttyid)
{
	struct ttyent *t;

	return (ttyid && (t = getttynam(ttyid)) ? t->ty_type :
	    login_getcapstr(lc, "term", UNKNOWN, UNKNOWN));
}

void
sleepexit(int eval)
{
	auth_close(as);
	(void)sleep(5);
	exit(eval);
}

void
quickexit(int eval)
{
	if (as)
		auth_close(as);
	exit(eval);
}


void
sighup(int signum)
{
	if (username)
		badlogin(username);
	_exit(0);
}
