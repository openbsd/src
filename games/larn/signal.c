/*	$OpenBSD: signal.c,v 1.4 2001/09/04 23:35:57 millert Exp $	*/
/*	$NetBSD: signal.c,v 1.6 1997/10/18 20:03:50 christos Exp $	*/

/* "Larn is copyrighted 1986 by Noah Morgan.\n" */

#ifndef lint
static char rcsid[] = "$OpenBSD: signal.c,v 1.4 2001/09/04 23:35:57 millert Exp $";
#endif	/* not lint */

#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include "header.h"
#include "extern.h"

static void s2choose __P((void));
static void cntlc __P((int));
static void sgam __P((int));
static void tstop __P((int));
static void sigpanic __P((int));

static void
s2choose()
{				/* text to be displayed if ^C during intro
				 * screen */
	cursor(1, 24);
	lprcat("Press ");
	setbold();
	lprcat("return");
	resetbold();
	lprcat(" to continue: ");
	lflush();
}

static void
cntlc(n)
	int n;
{				/* what to do for a ^C */
	if (nosignal)
		return;		/* don't do anything if inhibited */
	signal(SIGQUIT, SIG_IGN);
	signal(SIGINT, SIG_IGN);
	quit();
	if (predostuff == 1)
		s2choose();
	else
		showplayer();
	lflush();
	signal(SIGQUIT, cntlc);
	signal(SIGINT, cntlc);
}

/*
 *	subroutine to save the game if a hangup signal
 */
static void
sgam(n)
	int n;
{
	savegame(savefilename);
	wizard = 1;
	died(-257);		/* hangup signal */
}

#ifdef SIGTSTP
static void
tstop(n)
	int n;
{				/* control Z	 */
	if (nosignal)
		return;		/* nothing if inhibited */
	lcreat((char *) 0);
	clearvt100();
	lflush();
	signal(SIGTSTP, SIG_DFL);
	if (n == SIGTSTP) {
		sigset_t mask;

		/*
		 * We have to unblock SIGTSTP for the kill() below to
		 * have any effect.
		 */
		sigemptyset(&mask);
		sigaddset(&mask, SIGTSTP);
		sigprocmask(SIG_UNBLOCK, &mask, NULL);
	}
	kill(getpid(), SIGTSTP);

	setupvt100();
	signal(SIGTSTP, tstop);
	if (predostuff == 1)
		s2choose();
	else
		drawscreen();
	showplayer();
	lflush();
}
#endif	/* SIGTSTP */

/*
 *	subroutine to issue the needed signal traps called from main()
 */
void
sigsetup()
{
	signal(SIGQUIT, cntlc);
	signal(SIGINT, cntlc);
	signal(SIGKILL, SIG_IGN);
	signal(SIGHUP, sgam);
	signal(SIGILL, sigpanic);
	signal(SIGTRAP, sigpanic);
	signal(SIGIOT, sigpanic);
	signal(SIGEMT, sigpanic);
	signal(SIGFPE, sigpanic);
	signal(SIGBUS, sigpanic);
	signal(SIGSEGV, sigpanic);
	signal(SIGSYS, sigpanic);
	signal(SIGPIPE, sigpanic);
	signal(SIGTERM, sigpanic);
#ifdef SIGTSTP
	signal(SIGTSTP, tstop);
	signal(SIGSTOP, tstop);
#endif	/* SIGTSTP */
}

/*
 *	routine to process a fatal error signal
 */
static void
sigpanic(sig)
	int	sig;
{
	char	buf[128];

	signal(sig, SIG_DFL);
	sprintf(buf, "\nLarn - Panic! Signal %d received [SIG%s]", sig, sys_signame[sig]);
	write(2, buf, strlen(buf));
	sleep(2);
	sncbr();
	savegame(savefilename);
	kill(getpid(), sig);	/* this will terminate us */
}
