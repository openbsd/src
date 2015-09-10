/*	$OpenBSD: actions.c,v 1.3 2015/09/10 11:18:10 semarie Exp $ */
/*
 * Copyright (c) 2015 Sebastien Marie <semarie@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "actions.h"

int
execute_action(action_t action, va_list opts)
{
	errno = 0;

	switch (action) {
	case AC_EXIT:
		/* should be catched by manager.c (before been here) */
		_exit(ENOTSUP);
		/* NOTREACHED */

	case AC_KILL:
		kill(0, SIGINT);
		break;

	case AC_INET:
		socket(AF_INET, SOCK_STREAM, 0);
		break;

	case AC_TAME:
		tame(va_arg(opts, char *), NULL);
		break;

	case AC_ALLOWED_SYSCALLS:
		clock_getres(CLOCK_MONOTONIC, NULL);
		clock_gettime(CLOCK_MONOTONIC, NULL);
		/* fchdir(); */
		getdtablecount();
		getegid();
		geteuid();
		getgid();
		getgroups(0, NULL);
		getitimer(ITIMER_REAL, NULL);
		getlogin();
		getpgid(0);
		getpgrp();
		getpid();
		getppid();
		/* getresgid(); */
		/* getresuid(); */
		{ struct rlimit rl; getrlimit(RLIMIT_CORE, &rl); }
		getsid(0);
		getthrid();
		{ struct timeval tp; gettimeofday(&tp, NULL); }
		getuid();
		geteuid();
		issetugid();
		/* nanosleep(); */
		/* sigreturn(); */
		umask(0000);
		/* wait4(); */

		break;

	case AC_OPENFILE_RDONLY:
		{
			const char *filename = va_arg(opts, const char *);
			int fd = open(filename, O_RDONLY);
			if (fd != -1)
				close(fd);
		}
		break;
	}

	return (errno);
}
