/*	$NetBSD: hpib.c,v 1.8 1996/02/14 02:44:28 thorpej Exp $	*/

/*
 * Copyright (c) 1982, 1990, 1993
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
 *
 *	@(#)hpib.c	8.2 (Berkeley) 1/12/94
 */

/*
 * HPIB driver
 */
#include "hpib.h"
#if NHPIB > 0

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>

#include <hp300/dev/device.h>
#include <hp300/dev/hpibvar.h>
#include <hp300/dev/dmavar.h>

#include <machine/cpu.h>
#include <hp300/hp300/isr.h>

int	hpibmatch __P((struct hp_ctlr *));
void	hpibattach __P((struct hp_ctlr *));
void	hpibstart __P((int));
void	hpibgo __P((int, int, int, void *, int, int, int));
void	hpibdone __P((int));
int	hpibintr __P((void *));

struct	driver hpibdriver = {
	hpibmatch,
	hpibattach,
	"hpib",
	(int(*)())hpibstart,			/* XXX */
	(int(*)())hpibgo,			/* XXX */
	hpibintr,
	(int(*)())hpibdone,			/* XXX */
};

struct	hpib_softc hpib_softc[NHPIB];

extern	int nhpibtype __P((struct hp_ctlr *));	/* XXX */
extern	int fhpibtype __P((struct hp_ctlr *));	/* XXX */
extern	void nhpibattach __P((struct hp_ctlr *));	/* XXX */
extern	void fhpibattach __P((struct hp_ctlr *));	/* XXX */

int	hpibtimeout = 100000;	/* # of status tests before we give up */
int	hpibidtimeout = 10000;	/* # of status tests for hpibid() calls */
int	hpibdmathresh = 3;	/* byte count beyond which to attempt dma */

int
hpibmatch(hc)
	register struct hp_ctlr *hc;
{
	struct hp_hw *hw = hc->hp_args;
	extern caddr_t internalhpib;

	/* Special case for internal HP-IB. */
	if ((hw->hw_sc == 7) && internalhpib)
		goto hwid_ok;

	switch (hw->hw_id) {
	case 8:			/* 98625B */
	case 128:		/* 98624A */
 hwid_ok:
		if (nhpibtype(hc) || fhpibtype(hc))
			return (1);
	}

	return (0);
}

void
hpibattach(hc)
	struct hp_ctlr *hc;
{
	struct hpib_softc *hs = &hpib_softc[hc->hp_unit];

	/*
	 * Call the appropriate "attach" routine for this controller.
	 * The type is set in the "type" routine.
	 *
	 * XXX This is, by the way, exactly backwards.
	 */
	switch (hs->sc_type) {
	case HPIBA:
	case HPIBB:
		nhpibattach(hc);
		break;

	case HPIBC:
		fhpibattach(hc);
		break;

	default:
		panic("hpibattach: unknown type 0x%x", hs->sc_type);
		/* NOTREACHED */
	}

	hs->sc_hc = hc;
	hs->sc_dq.dq_unit = hc->hp_unit;
	hs->sc_dq.dq_driver = &hpibdriver;
	hs->sc_sq.dq_forw = hs->sc_sq.dq_back = &hs->sc_sq;

	/* Establish the interrupt handler. */
	isrlink(hpibintr, hs, hc->hp_ipl, ISRPRI_BIO);

	/* Reset the controller, display what we've seen, and we're done. */
	hpibreset(hc->hp_unit);
	printf(": %s\n", hs->sc_descrip);
}

void
hpibreset(unit)
	register int unit;
{

	(hpib_softc[unit].sc_controller->hpib_reset)(unit);
}

int
hpibreq(dq)
	register struct devqueue *dq;
{
	register struct devqueue *hq;

	hq = &hpib_softc[dq->dq_ctlr].sc_sq;
	insque(dq, hq->dq_back);
	if (dq->dq_back == hq)
		return(1);
	return(0);
}

void
hpibfree(dq)
	register struct devqueue *dq;
{
	register struct devqueue *hq;

	hq = &hpib_softc[dq->dq_ctlr].sc_sq;
	remque(dq);
	if ((dq = hq->dq_forw) != hq)
		(dq->dq_driver->d_start)(dq->dq_unit);
}

int
hpibid(unit, slave)
	int unit, slave;
{
	short id;
	int ohpibtimeout;

	/*
	 * XXX shorten timeout value so autoconfig doesn't
	 * take forever on slow CPUs.
	 */
	ohpibtimeout = hpibtimeout;
	hpibtimeout = hpibidtimeout * cpuspeed;
	if (hpibrecv(unit, 31, slave, &id, 2) != 2)
		id = 0;
	hpibtimeout = ohpibtimeout;
	return(id);
}

int
hpibsend(unit, slave, sec, addr, cnt)
	int unit, slave, sec, cnt;
	void *addr;
{

	return ((hpib_softc[unit].sc_controller->hpib_send)(unit, slave,
	    sec, addr, cnt));
}

int
hpibrecv(unit, slave, sec, addr, cnt)
	int unit, slave, sec, cnt;
	void *addr;
{

	return ((hpib_softc[unit].sc_controller->hpib_recv)(unit, slave,
	    sec, addr, cnt));
}

int
hpibpptest(unit, slave)
	register int unit;
	int slave;
{

	return ((hpib_softc[unit].sc_controller->hpib_ppoll)(unit) &
	    (0x80 >> slave));
}

void
hpibppclear(unit)
	int unit;
{
	hpib_softc[unit].sc_flags &= ~HPIBF_PPOLL;
}

hpibawait(unit)
	int unit;
{
	register struct hpib_softc *hs = &hpib_softc[unit];

	hs->sc_flags |= HPIBF_PPOLL;
	(hs->sc_controller->hpib_ppwatch)((void *)unit);
}

int
hpibswait(unit, slave)
	register int unit;
	int slave;
{
	register int timo = hpibtimeout;
	register int mask, (*ppoll) __P((int));

	ppoll = hpib_softc[unit].sc_controller->hpib_ppoll;
	mask = 0x80 >> slave;
	while (((ppoll)(unit) & mask) == 0)
		if (--timo == 0) {
			printf("%s: swait timeout\n",
			    hpib_softc[unit].sc_hc->hp_xname);
			return(-1);
		}
	return(0);
}

int
hpibustart(unit)
	int unit;
{
	register struct hpib_softc *hs = &hpib_softc[unit];

	if (hs->sc_type == HPIBA)
		hs->sc_dq.dq_ctlr = DMA0;
	else
		hs->sc_dq.dq_ctlr = DMA0 | DMA1;
	if (dmareq(&hs->sc_dq))
		return(1);
	return(0);
}

void
hpibstart(unit)
	int unit;
{
	register struct devqueue *dq;
	
	dq = hpib_softc[unit].sc_sq.dq_forw;
	(dq->dq_driver->d_go)(dq->dq_unit);
}

void
hpibgo(unit, slave, sec, addr, count, rw, timo)
	int unit, slave, sec, count, rw, timo;
	void *addr;
{

	(hpib_softc[unit].sc_controller->hpib_go)(unit, slave, sec,
	    addr, count, rw, timo);
}

void
hpibdone(unit)
	register int unit;
{

	(hpib_softc[unit].sc_controller->hpib_done)(unit);
}

int
hpibintr(arg)
	void *arg;
{
	struct hpib_softc *hs = arg;

	return ((hs->sc_controller->hpib_intr)(arg));
}
#endif /* NHPIB > 0 */
