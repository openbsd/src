/*	$OpenBSD: fingerd.c,v 1.29 2002/09/06 19:43:54 deraadt Exp $	*/

/*
 * Copyright (c) 1983, 1993
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
"@(#) Copyright (c) 1983, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "from: @(#)fingerd.c	8.1 (Berkeley) 6/4/93";
#else
static char rcsid[] = "$OpenBSD: fingerd.c,v 1.29 2002/09/06 19:43:54 deraadt Exp $";
#endif
#endif /* not lint */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#include <unistd.h>
#include <syslog.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "pathnames.h"

void err(const char *, ...);
void usage(void);

void
usage(void)
{
	syslog(LOG_ERR,
	    "usage: fingerd [-slumMpS] [-P filename]");
	exit(2);
}


int
main(int argc, char *argv[])
{
	FILE *fp;
	int ch, ac = 2;
	int p[2], logging, secure, user_required, short_list;
#define	ENTRIES	50
	char **comp, *prog;
	char **ap, *av[ENTRIES + 1], line[8192], *lp, *hname;
	char hostbuf[MAXHOSTNAMELEN];

	prog = _PATH_FINGER;
	logging = secure = user_required = short_list = 0;
	openlog("fingerd", LOG_PID, LOG_DAEMON);
	opterr = 0;
	while ((ch = getopt(argc, argv, "sluSmMpP:")) != -1)
		switch (ch) {
		case 'l':
			logging = 1;
			break;
		case 'P':
			prog = optarg;
			break;
		case 's':
			secure = 1;
			break;
		case 'u':
			user_required = 1;
			break;
		case 'S':
			if (ac < ENTRIES) {
				short_list = 1;
				av[ac++] = "-s";
			}
			break;
		case 'm':
			if (ac < ENTRIES)
				av[ac++] = "-m";
			break;
		case 'M':
			if (ac < ENTRIES)
				av[ac++] = "-M";
			break;
		case 'p':
			if (ac < ENTRIES)
				av[ac++] = "-p";
			break;
		default:
			usage();
		}

	if (logging) {
		struct sockaddr_storage ss;
		struct sockaddr *sa;
		socklen_t sval;

		sval = sizeof(ss);
		if (getpeername(0, (struct sockaddr *)&ss, &sval) < 0) {
			/* err("getpeername: %s", strerror(errno)); */
			exit(1);
		}
		sa = (struct sockaddr *)&ss;
		if (getnameinfo(sa, sa->sa_len, hostbuf, sizeof(hostbuf),
		    NULL, 0, 0) != 0) {
			strlcpy(hostbuf, "?", sizeof(hostbuf));
		}
		hname = hostbuf;
	}

	if (fgets(line, sizeof(line), stdin) == NULL) {
		if (logging)
			syslog(LOG_NOTICE, "query from %s: %s", hname,
			    feof(stdin) ? "EOF" : strerror(errno));
		exit(1);
	}

	if (logging)
		syslog(LOG_NOTICE, "query from %s: `%.*s'", hname,
		    (int)strcspn(line, "\r\n"), line);

	/*
	 * Note: we assume that finger(1) will treat "--" as end of
	 * command args (ie: that it uses getopt(3)).
	 */
	av[ac++] = "--";
	comp = &av[1];
	for (lp = line, ap = &av[ac]; ac < ENTRIES;) {
		if ((*ap = strtok(lp, " \t\r\n")) == NULL)
			break;
		lp = NULL;
		if (secure && strchr(*ap, '@')) {
			(void) puts("forwarding service denied\r");
			exit(1);
		}

		ch = strlen(*ap);
		while ((*ap)[ch-1] == '@')
			(*ap)[--ch] = '\0';
		if (**ap == '\0')
			continue;

		/* RFC1196: "/[Ww]" == "-l" */
		if ((*ap)[0] == '/' && ((*ap)[1] == 'W' || (*ap)[1] == 'w')) {
			if (!short_list) {
				av[1] = "-l";
				comp = &av[0];
			}
		} else {
			ap++;
			ac++;
		}
	}
	av[ENTRIES - 1] = NULL;

	if ((lp = strrchr(prog, '/')))
		*comp = ++lp;
	else
		*comp = prog;

	if (user_required) {
		for (ap = comp + 1; strcmp("--", *(ap++)); )
			;
		if (*ap == NULL) {
			(void) puts("must provide username\r");
			exit(1);
		}
	}

	if (pipe(p) < 0)
		err("pipe: %s", strerror(errno));

	switch (vfork()) {
	case 0:
		(void) close(p[0]);
		if (p[1] != 1) {
			(void) dup2(p[1], 1);
			(void) close(p[1]);
		}
		execv(prog, comp);
		err("execv: %s: %s", prog, strerror(errno));
		_exit(1);
	case -1:
		err("fork: %s", strerror(errno));
	}
	(void) close(p[1]);
	if (!(fp = fdopen(p[0], "r")))
		err("fdopen: %s", strerror(errno));
	while ((ch = getc(fp)) != EOF) {
		if (ch == '\n')
			putchar('\r');
		putchar(ch);
	}
	exit(0);
}

void
err(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	(void) vsyslog(LOG_ERR, fmt, ap);
	va_end(ap);
	exit(1);
	/* NOTREACHED */
}
