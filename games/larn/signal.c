/*	$OpenBSD: signal.c,v 1.3 1998/09/15 05:12:33 pjanzen Exp $	*/
/*	$NetBSD: signal.c,v 1.6 1997/10/18 20:03:50 christos Exp $	*/

/* "Larn is copyrighted 1986 by Noah Morgan.\n" */

#ifndef lint
static char rcsid[] = "$OpenBSD: signal.c,v 1.3 1998/09/15 05:12:33 pjanzen Exp $";
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

#define BIT(a) (1<<((a)-1))

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
{				/* control Y	 */
	if (nosignal)
		return;		/* nothing if inhibited */
	lcreat((char *) 0);
	clearvt100();
	lflush();
	signal(SIGTSTP, SIG_DFL);
#ifdef SIGVTALRM
	/*
	 * looks like BSD4.2 or higher - must clr mask for signal to take
	 * effect
	 */
	sigsetmask(sigblock(0) & ~BIT(SIGTSTP));
#endif
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
