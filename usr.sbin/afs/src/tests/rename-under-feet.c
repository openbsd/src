/*
 * Copyright (c) 2000 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <dirent.h>

#include <roken.h>

#include <err.h>

RCSID("$KTH: rename-under-feet.c,v 1.9 2000/10/03 00:35:39 lha Exp $");

static void
emkdir (const char *path, mode_t mode)
{
    int ret = mkdir (path, mode);
    if (ret < 0)
	err (1, "mkdir %s", path);
}

static pid_t child_pid;

static sig_atomic_t term_sig = 0;

static RETSIGTYPE
child_sigterm (int signo)
{
    term_sig = 1;
}

static int
child_chdir (const char *path)
{
    int ret;
    int pfd[2];

    ret = pipe (pfd);
    if (ret < 0)
	err (1, "pipe");

    child_pid = fork ();
    if (child_pid < 0)
	err (1, "fork");
    if (child_pid != 0) {
	close (pfd[1]);
	return pfd[0];
    } else {
	char buf[256];
	struct sigaction sa;
	FILE *fp;

	sa.sa_handler = child_sigterm;
	sigfillset(&sa.sa_mask);
	sa.sa_flags = 0;
	sigaction (SIGTERM, &sa, NULL);

	close (pfd[0]);
	ret = chdir (path);
	if (ret < 0)
	    err (1, "chdir %s", path);
	ret = write (pfd[1], "", 1);
	if (ret != 1)
	    err (1, "write");
	while (!term_sig)
	    pause ();
	if(getcwd (buf, sizeof(buf)) == NULL)
	    err (1, "getcwd");
	fp = fdopen (4, "w");
	if (fp != NULL)
	    fprintf (fp, "child: cwd = %s\n", buf);
	exit (0);
    }
}

static void
kill_child (void)
{
    kill (child_pid, SIGTERM);
}

int
main (int argc, char **argv)
{
    struct stat sb;
    int ret;
    int fd;
    char buf[1];
    int status;

    set_progname (argv[0]);

    emkdir ("one", 0777);
    emkdir ("two", 0777);
    emkdir ("one/a", 0777);

    fd = child_chdir ("one/a");
    atexit (kill_child);
    ret = read (fd, buf, 1);
    if (ret != 1)
	err (1, "read");
    ret = rename ("one/a", "two/a");
    if (ret < 0)
	err (1, "rename one/a two");
    ret = lstat ("two/a", &sb);
    if (ret < 0)
	err (1, "lstat two/a");
    ret = lstat ("one/a", &sb);
    if (ret != -1 || errno != ENOENT)
	errx (1, "one/a still exists");
    kill_child ();
    waitpid (child_pid, &status, 0);
    ret = lstat ("one/a", &sb);
    if (ret != -1 || errno != ENOENT)
	errx (1, "one/a still exists after child");
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
	return 0;
    else
	return 1;
}
