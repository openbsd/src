/*	$NetBSD: ms.c,v 1.1.1.1 1996/01/24 01:15:35 gwr Exp $	*/

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
 *	@(#)ms.c	8.1 (Berkeley) 6/11/93
 */

/*
 * Mouse driver (/dev/mouse)
 */

/*
 * Zilog Z8530 Dual UART driver (mouse interface)
 *
 * This is the "slave" driver that will be attached to
 * the "zsc" driver for a Sun mouse.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/ioctl.h>
#include <sys/kernel.h>
#include <sys/syslog.h>

#include <dev/ic/z8530reg.h>
#include <machine/z8530var.h>
#include <machine/vuid_event.h>

#include "event_var.h"

/*
 * How many input characters we can buffer.
 * The port-specific var.h may override this.
 * Note: must be a power of two!
 */
#define	MS_RX_RING_SIZE	256
#define MS_RX_RING_MASK (MS_RX_RING_SIZE-1)
/*
 * Output buffer.  Only need a few chars.
 */
#define	MS_TX_RING_SIZE	16
#define MS_TX_RING_MASK (MS_TX_RING_SIZE-1)
/*
 * Keyboard serial line speed is fixed at 1200 bps.
 */
#define MS_BPS 1200

/*
 * Mouse state.  A Mouse Systems mouse is a fairly simple device,
 * producing five-byte blobs of the form:
 *
 *	b dx dy dx dy
 *
 * where b is the button state, encoded as 0x80|(~buttons)---there are
 * three buttons (4=left, 2=middle, 1=right)---and dx,dy are X and Y
 * delta values, none of which have are in [0x80..0x87].  (This lets
 * us sync up with the mouse after an error.)
 */
struct ms_softc {
	struct	device ms_dev;		/* required first: base device */
	struct	zs_chanstate *ms_cs;

	/* Flags to communicate with ms_softintr() */
	volatile int ms_intr_flags;
#define	INTR_RX_OVERRUN 1
#define INTR_TX_EMPTY   2
#define INTR_ST_CHECK   4

	/*
	 * The receive ring buffer.
	 */
	u_int	ms_rbget;	/* ring buffer `get' index */
	volatile u_int	ms_rbput;	/* ring buffer `put' index */
	u_short	ms_rbuf[MS_RX_RING_SIZE]; /* rr1, data pairs */

	/*
	 * State of input translator
	 */
	short	ms_byteno;		/* input byte number, for decode */
	char	ms_mb;			/* mouse button state */
	char	ms_ub;			/* user button state */
	int	ms_dx;			/* delta-x */
	int	ms_dy;			/* delta-y */

	/*
	 * State of upper interface.
	 */
	volatile int ms_ready;		/* event queue is ready */
	struct	evvar ms_events;	/* event queue state */
} ms_softc;

cdev_decl(ms);	/* open, close, read, write, ioctl, stop, ... */

struct zsops zsops_ms;

/****************************************************************
 * Definition of the driver for autoconfig.
 ****************************************************************/

static int	ms_match(struct device *, void *, void *);
static void	ms_attach(struct device *, struct device *, void *);

struct cfdriver mscd = {
	NULL, "ms", ms_match, ms_attach,
	DV_DULL, sizeof(struct ms_softc), NULL,
};


/*
 * ms_match: how is this zs channel configured?
 */
int 
ms_match(parent, match, aux)
	struct device *parent;
	void   *match, *aux;
{
	struct cfdata *cf = match;
	struct zsc_attach_args *args = aux;

	/* Exact match required for keyboard. */
	if (cf->cf_loc[0] == args->channel)
		return 2;

	return 0;
}

void 
ms_attach(parent, self, aux)
	struct device *parent, *self;
	void   *aux;

{
	struct zsc_softc *zsc = (void *) parent;
	struct ms_softc *ms = (void *) self;
	struct zsc_attach_args *args = aux;
	struct zs_chanstate *cs;
	struct cfdata *cf;
	int channel, ms_unit;
	int reset, s, tconst;

	cf = ms->ms_dev.dv_cfdata;
	ms_unit = cf->cf_unit;
	channel = args->channel;
	cs = &zsc->zsc_cs[channel];
	cs->cs_private = ms;
	cs->cs_ops = &zsops_ms;
	ms->ms_cs = cs;

	printf("\n");

	/* Initialize the speed, etc. */
	tconst = BPS_TO_TCONST(cs->cs_pclk_div16, MS_BPS);
	s = splzs();
	/* May need reset... */
	reset = (channel == 0) ?
		ZSWR9_A_RESET : ZSWR9_B_RESET;
	ZS_WRITE(cs, 9, reset);
	/* These are OK as set by zscc: WR3, WR4, WR5 */
	cs->cs_preg[5] |= ZSWR5_DTR | ZSWR5_RTS;
	cs->cs_preg[12] = tconst;
	cs->cs_preg[13] = tconst >> 8;
	zs_loadchannelregs(cs);
	splx(s);

	/* Initialize translator. */
	ms->ms_byteno = -1;
}

/****************************************************************
 *  Entry points for /dev/mouse
 *  (open,close,read,write,...)
 ****************************************************************/

int
msopen(dev, flags, mode, p)
	dev_t dev;
	int flags, mode;
	struct proc *p;
{
	struct ms_softc *ms;
	int error, s, unit;

	unit = minor(dev);
	if (unit >= mscd.cd_ndevs)
		return (ENXIO);
	ms = mscd.cd_devs[unit];
	if (ms == NULL)
		return (ENXIO);

	/* This is an exclusive open device. */
	if (ms->ms_events.ev_io)
		return (EBUSY);
	ms->ms_events.ev_io = p;
	ev_init(&ms->ms_events);	/* may cause sleep */

	ms->ms_ready = 1;		/* start accepting events */
	return (0);
}

int
msclose(dev, flags, mode, p)
	dev_t dev;
	int flags, mode;
	struct proc *p;
{
	struct ms_softc *ms;

	ms = mscd.cd_devs[minor(dev)];
	ms->ms_ready = 0;		/* stop accepting events */
	ev_fini(&ms->ms_events);

	ms->ms_events.ev_io = NULL;
	return (0);
}

int
msread(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{
	struct ms_softc *ms;

	ms = mscd.cd_devs[minor(dev)];
	return (ev_read(&ms->ms_events, uio, flags));
}

/* this routine should not exist, but is convenient to write here for now */
int
mswrite(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{

	return (EOPNOTSUPP);
}

int
msioctl(dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	register caddr_t data;
	int flag;
	struct proc *p;
{
	struct ms_softc *ms;

	ms = mscd.cd_devs[minor(dev)];

	switch (cmd) {

	case FIONBIO:		/* we will remove this someday (soon???) */
		return (0);

	case FIOASYNC:
		ms->ms_events.ev_async = *(int *)data != 0;
		return (0);

	case TIOCSPGRP:
		if (*(int *)data != ms->ms_events.ev_io->p_pgid)
			return (EPERM);
		return (0);

	case VUIDGFORMAT:
		/* we only do firm_events */
		*(int *)data = VUID_FIRM_EVENT;
		return (0);

	case VUIDSFORMAT:
		if (*(int *)data != VUID_FIRM_EVENT)
			return (EINVAL);
		return (0);
	}
	return (ENOTTY);
}

int
msselect(dev, rw, p)
	dev_t dev;
	int rw;
	struct proc *p;
{
	struct ms_softc *ms;

	ms = mscd.cd_devs[minor(dev)];
	return (ev_select(&ms->ms_events, rw, p));
}


/****************************************************************
 * Middle layer (translator)
 ****************************************************************/

/*
 * Called by our ms_softint() routine on input.
 */
void
ms_input(ms, c)
	register struct ms_softc *ms;
	register int c;
{
	register struct firm_event *fe;
	register int mb, ub, d, get, put, any;
	static const char to_one[] = { 1, 2, 2, 4, 4, 4, 4 };
	static const int to_id[] = { MS_RIGHT, MS_MIDDLE, 0, MS_LEFT };

	/*
	 * Discard input if not ready.  Drop sync on parity or framing
	 * error; gain sync on button byte.
	 */
	if (ms->ms_ready == 0)
		return;
	if (c == -1) {
		ms->ms_byteno = -1;
		return;
	}
	if ((c & ~7) == 0x80)	/* if in 0x80..0x87 */
		ms->ms_byteno = 0;

	/*
	 * Run the decode loop, adding to the current information.
	 * We add, rather than replace, deltas, so that if the event queue
	 * fills, we accumulate data for when it opens up again.
	 */
	switch (ms->ms_byteno) {

	case -1:
		return;

	case 0:
		/* buttons */
		ms->ms_byteno = 1;
		ms->ms_mb = (~c) & 0x7;
		return;

	case 1:
		/* first delta-x */
		ms->ms_byteno = 2;
		ms->ms_dx += (char)c;
		return;

	case 2:
		/* first delta-y */
		ms->ms_byteno = 3;
		ms->ms_dy += (char)c;
		return;

	case 3:
		/* second delta-x */
		ms->ms_byteno = 4;
		ms->ms_dx += (char)c;
		return;

	case 4:
		/* second delta-x */
		ms->ms_byteno = -1;	/* wait for button-byte again */
		ms->ms_dy += (char)c;
		break;

	default:
		panic("ms_rint");
		/* NOTREACHED */
	}

	/*
	 * We have at least one event (mouse button, delta-X, or
	 * delta-Y; possibly all three, and possibly three separate
	 * button events).  Deliver these events until we are out
	 * of changes or out of room.  As events get delivered,
	 * mark them `unchanged'.
	 */
	any = 0;
	get = ms->ms_events.ev_get;
	put = ms->ms_events.ev_put;
	fe = &ms->ms_events.ev_q[put];

	/* NEXT prepares to put the next event, backing off if necessary */
#define	NEXT \
	if ((++put) % EV_QSIZE == get) { \
		put--; \
		goto out; \
	}
	/* ADVANCE completes the `put' of the event */
#define	ADVANCE \
	fe++; \
	if (put >= EV_QSIZE) { \
		put = 0; \
		fe = &ms->ms_events.ev_q[0]; \
	} \
	any = 1

	mb = ms->ms_mb;
	ub = ms->ms_ub;
	while ((d = mb ^ ub) != 0) {
		/*
		 * Mouse button change.  Convert up to three changes
		 * to the `first' change, and drop it into the event queue.
		 */
		NEXT;
		d = to_one[d - 1];		/* from 1..7 to {1,2,4} */
		fe->id = to_id[d - 1];		/* from {1,2,4} to ID */
		fe->value = mb & d ? VKEY_DOWN : VKEY_UP;
		fe->time = time;
		ADVANCE;
		ub ^= d;
	}
	if (ms->ms_dx) {
		NEXT;
		fe->id = LOC_X_DELTA;
		fe->value = ms->ms_dx;
		fe->time = time;
		ADVANCE;
		ms->ms_dx = 0;
	}
	if (ms->ms_dy) {
		NEXT;
		fe->id = LOC_Y_DELTA;
		fe->value = ms->ms_dy;
		fe->time = time;
		ADVANCE;
		ms->ms_dy = 0;
	}
out:
	if (any) {
		ms->ms_ub = ub;
		ms->ms_events.ev_put = put;
		EV_WAKEUP(&ms->ms_events);
	}
}

/****************************************************************
 * Interface to the lower layer (zscc)
 ****************************************************************/

static int
ms_rxint(cs)
	register struct zs_chanstate *cs;
{
	register struct ms_softc *ms;
	register int put, put_next;
	register u_char c, rr1;

	ms = cs->cs_private;
	put = ms->ms_rbput;

	/* Read the input data ASAP. */
	c = *(cs->cs_reg_data);
	ZS_DELAY();

	/* Save the status register too. */
	rr1 = ZS_READ(cs, 1);

	if (rr1 & (ZSRR1_FE | ZSRR1_DO | ZSRR1_PE)) {
		/* Clear the receive error. */
		*(cs->cs_reg_csr) = ZSWR0_RESET_ERRORS;
		ZS_DELAY();
	}

	ms->ms_rbuf[put] = (c << 8) | rr1;
	put_next = (put + 1) & MS_RX_RING_MASK;

	/* Would overrun if increment makes (put==get). */
	if (put_next == ms->ms_rbget) {
		ms->ms_intr_flags |= INTR_RX_OVERRUN;
	} else {
		/* OK, really increment. */
		put = put_next;
	}

	/* Done reading. */
	ms->ms_rbput = put;

	/* Ask for softint() call. */
	cs->cs_softreq = 1;
	return(1);
}


static int
ms_txint(cs)
	register struct zs_chanstate *cs;
{
	register struct ms_softc *ms;
	register int count, rval;

	ms = cs->cs_private;

	*(cs->cs_reg_csr) = ZSWR0_RESET_TXINT;
	ZS_DELAY();

	ms->ms_intr_flags |= INTR_TX_EMPTY;
	/* Ask for softint() call. */
	cs->cs_softreq = 1;
	return (1);
}


static int
ms_stint(cs)
	register struct zs_chanstate *cs;
{
	register struct ms_softc *ms;
	register int rr0;

	ms = cs->cs_private;

	rr0 = *(cs->cs_reg_csr);
	ZS_DELAY();

	*(cs->cs_reg_csr) = ZSWR0_RESET_STATUS;
	ZS_DELAY();

	ms->ms_intr_flags |= INTR_ST_CHECK;
	/* Ask for softint() call. */
	cs->cs_softreq = 1;
	return (1);
}


static int
ms_softint(cs)
	struct zs_chanstate *cs;
{
	register struct ms_softc *ms;
	register int get, c, s;
	int intr_flags;
	register u_short ring_data;
	register u_char rr0, rr1;

	ms = cs->cs_private;

	/* Atomically get and clear flags. */
	s = splzs();
	intr_flags = ms->ms_intr_flags;
	ms->ms_intr_flags = 0;
	splx(s);

	/*
	 * Copy data from the receive ring to the event layer.
	 */
	get = ms->ms_rbget;
	while (get != ms->ms_rbput) {
		ring_data = ms->ms_rbuf[get];
		get = (get + 1) & MS_RX_RING_MASK;

		/* low byte of ring_data is rr1 */
		c = (ring_data >> 8) & 0xff;

		if (ring_data & ZSRR1_DO)
			intr_flags |= INTR_RX_OVERRUN;
		if (ring_data & (ZSRR1_FE | ZSRR1_PE)) {
			log(LOG_ERR, "%s: input error (0x%x)\n",
				ms->ms_dev.dv_xname, ring_data);
			c = -1;	/* signal input error */
		}

		/* Pass this up to the "middle" layer. */
		ms_input(ms, c);
	}
	if (intr_flags & INTR_RX_OVERRUN) {
		log(LOG_ERR, "%s: input overrun\n",
		    ms->ms_dev.dv_xname);
	}
	ms->ms_rbget = get;

	if (intr_flags & INTR_TX_EMPTY) {
		/*
		 * Transmit done.  (Not expected.)
		 */
		log(LOG_ERR, "%s: transmit interrupt?\n",
		    ms->ms_dev.dv_xname);
	}

	if (intr_flags & INTR_ST_CHECK) {
		/*
		 * Status line change.  (Not expected.)
		 */
		log(LOG_ERR, "%s: status interrupt?\n",
		    ms->ms_dev.dv_xname);
	}

	return (1);
}

struct zsops zsops_ms = {
	ms_rxint,	/* receive char available */
	ms_stint,	/* external/status */
	ms_txint,	/* xmit buffer empty */
	ms_softint,	/* process software interrupt */
};
