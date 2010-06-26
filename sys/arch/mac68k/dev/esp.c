/*	$OpenBSD: esp.c,v 1.29 2010/06/26 23:24:43 guenther Exp $	*/
/*	$NetBSD: esp.c,v 1.17 1998/09/05 15:15:35 pk Exp $	*/

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
#include <sys/queue.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>
#include <scsi/scsi_message.h>

#include <machine/cpu.h>
#include <machine/bus.h>
#include <machine/param.h>

#include <dev/ic/ncr53c9xreg.h>
#include <dev/ic/ncr53c9xvar.h>

#include <machine/viareg.h>

#include <mac68k/dev/espvar.h>
#include <mac68k/dev/obiovar.h>

void	espattach(struct device *, struct device *, void *);
int	espmatch(struct device *, void *, void *);

/* Linkup to the rest of the kernel */
struct cfattach esp_ca = {
	sizeof(struct esp_softc), espmatch, espattach
};

struct scsi_adapter esp_switch = {
	ncr53c9x_scsi_cmd,
	scsi_minphys,		/* no max at this level; handled by DMA code */
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
void	esp_quick_write_reg(struct ncr53c9x_softc *, int, u_char);
int	esp_quick_dma_intr(struct ncr53c9x_softc *);
int	esp_quick_dma_setup(struct ncr53c9x_softc *, caddr_t *,
		size_t *, int, size_t *);
void	esp_quick_dma_go(struct ncr53c9x_softc *);
int	esp_intr(void *);

static __inline__ int esp_dafb_have_dreq(struct esp_softc *esc);
static __inline__ int esp_iosb_have_dreq(struct esp_softc *esc);
int (*esp_have_dreq) (struct esp_softc *esc);

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

	if ((cf->cf_unit == 0) && mac68k_machine.scsi96) {
		return 1;
	}
	if ((cf->cf_unit == 1) && mac68k_machine.scsi96_2) {
		return 1;
	}
	return 0;
}

/*
 * Attach this instance, and then all the sub-devices
 */
void
espattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct obio_attach_args *oa = (struct obio_attach_args *)aux;
	extern vaddr_t		SCSIBase;
	struct esp_softc	*esc = (void *)self;
	struct ncr53c9x_softc	*sc = &esc->sc_ncr53c9x;
	int			quick = 0;
	unsigned long		reg_offset;

	reg_offset = SCSIBase - IOBase;
	esc->sc_tag = oa->oa_tag;

	/*
	 * For Wombat, Primus and Optimus motherboards, DREQ is
	 * visible on bit 0 of the IOSB's emulated VIA2 vIFR (and
	 * the SCSI registers are offset 0x1000 bytes from IOBase).
	 *
	 * For the Q700/900/950 it's at f9800024 for bus 0 and
	 * f9800028 for bus 1 (900/950).  For these machines, that is also
	 * a (12-bit) configuration register for DAFB's control of the
	 * pseudo-DMA timing.  The default value is 0x1d1.
	 */
	esp_have_dreq = esp_dafb_have_dreq;
	if (sc->sc_dev.dv_unit == 0) {
		if (reg_offset == 0x10000) {
			quick = 1;
			esp_have_dreq = esp_iosb_have_dreq;
		} else if (reg_offset == 0x18000) {
			quick = 0;
		} else {
			if (bus_space_map(esc->sc_tag, 0xf9800024,
					4, 0, &esc->sc_bsh)) {
				printf("failed to map 4 at 0xf9800024.\n");
			} else {
				quick = 1;
				bus_space_write_4(esc->sc_tag,
					esc->sc_bsh, 0, 0x1d1);
			}
		}
	} else {
		if (bus_space_map(esc->sc_tag, 0xf9800028, 4, 0,
				&esc->sc_bsh)) {
			printf("failed to map 4 at 0xf9800028.\n");
		} else {
			quick = 1;
			bus_space_write_4(esc->sc_tag, esc->sc_bsh, 0, 0x1d1);
		}
	}
	if (quick) {
		esp_glue.gl_write_reg = esp_quick_write_reg;
		esp_glue.gl_dma_intr = esp_quick_dma_intr;
		esp_glue.gl_dma_setup = esp_quick_dma_setup;
		esp_glue.gl_dma_go = esp_quick_dma_go;
	}

	/*
	 * Set up the glue for MI code early; we use some of it here.
	 */
	sc->sc_glue = &esp_glue;

	esc->sc_ih.vh_fn = esp_intr;
	esc->sc_ih.vh_arg = esc;
	esc->sc_ih.vh_ipl = VIA2_SCSIIRQ;

	/*
	 * Save the regs
	 */
	if (sc->sc_dev.dv_unit == 0) {
		esc->sc_reg = (volatile u_char *) SCSIBase;
		via2_register_irq(&esc->sc_ih, self->dv_xname);
		esc->irq_mask = V2IF_SCSIIRQ;
		if (reg_offset == 0x10000) {
			/* From the Q650 developer's note */
			sc->sc_freq = 16500000;
		} else {
			sc->sc_freq = 25000000;
		}
	} else {
		esc->sc_reg = (volatile u_char *) SCSIBase + 0x402;
		via2_register_irq(&esc->sc_ih, self->dv_xname);
		esc->irq_mask = 0;
		sc->sc_freq = 25000000;
	}

	if (quick) {
		printf(" (pseudo-DMA)");
	}

#ifdef DEBUG
	printf(" address %p", esc->sc_reg);
#endif

	sc->sc_id = 7;

	/*
	 * It is necessary to try to load the 2nd config register here,
	 * to find out what rev the esp chip is, else the esp_reset
	 * will not set up the defaults correctly.
	 */
	sc->sc_cfg1 = sc->sc_id; /* | NCRCFG1_PARENB; */
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
	sc->sc_minsync = (1000 * 1000000) / sc->sc_freq;

	/* We need this to fit into the TCR... */
	sc->sc_maxxfer = 64 * 1024;

	if (quick == 0) {
		sc->sc_minsync = 0;	/* No synchronous xfers w/o DMA */
		sc->sc_maxxfer = 8 * 1024;
	}

	/* gimme MHz */
	sc->sc_freq /= 1000000;

	/*
	 * Configure interrupts.
	 */
	if (esc->irq_mask) {
		via2_reg(vPCR) = 0x22;
		via2_reg(vIFR) = esc->irq_mask;
		via2_reg(vIER) = 0x80 | esc->irq_mask;
	}

	/*
	 * Now try to attach all the sub-devices
	 */
	ncr53c9x_attach(sc, &esp_switch, &esp_dev);
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
	struct esp_softc *esc = (struct esp_softc *)sc;
	volatile u_char *cmdreg, *intrreg, *statreg, *fiforeg;
	u_char	*p;
	u_int	espphase, espstat, espintr;
	int	cnt;

	if (esc->sc_active == 0) {
		printf("dma_intr--inactive DMA\n");
		return -1;
	}

	if ((sc->sc_espintr & NCRINTR_BS) == 0) {
		esc->sc_active = 0;
		return 0;
	}

	cnt = *esc->sc_dmalen;
	if (*esc->sc_dmalen == 0) {
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
	*esc->sc_dmalen = cnt;

	if (*esc->sc_dmalen == 0) {
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
	esc->sc_dmalen = len;
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
		(*esc->sc_dmalen)--;
		(*esc->sc_dmaaddr)++;
	}
	esc->sc_active = 1;
}

void
esp_quick_write_reg(sc, reg, val)
	struct ncr53c9x_softc *sc;
	int reg;
	u_char val;
{
	struct esp_softc *esc = (struct esp_softc *)sc;
	u_char v = val;

	esc->sc_reg[reg * 16] = v;
}

int
esp_quick_dma_intr(sc)
	struct ncr53c9x_softc *sc;
{
	struct esp_softc *esc = (struct esp_softc *)sc;
	int trans=0, resid=0;

	if (esc->sc_active == 0)
		panic("dma_intr--inactive DMA");

	esc->sc_active = 0;

	if (esc->sc_dmasize == 0) {
		int	res;

		res = 65536;
		res -= NCR_READ_REG(sc, NCR_TCL);
		res -= NCR_READ_REG(sc, NCR_TCM) << 8;
		printf("dmaintr: discarded %d b (last transfer was %d b).\n",
			res, esc->sc_prevdmasize);
		return 0;
	}

	if (esc->sc_datain &&
	    (resid = (NCR_READ_REG(sc, NCR_FFLAG) & NCRFIFO_FF)) != 0) {
		printf("dmaintr: empty FIFO of %d\n", resid);
		DELAY(1);
	}

	if ((sc->sc_espstat & NCRSTAT_TC) == 0) {
		resid += NCR_READ_REG(sc, NCR_TCL);
		resid += NCR_READ_REG(sc, NCR_TCM) << 8;

		if (resid == 0)
			resid = 65536;
	}
	
	trans = esc->sc_dmasize - resid;
	if (trans < 0) {
		printf("dmaintr: trans < 0????");
		trans = esc->sc_dmasize;
	}

	NCR_DMA(("dmaintr: trans %d, resid %d.\n", trans, resid));
	*esc->sc_dmaaddr += trans;
	*esc->sc_dmalen -= trans;

	return 0;
}

int
esp_quick_dma_setup(sc, addr, len, datain, dmasize)
	struct ncr53c9x_softc *sc;
	caddr_t *addr;
	size_t *len;
	int datain;
	size_t *dmasize;
{
	struct esp_softc *esc = (struct esp_softc *)sc;

	esc->sc_dmaaddr = addr;
	esc->sc_dmalen = len;

	esc->sc_pdmaddr = (u_int16_t *) *addr;
	esc->sc_pdmalen = *len;

	if (esc->sc_pdmalen & 1) {
		esc->sc_pdmalen--;
		esc->sc_pad = 1;
	} else {
		esc->sc_pad = 0;
	}

	esc->sc_datain = datain;
	esc->sc_prevdmasize = esc->sc_dmasize;
	esc->sc_dmasize = *dmasize;

	return 0;
}

static __inline__ int
esp_dafb_have_dreq(esc)
	struct esp_softc *esc;
{
	return (*(volatile u_int32_t *)
	    bus_space_vaddr(esc->sc_tag, esc->sc_bsh) & 0x200);
}

static __inline__ int
esp_iosb_have_dreq(esc)
	struct esp_softc *esc;
{
	return (via2_reg(vIFR) & V2IF_SCSIDRQ);
}

/* Faster spl constructs, without saving old values */
#define	__splx(s)	__asm __volatile ("movew %0,sr" : : "di" (s));
#define	__splvm()	__splx(mac68k_vmipl)
#define	__splbio()	__splx(PSL_S | PSL_IPL2)

void
esp_quick_dma_go(sc)
	struct ncr53c9x_softc *sc;
{
	struct esp_softc *esc = (struct esp_softc *)sc;
	extern int *nofault;
	label_t faultbuf;
	u_int16_t volatile *pdma;
	u_char volatile *statreg;
	int espspl;

	esc->sc_active = 1;

	espspl = splbio();

restart_dmago:
	nofault = (int *) &faultbuf;
	if (setjmp((label_t *) nofault)) {
		int	i=0;

		nofault = (int *) 0;
		statreg = esc->sc_reg + NCR_STAT * 16;
		for (;;) {
			if (*statreg & 0x80) {
				goto gotintr;
			}

			if (esp_have_dreq(esc)) {
				break;
			}

			DELAY(1);
			if (i++ > 10000)
				panic("esp_dma_go: Argh!");
		}
		goto restart_dmago;
	}

	statreg = esc->sc_reg + NCR_STAT * 16;
	pdma = (u_int16_t *) (esc->sc_reg + 0x100);

#define WAIT while (!esp_have_dreq(esc)) if (*statreg & 0x80) goto gotintr

	if (esc->sc_datain == 0) {
		while (esc->sc_pdmalen) {
			WAIT;
			__splvm(); *pdma = *(esc->sc_pdmaddr)++; __splbio();
			esc->sc_pdmalen -= 2;
		}
		if (esc->sc_pad) {
			unsigned short  us;
			unsigned char   *c;
			c = (unsigned char *) esc->sc_pdmaddr;
			us = *c;
			WAIT;
			__splvm(); *pdma = us; __splbio();
		}
	} else {
		while (esc->sc_pdmalen) {
			WAIT;
			__splvm(); *(esc->sc_pdmaddr)++ = *pdma; __splbio();
			esc->sc_pdmalen -= 2;
		}
		if (esc->sc_pad) {
			unsigned short  us;
			unsigned char   *c;
			WAIT;
			__splvm(); us = *pdma; __splbio();
			c = (unsigned char *) esc->sc_pdmaddr;
			*c = us & 0xff;
		}
	}
#undef WAIT
	nofault = (int *) 0;
	
	if ((*statreg & 0x80) == 0) {
		splx(espspl);
		return;
	}

gotintr:
	ncr53c9x_intr(sc);
	splx(espspl);
}

int
esp_intr(void *v)
{
	struct esp_softc *esc = (struct esp_softc *)v;

	if (esc->sc_reg[NCR_STAT * 16] & NCRSTAT_INT)
		return (ncr53c9x_intr(v));

	return (0);
}
