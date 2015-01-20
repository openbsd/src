/*	$OpenBSD: child.c,v 1.24 2015/01/20 03:55:18 guenther Exp $	*/

/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.
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

#include "defs.h"

/*
 * Functions for rdist related to children
 */

#include <sys/types.h>
#include <sys/select.h>
#include <sys/wait.h>

typedef enum _PROCSTATE {
    PSrunning,
    PSdead
} PROCSTATE;

/*
 * Structure for child rdist processes mainted by the parent
 */
struct _child {
	char	       *c_name;			/* Name of child */
	int		c_readfd;		/* Read file descriptor */
	pid_t		c_pid;			/* Process ID */
	PROCSTATE       c_state;		/* Running? */
	struct _child  *c_next;			/* Next entry */
};
typedef struct _child CHILD;

static CHILD	       *childlist = NULL;	/* List of children */
int     		activechildren = 0;	/* Number of active children */
extern int		maxchildren;		/* Max active children */
static int 		needscan = FALSE;	/* Need to scan children */

static void removechild(CHILD *);
static CHILD *copychild(CHILD *);
static void addchild(CHILD *);
static void readchild(CHILD *);
static pid_t waitproc(int *, int);
static void reap(int);
static void childscan(void);

/*
 * Remove a child that has died (exited) 
 * from the list of active children
 */
static void
removechild(CHILD *child)
{
	CHILD *pc, *prevpc;

	debugmsg(DM_CALL, "removechild(%s, %d, %d) start",
		 child->c_name, child->c_pid, child->c_readfd);

	/*
	 * Find the child in the list
	 */
	for (pc = childlist, prevpc = NULL; pc != NULL; 
	     prevpc = pc, pc = pc->c_next)
		if (pc == child) 
			break;

	if (pc == NULL)
		error("RemoveChild called with bad child %s %d %d",
		      child->c_name, child->c_pid, child->c_readfd);
	else {
		/*
		 * Remove the child
		 */
		sigset_t set, oset;

		sigemptyset(&set);
		sigaddset(&set, SIGCHLD);
		sigprocmask(SIG_BLOCK, &set, &oset);

		if (prevpc != NULL)
			prevpc->c_next = pc->c_next;
		else
			childlist = pc->c_next;

		sigprocmask(SIG_SETMASK, &oset, NULL);

		(void) free(child->c_name);
		--activechildren;
		(void) close(child->c_readfd);
		(void) free(pc);
	}

	debugmsg(DM_CALL, "removechild() end");
}

/*
 * Create a totally new copy of a child.
 */
static CHILD *
copychild(CHILD *child)
{
	CHILD *newc;

	newc = xmalloc(sizeof *newc);

	newc->c_name = xstrdup(child->c_name);
	newc->c_readfd = child->c_readfd;
	newc->c_pid = child->c_pid;
	newc->c_state = child->c_state;
	newc->c_next = NULL;

	return(newc);
}

/*
 * Add a child to the list of children.
 */			
static void
addchild(CHILD *child)
{
	CHILD *pc;

	debugmsg(DM_CALL, "addchild() start\n");

	pc = copychild(child);
	pc->c_next = childlist;
	childlist = pc;

	++activechildren;

	debugmsg(DM_MISC,
		 "addchild() created '%s' pid %d fd %d (active=%d)\n",
		 child->c_name, child->c_pid, child->c_readfd, activechildren);
}

/*
 * Read input from a child process.
 */
static void
readchild(CHILD *child)
{
	char rbuf[BUFSIZ];
	ssize_t amt;

	debugmsg(DM_CALL, "[readchild(%s, %d, %d) start]", 
		 child->c_name, child->c_pid, child->c_readfd);

	/*
	 * Check that this is a valid child.
	 */
	if (child->c_name == NULL || child->c_readfd <= 0) {
		debugmsg(DM_MISC, "[readchild(%s, %d, %d) bad child]",
			 child->c_name, child->c_pid, child->c_readfd);
		return;
	}

	/*
	 * Read from child and display the result.
	 */
	while ((amt = read(child->c_readfd, rbuf, sizeof(rbuf))) > 0) {
		/* XXX remove these debug calls */
		debugmsg(DM_MISC, "[readchild(%s, %d, %d) got %zd bytes]", 
			 child->c_name, child->c_pid, child->c_readfd, amt);

		(void) xwrite(fileno(stdout), rbuf, amt);

		debugmsg(DM_MISC, "[readchild(%s, %d, %d) write done]",
			 child->c_name, child->c_pid, child->c_readfd);
	}

	debugmsg(DM_MISC, "readchild(%s, %d, %d) done: amt = %zd errno = %d\n",
		 child->c_name, child->c_pid, child->c_readfd, amt, errno);

	/* 
	 * See if we've reached EOF 
	 */
	if (amt == 0)
		debugmsg(DM_MISC, "readchild(%s, %d, %d) at EOF\n",
			 child->c_name, child->c_pid, child->c_readfd);
}

/*
 * Wait for processes to exit.  If "block" is true, then we block
 * until a process exits.  Otherwise, we return right away.  If
 * a process does exit, then the pointer "statval" is set to the
 * exit status of the exiting process, if statval is not NULL.
 */
static pid_t
waitproc(int *statval, int block)
{
	int status;
	pid_t pid;
	int exitval;

	debugmsg(DM_CALL, "waitproc() %s, active children = %d...\n", 
		 (block) ? "blocking" : "nonblocking", activechildren);

	pid = waitpid(-1, &status, (block) ? 0 : WNOHANG);

	exitval = WEXITSTATUS(status);

	if (pid > 0 && exitval != 0) {
		nerrs++;
		debugmsg(DM_MISC, 
			 "Child process %d exited with status %d.\n",
			 pid, exitval);
	}

	if (statval)
		*statval = exitval;

	debugmsg(DM_CALL, "waitproc() done (activechildren = %d)\n", 
		 activechildren);

	return(pid);
}

/*
 * Check to see if any children have exited, and if so, read any unread
 * input and then remove the child from the list of children.
 */
static void
reap(int dummy)
{
	CHILD *pc;
	int save_errno = errno;
	int status = 0;
	pid_t pid;

	debugmsg(DM_CALL, "reap() called\n");

	/*
	 * Reap every child that has exited.  Break out of the
	 * loop as soon as we run out of children that have
	 * exited so far.
	 */
	for ( ; ; ) {
		/*
		 * Do a non-blocking check for exiting processes
		 */
		pid = waitproc(&status, FALSE);
		debugmsg(DM_MISC, 
			 "reap() pid = %d status = %d activechildren=%d\n",
			 pid, status, activechildren);

		/*
		 * See if a child really exited
		 */
		if (pid == 0)
			break;
		if (pid < 0) {
			if (errno != ECHILD)
				error("Wait failed: %s", SYSERR);
			break;
		}

		/*
		 * Find the process (pid) and mark it as dead.
		 */
		for (pc = childlist; pc; pc = pc->c_next)
			if (pc->c_pid == pid) {
				needscan = TRUE;
				pc->c_state = PSdead;
			}

	}

	/*
	 * Reset signals
	 */
	(void) signal(SIGCHLD, reap);

	debugmsg(DM_CALL, "reap() done\n");
	errno = save_errno;
}

/*
 * Scan the children list to find the child that just exited, 
 * read any unread input, then remove it from the list of active children.
 */
static void
childscan(void)
{
	CHILD *pc, *nextpc;
	
	debugmsg(DM_CALL, "childscan() start");

	for (pc = childlist; pc; pc = nextpc) {
		nextpc = pc->c_next;
		if (pc->c_state == PSdead) {
			readchild(pc);
			removechild(pc);
		}
	}

	needscan = FALSE;
	debugmsg(DM_CALL, "childscan() end");
}

/*
 *
 * Wait for children to send output for us to read.
 *
 */
void
waitup(void)
{
	int count;
	CHILD *pc;
	fd_set *rchildfdsp = NULL;
	int rchildfdsn = 0;

	debugmsg(DM_CALL, "waitup() start\n");

	if (needscan)
		childscan();

	if (activechildren <= 0)
		return;

	/*
	 * Set up which children we want to select() on.
	 */
	for (pc = childlist; pc; pc = pc->c_next)
		if (pc->c_readfd > rchildfdsn)
			rchildfdsn = pc->c_readfd;
	rchildfdsp = xcalloc(howmany(rchildfdsn+1, NFDBITS), sizeof(fd_mask));

	for (pc = childlist; pc; pc = pc->c_next)
		if (pc->c_readfd > 0) {
			debugmsg(DM_MISC, "waitup() select on %d (%s)\n",
				 pc->c_readfd, pc->c_name);
			FD_SET(pc->c_readfd, rchildfdsp);
		}

	/*
	 * Actually call select()
	 */
	/* XXX remove debugmsg() calls */
	debugmsg(DM_MISC, "waitup() Call select(), activechildren=%d\n", 
		 activechildren);

	count = select(rchildfdsn+1, rchildfdsp, NULL, NULL, NULL);

	debugmsg(DM_MISC, "waitup() select returned %d activechildren = %d\n", 
		 count, activechildren);

	/*
	 * select() will return count < 0 and errno == EINTR when
	 * there are no active children left.
	 */
	if (count < 0) {
		if (errno != EINTR)
			error("Select failed reading children input: %s", 
			      SYSERR);
		free(rchildfdsp);
		return;
	}

	/*
	 * This should never happen.
	 */
	if (count == 0) {
		error("Select returned an unexpected count of 0.");
		free(rchildfdsp);
		return;
	}

	/*
	 * Go through the list of children and read from each child
	 * which select() detected as ready for reading.
	 */
	for (pc = childlist; pc && count > 0; pc = pc->c_next) {
		/* 
		 * Make sure child still exists 
		 */
		if (pc->c_name && kill(pc->c_pid, 0) < 0 && 
		    errno == ESRCH) {
			debugmsg(DM_MISC, 
				 "waitup() proc %d (%s) died unexpectedly!",
				 pc->c_pid, pc->c_name);
			pc->c_state = PSdead;
			needscan = TRUE;
		}

		if (pc->c_name == NULL ||
		    !FD_ISSET(pc->c_readfd, rchildfdsp))
			continue;

		readchild(pc);
		--count;
	}
	free(rchildfdsp);

	debugmsg(DM_CALL, "waitup() end\n");
}

/*
 * Enable non-blocking I/O mode.
 */
static int
setnonblocking(int fd)
{
	int	mode;

	if ((mode = fcntl(fd, F_GETFL)) < 0)
		return (-1);
	if (mode & O_NONBLOCK)
		return (0);
	return (fcntl(fd, F_SETFL, mode | O_NONBLOCK));
}

/*
 * Spawn (create) a new child process for "cmd".
 */
int
spawn(struct cmd *cmd, struct cmd *cmdlist)
{
	pid_t pid;
	int fildes[2];
	char *childname = cmd->c_name;

	if (pipe(fildes) < 0) {
		error("Cannot create pipe for %s: %s", childname, SYSERR);
		return(-1);
	}

	pid = fork();
	if (pid == (pid_t)-1) {
		error("Cannot spawn child for %s: fork failed: %s", 
		      childname, SYSERR);
		return(-1);
	} else if (pid > 0) {
		/*
		 * Parent
		 */
		static CHILD newchild;

		/* Receive notification when the child exits */
		(void) signal(SIGCHLD, reap);

		/* Settup the new child */
		newchild.c_next = NULL;
		newchild.c_name = childname;
		newchild.c_readfd = fildes[PIPE_READ];
		newchild.c_pid = pid;
		newchild.c_state = PSrunning;

		/* We're not going to write to the child */
		(void) close(fildes[PIPE_WRITE]);

		/* Set non-blocking I/O */
		if (setnonblocking(newchild.c_readfd) < 0) {
			error("Set nonblocking I/O failed: %s", SYSERR);
			return(-1);
		}

		/* Add new child to child list */
		addchild(&newchild);

		/* Mark all other entries for this host as assigned */
		markassigned(cmd, cmdlist);

		debugmsg(DM_CALL,
			 "spawn() Forked child %d for host %s active = %d\n",
			 pid, childname, activechildren);
		return(pid);
	} else {
		/* 
		 * Child 
		 */

		/* We're not going to read from our parent */
		(void) close(fildes[PIPE_READ]);

		/* Make stdout and stderr go to PIPE_WRITE (our parent) */
		if (dup2(fildes[PIPE_WRITE], (int)fileno(stdout)) < 0) {
			error("Cannot duplicate stdout file descriptor: %s", 
			      SYSERR);
			return(-1);
		}
		if (dup2(fildes[PIPE_WRITE], (int)fileno(stderr)) < 0) {
			error("Cannot duplicate stderr file descriptor: %s", 
			      SYSERR);
			return(-1);
		}

		return(0);
	}
}
