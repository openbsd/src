/*	$OpenBSD: popen.c,v 1.23 1998/09/27 21:16:42 millert Exp $	*/
/*	$NetBSD: popen.c,v 1.6 1997/05/13 06:48:42 mikel Exp $	*/

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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
static char sccsid[] = "@(#)popen.c	8.1 (Berkeley) 6/6/93";
#else
static char rcsid[] = "$OpenBSD: popen.c,v 1.23 1998/09/27 21:16:42 millert Exp $";
#endif
#endif /* not lint */

#include "rcv.h"
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include "extern.h"

#define READ 0
#define WRITE 1

struct fp {
	FILE *fp;
	int pipe;
	int pid;
	struct fp *link;
};
static struct fp *fp_head;

struct child {
	int pid;
	char done;
	char free;
	int status;
	struct child *link;
};
static struct child *child, *child_freelist = NULL;
static struct child *findchild __P((int, int));
static void delchild __P((struct child *));
static int file_pid __P((FILE *));
static int handle_spool_locks __P((int));

FILE *
Fopen(file, mode)
	char *file, *mode;
{
	FILE *fp;

	if ((fp = fopen(file, mode)) != NULL) {
		register_file(fp, 0, 0);
		(void)fcntl(fileno(fp), F_SETFD, 1);
	}
	return(fp);
}

FILE *
Fdopen(fd, mode)
	int fd;
	char *mode;
{
	FILE *fp;

	if ((fp = fdopen(fd, mode)) != NULL) {
		register_file(fp, 0, 0);
		(void)fcntl(fileno(fp), F_SETFD, 1);
	}
	return(fp);
}

int
Fclose(fp)
	FILE *fp;
{
	unregister_file(fp);
	return(fclose(fp));
}

FILE *
Popen(cmd, mode)
	char *cmd;
	char *mode;
{
	int p[2];
	int myside, hisside, fd0, fd1;
	int pid;
	sigset_t nset;
	FILE *fp;

	if (pipe(p) < 0)
		return(NULL);
	(void)fcntl(p[READ], F_SETFD, 1);
	(void)fcntl(p[WRITE], F_SETFD, 1);
	if (*mode == 'r') {
		myside = p[READ];
		fd0 = -1;
		hisside = fd1 = p[WRITE];
	} else {
		myside = p[WRITE];
		hisside = fd0 = p[READ];
		fd1 = -1;
	}
	sigemptyset(&nset);
	if ((pid = start_command(cmd, &nset, fd0, fd1, NULL, NULL, NULL)) < 0) {
		(void)close(p[READ]);
		(void)close(p[WRITE]);
		return(NULL);
	}
	(void)close(hisside);
	if ((fp = fdopen(myside, mode)) != NULL)
		register_file(fp, 1, pid);
	return(fp);
}

int
Pclose(ptr)
	FILE *ptr;
{
	int i;
	sigset_t nset, oset;

	i = file_pid(ptr);
	unregister_file(ptr);
	(void)fclose(ptr);
	sigemptyset(&nset);
	sigaddset(&nset, SIGINT);
	sigaddset(&nset, SIGHUP);
	sigprocmask(SIG_BLOCK, &nset, &oset);
	i = wait_child(i);
	sigprocmask(SIG_SETMASK, &oset, NULL);
	return(i);
}

void
close_all_files()
{

	while (fp_head)
		if (fp_head->pipe)
			(void)Pclose(fp_head->fp);
		else
			(void)Fclose(fp_head->fp);
}

void
register_file(fp, pipe, pid)
	FILE *fp;
	int pipe, pid;
{
	struct fp *fpp;

	if ((fpp = (struct fp *)malloc(sizeof(*fpp))) == NULL)
		errx(1, "Out of memory");
	fpp->fp = fp;
	fpp->pipe = pipe;
	fpp->pid = pid;
	fpp->link = fp_head;
	fp_head = fpp;
}

void
unregister_file(fp)
	FILE *fp;
{
	struct fp **pp, *p;

	for (pp = &fp_head; (p = *pp) != NULL; pp = &p->link)
		if (p->fp == fp) {
			*pp = p->link;
			(void)free(p);
			return;
		}
	errx(1, "Invalid file pointer");
}

static int
file_pid(fp)
	FILE *fp;
{
	struct fp *p;

	for (p = fp_head; p; p = p->link)
		if (p->fp == fp)
			return(p->pid);
	errx(1, "Invalid file pointer");
	/*NOTREACHED*/
}

/*
 * Run a command without a shell, with optional arguments and splicing
 * of stdin and stdout.  The command name can be a sequence of words.
 * Signals must be handled by the caller.
 * "nset" contains the signals to ignore in the new process.
 * SIGINT is enabled unless it's in "nset".
 */
/*VARARGS4*/
int
run_command(cmd, nset, infd, outfd, a0, a1, a2)
	char *cmd;
	sigset_t *nset;
	int infd, outfd;
	char *a0, *a1, *a2;
{
	int pid;

	if ((pid = start_command(cmd, nset, infd, outfd, a0, a1, a2)) < 0)
		return(-1);
	return(wait_command(pid));
}

/*VARARGS4*/
int
start_command(cmd, nset, infd, outfd, a0, a1, a2)
	char *cmd;
	sigset_t *nset;
	int infd, outfd;
	char *a0, *a1, *a2;
{
	int pid;

	if ((pid = vfork()) < 0) {
		warn("fork");
		return(-1);
	}
	if (pid == 0) {
		char *argv[100];
		int i = getrawlist(cmd, argv, sizeof(argv)/ sizeof(*argv));

		if ((argv[i++] = a0) != NULL &&
		    (argv[i++] = a1) != NULL &&
		    (argv[i++] = a2) != NULL)
			argv[i] = NULL;
		prepare_child(nset, infd, outfd);
		execvp(argv[0], argv);
		warn(argv[0]);
		_exit(1);
	}
	return(pid);
}

void
prepare_child(nset, infd, outfd)
	sigset_t *nset;
	int infd, outfd;
{
	int i;
	sigset_t eset;

	/*
	 * All file descriptors other than 0, 1, and 2 are supposed to be
	 * close-on-exec.
	 */
	if (infd >= 0)
		dup2(infd, 0);
	if (outfd >= 0)
		dup2(outfd, 1);
	if (nset == NULL)
		return;
	if (nset != NULL) {
		for (i = 1; i < NSIG; i++)
			if (sigismember(nset, i))
				(void)signal(i, SIG_IGN);
	}
	if (nset == NULL || !sigismember(nset, SIGINT))
		(void)signal(SIGINT, SIG_DFL);
	sigemptyset(&eset);
	(void)sigprocmask(SIG_SETMASK, &eset, NULL);
}

int
wait_command(pid)
	int pid;
{

	if (wait_child(pid) < 0) {
		puts("Fatal error in process.");
		return(-1);
	}
	return(0);
}

static struct child *
findchild(pid, dont_alloc)
	int pid;
	int dont_alloc;
{
	struct child **cpp;

	for (cpp = &child; *cpp != NULL && (*cpp)->pid != pid;
	     cpp = &(*cpp)->link)
			;
	if (*cpp == NULL) {
		if (dont_alloc)
			return(NULL);
		if (child_freelist) {
			*cpp = child_freelist;
			child_freelist = (*cpp)->link;
		} else
			*cpp = (struct child *)malloc(sizeof(struct child));
		(*cpp)->pid = pid;
		(*cpp)->done = (*cpp)->free = 0;
		(*cpp)->link = NULL;
	}
	return(*cpp);
}

static void
delchild(cp)
	struct child *cp;
{
	struct child **cpp;

	for (cpp = &child; *cpp != cp; cpp = &(*cpp)->link)
		;
	*cpp = cp->link;
	cp->link = child_freelist;
	child_freelist = cp;
}

void
sigchild(signo)
	int signo;
{
	int pid;
	int status;
	struct child *cp;
	int save_errno = errno;

	while ((pid =
	    waitpid((pid_t)-1, &status, WNOHANG)) > 0) {
		cp = findchild(pid, 1);
		if (!cp)
			continue;
		if (cp->free)
			delchild(cp);
		else {
			cp->done = 1;
			cp->status = status;
		}
	}
	errno = save_errno;
}

int wait_status;

/*
 * Wait for a specific child to die.
 */
int
wait_child(pid)
	int pid;
{
	struct child *cp;
	sigset_t nset, oset;
	int rv = 0;

	sigemptyset(&nset);
	sigaddset(&nset, SIGCHLD);
	sigprocmask(SIG_BLOCK, &nset, &oset);
	/*
	 * If we have not already waited on the pid (via sigchild)
	 * wait on it now.  Otherwise, use the wait status stashed
	 * by sigchild.
	 */
	cp = findchild(pid, 1);
	if (cp == NULL || !cp->done)
		rv = waitpid(pid, &wait_status, 0);
	else
		wait_status = cp->status;
	if (cp != NULL)
		delchild(cp);
	sigprocmask(SIG_SETMASK, &oset, NULL);
	if (rv == -1 || (WIFEXITED(wait_status) && WEXITSTATUS(wait_status)))
		return(-1);
	else
		return(0);
}

/*
 * Mark a child as don't care.
 */
void
free_child(pid)
	int pid;
{
	struct child *cp;
	sigset_t nset, oset;

	sigemptyset(&nset);
	sigaddset(&nset, SIGCHLD);
	sigprocmask(SIG_BLOCK, &nset, &oset);
	if ((cp = findchild(pid, 0)) != NULL) {
		if (cp->done)
			delchild(cp);
		else
			cp->free = 1;
	}
	sigprocmask(SIG_SETMASK, &oset, NULL);
}

/*
 * Lock(1)/unlock(0) mail spool using lockspool(1).
 * Returns 1 for success, 0 for failure, -1 for bad usage.
 */
static int
handle_spool_locks(action)
	int action;
{
	static FILE *lockfp = NULL;
	static int lock_pid;

	if (action == 0) {
		/* Clear the lock */
		if (lockfp == NULL) {
			fputs("handle_spool_locks: no spool lock to remove.\n",
			    stderr);
			return(-1);
		}
		(void)kill(lock_pid, SIGTERM);
		(void)Pclose(lockfp);
		lockfp = NULL;
	} else if (action == 1) {
		char *cmd = _PATH_LOCKSPOOL;

		/* XXX - lockspool requires root for user arg, we do not */
		if (uflag && asprintf(&cmd, "%s %s", _PATH_LOCKSPOOL,
		    myname) == -1)
			errx(1, "Out of memory");

		/* Create the lock */
		lockfp = Popen(cmd, "r");
		if (uflag)
			free(cmd);
		if (lockfp == NULL || getc(lockfp) != '1') {
			lockfp = NULL;
			return(0);
		}
		lock_pid = fp_head->pid;	/* new entries added at head */
	} else {
		(void)fprintf(stderr, "handle_spool_locks: unknown action %d\n",
		    action);
		return(-1);
	}

	return(1);
}

int
spool_lock()
{
	return(handle_spool_locks(1));
}

int
spool_unlock()
{
	return(handle_spool_locks(0));
}
