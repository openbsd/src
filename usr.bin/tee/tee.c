/*	$OpenBSD: tee.c,v 1.12 2017/07/11 13:14:59 bluhm Exp $	*/
/*	$NetBSD: tee.c,v 1.5 1994/12/09 01:43:39 jtc Exp $	*/

/*
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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/queue.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct list {
	SLIST_ENTRY(list) next;
	int fd;
	char *name;
};
SLIST_HEAD(, list) head;

static void
add(int fd, char *name)
{
	struct list *p;

	if ((p = malloc(sizeof(*p))) == NULL)
		err(1, NULL);
	p->fd = fd;
	p->name = name;
	SLIST_INSERT_HEAD(&head, p, next);
}

int
main(int argc, char *argv[])
{
	struct list *p;
	int fd;
	ssize_t n, rval, wval;
	char *bp;
	int append, ch, exitval;
	char buf[8192];

	if (pledge("stdio wpath cpath", NULL) == -1)
		err(1, "pledge");

	SLIST_INIT(&head);

	append = 0;
	while ((ch = getopt(argc, argv, "ai")) != -1) {
		switch(ch) {
		case 'a':
			append = 1;
			break;
		case 'i':
			(void)signal(SIGINT, SIG_IGN);
			break;
		default:
			(void)fprintf(stderr, "usage: tee [-ai] [file ...]\n");
			return 1;
		}
	}
	argv += optind;
	argc -= optind;

	add(STDOUT_FILENO, "stdout");

	exitval = 0;
	while (*argv) {
		if ((fd = open(*argv, O_WRONLY | O_CREAT |
		    (append ? O_APPEND : O_TRUNC), DEFFILEMODE)) == -1) {
			warn("%s", *argv);
			exitval = 1;
		} else
			add(fd, *argv);
		argv++;
	}

	if (pledge("stdio", NULL) == -1)
		err(1, "pledge");

	while ((rval = read(STDIN_FILENO, buf, sizeof(buf))) > 0) {
		SLIST_FOREACH(p, &head, next) {
			n = rval;
			bp = buf;
			do {
				if ((wval = write(p->fd, bp, n)) == -1) {
					warn("%s", p->name);
					exitval = 1;
					break;
				}
				bp += wval;
			} while (n -= wval);
		}
	}
	if (rval == -1) {
		warn("read");
		exitval = 1;
	}

	SLIST_FOREACH(p, &head, next) {
		if (close(p->fd) == -1) {
			warn("%s", p->name);
			exitval = 1;
		}
	}

	return exitval;
}
