/*	$NetBSD: fhpib.c,v 1.8 1995/12/02 18:21:56 thorpej Exp $	*/

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
 *	@(#)fhpib.c	8.2 (Berkeley) 1/12/94
 */

/*
 * 98625A/B HPIB driver
 */
#include "hpib.h"
#if NHPIB > 0

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/buf.h>

#include <hp300/dev/device.h>
#include <hp300/dev/fhpibreg.h>
#include <hp300/dev/hpibvar.h>
#include <hp300/dev/dmavar.h>

/*
 * Inline version of fhpibwait to be used in places where
 * we don't worry about getting hung.
 */
#define	FHPIBWAIT(hd, m)	while (((hd)->hpib_intr & (m)) == 0) DELAY(1)

#ifdef DEBUG
int	fhpibdebugunit = -1;
int	fhpibdebug = 0;
#define FDB_FAIL	0x01
#define FDB_DMA		0x02
#define FDB_WAIT	0x04
#define FDB_PPOLL	0x08

int	dopriodma = 0;	/* use high priority DMA */
int	doworddma = 1;	/* non-zero if we should attempt word dma */
int	doppollint = 1;	/* use ppoll interrupts instead of watchdog */
int	fhpibppolldelay = 50;

long	fhpibbadint[2] = { 0 };
long	fhpibtransfer[NHPIB] = { 0 };
long	fhpibnondma[NHPIB] = { 0 };
long	fhpibworddma[NHPIB] = { 0 };
long	fhpibppollfail[NHPIB] = { 0 };
#endif

int	fhpibcmd[NHPIB];

void	fhpibreset __P((int));
int	fhpibsend __P((int, int, int, void *, int));
int	fhpibrecv __P((int, int, int, void *, int));
int	fhpibppoll __P((int));
void	fhpibppwatch __P((void *));
void	fhpibgo __P((int, int, int, void *, int, int, int));
void	fhpibdone __P((int));
int	fhpibintr __P((int));

/*
 * Our controller ops structure.
 */
struct	hpib_controller fhpib_controller = {
	fhpibreset,
	fhpibsend,
	fhpibrecv,
	fhpibppoll,
	fhpibppwatch,
	fhpibgo,
	fhpibdone,
	fhpibintr
};

int
fhpibtype(hc)
	register struct hp_ctlr *hc;
{
	register struct hpib_softc *hs = &hpib_softc[hc->hp_unit];
	register struct fhpibdevice *hd = (struct fhpibdevice *)hc->hp_addr;

	if (hd->hpib_cid != HPIBC)
		return (0);

	hs->sc_type = HPIBC;
	hc->hp_ipl = HPIB_IPL(hd->hpib_ids);

	return (1);
}

void
fhpibattach(hc)
	struct hp_ctlr *hc;
{
	register struct hpib_softc *hs = &hpib_softc[hc->hp_unit];

	if (hs->sc_type != HPIBC)
		panic("fhpibattach: unknown type 0x%x", hs->sc_type);
		/* NOTREACHED */

	hs->sc_ba = HPIBC_BA;
	hs->sc_descrip = "98625A or 98625B fast HP-IB";
	hs->sc_controller = &fhpib_controller;
}

void
fhpibreset(unit)
	int unit;
{
	register struct hpib_softc *hs = &hpib_softc[unit];
	register struct fhpibdevice *hd;

	hd = (struct fhpibdevice *)hs->sc_hc->hp_addr;
	hd->hpib_cid = 0xFF;
	DELAY(100);
	hd->hpib_cmd = CT_8BIT;
	hd->hpib_ar = AR_ARONC;
	fhpibifc(hd);
	hd->hpib_ie = IDS_IE;
	hd->hpib_data = C_DCL;
	DELAY(100000);
	/*
	 * See if we can do word dma.
	 * If so, we should be able to write and read back the appropos bit.
	 */
	hd->hpib_ie |= IDS_WDMA;
	if (hd->hpib_ie & IDS_WDMA) {
		hd->hpib_ie &= ~IDS_WDMA;
		hs->sc_flags |= HPIBF_DMA16;
#ifdef DEBUG
		if (fhpibdebug & FDB_DMA)
			printf("fhpibtype: unit %d has word dma\n", unit);

#endif
	}
}

fhpibifc(hd)
	register struct fhpibdevice *hd;
{
	hd->hpib_cmd |= CT_IFC;
	hd->hpib_cmd |= CT_INITFIFO;
	DELAY(100);
	hd->hpib_cmd &= ~CT_IFC;
	hd->hpib_cmd |= CT_REN;
	hd->hpib_stat = ST_ATN;
}

int
fhpibsend(unit, slave, sec, ptr, origcnt)
	int unit, slave, sec, origcnt;
	void *ptr;
{
	register struct hpib_softc *hs = &hpib_softc[unit];
	register struct fhpibdevice *hd;
	register int cnt = origcnt;
	register int timo;
	char *addr = ptr;

	hd = (struct fhpibdevice *)hs->sc_hc->hp_addr;
	hd->hpib_stat = 0;
	hd->hpib_imask = IM_IDLE | IM_ROOM;
	if (fhpibwait(hd, IM_IDLE) < 0)
		goto senderr;
	hd->hpib_stat = ST_ATN;
	hd->hpib_data = C_UNL;
	hd->hpib_data = C_TAG + hs->sc_ba;
	hd->hpib_data = C_LAG + slave;
	if (sec < 0) {
		if (sec == -2)		/* selected device clear KLUDGE */
			hd->hpib_data = C_SDC;
	} else
		hd->hpib_data = C_SCG + sec;
	if (fhpibwait(hd, IM_IDLE) < 0)
		goto senderr;
	if (cnt) {
		hd->hpib_stat = ST_WRITE;
		while (--cnt) {
			hd->hpib_data = *addr++;
			timo = hpibtimeout;
			while ((hd->hpib_intr & IM_ROOM) == 0) {
				if (--timo <= 0)
					goto senderr;
				DELAY(1);
			}
		}
		hd->hpib_stat = ST_EOI;
		hd->hpib_data = *addr;
		FHPIBWAIT(hd, IM_ROOM);
		hd->hpib_stat = ST_ATN;
		/* XXX: HP-UX claims bug with CS80 transparent messages */
		if (sec == 0x12)
			DELAY(150);
		hd->hpib_data = C_UNL;
		(void) fhpibwait(hd, IM_IDLE);
	}
	hd->hpib_imask = 0;
	return (origcnt);

senderr:
	hd->hpib_imask = 0;
	fhpibifc(hd);
#ifdef DEBUG
	if (fhpibdebug & FDB_FAIL) {
		printf("%s: fhpibsend failed: slave %d, sec %x, ",
		    hs->sc_hc->hp_xname, slave, sec);
		printf("sent %d of %d bytes\n", origcnt-cnt-1, origcnt);
	}
#endif
	return (origcnt - cnt - 1);
}

int
fhpibrecv(unit, slave, sec, ptr, origcnt)
	int unit, slave, sec, origcnt;
	void *ptr;
{
	register struct hpib_softc *hs = &hpib_softc[unit];
	register struct fhpibdevice *hd;
	register int cnt = origcnt;
	register int timo;
	char *addr = ptr;

	hd = (struct fhpibdevice *)hs->sc_hc->hp_addr;
	/*
	 * Slave < 0 implies continuation of a previous receive
	 * that probably timed out.
	 */
	if (slave >= 0) {
		hd->hpib_stat = 0;
		hd->hpib_imask = IM_IDLE | IM_ROOM | IM_BYTE;
		if (fhpibwait(hd, IM_IDLE) < 0)
			goto recverror;
		hd->hpib_stat = ST_ATN;
		hd->hpib_data = C_UNL;
		hd->hpib_data = C_LAG + hs->sc_ba;
		hd->hpib_data = C_TAG + slave;
		if (sec != -1)
			hd->hpib_data = C_SCG + sec;
		if (fhpibwait(hd, IM_IDLE) < 0)
			goto recverror;
		hd->hpib_stat = ST_READ0;
		hd->hpib_data = 0;
	}
	if (cnt) {
		while (--cnt >= 0) {
			timo = hpibtimeout;
			while ((hd->hpib_intr & IM_BYTE) == 0) {
				if (--timo == 0)
					goto recvbyteserror;
				DELAY(1);
			}
			*addr++ = hd->hpib_data;
		}
		FHPIBWAIT(hd, IM_ROOM);
		hd->hpib_stat = ST_ATN;
		hd->hpib_data = (slave == 31) ? C_UNA : C_UNT;
		(void) fhpibwait(hd, IM_IDLE);
	}
	hd->hpib_imask = 0;
	return (origcnt);

recverror:
	fhpibifc(hd);
recvbyteserror:
	hd->hpib_imask = 0;
#ifdef DEBUG
	if (fhpibdebug & FDB_FAIL) {
		printf("%s: fhpibrecv failed: slave %d, sec %x, ",
		    hs->sc_hc->hp_xname, slave, sec);
		printf("got %d of %d bytes\n", origcnt-cnt-1, origcnt);
	}
#endif
	return (origcnt - cnt - 1);
}

void
fhpibgo(unit, slave, sec, ptr, count, rw, timo)
	int unit, slave, sec, count, rw, timo;
	void *ptr;
{
	register struct hpib_softc *hs = &hpib_softc[unit];
	register struct fhpibdevice *hd;
	register int i;
	char *addr = ptr;
	int flags = 0;

	hd = (struct fhpibdevice *)hs->sc_hc->hp_addr;
	hs->sc_flags |= HPIBF_IO;
	if (timo)
		hs->sc_flags |= HPIBF_TIMO;
	if (rw == B_READ)
		hs->sc_flags |= HPIBF_READ;
#ifdef DEBUG
	else if (hs->sc_flags & HPIBF_READ) {
		printf("fhpibgo: HPIBF_READ still set\n");
		hs->sc_flags &= ~HPIBF_READ;
	}
#endif
	hs->sc_count = count;
	hs->sc_addr = addr;
#ifdef DEBUG
	fhpibtransfer[unit]++;
#endif
	if ((hs->sc_flags & HPIBF_DMA16) &&
	    ((int)addr & 1) == 0 && count && (count & 1) == 0
#ifdef DEBUG
	    && doworddma
#endif
	    ) {
#ifdef DEBUG
		fhpibworddma[unit]++;
#endif
		flags |= DMAGO_WORD;
		hd->hpib_latch = 0;
	}
#ifdef DEBUG
	if (dopriodma)
		flags |= DMAGO_PRI;
#endif
	if (hs->sc_flags & HPIBF_READ) {
		fhpibcmd[unit] = CT_REN | CT_8BIT;
		hs->sc_curcnt = count;
		dmago(hs->sc_dq.dq_ctlr, addr, count, flags|DMAGO_READ);
		if (fhpibrecv(unit, slave, sec, 0, 0) < 0) {
#ifdef DEBUG
			printf("fhpibgo: recv failed, retrying...\n");
#endif
			(void) fhpibrecv(unit, slave, sec, 0, 0);
		}
		i = hd->hpib_cmd;
		hd->hpib_cmd = fhpibcmd[unit];
		hd->hpib_ie = IDS_DMA(hs->sc_dq.dq_ctlr) |
			((flags & DMAGO_WORD) ? IDS_WDMA : 0);
		return;
	}
	fhpibcmd[unit] = CT_REN | CT_8BIT | CT_FIFOSEL;
	if (count < hpibdmathresh) {
#ifdef DEBUG
		fhpibnondma[unit]++;
		if (flags & DMAGO_WORD)
			fhpibworddma[unit]--;
#endif
		hs->sc_curcnt = count;
		(void) fhpibsend(unit, slave, sec, addr, count);
		fhpibdone(unit);
		return;
	}
	count -= (flags & DMAGO_WORD) ? 2 : 1;
	hs->sc_curcnt = count;
	dmago(hs->sc_dq.dq_ctlr, addr, count, flags);
	if (fhpibsend(unit, slave, sec, 0, 0) < 0) {
#ifdef DEBUG
		printf("fhpibgo: send failed, retrying...\n");
#endif
		(void) fhpibsend(unit, slave, sec, 0, 0);
	}
	i = hd->hpib_cmd;
	hd->hpib_cmd = fhpibcmd[unit];
	hd->hpib_ie = IDS_DMA(hs->sc_dq.dq_ctlr) | IDS_WRITE |
		((flags & DMAGO_WORD) ? IDS_WDMA : 0);
}

/*
 * A DMA read can finish but the device can still be waiting (MAG-tape
 * with more data than we're waiting for).  This timeout routine
 * takes care of that.  Somehow, the thing gets hosed.  For now, since
 * this should be a very rare occurence, we RESET it.
 */
void
fhpibdmadone(arg)
	void *arg;
{
	register int unit;
	register struct hpib_softc *hs;
	int s = splbio();

	unit = (int)arg;
	hs = &hpib_softc[unit];
	if (hs->sc_flags & HPIBF_IO) {
		register struct fhpibdevice *hd;
		register struct devqueue *dq;

		hd = (struct fhpibdevice *)hs->sc_hc->hp_addr;
		hd->hpib_imask = 0;
		hd->hpib_cid = 0xFF;
		DELAY(100);
		hd->hpib_cmd = CT_8BIT;
		hd->hpib_ar = AR_ARONC;
		fhpibifc(hd);
		hd->hpib_ie = IDS_IE;
		hs->sc_flags &= ~(HPIBF_DONE|HPIBF_IO|HPIBF_READ|HPIBF_TIMO);
		dmafree(&hs->sc_dq);
		dq = hs->sc_sq.dq_forw;
		(dq->dq_driver->d_intr)(dq->dq_unit);
	}
	(void) splx(s);
}

void
fhpibdone(unit)
	int unit;
{
	register struct hpib_softc *hs = &hpib_softc[unit];
	register struct fhpibdevice *hd;
	register char *addr;
	register int cnt;

	hd = (struct fhpibdevice *)hs->sc_hc->hp_addr;
	cnt = hs->sc_curcnt;
	hs->sc_addr += cnt;
	hs->sc_count -= cnt;
#ifdef DEBUG
	if ((fhpibdebug & FDB_DMA) && fhpibdebugunit == unit)
		printf("fhpibdone: addr %x cnt %d\n",
		       hs->sc_addr, hs->sc_count);
#endif
	if (hs->sc_flags & HPIBF_READ) {
		hd->hpib_imask = IM_IDLE | IM_BYTE;
		if (hs->sc_flags & HPIBF_TIMO)
			timeout(fhpibdmadone, (void *)unit, hz >> 2);
	} else {
		cnt = hs->sc_count;
		if (cnt) {
			addr = hs->sc_addr;
			hd->hpib_imask = IM_IDLE | IM_ROOM;
			FHPIBWAIT(hd, IM_IDLE);
			hd->hpib_stat = ST_WRITE;
			while (--cnt) {
				hd->hpib_data = *addr++;
				FHPIBWAIT(hd, IM_ROOM);
			}
			hd->hpib_stat = ST_EOI;
			hd->hpib_data = *addr;
		}
		hd->hpib_imask = IM_IDLE;
	}
	hs->sc_flags |= HPIBF_DONE;
	hd->hpib_stat = ST_IENAB;
	hd->hpib_ie = IDS_IE;
}

int
fhpibintr(unit)
	register int unit;
{
	register struct hpib_softc *hs = &hpib_softc[unit];
	register struct fhpibdevice *hd;
	register struct devqueue *dq;
	register int stat0;

	hd = (struct fhpibdevice *)hs->sc_hc->hp_addr;
	stat0 = hd->hpib_ids;
	if ((stat0 & (IDS_IE|IDS_IR)) != (IDS_IE|IDS_IR)) {
#ifdef DEBUG
		if ((fhpibdebug & FDB_FAIL) && (stat0 & IDS_IR) &&
		    (hs->sc_flags & (HPIBF_IO|HPIBF_DONE)) != HPIBF_IO)
			printf("%s: fhpibintr: bad status %x\n",
			hs->sc_hc->hp_xname, stat0);
		fhpibbadint[0]++;
#endif
		return(0);
	}
	if ((hs->sc_flags & (HPIBF_IO|HPIBF_DONE)) == HPIBF_IO) {
#ifdef DEBUG
		fhpibbadint[1]++;
#endif
		return(0);
	}
#ifdef DEBUG
	if ((fhpibdebug & FDB_DMA) && fhpibdebugunit == unit)
		printf("fhpibintr: flags %x\n", hs->sc_flags);
#endif
	dq = hs->sc_sq.dq_forw;
	if (hs->sc_flags & HPIBF_IO) {
		if (hs->sc_flags & HPIBF_TIMO)
			untimeout(fhpibdmadone, (void *)unit);
		stat0 = hd->hpib_cmd;
		hd->hpib_cmd = fhpibcmd[unit] & ~CT_8BIT;
		hd->hpib_stat = 0;
		hd->hpib_cmd = CT_REN | CT_8BIT;
		stat0 = hd->hpib_intr;
		hd->hpib_imask = 0;
		hs->sc_flags &= ~(HPIBF_DONE|HPIBF_IO|HPIBF_READ|HPIBF_TIMO);
		dmafree(&hs->sc_dq);
		(dq->dq_driver->d_intr)(dq->dq_unit);
	} else if (hs->sc_flags & HPIBF_PPOLL) {
		stat0 = hd->hpib_intr;
#ifdef DEBUG
		if ((fhpibdebug & FDB_FAIL) &&
		    doppollint && (stat0 & IM_PPRESP) == 0)
			printf("%s: fhpibintr: bad intr reg %x\n",
			    hs->sc_hc->hp_xname, stat0);
#endif
		hd->hpib_stat = 0;
		hd->hpib_imask = 0;
#ifdef DEBUG
		stat0 = fhpibppoll(unit);
		if ((fhpibdebug & FDB_PPOLL) && unit == fhpibdebugunit)
			printf("fhpibintr: got PPOLL status %x\n", stat0);
		if ((stat0 & (0x80 >> dq->dq_slave)) == 0) {
			/*
			 * XXX give it another shot (68040)
			 */
			fhpibppollfail[unit]++;
			DELAY(fhpibppolldelay);
			stat0 = fhpibppoll(unit);
			if ((stat0 & (0x80 >> dq->dq_slave)) == 0 &&
			    (fhpibdebug & FDB_PPOLL) && unit == fhpibdebugunit)
				printf("fhpibintr: PPOLL: unit %d slave %d stat %x\n",
				       unit, dq->dq_slave, stat0);
		}
#endif
		hs->sc_flags &= ~HPIBF_PPOLL;
		(dq->dq_driver->d_intr)(dq->dq_unit);
	}
	return(1);
}

int
fhpibppoll(unit)
	int unit;
{
	register struct fhpibdevice *hd;
	register int ppoll;

	hd = (struct fhpibdevice *)hpib_softc[unit].sc_hc->hp_addr;
	hd->hpib_stat = 0;
	hd->hpib_psense = 0;
	hd->hpib_pmask = 0xFF;
	hd->hpib_imask = IM_PPRESP | IM_PABORT;
	DELAY(25);
	hd->hpib_intr = IM_PABORT;
	ppoll = hd->hpib_data;
	if (hd->hpib_intr & IM_PABORT)
		ppoll = 0;
	hd->hpib_imask = 0;
	hd->hpib_pmask = 0;
	hd->hpib_stat = ST_IENAB;
	return(ppoll);
}

int
fhpibwait(hd, x)
	register struct fhpibdevice *hd;
	int x;
{
	register int timo = hpibtimeout;

	while ((hd->hpib_intr & x) == 0 && --timo)
		DELAY(1);
	if (timo == 0) {
#ifdef DEBUG
		if (fhpibdebug & FDB_FAIL)
			printf("fhpibwait(%x, %x) timeout\n", hd, x);
#endif
		return(-1);
	}
	return(0);
}

/*
 * XXX: this will have to change if we ever allow more than one
 * pending operation per HP-IB.
 */
void
fhpibppwatch(arg)
	void *arg;
{
	register int unit;
	register struct hpib_softc *hs;
	register struct fhpibdevice *hd;
	register int slave;

	unit = (int)arg;
	hs = &hpib_softc[unit];
	if ((hs->sc_flags & HPIBF_PPOLL) == 0)
		return;
	hd = (struct fhpibdevice *)hs->sc_hc->hp_addr;
	slave = (0x80 >> hs->sc_sq.dq_forw->dq_slave);
#ifdef DEBUG
	if (!doppollint) {
		if (fhpibppoll(unit) & slave) {
			hd->hpib_stat = ST_IENAB;
			hd->hpib_imask = IM_IDLE | IM_ROOM;
		} else
			timeout(fhpibppwatch, (void *)unit, 1);
		return;
	}
	if ((fhpibdebug & FDB_PPOLL) && unit == fhpibdebugunit)
		printf("fhpibppwatch: sense request on %d\n", unit);
#endif
	hd->hpib_psense = ~slave;
	hd->hpib_pmask = slave;
	hd->hpib_stat = ST_IENAB;
	hd->hpib_imask = IM_PPRESP | IM_PABORT;
	hd->hpib_ie = IDS_IE;
}
#endif /* NHPIB > 0 */
