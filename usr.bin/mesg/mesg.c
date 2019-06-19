/*	$OpenBSD: mesg.c,v 1.15 2018/08/11 11:04:26 mestre Exp $	*/
/*	$NetBSD: mesg.c,v 1.4 1994/12/23 07:16:32 jtc Exp $	*/

/*
 * Copyright (c) 1987, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int
main(int argc, char *argv[])
{
	struct stat sb;
	char *tty;
	int ch;

	while ((ch = getopt(argc, argv, "")) != -1)
		switch (ch) {
		case '?':
		default:
			goto usage;
		}
	argc -= optind;
	argv += optind;

	if ((tty = ttyname(STDERR_FILENO)) == NULL)
		err(2, "ttyname");

	if (unveil(tty, "rw") == -1)
		err(2, "unveil");
	if (pledge("stdio rpath fattr", NULL) == -1)
		err(2, "pledge");

	if (stat(tty, &sb) < 0)
		err(2, "%s", tty);

	sb.st_mode &= ACCESSPERMS;

	if (*argv == NULL) {
		if (sb.st_mode & S_IWGRP) {
			(void)fprintf(stderr, "is y\n");
			exit(0);
		}
		(void)fprintf(stderr, "is n\n");
		exit(1);
	}

	switch (*argv[0]) {
	case 'y':
		if (chmod(tty, sb.st_mode | S_IWGRP) < 0)
			err(2, "%s", tty);
		exit(0);
	case 'n':
		if (chmod(tty, sb.st_mode & ~S_IWGRP) < 0)
			err(2, "%s", tty);
		exit(1);
	}

usage:	(void)fprintf(stderr, "usage: mesg [n | y]\n");
	exit(2);
}
