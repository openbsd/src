/*	$OpenBSD: esp.c,v 1.16 2003/02/11 19:20:26 mickey Exp $	*/
/*	$NetBSD: esp.c,v 1.59 1996/10/13 02:59:48 christos Exp $	*/

/*
 * Copyright (c) 1997 Jason R. Thorpe.
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
 *	This product includes software developed for the NetBSD Project
 *	by Jason R. Thorpe.
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

/*
 * Copyright (c) 1994 Peter Galbavy
 * Copyright (c) 1995 Paul Kranenburg
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
 *	This product includes software developed by Peter Galbavy
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Based on aic6360 by Jarle Greipsland
 *
 * Acknowledgements: Many of the algorithms used in this driver are
 * inspired by the work of Julian Elischer (julian@tfs.com) and
 * Charles Hannum (mycroft@duality.gnu.ai.mit.edu).  Thanks a million!
 */

/*
 * Initial m68k mac support from Allen Briggs <briggs@macbsd.com>
 * (basically consisting of the match, a bit of the attach, and the
 *  "DMA" glue functions).
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/device.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/queue.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>
#include <scsi/scsi_message.h>

#include <machine/cpu.h>
#include <machine/param.h>

#include <dev/ic/ncr53c9xreg.h>
#include <dev/ic/ncr53c9xvar.h>

#include <machine/viareg.h>

#include <mac68k/dev/espvar.h>

void	espattach(struct device *, struct device *, void *);
int	espmatch(struct device *, void *, void *);

/* Linkup to the rest of the kernel */
struct cfattach esp_ca = {
	sizeof(struct esp_softc), espmatch, espattach
};

struct scsi_adapter esp_switch = {
	ncr53c9x_scsi_cmd,
	minphys,		/* no max at this level; handled by DMA code */
	NULL,
	NULL,
};

struct scsi_device esp_dev = {
	NULL,			/* Use default error handler */
	NULL,			/* have a queue, served by this */
	NULL,			/* have no async handler */
	NULL,			/* Use default 'done' routine */
};

/*
 * Functions and the switch for the MI code.
 */
u_char	esp_read_reg(struct ncr53c9x_softc *, int);
void	esp_write_reg(struct ncr53c9x_softc *, int, u_char);
int	esp_dma_isintr(struct ncr53c9x_softc *);
void	esp_dma_reset(struct ncr53c9x_softc *);
int	esp_dma_intr(struct ncr53c9x_softc *);
int	esp_dma_setup(struct ncr53c9x_softc *, caddr_t *,
	    size_t *, int, size_t *);
void	esp_dma_go(struct ncr53c9x_softc *);
void	esp_dma_stop(struct ncr53c9x_softc *);
int	esp_dma_isactive(struct ncr53c9x_softc *);

struct ncr53c9x_glue esp_glue = {
	esp_read_reg,
	esp_write_reg,
	esp_dma_isintr,
	esp_dma_reset,
	esp_dma_intr,
	esp_dma_setup,
	esp_dma_go,
	esp_dma_stop,
	esp_dma_isactive,
	NULL,			/* gl_clear_latched_intr */
};

int
espmatch(parent, vcf, aux)
	struct device *parent;
	void *vcf, *aux;
{
	struct cfdata *cf = vcf;

	if ((cf->cf_unit == 0) && mac68k_machine.scsi96)
		return (1);
	if ((cf->cf_unit == 1) && mac68k_machine.scsi96_2)
		return (1);
	return (0);
}

/*
 * Attach this instance, and then all the sub-devices
 */
void
espattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	extern vm_offset_t	SCSIBase;
	struct esp_softc *esc = (void *)self;
	struct ncr53c9x_softc *sc = &esc->sc_ncr53c9x;

	/*
	 * Set up the glue for MI code early; we use some of it here.
	 */
	sc->sc_glue = &esp_glue;

	/*
	 * Save the regs
	 */
	if (sc->sc_dev.dv_unit == 0) {
		unsigned long	reg_offset;

		esc->sc_reg = (volatile u_char *) SCSIBase;
		via2_register_irq(VIA2_SCSIIRQ,
					(void (*)(void *))ncr53c9x_intr, esc);
		esc->irq_mask = V2IF_SCSIIRQ;
		reg_offset = SCSIBase - IOBase;
		if (reg_offset == 0x10000) {
			sc->sc_freq = 16500000;
		} else {
			sc->sc_freq = 25000000;
		}
	} else {
		esc->sc_reg = (volatile u_char *) SCSIBase + 0x402;
		via2_register_irq(VIA2_SCSIDRQ,
					(void (*)(void *))ncr53c9x_intr, esc);
		esc->irq_mask = V2IF_SCSIDRQ; /* V2IF_T1? */
		sc->sc_freq = 25000000;
	}

	printf(": address %p", esc->sc_reg);

	sc->sc_id = 7;

	/* gimme MHz */
	sc->sc_freq /= 1000000;

	/*
	 * It is necessary to try to load the 2nd config register here,
	 * to find out what rev the esp chip is, else the esp_reset
	 * will not set up the defaults correctly.
	 */
	sc->sc_cfg1 = sc->sc_id | NCRCFG1_PARENB;
	sc->sc_cfg2 = NCRCFG2_SCSI2;
	sc->sc_cfg3 = 0;
	sc->sc_rev = NCR_VARIANT_NCR53C96;

	/*
	 * This is the value used to start sync negotiations
	 * Note that the NCR register "SYNCTP" is programmed
	 * in "clocks per byte", and has a minimum value of 4.
	 * The SCSI period used in negotiation is one-fourth
	 * of the time (in nanoseconds) needed to transfer one byte.
	 * Since the chip's clock is given in MHz, we have the following
	 * formula: 4 * period = (1000 / freq) * 4
	 */
	sc->sc_minsync = 1000 / sc->sc_freq;

	sc->sc_minsync = 0;	/* No synchronous xfers w/o DMA */
	/* Really no limit, but since we want to fit into the TCR... */
	sc->sc_maxxfer = 64 * 1024;

	/*
	 * Now try to attach all the sub-devices
	 */
	ncr53c9x_attach(sc, &esp_switch, &esp_dev);

	/*
	 * Configure interrupts.
	 */
	via2_reg(vPCR) = 0x22;
	via2_reg(vIFR) = esc->irq_mask;
	via2_reg(vIER) = 0x80 | esc->irq_mask;
}

/*
 * Glue functions.
 */

u_char
esp_read_reg(sc, reg)
	struct ncr53c9x_softc *sc;
	int reg;
{
	struct esp_softc *esc = (struct esp_softc *)sc;

	return esc->sc_reg[reg * 16];
}

void
esp_write_reg(sc, reg, val)
	struct ncr53c9x_softc *sc;
	int reg;
	u_char val;
{
	struct esp_softc *esc = (struct esp_softc *)sc;
	u_char v = val;

	if (reg == NCR_CMD && v == (NCRCMD_TRANS|NCRCMD_DMA)) {
		v = NCRCMD_TRANS;
	}
	esc->sc_reg[reg * 16] = v;
}

int
esp_dma_isintr(sc)
	struct ncr53c9x_softc *sc;
{
	struct esp_softc *esc = (struct esp_softc *)sc;

	return esc->sc_reg[NCR_STAT * 16] & 0x80;
}

void
esp_dma_reset(sc)
	struct ncr53c9x_softc *sc;
{
	struct esp_softc *esc = (struct esp_softc *)sc;

	esc->sc_active = 0;
	esc->sc_tc = 0;
}

int
esp_dma_intr(sc)
	struct ncr53c9x_softc *sc;
{
	register struct esp_softc *esc = (struct esp_softc *)sc;
	register u_char	*p;
	volatile u_char *cmdreg, *intrreg, *statreg, *fiforeg;
	register u_int	espphase, espstat, espintr;
	register int	cnt;

	if (esc->sc_active == 0) {
		printf("dma_intr--inactive DMA\n");
		return -1;
	}

	if ((sc->sc_espintr & NCRINTR_BS) == 0) {
		esc->sc_active = 0;
		return 0;
	}

	cnt = *esc->sc_pdmalen;
	if (*esc->sc_pdmalen == 0) {
		printf("data interrupt, but no count left.");
	}

	p = *esc->sc_dmaaddr;
	espphase = sc->sc_phase;
	espstat = (u_int) sc->sc_espstat;
	espintr = (u_int) sc->sc_espintr;
	cmdreg = esc->sc_reg + NCR_CMD * 16;
	fiforeg = esc->sc_reg + NCR_FIFO * 16;
	statreg = esc->sc_reg + NCR_STAT * 16;
	intrreg = esc->sc_reg + NCR_INTR * 16;
	do {
		if (esc->sc_datain) {
			*p++ = *fiforeg;
			cnt--;
			if (espphase == DATA_IN_PHASE) {
				*cmdreg = NCRCMD_TRANS;
			} else {
				esc->sc_active = 0;
			}
	 	} else {
			if (   (espphase == DATA_OUT_PHASE)
			    || (espphase == MESSAGE_OUT_PHASE)) {
				*fiforeg = *p++;
				cnt--;
				*cmdreg = NCRCMD_TRANS;
			} else {
				esc->sc_active = 0;
			}
		}

		if (esc->sc_active) {
			while (!(*statreg & 0x80));
			espstat = *statreg;
			espintr = *intrreg;
			espphase = (espintr & NCRINTR_DIS)
				    ? /* Disconnected */ BUSFREE_PHASE
				    : espstat & PHASE_MASK;
		}
	} while (esc->sc_active && (espintr & NCRINTR_BS));
	sc->sc_phase = espphase;
	sc->sc_espstat = (u_char) espstat;
	sc->sc_espintr = (u_char) espintr;
	*esc->sc_dmaaddr = p;
	*esc->sc_pdmalen = cnt;

	if (*esc->sc_pdmalen == 0) {
		esc->sc_tc = NCRSTAT_TC;
	}
	sc->sc_espstat |= esc->sc_tc;
	return 0;
}

int
esp_dma_setup(sc, addr, len, datain, dmasize)
	struct ncr53c9x_softc *sc;
	caddr_t *addr;
	size_t *len;
	int datain;
	size_t *dmasize;
{
	struct esp_softc *esc = (struct esp_softc *)sc;

	esc->sc_dmaaddr = addr;
	esc->sc_pdmalen = len;
	esc->sc_datain = datain;
	esc->sc_dmasize = *dmasize;
	esc->sc_tc = 0;

	return 0;
}

void
esp_dma_go(sc)
	struct ncr53c9x_softc *sc;
{
	struct esp_softc *esc = (struct esp_softc *)sc;

	if (esc->sc_datain == 0) {
		esc->sc_reg[NCR_FIFO * 16] = **esc->sc_dmaaddr;
		(*esc->sc_pdmalen)--;
		(*esc->sc_dmaaddr)++;
	}
	esc->sc_active = 1;
}

void
esp_dma_stop(sc)
	struct ncr53c9x_softc *sc;
{
}

int
esp_dma_isactive(sc)
	struct ncr53c9x_softc *sc;
{
	struct esp_softc *esc = (struct esp_softc *)sc;

	return esc->sc_active;
}
