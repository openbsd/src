/*	$OpenBSD: tty.c,v 1.7 1997/07/22 18:54:45 millert Exp $	*/
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
static char sccsid[] = "@(#)tty.c	8.2 (Berkeley) 4/20/95";
#else
static char rcsid[] = "$OpenBSD: tty.c,v 1.7 1997/07/22 18:54:45 millert Exp $";
#endif
#endif /* not lint */

/*
 * Mail -- a mail program
 *
 * Generally useful tty stuff.
 */

#include "rcv.h"
#include "extern.h"
#include <sys/ioctl.h>

static	cc_t		c_erase;	/* Current erase char */
static	cc_t		c_kill;		/* Current kill char */
static	sigjmp_buf	rewrite;	/* Place to go when continued */
static	sigjmp_buf	intjmp;		/* Place to go when interrupted */
#ifndef TIOCSTI
static	int		ttyset;		/* We must now do erase/kill */
#endif

/*
 * Read all relevant header fields.
 */

int
grabh(hp, gflags)
	struct header *hp;
	int gflags;
{
	struct termios ttybuf;
	sig_t saveint;
#ifndef TIOCSTI
	sig_t savequit;
#else
	int extproc, flag;
#endif
	sig_t savetstp;
	sig_t savettou;
	sig_t savettin;
	int errs = 0;
#ifdef __GNUC__
	/* Avoid siglongjmp clobbering */
#ifdef TIOCSTI
	(void)&extproc;
#endif
	(void)&saveint;
	(void)&errs;
#endif

	savetstp = signal(SIGTSTP, SIG_DFL);
	savettou = signal(SIGTTOU, SIG_DFL);
	savettin = signal(SIGTTIN, SIG_DFL);
	errs = 0;
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
	if ((saveint = signal(SIGINT, SIG_IGN)) == SIG_DFL)
		signal(SIGINT, SIG_DFL);
	if ((savequit = signal(SIGQUIT, SIG_IGN)) == SIG_DFL)
		signal(SIGQUIT, SIG_DFL);
#else
# ifdef	TIOCEXT
	extproc = ((ttybuf.c_lflag & EXTPROC) ? 1 : 0);
	if (extproc) {
		flag = 0;
		if (ioctl(fileno(stdin), TIOCEXT, &flag) < 0)
			warn("TIOCEXT: off");
	}
# endif	/* TIOCEXT */
	if (sigsetjmp(intjmp, 1)) {
		errs = SIGINT;
		goto out;
	}
	saveint = signal(SIGINT, ttyint);
#endif
	if (gflags & GTO) {
#ifndef TIOCSTI
		if (!ttyset && hp->h_to != NIL)
			ttyset++, tcsetattr(fileno(stdin), TCSADRAIN, &ttybuf);
#endif
		hp->h_to =
			extract(readtty("To: ", detract(hp->h_to, 0)), GTO);
	}
	if (gflags & GSUBJECT) {
#ifndef TIOCSTI
		if (!ttyset && hp->h_subject != NULL)
			ttyset++, tcsetattr(fileno(stdin), TCSADRAIN, &ttybuf);
#endif
		hp->h_subject = readtty("Subject: ", hp->h_subject);
	}
	if (gflags & GCC) {
#ifndef TIOCSTI
		if (!ttyset && hp->h_cc != NIL)
			ttyset++, tcsetattr(fileno(stdin), TCSADRAIN, &ttybuf);
#endif
		hp->h_cc =
			extract(readtty("Cc: ", detract(hp->h_cc, 0)), GCC);
	}
	if (gflags & GBCC) {
#ifndef TIOCSTI
		if (!ttyset && hp->h_bcc != NIL)
			ttyset++, tcsetattr(fileno(stdin), TCSADRAIN, &ttybuf);
#endif
		hp->h_bcc =
			extract(readtty("Bcc: ", detract(hp->h_bcc, 0)), GBCC);
	}
out:
	signal(SIGTSTP, savetstp);
	signal(SIGTTOU, savettou);
	signal(SIGTTIN, savettin);
#ifndef TIOCSTI
	ttybuf.c_cc[VERASE] = c_erase;
	ttybuf.c_cc[VKILL] = c_kill;
	if (ttyset)
		tcsetattr(fileno(stdin), TCSADRAIN, &ttybuf);
	signal(SIGQUIT, savequit);
#else
# ifdef	TIOCEXT
	if (extproc) {
		flag = 1;
		if (ioctl(fileno(stdin), TIOCEXT, &flag) < 0)
			warn("TIOCEXT: on");
	}
# endif	/* TIOCEXT */
#endif
	signal(SIGINT, saveint);
	return(errs);
}

/*
 * Read up a header from standard input.
 * The source string has the preliminary contents to
 * be read.
 *
 */

char *
readtty(pr, src)
	char pr[], src[];
{
	char ch, canonb[BUFSIZ];
	int c;
	char *cp, *cp2;
#if __GNUC__
	/* Avoid siglongjmp clobbering */
	(void)&c;
	(void)&cp2;
#endif

	fputs(pr, stdout);
	fflush(stdout);
	if (src != NULL && strlen(src) > BUFSIZ - 2) {
		puts("too long to edit");
		return(src);
	}
#ifndef TIOCSTI
	if (src != NULL)
		cp = copy(src, canonb);
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
	cp2 = cp;
	while (cp2 < canonb + BUFSIZ)
		*cp2++ = 0;
	cp2 = cp;
	if (sigsetjmp(rewrite, 1))
		goto redo;
	signal(SIGTSTP, ttystop);
	signal(SIGTTOU, ttystop);
	signal(SIGTTIN, ttystop);
	clearerr(stdin);
	while (cp2 < canonb + BUFSIZ) {
		c = getc(stdin);
		if (c == EOF || c == '\n')
			break;
		*cp2++ = c;
	}
	*cp2 = 0;
	signal(SIGTSTP, SIG_DFL);
	signal(SIGTTOU, SIG_DFL);
	signal(SIGTTIN, SIG_DFL);
	if (c == EOF && ferror(stdin)) {
redo:
		cp = strlen(canonb) > 0 ? canonb : NULL;
		clearerr(stdin);
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
		return(NULL);
	return(savestr(canonb));
}

/*
 * Receipt continuation.
 */
void
ttystop(s)
	int s;
{
	sig_t old_action = signal(s, SIG_DFL);
	sigset_t nset;

	sigemptyset(&nset);
	sigaddset(&nset, s);
	sigprocmask(SIG_UNBLOCK, &nset, NULL);
	kill(0, s);
	sigprocmask(SIG_BLOCK, &nset, NULL);
	signal(s, old_action);
	siglongjmp(rewrite, 1);
}

/*ARGSUSED*/
void
ttyint(s)
	int s;
{
	siglongjmp(intjmp, 1);
}
