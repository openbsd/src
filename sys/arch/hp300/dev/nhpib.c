/*	$OpenBSD: nhpib.c,v 1.15 2005/01/15 21:13:08 miod Exp $	*/
/*	$NetBSD: nhpib.c,v 1.17 1997/05/05 21:06:41 thorpej Exp $	*/

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
 *	@(#)nhpib.c	8.2 (Berkeley) 1/12/94
 */

/*
 * Internal/98624 HPIB driver
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

#include <hp300/dev/nhpibreg.h>
#include <hp300/dev/hpibvar.h>

/*
 * ODD parity table for listen and talk addresses and secondary commands.
 * The TI9914A doesn't produce the parity bit.
 */
static u_char listnr_par[] = {
	0040,0241,0242,0043,0244,0045,0046,0247,
	0250,0051,0052,0253,0054,0255,0256,0057,
	0260,0061,0062,0263,0064,0265,0266,0067,
	0070,0271,0272,0073,0274,0075,0076,0277,
};
static u_char talker_par[] = {
	0100,0301,0302,0103,0304,0105,0106,0307,
	0310,0111,0112,0313,0114,0315,0316,0117,
	0320,0121,0122,0323,0124,0325,0326,0127,
	0130,0331,0332,0133,0334,0135,0136,0337,
};
static u_char sec_par[] = {
	0340,0141,0142,0343,0144,0345,0346,0147,
	0150,0351,0352,0153,0354,0155,0156,0357,
	0160,0361,0362,0163,0364,0165,0166,0367,
	0370,0171,0172,0373,0174,0375,0376,0177
};

void	nhpibifc(struct nhpibdevice *);
void	nhpibreadtimo(void *);
int	nhpibwait(struct nhpibdevice *, int);

void	nhpibreset(struct hpibbus_softc *);
int	nhpibsend(struct hpibbus_softc *, int, int, void *, int);
int	nhpibrecv(struct hpibbus_softc *, int, int, void *, int);
int	nhpibppoll(struct hpibbus_softc *);
void	nhpibppwatch(void *);
void	nhpibgo(struct hpibbus_softc *, int, int, void *, int, int, int);
void	nhpibdone(struct hpibbus_softc *);
int	nhpibintr(void *);

/*
 * Our controller ops structure.
 */
struct	hpib_controller nhpib_controller = {
	nhpibreset,
	nhpibsend,
	nhpibrecv,
	nhpibppoll,
	nhpibppwatch,
	nhpibgo,
	nhpibdone,
	nhpibintr
};

struct nhpib_softc {
	struct device sc_dev;		/* generic device glue */
	struct isr sc_isr;
	struct nhpibdevice *sc_regs;	/* device registers */
	struct hpibbus_softc *sc_hpibbus; /* XXX */
	struct timeout sc_read_to;	/* nhpibreadtimo timeout */
	struct timeout sc_watch_to;	/* nhpibppwatch timeout */
};

int	nhpibmatch(struct device *, void *, void *);
void	nhpibattach(struct device *, struct device *, void *);

struct cfattach nhpib_ca = {
	sizeof(struct nhpib_softc), nhpibmatch, nhpibattach
};

struct cfdriver nhpib_cd = {
	NULL, "nhpib", DV_DULL
};

int
nhpibmatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct dio_attach_args *da = aux;

	/*
	 * Internal HP-IB doesn't always return a device ID,
	 * so we rely on the sysflags.
	 */
	if (da->da_scode == 7 && internalhpib)
		return (1);

	if (da->da_id == DIO_DEVICE_ID_NHPIB)
		return (1);

	return (0);
}

void
nhpibattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct nhpib_softc *sc = (struct nhpib_softc *)self;
	struct dio_attach_args *da = aux;
	struct hpibdev_attach_args ha;
	const char *desc;
	int ipl, type = HPIBA;

	sc->sc_regs = (struct nhpibdevice *)iomap(dio_scodetopa(da->da_scode),
	    da->da_size);
	if (sc->sc_regs == NULL) {
		printf("\n%s: can't map registers\n", self->dv_xname);
		return;
	}

	ipl = DIO_IPL(sc->sc_regs);

	if (da->da_scode == 7 && internalhpib)
		desc = DIO_DEVICE_DESC_IHPIB;
	else if (da->da_id == DIO_DEVICE_ID_NHPIB) {
		type = HPIBB;
		desc = DIO_DEVICE_DESC_NHPIB;
	} else
		desc = "unknown HP-IB!";

	printf(" ipl %d: %s\n", ipl, desc);

	/* Establish the interrupt handler. */
	sc->sc_isr.isr_func = nhpibintr;
	sc->sc_isr.isr_arg = sc;
	sc->sc_isr.isr_ipl = ipl;
	sc->sc_isr.isr_priority = IPL_BIO;
	dio_intr_establish(&sc->sc_isr, self->dv_xname);

	/* Initialize timeout structures */
	timeout_set(&sc->sc_read_to, nhpibreadtimo, &sc->sc_hpibbus);
	timeout_set(&sc->sc_watch_to, nhpibppwatch, &sc->sc_hpibbus);

	ha.ha_ops = &nhpib_controller;
	ha.ha_type = type;			/* XXX */
	ha.ha_ba = (type == HPIBA) ? HPIBA_BA :
	    (sc->sc_regs->hpib_csa & CSA_BA);
	ha.ha_softcpp = &sc->sc_hpibbus;	/* XXX */
	(void)config_found(self, &ha, hpibdevprint);
}

void
nhpibreset(hs)
	struct hpibbus_softc *hs;
{
	struct nhpib_softc *sc = (struct nhpib_softc *)hs->sc_dev.dv_parent;
	struct nhpibdevice *hd = sc->sc_regs;

	hd->hpib_acr = AUX_SSWRST;
	hd->hpib_ar = hs->sc_ba;
	hd->hpib_lim = LIS_ERR;
	hd->hpib_mim = 0;
	hd->hpib_acr = AUX_CDAI;
	hd->hpib_acr = AUX_CSHDW;
	hd->hpib_acr = AUX_SSTD1;
	hd->hpib_acr = AUX_SVSTD1;
	hd->hpib_acr = AUX_CPP;
	hd->hpib_acr = AUX_CHDFA;
	hd->hpib_acr = AUX_CHDFE;
	hd->hpib_acr = AUX_RHDF;
	hd->hpib_acr = AUX_CSWRST;
	nhpibifc(hd);
	hd->hpib_ie = IDS_IE;
	hd->hpib_data = C_DCL_P;
	DELAY(100000);
}

void
nhpibifc(hd)
	struct nhpibdevice *hd;
{
	hd->hpib_acr = AUX_TCA;
	hd->hpib_acr = AUX_CSRE;
	hd->hpib_acr = AUX_SSIC;
	DELAY(100);
	hd->hpib_acr = AUX_CSIC;
	hd->hpib_acr = AUX_SSRE;
}

int
nhpibsend(hs, slave, sec, ptr, origcnt)
	struct hpibbus_softc *hs;
	int slave, sec, origcnt;
	void *ptr;
{
	struct nhpib_softc *sc = (struct nhpib_softc *)hs->sc_dev.dv_parent;
	struct nhpibdevice *hd = sc->sc_regs;
	int cnt = origcnt;
	char *addr = ptr;

	hd->hpib_acr = AUX_TCA;
	hd->hpib_data = C_UNL_P;
	if (nhpibwait(hd, MIS_BO))
		goto senderror;
	hd->hpib_data = talker_par[hs->sc_ba];
	hd->hpib_acr = AUX_STON;
	if (nhpibwait(hd, MIS_BO))
		goto senderror;
	hd->hpib_data = listnr_par[slave];
	if (nhpibwait(hd, MIS_BO))
		goto senderror;
	if (sec >= 0 || sec == -2) {
		if (sec == -2)		/* selected device clear KLUDGE */
			hd->hpib_data = C_SDC_P;
		else
			hd->hpib_data = sec_par[sec];
		if (nhpibwait(hd, MIS_BO))
			goto senderror;
	}
	hd->hpib_acr = AUX_GTS;
	if (cnt) {
		while (--cnt > 0) {
			hd->hpib_data = *addr++;
			if (nhpibwait(hd, MIS_BO))
				goto senderror;
		}
		hd->hpib_acr = AUX_EOI;
		hd->hpib_data = *addr;
		if (nhpibwait(hd, MIS_BO))
			goto senderror;
		hd->hpib_acr = AUX_TCA;
#if 0
		/*
		 * May be causing 345 disks to hang due to interference
		 * with PPOLL mechanism.
		 */
		hd->hpib_data = C_UNL_P;
		(void) nhpibwait(hd, MIS_BO);
#endif
	}
	return(origcnt);

senderror:
	nhpibifc(hd);
	return(origcnt - cnt - 1);
}

int
nhpibrecv(hs, slave, sec, ptr, origcnt)
	struct hpibbus_softc *hs;
	int slave, sec, origcnt;
	void *ptr;
{
	struct nhpib_softc *sc = (struct nhpib_softc *)hs->sc_dev.dv_parent;
	struct nhpibdevice *hd = sc->sc_regs;
	int cnt = origcnt;
	char *addr = ptr;

	/*
	 * Slave < 0 implies continuation of a previous receive
	 * that probably timed out.
	 */
	if (slave >= 0) {
		hd->hpib_acr = AUX_TCA;
		hd->hpib_data = C_UNL_P;
		if (nhpibwait(hd, MIS_BO))
			goto recverror;
		hd->hpib_data = listnr_par[hs->sc_ba];
		hd->hpib_acr = AUX_SLON;
		if (nhpibwait(hd, MIS_BO))
			goto recverror;
		hd->hpib_data = talker_par[slave];
		if (nhpibwait(hd, MIS_BO))
			goto recverror;
		if (sec >= 0) {
			hd->hpib_data = sec_par[sec];
			if (nhpibwait(hd, MIS_BO))
				goto recverror;
		}
		hd->hpib_acr = AUX_RHDF;
		hd->hpib_acr = AUX_GTS;
	}
	if (cnt) {
		while (--cnt >= 0) {
			if (nhpibwait(hd, MIS_BI))
				goto recvbyteserror;
			*addr++ = hd->hpib_data;
		}
		hd->hpib_acr = AUX_TCA;
		hd->hpib_data = (slave == 31) ? C_UNA_P : C_UNT_P;
		(void) nhpibwait(hd, MIS_BO);
	}
	return(origcnt);

recverror:
	nhpibifc(hd);
recvbyteserror:
	return(origcnt - cnt - 1);
}

void
nhpibgo(hs, slave, sec, ptr, count, rw, timo)
	struct hpibbus_softc *hs;
	int slave, sec, count, rw, timo;
	void *ptr;
{
	struct nhpib_softc *sc = (struct nhpib_softc *)hs->sc_dev.dv_parent;
	struct nhpibdevice *hd = sc->sc_regs;
	char *addr = ptr;

	hs->sc_flags |= HPIBF_IO;
	if (timo)
		hs->sc_flags |= HPIBF_TIMO;
	if (rw == B_READ)
		hs->sc_flags |= HPIBF_READ;
#ifdef DEBUG
	else if (hs->sc_flags & HPIBF_READ) {
		printf("nhpibgo: HPIBF_READ still set\n");
		hs->sc_flags &= ~HPIBF_READ;
	}
#endif
	hs->sc_count = count;
	hs->sc_addr = addr;
	if (hs->sc_flags & HPIBF_READ) {
		hs->sc_curcnt = count;
		dmago(hs->sc_dq->dq_chan, addr, count, DMAGO_BYTE|DMAGO_READ);
		nhpibrecv(hs, slave, sec, 0, 0);
		hd->hpib_mim = MIS_END;
	} else {
		hd->hpib_mim = 0;
		if (count < hpibdmathresh) {
			hs->sc_curcnt = count;
			nhpibsend(hs, slave, sec, addr, count);
			nhpibdone(hs);
			return;
		}
		hs->sc_curcnt = --count;
		dmago(hs->sc_dq->dq_chan, addr, count, DMAGO_BYTE);
		nhpibsend(hs, slave, sec, 0, 0);
	}
	hd->hpib_ie = IDS_IE | IDS_DMA(hs->sc_dq->dq_chan);
}

/*
 * This timeout can only happen if a DMA read finishes DMAing with the read
 * still pending (more data in read transaction than the driver was prepared
 * to accept).  At the moment, variable-record tape drives are the only things
 * capabale of doing this.  We repeat the necessary code from nhpibintr() -
 * easier and quicker than calling nhpibintr() for this special case.
 */
void
nhpibreadtimo(arg)
	void *arg;
{
	struct hpibbus_softc *hs = arg;
	struct nhpib_softc *sc = (struct nhpib_softc *)hs->sc_dev.dv_parent;
	int s = splbio();

	if (hs->sc_flags & HPIBF_IO) {
		struct nhpibdevice *hd = sc->sc_regs;
		struct hpibqueue *hq;

		hd->hpib_mim = 0;
		hd->hpib_acr = AUX_TCA;
		hs->sc_flags &= ~(HPIBF_DONE|HPIBF_IO|HPIBF_READ|HPIBF_TIMO);
		dmafree(hs->sc_dq);

		hq = TAILQ_FIRST(&hs->sc_queue);
		(hq->hq_intr)(hq->hq_softc);
	}
	splx(s);
}

void
nhpibdone(hs)
	struct hpibbus_softc *hs;
{
	struct nhpib_softc *sc = (struct nhpib_softc *)hs->sc_dev.dv_parent;
	struct nhpibdevice *hd = sc->sc_regs;
	int cnt;

	cnt = hs->sc_curcnt;
	hs->sc_addr += cnt;
	hs->sc_count -= cnt;
	hs->sc_flags |= HPIBF_DONE;
	hd->hpib_ie = IDS_IE;
	if (hs->sc_flags & HPIBF_READ) {
		if ((hs->sc_flags & HPIBF_TIMO) &&
		    (hd->hpib_ids & IDS_IR) == 0)
			timeout_add(&sc->sc_read_to, hz >> 2);
	} else {
		if (hs->sc_count == 1) {
			(void) nhpibwait(hd, MIS_BO);
			hd->hpib_acr = AUX_EOI;
			hd->hpib_data = *hs->sc_addr;
			hd->hpib_mim = MIS_BO;
		}
#ifdef DEBUG
		else if (hs->sc_count)
			panic("nhpibdone");
#endif
	}
}

int
nhpibintr(arg)
	void *arg;
{
	struct nhpib_softc *sc = arg;
	struct hpibbus_softc *hs = sc->sc_hpibbus;
	struct nhpibdevice *hd = sc->sc_regs;
	struct hpibqueue *hq;
	int stat0;
	int stat1;

#ifdef lint
	if (stat1 = unit) return(1);
#endif
	if ((hd->hpib_ids & IDS_IR) == 0)
		return(0);
	stat0 = hd->hpib_mis;
	stat1 = hd->hpib_lis;

	hq = TAILQ_FIRST(&hs->sc_queue);

	if (hs->sc_flags & HPIBF_IO) {
		hd->hpib_mim = 0;
		if ((hs->sc_flags & HPIBF_DONE) == 0) {
			hs->sc_flags &= ~HPIBF_TIMO;
			dmastop(hs->sc_dq->dq_chan);
		} else if (hs->sc_flags & HPIBF_TIMO)
			timeout_del(&sc->sc_read_to);
		hd->hpib_acr = AUX_TCA;
		hs->sc_flags &= ~(HPIBF_DONE|HPIBF_IO|HPIBF_READ|HPIBF_TIMO);

		dmafree(hs->sc_dq);
		(hq->hq_intr)(hq->hq_softc);
	} else if (hs->sc_flags & HPIBF_PPOLL) {
		hd->hpib_mim = 0;
		stat0 = nhpibppoll(hs);
		if (stat0 & (0x80 >> hq->hq_slave)) {
			hs->sc_flags &= ~HPIBF_PPOLL;
			(hq->hq_intr)(hq->hq_softc);
		}
#ifdef DEBUG
		else
			printf("%s: PPOLL intr bad status %x\n",
			       hs->sc_dev.dv_xname, stat0);
#endif
	}
	return(1);
}

int
nhpibppoll(hs)
	struct hpibbus_softc *hs;
{
	struct nhpib_softc *sc = (struct nhpib_softc *)hs->sc_dev.dv_parent;
	struct nhpibdevice *hd = sc->sc_regs;
	int ppoll;

	hd->hpib_acr = AUX_SPP;
	DELAY(25);
	ppoll = hd->hpib_cpt;
	hd->hpib_acr = AUX_CPP;
	return(ppoll);
}

#ifdef DEBUG
int nhpibreporttimo = 0;
#endif

int
nhpibwait(hd, x)
	struct nhpibdevice *hd;
	int x;
{
	int timo = hpibtimeout;

	while ((hd->hpib_mis & x) == 0 && --timo)
		DELAY(1);
	if (timo == 0) {
#ifdef DEBUG
		if (nhpibreporttimo)
			printf("hpib0: %s timo\n", x==MIS_BO?"OUT":"IN");
#endif
		return(-1);
	}
	return(0);
}

void
nhpibppwatch(arg)
	void *arg;
{
	struct hpibbus_softc *hs = arg;
	struct nhpib_softc *sc = (struct nhpib_softc *)hs->sc_dev.dv_parent;

	if ((hs->sc_flags & HPIBF_PPOLL) == 0)
		return;
again:
	if (nhpibppoll(hs) & (0x80 >> TAILQ_FIRST(&hs->sc_queue)->hq_slave))
       		sc->sc_regs->hpib_mim = MIS_BO;
	else if (cold)
		/* timeouts not working yet */
		goto again;
	else
		timeout_add(&sc->sc_watch_to, 1);
}
