/*	$OpenBSD: edit.c,v 1.17 2007/10/17 20:02:33 deraadt Exp $	*/
/*	$NetBSD: edit.c,v 1.5 1996/06/08 19:48:20 christos Exp $	*/

/*
 * Copyright (c) 1980, 1993
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

#ifndef lint
#if 0
static const char sccsid[] = "@(#)edit.c	8.1 (Berkeley) 6/6/93";
#else
static const char rcsid[] = "$OpenBSD: edit.c,v 1.17 2007/10/17 20:02:33 deraadt Exp $";
#endif
#endif /* not lint */

#include <sys/types.h>
#include <sys/wait.h>

#include "rcv.h"
#include <errno.h>
#include <fcntl.h>
#include "extern.h"

int editit(const char *, const char *);

/*
 * Mail -- a mail program
 *
 * Perform message editing functions.
 */

/*
 * Edit a message list.
 */
int
editor(void *v)
{
	int *msgvec = v;

	return(edit1(msgvec, 'e'));
}

/*
 * Invoke the visual editor on a message list.
 */
int
visual(void *v)
{
	int *msgvec = v;

	return(edit1(msgvec, 'v'));
}

/*
 * Edit a message by writing the message into a funnily-named file
 * (which should not exist) and forking an editor on it.
 * We get the editor from the stuff above.
 */
int
edit1(int *msgvec, int type)
{
	int c, i;
	FILE *fp;
	struct sigaction oact;
	sigset_t oset;
	struct message *mp;
	off_t size;

	/*
	 * Deal with each message to be edited . . .
	 */
	for (i = 0; msgvec[i] && i < msgCount; i++) {
		if (i > 0) {
			char buf[100];
			char *p;

			printf("Edit message %d [ynq]? ", msgvec[i]);
			if (fgets(buf, sizeof(buf), stdin) == NULL)
				break;
			for (p = buf; *p == ' ' || *p == '\t'; p++)
				;
			if (*p == 'q')
				break;
			if (*p == 'n')
				continue;
		}
		dot = mp = &message[msgvec[i] - 1];
		touch(mp);
		(void)ignoresig(SIGINT, &oact, &oset);
		fp = run_editor(setinput(mp), (off_t)mp->m_size, type, readonly);
		if (fp != NULL) {
			(void)fseek(otf, 0L, SEEK_END);
			size = ftell(otf);
			mp->m_block = blockof(size);
			mp->m_offset = offsetof(size);
			mp->m_size = fsize(fp);
			mp->m_lines = 0;
			mp->m_flag |= MODIFY;
			rewind(fp);
			while ((c = getc(fp)) != EOF) {
				if (c == '\n')
					mp->m_lines++;
				if (putc(c, otf) == EOF)
					break;
			}
			if (ferror(otf))
				warn("/tmp");
			(void)Fclose(fp);
		}
		(void)sigprocmask(SIG_SETMASK, &oset, NULL);
		(void)sigaction(SIGINT, &oact, NULL);
	}
	return(0);
}

/*
 * Run an editor on the file at "fpp" of "size" bytes,
 * and return a new file pointer.
 * Signals must be handled by the caller.
 * "Type" is 'e' for _PATH_EX, 'v' for _PATH_VI.
 */
FILE *
run_editor(FILE *fp, off_t size, int type, int readonly)
{
	FILE *nf = NULL;
	int t;
	time_t modtime;
	char *edit, tempname[PATHSIZE];
	struct stat statb;

	(void)snprintf(tempname, sizeof(tempname),
	    "%s/mail.ReXXXXXXXXXX", tmpdir);
	if ((t = mkstemp(tempname)) == -1 ||
	    (nf = Fdopen(t, "w")) == NULL) {
		warn("%s", tempname);
		goto out;
	}
	if (readonly && fchmod(t, 0400) == -1) {
		warn("%s", tempname);
		(void)rm(tempname);
		goto out;
	}
	if (size >= 0)
		while (--size >= 0 && (t = getc(fp)) != EOF)
			(void)putc(t, nf);
	else
		while ((t = getc(fp)) != EOF)
			(void)putc(t, nf);
	(void)fflush(nf);
	if (fstat(fileno(nf), &statb) < 0)
		modtime = 0;
	else
		modtime = statb.st_mtime;
	if (ferror(nf)) {
		(void)Fclose(nf);
		warn("%s", tempname);
		(void)rm(tempname);
		nf = NULL;
		goto out;
	}
	if (Fclose(nf) < 0) {
		warn("%s", tempname);
		(void)rm(tempname);
		nf = NULL;
		goto out;
	}
	nf = NULL;
	if (type == 'e') {
		edit = value("EDITOR");
		if (edit == NULL || edit[0] == '\0')
			edit = _PATH_EX;
	} else {
		edit = value("VISUAL");
		if (edit == NULL || edit[0] == '\0')
			edit = _PATH_VI;
	}
	if (editit(edit, tempname) == -1) {
		(void)rm(tempname);
		goto out;
	}
	/*
	 * If in read only mode or file unchanged, just remove the editor
	 * temporary and return.
	 */
	if (readonly) {
		(void)rm(tempname);
		goto out;
	}
	if (stat(tempname, &statb) < 0) {
		warn("%s", tempname);
		goto out;
	}
	if (modtime == statb.st_mtime) {
		(void)rm(tempname);
		goto out;
	}
	/*
	 * Now switch to new file.
	 */
	if ((nf = Fopen(tempname, "a+")) == NULL) {
		warn("%s", tempname);
		(void)rm(tempname);
		goto out;
	}
	(void)rm(tempname);
out:
	return(nf);
}

/*
 * Execute an editor on the specified pathname, which is interpreted
 * from the shell.  This means flags may be included.
 *
 * Returns -1 on error, or the exit value on success.
 */
int
editit(const char *ed, const char *pathname)
{
	char *argp[] = {"sh", "-c", NULL, NULL}, *p;
	sig_t sighup, sigint, sigquit, sigchld;
	pid_t pid;
	int saved_errno, st, ret = -1;

	if (ed == NULL)
		ed = getenv("VISUAL");
	if (ed == NULL || ed[0] == '\0')
		ed = getenv("EDITOR");
	if (ed == NULL || ed[0] == '\0')
		ed = _PATH_VI;
	if (asprintf(&p, "%s %s", ed, pathname) == -1)
		return (-1);
	argp[2] = p;

	sighup = signal(SIGHUP, SIG_IGN);
	sigint = signal(SIGINT, SIG_IGN);
	sigquit = signal(SIGQUIT, SIG_IGN);
	sigchld = signal(SIGCHLD, SIG_DFL);
	if ((pid = fork()) == -1)
		goto fail;
	if (pid == 0) {
		execv(_PATH_BSHELL, argp);
		_exit(127);
	}
	while (waitpid(pid, &st, 0) == -1)
		if (errno != EINTR)
			goto fail;
	if (!WIFEXITED(st))
		errno = EINTR;
	else
		ret = WEXITSTATUS(st);

 fail:
	saved_errno = errno;
	(void)signal(SIGHUP, sighup);
	(void)signal(SIGINT, sigint);
	(void)signal(SIGQUIT, sigquit);
	(void)signal(SIGCHLD, sigchld);
	free(p);
	errno = saved_errno;
	return (ret);
}
