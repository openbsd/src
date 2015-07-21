/* $OpenBSD: doas.c,v 1.18 2015/07/21 17:49:33 jmc Exp $ */
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

#include <limits.h>
#include <login_cap.h>
#include <bsd_auth.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <err.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <syslog.h>
#include <errno.h>

#include "doas.h"

static void __dead
usage(void)
{
	fprintf(stderr, "usage: doas [-s] [-C config] [-u user] command [args]\n");
	exit(1);
}

size_t
arraylen(const char **arr)
{
	size_t cnt = 0;

	while (*arr) {
		cnt++;
		arr++;
	}
	return cnt;
}

static int
parseuid(const char *s, uid_t *uid)
{
	struct passwd *pw;
	const char *errstr;

	if ((pw = getpwnam(s)) != NULL) {
		*uid = pw->pw_uid;
		return 0;
	}
	*uid = strtonum(s, 0, UID_MAX, &errstr);
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

static gid_t
strtogid(const char *s)
{
	struct group *gr;
	const char *errstr;
	gid_t gid;

	if ((gr = getgrnam(s)) != NULL)
		return gr->gr_gid;
	gid = strtonum(s, 0, GID_MAX, &errstr);
	if (errstr)
		return -1;
	return gid;
}

static int
match(uid_t uid, gid_t *groups, int ngroups, uid_t target, const char *cmd,
    const char **cmdargs, struct rule *r)
{
	int i;

	if (r->ident[0] == ':') {
		gid_t rgid = strtogid(r->ident + 1);
		if (rgid == -1)
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
permit(uid_t uid, gid_t *groups, int ngroups, struct rule **lastr,
    uid_t target, const char *cmd, const char **cmdargs)
{
	int i;

	*lastr = NULL;
	for (i = 0; i < nrules; i++) {
		if (match(uid, groups, ngroups, target, cmd, cmdargs, rules[i]))
			*lastr = rules[i];
	}
	if (!*lastr)
		return 0;
	return (*lastr)->action == PERMIT;
}

static void
parseconfig(const char *filename)
{
	extern FILE *yyfp;
	extern int yyparse(void);
	struct stat sb;

	yyfp = fopen(filename, "r");
	if (!yyfp) {
		fprintf(stderr, "doas is not enabled.\n");
		exit(1);
	}

	if (fstat(fileno(yyfp), &sb) != 0)
		err(1, "fstat(\"%s\")", filename);
	if ((sb.st_mode & (S_IWGRP|S_IWOTH)) != 0)
		errx(1, "%s is writable by group or other", filename);
	if (sb.st_uid != 0)
		errx(1, "%s is not owned by root", filename);

	yyparse();
	fclose(yyfp);
}

static int
copyenvhelper(const char **oldenvp, const char **safeset, int nsafe,
    char **envp, int ei)
{
	int i;

	for (i = 0; i < nsafe; i++) {
		const char **oe = oldenvp;
		while (*oe) {
			size_t len = strlen(safeset[i]);
			if (strncmp(*oe, safeset[i], len) == 0 &&
			    (*oe)[len] == '=') {
				if (!(envp[ei++] = strdup(*oe)))
					err(1, "strdup");
				break;
			}
			oe++;
		}
	}
	return ei;
}

static char **
copyenv(const char **oldenvp, struct rule *rule)
{
	const char *safeset[] = {
		"DISPLAY", "HOME", "LOGNAME", "MAIL",
		"PATH", "TERM", "USER", "USERNAME",
		NULL
	};
	const char *badset[] = {
		"ENV",
		NULL
	};
	char **envp;
	const char **extra;
	int ei;
	int nsafe, nbad;
	int nextras = 0;
	
	nbad = arraylen(badset);
	if ((rule->options & KEEPENV) && !rule->envlist) {
		size_t i, ii;
		size_t oldlen = arraylen(oldenvp);
		envp = reallocarray(NULL, oldlen + 1, sizeof(char *));
		if (!envp)
			err(1, "reallocarray");
		for (ii = i = 0; i < oldlen; i++) {
			size_t j;
			for (j = 0; j < nbad; j++) {
				size_t len = strlen(badset[j]);
				if (strncmp(oldenvp[i], badset[j], len) == 0 &&
			    	    oldenvp[i][len] == '=') {
					break;
				}
			}
			if (j == nbad) {
				if (!(envp[ii] = strdup(oldenvp[i])))
					err(1, "strdup");
				ii++;
			}
		}
		envp[ii] = NULL;
		return envp;
	}

	nsafe = arraylen(safeset);
	if ((extra = rule->envlist)) {
		size_t i;
		nextras = arraylen(extra);
		for (i = 0; i < nsafe; i++) {
			size_t j;
			for (j = 0; j < nextras; j++) {
				if (strcmp(extra[j], safeset[i]) == 0) {
					extra[j--] = extra[nextras--];
					extra[nextras] = NULL;
				}
			}
		}
	}

	envp = reallocarray(NULL, nsafe + nextras + 1, sizeof(char *));
	if (!envp)
		err(1, "can't allocate new environment");

	ei = 0;
	ei = copyenvhelper(oldenvp, safeset, nsafe, envp, ei);
	ei = copyenvhelper(oldenvp, rule->envlist, nextras, envp, ei);
	envp[ei] = NULL;

	return envp;
}

static void __dead
fail(void)
{
	fprintf(stderr, "Permission denied\n");
	exit(1);
}

int
main(int argc, char **argv, char **envp)
{
	const char *safepath = "/bin:/sbin:/usr/bin:/usr/sbin:"
	    "/usr/local/bin:/usr/local/sbin";
	char *shargv[] = { NULL, NULL };
	char *sh;
	const char *cmd;
	char cmdline[LINE_MAX];
	char myname[_PW_NAME_LEN + 1];
	struct passwd *pw;
	struct rule *rule;
	uid_t uid;
	uid_t target = 0;
	gid_t groups[NGROUPS_MAX + 1];
	int ngroups;
	int i, ch;
	int sflag = 0;

	while ((ch = getopt(argc, argv, "C:su:")) != -1) {
		switch (ch) {
		case 'C':
			target = getuid();
			setresuid(target, target, target);
			parseconfig(optarg);
			exit(0);
		case 'u':
			if (parseuid(optarg, &target) != 0)
				errx(1, "unknown user");
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

	if ((!sflag && !argc) || (sflag && argc))
		usage();

	parseconfig("/etc/doas.conf");

	uid = getuid();
	pw = getpwuid(uid);
	if (!pw)
		err(1, "getpwuid failed");
	if (strlcpy(myname, pw->pw_name, sizeof(myname)) >= sizeof(myname))
		errx(1, "pw_name too long");
	ngroups = getgroups(NGROUPS_MAX, groups);
	if (ngroups == -1)
		err(1, "can't get groups");
	groups[ngroups++] = getgid();

	if (sflag) {
		sh = getenv("SHELL");
		if (sh == NULL || *sh == '\0')
			shargv[0] = pw->pw_shell;
		else
			shargv[0] = sh;
		argv = shargv;
		argc = 1;
	}

	cmd = argv[0];
	if (strlcpy(cmdline, argv[0], sizeof(cmdline)) >= sizeof(cmdline))
		errx(1, "command line too long");
	for (i = 1; i < argc; i++) {
		if (strlcat(cmdline, " ", sizeof(cmdline)) >= sizeof(cmdline))
			errx(1, "command line too long");
		if (strlcat(cmdline, argv[i], sizeof(cmdline)) >= sizeof(cmdline))
			errx(1, "command line too long");
	}

	if (!permit(uid, groups, ngroups, &rule, target, cmd,
	    (const char**)argv + 1)) {
		syslog(LOG_AUTHPRIV | LOG_NOTICE,
		    "failed command for %s: %s", myname, cmdline);
		fail();
	}

	if (!(rule->options & NOPASS)) {
		if (!auth_userokay(myname, NULL, NULL, NULL)) {
			syslog(LOG_AUTHPRIV | LOG_NOTICE,
			    "failed password for %s", myname);
			fail();
		}
	}
	envp = copyenv((const char **)envp, rule);

	pw = getpwuid(target);
	if (!pw)
		errx(1, "no passwd entry for target");
	if (setusercontext(NULL, pw, target, LOGIN_SETGROUP |
	    LOGIN_SETPRIORITY | LOGIN_SETRESOURCES | LOGIN_SETUMASK |
	    LOGIN_SETUSER) != 0)
		errx(1, "failed to set user context for target");

	syslog(LOG_AUTHPRIV | LOG_INFO, "%s ran command as %s: %s",
	    myname, pw->pw_name, cmdline);
	if (setenv("PATH", safepath, 1) == -1)
		err(1, "failed to set PATH '%s'", safepath);
	execvpe(cmd, argv, envp);
	if (errno == ENOENT)
		errx(1, "%s: command not found", cmd);
	err(1, "%s", cmd);
}
