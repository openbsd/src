/* $OpenBSD: spc.c,v 1.6 2004/08/25 20:57:38 miod Exp $ */
/* $NetBSD: spc.c,v 1.2 2003/11/17 14:37:59 tsutsui Exp $ */

/*
 * Copyright (c) 2003 Izumi Tsutsui.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/buf.h>

#include <machine/autoconf.h>
#include <machine/cpu.h>
#include <machine/intr.h>

#include <hp300/dev/dioreg.h>
#include <hp300/dev/diovar.h>
#include <hp300/dev/diodevs.h>

#include <scsi/scsi_all.h>
#include <scsi/scsi_message.h>
#include <scsi/scsiconf.h>

#include <hp300/dev/mb89352reg.h>
#include <hp300/dev/mb89352var.h>

#include <hp300/dev/hp98265reg.h>
#include <hp300/dev/dmareg.h>
#include <hp300/dev/dmavar.h>

int  spc_dio_match(struct device *, void *, void *);
void spc_dio_attach(struct device *, struct device *, void *);
void spc_dio_dmastart(struct spc_softc *, void *, size_t, int);
void spc_dio_dmadone(struct spc_softc *);
void spc_dio_dmago(void *);
void spc_dio_dmastop(void *);
int  spc_dio_intr(void *);
void spc_dio_reset(struct spc_softc *);

#define	HPSPC_ADDRESS(o)	(dsc->sc_dregs + ((o) << 1) + 1)
#define	hpspc_read(o)		*(volatile u_int8_t *)(HPSPC_ADDRESS(o))
#define	hpspc_write(o, v)	*(volatile u_int8_t *)(HPSPC_ADDRESS(o)) = (v)

struct spc_dio_softc {
	struct spc_softc sc_spc;	/* MI spc softc */
	volatile u_int8_t *sc_dregs;	/* Complete registers */

	struct dmaqueue sc_dq;		/* DMA job queue */
	u_int sc_dflags;		/* DMA flags */
#define SCSI_DMA32	0x01		/* 32-bit DMA should be used */
#define SCSI_HAVEDMA	0x02		/* controller has DMA channel */
#define SCSI_DATAIN	0x04		/* DMA direction */
};

struct cfattach spc_ca = {
	sizeof(struct spc_dio_softc), spc_dio_match, spc_dio_attach
};

struct cfdriver spc_cd = {
	NULL, "spc", DV_DULL
};

int
spc_dio_match(struct device *parent, void *vcf, void *aux)
{
	struct dio_attach_args *da = aux;

	switch (da->da_id) {
	case DIO_DEVICE_ID_SCSI0:
	case DIO_DEVICE_ID_SCSI1:
	case DIO_DEVICE_ID_SCSI2:
	case DIO_DEVICE_ID_SCSI3:
		return 1;
	}

	return 0;
}

void
spc_dio_attach(struct device *parent, struct device *self, void *aux)
{
	struct spc_dio_softc *dsc = (struct spc_dio_softc *)self;
	struct spc_softc *sc = &dsc->sc_spc;
	struct dio_attach_args *da = aux;
	int ipl;
	u_int8_t id, hconf;

	dsc->sc_dregs = (u_int8_t *)iomap(dio_scodetopa(da->da_scode),
	    da->da_size);
	if (dsc->sc_dregs == NULL) {
		printf(": can't map SCSI registers\n");
		return;
	}
	sc->sc_regs = dsc->sc_dregs + SPC_OFFSET;

	ipl = DIO_IPL(sc->sc_regs);
	printf(" ipl %d: 98265A SCSI", ipl);

	hpspc_write(HPSCSI_ID, 0xff);
	DELAY(100);
	id = hpspc_read(HPSCSI_ID);
	hconf = hpspc_read(HPSCSI_HCONF);

	if ((id & ID_WORD_DMA) == 0) {
		printf(", 32-bit DMA");
		dsc->sc_dflags |= SCSI_DMA32;
	}
	if ((hconf & HCONF_PARITY) == 0)
		printf(", no parity");

	id &= ID_MASK;
	printf(", SCSI ID %d\n", id);

	if ((hconf & HCONF_PARITY) != 0)
		sc->sc_ctlflags = SCTL_PARITY_ENAB;

	sc->sc_initiator = id;

	sc->sc_dma_start = spc_dio_dmastart;
	sc->sc_dma_done  = spc_dio_dmadone;
	sc->sc_reset = spc_dio_reset;

	dsc->sc_dq.dq_softc = dsc;
	dsc->sc_dq.dq_start = spc_dio_dmago;
	dsc->sc_dq.dq_done  = spc_dio_dmastop;

	hpspc_write(HPSCSI_CSR, 0x00);
	hpspc_write(HPSCSI_HCONF, 0x00);

	dio_intr_establish(spc_dio_intr, (void *)dsc, ipl, IPL_BIO);

	spc_attach(sc);

	/* Enable SPC interrupts. */
	hpspc_write(HPSCSI_CSR, CSR_IE);
}

void
spc_dio_dmastart(struct spc_softc *sc, void *addr, size_t size, int datain)
{
	struct spc_dio_softc *dsc = (struct spc_dio_softc *)sc;

	dsc->sc_dq.dq_chan = DMA0 | DMA1;
	dsc->sc_dflags |= SCSI_HAVEDMA;
	if (datain)
		dsc->sc_dflags |= SCSI_DATAIN;
	else
		dsc->sc_dflags &= ~SCSI_DATAIN;

	if (dmareq(&dsc->sc_dq) != 0)
		/* DMA channel is available, so start DMA immediately */
		spc_dio_dmago((void *)dsc);
	/* else dma start function will be called later from dmafree(). */
}

void
spc_dio_dmago(void *arg)
{
	struct spc_dio_softc *dsc = (struct spc_dio_softc *)arg;
	struct spc_softc *sc = &dsc->sc_spc;
	int len, chan;
	u_int32_t dmaflags;
	u_int8_t cmd;

	hpspc_write(HPSCSI_HCONF, 0);
	cmd = CSR_IE;
	dmaflags = DMAGO_NOINT;
	chan = dsc->sc_dq.dq_chan;
	if ((dsc->sc_dflags & SCSI_DATAIN) != 0) {
		cmd |= CSR_DMAIN;
		dmaflags |= DMAGO_READ;
	}
	if ((dsc->sc_dflags & SCSI_DMA32) != 0 &&
	    ((u_int)sc->sc_dp & 3) == 0 &&
	    (sc->sc_dleft & 3) == 0) {
		cmd |= CSR_DMA32;
		dmaflags |= DMAGO_LWORD;
	} else
		dmaflags |= DMAGO_WORD;

	sc->sc_flags |= SPC_DOINGDMA;
	dmago(chan, sc->sc_dp, sc->sc_dleft, dmaflags);

	hpspc_write(HPSCSI_CSR, cmd);
	cmd |= (chan == 0) ? CSR_DE0 : CSR_DE1;
	hpspc_write(HPSCSI_CSR, cmd);

	cmd = SCMD_XFR;
	len = sc->sc_dleft;

	if ((len & (DEV_BSIZE -1)) != 0) {
		cmd |= SCMD_PAD;
#if 0
		/*
		 * XXX - If we don't do this, the last 2 or 4 bytes
		 * (depending on word/lword DMA) of a read get trashed.
		 * It looks like it is necessary for the DMA to complete
		 * before the SPC goes into "pad mode"???  Note: if we
		 * also do this on a write, the request never completes.
		 */
		if ((dsc->sc_dflags & SCSI_DATAIN) != 0)
			len += 2;
#endif
	}

	spc_write(TCH, len >> 16);
	spc_write(TCM, len >>  8);
	spc_write(TCL, len);
	spc_write(PCTL, sc->sc_phase | PCTL_BFINT_ENAB);
	spc_write(SCMD, cmd);
}

void
spc_dio_dmadone(struct spc_softc *sc)
{
	struct spc_dio_softc *dsc = (struct spc_dio_softc *)sc;
	int resid, trans;
	u_int8_t cmd;

	/* wait DMA complete */
	if ((spc_read(SSTS) & SSTS_BUSY) != 0) {
		int timeout = 1000; /* XXX how long? */
		while ((spc_read(SSTS) & SSTS_BUSY) != 0) {
			if (--timeout < 0) {
				printf("%s: DMA complete timeout\n",
				    sc->sc_dev.dv_xname);
				timeout = 1000;
			}
			DELAY(1);
		}
	}

	sc->sc_flags &= ~SPC_DOINGDMA;
	if ((dsc->sc_dflags & SCSI_HAVEDMA) != 0) {
		dsc->sc_dflags &= ~SCSI_HAVEDMA;
		dmafree(&dsc->sc_dq);
	}

	cmd = hpspc_read(HPSCSI_CSR);
	cmd &= ~(CSR_DE1 | CSR_DE0);
	hpspc_write(HPSCSI_CSR, cmd);

	resid = spc_read(TCH) << 16 |
	    spc_read(TCM) << 8 |
	    spc_read(TCL);
	trans = sc->sc_dleft - resid;
	sc->sc_dp += trans;
	sc->sc_dleft -= trans;
}

void
spc_dio_dmastop(void *arg)
{
	struct spc_dio_softc *dsc = (struct spc_dio_softc *)arg;
	struct spc_softc *sc = &dsc->sc_spc;
	u_int8_t cmd;

	cmd = hpspc_read(HPSCSI_CSR);
	cmd &= ~(CSR_DE1 | CSR_DE0);
	hpspc_write(HPSCSI_CSR, cmd);

	dsc->sc_dflags &= ~SCSI_HAVEDMA;
	sc->sc_flags &= ~SPC_DOINGDMA;
}

int
spc_dio_intr(void *arg)
{
	struct spc_dio_softc *dsc = (struct spc_dio_softc *)arg;

	/* if we are sharing the ipl level, this interrupt may not be for us. */
	if ((hpspc_read(HPSCSI_CSR) & (CSR_IE | CSR_IR)) != (CSR_IE | CSR_IR))
		return 0;

	return spc_intr(arg);
}

void
spc_dio_reset(struct spc_softc *sc)
{
	struct spc_dio_softc *dsc = (struct spc_dio_softc *)sc;

	spc_reset(sc);
	hpspc_write(HPSCSI_HCONF, 0x00);
}
