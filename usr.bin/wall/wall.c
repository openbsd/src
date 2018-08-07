/*	$OpenBSD: wall.c,v 1.33 2018/08/07 18:28:05 deraadt Exp $	*/
/*	$NetBSD: wall.c,v 1.6 1994/11/17 07:17:58 jtc Exp $	*/

/*
 * Copyright (c) 1988, 1990, 1993
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

/*
 * This program is not related to David Wall, whose Stanford Ph.D. thesis
 * is entitled "Mechanisms for Broadcast and Selective Broadcast".
 */

#include <sys/queue.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/uio.h>

#include <ctype.h>
#include <err.h>
#include <grp.h>
#include <limits.h>
#include <paths.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <utmp.h>

struct wallgroup {
	gid_t	gid;
	char	*name;
	char	**mem;
	struct wallgroup *next;
} *grouplist;

struct utmptty {
	char	*tty;
	SLIST_ENTRY(utmptty) next;
};

void	makemsg(char *);
void	addgroup(struct group *, char *);
char   *ttymsg(struct iovec *, int, char *, int);
__dead	void usage(void);
static int isu8cont(unsigned char);

int nobanner;
int mbufsize;
char *mbuf;

/* ARGSUSED */
int
main(int argc, char **argv)
{
	FILE *fp;
	int ch, ingroup;
	struct iovec iov;
	struct utmp utmp;
	char *p, **mem;
	char line[sizeof(utmp.ut_line) + 1];
	char username[sizeof(utmp.ut_name) + 1];
	struct passwd *pw;
	struct group *grp;
	struct wallgroup *g;
	struct utmptty *un;
	SLIST_HEAD(,utmptty) utmphead;
	SLIST_INIT(&utmphead);

	while ((ch = getopt(argc, argv, "ng:")) != -1)
		switch (ch) {
		case 'n':
			/* undoc option for shutdown: suppress banner */
			pw = getpwnam("nobody");
			if (geteuid() == 0 || (pw && getuid() == pw->pw_uid))
				nobanner = 1;
			break;
		case 'g':
			if ((grp = getgrnam(optarg)) == NULL)
				errx(1, "unknown group `%s'", optarg);
			addgroup(grp, optarg);
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;
	if (argc > 1)
		usage();

	makemsg(*argv);

	if (unveil(_PATH_UTMP, "r") == -1)
		err(1, "unveil");
	if (unveil(_PATH_DEV, "w") == -1)
		err(1, "unveil");
	if (pledge("stdio rpath wpath getpw proc", NULL) == -1)
		err(1, "pledge");

	if (!(fp = fopen(_PATH_UTMP, "r")))
		err(1, "cannot read %s", _PATH_UTMP);
	iov.iov_base = mbuf;
	iov.iov_len = mbufsize;

	while (fread(&utmp, sizeof(utmp), 1, fp) == 1) {
		if (!utmp.ut_name[0])
			continue;
		if (grouplist) {
			ingroup = 0;
			strncpy(username, utmp.ut_name, sizeof(utmp.ut_name));
			username[sizeof(utmp.ut_name)] = '\0';
			pw = getpwnam(username);
			if (!pw)
				continue;
			for (g = grouplist; g && ingroup == 0; g = g->next) {
				if (g->gid == pw->pw_gid)
					ingroup = 1;
				for (mem = g->mem; *mem && ingroup == 0; mem++)
					if (strcmp(username, *mem) == 0)
						ingroup = 1;
			}
			if (ingroup == 0)
				continue;
		}
		strncpy(line, utmp.ut_line, sizeof(utmp.ut_line));
		line[sizeof(utmp.ut_line)] = '\0';
		un = malloc(sizeof(struct utmptty));
		if (un == NULL)
			err(1, "malloc");
		un->tty = strndup(line, sizeof(utmp.ut_line));
		if (un->tty == NULL)
			err(1, "strndup");
		SLIST_INSERT_HEAD(&utmphead, un, next);
	}
	fclose(fp);

	if (pledge("stdio rpath wpath proc", NULL) == -1)
		err(1, "pledge");

	SLIST_FOREACH(un, &utmphead, next) {
		if ((p = ttymsg(&iov, 1, un->tty, 60*5)) != NULL)
			warnx("%s", p);
	}

	exit(0);
}

void
makemsg(char *fname)
{
	int cnt;
	struct tm *lt;
	struct passwd *pw;
	struct stat sbuf;
	time_t now;
	FILE *fp;
	int fd;
	char *p, *whom, hostname[HOST_NAME_MAX+1], lbuf[100], tmpname[PATH_MAX];
	char *ttynam;
	unsigned char ch;

	snprintf(tmpname, sizeof(tmpname), "%s/wall.XXXXXXXXXX", _PATH_TMP);
	if ((fd = mkstemp(tmpname)) >= 0) {
		(void)unlink(tmpname);
		fp = fdopen(fd, "r+");
	}
	if (fd == -1 || fp == NULL)
		err(1, "can't open temporary file");

	if (!nobanner) {
		if (!(whom = getlogin()))
			whom = (pw = getpwuid(getuid())) ? pw->pw_name : "???";
		(void)gethostname(hostname, sizeof(hostname));
		(void)time(&now);
		lt = localtime(&now);
		if ((ttynam = ttyname(STDERR_FILENO)) == NULL)
			ttynam = "(not a tty)";

		/*
		 * all this stuff is to blank out a square for the message;
		 * we wrap message lines at column 79, not 80, because some
		 * terminals wrap after 79, some do not, and we can't tell.
		 * Which means that we may leave a non-blank character
		 * in column 80, but that can't be helped.
		 */
		(void)fprintf(fp, "\r%79s\r\n", " ");
		(void)snprintf(lbuf, sizeof lbuf,
		    "Broadcast Message from %s@%s", whom, hostname);
		(void)fprintf(fp, "%-79.79s\007\007\r\n", lbuf);
		(void)snprintf(lbuf, sizeof lbuf,
		    "        (%s) at %d:%02d ...", ttynam,
		    lt->tm_hour, lt->tm_min);
		(void)fprintf(fp, "%-79.79s\r\n", lbuf);
	}
	(void)fprintf(fp, "%79s\r\n", " ");

	if (fname) {
		gid_t egid = getegid();

		setegid(getgid());
		if (freopen(fname, "r", stdin) == NULL)
			err(1, "can't read %s", fname);
		setegid(egid);
	}
	while (fgets(lbuf, sizeof(lbuf), stdin))
		for (cnt = 0, p = lbuf; (ch = *p) != '\0'; ++p, ++cnt) {
			if (cnt == 79 || ch == '\n') {
				for (; cnt < 79; ++cnt)
					putc(' ', fp);
				putc('\r', fp);
				putc('\n', fp);
				cnt = -1;
			} else if (!isu8cont(ch))
				putc(isprint(ch) || isspace(ch) || ch == '\a' ?
				    ch : '?', fp);
		}
	(void)fprintf(fp, "%79s\r\n", " ");
	rewind(fp);

	if (fstat(fd, &sbuf))
		err(1, "can't stat temporary file");
	mbufsize = sbuf.st_size;
	mbuf = malloc((u_int)mbufsize);
	if (mbuf == NULL)
		err(1, NULL);
	if (fread(mbuf, sizeof(*mbuf), mbufsize, fp) != mbufsize)
		err(1, "can't read temporary file");
	(void)close(fd);
}

void
addgroup(struct group *grp, char *name)
{
	int i;
	struct wallgroup *g;

	for (i = 0; grp->gr_mem[i]; i++)
		;

	g = malloc(sizeof *g);
	if (g == NULL)
		err(1, NULL);
	g->gid = grp->gr_gid;
	g->name = name;
	g->mem = calloc(i + 1, sizeof(char *));
	if (g->mem == NULL)
		err(1, NULL);
	for (i = 0; grp->gr_mem[i] != NULL; i++) {
		g->mem[i] = strdup(grp->gr_mem[i]);
		if (g->mem[i] == NULL)
			err(1, NULL);
	}
	g->mem[i] = NULL;
	g->next = grouplist;
	grouplist = g;
}

void
usage(void)
{
	extern char *__progname;

	(void)fprintf(stderr, "usage: %s [-g group] [file]\n", __progname);
	exit(1);
}

static int
isu8cont(unsigned char c)
{
	return (c & (0x80 | 0x40)) == 0x80;
}
