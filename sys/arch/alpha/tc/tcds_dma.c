/*	$NetBSD: tcds_dma.c,v 1.6 1995/12/20 00:40:32 cgd Exp $	*/

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

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#include <dev/tc/tcvar.h>
#include <alpha/tc/tcdsreg.h>
#include <alpha/tc/tcdsvar.h>
#include <alpha/tc/espreg.h>
#include <alpha/tc/espvar.h>

void
tcds_dma_reset(sc)
	struct tcds_slotconfig *sc;
{
	/* TCDS SCSI disable/reset/enable. */
	tcds_scsi_reset(sc);			/* XXX */

	sc->sc_active = 0;			/* and of course we aren't */
}


void
tcds_dma_enintr(sc)
	struct tcds_slotconfig *sc;
{

	/* XXX */
}

int
tcds_dma_isintr(sc)
	struct tcds_slotconfig *sc;
{
	int x;

	x = tcds_scsi_isintr(sc, 0);

	/* Clear the TCDS interrupt bit. */
	(void)tcds_scsi_isintr(sc, 1);

	/* XXX */
	return x;
}

#define ESPMAX		(64 * 1024)
#define DMAMAX(a)	(0x02000 - ((a) & 0x1fff))

/*
 * start a dma transfer or keep it going
 */
void
tcds_dma_start(sc, addr, len, datain)
	struct tcds_slotconfig *sc;
	caddr_t *addr;
	size_t *len;
	int datain;				/* DMA into main memory */
{
	u_int32_t dic;
	size_t size;

	sc->sc_dmaaddr = addr;
	sc->sc_dmalen = len;
	sc->sc_iswrite = datain;

	ESP_DMA(("tcds_dma %d: start %d@0x%lx,%d\n", sc->sc_slot, *sc->sc_dmalen, *sc->sc_dmaaddr, sc->sc_iswrite));

	/*
	 * the rules say we cannot transfer more than the limit
	 * of this DMA chip (64k) and we cannot cross a 8k boundary.
	 */
	size = min(*sc->sc_dmalen, ESPMAX);
	size = min(size, DMAMAX((size_t) *sc->sc_dmaaddr));
	sc->sc_dmasize = size;

	ESP_DMA(("dma_start: dmasize = %d\n", sc->sc_dmasize));

	/* Load address, set/clear unaligned transfer and read/write bits. */
	/* XXX PICK AN ADDRESS TYPE, AND STICK TO IT! */
	if ((u_long)*addr > VM_MIN_KERNEL_ADDRESS) {
		*sc->sc_sda = vatopa((u_long)*addr) >> 2;
	} else {
		*sc->sc_sda = k0segtophys((u_long)*addr) >> 2;
	}
	wbflush();
	dic = *sc->sc_dic;
	dic &= ~TCDS_DIC_ADDRMASK;
	dic |= (vm_offset_t)*addr & TCDS_DIC_ADDRMASK;
	if (datain)
		dic |= TCDS_DIC_WRITE;
	else
		dic &= ~TCDS_DIC_WRITE;
	*sc->sc_dic = dic;
	wbflush();

	/* Program the SCSI counter */
	ESP_WRITE_REG(sc->sc_esp, ESP_TCL, size);
	ESP_WRITE_REG(sc->sc_esp, ESP_TCM, size >> 8);
	if (sc->sc_esp->sc_rev == ESP200) {
		ESP_WRITE_REG(sc->sc_esp, ESP_TCH, size >> 16);
	}
	/* load the count in */
	ESPCMD(sc->sc_esp, ESPCMD_NOP|ESPCMD_DMA);

	/*
	 * Note that if `size' is 0, we've already transceived all
	 * the bytes we want but we're still in the DATA PHASE.
	 * Apparently, the device needs padding. Also, a transfer
	 * size of 0 means "maximum" to the chip DMA logic.
	 */
	ESPCMD(sc->sc_esp, (size==0?ESPCMD_TRPAD:ESPCMD_TRANS)|ESPCMD_DMA);

	sc->sc_active = 1;

	/* Start DMA */
	tcds_dma_enable(sc, 1);
}

/*
 * Pseudo (chained) interrupt from the esp driver to kick the
 * current running DMA transfer. I am replying on espintr() to
 * pickup and clean errors for now
 *
 * return 1 if it was a DMA continue.
 */
int
tcds_dma_intr(sc)
	struct tcds_slotconfig *sc;
{
	u_int32_t dud;
	int trans = 0, resid = 0;
	u_int32_t *addr, dudmask;
	u_char tcl, tcm, tch;

	ESP_DMA(("tcds_dma %d: intr", sc->sc_slot));

	if (tcds_scsi_iserr(sc))
		return (0);

	/* This is an "assertion" :) */
	if (sc->sc_active == 0)
		panic("dmaintr: DMA wasn't active");

	/* DMA has stopped */
	tcds_dma_enable(sc, 0);
	sc->sc_active = 0;

	if (sc->sc_dmasize == 0) {
		/* A "Transfer Pad" operation completed */
		tcl = ESP_READ_REG(sc->sc_esp, ESP_TCL);
		tcm = ESP_READ_REG(sc->sc_esp, ESP_TCM);
		ESP_DMA(("dmaintr: discarded %d bytes (tcl=%d, tcm=%d)\n",
		    tcl | (tcm << 8), tcl, tcm));
		return 0;
	}

	if (!sc->sc_iswrite &&
	    (resid = (ESP_READ_REG(sc->sc_esp, ESP_FFLAG) & ESPFIFO_FF)) != 0) {
		printf("empty FIFO of %d ", resid);
		ESPCMD(sc->sc_esp, ESPCMD_FLUSH);
		DELAY(1);
	}

	resid += (tcl = ESP_READ_REG(sc->sc_esp, ESP_TCL));
	resid += (tcm = ESP_READ_REG(sc->sc_esp, ESP_TCM)) << 8;
	if (sc->sc_esp->sc_rev == ESP200)
		resid += (tch = ESP_READ_REG(sc->sc_esp, ESP_TCH)) << 16;
	else
		tch = 0;

	if (resid == 0 && (sc->sc_esp->sc_rev <= ESP100A) &&
	    (sc->sc_esp->sc_espstat & ESPSTAT_TC) == 0)
		resid = 65536;

	trans = sc->sc_dmasize - resid;
	if (trans < 0) {			/* transferred < 0 ? */
		printf("tcds_dma %d: xfer (%d) > req (%d)\n",
		    sc->sc_slot, trans, sc->sc_dmasize);
		trans = sc->sc_dmasize;
	}

	ESP_DMA(("dmaintr: tcl=%d, tcm=%d, tch=%d; trans=%d, resid=%d\n",
	    tcl, tcm, tch, trans, resid));

	/*
	 * Clean up unaligned DMAs into main memory.
	 */
	if (sc->sc_iswrite) {
		/* Handle unaligned starting address, length. */
		dud = *sc->sc_dud0;
		if ((dud & TCDS_DUD0_VALIDBITS) != 0) {
			addr = (u_int32_t *)
			    ((vm_offset_t)sc->sc_dmaaddr & ~0x3);
			dudmask = 0;
			if (dud & TCDS_DUD0_VALID00)
				panic("tcds_dma: dud0 byte 0 valid");
			if (dud & TCDS_DUD0_VALID01)
				dudmask |= TCDS_DUD_BYTE01;
			if (dud & TCDS_DUD0_VALID10)
				dudmask |= TCDS_DUD_BYTE10;
#ifdef DIAGNOSTIC
			if (dud & TCDS_DUD0_VALID11)
				dudmask |= TCDS_DUD_BYTE11;
#endif
			ESP_DMA(("dud0 at 0x%lx dudmask 0x%x\n",
			    addr, dudmask));
			addr = (u_int32_t *)phystok0seg(addr);
			*addr = (*addr & ~dudmask) | (dud & dudmask);
		}
		dud = *sc->sc_dud1;
		if ((dud & TCDS_DUD1_VALIDBITS) != 0) {
	
			addr = (u_int32_t *)
			    ((vm_offset_t)*sc->sc_sda << 2);
			dudmask = 0;
			if (dud & TCDS_DUD1_VALID00)
				dudmask |= TCDS_DUD_BYTE00;
			if (dud & TCDS_DUD1_VALID01)
				dudmask |= TCDS_DUD_BYTE01;
			if (dud & TCDS_DUD1_VALID10)
				dudmask |= TCDS_DUD_BYTE10;
#ifdef DIAGNOSTIC
			if (dud & TCDS_DUD1_VALID11)
				panic("tcds_dma: dud1 byte 3 valid");
#endif
			ESP_DMA(("dud1 at 0x%lx dudmask 0x%x\n",
			    addr, dudmask));
			addr = (u_int32_t *)phystok0seg(addr);
			*addr = (*addr & ~dudmask) | (dud & dudmask);
		}
		/* XXX deal with saved residual byte? */
	}

	*sc->sc_dmalen -= trans;
	*sc->sc_dmaaddr += trans;

#if 0 /* this is not normal operation just yet */
	if (*sc->sc_dmalen == 0 ||
	    sc->sc_esp->sc_phase != sc->sc_esp->sc_prevphase)
		return 0;

	/* and again */
	dma_start(sc, sc->sc_dmaaddr, sc->sc_dmalen, sc->sc_iswrite);
	return 1;
#endif
	return 0;
}
