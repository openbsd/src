/*	$NetBSD: tcds_dma.c,v 1.5 1995/09/05 15:07:05 cgd Exp $	*/

/*
 * Copyright (c) 1994 Peter Galbavy.  All rights reserved.
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
 *	This product includes software developed by Peter Galbavy.
 * 4. The name of the author may not be used to endorse or promote products
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

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/user.h>

#include <alpha/autoconf.h>
#include <alpha/cpu.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#include <alpha/tc/tcds_dmavar.h>
#include <alpha/tc/espreg.h>
#include <alpha/tc/espvar.h>
#include <alpha/tc/tcds.h>

int dmaprint		__P((void *, char *));
void dmaattach		__P((struct device *, struct device *, void *));
int dmamatch		__P((struct device *, struct cfdata *, void *));
void dma_reset		__P((struct dma_softc *));
void dma_start		__P((struct dma_softc *, caddr_t *, size_t *, int));
int dmaintr		__P((struct dma_softc *));

/*
 * dma_init --
 *	Initialize AXP DMA for ESP.
 */
void
dma_init(sc)
	struct dma_softc *sc;
{
	/* TCDS register address initialization. */
	tcds_dma_init(sc, sc->sc_dev.dv_unit);

	/* Indirect functions. */
	sc->reset = dma_reset;
	sc->enintr = NULL;
	sc->start = dma_start;
	sc->isintr = NULL;
	sc->intr = dmaintr;
}

void
dma_reset(sc)
	struct dma_softc *sc;
{
	/* TCDS SCSI disable/reset/enable. */
	tcds_scsi_reset(sc->sc_dev.dv_unit);

	sc->sc_active = 0;			/* and of course we aren't */
}

/*
 * SPARC:
 *	The rules say we cannot transfer more than the limit of this
 *	DMA chip (64k for old and 16Mb for new), and we cannot cross
 *	a 16Mb boundary.
 * AXP:
 *	We're doing physical DMA.  Since pages on the AXP are 8K, we
 *	don't transfer more than that, or cross an 8K boundary, in a
 *	single transfer.
 */
#define ESPMAX								\
	((sc->sc_esp->sc_rev == ESP200) ? (16 * 1024 * 1024) :		\
	(sc->sc_esp->sc_rev == NCR53C94) ? (8 * 1024) : (64 * 1024))
#define DMAMAX(a)							\
	(sc->sc_esp->sc_rev == NCR53C94) ? 0x2000 - ((a) & 0x1fff) :	\
	(0x01000000 - ((a) & 0x00ffffff))

/*
 * start a dma transfer or keep it going
 */
void
dma_start(sc, addr, len, datain)
	struct dma_softc *sc;
	char **addr;
	size_t *len;
	int datain;
{
	/* we do the loading of the transfer counter */
	volatile espreg_t *esp = sc->sc_esp->sc_reg;
	u_int32_t dic;
	int size;

	sc->sc_dmaaddr = addr;
	sc->sc_dmalen = len;

#ifdef DMA_DEBUG
	printf("%s: dma_start %d bytes %s 0x%08x\n",
	    sc->sc_dev.dv_xname, *len, datain ? "to" : "from", *addr);
#endif
	size = min(*len, ESPMAX);
	size = min(size, DMAMAX((size_t)*addr));
	sc->sc_dmasize = size;

#ifdef DMA_DEBUG
	printf("dma_start: transfer = %d\n", sc->sc_dmasize);
#endif

#ifdef TK_NOT_NECESSARY
	tcds_dma_disable(sc->sc_dev.dv_unit);			wbflush();
	tcds_scsi_disable(sc->sc_dev.dv_unit);			wbflush();
#endif

	/* Load the count in. */
	esp->esp_tcl = size & 0xff;				wbflush();
	esp->esp_tcm = (size >> 8) & 0xff;			wbflush();
	if (sc->sc_esp->sc_rev == ESP200) {
		esp->esp_tch = (size >> 16) & 0xff;		wbflush();
	}
	ESPCMD(sc->sc_esp, ESPCMD_NOP|ESPCMD_DMA);

	/* Load address, set/clear unaligned transfer and read/write bits. */
	/* XXX PICK AN ADDRESS TYPE, AND STICK TO IT! */
	if ((u_long)*addr > VM_MIN_KERNEL_ADDRESS) {
		*sc->sda = vatopa((u_long)*addr) >> 2;		wbflush();
	} else {
		*sc->sda = k0segtophys((u_long)*addr) >> 2;	wbflush();
	}
	dic = *sc->dic;
	dic &= ~TCDS_DIC_ADDRMASK;
	dic |= (vm_offset_t)*addr & TCDS_DIC_ADDRMASK;
	if (datain)
		dic |= TCDS_DIC_WRITE;
	else
		dic &= ~TCDS_DIC_WRITE;
	*sc->dic = dic;						wbflush();

#ifdef TK_NOT_NECESSARY
	tcds_scsi_enable(sc->sc_dev.dv_unit);			wbflush();
	tcds_dma_enable(sc->sc_dev.dv_unit);			wbflush();
#endif

	/* and kick the SCSI */
	ESPCMD(sc->sc_esp, ESPCMD_TRANS|ESPCMD_DMA);

	sc->sc_active = 1;
}

/*
 * Pseudo (chained) interrupt from the esp driver to kick the
 * current running DMA transfer. I am replying on espintr() to
 * pickup and clean errors for now
 *
 * return 1 if it was a DMA continue.
 */
int
dmaintr(sc)
	struct dma_softc *sc;
{
	volatile espreg_t *esp = sc->sc_esp->sc_reg;
	u_int32_t dud;
	int resid, trans;
	char *addr;

#ifdef DMA_DEBUG
	printf("%s: dmaintr\n", sc->sc_dev.dv_xname);
#endif

#ifdef DIAGNOSTIC
	if (sc->sc_active == 0)
		panic("dmaintr: %s: DMA inactive", sc->sc_dev.dv_xname);
#endif

	if (tcds_scsi_iserr(sc))
		return (0);

#ifdef TK_NOT_NECESSARY
	tcds_dma_disable(sc->sc_dev.dv_unit);			wbflush();
	tcds_scsi_disable(sc->sc_dev.dv_unit);			wbflush();
#endif
	sc->sc_active = 0;

	resid = RR(esp->esp_fflag) & ESPFIFO_FF;		wbflush();
	if (!(*sc->dic & TCDS_DIC_WRITE) && resid != 0) {	wbflush();
		printf("%s: empty FIFO of %d ", sc->sc_dev.dv_xname, resid);
		ESPCMD(sc->sc_esp, ESPCMD_FLUSH);
		DELAY(1);
	}

	resid += RR(esp->esp_tcl);				wbflush();
	resid += RR(esp->esp_tcm) << 8;				wbflush();
	if (sc->sc_esp->sc_rev == ESP200)
		resid += RR(esp->esp_tch) << 16;		wbflush();
	trans = sc->sc_dmasize - resid;
	if (trans < 0) {			/* transferred < 0? */
		printf("%s: xfer (%d) > req (%d)\n",
		    sc->sc_dev.dv_xname, trans, sc->sc_dmasize);
		trans = sc->sc_dmasize;
	}

	/* Handle unaligned starting address, length. */
	dud = *sc->dud0;					wbflush();
	if (dud & (TCDS_SCSI0_DUD0_VALID01 |
	    TCDS_SCSI0_DUD0_VALID10 | TCDS_SCSI0_DUD0_VALID11)) {
		addr = (char *)((vm_offset_t)*sc->sc_dmaaddr & ~0x03);
		if (dud & TCDS_SCSI0_DUD0_VALID01)
			addr[1] = dud & TCDS_SCSI0_DUD0_BYTE01;
		if (dud & TCDS_SCSI0_DUD0_VALID10)
			addr[2] = dud & TCDS_SCSI0_DUD0_BYTE10;
		if (dud & TCDS_SCSI0_DUD0_VALID11)
			addr[3] = dud & TCDS_SCSI0_DUD0_BYTE11;
	}
	dud = *sc->dud1;					wbflush();
	if (dud & (TCDS_SCSI0_DUD1_VALID00 |
	    TCDS_SCSI0_DUD1_VALID01 | TCDS_SCSI0_DUD1_VALID10)) {
		addr = (char *)((vm_offset_t)(*sc->sc_dmaaddr + trans) & ~0x03);
		if (dud & TCDS_SCSI0_DUD1_VALID00)
			addr[0] = dud & TCDS_SCSI0_DUD1_BYTE00;
		if (dud & TCDS_SCSI0_DUD1_VALID01)
			addr[1] = dud & TCDS_SCSI0_DUD1_BYTE01;
		if (dud & TCDS_SCSI0_DUD1_VALID10)
			addr[2] = dud & TCDS_SCSI0_DUD1_BYTE10;
	}

#ifdef DMA_DEBUG
	{ u_int32_t tcl, tcm, tch;
	tcl = RR(esp->esp_tcl);					wbflush();
	tcm = RR(esp->esp_tcm);					wbflush();
	tch = sc->sc_esp->sc_rev == ESP200 ? RR(esp->esp_tch) : 0;
								wbflush();
	printf("dmaintr: tcl=%d, tcm=%d, tch=%d, resid=%d, trans=%d\n",
	    tcl, tcm, tch, resid, trans);
	}
#endif
#ifdef SPARC_DRIVER
	if (DMACSR(sc) & D_WRITE)
		cache_flush(*sc->sc_dmaaddr, trans);
#endif
	*sc->sc_dmalen -= trans;
	*sc->sc_dmaaddr += trans;

	if (!*sc->sc_dmalen ||
	    sc->sc_esp->sc_phase != sc->sc_esp->sc_prevphase) {
#ifdef TK_NOT_NECESSARY
		tcds_scsi_enable(sc->sc_dev.dv_unit);		wbflush();
#endif
		return 0;
	}

	/* and again */
	dma_start(sc, sc->sc_dmaaddr, sc->sc_dmalen, *sc->dic & TCDS_DIC_WRITE);
	return 1;
}
