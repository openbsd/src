/* $OpenBSD: doas.c,v 1.31 2015/07/28 21:36:03 deraadt Exp $ */
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
	fprintf(stderr, "usage: doas [-ns] [-C config] [-u user] command [args]\n");
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
	if (!yyfp) {
		if (checkperms)
			fprintf(stderr, "doas is not enabled.\n");
		else
			warn("could not open config file");
		exit(1);
	}

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
	if (parse_errors)
		exit(1);
}

/*
 * Copy to envp environment variables from oldenvp which names are
 * in safeset.
 */
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

	/* if there was no envvar whitelist, pass all except badset ones */
	nbad = arraylen(badset);
	if ((rule->options & KEEPENV) && !rule->envlist) {
		size_t iold, inew;
		size_t oldlen = arraylen(oldenvp);
		envp = reallocarray(NULL, oldlen + 1, sizeof(char *));
		if (!envp)
			err(1, "reallocarray");
		for (inew = iold = 0; iold < oldlen; iold++) {
			size_t ibad;
			for (ibad = 0; ibad < nbad; ibad++) {
				size_t len = strlen(badset[ibad]);
				if (strncmp(oldenvp[iold], badset[ibad], len) == 0 &&
				    oldenvp[iold][len] == '=') {
					break;
				}
			}
			if (ibad == nbad) {
				if (!(envp[inew] = strdup(oldenvp[iold])))
					err(1, "strdup");
				inew++;
			}
		}
		envp[inew] = NULL;
		return envp;
	}

	nsafe = arraylen(safeset);
	if ((extra = rule->envlist)) {
		size_t isafe;
		nextras = arraylen(extra);
		for (isafe = 0; isafe < nsafe; isafe++) {
			size_t iextras;
			for (iextras = 0; iextras < nextras; iextras++) {
				if (strcmp(extra[iextras], safeset[isafe]) == 0) {
					nextras--;
					extra[iextras] = extra[nextras];
					extra[nextras] = NULL;
					iextras--;
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

static void __dead
checkconfig(const char *confpath, int argc, char **argv,
    uid_t uid, gid_t *groups, int ngroups, uid_t target)
{
	struct rule *rule;

	setresuid(uid, uid, uid);
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

int
main(int argc, char **argv, char **envp)
{
	const char *safepath = "/bin:/sbin:/usr/bin:/usr/sbin:"
	    "/usr/local/bin:/usr/local/sbin";
	const char *confpath = NULL;
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
	int nflag = 0;

	uid = getuid();
	while ((ch = getopt(argc, argv, "C:nsu:")) != -1) {
		switch (ch) {
		case 'C':
			confpath = optarg;
			break;
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

	if (confpath) {
		checkconfig(confpath, argc, argv, uid, groups, ngroups,
		    target);
		exit(1);	/* fail safe */
	}

	parseconfig("/etc/doas.conf", 1);

	/* cmdline is used only for logging, no need to abort on truncate */
	(void) strlcpy(cmdline, argv[0], sizeof(cmdline));
	for (i = 1; i < argc; i++) {
		if (strlcat(cmdline, " ", sizeof(cmdline)) >= sizeof(cmdline))
			break;
		if (strlcat(cmdline, argv[i], sizeof(cmdline)) >= sizeof(cmdline))
			break;
	}

	cmd = argv[0];
	if (!permit(uid, groups, ngroups, &rule, target, cmd,
	    (const char**)argv + 1)) {
		syslog(LOG_AUTHPRIV | LOG_NOTICE,
		    "failed command for %s: %s", myname, cmdline);
		fail();
	}

	if (!(rule->options & NOPASS)) {
		if (nflag)
			errx(1, "Authorization required");
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
