/*	$OpenPackages$ */
/*	$OpenBSD: cmd_exec.c,v 1.2 2002/07/31 19:29:20 mickey Exp $ */
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
#include <unistd.h>
#include "config.h"
#include "defines.h"
#include "cmd_exec.h"
#include "buf.h"
#include "memory.h"
#include "pathnames.h"

char *
Cmd_Exec(cmd, err)
    const char	*cmd;
    char	**err;
{
    char	*args[4];	/* Args for invoking the shell */
    int 	fds[2]; 	/* Pipe streams */
    pid_t 	cpid;		/* Child PID */
    pid_t 	pid;		/* PID from wait() */
    char	*result;	/* Result */
    int 	status; 	/* Command exit status */
    BUFFER	buf;		/* Buffer to store the result. */
    char	*cp;		/* Pointer into result. */
    ssize_t	cc;		/* Characters read from pipe. */
    size_t	length;		/* Total length of result. */


    *err = NULL;

    /* Set up arguments for the shell. */
    args[0] = "sh";
    args[1] = "-c";
    args[2] = (char *)cmd;
    args[3] = NULL;

    /* Open a pipe for retrieving shell's output. */
    if (pipe(fds) == -1) {
	*err = "Couldn't create pipe for \"%s\"";
	goto bad;
    }

    /* Fork */
    switch (cpid = fork()) {
    case 0:
	/* Close input side of pipe */
	(void)close(fds[0]);

	/* Duplicate the output stream to the shell's output, then
	 * shut the extra thing down. Note we don't fetch the error
	 * stream: user can use redirection to grab it as this goes
	 * through /bin/sh.
	 */
	(void)dup2(fds[1], 1);
	if (fds[1] != 1)
	    (void)close(fds[1]);

	(void)execv(_PATH_BSHELL, args);
	_exit(1);
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
	}
	while (cc > 0 || (cc == -1 && errno == EINTR));

	/* Close the input side of the pipe.  */
	(void)close(fds[0]);

	/* Wait for the child to exit.  */
	while ((pid = wait(&status)) != cpid && pid >= 0)
	    continue;

	if (cc == -1)
	    *err = "Couldn't read shell's output for \"%s\"";

	if (status)
	    *err = "\"%s\" returned non-zero status";

	length = Buf_Size(&buf);
	result = Buf_Retrieve(&buf);

	/* The result is null terminated, Convert newlines to spaces. */
	cp = result + length - 1;

	if (*cp == '\n')
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

