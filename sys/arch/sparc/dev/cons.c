/*	$NetBSD: cons.c,v 1.15 1995/07/13 12:02:19 pk Exp $ */

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
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
 *
 *	@(#)cons.c	8.3 (Berkeley) 12/14/93
 */

/*
 * Console (indirect) driver.
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/file.h>
#include <sys/conf.h>

#include <machine/bsd_openprom.h>
#include <machine/psl.h>
#ifdef SUN4
#include <machine/oldmon.h>
#endif

#include "zs.h"

struct	tty *constty = 0;	/* virtual console output device */
struct	tty *fbconstty = 0;	/* tty structure for frame buffer console */
int	rom_console_input;	/* when set, hardclock calls cnrom() */

int	cons_ocount;		/* output byte count */

extern struct promvec *promvec;

/*
 * The output driver may munge the minor number in cons.t_dev.
 */
struct tty cons;		/* rom console tty device */
static void (*fcnstop) __P((struct tty *, int));

static void cnstart __P((struct tty *));
void cnstop __P((struct tty *, int));

static void cnfbstart __P((struct tty *));
static void cnfbstop __P((struct tty *, int));
static void cnfbdma __P((void *));

extern char char_type[];

/*XXX*/static struct tty *
cntty()
{
	return &cons;
}

void
consinit()
{
	register struct tty *tp = &cons;
	register int in, out;
	void zsconsole();

/*XXX*/	cdevsw[0].d_tty = cntty;
	tp->t_dev = makedev(0, 0);	/* /dev/console */
	tp->t_ispeed = tp->t_ospeed = TTYDEF_SPEED;
	tp->t_param = (int (*)(struct tty *, struct termios *))nullop;
	in = *promvec->pv_stdin;
	out = *promvec->pv_stdout;
	switch (in) {

#if NZS > 0
	case PROMDEV_TTYA:
		zsconsole(tp, 0, 0, NULL);
		break;

	case PROMDEV_TTYB:
		zsconsole(tp, 1, 0, NULL);
		break;
#endif

	case PROMDEV_KBD:
		/*
		 * Tell the keyboard driver to direct ASCII input here.
		 */
		kbd_ascii(tp);
		break;

	default:
		rom_console_input = 1;
		printf("unknown console input source %d; using rom\n", in);
		break;
	}
	switch (out) {

#if NZS > 0
	case PROMDEV_TTYA:
		zsconsole(tp, 0, 1, &fcnstop);
		break;

	case PROMDEV_TTYB:
		zsconsole(tp, 1, 1, &fcnstop);
		break;
#endif

	case PROMDEV_SCREEN:
		fbconstty = tp;
		tp->t_oproc = cnfbstart;
		fcnstop = cnfbstop;
		break;

	default:
		printf("unknown console output sink %d; using rom\n", out);
		tp->t_oproc = cnstart;
		fcnstop = (void (*)(struct tty *, int))nullop;
		break;
	}
}

/* ARGSUSED */
cnopen(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	register struct tty *tp = &cons;
 	static int firstopen = 1;

	if(firstopen) {
		clalloc(&tp->t_rawq, 1024, 1);
		clalloc(&tp->t_canq, 1024, 1);
		/* output queue doesn't need quoting */
		clalloc(&tp->t_outq, 1024, 0);
		firstopen = 0;
	}

	if ((tp->t_state & TS_ISOPEN) == 0) {
		/*
		 * Leave baud rate alone!
		 */
		ttychars(tp);
		tp->t_iflag = TTYDEF_IFLAG;
		tp->t_oflag = TTYDEF_OFLAG;
		tp->t_lflag = TTYDEF_LFLAG;
		tp->t_cflag = TTYDEF_CFLAG;
		tp->t_state = TS_ISOPEN | TS_CARR_ON;
		(void)(*tp->t_param)(tp, &tp->t_termios);
		ttsetwater(tp);
	} else if (tp->t_state & TS_XCLUDE && p->p_ucred->cr_uid != 0)
		return (EBUSY);
	return ((*linesw[tp->t_line].l_open)(dev, tp));
}

/* ARGSUSED */
cnclose(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	register struct tty *tp = &cons;

	(*linesw[tp->t_line].l_close)(tp, flag);
	ttyclose(tp);
	return (0);
}

/* ARGSUSED */
cnread(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	register struct tty *tp = &cons;

	return ((*linesw[tp->t_line].l_read)(tp, uio, flag));
}

/* ARGSUSED */
cnwrite(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	register struct tty *tp;
	
	if ((tp = constty) == NULL ||
	    (tp->t_state & (TS_CARR_ON|TS_ISOPEN)) != (TS_CARR_ON|TS_ISOPEN))
		tp = &cons;
	return ((*linesw[tp->t_line].l_write)(tp, uio, flag));
}

cnioctl(dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	register struct tty *tp;
	int error;

	/*
	 * Superuser can always use this to wrest control of console
	 * output from the "virtual" console.
	 */
	if (cmd == TIOCCONS && constty) {
		error = suser(p->p_ucred, (u_short *)NULL);
		if (error)
			return (error);
		constty = NULL;
		return (0);
	}
	tp = &cons;
	if ((error = linesw[tp->t_line].l_ioctl(tp, cmd, data, flag, p)) >= 0)
		return (error);
	if ((error = ttioctl(tp, cmd, data, flag, p)) >= 0)
		return (error);
	return (ENOTTY);
}

cnselect(dev, which, p)
	dev_t dev;
	int which;
	struct proc *p;
{

	return (ttselect(makedev(major(dev), 0), which, p));
}

/*
 * The rest of this code is run only when we are using the ROM vectors.
 */

/*
 * Generic output.  We just call putchar.  (Very bad for performance.)
 */
static void
cnstart(tp)
	register struct tty *tp;
{
	register int c, s;
	register union {
		void (*v1)__P((int));
		void (*v3)__P((int, u_char *, int));
	} putc;
	register int fd, v;

	s = spltty();
	if (tp->t_state & (TS_TIMEOUT | TS_TTSTOP)) {
		splx(s);
		return;
	}
	if ((v = promvec->pv_romvec_vers) > 2) {
		putc.v3 = (void (*))promvec->pv_v2devops.v2_write;
		fd = *promvec->pv_v2bootargs.v2_fd1;
	} else
		putc.v1 = promvec->pv_putchar;
	while (tp->t_outq.c_cc) {
		c = getc(&tp->t_outq);
		/*
		 * *%&!*& ROM monitor console putchar is not reentrant!
		 * splhigh/tty around it so as not to run so long with
		 * clock interrupts blocked.
		 */
		(void) splhigh();
		if (v > 2) {
			u_char c0 = c & 0177;
			(*putc.v3)(fd, &c0, 1);
		} else
			(*putc.v1)(c & 0177);
		(void) spltty();
	}
	if (tp->t_state & TS_ASLEEP) {		/* can't happen? */
		tp->t_state &= ~TS_ASLEEP;
		wakeup((caddr_t)&tp->t_outq);
	}
	selwakeup(&tp->t_wsel);
	splx(s);
}

void
cnstop(tp, flag)
	register struct tty *tp;
	int flag;
{
	(*fcnstop)(tp, flag);
}

/*
 * Frame buffer output.
 * We use pseudo-DMA, via the ROM `write string' function, called from
 * software clock interrupts.
 */
static void
cnfbstart(tp)
	register struct tty *tp;
{
	register int s;

	s = spltty();		/* paranoid: splsoftclock should suffice */
	if (tp->t_state & (TS_TIMEOUT | TS_BUSY | TS_TTSTOP)) {
		splx(s);
		return;
	}
	/*
	 * If there are sleepers, and output has drained below low
	 * water mark, awaken.
	 */
	if (tp->t_outq.c_cc <= tp->t_lowat) {
		if (tp->t_state & TS_ASLEEP) {
			tp->t_state &= ~TS_ASLEEP;
			wakeup((caddr_t)&tp->t_outq);
		}
		selwakeup(&tp->t_wsel);
	}
	if (tp->t_outq.c_cc) {
		tp->t_state |= TS_BUSY;
		if (s == 0) {
			(void) splsoftclock();
			cnfbdma((void *)tp);
		} else
			timeout(cnfbdma, tp, 1);
	}
	splx(s);
}

/*
 * Stop frame buffer output: just assert TS_FLUSH if necessary.
 */
static void
cnfbstop(tp, flag)
	register struct tty *tp;
	int flag;
{
	register int s = spltty();	/* paranoid */

	if ((tp->t_state & (TS_BUSY | TS_TTSTOP)) == TS_BUSY)
		tp->t_state |= TS_FLUSH;
	splx(s);
}

/*
 * Do pseudo-dma (called from software interrupt).
 */
static void
cnfbdma(tpaddr)
	void *tpaddr;
{
	register struct tty *tp = tpaddr;
	register char *p, *q;
	register int n, c, s;

	s = spltty();			/* paranoid */
	if (tp->t_state & TS_FLUSH) {
		tp->t_state &= ~(TS_BUSY | TS_FLUSH);
		splx(s);
	} else {
		tp->t_state &= ~TS_BUSY;
		splx(s);
		p = tp->t_outq.c_cf;
		n = ndqb(&tp->t_outq, 0);
		for (q = p, c = n; --c >= 0; q++)
			if (*q & 0200)	/* high bits seem to be bad */
				*q &= ~0200;
		if (promvec->pv_romvec_vers > 2) {
			(*promvec->pv_v2devops.v2_write)
				(*promvec->pv_v2bootargs.v2_fd1, p, n);
		} else
			(*promvec->pv_putstr)(p, n);
		ndflush(&tp->t_outq, n);
	}
	if (tp->t_line)
		(*linesw[tp->t_line].l_start)(tp);
	else
		cnfbstart(tp);
}

/*
 * The following is for rom console input.  The rom will not call
 * an `interrupt' routine on console input ready, so we must poll.
 * This is all rather sad.
 */
volatile int	cn_rxc = -1;		/* XXX receive `silo' */

/* called from hardclock, which is above spltty, so no tty calls! */
cnrom()
{
	register int c;

	if (cn_rxc >= 0)
		return (1);
	if (promvec->pv_romvec_vers > 2) {
		unsigned char c0;
		if ((*promvec->pv_v2devops.v2_read)
			(*promvec->pv_v2bootargs.v2_fd0, &c0, 1) <= 0)
			return (0);
		c = c0;
	} else if ((c = (*promvec->pv_nbgetchar)()) < 0)
		return (0);
	cn_rxc = c;
	return (1);
}

/* pseudo console software interrupt scheduled when cnrom() returns 1 */
cnrint()
{
	register struct tty *tp;
	register int c, s;

	s = splclock();
	c = cn_rxc;
	cn_rxc = -1;
	splx(s);
	if (c < 0)
		return;
	tp = &cons;
	if ((tp->t_cflag & CSIZE) == CS7) {
		/* XXX this should be done elsewhere, if at all */
		if (tp->t_cflag & PARENB)
			if (tp->t_cflag & PARODD ?
			    (char_type[c & 0177] & 0200) == (c & 0200) :
			    (char_type[c & 0177] & 0200) != (c & 0200))
				c |= TTY_PE;
		c &= ~0200;
	}
	(*linesw[tp->t_line].l_rint)(c, tp);
}

cngetc()
{
	register int s, c;

	if (promvec->pv_romvec_vers > 2) {
		register int n = 0;
		unsigned char c0;
		while (n <= 0) {
			s = splhigh();
			n = (*promvec->pv_v2devops.v2_read)
				(*promvec->pv_v2bootargs.v2_fd0, &c0, 1);
			splx(s);
		}
		c = c0;
	} else {
#ifdef SUN4
		/* SUN4 PROM: must turn off echo to avoid double char echo */
		extern struct om_vector *oldpvec;
		int saveecho;
#endif

		s = splhigh();
#ifdef SUN4
		if (cputyp == CPU_SUN4) {
			saveecho = *(oldpvec->echo);
			*(oldpvec->echo) = 0;
		}
#endif
		c = (*promvec->pv_getchar)();
#ifdef SUN4
		if (cputyp == CPU_SUN4) 
			*(oldpvec->echo) = saveecho;
#endif
		splx(s);
	}
	if (c == '\r')
		c = '\n';
	return (c);
}

cnputc(c)
	register int c;
{
	register int s;

	if (c == '\n')
		cnputc('\r');
	s = splhigh();
	if (promvec->pv_romvec_vers > 2) {
		unsigned char c0 = c;
		(*promvec->pv_v2devops.v2_write)
			(*promvec->pv_v2bootargs.v2_fd1, &c0, 1);
	} else
		(*promvec->pv_putchar)(c);
	splx(s);
}

cnpollc(on)
	int on;
{
}
