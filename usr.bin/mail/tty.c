/*	$OpenBSD: tty.c,v 1.19 2009/10/27 23:59:40 deraadt Exp $	*/
/*	$NetBSD: tty.c,v 1.7 1997/07/09 05:25:46 mikel Exp $	*/

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

/*
 * Mail -- a mail program
 *
 * Generally useful tty stuff.
 */

#include "rcv.h"
#include "extern.h"
#include <sys/ioctl.h>
#include <errno.h>

static	cc_t		c_erase;	/* Current erase char */
static	cc_t		c_kill;		/* Current kill char */
#ifndef TIOCSTI
static	int		ttyset;		/* We must now do erase/kill */
#endif
static	volatile sig_atomic_t	ttysignal;	/* Interrupted by a signal? */

/*
 * Read all relevant header fields.
 */
int
grabh(struct header *hp, int gflags)
{
	struct termios ttybuf;
#ifndef TIOCSTI
	struct sigaction savequit;
#else
# ifdef	TIOCEXT
	int extproc;
	int flag;
# endif /* TIOCEXT */
#endif
	struct sigaction savetstp;
	struct sigaction savettou;
	struct sigaction savettin;
	struct sigaction act;
	char *s;
	int error;

	sigemptyset(&act.sa_mask);
	act.sa_flags = SA_RESTART;
	act.sa_handler = SIG_DFL;
	(void)sigaction(SIGTSTP, &act, &savetstp);
	(void)sigaction(SIGTTOU, &act, &savettou);
	(void)sigaction(SIGTTIN, &act, &savettin);
	error = 1;
#ifndef TIOCSTI
	ttyset = 0;
#endif
	if (tcgetattr(fileno(stdin), &ttybuf) < 0) {
		warn("tcgetattr");
		return(-1);
	}
	c_erase = ttybuf.c_cc[VERASE];
	c_kill = ttybuf.c_cc[VKILL];
#ifndef TIOCSTI
	ttybuf.c_cc[VERASE] = 0;
	ttybuf.c_cc[VKILL] = 0;
	act.sa_handler = SIG_IGN;
	if (sigaction(SIGQUIT, &act, &savequit) == 0 &&
	    savequit.sa_handler == SIG_DFL)
		(void)sigaction(SIGQUIT, &savequit, NULL);
#else
# ifdef	TIOCEXT
	extproc = ((ttybuf.c_lflag & EXTPROC) ? 1 : 0);
	if (extproc) {
		flag = 0;
		if (ioctl(fileno(stdin), TIOCEXT, &flag) < 0)
			warn("TIOCEXT: off");
	}
# endif /* TIOCEXT */
#endif
	if (gflags & GTO) {
#ifndef TIOCSTI
		if (!ttyset && hp->h_to != NULL)
			ttyset++, tcsetattr(fileno(stdin), TCSADRAIN, &ttybuf);
#endif
		s = readtty("To: ", detract(hp->h_to, 0));
		if (s == NULL)
			goto out;
		hp->h_to = extract(s, GTO);
	}
	if (gflags & GSUBJECT) {
#ifndef TIOCSTI
		if (!ttyset && hp->h_subject != NULL)
			ttyset++, tcsetattr(fileno(stdin), TCSADRAIN, &ttybuf);
#endif
		s = readtty("Subject: ", hp->h_subject);
		if (s == NULL)
			goto out;
		hp->h_subject = s;
	}
	if (gflags & GCC) {
#ifndef TIOCSTI
		if (!ttyset && hp->h_cc != NULL)
			ttyset++, tcsetattr(fileno(stdin), TCSADRAIN, &ttybuf);
#endif
		s = readtty("Cc: ", detract(hp->h_cc, 0));
		if (s == NULL)
			goto out;
		hp->h_cc = extract(s, GCC);
	}
	if (gflags & GBCC) {
#ifndef TIOCSTI
		if (!ttyset && hp->h_bcc != NULL)
			ttyset++, tcsetattr(fileno(stdin), TCSADRAIN, &ttybuf);
#endif
		s = readtty("Bcc: ", detract(hp->h_bcc, 0));
		if (s == NULL)
			goto out;
		hp->h_bcc = extract(s, GBCC);
	}
	error = 0;
out:
	(void)sigaction(SIGTSTP, &savetstp, NULL);
	(void)sigaction(SIGTTOU, &savettou, NULL);
	(void)sigaction(SIGTTIN, &savettin, NULL);
#ifndef TIOCSTI
	ttybuf.c_cc[VERASE] = c_erase;
	ttybuf.c_cc[VKILL] = c_kill;
	if (ttyset)
		tcsetattr(fileno(stdin), TCSADRAIN, &ttybuf);
	(void)sigaction(SIGQUIT, &savequit, NULL);
#else
# ifdef	TIOCEXT
	if (extproc) {
		flag = 1;
		if (ioctl(fileno(stdin), TIOCEXT, &flag) < 0)
			warn("TIOCEXT: on");
	}
# endif /* TIOCEXT */
#endif
	return(error);
}

/*
 * Read up a header from standard input.
 * The source string has the preliminary contents to
 * be read.
 *
 */
char *
readtty(char *pr, char *src)
{
	struct sigaction act, saveint;
	char ch, canonb[BUFSIZ];
	char *cp, *cp2;
	sigset_t oset;
	int c;

	fputs(pr, stdout);
	fflush(stdout);
	if (src != NULL && strlen(src) > sizeof(canonb) - 2) {
		puts("too long to edit");
		return(src);
	}
#ifndef TIOCSTI
	if (src != NULL)
		cp = copy(src, canonb);	/* safe, bounds checked above */
	else
		cp = copy("", canonb);
	fputs(canonb, stdout);
	fflush(stdout);
#else
	cp = src == NULL ? "" : src;
	while ((c = *cp++) != '\0') {
		if ((c_erase != _POSIX_VDISABLE && c == c_erase) ||
		    (c_kill != _POSIX_VDISABLE && c == c_kill)) {
			ch = '\\';
			ioctl(0, TIOCSTI, &ch);
		}
		ch = c;
		ioctl(0, TIOCSTI, &ch);
	}
	cp = canonb;
	*cp = 0;
#endif
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;		/* Note: will not restart syscalls */
	act.sa_handler = ttyint;
	(void)sigaction(SIGINT, &act, &saveint);
	act.sa_handler = ttystop;
	(void)sigaction(SIGTSTP, &act, NULL);
	(void)sigaction(SIGTTOU, &act, NULL);
	(void)sigaction(SIGTTIN, &act, NULL);
	(void)sigprocmask(SIG_UNBLOCK, &intset, &oset);
	clearerr(stdin);
	memset(cp, 0, canonb + sizeof(canonb) - cp);
	for (cp2 = cp; cp2 < canonb + sizeof(canonb) - 1; ) {
		c = getc(stdin);
		switch (ttysignal) {
			case SIGINT:
				ttysignal = 0;
				cp2 = NULL;
				c = EOF;
				/* FALLTHROUGH */
			case 0:
				break;
			default:
				ttysignal = 0;
				goto redo;
		}
		if (c == EOF || c == '\n')
			break;
		*cp2++ = c;
	}
	act.sa_handler = SIG_DFL;
	sigemptyset(&act.sa_mask);
	act.sa_flags = SA_RESTART;
	(void)sigprocmask(SIG_SETMASK, &oset, NULL);
	(void)sigaction(SIGTSTP, &act, NULL);
	(void)sigaction(SIGTTOU, &act, NULL);
	(void)sigaction(SIGTTIN, &act, NULL);
	(void)sigaction(SIGINT, &saveint, NULL);
	if (cp2 == NULL)
		return(NULL);			/* user hit ^C */
	*cp2 = '\0';
	if (c == EOF && ferror(stdin)) {
redo:
		cp = strlen(canonb) > 0 ? canonb : NULL;
		clearerr(stdin);
		/* XXX - make iterative, not recursive */
		return(readtty(pr, cp));
	}
#ifndef TIOCSTI
	if (cp == NULL || *cp == '\0')
		return(src);
	cp2 = cp;
	if (!ttyset)
		return(strlen(canonb) > 0 ? savestr(canonb) : NULL);
	while (*cp != '\0') {
		c = *cp++;
		if (c_erase != _POSIX_VDISABLE && c == c_erase) {
			if (cp2 == canonb)
				continue;
			if (cp2[-1] == '\\') {
				cp2[-1] = c;
				continue;
			}
			cp2--;
			continue;
		}
		if (c_kill != _POSIX_VDISABLE && c == c_kill) {
			if (cp2 == canonb)
				continue;
			if (cp2[-1] == '\\') {
				cp2[-1] = c;
				continue;
			}
			cp2 = canonb;
			continue;
		}
		*cp2++ = c;
	}
	*cp2 = '\0';
#endif
	if (equal("", canonb))
		return("");
	return(savestr(canonb));
}

/*
 * Receipt continuation.
 */
void
ttystop(int s)
{
	struct sigaction act, oact;
	sigset_t nset;
	int save_errno;

	/*
	 * Save old handler and set to default.
	 * Unblock receipt of 's' and then resend it.
	 */
	save_errno = errno;
	(void)sigemptyset(&act.sa_mask);
	act.sa_flags = SA_RESTART;
	act.sa_handler = SIG_DFL;
	(void)sigaction(s, &act, &oact);
	(void)sigemptyset(&nset);
	(void)sigaddset(&nset, s);
	(void)sigprocmask(SIG_UNBLOCK, &nset, NULL);
	(void)kill(0, s);
	(void)sigprocmask(SIG_BLOCK, &nset, NULL);
	(void)sigaction(s, &oact, NULL);
	ttysignal = s;
	errno = save_errno;
}

/*ARGSUSED*/
void
ttyint(int s)
{

	ttysignal = s;
}
