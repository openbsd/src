/*	$OpenBSD: tty_tb.c,v 1.5 2003/08/11 09:56:49 mickey Exp $	*/
/*	$NetBSD: tty_tb.c,v 1.18 1996/02/04 02:17:36 christos Exp $	*/

/*-
 * Copyright (c) 1982, 1986, 1991, 1993
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
 *
 *	@(#)tty_tb.c	8.1 (Berkeley) 6/10/93
 */

#include "tb.h"

/*
 * Line discipline for RS232 tablets;
 * supplies binary coordinate data.
 */
#include <sys/param.h>
#include <sys/tablet.h>
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <sys/ioctl_compat.h>
#include <sys/tty.h>
#include <sys/proc.h>

union tbpos {
	struct	hitpos hitpos;
	struct	gtcopos gtcopos;
	struct	polpos polpos;
};

/*
 * Tablet configuration table.
 */
struct	tbconf {
	short	tbc_recsize;	/* input record size in bytes */
	short	tbc_uiosize;	/* size of data record returned user */
	int	tbc_sync;	/* mask for finding sync byte/bit */
				/* decoding routine */
    	void    (*tbc_decode)(const struct tbconf *, char *, union tbpos *);
	u_char	*tbc_run;	/* enter run mode sequence */
	u_char	*tbc_point;	/* enter point mode sequence */
	u_char	*tbc_stop;	/* stop sequence */
	u_char	*tbc_start;	/* start/restart sequence */
	int	tbc_flags;
#define	TBF_POL		0x1	/* polhemus hack */
#define	TBF_INPROX	0x2	/* tablet has proximity info */
};

static void gtcodecode(const struct tbconf *, char *, union tbpos *);
static void tbolddecode(const struct tbconf *, char *, union tbpos *);
static void tblresdecode(const struct tbconf *, char *, union tbpos *);
static void tbhresdecode(const struct tbconf *, char *, union tbpos *);
static void poldecode(const struct tbconf *, char *, union tbpos *);


const struct	tbconf tbconf[TBTYPE] = {
{ 0 },
{ 5, sizeof (struct hitpos), 0200, tbolddecode, "6", "4" },
{ 5, sizeof (struct hitpos), 0200, tbolddecode, "\1CN", "\1RT", "\2", "\4" },
{ 8, sizeof (struct gtcopos), 0200, gtcodecode },
{17, sizeof (struct polpos), 0200, poldecode, 0, 0, "\21", "\5\22\2\23",
  TBF_POL },
{ 5, sizeof (struct hitpos), 0100, tblresdecode, "\1CN", "\1PT", "\2", "\4",
  TBF_INPROX },
{ 6, sizeof (struct hitpos), 0200, tbhresdecode, "\1CN", "\1PT", "\2", "\4",
  TBF_INPROX },
{ 5, sizeof (struct hitpos), 0100, tblresdecode, "\1CL\33", "\1PT\33", 0, 0},
{ 6, sizeof (struct hitpos), 0200, tbhresdecode, "\1CL\33", "\1PT\33", 0, 0},
};

/*
 * Tablet state
 */
struct tb {
	int	tbflags;		/* mode & type bits */
#define	TBMAXREC	17	/* max input record size */
	char	cbuf[TBMAXREC];		/* input buffer */
	int	tbinbuf;
	char	*tbcp;
	union	tbpos tbpos; 
} tb[NTB];


int	tbopen(dev_t, struct tty *);
void	tbclose(struct tty *);
int	tbread(struct tty *, struct uio *);
void	tbinput(int, struct tty *);
int	tbtioctl(struct tty *, u_long, caddr_t, int, struct proc *);
void	tbattach(int);

/*
 * Open as tablet discipline; called on discipline change.
 */
/*ARGSUSED*/
int
tbopen(dev, tp)
	dev_t dev;
	register struct tty *tp;
{
	register struct tb *tbp;

	if (tp->t_line == TABLDISC)
		return (ENODEV);
	ttywflush(tp);
	for (tbp = tb; tbp < &tb[NTB]; tbp++)
		if (tbp->tbflags == 0)
			break;
	if (tbp >= &tb[NTB])
		return (EBUSY);
	tbp->tbflags = TBTIGER|TBPOINT;		/* default */
	tbp->tbcp = tbp->cbuf;
	tbp->tbinbuf = 0;
	bzero((caddr_t)&tbp->tbpos, sizeof (tbp->tbpos));
	tp->t_sc = (caddr_t)tbp;
	tp->t_flags |= LITOUT;
	return (0);
}

/*
 * Line discipline change or last device close.
 */
void
tbclose(tp)
	register struct tty *tp;
{
	int modebits = TBPOINT|TBSTOP;

	tbtioctl(tp, BIOSMODE, (caddr_t) &modebits, 0, curproc);
}

/*
 * Read from a tablet line.
 * Characters have been buffered in a buffer and decoded.
 */
int
tbread(tp, uio)
	register struct tty *tp;
	struct uio *uio;
{
	struct tb *tbp = (struct tb *)tp->t_sc;
	const struct tbconf *tc = &tbconf[tbp->tbflags & TBTYPE];
	int ret;

	if ((tp->t_state&TS_CARR_ON) == 0)
		return (EIO);
	ret = uiomove((caddr_t) &tbp->tbpos, tc->tbc_uiosize, uio);
	if (tc->tbc_flags&TBF_POL)
		tbp->tbpos.polpos.p_key = ' ';
	return (ret);
}

/*
 * Low level character input routine.
 * Stuff the character in the buffer, and decode
 * if all the chars are there.
 *
 * This routine could be expanded in-line in the receiver
 * interrupt routine to make it run as fast as possible.
 */
void
tbinput(c, tp)
	register int c;
	register struct tty *tp;
{
	struct tb *tbp = (struct tb *)tp->t_sc;
	const struct tbconf *tc = &tbconf[tbp->tbflags & TBTYPE];

	if (tc->tbc_recsize == 0 || tc->tbc_decode == 0)	/* paranoid? */
		return;
	/*
	 * Locate sync bit/byte or reset input buffer.
	 */
	if (c&tc->tbc_sync || tbp->tbinbuf == tc->tbc_recsize) {
		tbp->tbcp = tbp->cbuf;
		tbp->tbinbuf = 0;
	}
	*tbp->tbcp++ = c&0177;
	/*
	 * Call decode routine only if a full record has been collected.
	 */
	if (++tbp->tbinbuf == tc->tbc_recsize)
		(*tc->tbc_decode)(tc, tbp->cbuf, &tbp->tbpos);
}

/*
 * Decode GTCO 8 byte format (high res, tilt, and pressure).
 */
static void
gtcodecode(tc, cp, u)
	const struct tbconf *tc;
	register char *cp;
	register union tbpos *u;
{
	struct gtcopos *pos = &u->gtcopos;
	pos->pressure = *cp >> 2;
	pos->status = (pos->pressure > 16) | TBINPROX; /* half way down */
	pos->xpos = (*cp++ & 03) << 14;
	pos->xpos |= *cp++ << 7;
	pos->xpos |= *cp++;
	pos->ypos = (*cp++ & 03) << 14;
	pos->ypos |= *cp++ << 7;
	pos->ypos |= *cp++;
	pos->xtilt = *cp++;
	pos->ytilt = *cp++;
	pos->scount++;
}

/*
 * Decode old Hitachi 5 byte format (low res).
 */
static void
tbolddecode(tc, cp, u)
	const struct tbconf *tc;
	register char *cp;
	register union tbpos *u;
{
	struct hitpos *pos = &u->hitpos;
	register char byte;

	byte = *cp++;
	pos->status = (byte&0100) ? TBINPROX : 0;
	byte &= ~0100;
	if (byte > 036)
		pos->status |= 1 << ((byte-040)/2);
	pos->xpos = *cp++ << 7;
	pos->xpos |= *cp++;
	if (pos->xpos < 256)			/* tablet wraps around at 256 */
		pos->status &= ~TBINPROX;	/* make it out of proximity */
	pos->ypos = *cp++ << 7;
	pos->ypos |= *cp++;
	pos->scount++;
}

/*
 * Decode new Hitach 5-byte format (low res).
 */
static void
tblresdecode(tc, cp, u)
	const struct tbconf *tc;
	register char *cp;
	register union tbpos *u;
{
	struct hitpos *pos = &u->hitpos;

	*cp &= ~0100;		/* mask sync bit */
	pos->status = (*cp++ >> 2) | TBINPROX;
	if (tc->tbc_flags&TBF_INPROX && pos->status&020)
		pos->status &= ~(020|TBINPROX);
	pos->xpos = *cp++;
	pos->xpos |= *cp++ << 6;
	pos->ypos = *cp++;
	pos->ypos |= *cp++ << 6;
	pos->scount++;
}

/*
 * Decode new Hitach 6-byte format (high res).
 */
static void
tbhresdecode(tc, cp, u)
	const struct tbconf *tc;
	register char *cp;
	register union tbpos *u;
{
	struct hitpos *pos = &u->hitpos;
	char byte;

	byte = *cp++;
	pos->xpos = (byte & 03) << 14;
	pos->xpos |= *cp++ << 7;
	pos->xpos |= *cp++;
	pos->ypos = *cp++ << 14;
	pos->ypos |= *cp++ << 7;
	pos->ypos |= *cp++;
	pos->status = (byte >> 2) | TBINPROX;
	if (tc->tbc_flags&TBF_INPROX && pos->status&020)
		pos->status &= ~(020|TBINPROX);
	pos->scount++;
}

/*
 * Polhemus decode.
 */
static void
poldecode(tc, cp, u)
	const struct tbconf *tc;
	register char *cp;
	register union tbpos *u;
{
	struct polpos *pos = &u->polpos;

	pos->p_x = cp[4] | cp[3]<<7 | (cp[9] & 0x03) << 14;
	pos->p_y = cp[6] | cp[5]<<7 | (cp[9] & 0x0c) << 12;
	pos->p_z = cp[8] | cp[7]<<7 | (cp[9] & 0x30) << 10;
	pos->p_azi = cp[11] | cp[10]<<7 | (cp[16] & 0x03) << 14;
	pos->p_pit = cp[13] | cp[12]<<7 | (cp[16] & 0x0c) << 12;
	pos->p_rol = cp[15] | cp[14]<<7 | (cp[16] & 0x30) << 10;
	pos->p_stat = cp[1] | cp[0]<<7;
	if (cp[2] != ' ')
		pos->p_key = cp[2];
}

/*ARGSUSED*/
int
tbtioctl(tp, cmd, data, flag, p)
	struct tty *tp;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	register struct tb *tbp = (struct tb *)tp->t_sc;

	switch (cmd) {

	case BIOGMODE:
		*(int *)data = tbp->tbflags & TBMODE;
		break;

	case BIOSTYPE:
		if (tbconf[*(int *)data & TBTYPE].tbc_recsize == 0 ||
		    tbconf[*(int *)data & TBTYPE].tbc_decode == 0)
			return (EINVAL);
		tbp->tbflags &= ~TBTYPE;
		tbp->tbflags |= *(int *)data & TBTYPE;
		/* fall thru... to set mode bits */

	case BIOSMODE: {
		const struct tbconf *tc;
		u_char *c;

		tbp->tbflags &= ~TBMODE;
		tbp->tbflags |= *(int *)data & TBMODE;
		tc = &tbconf[tbp->tbflags & TBTYPE];
		if (tbp->tbflags & TBSTOP) {
			if (tc->tbc_stop)
				for (c = tc->tbc_stop; *c != '\0'; c++)
					ttyoutput(*c, tp);
		} else if (tc->tbc_start)
			for (c = tc->tbc_start; *c != '\0'; c++)
				ttyoutput(*c, tp);
		if (tbp->tbflags & TBPOINT) {
			if (tc->tbc_point)
				for (c = tc->tbc_point; *c != '\0'; c++)
					ttyoutput(*c, tp);
		} else if (tc->tbc_run)
			for (c = tc->tbc_run; *c != '\0'; c++)
				ttyoutput(*c, tp);
		ttstart(tp);
		break;
	}

	case BIOGTYPE:
		*(int *)data = tbp->tbflags & TBTYPE;
		break;

	case TIOCSETD:
	case TIOCGETD:
	case TIOCGETP:
	case TIOCGETC:
		return (-1);		/* pass thru... */

	default:
		return (ENOTTY);
	}
	return (0);
}

void
tbattach(dummy)
       int dummy;
{
    /* stub to handle side effect of new config */
}
