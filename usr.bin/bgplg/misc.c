/*	$OpenBSD: misc.c,v 1.4 2011/04/19 23:54:00 matthew Exp $	*/

/*
 * Copyright (c) 2005, 2006 Reyk Floeter <reyk@vantronix.net>
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

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <sys/time.h>

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>

#include "bgplg.h"

static volatile pid_t child = -1;

int
lg_show_version(struct cmd *cmds, char **argv)
{
	struct utsname uts;
	if (uname(&uts) >= 0)
		printf("%s %s (%s)\n\n", uts.sysname, uts.release, uts.machine);
	printf("%s - %s\n", NAME, BRIEF);
	return (0);
}

int
lg_checkperm(struct cmd *cmd)
{
	struct stat stbuf;

	/* No external command to execute, this is always valid */
	if (cmd->earg == NULL || cmd->earg[0] == NULL)
		return (1);

	/*
	 * Skip commands if the executable is missing or
	 * the permission mode has been set to zero (the default
	 * in a CGI environment).
	 */
	if (stat(cmd->earg[0], &stbuf) != 0 ||
	    (stbuf.st_mode & ~S_IFMT) == 0)
		return (0);

	return (1);
}

int
lg_help(struct cmd *cmds, char **argv)
{
	u_int i;

	printf("valid commands:\n");
	for (i = 0; cmds[i].name != NULL; i++) {
		if (!lg_checkperm(&cmds[i]))
			continue;

		printf("  %s", cmds[i].name);
		if (cmds[i].minargs > 0)
			printf(" { arg }");
		else if (cmds[i].maxargs > 0)
			printf(" [ arg ]");
		printf("\n");
	}
	return (0);
}

void
lg_sig_alarm(int sig)
{
	if (child != -1) {
		/* Forcibly kill the child, no excuse... */
		kill(child, SIGKILL);
	}
}

int
lg_exec(const char *file, char **new_argv)
{
	int status = 0, ret = 0;
	sig_t save_quit, save_int, save_chld;
	struct itimerval it;

	if (new_argv == NULL)
		return (EFAULT);

	save_quit = signal(SIGQUIT, SIG_IGN);
	save_int = signal(SIGINT, SIG_IGN);
	save_chld = signal(SIGCHLD, SIG_DFL);

	switch (child = fork()) {
	case -1:
		ret = errno;
		goto done;
	case 0:
		signal(SIGQUIT, SIG_DFL);
		signal(SIGINT, SIG_DFL);
		signal(SIGCHLD, SIG_DFL);

		execvp(file, new_argv);
		_exit(127);
		break;
	default:
		/* Kill the process after a timeout */
		signal(SIGALRM, lg_sig_alarm);
		bzero(&it, sizeof(it));
		it.it_value.tv_sec = BGPLG_TIMEOUT;
		setitimer(ITIMER_REAL, &it, NULL);

		waitpid(child, &status, 0);
		break;
	}

	switch (ret) {
	case -1:
		ret = ECHILD;
		break;
	default:
		if (WIFEXITED(status))
			ret = WEXITSTATUS(status);
		else
			ret = ECHILD;
	}

 done:
	/* Disable the process timeout timer */
	bzero(&it, sizeof(it));
	setitimer(ITIMER_REAL, &it, NULL);
	child = -1;

	signal(SIGQUIT, save_quit);
	signal(SIGINT, save_int);
	signal(SIGCHLD, save_chld);
	signal(SIGALRM, SIG_DFL);

	return (ret);
}

ssize_t
lg_strip(char *str)
{
	size_t len;

	if ((len = strlen(str)) < 1)
		return (0); /* XXX EINVAL? */

	if (isspace(str[len - 1])) {
		str[len - 1] = '\0';
		return (lg_strip(str));
	}

	return (strlen(str));
}
