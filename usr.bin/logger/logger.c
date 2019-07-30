/*	$OpenBSD: logger.c,v 1.17 2016/03/28 18:18:52 chl Exp $	*/
/*	$NetBSD: logger.c,v 1.4 1994/12/22 06:27:00 jtc Exp $	*/

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

#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <err.h>

#define	SYSLOG_NAMES
#include <syslog.h>

int	decode(char *, CODE *);
int	pencode(char *);
void	usage(void);

/*
 * logger -- read and log utility
 *
 *	Reads from an input and arranges to write the result on the system
 *	log.
 */
int
main(int argc, char *argv[])
{
	int ch, logflags, pri;
	char *tag, buf[1024];

	tag = NULL;
	pri = LOG_NOTICE;
	logflags = 0;
	while ((ch = getopt(argc, argv, "f:ip:st:")) != -1)
		switch(ch) {
		case 'f':		/* file to log */
			if (freopen(optarg, "r", stdin) == NULL) {
				(void)fprintf(stderr, "logger: %s: %s.\n",
				    optarg, strerror(errno));
				exit(1);
			}
			break;
		case 'i':		/* log process id also */
			logflags |= LOG_PID;
			break;
		case 'p':		/* priority */
			pri = pencode(optarg);
			break;
		case 's':		/* log to standard error */
			logflags |= LOG_PERROR;
			break;
		case 't':		/* tag */
			tag = optarg;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	/* setup for logging */
	openlog(tag ? tag : getlogin(), logflags, 0);
	(void) fclose(stdout);

	if (pledge("stdio", NULL) == -1)
		err(1, "pledge");

	/* log input line if appropriate */
	if (argc > 0) {
		char *p, *endp;
		size_t len;

		for (p = buf, endp = buf + sizeof(buf) - 2; *argv;) {
			len = strlen(*argv);
			if (p + len > endp && p > buf) {
				syslog(pri, "%s", buf);
				p = buf;
			}
			if (len > sizeof(buf) - 1)
				syslog(pri, "%s", *argv++);
			else {
				if (p != buf)
					*p++ = ' ';
				bcopy(*argv++, p, len);
				*(p += len) = '\0';
			}
		}
		if (p != buf)
			syslog(pri, "%s", buf);
	} else
		while (fgets(buf, sizeof(buf), stdin) != NULL)
			syslog(pri, "%s", buf);
	exit(0);
}

/*
 *  Decode a symbolic name to a numeric value
 */
int
pencode(char *s)
{
	char *save;
	int fac, lev;

	for (save = s; *s && *s != '.'; ++s);
	if (*s) {
		*s = '\0';
		fac = decode(save, facilitynames);
		if (fac < 0) {
			(void)fprintf(stderr,
			    "logger: unknown facility name: %s.\n", save);
			exit(1);
		}
		*s++ = '.';
	}
	else {
		fac = 0;
		s = save;
	}
	lev = decode(s, prioritynames);
	if (lev < 0) {
		(void)fprintf(stderr,
		    "logger: unknown priority name: %s.\n", save);
		exit(1);
	}
	return ((lev & LOG_PRIMASK) | (fac & LOG_FACMASK));
}

int
decode(char *name, CODE *codetab)
{
	CODE *c;

	if (isdigit((unsigned char)*name)) {
		const char *errstr;
		int n = strtonum(name, 0, INT_MAX, &errstr);
		if (!errstr)
			return (n);
	}

	for (c = codetab; c->c_name; c++)
		if (!strcasecmp(name, c->c_name))
			return (c->c_val);

	return (-1);
}

void
usage(void)
{
	(void)fprintf(stderr,
	    "usage: logger [-is] [-f file] [-p pri] [-t tag] [message ...]\n");
	exit(1);
}
