/*	$OpenBSD: su.c,v 1.35 2000/12/02 22:44:49 hin Exp $	*/

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
char copyright[] =
"@(#) Copyright (c) 1988 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
/*static char sccsid[] = "from: @(#)su.c	5.26 (Berkeley) 7/6/91";*/
static char rcsid[] = "$OpenBSD: su.c,v 1.35 2000/12/02 22:44:49 hin Exp $";
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
#include <fcntl.h>

#ifdef  SKEY
#include <skey.h>                                                       
#endif                                                                       

#ifdef KERBEROS
#include <des.h>
#include <kerberosIV/krb.h>
#include <netdb.h>

int kerberos __P((char *username, char *user, int uid));

#define	ARGSTR	"-Kc:flm"

int use_kerberos = 1;
char krbtkfile[MAXPATHLEN];
char lrealm[REALM_SZ];
int ksettkfile(char *);
#else
#define	ARGSTR	"-c:flm"
#endif

char   *ontty __P((void));
int	chshell __P((char *));

int
main(argc, argv)
	int argc;
	char **argv;
{
	extern char **environ;
	register struct passwd *pwd;
	register char *p, **g;
	struct group *gr;
	uid_t ruid;
	login_cap_t *lc;
	int asme, ch, asthem, fastlogin, prio;
	enum { UNSET, YES, NO } iscsh;
	char *user, *shell, *avshell, *username, *class, **np;
	char shellbuf[MAXPATHLEN], avshellbuf[MAXPATHLEN];

	iscsh = UNSET;
	shell = class = NULL;
	asme = asthem = fastlogin = 0;
	while ((ch = getopt(argc, argv, ARGSTR)) != -1)
		switch((char)ch) {
#ifdef KERBEROS
		case 'K':
			use_kerberos = 0;
			break;
#endif
		case 'c':
			class = optarg;
			break;
		case 'f':
			fastlogin = 1;
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
			(void)fprintf(stderr,
			    "usage: su [%s] [login [shell arguments]]\n",
			    ARGSTR);
			exit(1);
		}
	argv += optind;

	errno = 0;
	prio = getpriority(PRIO_PROCESS, 0);
	if (errno)
		prio = 0;
	(void)setpriority(PRIO_PROCESS, 0, -2);
	openlog("su", LOG_CONS, 0);

	/* get current login name and shell */
	ruid = getuid();
	username = getlogin();
	if (username == NULL || (pwd = getpwnam(username)) == NULL ||
	    pwd->pw_uid != ruid)
		pwd = getpwuid(ruid);
	if (pwd == NULL)
		errx(1, "who are you?");
	if ((username = strdup(pwd->pw_name)) == NULL)
		err(1, "can't allocate memory");
	if (asme) {
		if (pwd->pw_shell && *pwd->pw_shell) {
			shell = strncpy(shellbuf, pwd->pw_shell, sizeof(shellbuf) - 1);
			shellbuf[sizeof(shellbuf) - 1] = '\0';
		} else {
			shell = _PATH_BSHELL;
			iscsh = NO;
		}
	}

	/* get target login information, default to root */
	user = *argv ? *argv : "root";
	np = *argv ? argv : argv-1;

	if ((pwd = getpwnam(user)) == NULL)
		errx(1, "unknown login %s", user);
	if ((user = strdup(pwd->pw_name)) == NULL)
		err(1, "can't allocate memory");

	/* If the user specified a login class and we are root, use it */
	if (ruid && class)
		errx(1, "only the superuser may specify a login class");
	if (class)
		pwd->pw_class = class;
	if ((lc = login_getclass(pwd->pw_class)) == NULL)
		errx(1, "no such login class: %s",
		    class ? class : LOGIN_DEFCLASS);

#if KERBEROS
	if (ksettkfile(user))
		use_kerberos = 0;
#endif

	if (ruid) {
#ifdef KERBEROS
	    if (!use_kerberos || kerberos(username, user, pwd->pw_uid))
#endif
	    {
		/* only allow those in group zero to su to root. */
		if (pwd->pw_uid == 0 && (gr = getgrgid((gid_t)0))
		    && gr->gr_mem && *(gr->gr_mem))
			for (g = gr->gr_mem;; ++g) {
				if (!*g)
					errx(1, "you are not in the correct group to su %s.", user);
				if (strcmp(username, *g) == 0)
					break;
		}
		/* if target requires a password, verify it */
		if (*pwd->pw_passwd) {
			p = getpass("Password:");
#ifdef SKEY
			if (strcasecmp(p, "s/key") == 0) {
				if (skey_authenticate(user))
					goto badlogin;
			} else
#endif
			if (strcmp(pwd->pw_passwd, crypt(p, pwd->pw_passwd))) {
badlogin:
				fprintf(stderr, "Sorry\n");
				syslog(LOG_AUTH|LOG_WARNING,
					"BAD SU %s to %s%s", username,
					user, ontty());
				exit(1);
			}
		}
	    }
	    if (pwd->pw_expire && time(NULL) >= pwd->pw_expire) {
		    fprintf(stderr, "Sorry - account expired\n");
		    syslog(LOG_AUTH|LOG_WARNING, "BAD SU %s to %s%s", username,
			    user, ontty());
		    exit(1);
	    }
	}

	if (asme) {
		/* if asme and non-standard target shell, must be root */
		if (!chshell(pwd->pw_shell) && ruid)
			errx(1, "permission denied (shell).");
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
				errx(1, "calloc");
			if (setusercontext(lc, pwd, pwd->pw_uid, LOGIN_SETPATH))
				err(1, "unable to set user context");
			if (p) {
				if (setenv("TERM", p, 1) == -1)
					err(1, "unable to set environment");
			}

			seteuid(pwd->pw_uid);
			setegid(pwd->pw_gid);
			if (chdir(pwd->pw_dir) < 0)
				err(1, "%s", pwd->pw_dir);
			seteuid(0);
			setegid(0);	/* XXX use a saved gid instead? */
		} else if (pwd->pw_uid == 0) {
			/* XXX - this seems questionable to me */
			if (setusercontext(lc,
			    pwd, pwd->pw_uid, LOGIN_SETPATH|LOGIN_SETUMASK))
				err(1, "unable to set user context");
		}
		if (asthem || pwd->pw_uid) {
			if (setenv("LOGNAME", pwd->pw_name, 1) == -1 ||
			    setenv("USER", pwd->pw_name, 1) == -1)
				err(1, "unable to set environment");
		}
		if (setenv("HOME", pwd->pw_dir, 1) == -1 ||
		    setenv("SHELL", shell, 1) == -1)
			err(1, "unable to set environment");
	}

#ifdef KERBEROS
	if (*krbtkfile) {
		if (setenv("KRBTKFILE", krbtkfile, 1) == -1)
			err(1, "unable to set environment");
	}
#endif

	if (iscsh == YES) {
		if (fastlogin)
			*np-- = "-f";
		if (asme)
			*np-- = "-m";
	}

	if (asthem) {
		avshellbuf[0] = '-';
		strncpy(avshellbuf+1, avshell, sizeof(avshellbuf) - 2);
		avshellbuf[sizeof(avshellbuf) - 1] = '\0';
		avshell = avshellbuf;
	} else if (iscsh == YES) {
		/* csh strips the first character... */
		avshellbuf[0] = '_';
		strncpy(avshellbuf+1, avshell, sizeof(avshellbuf) - 2);
		avshellbuf[sizeof(avshellbuf) - 1] = '\0';
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
		err(1, "unable to set user context");

	execv(shell, np);
	err(1, "%s", shell);
}

int
chshell(sh)
	char *sh;
{
	register char *cp;

	while ((cp = getusershell()) != NULL)
		if (strcmp(cp, sh) == 0)
			return (1);
	return (0);
}

char *
ontty()
{
	char *p, *ttyname();
	static char buf[MAXPATHLEN + 4];

	buf[0] = 0;
	if ((p = ttyname(STDERR_FILENO)))
		snprintf(buf, sizeof(buf), " on %s", p);
	return (buf);
}

#ifdef KERBEROS
int koktologin __P((char *, char *, char *));

int
kerberos(username, user, uid)
	char *username, *user;
	int uid;
{
	KTEXT_ST ticket;
	AUTH_DAT authdata;
	struct hostent *hp;
	int kerno, fd;
	in_addr_t faddr;
	char hostname[MAXHOSTNAMELEN], savehost[MAXHOSTNAMELEN];
	char *ontty(), *krb_get_phost();

	/* Don't bother with Kerberos if there is no srvtab file */
	if ((fd = open(KEYFILE, O_RDONLY, 0)) < 0)
		return (1);
	close(fd);

	if (koktologin(username, lrealm, user) && !uid) {
		(void)fprintf(stderr, "kerberos su: not in %s's ACL.\n", user);
		return (1);
	}
	(void)krb_set_tkt_string(krbtkfile);

	/*
	 * Set real as well as effective ID to 0 for the moment,
	 * to make the kerberos library do the right thing.
	 */
	if (setuid(0) < 0) {
		warn("setuid");
		return (1);
	}

	/*
	 * Little trick here -- if we are su'ing to root,
	 * we need to get a ticket for "xxx.root", where xxx represents
	 * the name of the person su'ing.  Otherwise (non-root case),
	 * we need to get a ticket for "yyy.", where yyy represents
	 * the name of the person being su'd to, and the instance is null
	 */

	printf("%s%s@%s's ", (uid == 0 ? username : user), 
	       (uid == 0 ? ".root" : ""), lrealm);
	fflush(stdout);
	kerno = krb_get_pw_in_tkt((uid == 0 ? username : user),
		(uid == 0 ? "root" : ""), lrealm,
	    	"krbtgt", lrealm, DEFAULT_TKT_LIFE, 0);

	if (kerno != KSUCCESS) {
		if (kerno == KDC_PR_UNKNOWN) {
			warnx("kerberos principal unknown: %s.%s@%s",
				(uid == 0 ? username : user),
				(uid == 0 ? "root" : ""), lrealm);
			return (1);
		}
		warnx("unable to su: %s", krb_err_txt[kerno]);
		syslog(LOG_NOTICE|LOG_AUTH,
		    "BAD Kerberos SU: %s to %s%s: %s",
		    username, user, ontty(), krb_err_txt[kerno]);
		return (1);
	}

	/*
	 * Set the owner of the ticket file to root but bail if someone
	 * has nefariously swapped a link in place of the file.
	 */
	fd = open(krbtkfile, O_RDWR|O_NOFOLLOW, 0);
	if (fd == -1) {
		warn("unable to open ticket file");
		(void)unlink(krbtkfile);
		return (1);
	}
	if (fchown(fd, uid, -1) < 0) {
		warn("fchown");
		(void)unlink(krbtkfile);
		return (1);
	}
	close(fd);

	(void)setpriority(PRIO_PROCESS, 0, -2);

	if (gethostname(hostname, sizeof(hostname)) == -1) {
		warn("gethostname");
		dest_tkt();
		return (1);
	}

	(void)strncpy(savehost, krb_get_phost(hostname), sizeof(savehost) - 1);
	savehost[sizeof(savehost) - 1] = '\0';

	kerno = krb_mk_req(&ticket, "rcmd", savehost, lrealm, 33);

	if (kerno == KDC_PR_UNKNOWN) {
		warnx("Warning: TGT not verified.");
		syslog(LOG_NOTICE|LOG_AUTH,
		    "%s to %s%s, TGT not verified (%s); %s.%s not registered?",
		    username, user, ontty(), krb_err_txt[kerno],
		    "rcmd", savehost);
	} else if (kerno != KSUCCESS) {
		warnx("Unable to use TGT: %s", krb_err_txt[kerno]);
		syslog(LOG_NOTICE|LOG_AUTH, "failed su: %s to %s%s: %s",
		    username, user, ontty(), krb_err_txt[kerno]);
		dest_tkt();
		return (1);
	} else {
		if (!(hp = gethostbyname(hostname))) {
			warnx("can't get addr of %s", hostname);
			dest_tkt();
			return (1);
		}
		(void)memcpy((void *)&faddr, (void *)hp->h_addr, sizeof(faddr));

		if ((kerno = krb_rd_req(&ticket, "rcmd", savehost, faddr,
		    &authdata, "")) != KSUCCESS) {
			warnx("unable to verify rcmd ticket: %s",
			      krb_err_txt[kerno]);
			syslog(LOG_NOTICE|LOG_AUTH,
			    "failed su: %s to %s%s: %s", username,
			     user, ontty(), krb_err_txt[kerno]);
			dest_tkt();
			return (1);
		}
	}
	return (0);
}

int
koktologin(name, realm, toname)
	char *name, *realm, *toname;
{
	register AUTH_DAT *kdata;
	AUTH_DAT kdata_st;

	memset((void *)&kdata_st, 0, sizeof(kdata_st));
	kdata = &kdata_st;

	(void)strncpy(kdata->pname, name, sizeof(kdata->pname) - 1);
	kdata->pname[sizeof(kdata->pname) - 1] = '\0';

	(void)strncpy(kdata->pinst,
	    ((strcmp(toname, "root") == 0) ? "root" : ""), sizeof(kdata->pinst) - 1);
	kdata->pinst[sizeof(kdata->pinst) -1] = '\0';

	(void)strncpy(kdata->prealm, realm, sizeof(kdata->prealm) - 1);
	kdata->prealm[sizeof(kdata->prealm) -1] = '\0';

	return (kuserok(kdata, toname));
}

int
ksettkfile(user)
	char *user;
{
	if (krb_get_lrealm(lrealm, 1) != KSUCCESS)
		return (1);
	(void)snprintf(krbtkfile, sizeof(krbtkfile), "%s_%s_%u", TKT_ROOT,
		user, getuid());
	return (0);
}
#endif
