#ifndef lint
static char rcsid[] = "$NetBSD: signal.c,v 1.5 1995/12/21 11:27:51 mycroft Exp $";
#endif /* not lint */

#include <signal.h>
#include "header.h"			/* "Larn is copyrighted 1986 by Noah Morgan.\n" */
#include <string.h>

#define BIT(a) (1<<((a)-1))
extern char savefilename[],wizard,predostuff,nosignal;
static s2choose()	/* text to be displayed if ^C during intro screen */
	{
	cursor(1,24); lprcat("Press "); setbold(); lprcat("return"); resetbold();
	lprcat(" to continue: ");   lflush(); 
	}

static void
cntlc()	/* what to do for a ^C */
	{
	if (nosignal) return;	/* don't do anything if inhibited */
	signal(SIGQUIT,SIG_IGN);	signal(SIGINT,SIG_IGN);
	quit(); if (predostuff==1) s2choose(); else showplayer();
	lflush();
	signal(SIGQUIT,cntlc);	signal(SIGINT,cntlc);
	}

/*
 *	subroutine to save the game if a hangup signal
 */
static void
sgam()
	{
	savegame(savefilename);  wizard=1;  died(-257); /* hangup signal */
	}

#ifdef SIGTSTP
static void
tstop() /* control Y	*/
	{
	if (nosignal)   return;  /* nothing if inhibited */
	lcreat((char*)0);  clearvt100();	lflush();	  signal(SIGTSTP,SIG_DFL);
#ifdef SIGVTALRM
	/* looks like BSD4.2 or higher - must clr mask for signal to take effect*/
	sigsetmask(sigblock(0)& ~BIT(SIGTSTP));
#endif
	kill(getpid(),SIGTSTP);

	setupvt100();  signal(SIGTSTP,tstop);
	if (predostuff==1) s2choose(); else drawscreen();
	showplayer();	lflush();
	}
#endif SIGTSTP

/*
 *	subroutine to issue the needed signal traps  called from main()
 */
static void sigpanic();
sigsetup()
	{
	signal(SIGQUIT, cntlc); 		signal(SIGINT,  cntlc); 
	signal(SIGKILL, SIG_IGN);		signal(SIGHUP,  sgam);
	signal(SIGILL,  sigpanic);		signal(SIGTRAP, sigpanic);
	signal(SIGIOT,  sigpanic);		signal(SIGEMT,  sigpanic);
	signal(SIGFPE,  sigpanic);		signal(SIGBUS,  sigpanic);
	signal(SIGSEGV, sigpanic);		signal(SIGSYS,  sigpanic);
	signal(SIGPIPE, sigpanic);		signal(SIGTERM, sigpanic);
#ifdef SIGTSTP
	signal(SIGTSTP,tstop);		signal(SIGSTOP,tstop);
#endif SIGTSTP
	}

/*
 *	routine to process a fatal error signal
 */
static void
sigpanic(sig)
	int sig;
	{
	char buf[128];
	signal(sig,SIG_DFL);
	sprintf(buf,"\nLarn - Panic! Signal %d received [SIG%s]",sig,sys_signame[sig]);
	write(2,buf,strlen(buf));  sleep(2);
	sncbr();
	savegame(savefilename); 
	kill(getpid(),sig); /* this will terminate us */
	}
