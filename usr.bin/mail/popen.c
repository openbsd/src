/*	$OpenBSD: popen.c,v 1.35 2009/10/27 23:59:40 deraadt Exp $	*/
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

#include "rcv.h"
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <stdarg.h>
#include "extern.h"

#define READ 0
#define WRITE 1

struct fp {
	FILE *fp;
	int pipe;
	pid_t pid;
	struct fp *link;
};
static struct fp *fp_head;

struct child {
	pid_t pid;
	char done;
	char free;
	int status;
	struct child *link;
};
static struct child *child, *child_freelist = NULL;

static struct child *findchild(pid_t, int);
static void delchild(struct child *);
static pid_t file_pid(FILE *);
static int handle_spool_locks(int);

FILE *
Fopen(char *file, char *mode)
{
	FILE *fp;

	if ((fp = fopen(file, mode)) != NULL) {
		register_file(fp, 0, 0);
		(void)fcntl(fileno(fp), F_SETFD, 1);
	}
	return(fp);
}

FILE *
Fdopen(int fd, char *mode)
{
	FILE *fp;

	if ((fp = fdopen(fd, mode)) != NULL) {
		register_file(fp, 0, 0);
		(void)fcntl(fileno(fp), F_SETFD, 1);
	}
	return(fp);
}

int
Fclose(FILE *fp)
{

	unregister_file(fp);
	return(fclose(fp));
}

FILE *
Popen(char *cmd, char *mode)
{
	int p[2];
	int myside, hisside, fd0, fd1;
	pid_t pid;
	sigset_t nset;
	FILE *fp;

	if (pipe(p) < 0)
		return(NULL);
	(void)fcntl(p[READ], F_SETFD, 1);
	(void)fcntl(p[WRITE], F_SETFD, 1);
	if (*mode == 'r') {
		myside = p[READ];
		hisside = fd0 = fd1 = p[WRITE];
	} else {
		myside = p[WRITE];
		hisside = fd0 = p[READ];
		fd1 = -1;
	}
	sigemptyset(&nset);
	pid = start_command(value("SHELL"), &nset, fd0, fd1, "-c", cmd, NULL);
	if (pid < 0) {
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
Pclose(FILE *ptr)
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
close_all_files(void)
{

	while (fp_head)
		if (fp_head->pipe)
			(void)Pclose(fp_head->fp);
		else
			(void)Fclose(fp_head->fp);
}

void
register_file(FILE *fp, int pipe, pid_t pid)
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
unregister_file(FILE *fp)
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

static pid_t
file_pid(FILE *fp)
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
 * of stdin (-1 means none) and stdout.  The command name can be a sequence
 * of words.
 * Signals must be handled by the caller.
 * "nset" contains the signals to ignore in the new process.
 * SIGINT is enabled unless it's in "nset".
 */
pid_t
start_commandv(char *cmd, sigset_t *nset, int infd, int outfd, va_list args)
{
	pid_t pid;

	if ((pid = fork()) < 0) {
		warn("fork");
		return(-1);
	}
	if (pid == 0) {
		char *argv[100];
		int i = getrawlist(cmd, argv, sizeof(argv)/ sizeof(*argv));

		while ((argv[i++] = va_arg(args, char *)))
			;
		argv[i] = NULL;
		prepare_child(nset, infd, outfd);
		execvp(argv[0], argv);
		warn("%s", argv[0]);
		_exit(1);
	}
	return(pid);
}

int
run_command(char *cmd, sigset_t *nset, int infd, int outfd, ...)
{
	pid_t pid;
	va_list args;

	va_start(args, outfd);
	pid = start_commandv(cmd, nset, infd, outfd, args);
	va_end(args);
	if (pid < 0)
		return(-1);
	return(wait_command(pid));
}

int
start_command(char *cmd, sigset_t *nset, int infd, int outfd, ...)
{
	va_list args;
	int r;

	va_start(args, outfd);
	r = start_commandv(cmd, nset, infd, outfd, args);
	va_end(args);
	return(r);
}

void
prepare_child(sigset_t *nset, int infd, int outfd)
{
	int i;
	sigset_t eset;

	/*
	 * All file descriptors other than 0, 1, and 2 are supposed to be
	 * close-on-exec.
	 */
	if (infd > 0) {
		dup2(infd, 0);
	} else if (infd != 0) {
		/* we don't want the child stealing my stdin input */
		close(0);
		open(_PATH_DEVNULL, O_RDONLY, 0);
	}
	if (outfd >= 0 && outfd != 1)
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
wait_command(pid_t pid)
{

	if (wait_child(pid) < 0) {
		puts("Fatal error in process.");
		return(-1);
	}
	return(0);
}

static struct child *
findchild(pid_t pid, int dont_alloc)
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
		} else {
			*cpp = (struct child *)malloc(sizeof(struct child));
			if (*cpp == NULL)
				errx(1, "Out of memory");
		}
		(*cpp)->pid = pid;
		(*cpp)->done = (*cpp)->free = 0;
		(*cpp)->link = NULL;
	}
	return(*cpp);
}

static void
delchild(struct child *cp)
{
	struct child **cpp;

	for (cpp = &child; *cpp != cp; cpp = &(*cpp)->link)
		;
	*cpp = cp->link;
	cp->link = child_freelist;
	child_freelist = cp;
}

/* ARGSUSED */
void
sigchild(int signo)
{
	pid_t pid;
	int status;
	struct child *cp;
	int save_errno = errno;

	while ((pid = waitpid((pid_t)-1, &status, WNOHANG)) > 0) {
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
wait_child(pid_t pid)
{
	struct child *cp;
	sigset_t nset, oset;
	pid_t rv = 0;

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
free_child(pid_t pid)
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
handle_spool_locks(int action)
{
	static FILE *lockfp = NULL;

	if (action == 0) {
		/* Clear the lock */
		if (lockfp == NULL) {
			fputs("handle_spool_locks: no spool lock to remove.\n",
			    stderr);
			return(-1);
		}
		(void)Pclose(lockfp);
		lockfp = NULL;
	} else if (action == 1) {
		char *cmd;
		char buf[sizeof(_PATH_LOCKSPOOL) + MAXLOGNAME + 1];

		/* XXX - lockspool requires root for user arg, we do not */
		if (uflag) {
			snprintf(buf, sizeof(buf), "%s %s", _PATH_LOCKSPOOL,
			    myname);
			cmd = buf;
		} else
			cmd = _PATH_LOCKSPOOL;

		/* Create the lock */
		lockfp = Popen(cmd, "r");
		if (lockfp == NULL)
			return(0);
		if (getc(lockfp) != '1') {
			Pclose(lockfp);
			lockfp = NULL;
			return(0);
		}
	} else {
		(void)fprintf(stderr, "handle_spool_locks: unknown action %d\n",
		    action);
		return(-1);
	}

	return(1);
}

int
spool_lock(void)
{

	return(handle_spool_locks(1));
}

int
spool_unlock(void)
{

	return(handle_spool_locks(0));
}
