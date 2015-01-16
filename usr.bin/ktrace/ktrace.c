/*	$OpenBSD: ktrace.c,v 1.31 2015/01/16 06:40:09 deraadt Exp $	*/
/*	$NetBSD: ktrace.c,v 1.4 1995/08/31 23:01:44 jtc Exp $	*/

/*-
 * Copyright (c) 1988, 1993
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

#include <sys/param.h>	/* MAXCOMLEN */
#include <sys/signal.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/ktrace.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "ktrace.h"
#include "extern.h"

extern char *__progname;

static int rpid(const char *);
static void no_ktrace(int);
static void usage(void);

int	is_ltrace;

int
main(int argc, char *argv[])
{
	enum { NOTSET, CLEAR, CLEARALL } clear;
	int append, ch, fd, inherit, ops, pidset, trpoints;
	pid_t pid;
	char *tracefile, *tracespec;
	mode_t omask;
	struct stat sb;

	is_ltrace = strcmp(__progname, "ltrace") == 0;

	clear = NOTSET;
	append = ops = pidset = inherit = pid = 0;
	trpoints = is_ltrace ? KTRFAC_USER : DEF_POINTS;
	tracefile = DEF_TRACEFILE;
	tracespec = NULL;

	if (is_ltrace) {
		while ((ch = getopt(argc, argv, "af:it:u:")) != -1)
			switch ((char)ch) {
			case 'a':
				append = 1;
				break;
			case 'f':
				tracefile = optarg;
				break;
			case 'i':
				inherit = 1;
				break;
			case 't':
				trpoints = getpoints(optarg);
				if (trpoints < 0) {
					warnx("unknown facility in %s", optarg);
					usage();
				}
				break;
			case 'u':
				tracespec = optarg;
				break;
			default:
				usage();
			}
	} else {
		while ((ch = getopt(argc, argv, "aBCcdf:g:ip:t:")) != -1)
			switch ((char)ch) {
			case 'a':
				append = 1;
				break;
			case 'B':
				putenv("LD_BIND_NOW=");
				break;
			case 'C':
				clear = CLEARALL;
				pidset = 1;
				break;
			case 'c':
				clear = CLEAR;
				break;
			case 'd':
				ops |= KTRFLAG_DESCEND;
				break;
			case 'f':
				tracefile = optarg;
				break;
			case 'g':
				pid = -rpid(optarg);
				pidset = 1;
				break;
			case 'i':
				inherit = 1;
				break;
			case 'p':
				pid = rpid(optarg);
				pidset = 1;
				break;
			case 't':
				trpoints = getpoints(optarg);
				if (trpoints < 0) {
					warnx("unknown facility in %s", optarg);
					usage();
				}
				break;
			default:
				usage();
			}
	}

	argv += optind;
	argc -= optind;
	
	if ((pidset && *argv) || (!pidset && !*argv && clear != CLEAR))
		usage();

	if (inherit)
		trpoints |= KTRFAC_INHERIT;

	(void)signal(SIGSYS, no_ktrace);
	if (clear != NOTSET) {
		if (clear == CLEARALL) {
			ops = KTROP_CLEAR | KTRFLAG_DESCEND;
			trpoints = ALL_POINTS;
			pid = 1;
		} else
			ops |= pid ? KTROP_CLEAR : KTROP_CLEARFILE;

		if (ktrace(tracefile, ops, trpoints, pid) < 0) {
			if (errno == ESRCH)
				err(1, "%d", pid);
			err(1, "%s", tracefile);
		}
		exit(0);
	}

	omask = umask(S_IRWXG|S_IRWXO);
	if (append) {
		if ((fd = open(tracefile, O_CREAT | O_WRONLY, DEFFILEMODE)) < 0)
			err(1, "%s", tracefile);
		if (fstat(fd, &sb) != 0 || sb.st_uid != getuid())
			errx(1, "Refuse to append to %s: not owned by you.",
			    tracefile);
	} else {
		if (unlink(tracefile) == -1 && errno != ENOENT)
			err(1, "unlink %s", tracefile);
		if ((fd = open(tracefile, O_CREAT | O_EXCL | O_WRONLY,
		    DEFFILEMODE)) < 0)
			err(1, "%s", tracefile);
	}
	(void)umask(omask);
	(void)close(fd);

	if (*argv) { 
		if (is_ltrace) {
			if (setenv("LD_TRACE_PLT", inherit ? "i" : "", 1) < 0)
				err(1, "setenv(LD_TRACE_PLT)");
			if (tracespec &&
			    setenv("LD_TRACE_PLTSPEC", tracespec, 1) < 0)
				err(1, "setenv(LD_TRACE_PLTSPEC)");
		}
		if (ktrace(tracefile, ops, trpoints, getpid()) < 0)
			err(1, "%s", tracefile);
		execvp(argv[0], &argv[0]);
		err(1, "exec of '%s' failed", argv[0]);
	}
	else if (ktrace(tracefile, ops, trpoints, pid) < 0) {
		if (errno == ESRCH)
			err(1, "%d", pid);
		err(1, "%s", tracefile);
	}
	exit(0);
}

static int
rpid(const char *p)
{
	static int first;

	if (first++) {
		warnx("only one -g or -p flag is permitted.");
		usage();
	}
	if (!*p) {
		warnx("illegal process id.");
		usage();
	}
	return(atoi(p));
}

static void
usage(void)
{
	if (is_ltrace)
		fprintf(stderr, "usage: %s [-ai] [-f trfile] [-t trstr]"
		    " [-u trspec] command\n",
		    __progname);
	else
		fprintf(stderr, "usage: %s [-aBCcdi] [-f trfile] [-g pgid]"
		    " [-p pid] [-t trstr]\n"
		    "       %s [-adi] [-f trfile] [-t trstr] command\n",
		    __progname, __progname);
	exit(1);
}

/* ARGSUSED */
static void
no_ktrace(int signo)
{
	char buf[8192];

	snprintf(buf, sizeof(buf),
"error:\tktrace() system call not supported in the running kernel\n\tre-compile kernel with 'option KTRACE'\n");
	write(STDERR_FILENO, buf, strlen(buf));
	_exit(1);
}
