/*	$OpenBSD: wall.c,v 1.10 1998/12/16 01:20:14 deraadt Exp $	*/
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
static char copyright[] =
"@(#) Copyright (c) 1988, 1990, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)wall.c	8.2 (Berkeley) 11/16/93";
#endif
static char rcsid[] = "$OpenBSD: wall.c,v 1.10 1998/12/16 01:20:14 deraadt Exp $";
#endif /* not lint */

/*
 * This program is not related to David Wall, whose Stanford Ph.D. thesis
 * is entitled "Mechanisms for Broadcast and Selective Broadcast".
 */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/uio.h>

#include <paths.h>
#include <pwd.h>
#include <grp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <utmp.h>
#include <vis.h>
#include <err.h>

struct wallgroup {
	struct wallgroup *next;
	char	*name;
	gid_t	gid;
} *grouplist;

void	makemsg __P((char *));

#define	IGNOREUSER	"sleeper"

int nobanner;
int mbufsize;
char *mbuf;

/* ARGSUSED */
int
main(argc, argv)
	int argc;
	char **argv;
{
	extern int optind;
	int ch;
	struct iovec iov;
	struct utmp utmp;
	FILE *fp;
	char *p, *ttymsg();
	struct passwd *pep = getpwnam("nobody");
	char line[sizeof(utmp.ut_line) + 1];
	struct wallgroup *g;

	while ((ch = getopt(argc, argv, "ng:")) != -1)
		switch (ch) {
		case 'n':
			/* undoc option for shutdown: suppress banner */
			if (geteuid() == 0 || (pep && getuid() == pep->pw_uid))
				nobanner = 1;
			break;
		case 'g':
			g = (struct wallgroup *)malloc(sizeof *g);
			g->next = grouplist;
			g->name = optarg;
			g->gid = -1;
			grouplist = g;
			break;
		case '?':
		default:
usage:
			(void)fprintf(stderr, "usage: wall [-g group] [file]\n");
			exit(1);
		}
	argc -= optind;
	argv += optind;
	if (argc > 1)
		goto usage;

	for (g = grouplist; g; g = g->next) {
		struct group *grp;

		grp = getgrnam(g->name);
		if (grp)
			g->gid = grp->gr_gid;
	}

	makemsg(*argv);

	if (!(fp = fopen(_PATH_UTMP, "r")))
		errx(1, "cannot read %s.\n", _PATH_UTMP);
	iov.iov_base = mbuf;
	iov.iov_len = mbufsize;
	/* NOSTRICT */
	while (fread((char *)&utmp, sizeof(utmp), 1, fp) == 1) {
		if (!utmp.ut_name[0] ||
		    !strncmp(utmp.ut_name, IGNOREUSER, sizeof(utmp.ut_name)))
			continue;
		if (grouplist) {
			int ingroup = 0, ngrps, i;
			char username[16];
			struct passwd *pw;
			gid_t grps[NGROUPS_MAX];

			bzero(username, sizeof username);
			strncpy(username, utmp.ut_name, sizeof utmp.ut_name);
			pw = getpwnam(username);
			if (!pw)
				continue;
			ngrps = getgroups(pw->pw_gid, grps);
			for (g = grouplist; g && ingroup == 0; g = g->next) {
				if (g->gid == -1)
					continue;
				if (g->gid == pw->pw_gid)
					ingroup = 1;
				for (i = 0; i < ngrps && ingroup == 0; i++)
					if (g->gid == grps[i])
						ingroup = 1;
			}
			if (ingroup == 0)
				continue;
		}
		strncpy(line, utmp.ut_line, sizeof(utmp.ut_line));
		line[sizeof(utmp.ut_line)] = '\0';
		if ((p = ttymsg(&iov, 1, line, 60*5)) != NULL)
			warnx("%s\n", p);
	}
	exit(0);
}

void
makemsg(fname)
	char *fname;
{
	register int ch, cnt;
	struct tm *lt;
	struct passwd *pw;
	struct stat sbuf;
	time_t now, time();
	FILE *fp;
	int fd;
	char *p, *whom, hostname[MAXHOSTNAMELEN], lbuf[100], tmpname[64];
	char tmpbuf[5];

	snprintf(tmpname, sizeof(tmpname), "%s/wall.XXXXXX", _PATH_TMP);
	if (!(fd = mkstemp(tmpname)) || !(fp = fdopen(fd, "r+")))
		errx(1, "can't open temporary file.\n");
	(void)unlink(tmpname);

	if (!nobanner) {
		if (!(whom = getlogin()))
			whom = (pw = getpwuid(getuid())) ? pw->pw_name : "???";
		(void)gethostname(hostname, sizeof(hostname));
		(void)time(&now);
		lt = localtime(&now);

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
		    "        (%s) at %d:%02d ...", ttyname(2),
		    lt->tm_hour, lt->tm_min);
		(void)fprintf(fp, "%-79.79s\r\n", lbuf);
	}
	(void)fprintf(fp, "%79s\r\n", " ");

	if (fname && !(freopen(fname, "r", stdin)))
		errx(1, "can't read %s.\n", fname);
	while (fgets(lbuf, sizeof(lbuf), stdin))
		for (cnt = 0, p = lbuf; (ch = *p) != '\0'; ++p, ++cnt) {
			vis(tmpbuf, ch, VIS_SAFE|VIS_NOSLASH, p[1]);
			if (cnt == 79+1-strlen(tmpbuf) || ch == '\n') {
				for (; cnt < 79+1-strlen(tmpbuf); ++cnt)
					putc(' ', fp);
				putc('\r', fp);
				putc('\n', fp);
				cnt = -1;
			}
			if (ch != '\n') {
				int xx;

				for (xx = 0; tmpbuf[xx]; xx++)
					putc(tmpbuf[xx], fp);
			}
		}
	(void)fprintf(fp, "%79s\r\n", " ");
	rewind(fp);

	if (fstat(fd, &sbuf))
		errx(1, "can't stat temporary file.\n");
	mbufsize = sbuf.st_size;
	if (!(mbuf = malloc((u_int)mbufsize)))
		errx(1, "out of memory.\n");
	if (fread(mbuf, sizeof(*mbuf), mbufsize, fp) != mbufsize)
		errx(1, "can't read temporary file.\n");
	(void)close(fd);
}
