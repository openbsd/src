/*	$NetBSD: wstsc.c,v 1.9 1995/08/18 15:28:17 chopps Exp $	*/

/*
 * Copyright (c) 1994 Michael L. Hitch
 * Copyright (c) 1982, 1990 The Regents of the University of California.
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
 *	@(#)supradma.c
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>
#include <amiga/amiga/device.h>
#include <amiga/amiga/isr.h>
#include <amiga/dev/scireg.h>
#include <amiga/dev/scivar.h>
#include <amiga/dev/zbusvar.h>

int wstscprint __P((void *auxp, char *));
void wstscattach __P((struct device *, struct device *, void *));
int wstscmatch __P((struct device *, struct cfdata *, void *));

int wstsc_dma_xfer_in __P((struct sci_softc *dev, int len,
    register u_char *buf, int phase));
int wstsc_dma_xfer_out __P((struct sci_softc *dev, int len,
    register u_char *buf, int phase));
int wstsc_dma_xfer_in2 __P((struct sci_softc *dev, int len,
    register u_short *buf, int phase));
int wstsc_dma_xfer_out2 __P((struct sci_softc *dev, int len,
    register u_short *buf, int phase));
int wstsc_intr __P((struct sci_softc *));

struct scsi_adapter wstsc_scsiswitch = {
	sci_scsicmd,
	sci_minphys,
	0,			/* no lun support */
	0,			/* no lun support */
};

struct scsi_device wstsc_scsidev = {
	NULL,		/* use default error handler */
	NULL,		/* do not have a start functio */
	NULL,		/* have no async handler */
	NULL,		/* Use default done routine */
};

#define QPRINTF

#ifdef DEBUG
extern int sci_debug;
#endif

extern int sci_data_wait;

int supradma_pseudo = 0;	/* 0=none, 1=byte, 2=word */

struct cfdriver wstsccd = {
	NULL, "wstsc", (cfmatch_t)wstscmatch, wstscattach, 
	DV_DULL, sizeof(struct sci_softc), NULL, 0 };

/*
 * if this a Supra WordSync board
 */
int
wstscmatch(pdp, cdp, auxp)
	struct device *pdp;
	struct cfdata *cdp;
	void *auxp;
{
	struct zbus_args *zap;

	zap = auxp;

	/*
	 * Check manufacturer and product id.
	 */
	if (zap->manid == 1056 && (
	    zap->prodid == 12 ||	/* WordSync */
	    zap->prodid == 13))		/* ByteSync */
		return(1);
	else
		return(0);
}

void
wstscattach(pdp, dp, auxp)
	struct device *pdp, *dp;
	void *auxp;
{
	volatile u_char *rp;
	struct sci_softc *sc;
	struct zbus_args *zap;

	printf("\n");

	zap = auxp;
	
	sc = (struct sci_softc *)dp;
	rp = zap->va;
	/*
	 * set up 5380 register pointers
	 * (Needs check on which Supra board this is - for now,
	 *  just do the WordSync)
	 */
	sc->sci_data = rp + 0;
	sc->sci_odata = rp + 0;
	sc->sci_icmd = rp + 2;
	sc->sci_mode = rp + 4;
	sc->sci_tcmd = rp + 6;
	sc->sci_bus_csr = rp + 8;
	sc->sci_sel_enb = rp + 8;
	sc->sci_csr = rp + 10;
	sc->sci_dma_send = rp + 10;
	sc->sci_idata = rp + 12;
	sc->sci_trecv = rp + 12;
	sc->sci_iack = rp + 14;
	sc->sci_irecv = rp + 14;

	if (supradma_pseudo == 2) {
		sc->dma_xfer_in = wstsc_dma_xfer_in2;
		sc->dma_xfer_out = wstsc_dma_xfer_out2;
	}
	else if (supradma_pseudo == 1) {
		sc->dma_xfer_in = wstsc_dma_xfer_in;
		sc->dma_xfer_out = wstsc_dma_xfer_out;
	}

	sc->sc_isr.isr_intr = wstsc_intr;
	sc->sc_isr.isr_arg = sc;
	sc->sc_isr.isr_ipl = 2;
	add_isr(&sc->sc_isr);

	scireset(sc);

	sc->sc_link.adapter_softc = sc;
	sc->sc_link.adapter_target = 7;
	sc->sc_link.adapter = &wstsc_scsiswitch;
	sc->sc_link.device = &wstsc_scsidev;
	sc->sc_link.openings = 1;
	TAILQ_INIT(&sc->sc_xslist);

	/*
	 * attach all scsi units on us
	 */
	config_found(dp, &sc->sc_link, wstscprint);
}

/*
 * print diag if pnp is NULL else just extra
 */
int
wstscprint(auxp, pnp)
	void *auxp;
	char *pnp;
{
	if (pnp == NULL)
		return(UNCONF);
	return(QUIET);
}

int
wstsc_dma_xfer_in (dev, len, buf, phase)
	struct sci_softc *dev;
	int len;
	register u_char *buf;
	int phase;
{
	int wait = sci_data_wait;
	u_char csr;
	u_char *obp = (u_char *) buf;
	volatile register u_char *sci_dma = dev->sci_idata;
	volatile register u_char *sci_csr = dev->sci_csr;

	QPRINTF(("supradma_in %d, csr=%02x\n", len, *dev->sci_bus_csr));

	*dev->sci_tcmd = phase;
	*dev->sci_icmd = 0;
	*dev->sci_mode = SCI_MODE_DMA;
	*dev->sci_irecv = 0;

	while (len >= 128) {
		wait = sci_data_wait;
		while ((*sci_csr & (SCI_CSR_DREQ|SCI_CSR_PHASE_MATCH)) !=
		  (SCI_CSR_DREQ|SCI_CSR_PHASE_MATCH)) {
			if (!(*sci_csr & SCI_CSR_PHASE_MATCH)
			  || !(*dev->sci_bus_csr & SCI_BUS_BSY)
			  || --wait < 0) {
#ifdef DEBUG
				if (sci_debug | 1)
					printf("supradma2_in fail: l%d i%x w%d\n",
					len, *dev->sci_bus_csr, wait);
#endif
				*dev->sci_mode = 0;
				return 0;
			}
		}

#define R1	(*buf++ = *sci_dma)
		R1; R1; R1; R1; R1; R1; R1; R1;
		R1; R1; R1; R1; R1; R1; R1; R1;
		R1; R1; R1; R1; R1; R1; R1; R1;
		R1; R1; R1; R1; R1; R1; R1; R1;
		R1; R1; R1; R1; R1; R1; R1; R1;
		R1; R1; R1; R1; R1; R1; R1; R1;
		R1; R1; R1; R1; R1; R1; R1; R1;
		R1; R1; R1; R1; R1; R1; R1; R1;
		R1; R1; R1; R1; R1; R1; R1; R1;
		R1; R1; R1; R1; R1; R1; R1; R1;
		R1; R1; R1; R1; R1; R1; R1; R1;
		R1; R1; R1; R1; R1; R1; R1; R1;
		R1; R1; R1; R1; R1; R1; R1; R1;
		R1; R1; R1; R1; R1; R1; R1; R1;
		R1; R1; R1; R1; R1; R1; R1; R1;
		R1; R1; R1; R1; R1; R1; R1; R1;
		len -= 128;
	}

	while (len > 0) {
		wait = sci_data_wait;
		while ((*sci_csr & (SCI_CSR_DREQ|SCI_CSR_PHASE_MATCH)) !=
		  (SCI_CSR_DREQ|SCI_CSR_PHASE_MATCH)) {
			if (!(*sci_csr & SCI_CSR_PHASE_MATCH)
			  || !(*dev->sci_bus_csr & SCI_BUS_BSY)
			  || --wait < 0) {
#ifdef DEBUG
				if (sci_debug | 1)
					printf("supradma1_in fail: l%d i%x w%d\n",
					len, *dev->sci_bus_csr, wait);
#endif
				*dev->sci_mode = 0;
				return 0;
			}
		}

		*buf++ = *sci_dma;
		len--;
	}

	QPRINTF(("supradma_in {%d} %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
	  len, obp[0], obp[1], obp[2], obp[3], obp[4], obp[5],
	  obp[6], obp[7], obp[8], obp[9]));

	*dev->sci_mode = 0;
	return 0;
}

int
wstsc_dma_xfer_out (dev, len, buf, phase)
	struct sci_softc *dev;
	int len;
	register u_char *buf;
	int phase;
{
	int wait = sci_data_wait;
	u_char csr;
	u_char *obp = buf;
	volatile register u_char *sci_dma = dev->sci_data;
	volatile register u_char *sci_csr = dev->sci_csr;

	QPRINTF(("supradma_out %d, csr=%02x\n", len, *dev->sci_bus_csr));

	QPRINTF(("supradma_out {%d} %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
  	 len, buf[0], buf[1], buf[2], buf[3], buf[4], buf[5],
	 buf[6], buf[7], buf[8], buf[9]));

	*dev->sci_tcmd = phase;
	*dev->sci_mode = SCI_MODE_DMA;
	*dev->sci_icmd = SCI_ICMD_DATA;
	*dev->sci_dma_send = 0;
	while (len > 0) {
		wait = sci_data_wait;
		while ((*sci_csr & (SCI_CSR_DREQ|SCI_CSR_PHASE_MATCH)) !=
		  (SCI_CSR_DREQ|SCI_CSR_PHASE_MATCH)) {
			if (!(*sci_csr & SCI_CSR_PHASE_MATCH)
			  || !(*dev->sci_bus_csr & SCI_BUS_BSY)
			  || --wait < 0) {
#ifdef DEBUG
				if (sci_debug)
					printf("supradma_out fail: l%d i%x w%d\n",
					len, csr, wait);
#endif
				*dev->sci_mode = 0;
				return 0;
			}
		}

		*sci_dma = *buf++;
		len--;
	}

	wait = sci_data_wait;
	while ((*sci_csr & (SCI_CSR_DREQ|SCI_CSR_PHASE_MATCH)) ==
	  SCI_CSR_PHASE_MATCH && --wait);


	*dev->sci_mode = 0;
	*dev->sci_icmd = 0;
	return 0;
}


int
wstsc_dma_xfer_in2 (dev, len, buf, phase)
	struct sci_softc *dev;
	int len;
	register u_short *buf;
	int phase;
{
	int wait = sci_data_wait;
	u_char csr;
	u_char *obp = (u_char *) buf;
	volatile register u_short *sci_dma = (u_short *)(dev->sci_idata + 0x10);
	volatile register u_char *sci_csr = dev->sci_csr + 0x10;

	QPRINTF(("supradma_in2 %d, csr=%02x\n", len, *dev->sci_bus_csr));

	*dev->sci_tcmd = phase;
	*dev->sci_mode = SCI_MODE_DMA;
	*dev->sci_icmd = 0;
	*(dev->sci_irecv + 16) = 0;
	while (len >= 128) {
#if 0
		wait = sci_data_wait;
		while ((*sci_csr & (SCI_CSR_DREQ|SCI_CSR_PHASE_MATCH)) !=
		  (SCI_CSR_DREQ|SCI_CSR_PHASE_MATCH)) {
			if (!(*sci_csr & SCI_CSR_PHASE_MATCH)
			  || !(*dev->sci_bus_csr & SCI_BUS_BSY)
			  || --wait < 0) {
#ifdef DEBUG
				if (sci_debug | 1)
					printf("supradma2_in2 fail: l%d i%x w%d\n",
					len, *dev->sci_bus_csr, wait);
#endif
				*dev->sci_mode &= ~SCI_MODE_DMA;
				return 0;
			}
		}
#else
		while (!(*sci_csr & SCI_CSR_DREQ))
			;
#endif

#define R2	(*buf++ = *sci_dma)
		R2; R2; R2; R2; R2; R2; R2; R2;
		R2; R2; R2; R2; R2; R2; R2; R2;
		R2; R2; R2; R2; R2; R2; R2; R2;
		R2; R2; R2; R2; R2; R2; R2; R2;
		R2; R2; R2; R2; R2; R2; R2; R2;
		R2; R2; R2; R2; R2; R2; R2; R2;
		R2; R2; R2; R2; R2; R2; R2; R2;
		R2; R2; R2; R2; R2; R2; R2; R2;
		len -= 128;
	}
	while (len > 0) {
#if 0
		wait = sci_data_wait;
		while ((*sci_csr & (SCI_CSR_DREQ|SCI_CSR_PHASE_MATCH)) !=
		  (SCI_CSR_DREQ|SCI_CSR_PHASE_MATCH)) {
			if (!(*sci_csr & SCI_CSR_PHASE_MATCH)
			  || !(*dev->sci_bus_csr & SCI_BUS_BSY)
			  || --wait < 0) {
#ifdef DEBUG
				if (sci_debug | 1)
					printf("supradma1_in2 fail: l%d i%x w%d\n",
					len, *dev->sci_bus_csr, wait);
#endif
				*dev->sci_mode &= ~SCI_MODE_DMA;
				return 0;
			}
		}
#else
		while (!(*sci_csr * SCI_CSR_DREQ))
			;
#endif

		*buf++ = *sci_dma;
		len -= 2;
	}

	QPRINTF(("supradma_in2 {%d} %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
	  len, obp[0], obp[1], obp[2], obp[3], obp[4], obp[5],
	  obp[6], obp[7], obp[8], obp[9]));

	*dev->sci_irecv = 0;
	*dev->sci_mode = 0;
	return 0;
}

int
wstsc_dma_xfer_out2 (dev, len, buf, phase)
	struct sci_softc *dev;
	int len;
	register u_short *buf;
	int phase;
{
	int wait = sci_data_wait;
	u_char csr;
	u_char *obp = (u_char *) buf;
	volatile register u_short *sci_dma = (ushort *)(dev->sci_data + 0x10);
	volatile register u_char *sci_bus_csr = dev->sci_bus_csr;

	QPRINTF(("supradma_out2 %d, csr=%02x\n", len, *dev->sci_bus_csr));

	QPRINTF(("supradma_out2 {%d} %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
  	 len, obp[0], obp[1], obp[2], obp[3], obp[4], obp[5],
	 obp[6], obp[7], obp[8], obp[9]));

	*dev->sci_tcmd = phase;
	*dev->sci_mode = SCI_MODE_DMA;
	*dev->sci_icmd = SCI_ICMD_DATA;
	*dev->sci_dma_send = 0;
	while (len > 64) {
#if 0
		wait = sci_data_wait;
		while ((*sci_csr & (SCI_CSR_DREQ|SCI_CSR_PHASE_MATCH)) !=
		  (SCI_CSR_DREQ|SCI_CSR_PHASE_MATCH)) {
			if (!(*sci_csr & SCI_CSR_PHASE_MATCH)
			  || !(*dev->sci_bus_csr & SCI_BUS_BSY)
			  || --wait < 0) {
#ifdef DEBUG
				if (sci_debug)
					printf("supradma_out2 fail: l%d i%x w%d\n",
					len, csr, wait);
#endif
				*dev->sci_mode = 0;
				return 0;
			}
		}
#else
		*dev->sci_mode = 0;
		*dev->sci_icmd &= ~SCI_ICMD_ACK;
		while (!(*sci_bus_csr & SCI_BUS_REQ))
			;
		*dev->sci_mode = SCI_MODE_DMA;
		*dev->sci_dma_send = 0;
#endif

#define W2	(*sci_dma = *buf++)
		W2; W2; W2; W2; W2; W2; W2; W2;
		W2; W2; W2; W2; W2; W2; W2; W2;
		if (*(sci_bus_csr + 0x10) & SCI_BUS_REQ)
			;
		len -= 64;
	}

	while (len > 0) {
#if 0
		wait = sci_data_wait;
		while ((*sci_csr & (SCI_CSR_DREQ|SCI_CSR_PHASE_MATCH)) !=
		  (SCI_CSR_DREQ|SCI_CSR_PHASE_MATCH)) {
			if (!(*sci_csr & SCI_CSR_PHASE_MATCH)
			  || !(*dev->sci_bus_csr & SCI_BUS_BSY)
			  || --wait < 0) {
#ifdef DEBUG
				if (sci_debug)
					printf("supradma_out2 fail: l%d i%x w%d\n",
					len, csr, wait);
#endif
				*dev->sci_mode = 0;
				return 0;
			}
		}
#else
		*dev->sci_mode = 0;
		*dev->sci_icmd &= ~SCI_ICMD_ACK;
		while (!(*sci_bus_csr & SCI_BUS_REQ))
			;
		*dev->sci_mode = SCI_MODE_DMA;
		*dev->sci_dma_send = 0;
#endif

		*sci_dma = *buf++;
		if (*(sci_bus_csr + 0x10) & SCI_BUS_REQ)
			;
		len -= 2;
	}

#if 0
	wait = sci_data_wait;
	while ((*sci_csr & (SCI_CSR_DREQ|SCI_CSR_PHASE_MATCH)) ==
	  SCI_CSR_PHASE_MATCH && --wait);
#endif


	*dev->sci_irecv = 0;
	*dev->sci_icmd &= ~SCI_ICMD_ACK;
	*dev->sci_mode = 0;
	*dev->sci_icmd = 0;
	return 0;
}

int
wstsc_intr(dev)
	struct sci_softc *dev;
{
	int i, found;
	u_char stat;

	if ((*(dev->sci_csr + 0x10) & SCI_CSR_INT) == 0)
		return (0);
	stat = *(dev->sci_iack + 0x10);
	return (1);
}
