/*	$OpenBSD: vipw.c,v 1.11 2003/01/24 19:16:43 deraadt Exp $	 */

/*
 * Copyright (c) 1987, 1993, 1994
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
"@(#) Copyright (c) 1987, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)vipw.c	8.3 (Berkeley) 4/2/94";
#endif /* not lint */

#include <sys/time.h>
#include <sys/stat.h>

#include <err.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <util.h>

void	copyfile(int, int);
void	usage(void);

int
main(int argc, char *argv[])
{
	int pfd, tfd;
	struct stat begin, end;
	int ch;

	while ((ch = getopt(argc, argv, "")) != -1) {
		switch (ch) {
		case '?':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 0)
		usage();

	pw_init();
	tfd = pw_lock(0);
	if (tfd < 0)
		errx(1, "the passwd file is busy or you cannot lock.");
	pfd = open(_PATH_MASTERPASSWD, O_RDONLY, 0);
	if (pfd < 0)
		pw_error(_PATH_MASTERPASSWD, 1, 1);
	copyfile(pfd, tfd);
	(void)close(tfd);

	for (;;) {
		if (stat(_PATH_MASTERPASSWD_LOCK, &begin))
			pw_error(_PATH_MASTERPASSWD_LOCK, 1, 1);
		pw_edit(0, NULL);
		if (stat(_PATH_MASTERPASSWD_LOCK, &end))
			pw_error(_PATH_MASTERPASSWD_LOCK, 1, 1);
		if (!timespeccmp(&begin.st_mtimespec, &end.st_mtimespec, -) &&
		    begin.st_size == end.st_size) {
			warnx("no changes made");
			pw_error((char *)NULL, 0, 0);
		}
		if (pw_mkdb(NULL, 0) == 0)
			break;
		pw_prompt();
	}
	exit(0);
}

void
copyfile(int from, int to)
{
	int nr, nw, off;
	char buf[8*1024];
	struct stat sb;
	struct timeval tv[2];

	if (fstat(from, &sb) == -1)
		pw_error(_PATH_MASTERPASSWD, 1, 1);
	while ((nr = read(from, buf, sizeof(buf))) > 0)
		for (off = 0; off < nr; nr -= nw, off += nw)
			if ((nw = write(to, buf + off, nr)) < 0)
				pw_error(_PATH_MASTERPASSWD_LOCK, 1, 1);
	if (nr < 0)
		pw_error(_PATH_MASTERPASSWD, 1, 1);

	TIMESPEC_TO_TIMEVAL(&tv[0], &sb.st_atimespec);
	TIMESPEC_TO_TIMEVAL(&tv[1], &sb.st_mtimespec);
	(void)futimes(to, tv);
}

void
usage(void)
{
	extern char *__progname;

	(void)fprintf(stderr, "usage: %s\n", __progname);
	exit(1);
}
