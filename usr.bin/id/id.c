/*	$OpenBSD: id.c,v 1.23 2015/05/19 16:03:19 millert Exp $	*/

/*-
 * Copyright (c) 1991, 1993
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

#include <err.h>
#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <login_cap.h>

void	current(void);
void	pretty(struct passwd *);
void	group(struct passwd *, int);
void	usage(void);
void	user(struct passwd *);
struct passwd *
	who(char *);

int
main(int argc, char *argv[])
{
	struct group *gr;
	struct passwd *pw;
	int ch, cflag, Gflag, gflag, nflag, pflag, rflag, uflag;
	uid_t uid;
	gid_t gid;
	const char *opts;

	cflag = Gflag = gflag = nflag = pflag = rflag = uflag = 0;

	if (strcmp(getprogname(), "groups") == 0) {
		Gflag = 1;
		nflag = 1;
		opts = "";
		if (argc > 2)
			usage();
	} else if (strcmp(getprogname(), "whoami") == 0) {
		uflag = 1;
		nflag = 1;
		opts = "";
		if (argc > 1)
			usage();
	} else
		opts = "cGgnpru";

	while ((ch = getopt(argc, argv, opts)) != -1)
		switch(ch) {
		case 'c':
			cflag = 1;
			break;
		case 'G':
			Gflag = 1;
			break;
		case 'g':
			gflag = 1;
			break;
		case 'n':
			nflag = 1;
			break;
		case 'p':
			pflag = 1;
			break;
		case 'r':
			rflag = 1;
			break;
		case 'u':
			uflag = 1;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	switch (cflag + Gflag + gflag + pflag + uflag) {
	case 1:
		break;
	case 0:
		if (!nflag && !rflag)
			break;
		/* FALLTHROUGH */
	default:
		usage();
	}

	if (strcmp(opts, "") != 0 && argc > 1)
		usage();

	pw = *argv ? who(*argv) : NULL;

	if (cflag) {
		if (pw == NULL)
			pw = getpwuid(getuid());
		if (pw != NULL && pw->pw_class != NULL && *pw->pw_class != '\0')
			(void)printf("%s\n", pw->pw_class);
		else
			(void)printf("%s\n", LOGIN_DEFCLASS);
		exit(0);
	}

	if (gflag) {
		gid = pw ? pw->pw_gid : rflag ? getgid() : getegid();
		if (nflag && (gr = getgrgid(gid)))
			(void)printf("%s\n", gr->gr_name);
		else
			(void)printf("%u\n", gid);
		exit(0);
	}

	if (uflag) {
		uid = pw ? pw->pw_uid : rflag ? getuid() : geteuid();
		if (nflag && (pw = getpwuid(uid)))
			(void)printf("%s\n", pw->pw_name);
		else
			(void)printf("%u\n", uid);
		exit(0);
	}

	if (Gflag) {
		group(pw, nflag);
		exit(0);
	}

	if (pflag) {
		pretty(pw);
		exit(0);
	}

	if (pw)
		user(pw);
	else
		current();
	exit(0);
}

void
pretty(struct passwd *pw)
{
	struct group *gr;
	uid_t eid, rid;
	char *login;

	if (pw) {
		(void)printf("uid\t%s\n", pw->pw_name);
		(void)printf("groups\t");
		group(pw, 1);
	} else {
		if ((login = getlogin()) == NULL)
			err(1, "getlogin");

		pw = getpwuid(rid = getuid());
		if (pw == NULL || strcmp(login, pw->pw_name))
			(void)printf("login\t%s\n", login);
		if (pw)
			(void)printf("uid\t%s\n", pw->pw_name);
		else
			(void)printf("uid\t%u\n", rid);

		if ((eid = geteuid()) != rid) {
			if ((pw = getpwuid(eid)))
				(void)printf("euid\t%s\n", pw->pw_name);
			else
				(void)printf("euid\t%u\n", eid);
		}
		if ((rid = getgid()) != (eid = getegid())) {
			if ((gr = getgrgid(rid)))
				(void)printf("rgid\t%s\n", gr->gr_name);
			else
				(void)printf("rgid\t%u\n", rid);
		}
		(void)printf("groups\t");
		group(NULL, 1);
	}
	if (pw != NULL && pw->pw_class != NULL && *pw->pw_class != '\0')
		(void)printf("class\t%s\n", pw->pw_class);
}

void
current(void)
{
	struct group *gr;
	struct passwd *pw;
	int cnt, ngroups;
	uid_t uid, euid;
	gid_t groups[NGROUPS_MAX], gid, egid, lastgid;
	char *prefix;

	uid = getuid();
	(void)printf("uid=%u", uid);
	if ((pw = getpwuid(uid)))
		(void)printf("(%s)", pw->pw_name);
	if ((euid = geteuid()) != uid) {
		(void)printf(" euid=%u", euid);
		if ((pw = getpwuid(euid)))
			(void)printf("(%s)", pw->pw_name);
	}
	gid = getgid();
	(void)printf(" gid=%u", gid);
	if ((gr = getgrgid(gid)))
		(void)printf("(%s)", gr->gr_name);
	if ((egid = getegid()) != gid) {
		(void)printf(" egid=%u", egid);
		if ((gr = getgrgid(egid)))
			(void)printf("(%s)", gr->gr_name);
	}
	if ((ngroups = getgroups(NGROUPS_MAX, groups))) {
		for (prefix = " groups=", lastgid = (gid_t)-1, cnt = 0;
		    cnt < ngroups; prefix = ", ", lastgid = gid) {
			gid = groups[cnt++];
			if (lastgid == gid)
				continue;
			(void)printf("%s%u", prefix, gid);
			if ((gr = getgrgid(gid)))
				(void)printf("(%s)", gr->gr_name);
		}
	}
	(void)printf("\n");
}

void
user(struct passwd *pw)
{
	gid_t gid, groups[NGROUPS_MAX + 1];
	int cnt, ngroups;
	uid_t uid;
	struct group *gr;
	char *prefix;

	uid = pw->pw_uid;
	(void)printf("uid=%u(%s)", uid, pw->pw_name);
	(void)printf(" gid=%u", pw->pw_gid);
	if ((gr = getgrgid(pw->pw_gid)))
		(void)printf("(%s)", gr->gr_name);
	ngroups = NGROUPS_MAX + 1;
	(void) getgrouplist(pw->pw_name, pw->pw_gid, groups, &ngroups);
	prefix = " groups=";
	for (cnt = 0; cnt < ngroups;) {
		gid = groups[cnt];
		(void)printf("%s%u", prefix, gid);
		prefix = ", ";
		if ((gr = getgrgid(gid)))
			(void)printf("(%s)", gr->gr_name);
		/* Skip same gid entries. */
		while (++cnt < ngroups && gid == groups[cnt]);
	}
	(void)printf("\n");
}

void
group(struct passwd *pw, int nflag)
{
	int cnt, ngroups;
	gid_t gid, groups[NGROUPS_MAX + 1];
	struct group *gr;
	char *prefix;

	if (pw) {
		ngroups = NGROUPS_MAX + 1;
		(void) getgrouplist(pw->pw_name, pw->pw_gid, groups, &ngroups);
	} else {
		groups[0] = getgid();
		ngroups = getgroups(NGROUPS_MAX, groups + 1) + 1;
	}
	prefix = "";
	for (cnt = 0; cnt < ngroups;) {
		gid = groups[cnt];
		if (nflag) {
			if ((gr = getgrgid(gid)))
				(void)printf("%s%s", prefix, gr->gr_name);
			else
				(void)printf("%s%u", prefix, gid);
		} else {
			(void)printf("%s%u", prefix, gid);
		}
		prefix = " ";
		/* Skip same gid entries. */
		while (++cnt < ngroups && gid == groups[cnt]);
	}
	(void)printf("\n");
}

struct passwd *
who(char *u)
{
	struct passwd *pw;
	uid_t uid;
	const char *errstr;

	/*
	 * Translate user argument into a pw pointer.  First, try to
	 * get it as specified.  If that fails, try it as a number.
	 */
	if ((pw = getpwnam(u)))
		return(pw);
	uid = strtonum(u, 0, UID_MAX, &errstr);
	if (!errstr && (pw = getpwuid(uid)))
		return(pw);
	errx(1, "%s: No such user", u);
	/* NOTREACHED */
}

void
usage(void)
{
	if (strcmp(getprogname(), "groups") == 0) {
		(void)fprintf(stderr, "usage: groups [user]\n");
	} else if (strcmp(getprogname(), "whoami") == 0) {
		(void)fprintf(stderr, "usage: whoami\n");
	} else {
		(void)fprintf(stderr, "usage: id [user]\n");
		(void)fprintf(stderr, "       id -c [user]\n");
		(void)fprintf(stderr, "       id -G [-n] [user]\n");
		(void)fprintf(stderr, "       id -g [-nr] [user]\n");
		(void)fprintf(stderr, "       id -p [user]\n");
		(void)fprintf(stderr, "       id -u [-nr] [user]\n");
	}
	exit(1);
}
