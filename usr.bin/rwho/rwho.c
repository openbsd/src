/*	$OpenBSD: rwho.c,v 1.16 2009/10/27 23:59:43 deraadt Exp $	*/

/*
 * Copyright (c) 1983 The Regents of the University of California.
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

#include <sys/types.h>
#include <sys/param.h>
#include <sys/file.h>
#include <dirent.h>
#include <protocols/rwhod.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <utmp.h>
#include <vis.h>
#include <err.h>

DIR	*dirp;

struct	whod wd;
#define	NUSERS	1000
struct	myutmp {
	char	myhost[MAXHOSTNAMELEN];
	int	myidle;
	struct {
		char	out_line[9];		/* tty name + NUL */
		char	out_name[9];		/* user id + NUL */
		int32_t	out_time;		/* time on */
	} myutmp;
} myutmp[NUSERS];
int	nusers;

int utmpcmp(const void *, const void *);
__dead void usage(void);

#define	WHDRSIZE	(sizeof(wd) - sizeof(wd.wd_we))
/* 
 * this macro should be shared with ruptime.
 */
#define	down(w,now)	((now) - (w)->wd_recvtime > 11 * 60)

time_t	now;
int	aflg;

int
main(int argc, char **argv)
{
	int ch;
	struct dirent *dp;
	int cc, width;
	struct whod *w = &wd;
	struct whoent *we;
	struct myutmp *mp;
	int f, n, i;
	int nhosts = 0;

	while ((ch = getopt(argc, argv, "a")) != -1)
		switch((char)ch) {
		case 'a':
			aflg = 1;
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;
	if (argc != 0)
		usage();

	if (chdir(_PATH_RWHODIR) || (dirp = opendir(".")) == NULL)
		err(1, _PATH_RWHODIR);
	mp = myutmp;
	(void)time(&now);
	while ((dp = readdir(dirp))) {
		if (dp->d_ino == 0 || strncmp(dp->d_name, "whod.", 5))
			continue;
		f = open(dp->d_name, O_RDONLY);
		if (f < 0)
			continue;
		cc = read(f, &wd, sizeof(struct whod));
		if (cc < WHDRSIZE) {
			(void)close(f);
			continue;
		}
		nhosts++;
		if (down(w,now)) {
			(void)close(f);
			continue;
		}
		cc -= WHDRSIZE;
		we = w->wd_we;
		for (n = cc / sizeof(struct whoent); n > 0; n--) {
			if (aflg == 0 && we->we_idle >= 60*60) {
				we++;
				continue;
			}
			if (nusers >= NUSERS)
				errx(1, "too many users");
			memcpy(mp->myhost, w->wd_hostname,
			    sizeof(mp->myhost)-1);
			mp->myhost[sizeof(mp->myhost)-1] = '\0';
			mp->myidle = we->we_idle;
			/*
			 * Copy we->we_utmp by hand since the name and line
			 * variables in myutmp have room for NUL (unlike outmp).
			 */
			memcpy(mp->myutmp.out_line, we->we_utmp.out_line,
			    sizeof(mp->myutmp.out_line)-1);
			mp->myutmp.out_line[sizeof(mp->myutmp.out_line)-1] = 0;
			memcpy(mp->myutmp.out_name, we->we_utmp.out_name,
			    sizeof(mp->myutmp.out_name)-1);
			mp->myutmp.out_name[sizeof(mp->myutmp.out_name)-1] = 0;
			mp->myutmp.out_time = we->we_utmp.out_time;
			nusers++; we++; mp++;
		}
		(void)close(f);
	}
	if (nhosts == 0)
		errx(0, "no hosts in %s.", _PATH_RWHODIR);
	qsort(myutmp, nusers, sizeof(struct myutmp), utmpcmp);
	mp = myutmp;
	width = 0;
	for (i = 0; i < nusers; i++) {
		int j = strlen(mp->myhost) + 1 + strlen(mp->myutmp.out_line);
		if (j > width)
			width = j;
		mp++;
	}
	mp = myutmp;
	for (i = 0; i < nusers; i++) {
		char buf[BUFSIZ], vis_user[4 * sizeof(mp->myutmp.out_name) + 1];

		(void)snprintf(buf, sizeof(buf), "%s:%s", mp->myhost,
		    mp->myutmp.out_line);
		strnvis(vis_user, mp->myutmp.out_name, sizeof vis_user,
		    VIS_CSTYLE);
		printf("%-*.*s %-*s %.12s",
		   UT_NAMESIZE, UT_NAMESIZE, vis_user, width, buf,
		   ctime((time_t *)&mp->myutmp.out_time)+4);
		mp->myidle /= 60;
		if (mp->myidle) {
			if (aflg) {
				if (mp->myidle >= 100*60)
					mp->myidle = 100*60 - 1;
				if (mp->myidle >= 60)
					printf(" %2d", mp->myidle / 60);
				else
					printf("   ");
			} else
				printf(" ");
			printf(":%02d", mp->myidle % 60);
		}
		printf("\n");
		mp++;
	}
	exit(0);
}

int
utmpcmp(const void *v1, const void *v2)
{
	int rc;
	const struct myutmp *u1 = (struct myutmp *)v1;
	const struct myutmp *u2 = (struct myutmp *)v2;

	rc = strncmp(u1->myutmp.out_name, u2->myutmp.out_name, 8);
	if (rc)
		return (rc);
	rc = strncmp(u1->myhost, u2->myhost, 8);
	if (rc)
		return (rc);
	return (strncmp(u1->myutmp.out_line, u2->myutmp.out_line, 8));
}

void
usage(void)
{
	extern char *__progname;

	fprintf(stderr, "usage: %s [-a]\n", __progname);
	exit(1);
}
