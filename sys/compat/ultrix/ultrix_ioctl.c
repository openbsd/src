/*	$OpenBSD: ultrix_ioctl.c,v 1.11 2004/09/19 21:34:43 mickey Exp $ */
/*	$NetBSD: ultrix_ioctl.c,v 1.3.4.1 1996/06/13 18:22:37 jonathan Exp $ */
/*	from : NetBSD: sunos_ioctl.c,v 1.21 1995/10/07 06:27:31 mycroft Exp */

/*
 * Copyright (c) 1993 Markus Wild.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * loosely from: Header: sunos_ioctl.c,v 1.7 93/05/28 04:40:43 torek Exp 
 */

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/ioctl.h>
#include <sys/termios.h>
#include <sys/tty.h>
#include <sys/socket.h>
#include <sys/audioio.h>
#include <net/if.h>

#include <sys/mount.h>

#include <compat/ultrix/ultrix_syscallargs.h>
#include <sys/syscallargs.h>

#include <compat/sunos/sunos.h>

#include "ultrix_tty.h"

#define emul_termio	ultrix_termio
#define emul_termios	ultrix_termios

/*
 * SunOS ioctl calls.
 * This file is something of a hodge-podge.
 * Support gets added as things turn up....
 */

static const struct speedtab sptab[] = {
	{ 0, 0 },
	{ 50, 1 },
	{ 75, 2 },
	{ 110, 3 },
	{ 134, 4 },
	{ 135, 4 },
	{ 150, 5 },
	{ 200, 6 },
	{ 300, 7 },
	{ 600, 8 },
	{ 1200, 9 },
	{ 1800, 10 },
	{ 2400, 11 },
	{ 4800, 12 },
	{ 9600, 13 },
	{ 19200, 14 },
	{ 38400, 15 },
	{ -1, -1 }
};

static const u_long s2btab[] = { 
	0,
	50,
	75,
	110,
	134,
	150,
	200,
	300,
	600,
	1200,
	1800,
	2400,
	4800,
	9600,
	19200,
	38400,
};


/*
 * Translate a single tty control char from the emulation value
 * to native termios, and vice-versa. Special-case
 * the value of POSIX_VDISABLE, mapping it to and from 0.
 */
#define NATIVE_TO_EMUL_CC(bsd_cc) \
 (((bsd_cc)   != _POSIX_VDISABLE) ? (bsd_cc) : 0)

#define EMUL_TO_NATIVE_CC(emul_cc) \
 (emul_cc) ? (emul_cc) : _POSIX_VDISABLE;



/*
 * these two conversion functions have mostly been done
 * with some perl cut&paste, then handedited to comment
 * out what doesn't exist under NetBSD.
 * A note from Markus's code:
 *	(l & BITMASK1) / BITMASK1 * BITMASK2  is translated
 *	optimally by gcc m68k, much better than any ?: stuff.
 *	Code may vary with different architectures of course.
 *
 * I don't know what optimizer you used, but seeing divu's and
 * bfextu's in the m68k assembly output did not encourage me...
 * as well, gcc on the sparc definately generates much better
 * code with ?:.
 */


static void
stios2btios(st, bt)
	struct emul_termios *st;
	struct termios *bt;
{
	register u_long l, r;

	l = st->c_iflag;
	r = 	((l & 0x00000001) ? IGNBRK	: 0);
	r |=	((l & 0x00000002) ? BRKINT	: 0);
	r |=	((l & 0x00000004) ? IGNPAR	: 0);
	r |=	((l & 0x00000008) ? PARMRK	: 0);
	r |=	((l & 0x00000010) ? INPCK	: 0);
	r |=	((l & 0x00000020) ? ISTRIP	: 0);
	r |= 	((l & 0x00000040) ? INLCR	: 0);
	r |=	((l & 0x00000080) ? IGNCR	: 0);
	r |=	((l & 0x00000100) ? ICRNL	: 0);
	/*	((l & 0x00000200) ? IUCLC	: 0) */
	r |=	((l & 0x00000400) ? IXON	: 0);
	r |=	((l & 0x00000800) ? IXANY	: 0);
	r |=	((l & 0x00001000) ? IXOFF	: 0);
	r |=	((l & 0x00002000) ? IMAXBEL	: 0);
	bt->c_iflag = r;

	l = st->c_oflag;
	r = 	((l & 0x00000001) ? OPOST	: 0);
	/*	((l & 0x00000002) ? OLCUC	: 0) */
	r |=	((l & 0x00000004) ? ONLCR	: 0);
	/*	((l & 0x00000008) ? OCRNL	: 0) */
	/*	((l & 0x00000010) ? ONOCR	: 0) */
	/*	((l & 0x00000020) ? ONLRET	: 0) */
	/*	((l & 0x00000040) ? OFILL	: 0) */
	/*	((l & 0x00000080) ? OFDEL	: 0) */
	/*	((l & 0x00000100) ? NLDLY	: 0) */
	/*	((l & 0x00000100) ? NL1		: 0) */
	/*	((l & 0x00000600) ? CRDLY	: 0) */
	/*	((l & 0x00000200) ? CR1		: 0) */
	/*	((l & 0x00000400) ? CR2		: 0) */
	/*	((l & 0x00000600) ? CR3		: 0) */
	/*	((l & 0x00001800) ? TABDLY	: 0) */
	/*	((l & 0x00000800) ? TAB1	: 0) */
	/*	((l & 0x00001000) ? TAB2	: 0) */
	r |=	((l & 0x00001800) ? OXTABS	: 0);
	/*	((l & 0x00002000) ? BSDLY	: 0) */
	/*	((l & 0x00002000) ? BS1		: 0) */
	/*	((l & 0x00004000) ? VTDLY	: 0) */
	/*	((l & 0x00004000) ? VT1		: 0) */
	/*	((l & 0x00008000) ? FFDLY	: 0) */
	/*	((l & 0x00008000) ? FF1		: 0) */
	/*	((l & 0x00010000) ? PAGEOUT	: 0) */
	/*	((l & 0x00020000) ? WRAP	: 0) */
	bt->c_oflag = r;

	l = st->c_cflag;
	switch (l & 0x00000030) {
	case 0:
		r = CS5;
		break;
	case 0x00000010:
		r = CS6;
		break;
	case 0x00000020:
		r = CS7;
		break;
	case 0x00000030:
		r = CS8;
		break;
	}		
	r |=	((l & 0x00000040) ? CSTOPB	: 0);
	r |=	((l & 0x00000080) ? CREAD	: 0);
	r |= 	((l & 0x00000100) ? PARENB	: 0);
	r |=	((l & 0x00000200) ? PARODD	: 0);
	r |=	((l & 0x00000400) ? HUPCL	: 0);
	r |=	((l & 0x00000800) ? CLOCAL	: 0);
	/*	((l & 0x00001000) ? LOBLK	: 0) */
	r |=	((l & 0x80000000) ? (CRTS_IFLOW|CCTS_OFLOW) : 0);
	bt->c_cflag = r;

	bt->c_ispeed = bt->c_ospeed = s2btab[l & 0x0000000f];

	l = st->c_lflag;
	r = 	((l & 0x00000001) ? ISIG	: 0);
	r |=	((l & 0x00000002) ? ICANON	: 0);
	/*	((l & 0x00000004) ? XCASE	: 0) */
	r |=	((l & 0x00000008) ? ECHO	: 0);
	r |=	((l & 0x00000010) ? ECHOE	: 0);
	r |=	((l & 0x00000020) ? ECHOK	: 0);
	r |=	((l & 0x00000040) ? ECHONL	: 0);
	r |= 	((l & 0x00000080) ? NOFLSH	: 0);
	r |=	((l & 0x00000100) ? TOSTOP	: 0);
	r |=	((l & 0x00000200) ? ECHOCTL	: 0);
	r |=	((l & 0x00000400) ? ECHOPRT	: 0);
	r |=	((l & 0x00000800) ? ECHOKE	: 0);
	/*	((l & 0x00001000) ? DEFECHO	: 0) */
	r |=	((l & 0x00002000) ? FLUSHO	: 0);
	r |=	((l & 0x00004000) ? PENDIN	: 0);
	bt->c_lflag = r;

	bt->c_cc[VINTR]    = EMUL_TO_NATIVE_CC(st->c_cc[0]);
	bt->c_cc[VQUIT]    = EMUL_TO_NATIVE_CC(st->c_cc[1]);
	bt->c_cc[VERASE]   = EMUL_TO_NATIVE_CC(st->c_cc[2]);
	bt->c_cc[VKILL]    = EMUL_TO_NATIVE_CC(st->c_cc[3]);
	bt->c_cc[VEOF]     = EMUL_TO_NATIVE_CC(st->c_cc[4]);
	bt->c_cc[VEOL]     = EMUL_TO_NATIVE_CC(st->c_cc[5]);
	bt->c_cc[VEOL2]    = EMUL_TO_NATIVE_CC(st->c_cc[6]);
	/* not present on NetBSD */
	/* bt->c_cc[VSWTCH]   = EMUL_TO_NATIVE_CC(st->c_cc[7]); */
	bt->c_cc[VSTART]   = EMUL_TO_NATIVE_CC(st->c_cc[10]);
	bt->c_cc[VSTOP]    = EMUL_TO_NATIVE_CC(st->c_cc[11]);
	bt->c_cc[VSUSP]    = EMUL_TO_NATIVE_CC(st->c_cc[12]);
	bt->c_cc[VDSUSP]   = EMUL_TO_NATIVE_CC(st->c_cc[13]);
	bt->c_cc[VREPRINT] = EMUL_TO_NATIVE_CC(st->c_cc[14]);
	bt->c_cc[VDISCARD] = EMUL_TO_NATIVE_CC(st->c_cc[15]);
	bt->c_cc[VWERASE]  = EMUL_TO_NATIVE_CC(st->c_cc[16]);
	bt->c_cc[VLNEXT]   = EMUL_TO_NATIVE_CC(st->c_cc[17]);
	bt->c_cc[VSTATUS]  = EMUL_TO_NATIVE_CC(st->c_cc[18]);

#ifdef COMPAT_ULTRIX
	/* Ultrix termio/termios has real vmin/vtime */
	bt->c_cc[VMIN]	   = EMUL_TO_NATIVE_CC(st->c_cc[8]);
	bt->c_cc[VTIME]	   = EMUL_TO_NATIVE_CC(st->c_cc[9]);
#else
	/* if `raw mode', create native VMIN/VTIME from SunOS VEOF/VEOL */
	bt->c_cc[VMIN]	   = (bt->c_lflag & ICANON) ? 1 : bt->c_cc[VEOF];
	bt->c_cc[VTIME]	   = (bt->c_lflag & ICANON) ? 1 : bt->c_cc[VEOL];
#endif

}

/*
 * Convert bsd termios to "sunos" emulated termios
 */
static void
btios2stios(bt, st)
	struct termios *bt;
	struct emul_termios *st;
{
	register u_long l, r;

	l = bt->c_iflag;
	r = 	((l &  IGNBRK) ? 0x00000001	: 0);
	r |=	((l &  BRKINT) ? 0x00000002	: 0);
	r |=	((l &  IGNPAR) ? 0x00000004	: 0);
	r |=	((l &  PARMRK) ? 0x00000008	: 0);
	r |=	((l &   INPCK) ? 0x00000010	: 0);
	r |=	((l &  ISTRIP) ? 0x00000020	: 0);
	r |=	((l &   INLCR) ? 0x00000040	: 0);
	r |=	((l &   IGNCR) ? 0x00000080	: 0);
	r |=	((l &   ICRNL) ? 0x00000100	: 0);
	/*	((l &   IUCLC) ? 0x00000200	: 0) */
	r |=	((l &    IXON) ? 0x00000400	: 0);
	r |=	((l &   IXANY) ? 0x00000800	: 0);
	r |=	((l &   IXOFF) ? 0x00001000	: 0);
	r |=	((l & IMAXBEL) ? 0x00002000	: 0);
	st->c_iflag = r;

	l = bt->c_oflag;
	r =	((l &   OPOST) ? 0x00000001	: 0);
	/*	((l &   OLCUC) ? 0x00000002	: 0) */
	r |=	((l &   ONLCR) ? 0x00000004	: 0);
	/*	((l &   OCRNL) ? 0x00000008	: 0) */
	/*	((l &   ONOCR) ? 0x00000010	: 0) */
	/*	((l &  ONLRET) ? 0x00000020	: 0) */
	/*	((l &   OFILL) ? 0x00000040	: 0) */
	/*	((l &   OFDEL) ? 0x00000080	: 0) */
	/*	((l &   NLDLY) ? 0x00000100	: 0) */
	/*	((l &     NL1) ? 0x00000100	: 0) */
	/*	((l &   CRDLY) ? 0x00000600	: 0) */
	/*	((l &     CR1) ? 0x00000200	: 0) */
	/*	((l &     CR2) ? 0x00000400	: 0) */
	/*	((l &     CR3) ? 0x00000600	: 0) */
	/*	((l &  TABDLY) ? 0x00001800	: 0) */
	/*	((l &    TAB1) ? 0x00000800	: 0) */
	/*	((l &    TAB2) ? 0x00001000	: 0) */
	r |=	((l &  OXTABS) ? 0x00001800	: 0);
	/*	((l &   BSDLY) ? 0x00002000	: 0) */
	/*	((l &     BS1) ? 0x00002000	: 0) */
	/*	((l &   VTDLY) ? 0x00004000	: 0) */
	/*	((l &     VT1) ? 0x00004000	: 0) */
	/*	((l &   FFDLY) ? 0x00008000	: 0) */
	/*	((l &     FF1) ? 0x00008000	: 0) */
	/*	((l & PAGEOUT) ? 0x00010000	: 0) */
	/*	((l &    WRAP) ? 0x00020000	: 0) */
	st->c_oflag = r;

	l = bt->c_cflag;
	switch (l & CSIZE) {
	case CS5:
		r = 0;
		break;
	case CS6:
		r = 0x00000010;
		break;
	case CS7:
		r = 0x00000020;
		break;
	case CS8:
		r = 0x00000030;
		break;
	}
	r |=	((l &  CSTOPB) ? 0x00000040	: 0);
	r |=	((l &   CREAD) ? 0x00000080	: 0);
	r |=	((l &  PARENB) ? 0x00000100	: 0);
	r |=	((l &  PARODD) ? 0x00000200	: 0);
	r |=	((l &   HUPCL) ? 0x00000400	: 0);
	r |=	((l &  CLOCAL) ? 0x00000800	: 0);
	/*	((l &   LOBLK) ? 0x00001000	: 0) */
	r |=	((l & (CRTS_IFLOW|CCTS_OFLOW)) ? 0x80000000 : 0);
	st->c_cflag = r;

	l = bt->c_lflag;
	r =	((l &    ISIG) ? 0x00000001	: 0);
	r |=	((l &  ICANON) ? 0x00000002	: 0);
	/*	((l &   XCASE) ? 0x00000004	: 0) */
	r |=	((l &    ECHO) ? 0x00000008	: 0);
	r |=	((l &   ECHOE) ? 0x00000010	: 0);
	r |=	((l &   ECHOK) ? 0x00000020	: 0);
	r |=	((l &  ECHONL) ? 0x00000040	: 0);
	r |=	((l &  NOFLSH) ? 0x00000080	: 0);
	r |=	((l &  TOSTOP) ? 0x00000100	: 0);
	r |=	((l & ECHOCTL) ? 0x00000200	: 0);
	r |=	((l & ECHOPRT) ? 0x00000400	: 0);
	r |=	((l &  ECHOKE) ? 0x00000800	: 0);
	/*	((l & DEFECHO) ? 0x00001000	: 0) */
	r |=	((l &  FLUSHO) ? 0x00002000	: 0);
	r |=	((l &  PENDIN) ? 0x00004000	: 0);
	st->c_lflag = r;

	l = ttspeedtab(bt->c_ospeed, sptab);
	if (l >= 0)
		st->c_cflag |= l;

	st->c_cc[0] = NATIVE_TO_EMUL_CC(bt->c_cc[VINTR]);
	st->c_cc[1] = NATIVE_TO_EMUL_CC(bt->c_cc[VQUIT]);
	st->c_cc[2] = NATIVE_TO_EMUL_CC(bt->c_cc[VERASE]);
	st->c_cc[3] = NATIVE_TO_EMUL_CC(bt->c_cc[VKILL]);
	st->c_cc[4] = NATIVE_TO_EMUL_CC(bt->c_cc[VEOF]);
	st->c_cc[5] = NATIVE_TO_EMUL_CC(bt->c_cc[VEOL]);
	st->c_cc[6] = NATIVE_TO_EMUL_CC(bt->c_cc[VEOL2]);
/* XXX - the next line was an ifdef instead of an ifndef - but i still
   have to find out what to do here
*/
#ifndef COMPAT_ULTRIX
	st->c_cc[7] = NATIVE_TO_EMUL_CC(bt->c_cc[VSWTCH]);
#else
	st->c_cc[7] = 0;
#endif
	st->c_cc[10] = NATIVE_TO_EMUL_CC(bt->c_cc[VSTART]);
	st->c_cc[11] = NATIVE_TO_EMUL_CC(bt->c_cc[VSTOP]);
	st->c_cc[12]= NATIVE_TO_EMUL_CC(bt->c_cc[VSUSP]);
	st->c_cc[13]= NATIVE_TO_EMUL_CC(bt->c_cc[VDSUSP]);
	st->c_cc[14]= NATIVE_TO_EMUL_CC(bt->c_cc[VREPRINT]);
	st->c_cc[15]= NATIVE_TO_EMUL_CC(bt->c_cc[VDISCARD]);
	st->c_cc[16]= NATIVE_TO_EMUL_CC(bt->c_cc[VWERASE]);
	st->c_cc[17]= NATIVE_TO_EMUL_CC(bt->c_cc[VLNEXT]);
	st->c_cc[18]= NATIVE_TO_EMUL_CC(bt->c_cc[VSTATUS]);

#ifdef COMPAT_ULTRIX
	st->c_cc[8]= NATIVE_TO_EMUL_CC(bt->c_cc[VMIN]);
	st->c_cc[9]= NATIVE_TO_EMUL_CC(bt->c_cc[VTIME]);
#else
	if (!(bt->c_lflag & ICANON)) {
		/* SunOS stores VMIN/VTIME in VEOF/VEOL (if ICANON is off) */
		st->c_cc[4] = bt->c_cc[VMIN];
		st->c_cc[5] = bt->c_cc[VTIME];
	}
#endif

#ifdef COMPAT_SUNOS
	st->c_line = 0;	/* 4.3bsd "old" line discipline */
#else
	st->c_line = 2;	/* 4.3bsd "new" line discipline */
#endif
}

#define TERMIO_NCC 10	/* ultrix termio NCC is 10 */

/*
 * Convert emulated struct termios to termio(?)
 */
static void
stios2stio(ts, t)
	struct emul_termios *ts;
	struct emul_termio *t;
{
	t->c_iflag = ts->c_iflag;
	t->c_oflag = ts->c_oflag;
	t->c_cflag = ts->c_cflag;
	t->c_lflag = ts->c_lflag;
	t->c_line  = ts->c_line;
	bcopy(ts->c_cc, t->c_cc, TERMIO_NCC);
}

/*
 * Convert the other way
 */
static void
stio2stios(t, ts)
	struct emul_termio *t;
	struct emul_termios *ts;
{
	ts->c_iflag = t->c_iflag;
	ts->c_oflag = t->c_oflag;
	ts->c_cflag = t->c_cflag;
	ts->c_lflag = t->c_lflag;
	ts->c_line  = t->c_line;
	bcopy(t->c_cc, ts->c_cc, TERMIO_NCC); /* don't touch the upper fields! */
}

int
ultrix_sys_ioctl(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct ultrix_sys_ioctl_args *uap = v;
	struct filedesc *fdp = p->p_fd;
	struct file *fp;
	register int (*ctl)();
	int error;

	if ((fp = fd_getfile(fdp, SCARG(uap, fd))) == NULL)
		return EBADF;
	FREF(fp);

	if ((fp->f_flag & (FREAD|FWRITE)) == 0) {
		error = EBADF;
		goto out;
	}

	ctl = fp->f_ops->fo_ioctl;

	switch (SCARG(uap, com)) {
	case _IOR('t', 0, int):
		SCARG(uap, com) = TIOCGETD;
		break;
	case _IOW('t', 1, int):
	    {
		int disc;

		if ((error = copyin(SCARG(uap, data), (caddr_t)&disc,
		    sizeof disc)) != 0)
			goto out;

		/* map SunOS NTTYDISC into our termios discipline */
		if (disc == 2)
			disc = 0;
		/* all other disciplines are not supported by NetBSD */
		if (disc) {
			error = ENXIO;
			goto out;
		}

		error = (*ctl)(fp, TIOCSETD, (caddr_t)&disc, p);
		goto out;
	    }
	case _IOW('t', 101, int):	/* sun SUNOS_TIOCSSOFTCAR */
	    {
		int x;	/* unused */

		error = copyin((caddr_t)&x, SCARG(uap, data), sizeof x);
		goto out;
	    }
	case _IOR('t', 100, int):	/* sun SUNOS_TIOCSSOFTCAR */
	    {
		int x = 0;

		error = copyout((caddr_t)&x, SCARG(uap, data), sizeof x);
		goto out;
	    }
	case _IO('t', 36): 		/* sun TIOCCONS, no parameters */
	    {
		int on = 1;
		error = (*ctl)(fp, TIOCCONS, (caddr_t)&on, p);
		goto out;
	    }
	case _IOW('t', 37, struct sunos_ttysize): 
	    {
		struct winsize ws;
		struct sunos_ttysize ss;

		if ((error = (*ctl)(fp, TIOCGWINSZ, (caddr_t)&ws, p)) != 0)
			goto out;

		if ((error = copyin (SCARG(uap, data), &ss, sizeof (ss))) != 0)
			goto out;

		ws.ws_row = ss.ts_row;
		ws.ws_col = ss.ts_col;

		error = ((*ctl)(fp, TIOCSWINSZ, (caddr_t)&ws, p));
		goto out;
	    }
	case _IOW('t', 38, struct sunos_ttysize): 
	    {
		struct winsize ws;
		struct sunos_ttysize ss;

		if ((error = (*ctl)(fp, TIOCGWINSZ, (caddr_t)&ws, p)) != 0)
			goto out;

		ss.ts_row = ws.ws_row;
		ss.ts_col = ws.ws_col;

		error = copyout ((caddr_t)&ss, SCARG(uap, data), sizeof (ss));
		goto out;
	    }
	case _IOW('t', 130, int):
		SCARG(uap, com) = TIOCSPGRP;
		break;
	case _IOR('t', 131, int):
		SCARG(uap, com) = TIOCGPGRP;
		break;
	case _IO('t', 132):
		SCARG(uap, com) = TIOCSCTTY;
		break;
	/* Emulate termio or termios tcget() */	
	case ULTRIX_TCGETA:
	case ULTRIX_TCGETS: 
	    {
		struct termios bts;
		struct ultrix_termios sts;
		struct ultrix_termio st;
	
		if ((error = (*ctl)(fp, TIOCGETA, (caddr_t)&bts, p)) != 0)
			goto out;
	
		btios2stios (&bts, &sts);
		if (SCARG(uap, com) == ULTRIX_TCGETA) {
			stios2stio (&sts, &st);
			error = copyout((caddr_t)&st, SCARG(uap, data),
			    sizeof (st));
			goto out;
		} else {
			error = copyout((caddr_t)&sts, SCARG(uap, data),
			    sizeof (sts));
			goto out;
		}
		/*NOTREACHED*/
	    }
	/* Emulate termio tcset() */
	case ULTRIX_TCSETA:
	case ULTRIX_TCSETAW:
	case ULTRIX_TCSETAF:
	    {
		struct termios bts;
		struct ultrix_termios sts;
		struct ultrix_termio st;
	       
		if ((error = copyin(SCARG(uap, data), (caddr_t)&st,
		    sizeof (st))) != 0)
			goto out;

		/* get full BSD termios so we don't lose information */
		if ((error = (*ctl)(fp, TIOCGETA, (caddr_t)&bts, p)) != 0)
			goto out;

		/*
		 * convert to sun termios, copy in information from
		 * termio, and convert back, then set new values.
		 */
		btios2stios(&bts, &sts);
		stio2stios(&st, &sts);
		stios2btios(&sts, &bts);

		/*
		 * map ioctl code: ultrix tcsets are numbered in reverse order
		 */
		error = (*ctl)(fp, ULTRIX_TCSETA - SCARG(uap, com) + TIOCSETA,
		    (caddr_t)&bts, p);
		printf("ultrix TCSETA %lx returns %d\n",
		       ULTRIX_TCSETA - SCARG(uap, com), error);
		goto out;
	    }
	/* Emulate termios tcset() */
	case ULTRIX_TCSETS:
	case ULTRIX_TCSETSW:
	case ULTRIX_TCSETSF:
	    {
		struct termios bts;
		struct ultrix_termios sts;

		if ((error = copyin (SCARG(uap, data), (caddr_t)&sts,
		    sizeof (sts))) != 0)
			goto out;
		stios2btios (&sts, &bts);
		error = (*ctl)(fp, ULTRIX_TCSETS - SCARG(uap, com) + TIOCSETA,
		    (caddr_t)&bts, p);
		goto out;
	    }
/*
 * Pseudo-tty ioctl translations.
 */
	case _IOW('t', 32, int): {	/* TIOCTCNTL */
		int error, on;

		error = copyin (SCARG(uap, data), (caddr_t)&on, sizeof (on));
		if (error != 0)
			goto out;
		error = (*ctl)(fp, TIOCUCNTL, (caddr_t)&on, p);
		goto out;
	}
	case _IOW('t', 33, int): {	/* TIOCSIGNAL */
		int error, sig;

		error = copyin (SCARG(uap, data), (caddr_t)&sig, sizeof (sig));
		if (error != 0)
			goto out;
		error = (*ctl)(fp, TIOCSIG, (caddr_t)&sig, p);
		goto out;
	}
	
/*
 * Socket ioctl translations.
 */
#define IN_TYPE(a, type_t) { \
	type_t localbuf; \
	if ((error = copyin (SCARG(uap, data), \
				(caddr_t)&localbuf, sizeof (type_t))) != 0) \
		goto out; \
	error = (*ctl)(fp, a, (caddr_t)&localbuf, p); \
	goto out; \
}

#define INOUT_TYPE(a, type_t) { \
	type_t localbuf; \
	if ((error = copyin (SCARG(uap, data), (caddr_t)&localbuf,	\
			     sizeof (type_t))) != 0) \
		goto out; \
	if ((error = (*ctl)(fp, a, (caddr_t)&localbuf, p)) != 0) \
		goto out; \
	error = copyout ((caddr_t)&localbuf, SCARG(uap, data), sizeof (type_t)); \
	goto out; \
}


#define IFREQ_IN(a) { \
	struct ifreq ifreq; \
	if ((error = copyin (SCARG(uap, data), (caddr_t)&ifreq, sizeof (ifreq))) != 0) \
		goto out; \
	error = (*ctl)(fp, a, (caddr_t)&ifreq, p); \
	goto out; \
}

#define IFREQ_INOUT(a) { \
	struct ifreq ifreq; \
	if ((error = copyin (SCARG(uap, data), (caddr_t)&ifreq, sizeof (ifreq))) != 0) \
		goto out; \
	if ((error = (*ctl)(fp, a, (caddr_t)&ifreq, p)) != 0) \
		goto out; \
	error = copyout ((caddr_t)&ifreq, SCARG(uap, data), sizeof (ifreq)); \
	goto out; \
}

	case _IOW('i', 12, struct ifreq):
		/* SIOCSIFADDR */
		break;

	case _IOWR('i', 13, struct ifreq):
		IFREQ_INOUT(OSIOCGIFADDR);

	case _IOW('i', 14, struct ifreq):
		/* SIOCSIFDSTADDR */
		break;

	case _IOWR('i', 15, struct ifreq):
		IFREQ_INOUT(OSIOCGIFDSTADDR);

	case _IOW('i', 16, struct ifreq):
		/* SIOCSIFFLAGS */
		break;

	case _IOWR('i', 17, struct ifreq):
		/* SIOCGIFFLAGS */
		break;

	case _IOW('i', 21, struct ifreq):
		IFREQ_IN(SIOCSIFMTU);

	case _IOWR('i', 22, struct ifreq):
		IFREQ_INOUT(SIOCGIFMTU);

	case _IOWR('i', 23, struct ifreq):
		IFREQ_INOUT(SIOCGIFBRDADDR);

	case _IOW('i', 24, struct ifreq):
		IFREQ_IN(SIOCSIFBRDADDR);

	case _IOWR('i', 25, struct ifreq):
		IFREQ_INOUT(OSIOCGIFNETMASK);

	case _IOW('i', 26, struct ifreq):
		IFREQ_IN(SIOCSIFNETMASK);

	case _IOWR('i', 27, struct ifreq):
		IFREQ_INOUT(SIOCGIFMETRIC);

	case _IOWR('i', 28, struct ifreq):
		IFREQ_IN(SIOCSIFMETRIC);

	case _IOW('i', 30, struct arpreq):
		/* SIOCSARP */
		break;

	case _IOWR('i', 31, struct arpreq):
		/* SIOCGARP */
		break;

	case _IOW('i', 32, struct arpreq):
		/* SIOCDARP */
		break;

	case _IOW('i', 18, struct ifreq):	/* SIOCSIFMEM */
	case _IOWR('i', 19, struct ifreq):	/* SIOCGIFMEM */
	case _IOW('i', 40, struct ifreq):	/* SIOCUPPER */
	case _IOW('i', 41, struct ifreq):	/* SIOCLOWER */
	case _IOW('i', 44, struct ifreq):	/* SIOCSETSYNC */
	case _IOWR('i', 45, struct ifreq):	/* SIOCGETSYNC */
	case _IOWR('i', 46, struct ifreq):	/* SIOCSDSTATS */
	case _IOWR('i', 47, struct ifreq):	/* SIOCSESTATS */
	case _IOW('i', 48, int):		/* SIOCSPROMISC */
	case _IOW('i', 49, struct ifreq):	/* SIOCADDMULTI */
	case _IOW('i', 50, struct ifreq):	/* SIOCDELMULTI */
		error = EOPNOTSUPP;
		goto out;

	case _IOWR('i', 20, struct ifconf):	/* SIOCGIFCONF */
	    {
		struct ifconf ifconf;

		/*
		 * XXX: two more problems
		 * 1. our sockaddr's are variable length, not always sizeof(sockaddr)
		 * 2. this returns a name per protocol, ie. it returns two "lo0"'s
		 */
		if ((error = copyin (SCARG(uap, data), (caddr_t)&ifconf,
				     sizeof (ifconf))) != 0)
			goto out;
		if ((error = (*ctl)(fp, OSIOCGIFCONF,
				    * (caddr_t)&ifconf, p)) !=0 )
			goto out;
		error = copyout ((caddr_t)&ifconf, SCARG(uap, data),
		    sizeof (ifconf));
		goto out;
	    }

	}
	error = (sys_ioctl(p, uap, retval));

out:
	FRELE(fp);
	return (error);
}
