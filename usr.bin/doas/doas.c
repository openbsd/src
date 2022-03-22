/* $OpenBSD: doas.c,v 1.97 2022/03/22 20:36:49 deraadt Exp $ */
/*
 * Copyright (c) 2015 Ted Unangst <tedu@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include <limits.h>
#include <login_cap.h>
#include <bsd_auth.h>
#include <readpassphrase.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <err.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <syslog.h>
#include <errno.h>
#include <fcntl.h>

#include "doas.h"

static void __dead
usage(void)
{
	fprintf(stderr, "usage: doas [-Lns] [-a style] [-C config] [-u user]"
	    " command [args]\n");
	exit(1);
}

static int
parseuid(const char *s, uid_t *uid)
{
	struct passwd *pw;
	const char *errstr;

	if ((pw = getpwnam(s)) != NULL) {
		*uid = pw->pw_uid;
		if (*uid == UID_MAX)
			return -1;
		return 0;
	}
	*uid = strtonum(s, 0, UID_MAX - 1, &errstr);
	if (errstr)
		return -1;
	return 0;
}

static int
uidcheck(const char *s, uid_t desired)
{
	uid_t uid;

	if (parseuid(s, &uid) != 0)
		return -1;
	if (uid != desired)
		return -1;
	return 0;
}

static int
parsegid(const char *s, gid_t *gid)
{
	struct group *gr;
	const char *errstr;

	if ((gr = getgrnam(s)) != NULL) {
		*gid = gr->gr_gid;
		if (*gid == GID_MAX)
			return -1;
		return 0;
	}
	*gid = strtonum(s, 0, GID_MAX - 1, &errstr);
	if (errstr)
		return -1;
	return 0;
}

static int
match(uid_t uid, gid_t *groups, int ngroups, uid_t target, const char *cmd,
    const char **cmdargs, struct rule *r)
{
	int i;

	if (r->ident[0] == ':') {
		gid_t rgid;
		if (parsegid(r->ident + 1, &rgid) == -1)
			return 0;
		for (i = 0; i < ngroups; i++) {
			if (rgid == groups[i])
				break;
		}
		if (i == ngroups)
			return 0;
	} else {
		if (uidcheck(r->ident, uid) != 0)
			return 0;
	}
	if (r->target && uidcheck(r->target, target) != 0)
		return 0;
	if (r->cmd) {
		if (strcmp(r->cmd, cmd))
			return 0;
		if (r->cmdargs) {
			/* if arguments were given, they should match explicitly */
			for (i = 0; r->cmdargs[i]; i++) {
				if (!cmdargs[i])
					return 0;
				if (strcmp(r->cmdargs[i], cmdargs[i]))
					return 0;
			}
			if (cmdargs[i])
				return 0;
		}
	}
	return 1;
}

static int
permit(uid_t uid, gid_t *groups, int ngroups, const struct rule **lastr,
    uid_t target, const char *cmd, const char **cmdargs)
{
	size_t i;

	*lastr = NULL;
	for (i = 0; i < nrules; i++) {
		if (match(uid, groups, ngroups, target, cmd,
		    cmdargs, rules[i]))
			*lastr = rules[i];
	}
	if (!*lastr)
		return 0;
	return (*lastr)->action == PERMIT;
}

static void
parseconfig(const char *filename, int checkperms)
{
	extern FILE *yyfp;
	extern int yyparse(void);
	struct stat sb;

	yyfp = fopen(filename, "r");
	if (!yyfp)
		err(1, checkperms ? "doas is not enabled, %s" :
		    "could not open config file %s", filename);

	if (checkperms) {
		if (fstat(fileno(yyfp), &sb) != 0)
			err(1, "fstat(\"%s\")", filename);
		if ((sb.st_mode & (S_IWGRP|S_IWOTH)) != 0)
			errx(1, "%s is writable by group or other", filename);
		if (sb.st_uid != 0)
			errx(1, "%s is not owned by root", filename);
	}

	yyparse();
	fclose(yyfp);
	if (parse_error)
		exit(1);
}

static void __dead
checkconfig(const char *confpath, int argc, char **argv,
    uid_t uid, gid_t *groups, int ngroups, uid_t target)
{
	const struct rule *rule;

	setresuid(uid, uid, uid);
	if (pledge("stdio rpath getpw", NULL) == -1)
		err(1, "pledge");
	parseconfig(confpath, 0);
	if (!argc)
		exit(0);

	if (permit(uid, groups, ngroups, &rule, target, argv[0],
	    (const char **)argv + 1)) {
		printf("permit%s\n", (rule->options & NOPASS) ? " nopass" : "");
		exit(0);
	} else {
		printf("deny\n");
		exit(1);
	}
}

static int
authuser_checkpass(char *myname, char *login_style)
{
	char *challenge = NULL, *response, rbuf[1024], cbuf[128];
	auth_session_t *as;

	if (!(as = auth_userchallenge(myname, login_style, "auth-doas",
	    &challenge))) {
		warnx("Authentication failed");
		return AUTH_FAILED;
	}
	if (!challenge) {
		char host[HOST_NAME_MAX + 1];

		if (gethostname(host, sizeof(host)))
			snprintf(host, sizeof(host), "?");
		snprintf(cbuf, sizeof(cbuf),
		    "\rdoas (%.32s@%.32s) password: ", myname, host);
		challenge = cbuf;
	}
	response = readpassphrase(challenge, rbuf, sizeof(rbuf),
	    RPP_REQUIRE_TTY);
	if (response == NULL && errno == ENOTTY) {
		syslog(LOG_AUTHPRIV | LOG_NOTICE,
		    "tty required for %s", myname);
		errx(1, "a tty is required");
	}
	if (!auth_userresponse(as, response, 0)) {
		explicit_bzero(rbuf, sizeof(rbuf));
		syslog(LOG_AUTHPRIV | LOG_NOTICE,
		    "failed auth for %s", myname);
		warnx("Authentication failed");
		return AUTH_FAILED;
	}
	explicit_bzero(rbuf, sizeof(rbuf));
	return AUTH_OK;
}

static void
authuser(char *myname, char *login_style, int persist)
{
	int i, fd = -1;

	if (persist)
		fd = open("/dev/tty", O_RDWR);
	if (fd != -1) {
		if (ioctl(fd, TIOCCHKVERAUTH) == 0)
			goto good;
	}
	for (i = 0; i < AUTH_RETRIES; i++) {
		if (authuser_checkpass(myname, login_style) == AUTH_OK)
			goto good;
	}
	exit(1);
good:
	if (fd != -1) {
		int secs = 5 * 60;
		ioctl(fd, TIOCSETVERAUTH, &secs);
		close(fd);
	}
}

int
unveilcommands(const char *ipath, const char *cmd)
{
	char *path = NULL, *p;
	int unveils = 0;

	if (strchr(cmd, '/') != NULL) {
		if (unveil(cmd, "x") != -1)
			unveils++;
		goto done;
	}

	if (!ipath) {
		errno = ENOENT;
		goto done;
	}
	path = strdup(ipath);
	if (!path) {
		errno = ENOENT;
		goto done;
	}
	for (p = path; p && *p; ) {
		char buf[PATH_MAX];
		char *cp = strsep(&p, ":");

		if (cp) {
			int r = snprintf(buf, sizeof buf, "%s/%s", cp, cmd);
			if (r >= 0 && r < sizeof buf) {
				if (unveil(buf, "x") != -1)
					unveils++;
			}
		}
	}
done:
	free(path);
	return (unveils);
}

int
main(int argc, char **argv)
{
	const char *safepath = "/bin:/sbin:/usr/bin:/usr/sbin:"
	    "/usr/local/bin:/usr/local/sbin";
	const char *confpath = NULL;
	char *shargv[] = { NULL, NULL };
	char *sh;
	const char *p;
	const char *cmd;
	char cmdline[LINE_MAX];
	char mypwbuf[_PW_BUF_LEN], targpwbuf[_PW_BUF_LEN];
	struct passwd mypwstore, targpwstore;
	struct passwd *mypw, *targpw;
	const struct rule *rule;
	uid_t uid;
	uid_t target = 0;
	gid_t groups[NGROUPS_MAX + 1];
	int ngroups;
	int i, ch, rv;
	int sflag = 0;
	int nflag = 0;
	char cwdpath[PATH_MAX];
	const char *cwd;
	char *login_style = NULL;
	char **envp;

	setprogname("doas");

	closefrom(STDERR_FILENO + 1);

	uid = getuid();

	while ((ch = getopt(argc, argv, "a:C:Lnsu:")) != -1) {
		switch (ch) {
		case 'a':
			login_style = optarg;
			break;
		case 'C':
			confpath = optarg;
			break;
		case 'L':
			i = open("/dev/tty", O_RDWR);
			if (i != -1)
				ioctl(i, TIOCCLRVERAUTH);
			exit(i == -1);
		case 'u':
			if (parseuid(optarg, &target) != 0)
				errx(1, "unknown user");
			break;
		case 'n':
			nflag = 1;
			break;
		case 's':
			sflag = 1;
			break;
		default:
			usage();
			break;
		}
	}
	argv += optind;
	argc -= optind;

	if (confpath) {
		if (sflag)
			usage();
	} else if ((!sflag && !argc) || (sflag && argc))
		usage();

	rv = getpwuid_r(uid, &mypwstore, mypwbuf, sizeof(mypwbuf), &mypw);
	if (rv != 0)
		err(1, "getpwuid_r failed");
	if (mypw == NULL)
		errx(1, "no passwd entry for self");
	ngroups = getgroups(NGROUPS_MAX, groups);
	if (ngroups == -1)
		err(1, "can't get groups");
	groups[ngroups++] = getgid();

	if (sflag) {
		sh = getenv("SHELL");
		if (sh == NULL || *sh == '\0') {
			shargv[0] = mypw->pw_shell;
		} else
			shargv[0] = sh;
		argv = shargv;
		argc = 1;
	}

	if (confpath) {
		if (pledge("stdio rpath getpw id", NULL) == -1)
			err(1, "pledge");
		checkconfig(confpath, argc, argv, uid, groups, ngroups,
		    target);
		exit(1);	/* fail safe */
	}

	if (geteuid())
		errx(1, "not installed setuid");

	parseconfig("/etc/doas.conf", 1);

	/* cmdline is used only for logging, no need to abort on truncate */
	(void)strlcpy(cmdline, argv[0], sizeof(cmdline));
	for (i = 1; i < argc; i++) {
		if (strlcat(cmdline, " ", sizeof(cmdline)) >= sizeof(cmdline))
			break;
		if (strlcat(cmdline, argv[i], sizeof(cmdline)) >= sizeof(cmdline))
			break;
	}

	cmd = argv[0];
	if (!permit(uid, groups, ngroups, &rule, target, cmd,
	    (const char **)argv + 1)) {
		syslog(LOG_AUTHPRIV | LOG_NOTICE,
		    "command not permitted for %s: %s", mypw->pw_name, cmdline);
		errc(1, EPERM, NULL);
	}

	if (!(rule->options & NOPASS)) {
		if (nflag)
			errx(1, "Authentication required");

		authuser(mypw->pw_name, login_style, rule->options & PERSIST);
	}

	if ((p = getenv("PATH")) != NULL)
		formerpath = strdup(p);
	if (formerpath == NULL)
		formerpath = "";

	if (unveil(_PATH_LOGIN_CONF, "r") == -1)
		err(1, "unveil %s", _PATH_LOGIN_CONF);
	if (unveil(_PATH_LOGIN_CONF ".db", "r") == -1)
		err(1, "unveil %s.db", _PATH_LOGIN_CONF);
	if (unveil(_PATH_LOGIN_CONF_D, "r") == -1)
		err(1, "unveil %s", _PATH_LOGIN_CONF_D);
	if (rule->cmd) {
		if (setenv("PATH", safepath, 1) == -1)
			err(1, "failed to set PATH '%s'", safepath);
	}
	if (unveilcommands(getenv("PATH"), cmd) == 0)
		goto fail;

	if (pledge("stdio rpath getpw exec id", NULL) == -1)
		err(1, "pledge");

	rv = getpwuid_r(target, &targpwstore, targpwbuf, sizeof(targpwbuf), &targpw);
	if (rv != 0)
		err(1, "getpwuid_r failed");
	if (targpw == NULL)
		errx(1, "no passwd entry for target");

	if (setusercontext(NULL, targpw, target, LOGIN_SETGROUP |
	    LOGIN_SETPATH |
	    LOGIN_SETPRIORITY | LOGIN_SETRESOURCES | LOGIN_SETUMASK |
	    LOGIN_SETUSER | LOGIN_SETENV | LOGIN_SETRTABLE) != 0)
		errx(1, "failed to set user context for target");

	if (pledge("stdio rpath exec", NULL) == -1)
		err(1, "pledge");

	if (getcwd(cwdpath, sizeof(cwdpath)) == NULL)
		cwd = "(failed)";
	else
		cwd = cwdpath;

	if (pledge("stdio exec", NULL) == -1)
		err(1, "pledge");

	if (!(rule->options & NOLOG)) {
		syslog(LOG_AUTHPRIV | LOG_INFO,
		    "%s ran command %s as %s from %s",
		    mypw->pw_name, cmdline, targpw->pw_name, cwd);
	}

	envp = prepenv(rule, mypw, targpw);

	/* setusercontext set path for the next process, so reset it for us */
	if (rule->cmd) {
		if (setenv("PATH", safepath, 1) == -1)
			err(1, "failed to set PATH '%s'", safepath);
	} else {
		if (setenv("PATH", formerpath, 1) == -1)
			err(1, "failed to set PATH '%s'", formerpath);
	}
	execvpe(cmd, argv, envp);
fail:
	if (errno == ENOENT)
		errx(1, "%s: command not found", cmd);
	err(1, "%s", cmd);
}
