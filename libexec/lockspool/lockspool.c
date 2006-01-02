/*	$OpenBSD: lockspool.c,v 1.12 2006/01/02 16:23:26 millert Exp $	*/

/*
 * Copyright (c) 1998 Theo de Raadt <deraadt@theos.com>
 * Copyright (c) 1998 Todd C. Miller <Todd.Miller@courtesan.com>
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
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef lint
static const char rcsid[] = "$OpenBSD: lockspool.c,v 1.12 2006/01/02 16:23:26 millert Exp $";
#endif /* not lint */

#include <signal.h>
#include <pwd.h>
#include <syslog.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include "mail.local.h"

void unhold(int);
void usage(void);

extern char *__progname;

int
main(int argc, char *argv[])
{
	struct passwd *pw;
	char *from, c;
	int holdfd;

	openlog(__progname, LOG_PERROR, LOG_MAIL);

	if (argc != 1 && argc != 2)
		usage();
	if (argc == 2 && getuid() != 0)
		merr(FATAL, "you must be root to lock someone else's spool");

	signal(SIGTERM, unhold);
	signal(SIGINT, unhold);
	signal(SIGHUP, unhold);
	signal(SIGPIPE, unhold);

	if (argc == 2)
		from = argv[1];
	else
		from = getlogin();

	if (from) {
		pw = getpwnam(from);
		if (pw == NULL)
			exit (1);
	} else {
		pw = getpwuid(getuid());
		if (pw)
			from = pw->pw_name;
		else
			exit (1);
	}

	holdfd = getlock(from, pw);
	if (holdfd == -1) {
		write(STDOUT_FILENO, "0\n", 2);
		exit (1);
	}
	write(STDOUT_FILENO, "1\n", 2);

	while (read(STDIN_FILENO, &c, 1) == -1 && errno == EINTR)
		;
	rellock();
	exit (0);
}

void
unhold(int signo)
{

	rellock();
	_exit(0);
}

void
usage(void)
{

	merr(FATAL, "usage: %s [username]", __progname);
}
