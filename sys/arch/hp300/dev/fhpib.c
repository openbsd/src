/*	$OpenBSD: fhpib.c,v 1.15 2005/11/13 18:52:15 miod Exp $	*/
/*	$NetBSD: fhpib.c,v 1.18 1997/05/05 21:04:16 thorpej Exp $	*/

/*
 * Copyright (c) 1996, 1997 Jason R. Thorpe.  All rights reserved.
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
 *	@(#)fhpib.c	8.2 (Berkeley) 1/12/94
 */

/*
 * 98625A/B HPIB driver
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/timeout.h>

#include <machine/autoconf.h>
#include <machine/intr.h>

#include <hp300/dev/dioreg.h>
#include <hp300/dev/diovar.h>
#include <hp300/dev/diodevs.h>

#include <hp300/dev/dmavar.h>

#include <hp300/dev/fhpibreg.h>
#include <hp300/dev/hpibvar.h>

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
#endif

void	fhpibifc(struct fhpibdevice *);
void	fhpibdmadone(void *);
int	fhpibwait(struct fhpibdevice *, int);

void	fhpibreset(struct hpibbus_softc *);
int	fhpibsend(struct hpibbus_softc *, int, int, void *, int);
int	fhpibrecv(struct hpibbus_softc *, int, int, void *, int);
int	fhpibppoll(struct hpibbus_softc *);
void	fhpibppwatch(void *);
void	fhpibgo(struct hpibbus_softc *, int, int, void *, int, int, int);
void	fhpibdone(struct hpibbus_softc *);
int	fhpibintr(void *);

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

struct fhpib_softc {
	struct device sc_dev;		/* generic device glue */
	struct isr sc_isr;
	struct fhpibdevice *sc_regs;	/* device registers */
	struct timeout sc_dma_to;	/* DMA done timeout */
#ifdef DEBUG
	struct timeout sc_watch_to;	/* fhpibppwatch timeout */
#endif
	int	sc_cmd;
	struct hpibbus_softc *sc_hpibbus; /* XXX */
};

int	fhpibmatch(struct device *, void *, void *);
void	fhpibattach(struct device *, struct device *, void *);

struct cfattach fhpib_ca = {
	sizeof(struct fhpib_softc), fhpibmatch, fhpibattach
};

struct cfdriver fhpib_cd = {
	NULL, "fhpib", DV_DULL
};

int
fhpibmatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct dio_attach_args *da = aux;

	if (da->da_id == DIO_DEVICE_ID_FHPIB)
		return (1);

	return (0);
}

void
fhpibattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct fhpib_softc *sc = (struct fhpib_softc *)self;
	struct dio_attach_args *da = aux;
	struct hpibdev_attach_args ha;
	int ipl;

	sc->sc_regs = (struct fhpibdevice *)iomap(dio_scodetopa(da->da_scode),
	    da->da_size);
	if (sc->sc_regs == NULL) {
		printf("\n%s: can't map registers\n", self->dv_xname);
		return;
	}

	ipl = DIO_IPL(sc->sc_regs);
	printf(" ipl %d: %s\n", ipl, DIO_DEVICE_DESC_FHPIB);

	/* Initialize timeout structures */
	timeout_set(&sc->sc_dma_to, fhpibdmadone, sc);

	/* Establish the interrupt handler. */
	sc->sc_isr.isr_func = fhpibintr;
	sc->sc_isr.isr_arg = sc;
	sc->sc_isr.isr_ipl = ipl;
	sc->sc_isr.isr_priority = IPL_BIO;
	dio_intr_establish(&sc->sc_isr, self->dv_xname);

	ha.ha_ops = &fhpib_controller;
	ha.ha_type = HPIBC;			/* XXX */
	ha.ha_ba = HPIBC_BA;
	ha.ha_softcpp = &sc->sc_hpibbus;	/* XXX */
	(void)config_found(self, &ha, hpibdevprint);
}

void
fhpibreset(hs)
	struct hpibbus_softc *hs;
{
	struct fhpib_softc *sc = (struct fhpib_softc *)hs->sc_dev.dv_parent;
	struct fhpibdevice *hd = sc->sc_regs;

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
	 * If so, we should be able to write and read back the apropos bit.
	 */
	hd->hpib_ie |= IDS_WDMA;
	if (hd->hpib_ie & IDS_WDMA) {
		hd->hpib_ie &= ~IDS_WDMA;
		hs->sc_flags |= HPIBF_DMA16;
#ifdef DEBUG
		if (fhpibdebug & FDB_DMA)
			printf("fhpibtype: %s has word dma\n",
			    sc->sc_dev.dv_xname);

#endif
	}
}

void
fhpibifc(hd)
	struct fhpibdevice *hd;
{
	hd->hpib_cmd |= CT_IFC;
	hd->hpib_cmd |= CT_INITFIFO;
	DELAY(100);
	hd->hpib_cmd &= ~CT_IFC;
	hd->hpib_cmd |= CT_REN;
	hd->hpib_stat = ST_ATN;
}

int
fhpibsend(hs, slave, sec, ptr, origcnt)
	struct hpibbus_softc *hs;
	int slave, sec, origcnt;
	void *ptr;
{
	struct fhpib_softc *sc = (struct fhpib_softc *)hs->sc_dev.dv_parent;
	struct fhpibdevice *hd = sc->sc_regs;
	int cnt = origcnt;
	int timo;
	char *addr = ptr;

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
		    sc->sc_dev.dv_xname, slave, sec);
		printf("sent %d of %d bytes\n", origcnt-cnt-1, origcnt);
	}
#endif
	return (origcnt - cnt - 1);
}

int
fhpibrecv(hs, slave, sec, ptr, origcnt)
	struct hpibbus_softc *hs;
	int slave, sec, origcnt;
	void *ptr;
{
	struct fhpib_softc *sc = (struct fhpib_softc *)hs->sc_dev.dv_parent;
	struct fhpibdevice *hd = sc->sc_regs;
	int cnt = origcnt;
	int timo;
	char *addr = ptr;

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
		    sc->sc_dev.dv_xname, slave, sec);
		printf("got %d of %d bytes\n", origcnt-cnt-1, origcnt);
	}
#endif
	return (origcnt - cnt - 1);
}

void
fhpibgo(hs, slave, sec, ptr, count, rw, timo)
	struct hpibbus_softc *hs;
	int slave, sec, count, rw, timo;
	void *ptr;
{
	struct fhpib_softc *sc = (struct fhpib_softc *)hs->sc_dev.dv_parent;
	struct fhpibdevice *hd = sc->sc_regs;
	int i;
	char *addr = ptr;
	int flags = 0;

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
	/* fhpibtransfer[unit]++;			XXX */
#endif
	if ((hs->sc_flags & HPIBF_DMA16) &&
	    ((int)addr & 1) == 0 && count && (count & 1) == 0
#ifdef DEBUG
	    && doworddma
#endif
	    ) {
#ifdef DEBUG
		/* fhpibworddma[unit]++;		XXX */
#endif
		flags |= DMAGO_WORD;
		hd->hpib_latch = 0;
	}
#ifdef DEBUG
	if (dopriodma)
		flags |= DMAGO_PRI;
#endif
	if (hs->sc_flags & HPIBF_READ) {
		sc->sc_cmd = CT_REN | CT_8BIT;
		hs->sc_curcnt = count;
		dmago(hs->sc_dq->dq_chan, addr, count, flags|DMAGO_READ);
		if (fhpibrecv(hs, slave, sec, 0, 0) < 0) {
#ifdef DEBUG
			printf("fhpibgo: recv failed, retrying...\n");
#endif
			(void) fhpibrecv(hs, slave, sec, 0, 0);
		}
		i = hd->hpib_cmd;
		hd->hpib_cmd = sc->sc_cmd;
		hd->hpib_ie = IDS_DMA(hs->sc_dq->dq_chan) |
			((flags & DMAGO_WORD) ? IDS_WDMA : 0);
		return;
	}
	sc->sc_cmd = CT_REN | CT_8BIT | CT_FIFOSEL;
	if (count < hpibdmathresh) {
#ifdef DEBUG
		/* fhpibnondma[unit]++;			XXX */
		if (flags & DMAGO_WORD)
			/* fhpibworddma[unit]--;	XXX */ ;
#endif
		hs->sc_curcnt = count;
		(void) fhpibsend(hs, slave, sec, addr, count);
		fhpibdone(hs);
		return;
	}
	count -= (flags & DMAGO_WORD) ? 2 : 1;
	hs->sc_curcnt = count;
	dmago(hs->sc_dq->dq_chan, addr, count, flags);
	if (fhpibsend(hs, slave, sec, 0, 0) < 0) {
#ifdef DEBUG
		printf("fhpibgo: send failed, retrying...\n");
#endif
		(void) fhpibsend(hs, slave, sec, 0, 0);
	}
	i = hd->hpib_cmd;
	hd->hpib_cmd = sc->sc_cmd;
	hd->hpib_ie = IDS_DMA(hs->sc_dq->dq_chan) | IDS_WRITE |
		((flags & DMAGO_WORD) ? IDS_WDMA : 0);
}

/*
 * A DMA read can finish but the device can still be waiting (MAG-tape
 * with more data than we're waiting for).  This timeout routine
 * takes care of that.  Somehow, the thing gets hosed.  For now, since
 * this should be a very rare occurrence, we RESET it.
 */
void
fhpibdmadone(arg)
	void *arg;
{
	struct fhpib_softc *sc = arg;
	struct hpibbus_softc *hs = sc->sc_hpibbus;
	int s;

	s  = splbio();
	if (hs->sc_flags & HPIBF_IO) {
		struct fhpibdevice *hd = sc->sc_regs;
		struct hpibqueue *hq;

		hd->hpib_imask = 0;
		hd->hpib_cid = 0xFF;
		DELAY(100);
		hd->hpib_cmd = CT_8BIT;
		hd->hpib_ar = AR_ARONC;
		fhpibifc(hd);
		hd->hpib_ie = IDS_IE;
		hs->sc_flags &= ~(HPIBF_DONE|HPIBF_IO|HPIBF_READ|HPIBF_TIMO);
		dmafree(hs->sc_dq);

		hq = TAILQ_FIRST(&hs->sc_queue);
		(hq->hq_intr)(hq->hq_softc);
	}
	splx(s);
}

void
fhpibdone(hs)
	struct hpibbus_softc *hs;
{
	struct fhpib_softc *sc = (struct fhpib_softc *)hs->sc_dev.dv_parent;
	struct fhpibdevice *hd = sc->sc_regs;
	char *addr;
	int cnt;

	cnt = hs->sc_curcnt;
	hs->sc_addr += cnt;
	hs->sc_count -= cnt;
#ifdef DEBUG
	if ((fhpibdebug & FDB_DMA) && fhpibdebugunit == sc->sc_dev.dv_unit)
		printf("fhpibdone: addr %p cnt %d\n",
		       hs->sc_addr, hs->sc_count);
#endif
	if (hs->sc_flags & HPIBF_READ) {
		hd->hpib_imask = IM_IDLE | IM_BYTE;
		if (hs->sc_flags & HPIBF_TIMO)
			timeout_add(&sc->sc_dma_to, hz >> 2);
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
fhpibintr(arg)
	void *arg;
{
	struct fhpib_softc *sc = arg;
	struct hpibbus_softc *hs = sc->sc_hpibbus;
	struct fhpibdevice *hd = sc->sc_regs;
	struct hpibqueue *hq;
	int stat0;

	stat0 = hd->hpib_ids;
	if ((stat0 & (IDS_IE|IDS_IR)) != (IDS_IE|IDS_IR)) {
#ifdef DEBUG
		if ((fhpibdebug & FDB_FAIL) && (stat0 & IDS_IR) &&
		    (hs->sc_flags & (HPIBF_IO|HPIBF_DONE)) != HPIBF_IO)
			printf("%s: fhpibintr: bad status %x\n",
			sc->sc_dev.dv_xname, stat0);
		/* fhpibbadint[0]++;			XXX */
#endif
		return(0);
	}
	if ((hs->sc_flags & (HPIBF_IO|HPIBF_DONE)) == HPIBF_IO) {
#ifdef DEBUG
		/* fhpibbadint[1]++;			XXX */
#endif
		return(0);
	}

#ifdef DEBUG
	if ((fhpibdebug & FDB_DMA) && fhpibdebugunit == sc->sc_dev.dv_unit)
		printf("fhpibintr: flags %x\n", hs->sc_flags);
#endif
	hq = TAILQ_FIRST(&hs->sc_queue);
	if (hs->sc_flags & HPIBF_IO) {
		if (hs->sc_flags & HPIBF_TIMO)
			timeout_del(&sc->sc_dma_to);
		stat0 = hd->hpib_cmd;
		hd->hpib_cmd = sc->sc_cmd & ~CT_8BIT;
		hd->hpib_stat = 0;
		hd->hpib_cmd = CT_REN | CT_8BIT;
		stat0 = hd->hpib_intr;
		hd->hpib_imask = 0;
		hs->sc_flags &= ~(HPIBF_DONE|HPIBF_IO|HPIBF_READ|HPIBF_TIMO);
		dmafree(hs->sc_dq);
		(hq->hq_intr)(hq->hq_softc);
	} else if (hs->sc_flags & HPIBF_PPOLL) {
		stat0 = hd->hpib_intr;
#ifdef DEBUG
		if ((fhpibdebug & FDB_FAIL) &&
		    doppollint && (stat0 & IM_PPRESP) == 0)
			printf("%s: fhpibintr: bad intr reg %x\n",
			    sc->sc_dev.dv_xname, stat0);
#endif
		hd->hpib_stat = 0;
		hd->hpib_imask = 0;
#ifdef DEBUG
		stat0 = fhpibppoll(hs);
		if ((fhpibdebug & FDB_PPOLL) &&
		    fhpibdebugunit == sc->sc_dev.dv_unit)
			printf("fhpibintr: got PPOLL status %x\n", stat0);
		if ((stat0 & (0x80 >> hq->hq_slave)) == 0) {
			/*
			 * XXX give it another shot (68040)
			 */
			/* fhpibppollfail[unit]++;	XXX */
			DELAY(fhpibppolldelay);
			stat0 = fhpibppoll(hs);
			if ((stat0 & (0x80 >> hq->hq_slave)) == 0 &&
			    (fhpibdebug & FDB_PPOLL) &&
			    fhpibdebugunit == sc->sc_dev.dv_unit)
				printf("fhpibintr: PPOLL: unit %d slave %d stat %x\n",
				       sc->sc_dev.dv_unit, hq->hq_slave, stat0);
		}
#endif
		hs->sc_flags &= ~HPIBF_PPOLL;
		(hq->hq_intr)(hq->hq_softc);
	}
	return(1);
}

int
fhpibppoll(hs)
	struct hpibbus_softc *hs;
{
	struct fhpib_softc *sc = (struct fhpib_softc *)hs->sc_dev.dv_parent;
	struct fhpibdevice *hd = sc->sc_regs;
	int ppoll;

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
	struct fhpibdevice *hd;
	int x;
{
	int timo = hpibtimeout;

	while ((hd->hpib_intr & x) == 0 && --timo)
		DELAY(1);
	if (timo == 0) {
#ifdef DEBUG
		if (fhpibdebug & FDB_FAIL)
			printf("fhpibwait(%p, %x) timeout\n", hd, x);
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
	struct hpibbus_softc *hs = arg;
	struct fhpib_softc *sc = (struct fhpib_softc *)hs->sc_dev.dv_parent;
	struct fhpibdevice *hd = sc->sc_regs;
	int slave;

	if ((hs->sc_flags & HPIBF_PPOLL) == 0)
		return;
	slave = (0x80 >> TAILQ_FIRST(&hs->sc_queue)->hq_slave);
#ifdef DEBUG
	if (!doppollint) {
		if (fhpibppoll(hs) & slave) {
			hd->hpib_stat = ST_IENAB;
			hd->hpib_imask = IM_IDLE | IM_ROOM;
		} else {
			timeout_set(&sc->sc_watch_to, fhpibppwatch, hs);
			timeout_add(&sc->sc_watch_to, 1);
		}
		return;
	}
	if ((fhpibdebug & FDB_PPOLL) && sc->sc_dev.dv_unit == fhpibdebugunit)
		printf("fhpibppwatch: sense request on %s\n",
		    sc->sc_dev.dv_xname);
#endif
	hd->hpib_psense = ~slave;
	hd->hpib_pmask = slave;
	hd->hpib_stat = ST_IENAB;
	hd->hpib_imask = IM_PPRESP | IM_PABORT;
	hd->hpib_ie = IDS_IE;
}
