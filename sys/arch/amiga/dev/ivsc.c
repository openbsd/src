/*	$OpenBSD: ivsc.c,v 1.2 1996/04/21 22:15:32 deraadt Exp $	*/
/*	$NetBSD: ivsc.c,v 1.14 1996/03/28 19:23:34 is Exp $	*/

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
 *	@(#)ivsdma.c
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>
#include <amiga/amiga/custom.h>
#include <amiga/amiga/device.h>
#include <amiga/amiga/isr.h>
#include <amiga/dev/scireg.h>
#include <amiga/dev/scivar.h>
#include <amiga/dev/zbusvar.h>

int ivscprint __P((void *auxp, char *));
void ivscattach __P((struct device *, struct device *, void *));
int ivscmatch __P((struct device *, void *, void *));

int ivsc_intr __P((struct sci_softc *));
int ivsc_dma_xfer_in __P((struct sci_softc *dev, int len,
    register u_char *buf, int phase));
int ivsc_dma_xfer_out __P((struct sci_softc *dev, int len,
    register u_char *buf, int phase));

struct scsi_adapter ivsc_scsiswitch = {
	sci_scsicmd,
	sci_minphys,
	0,			/* no lun support */
	0,			/* no lun support */
};

struct scsi_device ivsc_scsidev = {
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

int ivsdma_pseudo = 1;		/* 0=off, 1=on */

struct cfattach ivsc_ca = {
	sizeof(struct sci_softc), ivscmatch, ivscattach
};

struct cfdriver ivsc_cd = {
	NULL, "ivsc", DV_DULL, NULL, 0
};

/*
 * if this is an IVS board
 */
int
ivscmatch(pdp, match, auxp)
	struct device *pdp;
	void *match, *auxp;
{
	struct zbus_args *zap;

	zap = auxp;

	/*
	 * Check manufacturer and product id.
	 */
	if (zap->manid != 2112 ||	/* If manufacturer is IVS */
	    (zap->prodid != 48 &&	/*   product = Trumpcard 500 */
	    zap->prodid != 52 &&	/*   product = Trumpcard */
	    zap->prodid != 243))	/*   product = Vector SCSI */
		return(0);		/* didn't match */
	return(1);
}

void
ivscattach(pdp, dp, auxp)
	struct device *pdp, *dp;
	void *auxp;
{
	volatile u_char *rp;
	struct sci_softc *sc;
	struct zbus_args *zap;

	printf("\n");

	zap = auxp;
	
	sc = (struct sci_softc *)dp;
	rp = zap->va + 0x40;
	sc->sci_data = rp;
	sc->sci_odata = rp;
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

	if (ivsdma_pseudo == 1) {
		sc->dma_xfer_in = ivsc_dma_xfer_in;
		sc->dma_xfer_out = ivsc_dma_xfer_out;
	}

	sc->sc_isr.isr_intr = ivsc_intr;
	sc->sc_isr.isr_arg = sc;
	sc->sc_isr.isr_ipl = 2;
	add_isr(&sc->sc_isr);

	scireset(sc);

	sc->sc_link.adapter_softc = sc;
	sc->sc_link.adapter_target = 7;
	sc->sc_link.adapter = &ivsc_scsiswitch;
	sc->sc_link.device = &ivsc_scsidev;
	sc->sc_link.openings = 1;
	TAILQ_INIT(&sc->sc_xslist);

	/*
	 * attach all scsi units on us
	 */
	config_found(dp, &sc->sc_link, ivscprint);
}

/*
 * print diag if pnp is NULL else just extra
 */
int
ivscprint(auxp, pnp)
	void *auxp;
	char *pnp;
{
	if (pnp == NULL)
		return(UNCONF);
	return(QUIET);
}

int
ivsc_dma_xfer_in (dev, len, buf, phase)
	struct sci_softc *dev;
	int len;
	register u_char *buf;
	int phase;
{
	int wait = sci_data_wait;
	u_char csr;
	u_char *obp = buf;
	volatile register u_char *sci_dma = dev->sci_idata + 0x20;
	volatile register u_char *sci_csr = dev->sci_csr;

	QPRINTF(("ivsc_dma_in %d, csr=%02x\n", len, *dev->sci_bus_csr));

	*dev->sci_tcmd = phase;
	*dev->sci_mode |= SCI_MODE_DMA;
	*dev->sci_irecv = 0;

	while (len >= 128) {
		wait = sci_data_wait;
		while ((*sci_csr & (SCI_CSR_DREQ|SCI_CSR_PHASE_MATCH)) !=
		  (SCI_CSR_DREQ|SCI_CSR_PHASE_MATCH)) {
			if (!(*sci_csr & SCI_CSR_PHASE_MATCH)
			  || !(*dev->sci_bus_csr & SCI_BUS_BSY)
			  || --wait < 0) {
#ifdef DEBUG
				if (sci_debug)
					printf("ivsc_dma_in2 fail: l%d i%x w%d\n",
					len, csr, wait);
#endif
				*dev->sci_mode &= ~SCI_MODE_DMA;
				return 0;
			}
		}

#define	R1	(*buf++ = *sci_dma)
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
				if (sci_debug)
					printf("ivsc_dma_in1 fail: l%d i%x w%d\n",
					len, csr, wait);
#endif
				*dev->sci_mode &= ~SCI_MODE_DMA;
				return 0;
			}
		}

		*buf++ = *sci_dma;
		len--;
	}

	QPRINTF(("ivsc_dma_in {%d} %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
	  len, obp[0], obp[1], obp[2], obp[3], obp[4], obp[5],
	  obp[6], obp[7], obp[8], obp[9]));

	*dev->sci_mode &= ~SCI_MODE_DMA;
	return 0;
}

int
ivsc_dma_xfer_out (dev, len, buf, phase)
	struct sci_softc *dev;
	int len;
	register u_char *buf;
	int phase;
{
	int wait = sci_data_wait;
	u_char csr;
	u_char *obp = buf;
	volatile register u_char *sci_dma = dev->sci_data + 0x20;
	volatile register u_char *sci_csr = dev->sci_csr;

	QPRINTF(("ivsc_dma_out %d, csr=%02x\n", len, *dev->sci_bus_csr));

	QPRINTF(("ivsc_dma_out {%d} %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
  	 len, buf[0], buf[1], buf[2], buf[3], buf[4], buf[5],
	 buf[6], buf[7], buf[8], buf[9]));

	*dev->sci_tcmd = phase;
	*dev->sci_mode |= SCI_MODE_DMA;
	*dev->sci_icmd |= SCI_ICMD_DATA;
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
					printf("ivsc_dma_out fail: l%d i%x w%d\n",
					len, csr, wait);
#endif
				*dev->sci_mode &= ~SCI_MODE_DMA;
				return 0;
			}
		}

		*sci_dma = *buf++;
		len--;
	}

	wait = sci_data_wait;
	while ((*sci_csr & (SCI_CSR_DREQ|SCI_CSR_PHASE_MATCH)) ==
	  SCI_CSR_PHASE_MATCH && --wait);


	*dev->sci_mode &= ~SCI_MODE_DMA;
	return 0;
}

int
ivsc_intr(dev)
	struct sci_softc *dev;
{
	u_char stat;

	if ((*dev->sci_csr & SCI_CSR_INT) == 0)
		return(0);
	stat = *dev->sci_iack;
	/* XXXX is: something is missing here, at least a: */
	return(1);
}
