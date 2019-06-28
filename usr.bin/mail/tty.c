/*	$OpenBSD: tty.c,v 1.22 2019/06/28 13:35:02 deraadt Exp $	*/
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
#include <fcntl.h>

#define	TABWIDTH	8

struct tty {
	int	 fdin;
	int	 fdout;
	int	 flags;
#define	TTY_ALTWERASE	0x1
#define	TTY_ERR		0x2
	cc_t	*keys;
	char	*buf;
	size_t	 size;
	size_t	 len;
	size_t	 cursor;
};

static void	tty_flush(struct tty *);
static int	tty_getc(struct tty *);
static int	tty_insert(struct tty *, int, int);
static void	tty_putc(struct tty *, int);
static void	tty_reset(struct tty *);
static void	tty_visc(struct tty *, int);

static struct tty		tty;
static	volatile sig_atomic_t	ttysignal;	/* Interrupted by a signal? */

/*
 * Read all relevant header fields.
 */
int
grabh(struct header *hp, int gflags)
{
	struct termios newtio, oldtio;
#ifdef TIOCEXT
	int extproc;
	int flag;
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
	memset(&tty, 0, sizeof(tty));
	tty.fdin = fileno(stdin);
	tty.fdout = fileno(stdout);
	if (tcgetattr(tty.fdin, &oldtio) == -1) {
		warn("tcgetattr");
		return(-1);
	}
	tty.keys = oldtio.c_cc;
	if (oldtio.c_lflag & ALTWERASE)
		tty.flags |= TTY_ALTWERASE;

	newtio = oldtio;
	newtio.c_lflag &= ~(ECHO | ICANON);
	newtio.c_cc[VMIN] = 1;
	newtio.c_cc[VTIME] = 0;
	if (tcsetattr(tty.fdin, TCSADRAIN, &newtio) == -1) {
		warn("tcsetattr");
		return(-1);
	}

#ifdef TIOCEXT
	extproc = ((oldtio.c_lflag & EXTPROC) ? 1 : 0);
	if (extproc) {
		flag = 0;
		if (ioctl(fileno(stdin), TIOCEXT, &flag) == -1)
			warn("TIOCEXT: off");
	}
#endif
	if (gflags & GTO) {
		s = readtty("To: ", detract(hp->h_to, 0));
		if (s == NULL)
			goto out;
		hp->h_to = extract(s, GTO);
	}
	if (gflags & GSUBJECT) {
		s = readtty("Subject: ", hp->h_subject);
		if (s == NULL)
			goto out;
		hp->h_subject = s;
	}
	if (gflags & GCC) {
		s = readtty("Cc: ", detract(hp->h_cc, 0));
		if (s == NULL)
			goto out;
		hp->h_cc = extract(s, GCC);
	}
	if (gflags & GBCC) {
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
#ifdef TIOCEXT
	if (extproc) {
		flag = 1;
		if (ioctl(fileno(stdin), TIOCEXT, &flag) == -1)
			warn("TIOCEXT: on");
	}
#endif
	if (tcsetattr(tty.fdin, TCSADRAIN, &oldtio) == -1)
		warn("tcsetattr");
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
	unsigned char canonb[BUFSIZ];
	char *cp;
	sigset_t oset;
	int c, done;

	memset(canonb, 0, sizeof(canonb));
	tty.buf = canonb;
	tty.size = sizeof(canonb) - 1;

	for (cp = pr; *cp != '\0'; cp++)
		tty_insert(&tty, *cp, 1);
	tty_flush(&tty);
	tty_reset(&tty);

	if (src != NULL && strlen(src) > sizeof(canonb) - 2) {
		puts("too long to edit");
		return(src);
	}
	if (src != NULL) {
		for (cp = src; *cp != '\0'; cp++)
			tty_insert(&tty, *cp, 1);
		tty_flush(&tty);
	}

	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;		/* Note: will not restart syscalls */
	act.sa_handler = ttyint;
	(void)sigaction(SIGINT, &act, &saveint);
	act.sa_handler = ttystop;
	(void)sigaction(SIGTSTP, &act, NULL);
	(void)sigaction(SIGTTOU, &act, NULL);
	(void)sigaction(SIGTTIN, &act, NULL);
	(void)sigprocmask(SIG_UNBLOCK, &intset, &oset);
	for (;;) {
		c = tty_getc(&tty);
		switch (ttysignal) {
			case SIGINT:
				tty_visc(&tty, '\003');	/* output ^C */
				/* FALLTHROUGH */
			case 0:
				break;
			default:
				ttysignal = 0;
				goto redo;
		}
		if (c == 0) {
			done = 1;
		} else if (c == '\n') {
			tty_putc(&tty, c);
			done = 1;
		} else {
			done = tty_insert(&tty, c, 0);
			tty_flush(&tty);
		}
		if (done)
			break;
	}
	act.sa_handler = SIG_DFL;
	sigemptyset(&act.sa_mask);
	act.sa_flags = SA_RESTART;
	(void)sigprocmask(SIG_SETMASK, &oset, NULL);
	(void)sigaction(SIGTSTP, &act, NULL);
	(void)sigaction(SIGTTOU, &act, NULL);
	(void)sigaction(SIGTTIN, &act, NULL);
	(void)sigaction(SIGINT, &saveint, NULL);
	if (tty.flags & TTY_ERR) {
		if (ttysignal == SIGINT) {
			ttysignal = 0;
			return(NULL);	/* user hit ^C */
		}

redo:
		cp = strlen(canonb) > 0 ? canonb : NULL;
		/* XXX - make iterative, not recursive */
		return(readtty(pr, cp));
	}
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

static void
tty_flush(struct tty *t)
{
	size_t	i, len;
	int	c;

	if (t->cursor < t->len) {
		for (; t->cursor < t->len; t->cursor++)
			tty_visc(t, t->buf[t->cursor]);
	} else if (t->cursor > t->len) {
		len = t->cursor - t->len;
		for (i = len; i > 0; i--) {
			c = t->buf[--t->cursor];
			if (c == '\t')
				len += TABWIDTH - 1;
			else if (iscntrl(c))
				len++;	/* account for leading ^ */
		}
		for (i = 0; i < len; i++)
			tty_putc(t, '\b');
		for (i = 0; i < len; i++)
			tty_putc(t, ' ');
		for (i = 0; i < len; i++)
			tty_putc(t, '\b');
		t->cursor = t->len;
	}

	t->buf[t->len] = '\0';
}

static int
tty_getc(struct tty *t)
{
	ssize_t		n;
	unsigned char	c;

	n = read(t->fdin, &c, 1);
	switch (n) {
	case -1:
		t->flags |= TTY_ERR;
		/* FALLTHROUGH */
	case 0:
		return 0;
	default:
		return c & 0x7f;
	}
}

static int
tty_insert(struct tty *t, int c, int nocntrl)
{
	const unsigned char	*ws = " \t";

	if (CCEQ(t->keys[VERASE], c)) {
		if (nocntrl)
			return 0;
		if (t->len > 0)
			t->len--;
	} else if (CCEQ(t->keys[VWERASE], c)) {
		if (nocntrl)
			return 0;
		for (; t->len > 0; t->len--)
			if (strchr(ws, t->buf[t->len - 1]) == NULL
			    && ((t->flags & TTY_ALTWERASE) == 0
				    || isalpha(t->buf[t->len - 1])))
				break;
		for (; t->len > 0; t->len--)
			if (strchr(ws, t->buf[t->len - 1]) != NULL
			    || ((t->flags & TTY_ALTWERASE)
				    && !isalpha(t->buf[t->len - 1])))
				break;
	} else if (CCEQ(t->keys[VKILL], c)) {
		if (nocntrl)
			return 0;
		t->len = 0;
	} else {
		if (t->len == t->size)
			return 1;
		t->buf[t->len++] = c;
	}

	return 0;
}

static void
tty_putc(struct tty *t, int c)
{
	unsigned char	cc = c;

	write(t->fdout, &cc, 1);
}

static void
tty_reset(struct tty *t)
{
	memset(t->buf, 0, t->len);
	t->len = t->cursor = 0;
}

static void
tty_visc(struct tty *t, int c)
{
	int	i;

	if (c == '\t') {
		for (i = 0; i < TABWIDTH; i++)
			tty_putc(t, ' ');
	} else if (iscntrl(c)) {
		tty_putc(t, '^');
		if (c == 0x7F)
			tty_putc(t, '?');
		else
			tty_putc(t, (c | 0x40));
	} else {
		tty_putc(t, c);
	}
}
