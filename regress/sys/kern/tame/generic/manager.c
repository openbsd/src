/*	$OpenBSD: manager.c,v 1.2 2015/09/10 11:18:10 semarie Exp $ */
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

#include <sys/syslimits.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "actions.h"

extern char *__progname;

int	execute_action(action_t, va_list);

static const char *
coredump_name()
{
	static char coredump[PATH_MAX] = "";

	if (*coredump)
		return (coredump);

	if (strlcpy(coredump, __progname, sizeof(coredump)) >= sizeof(coredump))
		errx(1, "coredump: strlcpy");

	if (strlcat(coredump, ".core", sizeof(coredump)) >= sizeof(coredump))
		errx(1, "coredump: strlcat");

	return (coredump);
}


static int
check_coredump()
{
	const char *coredump = coredump_name();
	int fd;

	if ((fd = open(coredump, O_RDONLY)) == -1) {
		if (errno == ENOENT)
			return (1); /* coredump not found */
		else
			return (-1); /* error */
	}

	(void)close(fd);
	return (0); /* coredump found */
}


static int
clear_coredump(int *ret, int ntest)
{
	int saved_errno = errno;
	int u;

	if (((u = unlink(coredump_name())) != 0) && (errno != ENOENT)) {
		warn("test(%d): clear_coredump", ntest);
		*ret = EXIT_FAILURE;
		return (-1);
	}
	errno = saved_errno;

	return (0);
}


static int
grab_syscall(pid_t pid)
{
	int	 ret = -1;
	char	*search = NULL;
	int	 searchlen;
	FILE	*fd;
	char	 line[1024];
	char	*end;

	/* build searched string */
	if ((searchlen = asprintf(&search, "%s(%d): syscall ", __progname, pid))
	    <= 0)
		goto out;

	/* call dmesg */
	if ((fd = popen("/sbin/dmesg", "r")) == NULL)
		goto out;

	/* search the string */
	while (1) {
		/* read a line */
		fgets(line, sizeof(line), fd);

		/* error checking */
		if (ferror(fd)) {
			ret = -1;
			goto out;
		}

		/* quit */
		if (feof(fd))
			break;

		/* strip trailing '\n' */
		end = strchr(line, '\n');
		if (*end == '\n')
			*end = '\0';

		/* check if found */
		if (strncmp(search, line, searchlen) == 0) {
			const char *errstr = NULL;
			/* found */
			ret = strtonum(line + searchlen, 0, 255, &errstr);
			if (errstr) {
				warn("strtonum: line=%s err=%s", line, errstr);
				return (-1);
			}
		}
	}

	/* cleanup */
	if (pclose(fd) == -1)
		goto out;

	/* not found */
	if (ret == -1)
		ret = 0;

out:
	free(search);
	return (ret);
}


void
start_test(int *ret, int ntest, const char *request, const char *paths[], ...)
{
	static int ntest_check = 0;
	pid_t pid;
	int status;
	va_list ap;
	action_t action;
	int i;

#ifndef DEBUG
	/* check ntest (useful for dev) */
	if (ntest != ++ntest_check)
		errx(EXIT_FAILURE,
		    "invalid test number: should be %d but is %d",
		    ntest_check, ntest);
#endif /* DEBUG */

	/* unlink previous coredump (if exists) */
	if (clear_coredump(ret, ntest) == -1)
		return;
	
	/* fork and launch the test */
	switch (pid = fork()) {
	case -1:
		warn("test(%d) fork", ntest);
		*ret = EXIT_FAILURE;
		return;

	case 0:
		/* create a new session (for AC_KILL) */
		setsid();

		/* XXX redirect output to /dev/null ? */
		if (tame(request, paths) != 0)
			err(errno, "tame");
		
		va_start(ap, paths);
		while ((action = va_arg(ap, action_t)) != AC_EXIT) {
			execute_action(action, ap);
			if (errno != 0)
				_exit(errno);
		}
		va_end(ap);

		_exit(EXIT_SUCCESS);
		/* NOTREACHED */
	}

	/* wait for test to terminate */
	while (waitpid(pid, &status, 0) < 0) {
		if (errno == EAGAIN)
			continue;
		warn("test(%d): waitpid", ntest);
		*ret = EXIT_FAILURE;
		return;
	}

	/* show status and details */
	printf("test(%d): tame=(\"%s\",{", ntest, request);
	for (i = 0; paths && paths[i] != NULL; i++)
		printf("\"%s\",", paths[i]);
	printf("NULL}) status=%d", status);

	if (WIFCONTINUED(status))
		printf(" continued");

	if (WIFEXITED(status)) {
		int e = WEXITSTATUS(status);
		printf(" exit=%d", e);
		if (e > 0 && e <= ELAST)
			printf(" (errno: \"%s\")", strerror(e));
	}

	if (WIFSIGNALED(status)) {
		int signal = WTERMSIG(status);
		printf(" signal=%d", signal);

		/* check if core file is really here ? */
		if (WCOREDUMP(status)) {
			int coredump = check_coredump();

			switch(coredump) {
			case -1: /* error */
				warn("test(%d): check_coredump", ntest);
				*ret = EXIT_FAILURE;
				return;

			case 0: /* found */
				printf(" coredump=present");
				break;

			case 1:	/* not found */
				printf(" coredump=absent");
				break;

			default:
				warnx("test(%d): unknown coredump code %d",
				    ntest, coredump);
				*ret = EXIT_FAILURE;
				return;
			}

		}

		/* grab tamed syscall from dmesg */
		if ((signal == SIGKILL) || (signal = SIGABRT)) {
			int syscall = grab_syscall(pid);
			switch (syscall) {
			case -1:	/* error */
				warn("test(%d): grab_syscall pid=%d", ntest,
				    pid);
				*ret = EXIT_FAILURE;
				return;

			case 0:		/* not found */
				printf(" tamed_syscall=not_found");
				break;

			default:
				printf(" tamed_syscall=%d", syscall);
			}
		}
	}

	if (WIFSTOPPED(status))
		printf(" stop=%d", WSTOPSIG(status));

	printf("\n");
}
