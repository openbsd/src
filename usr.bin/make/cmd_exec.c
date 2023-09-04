/*	$OpenBSD: cmd_exec.c,v 1.13 2023/09/04 11:35:11 espie Exp $ */
/*
 * Copyright (c) 2001 Marc Espie.
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
 * THIS SOFTWARE IS PROVIDED BY THE OPENBSD PROJECT AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OPENBSD
 * PROJECT OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "defines.h"
#include "cmd_exec.h"
#include "buf.h"
#include "memory.h"
#include "pathnames.h"
#include "job.h"
#include "str.h"

/* The following array is used to make a fast determination of which
 * characters are interpreted specially by the shell.  If a command
 * contains any of these characters, it is executed by the shell, not
 * directly by us.  */
static char	    meta[256];

void
CmdExec_Init(void)
{
	char *p;

	for (p = "#=|^(){};&<>*?[]:$`\\\n~"; *p != '\0'; p++)
		meta[(unsigned char) *p] = 1;
	/* The null character serves as a sentinel in the string.  */
	meta[0] = 1;
}

static char **
recheck_command_for_shell(char **av)
{
	char *runsh[] = {
		"!", "alias", "cd", "eval", "exit", "read", "set", "ulimit",
		"unalias", "unset", "wait", "umask", NULL
	};

	char **p;

	/* optimization: if exec cmd, we avoid the intermediate shell */
	if (strcmp(av[0], "exec") == 0)
		av++;

	if (!av[0])
		return NULL;

	for (p = runsh; *p; p++)
		if (strcmp(av[0], *p) == 0)
			return NULL;

	return av;
}

void
run_command(const char *cmd, bool errCheck)
{
	const char *p;
	char *shargv[4];
	char **todo;

	shargv[0] = _PATH_BSHELL;

	shargv[1] = errCheck ? "-ec" : "-c";
	shargv[2] = (char *)cmd;
	shargv[3] = NULL;

	todo = shargv;


	/* Search for meta characters in the command. If there are no meta
	 * characters, there's no need to execute a shell to execute the
	 * command.  */
	for (p = cmd; !meta[(unsigned char)*p]; p++)
		continue;
	if (*p == '\0') {
		char *bp;
		char **av;
		int argc;
		/* No meta-characters, so probably no need to exec a shell.
		 * Break the command into words to form an argument vector
		 * we can execute.  */
		av = brk_string(cmd, &argc, &bp);
		av = recheck_command_for_shell(av);
		if (av != NULL)
			todo = av;
	}
	execvp(todo[0], todo);
	if (errno == ENOENT)
		fprintf(stderr, "%s: not found\n", todo[0]);
	else
		perror(todo[0]);
	_exit(1);
}

char *
Cmd_Exec(const char *cmd, char **err)
{
	int 	fds[2]; 	/* Pipe streams */
	pid_t 	cpid;		/* Child PID */
	char	*result;	/* Result */
	int 	status; 	/* Command exit status */
	BUFFER	buf;		/* Buffer to store the result. */
	char	*cp;		/* Pointer into result. */
	ssize_t	cc;		/* Characters read from pipe. */
	size_t	length;		/* Total length of result. */


	*err = NULL;

	/* Open a pipe for retrieving shell's output. */
	if (pipe(fds) == -1) {
		*err = "Couldn't create pipe for \"%s\"";
		goto bad;
	}

	/* Fork */
	switch (cpid = fork()) {
	case 0:
		reset_signal_mask();
		/* Close input side of pipe */
		(void)close(fds[0]);

		/* Duplicate the output stream to the shell's output, then
		 * shut the extra thing down. Note we don't fetch the error
		 * stream: user can use redirection to grab it as this goes
		 * through /bin/sh.
		 */
		if (fds[1] != 1) {
			(void)dup2(fds[1], 1);
			(void)close(fds[1]);
		}

		run_command(cmd, false);
		/*NOTREACHED*/

	case -1:
		*err = "Couldn't exec \"%s\"";
		goto bad;

	default:
		/* No need for the writing half. */
		(void)close(fds[1]);

		Buf_Init(&buf, MAKE_BSIZE);

		do {
			char   grab[BUFSIZ];

			cc = read(fds[0], grab, sizeof(grab));
			if (cc > 0)
				Buf_AddChars(&buf, cc, grab);
		} while (cc > 0 || (cc == -1 && errno == EINTR));

		/* Close the input side of the pipe.  */
		(void)close(fds[0]);

		/* Wait for the child to exit.  */
		while (waitpid(cpid, &status, 0) == -1 && errno == EINTR)
			continue;

		if (cc == -1)
			*err = "Couldn't read shell's output for \"%s\"";

		if (status)
			*err = "\"%s\" returned non-zero status";

		length = Buf_Size(&buf);
		result = Buf_Retrieve(&buf);

		/* The result is null terminated, Convert newlines to spaces. */
		cp = result + length - 1;

		if (cp >= result && *cp == '\n')
			/* A final newline is just stripped.  */
			*cp-- = '\0';

		while (cp >= result) {
			if (*cp == '\n')
				*cp = ' ';
			cp--;
		}
		break;
	}
	return result;
    bad:
	return estrdup("");
}

