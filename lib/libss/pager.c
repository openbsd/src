/*	$OpenBSD: pager.c,v 1.2 1998/06/03 16:20:30 deraadt Exp $	*/

/*-
 * Copyright 1987, 1988 by the Student Information Processing Board
 *	of the Massachusetts Institute of Technology
 *
 * Permission to use, copy, modify, and distribute this software
 * and its documentation for any purpose and without fee is
 * hereby granted, provided that the above copyright notice
 * appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation,
 * and that the names of M.I.T. and the M.I.T. S.I.P.B. not be
 * used in advertising or publicity pertaining to distribution
 * of the software without specific, written prior permission.
 * M.I.T. and the M.I.T. S.I.P.B. make no representations about
 * the suitability of this software for any purpose.  It is
 * provided "as is" without express or implied warranty.
 */

/*
 * Pager: Routines to create a "more" running out of a particular file
 * descriptor.
 */

#include "ss_internal.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/file.h>
#include <unistd.h>
#include <signal.h>

static char MORE[] = "more";
extern char *_ss_pager_name;
extern char *getenv();
extern int errno;

/*
 * this needs a *lot* of work....
 *
 * run in same process
 * handle SIGINT sensibly
 * allow finer control -- put-page-break-here
 */
void ss_page_stdin();

#ifndef NO_FORK
int
ss_pager_create() 
{
	int filedes[2];
     
	if (pipe(filedes) != 0)
		return(-1);

	switch(fork()) {
	case -1:
		return(-1);
	case 0:
		/*
		 * Child; dup read half to 0, close all but 0, 1, and 2
		 */
		if (dup2(filedes[0], 0) == -1)
			exit(1);
		ss_page_stdin();
	default:
		/*
		 * Parent:  close "read" side of pipe, return
		 * "write" side.
		 */
		(void) close(filedes[0]);
		return(filedes[1]);
	}
}
#else /* don't fork */
int
ss_pager_create()
{
    int fd;
    fd = open("/dev/tty", O_WRONLY, 0);
    return fd;
}
#endif

void
ss_page_stdin()
{
	int i;
#ifndef NPOSIX
	struct sigaction sa;
	sigset_t mask;
#endif
	
	for (i = 3; i < 32; i++)
		(void) close(i);
#ifndef NPOSIX
	memset(&sa, 0, sizeof sa);
	sa.sa_handler = SIG_DFL;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sigaction(SIGINT, &sa, (struct sigaction *)0);
#else
	(void) signal(SIGINT, SIG_DFL);
#endif
	{
#ifndef NPOSIX
		sigemptyset(&mask);
		sigaddset(&mask, SIGINT);
		sigprocmask(SIG_UNBLOCK, &mask, (sigset_t *)0);
#else
		register int mask = sigblock(0);
		mask &= ~sigmask(SIGINT);
		sigsetmask(mask);
#endif
	}
	if (_ss_pager_name == (char *)NULL) {
		if ((_ss_pager_name = getenv("PAGER")) == (char *)NULL)
			_ss_pager_name = MORE;
	}
	(void) execlp(_ss_pager_name, _ss_pager_name, (char *) NULL);
	{
		/* minimal recovery if pager program isn't found */
		char buf[80];
		register int n;
		while ((n = read(0, buf, 80)) > 0)
			write(1, buf, n);
	}
	exit(errno);
}
