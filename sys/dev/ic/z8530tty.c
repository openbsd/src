/*	$OpenBSD: z8530tty.c,v 1.23 2010/07/02 17:27:01 nicm Exp $ */
/*	$NetBSD: z8530tty.c,v 1.13 1996/10/16 20:42:14 gwr Exp $	*/

/*
 * Copyright (c) 1994 Gordon W. Ross
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
 *	@(#)zs.c	8.1 (Berkeley) 7/19/93
 */

/*
 * Zilog Z8530 Dual UART driver (tty interface)
 *
 * This is the "slave" driver that will be attached to
 * the "zsc" driver for plain "tty" async. serial lines.
 *
 * Credits, history:
 *
 * The original version of this code was the sparc/dev/zs.c driver
 * as distributed with the Berkeley 4.4 Lite release.  Since then,
 * Gordon Ross reorganized the code into the current parent/child
 * driver scheme, separating the Sun keyboard and mouse support
 * into independent child drivers.
 *
 * RTS/CTS flow-control support was a collaboration of:
 *	Gordon Ross <gwr@netbsd.org>,
 *	Bill Studenmund <wrstuden@loki.stanford.edu>
 *	Ian Dall <Ian.Dall@dsto.defence.gov.au>
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#include <sys/tty.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/syslog.h>

#include <dev/ic/z8530reg.h>
#include <machine/z8530var.h>

#ifdef KGDB
extern int zs_check_kgdb();
#endif

/*
 * Allow the MD var.h to override the default CFLAG so that
 * console messages during boot come out with correct parity.
 */
#ifndef	ZSTTY_DEF_CFLAG
#define	ZSTTY_DEF_CFLAG	TTYDEF_CFLAG
#endif

/*
 * How many input characters we can buffer.
 * The port-specific var.h may override this.
 * Note: must be a power of two!
 */
#ifndef	ZSTTY_RING_SIZE
#define	ZSTTY_RING_SIZE	2048
#endif

/*
 * Make this an option variable one can patch.
 * But be warned:  this must be a power of 2!
 */
int zstty_rbuf_size = ZSTTY_RING_SIZE;

/* This should usually be 3/4 of ZSTTY_RING_SIZE */
int zstty_rbuf_hiwat = (ZSTTY_RING_SIZE - (ZSTTY_RING_SIZE >> 2));

struct zstty_softc {
	struct	device zst_dev;		/* required first: base device */
	struct  tty *zst_tty;
	struct	zs_chanstate *zst_cs;

	int zst_hwflags;	/* see z8530var.h */
	int zst_swflags;	/* TIOCFLAG_SOFTCAR, ... <ttycom.h> */

	/*
	 * Printing an overrun error message often takes long enough to
	 * cause another overrun, so we only print one per second.
	 */
	long	zst_rotime;		/* time of last ring overrun */
	long	zst_fotime;		/* time of last fifo overrun */

	/*
	 * The receive ring buffer.
	 */
	int	zst_rbget;	/* ring buffer `get' index */
	volatile int	zst_rbput;	/* ring buffer `put' index */
	int	zst_ringmask;
	int	zst_rbhiwat;

	u_short	*zst_rbuf; /* rr1, data pairs */

	/*
	 * The transmit byte count and address are used for pseudo-DMA
	 * output in the hardware interrupt code.  PDMA can be suspended
	 * to get pending changes done; heldtbc is used for this.  It can
	 * also be stopped for ^S; this sets TS_TTSTOP in tp->t_state.
	 */
	int 	zst_tbc;			/* transmit byte count */
	caddr_t	zst_tba;			/* transmit buffer address */
	int 	zst_heldtbc;		/* held tbc while xmission stopped */

	/* Flags to communicate with zstty_softint() */
	volatile char zst_rx_blocked;	/* input block at ring */
	volatile char zst_rx_overrun;	/* ring overrun */
	volatile char zst_tx_busy;	/* working on an output chunk */
	volatile char zst_tx_done;	/* done with one output chunk */
	volatile char zst_tx_stopped;	/* H/W level stop (lost CTS) */
	volatile char zst_st_check;	/* got a status interrupt */
	char pad[2];
};


/* Definition of the driver for autoconfig. */
static int	zstty_match(struct device *, void *, void *);
static void	zstty_attach(struct device *, struct device *, void *);

struct cfattach zstty_ca = {
	sizeof(struct zstty_softc), zstty_match, zstty_attach
};

struct cfdriver zstty_cd = {
	NULL, "zstty", DV_TTY
};

struct zsops zsops_tty;

/* Routines called from other code. */
cdev_decl(zs);	/* open, close, read, write, ioctl, stop, ... */

static void	zsstart(struct tty *);
static int	zsparam(struct tty *, struct termios *);
static void	zs_modem(struct zstty_softc *zst, int onoff);
static int	zshwiflow(struct tty *, int);
static void	zs_hwiflow(struct zstty_softc *, int);
static void	zstty_rxint(register struct zs_chanstate *);
static void	zstty_txint(register struct zs_chanstate *);
static void	zstty_stint(register struct zs_chanstate *);
static void	zstty_softint(struct zs_chanstate *);
static void	zsoverrun(struct zstty_softc *, long *, char *);
/*
 * zstty_match: how is this zs channel configured?
 */
int 
zstty_match(parent, match, aux)
	struct device *parent;
	void   *match, *aux;
{
	struct cfdata *cf = match;
	struct zsc_attach_args *args = aux;

	/* Exact match is better than wildcard. */
	if (cf->cf_loc[0] == args->channel)
		return 2;

	/* This driver accepts wildcard. */
	if (cf->cf_loc[0] == -1)
		return 1;

	return 0;
}

void 
zstty_attach(parent, self, aux)
	struct device *parent, *self;
	void   *aux;

{
	struct zsc_softc *zsc = (void *) parent;
	struct zstty_softc *zst = (void *) self;
	struct zsc_attach_args *args = aux;
	struct zs_chanstate *cs;
	struct cfdata *cf;
	struct tty *tp;
	int channel, tty_unit;
	dev_t dev;

	cf = zst->zst_dev.dv_cfdata;
	tty_unit = zst->zst_dev.dv_unit;
	channel = args->channel;
	cs = &zsc->zsc_cs[channel];
	cs->cs_private = zst;
	cs->cs_ops = &zsops_tty;

	zst->zst_cs = cs;
	zst->zst_swflags = cf->cf_flags;	/* softcar, etc. */
	zst->zst_hwflags = args->hwflags;
	dev = makedev(ZSTTY_MAJOR, tty_unit);

	if (zst->zst_swflags)
		printf(" flags 0x%x", zst->zst_swflags);

	if (zst->zst_hwflags & ZS_HWFLAG_CONSOLE)
		printf(": console");
	else {
#ifdef KGDB
		/*
		 * Allow kgdb to "take over" this port.  If this port is
		 * NOT the kgdb port, zs_check_kgdb() will return zero.
		 * If it IS the kgdb port, it will print "kgdb,...\n"
		 * and then return non-zero.
		 */
		if (zs_check_kgdb(cs, dev)) {
			/*
			 * This is the kgdb port (exclusive use)
			 * so skip the normal attach code.
			 */
			return;
		}
#endif
	}
	printf("\n");

	tp = ttymalloc(0);
	tp->t_dev = dev;
	tp->t_oproc = zsstart;
	tp->t_param = zsparam;
	tp->t_hwiflow = zshwiflow;

	zst->zst_tty = tp;
	zst->zst_rbhiwat =  zstty_rbuf_size;	/* impossible value */
	zst->zst_ringmask = zstty_rbuf_size - 1;
	zst->zst_rbuf = malloc(zstty_rbuf_size * sizeof(zst->zst_rbuf[0]),
			      M_DEVBUF, M_WAITOK);

	/*
	 * Hardware init
	 */
	if (zst->zst_hwflags & ZS_HWFLAG_CONSOLE) {
		/* This unit is the console. */
		zst->zst_swflags |= TIOCFLAG_SOFTCAR;
		/* Call _param so interrupts get enabled. */
		cs->cs_defspeed = zs_getspeed(cs);
		tp->t_ispeed = cs->cs_defspeed;
		tp->t_ospeed = cs->cs_defspeed;
		tp->t_cflag = ZSTTY_DEF_CFLAG;
		(void) zsparam(tp, &tp->t_termios);
	} else {
		/* Not the console; may need reset. */
		int reset, s;
		reset = (channel == 0) ?
			ZSWR9_A_RESET : ZSWR9_B_RESET;
		s = splzs();
		zs_write_reg(cs, 9, reset);
		splx(s);
	}

	/*
	 * Initialize state of modem control lines (DTR).
	 * If softcar is set, turn on DTR now and leave it.
	 * otherwise, turn off DTR now, and raise in open.
	 * (Keeps modem from answering too early.)
	 */
	zs_modem(zst, (zst->zst_swflags & TIOCFLAG_SOFTCAR) ? 1 : 0);
}


/*
 * Return pointer to our tty.
 */
struct tty *
zstty(dev)
	dev_t dev;
{
	struct zstty_softc *zst;
	int unit = minor(dev);

#ifdef	DIAGNOSTIC
	if (unit >= zstty_cd.cd_ndevs)
		panic("zstty");
#endif
	zst = zstty_cd.cd_devs[unit];
	return (zst->zst_tty);
}


/*
 * Open a zs serial (tty) port.
 */
int
zsopen(dev, flags, mode, p)
	dev_t dev;
	int flags;
	int mode;
	struct proc *p;
{
	register struct tty *tp;
	register struct zs_chanstate *cs;
	struct zstty_softc *zst;
	int error, s, unit;

	unit = minor(dev);
	if (unit >= zstty_cd.cd_ndevs)
		return (ENXIO);
	zst = zstty_cd.cd_devs[unit];
	if (zst == NULL)
		return (ENXIO);
	tp = zst->zst_tty;
	cs = zst->zst_cs;

	/* If KGDB took the line, then tp==NULL */
	if (tp == NULL)
		return (EBUSY);

	/* It's simpler to do this up here. */
	if (((tp->t_state & (TS_ISOPEN | TS_XCLUDE))
	     ==             (TS_ISOPEN | TS_XCLUDE))
	    && (suser(p, 0) != 0) )
	{
		return (EBUSY);
	}

	s = spltty();

	if ((tp->t_state & TS_ISOPEN) == 0) {
		/* First open. */
		ttychars(tp);
		tp->t_iflag = TTYDEF_IFLAG;
		tp->t_oflag = TTYDEF_OFLAG;
		tp->t_cflag = ZSTTY_DEF_CFLAG;
		if (zst->zst_swflags & TIOCFLAG_CLOCAL)
			tp->t_cflag |= CLOCAL;
		if (zst->zst_swflags & TIOCFLAG_CRTSCTS)
			tp->t_cflag |= CRTSCTS;
		if (zst->zst_swflags & TIOCFLAG_MDMBUF)
			tp->t_cflag |= MDMBUF;
		tp->t_lflag = TTYDEF_LFLAG;
		tp->t_ispeed = tp->t_ospeed = cs->cs_defspeed;
		(void) zsparam(tp, &tp->t_termios);
		ttsetwater(tp);
		/* Flush any pending input. */
		zst->zst_rbget = zst->zst_rbput;
		zs_iflush(cs);	/* XXX */
		/* Turn on DTR */
		zs_modem(zst, 1);
		if (zst->zst_swflags & TIOCFLAG_SOFTCAR) {
			tp->t_state |= TS_CARR_ON;
		}
	}
	error = 0;

	/* Wait for carrier. */
	for (;;) {

		/* Might never get status intr if carrier already on. */
		cs->cs_rr0 = zs_read_csr(cs);
		if (cs->cs_rr0 & ZSRR0_DCD) {
			tp->t_state |= TS_CARR_ON;
			break;
		}

		if ((tp->t_state & TS_CARR_ON) ||
		    (tp->t_cflag & CLOCAL) ||
		    (flags & O_NONBLOCK) )
		{
			break;
		}

		tp->t_state |= TS_WOPEN;
		error = ttysleep(tp, (caddr_t)&tp->t_rawq,
			TTIPRI | PCATCH, ttopen, 0);
		if (error) {
			if ((tp->t_state & TS_ISOPEN) == 0) {
				/* Never get here with softcar */
				zs_modem(zst, 0);
				tp->t_state &= ~TS_WOPEN;
				ttwakeup(tp);
			}
			break;
		}
	}

	splx(s);

	if (error == 0)
		error = linesw[tp->t_line].l_open(dev, tp, p);

	return (error);
}

/*
 * Close a zs serial port.
 */
int
zsclose(dev, flags, mode, p)
	dev_t dev;
	int flags;
	int mode;
	struct proc *p;
{
	struct zstty_softc *zst;
	register struct zs_chanstate *cs;
	register struct tty *tp;
	int hup;

	zst = zstty_cd.cd_devs[minor(dev)];
	cs = zst->zst_cs;
	tp = zst->zst_tty;

	/* XXX This is for cons.c. */
	if ((tp->t_state & TS_ISOPEN) == 0)
		return 0;

	(*linesw[tp->t_line].l_close)(tp, flags, p);
	hup = tp->t_cflag & HUPCL;
	if (zst->zst_swflags & TIOCFLAG_SOFTCAR)
		hup = 0;
	if (hup) {
		zs_modem(zst, 0);
		/* hold low for 1 second */
		(void) tsleep((caddr_t)cs, TTIPRI, ttclos, hz);
	}
	if (cs->cs_creg[5] & ZSWR5_BREAK) {
		zs_break(cs, 0);
	}
	/* XXX - turn off interrupts? */

	ttyclose(tp);
	return (0);
}

/*
 * Read/write zs serial port.
 */
int
zsread(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{
	register struct zstty_softc *zst;
	register struct tty *tp;

	zst = zstty_cd.cd_devs[minor(dev)];
	tp = zst->zst_tty;
	return (linesw[tp->t_line].l_read(tp, uio, flags));
}

int
zswrite(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{
	register struct zstty_softc *zst;
	register struct tty *tp;

	zst = zstty_cd.cd_devs[minor(dev)];
	tp = zst->zst_tty;
	return (linesw[tp->t_line].l_write(tp, uio, flags));
}

#define TIOCFLAG_ALL (TIOCFLAG_SOFTCAR | TIOCFLAG_CLOCAL | \
                      TIOCFLAG_CRTSCTS | TIOCFLAG_MDMBUF )

int
zsioctl(dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	register struct zstty_softc *zst;
	register struct zs_chanstate *cs;
	register struct tty *tp;
	register int error, tmp;

	zst = zstty_cd.cd_devs[minor(dev)];
	cs = zst->zst_cs;
	tp = zst->zst_tty;

	error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag, p);
	if (error >= 0)
		return (error);
	error = ttioctl(tp, cmd, data, flag, p);
	if (error >= 0)
		return (error);

	switch (cmd) {

	case TIOCSBRK:
		zs_break(cs, 1);
		break;

	case TIOCCBRK:
		zs_break(cs, 0);
		break;

	case TIOCGFLAGS:
		*(int *)data = zst->zst_swflags;
		break;

	case TIOCSFLAGS:
		error = suser(p, 0);
		if (error != 0)
			return (EPERM);
		tmp = *(int *)data;
		/* Check for random bits... */
		if (tmp & ~TIOCFLAG_ALL)
			return(EINVAL);
		/* Silently enforce softcar on the console. */
		if (zst->zst_hwflags & ZS_HWFLAG_CONSOLE)
			tmp |= TIOCFLAG_SOFTCAR;
		/* These flags take effect during open. */
		zst->zst_swflags = tmp;
		break;

	case TIOCSDTR:
		zs_modem(zst, 1);
		break;

	case TIOCCDTR:
		zs_modem(zst, 0);
		break;

	case TIOCMSET:
	case TIOCMBIS:
	case TIOCMBIC:
	case TIOCMGET:
	default:
		return (ENOTTY);
	}
	return (0);
}

/*
 * Start or restart transmission.
 */
static void
zsstart(tp)
	register struct tty *tp;
{
	register struct zstty_softc *zst;
	register struct zs_chanstate *cs;
	register int s, nch;

	zst = zstty_cd.cd_devs[minor(tp->t_dev)];
	cs = zst->zst_cs;

	s = spltty();

	/*
	 * If currently active or delaying, no need to do anything.
	 */
	if (tp->t_state & (TS_TIMEOUT | TS_BUSY | TS_TTSTOP))
		goto out;

	/*
	 * If under CRTSCTS hfc and halted, do nothing
	 */
	if (tp->t_cflag & CRTSCTS)
		if (zst->zst_tx_stopped)
			goto out;

	/*
	 * If there are sleepers, and output has drained below low
	 * water mark, awaken.
	 */
	ttwakeupwr(tp);

	nch = ndqb(&tp->t_outq, 0);	/* XXX */
	(void) splzs();

	if (nch) {
		register char *p = tp->t_outq.c_cf;

		/* mark busy, enable tx done interrupts, & send first byte */
		tp->t_state |= TS_BUSY;
		zst->zst_tx_busy = 1;
		cs->cs_preg[1] |= ZSWR1_TIE;
		cs->cs_creg[1] = cs->cs_preg[1];
		zs_write_reg(cs, 1, cs->cs_creg[1]);
		zs_write_data(cs, *p);
		zst->zst_tba = p + 1;
		zst->zst_tbc = nch - 1;
	} else {
		/*
		 * Nothing to send, turn off transmit done interrupts.
		 * This is useful if something is doing polled output.
		 */
		cs->cs_preg[1] &= ~ZSWR1_TIE;
		cs->cs_creg[1] = cs->cs_preg[1];
		zs_write_reg(cs, 1, cs->cs_creg[1]);
	}
out:
	splx(s);
}

/*
 * Stop output, e.g., for ^S or output flush.
 */
int
zsstop(tp, flag)
	struct tty *tp;
	int flag;
{
	register struct zstty_softc *zst;
	register struct zs_chanstate *cs;
	register int s;

	zst = zstty_cd.cd_devs[minor(tp->t_dev)];
	cs = zst->zst_cs;

	s = splzs();
	if (tp->t_state & TS_BUSY) {
		/*
		 * Device is transmitting; must stop it.
		 * Also clear _heldtbc to prevent any
		 * flow-control event from resuming.
		 */
		zst->zst_tbc = 0;
		zst->zst_heldtbc = 0;
		if ((tp->t_state & TS_TTSTOP) == 0)
			tp->t_state |= TS_FLUSH;
	}
	splx(s);
	return (0);
}

/*
 * Set ZS tty parameters from termios.
 * XXX - Should just copy the whole termios after
 * making sure all the changes could be done.
 * XXX - Only whack the UART when params change...
 */
static int
zsparam(tp, t)
	register struct tty *tp;
	register struct termios *t;
{
	register struct zstty_softc *zst;
	register struct zs_chanstate *cs;
	register int s, bps, cflag, tconst;
	u_char tmp3, tmp4, tmp5;

	zst = zstty_cd.cd_devs[minor(tp->t_dev)];
	cs = zst->zst_cs;

	/* XXX: Need to use an MD function for this. */
	bps = t->c_ospeed;
	if (bps < 0 || (t->c_ispeed && t->c_ispeed != bps))
		return (EINVAL);
	if (bps == 0) {
		/* stty 0 => drop DTR and RTS */
		zs_modem(zst, 0);
		return (0);
	}
	tconst = BPS_TO_TCONST(cs->cs_brg_clk, bps);
	if (tconst < 0)
		return (EINVAL);

	/* Convert back to make sure we can do it. */
	bps = TCONST_TO_BPS(cs->cs_brg_clk, tconst);
	if (bps != t->c_ospeed)
		return (EINVAL);
	tp->t_ispeed = tp->t_ospeed = bps;

	cflag = t->c_cflag;
	tp->t_cflag = cflag;

	/*
	 * Block interrupts so that state will not
	 * be altered until we are done setting it up.
	 */
	s = splzs();

	/*
	 * Initial values in cs_preg are set before
	 * our attach routine is called.  The master
	 * interrupt enable is handled by zsc.c
	 */

	cs->cs_preg[12] = tconst;
	cs->cs_preg[13] = tconst >> 8;

	switch (cflag & CSIZE) {
	case CS5:
		tmp3 = ZSWR3_RX_5;
		tmp5 = ZSWR5_TX_5;
		break;
	case CS6:
		tmp3 = ZSWR3_RX_6;
		tmp5 = ZSWR5_TX_6;
		break;
	case CS7:
		tmp3 = ZSWR3_RX_7;
		tmp5 = ZSWR5_TX_7;
		break;
	case CS8:
	default:
		tmp3 = ZSWR3_RX_8;
		tmp5 = ZSWR5_TX_8;
		break;
	}

	cs->cs_preg[3] = tmp3 | ZSWR3_RX_ENABLE;
	cs->cs_preg[5] = tmp5 | ZSWR5_TX_ENABLE | ZSWR5_DTR | ZSWR5_RTS;

	tmp4 = ZSWR4_CLK_X16 | (cflag & CSTOPB ? ZSWR4_TWOSB : ZSWR4_ONESB);
	if ((cflag & PARODD) == 0)
		tmp4 |= ZSWR4_EVENP;
	if (cflag & PARENB)
		tmp4 |= ZSWR4_PARENB;
	cs->cs_preg[4] = tmp4;

	/*
	 * Output hardware flow control on the chip is horrendous:
	 * if carrier detect drops, the receiver is disabled.
	 * Therefore, NEVER set the HFC bit, and instead use
	 * the status interrupts to detect CTS changes.
	 */
	if (cflag & CRTSCTS) {
		zst->zst_rbhiwat = zstty_rbuf_hiwat;
		cs->cs_preg[15] |= ZSWR15_CTS_IE;
	} else {
		zst->zst_rbhiwat = zstty_rbuf_size; /* impossible value */
		cs->cs_preg[15] &= ~ZSWR15_CTS_IE;
	}

	/*
	 * If nothing is being transmitted, set up new current values,
	 * else mark them as pending.
	 */
	if (cs->cs_heldchange == 0) {
		if (zst->zst_tx_busy) {
			zst->zst_heldtbc = zst->zst_tbc;
			zst->zst_tbc = 0;
			cs->cs_heldchange = 0xFF; /* XXX */
		} else {
			zs_loadchannelregs(cs);
		}
	}
	splx(s);
	return (0);
}

/*
 * Raise or lower modem control (DTR/RTS) signals.  If a character is
 * in transmission, the change is deferred.
 */
static void
zs_modem(zst, onoff)
	struct zstty_softc *zst;
	int onoff;
{
	struct zs_chanstate *cs;
	struct tty *tp;
	int s, bis, and;

	cs = zst->zst_cs;
	tp = zst->zst_tty;

	if (onoff) {
		bis = ZSWR5_DTR | ZSWR5_RTS;
		and = ~0;
	} else {
		bis = 0;
		and = ~(ZSWR5_DTR | ZSWR5_RTS);
	}
	s = splzs();
	cs->cs_preg[5] = (cs->cs_preg[5] | bis) & and;
	if (cs->cs_heldchange == 0) {
		if (zst->zst_tx_busy) {
			zst->zst_heldtbc = zst->zst_tbc;
			zst->zst_tbc = 0;
			cs->cs_heldchange = (1<<5);
		} else {
			cs->cs_creg[5] = cs->cs_preg[5];
			zs_write_reg(cs, 5, cs->cs_creg[5]);
		}
	}
	splx(s);
}

/*
 * Try to block or unblock input using hardware flow-control.
 * This is called by kern/tty.c if MDMBUF|CRTSCTS is set, and
 * if this function returns non-zero, the TS_TBLOCK flag will
 * be set or cleared according to the "stop" arg passed.
 */
int
zshwiflow(tp, stop)
	struct tty *tp;
	int stop;
{
	register struct zstty_softc *zst;
	int s;

	zst = zstty_cd.cd_devs[minor(tp->t_dev)];

	s = splzs();
	if (stop) {
		/*
		 * The tty layer is asking us to block input.
		 * If we already did it, just return TRUE.
		 */
		if (zst->zst_rx_blocked)
			goto out;
		zst->zst_rx_blocked = 1;
	} else {
		/*
		 * The tty layer is asking us to resume input.
		 * The input ring is always empty by now.
		 */
		zst->zst_rx_blocked = 0;
	}
	zs_hwiflow(zst, stop);
 out:
	splx(s);
	return 1;
}

/*
 * Internal version of zshwiflow
 * called at splzs
 */
static void
zs_hwiflow(zst, stop)
	register struct zstty_softc *zst;
	int stop;
{
	register struct zs_chanstate *cs;
	register struct tty *tp;
	register int bis, and;

	cs = zst->zst_cs;
	tp = zst->zst_tty;

	if (stop) {
		/* Block input (Lower RTS) */
		bis = 0;
		and = ~ZSWR5_RTS;
	} else {
		/* Unblock input (Raise RTS) */
		bis = ZSWR5_RTS;
		and = ~0;
	}

	cs->cs_preg[5] = (cs->cs_preg[5] | bis) & and;
	if (cs->cs_heldchange == 0) {
		if (zst->zst_tx_busy) {
			zst->zst_heldtbc = zst->zst_tbc;
			zst->zst_tbc = 0;
			cs->cs_heldchange = (1<<5);
		} else {
			cs->cs_creg[5] = cs->cs_preg[5];
			zs_write_reg(cs, 5, cs->cs_creg[5]);
		}
	}
}


/****************************************************************
 * Interface to the lower layer (zscc)
 ****************************************************************/


/*
 * receiver ready interrupt.
 * called at splzs
 */
static void
zstty_rxint(cs)
	register struct zs_chanstate *cs;
{
	register struct zstty_softc *zst;
	register int cc, put, put_next, ringmask;
	register u_char c, rr0, rr1;
	register u_short ch_rr1;

	zst = cs->cs_private;
	put = zst->zst_rbput;
	ringmask = zst->zst_ringmask;

nextchar:

	/*
	 * First read the status, because reading the received char
	 * destroys the status of this char.
	 */
	rr1 = zs_read_reg(cs, 1);
	c = zs_read_data(cs);
	ch_rr1 = (c << 8) | rr1;

	if (ch_rr1 & (ZSRR1_FE | ZSRR1_DO | ZSRR1_PE)) {
		/* Clear the receive error. */
		zs_write_csr(cs, ZSWR0_RESET_ERRORS);
	}

	/* XXX: Check for the stop character? */

	zst->zst_rbuf[put] = ch_rr1;
	put_next = (put + 1) & ringmask;

	/* Would overrun if increment makes (put==get). */
	if (put_next == zst->zst_rbget) {
		zst->zst_rx_overrun = 1;
	} else {
		/* OK, really increment. */
		put = put_next;
	}

	/* Keep reading until the FIFO is empty. */
	rr0 = zs_read_csr(cs);
	if (rr0 & ZSRR0_RX_READY)
		goto nextchar;

	/* Done reading. */
	zst->zst_rbput = put;

	/*
	 * If ring is getting too full, try to block input.
	 */
	cc = put - zst->zst_rbget;
	if (cc < 0)
		cc += zstty_rbuf_size;
	if ((cc > zst->zst_rbhiwat) && (zst->zst_rx_blocked == 0)) {
		zst->zst_rx_blocked = 1;
		zs_hwiflow(zst, 1);
	}

	/* Ask for softint() call. */
	cs->cs_softreq = 1;
}

/*
 * transmitter ready interrupt.  (splzs)
 */
static void
zstty_txint(cs)
	register struct zs_chanstate *cs;
{
	register struct zstty_softc *zst;
	register int count;

	zst = cs->cs_private;

	/*
	 * If we suspended output for a "held" change,
	 * then handle that now and resume.
	 * Do flow-control changes ASAP.
	 * When the only change is for flow control,
	 * avoid hitting other registers, because that
	 * often makes the stupid zs drop input...
	 */
	if (cs->cs_heldchange) {
		if (cs->cs_heldchange == (1<<5)) {
			/* Avoid whacking the chip... */
			cs->cs_creg[5] = cs->cs_preg[5];
			zs_write_reg(cs, 5, cs->cs_creg[5]);
		} else
			zs_loadchannelregs(cs);
		cs->cs_heldchange = 0;
		count = zst->zst_heldtbc;
	} else
		count = zst->zst_tbc;

	/*
	 * If our transmit buffer still has data,
	 * just send the next character.
	 */
	if (count > 0) {
		/* Send the next char. */
		zst->zst_tbc = --count;
		zs_write_data(cs, *zst->zst_tba);
		zst->zst_tba++;
		return;
	}

	zs_write_csr(cs, ZSWR0_RESET_TXINT);

	/* Ask the softint routine for more output. */
	zst->zst_tx_busy = 0;
	zst->zst_tx_done = 1;
	cs->cs_softreq = 1;
}

/*
 * status change interrupt.  (splzs)
 */
static void
zstty_stint(cs)
	register struct zs_chanstate *cs;
{
	register struct zstty_softc *zst;
	register struct tty *tp;
	register u_char rr0;

	zst = cs->cs_private;
	tp  = zst->zst_tty;

	rr0 = zs_read_csr(cs);
	zs_write_csr(cs, ZSWR0_RESET_STATUS);

	/*
	 * Check here for console break, so that we can abort
	 * even when interrupts are locking up the machine.
	 */
	if ((rr0 & ZSRR0_BREAK) &&
		(zst->zst_hwflags & ZS_HWFLAG_CONSOLE))
	{
		zs_abort();
		return;
	}

	/*
	 * Need to handle CTS output flow control here.
	 * Output remains stopped as long as either the
	 * zst_tx_stopped or TS_TTSTOP flag is set.
	 * Never restart here; the softint routine will
	 * do that after things are ready to move.
	 */
	if (((rr0 & ZSRR0_CTS) == 0) && (tp->t_cflag & CRTSCTS)) {
		zst->zst_tbc = 0;
		zst->zst_heldtbc = 0;
		zst->zst_tx_stopped = 1;
	}

	/*
	 * We have to accumulate status line changes here.
	 * Otherwise, if we get multiple status interrupts
	 * before the softint runs, we could fail to notice
	 * some status line changes in the softint routine.
	 * Fix from Bill Studenmund, October 1996.
	 */
	cs->cs_rr0_delta |= (cs->cs_rr0 ^ rr0);

	ttytstamp(tp, cs->cs_rr0 & ZSRR0_CTS, rr0 & ZSRR0_CTS,
	    cs->cs_rr0 & ZSRR0_DCD, rr0 & ZSRR0_DCD);

	cs->cs_rr0 = rr0;
	zst->zst_st_check = 1;

	/* Ask for softint() call. */
	cs->cs_softreq = 1;
}

/*
 * Print out a ring or fifo overrun error message.
 */
static void
zsoverrun(zst, ptime, what)
	struct zstty_softc *zst;
	long *ptime;
	char *what;
{

	if (*ptime != time_second) {
		*ptime = time_second;
		log(LOG_WARNING, "%s: %s overrun\n",
			zst->zst_dev.dv_xname, what);
	}
}

/*
 * Software interrupt.  Called at zssoft
 *
 * The main job to be done here is to empty the input ring
 * by passing its contents up to the tty layer.  The ring is
 * always emptied during this operation, therefore the ring
 * must not be larger than the space after "high water" in
 * the tty layer, or the tty layer might drop our input.
 *
 * Note: an "input blockage" condition is assumed to exist if
 * EITHER the TS_TBLOCK flag or zst_rx_blocked flag is set.
 */
static void
zstty_softint(cs)
	struct zs_chanstate *cs;
{
	register struct zstty_softc *zst;
	register struct linesw *line;
	register struct tty *tp;
	register int get, c, s;
	int ringmask, overrun;
	register u_short ring_data;
	register u_char rr0, delta;

	zst  = cs->cs_private;
	tp   = zst->zst_tty;
	line = &linesw[tp->t_line];
	ringmask = zst->zst_ringmask;
	overrun = 0;

	/*
	 * Raise to tty priority while servicing the ring.
	 */
	s = spltty();

	if (zst->zst_rx_overrun) {
		zst->zst_rx_overrun = 0;
		zsoverrun(zst, &zst->zst_rotime, "ring");
	}

	/*
	 * Copy data from the receive ring into the tty layer.
	 */
	get = zst->zst_rbget;
	while (get != zst->zst_rbput) {
		ring_data = zst->zst_rbuf[get];
		get = (get + 1) & ringmask;

		if (ring_data & ZSRR1_DO)
			overrun++;
		/* low byte of ring_data is rr1 */
		c = (ring_data >> 8) & 0xff;
		if (ring_data & ZSRR1_FE)
			c |= TTY_FE;
		if (ring_data & ZSRR1_PE)
			c |= TTY_PE;

		line->l_rint(c, tp);
	}
	zst->zst_rbget = get;

	/*
	 * If the overrun flag is set now, it was set while
	 * copying char/status pairs from the ring, which
	 * means this was a hardware (fifo) overrun.
	 */
	if (overrun) {
		zsoverrun(zst, &zst->zst_fotime, "fifo");
	}

	/*
	 * We have emptied the input ring.  Maybe unblock input.
	 * Note: an "input blockage" condition is assumed to exist
	 * when EITHER zst_rx_blocked or the TS_TBLOCK flag is set,
	 * so unblock here ONLY if TS_TBLOCK has not been set.
	 */
	if (zst->zst_rx_blocked && ((tp->t_state & TS_TBLOCK) == 0)) {
		(void) splzs();
		zst->zst_rx_blocked = 0;
		zs_hwiflow(zst, 0);	/* unblock input */
		(void) spltty();
	}

	/*
	 * Do any deferred work for status interrupts.
	 * The rr0 was saved in the h/w interrupt to
	 * avoid another splzs in here.
	 */
	if (zst->zst_st_check) {
		zst->zst_st_check = 0;

		rr0 = cs->cs_rr0;
		delta = cs->cs_rr0_delta;
		cs->cs_rr0_delta = 0;
		if (delta & ZSRR0_DCD) {
			c = ((rr0 & ZSRR0_DCD) != 0);
			if (line->l_modem(tp, c) == 0)
				zs_modem(zst, c);
		}
		if ((delta & ZSRR0_CTS) && (tp->t_cflag & CRTSCTS)) {
			/*
			 * Only do restart here.  Stop is handled
			 * at the h/w interrupt level.
			 */
			if (rr0 & ZSRR0_CTS) {
				zst->zst_tx_stopped = 0;
				tp->t_state &= ~TS_TTSTOP;
				(*line->l_start)(tp);
			}
		}
	}

	if (zst->zst_tx_done) {
		zst->zst_tx_done = 0;
		tp->t_state &= ~TS_BUSY;
		if (tp->t_state & TS_FLUSH)
			tp->t_state &= ~TS_FLUSH;
		else
			ndflush(&tp->t_outq, zst->zst_tba -
				(caddr_t) tp->t_outq.c_cf);
		line->l_start(tp);
	}

	splx(s);
}

struct zsops zsops_tty = {
	zstty_rxint,	/* receive char available */
	zstty_stint,	/* external/status */
	zstty_txint,	/* xmit buffer empty */
	zstty_softint,	/* process software interrupt */
};

