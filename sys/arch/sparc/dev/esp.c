/*	$OpenBSD: esp.c,v 1.36 2015/03/29 15:41:15 miod Exp $	*/
/*	$NetBSD: esp.c,v 1.69 1997/08/27 11:24:18 bouyer Exp $	*/

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
 * Copyright (c) 1996 Charles M. Hannum.  All rights reserved.
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
 *	This product includes software developed by Charles M. Hannum.
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
#include <sys/malloc.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>
#include <scsi/scsi_message.h>

#include <uvm/uvm_extern.h>

#include <machine/autoconf.h>
#include <machine/cpu.h>

#include <dev/ic/ncr53c9xreg.h>
#include <dev/ic/ncr53c9xvar.h>

#include <sparc/dev/sbusvar.h>
#include <sparc/dev/dmareg.h>
#include <sparc/dev/dmavar.h>
#include <sparc/dev/espvar.h>

void	espattach(struct device *, struct device *, void *);
int	espmatch(struct device *, void *, void *);

/* Linkup to the rest of the kernel */
struct cfattach esp_ca = {
	sizeof(struct esp_softc), espmatch, espattach
};

/*
 * Functions and the switch for the MI code.
 */
u_char	esp_read_reg(struct ncr53c9x_softc *, int);
void	esp_write_reg(struct ncr53c9x_softc *, int, u_char);
u_char	esp_rdreg1(struct ncr53c9x_softc *, int);
void	esp_wrreg1(struct ncr53c9x_softc *, int, u_char);
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

#if defined(SUN4C) || defined(SUN4D) || defined(SUN4E) || defined(SUN4M)
struct ncr53c9x_glue esp_glue1 = {
	esp_rdreg1,
	esp_wrreg1,
	esp_dma_isintr,
	esp_dma_reset,
	esp_dma_intr,
	esp_dma_setup,
	esp_dma_go,
	esp_dma_stop,
	esp_dma_isactive,
	NULL,			/* gl_clear_latched_intr */
};
#endif

int
espmatch(parent, vcf, aux)
	struct device *parent;
	void *vcf, *aux;
{
	struct cfdata *cf = vcf;
	struct confargs *ca = aux;
	struct romaux *ra = &ca->ca_ra;

#if defined(SUN4C) || defined(SUN4D) || defined(SUN4E) || defined(SUN4M)
	if (ca->ca_bustype == BUS_SBUS) {
		if (!sbus_testdma((struct sbus_softc *)parent, ca))
			return (0);

		if (strcmp("SUNW,fas", ra->ra_name) == 0 ||
		    strcmp("ptscII", ra->ra_name) == 0)
			return (1);
	}
#endif

	if (strcmp(cf->cf_driver->cd_name, ra->ra_name))
		return (0);

#if defined(SUN4C) || defined(SUN4D) || defined(SUN4E) || defined(SUN4M)
	if (ca->ca_bustype == BUS_SBUS)
		return (1);
#endif
#ifdef SUN4
	if (cpuinfo.cpu_type == CPUTYP_4_100)
		return (0);
#endif
	ra->ra_len = NBPG;
	return (probeget(ra->ra_vaddr, 1) != -1);
}

/*
 * Attach this instance, and then all the sub-devices
 */
void
espattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	register struct confargs *ca = aux;
	struct esp_softc *esc = (void *)self;
	struct ncr53c9x_softc *sc = &esc->sc_ncr53c9x;
	struct bootpath *bp;
	int dmachild;
	unsigned int uid = 0;

	/*
	 * If no interrupts properties, bail out: this might happen
	 * on the Sparc X terminal.
	 */
	if (ca->ca_ra.ra_nintr != 1) {
		printf(": expected 1 interrupt, got %d\n", ca->ca_ra.ra_nintr);
		return;
	}

	esc->sc_pri = ca->ca_ra.ra_intr[0].int_pri;
	printf(" pri %d", esc->sc_pri);

	/* Other settings */
	esc->sc_node = ca->ca_ra.ra_node;
	if (ca->ca_bustype == BUS_SBUS) {
		sc->sc_id = getpropint(esc->sc_node, "initiator-id", 7);
		sc->sc_freq = getpropint(esc->sc_node, "clock-frequency", -1);
	} else {
		sc->sc_id = 7;
		sc->sc_freq = 24000000;
	}
	if (sc->sc_freq < 0)
		sc->sc_freq = ((struct sbus_softc *)
		    sc->sc_dev.dv_parent)->sc_clockfreq;

	/* gimme MHz */
	sc->sc_freq /= 1000000;

#if defined(SUN4C) || defined(SUN4D) || defined(SUN4E) || defined(SUN4M)
	if (ca->ca_bustype == BUS_SBUS &&
	    strcmp("SUNW,fas", ca->ca_ra.ra_name) == 0) {
		struct dma_softc *dsc;

		/*
		 * fas has 2 register spaces: DMA (lsi64854) and SCSI core
		 * (ncr53c9x).
		 */
		if (ca->ca_ra.ra_nreg != 2) {
			printf(": expected 2 register spaces, found %d\n",
			    ca->ca_ra.ra_nreg);
			return;
		}

		/*
		 * Allocate a softc for the DMA companion, which will not
		 * get a regular attachment.
		 */
		dsc = malloc(sizeof(*dsc), M_DEVBUF, M_NOWAIT | M_ZERO);
		if (dsc == NULL) {
			printf(": could not allocate dma softc\n");
			return;
		}
		strlcpy(dsc->sc_dev.dv_xname, sc->sc_dev.dv_xname,
		    sizeof(dsc->sc_dev.dv_xname));
		esc->sc_dma = dsc;

		/*
		 * Map DMA registers
		 */
		dsc->sc_regs = (struct dma_regs *)mapiodev(&ca->ca_ra.ra_reg[0],
		    0, ca->ca_ra.ra_reg[0].rr_len);
		if (dsc->sc_regs == NULL) {
			printf(": can't map DMA registers\n");
			return;
		}
		dsc->sc_rev = dsc->sc_regs->csr & D_DEV_ID;
		dsc->sc_esp = esc;

#ifdef SUN4M
		/*
		 * Get transfer burst size from PROM and plug it into the
		 * controller registers. This is needed on the Sun4m; do
		 * others need it too?
		 */
		if (CPU_ISSUN4M) {
			int sbusburst;

			sbusburst = ((struct sbus_softc *)parent)->sc_burst;
			if (sbusburst == 0)
				sbusburst = SBUS_BURST_32 - 1;	/* 1 -> 16 */

			dsc->sc_burst =
			    getpropint(ca->ca_ra.ra_node, "burst-sizes", -1);
			if (dsc->sc_burst == -1)
				/* take SBus burst sizes */
				dsc->sc_burst = sbusburst;

			/* Clamp at parent's burst sizes */
			dsc->sc_burst &= sbusburst;
		}
#endif	/* SUN4M */

		/* indirect functions */
		dma_setuphandlers(dsc);

		/*
		 * Map SCSI core registers
		 */
		esc->sc_reg = (volatile u_char *)mapiodev(&ca->ca_ra.ra_reg[1],
		    0, ca->ca_ra.ra_reg[1].rr_len);
		if (esc->sc_reg == NULL) {
			printf(": can't map SCSI core registers\n");
			return;
		}

		dmachild = 0;
	} else
#endif
	{
		/*
		 * Map my registers in, if they aren't already in virtual
		 * address space.
		 */
		if (ca->ca_ra.ra_vaddr)
			esc->sc_reg = (volatile u_char *) ca->ca_ra.ra_vaddr;
		else {
			esc->sc_reg = (volatile u_char *)
			    mapiodev(ca->ca_ra.ra_reg, 0, ca->ca_ra.ra_len);
		}

		dmachild = strcmp(parent->dv_cfdata->cf_driver->cd_name, "dma") == 0;
		if (dmachild) {
			esc->sc_dma = (struct dma_softc *)parent;
			esc->sc_dma->sc_esp = esc;
		} else {
			/*
			 * Find the DMA by poking around the DMA device
			 * structures.
			 *
			 * What happens here is that if the DMA driver has
			 * not been configured, then this returns a NULL
			 * pointer. Then when the DMA actually gets configured,
			 * it does the opposite test, and if the sc->sc_esp
			 * field in its softc is NULL, then tries to find the
			 * matching esp driver.
			 */
			esc->sc_dma = (struct dma_softc *)
			    getdevunit("dma", sc->sc_dev.dv_unit);

			/*
			 * ...and a back pointer to us, for DMA.
			 */
			if (esc->sc_dma)
				esc->sc_dma->sc_esp = esc;
			else {
				printf("\n");
				panic("espattach: no dma found");
			}
		}
	}

	/*
	 * Set up glue for MI code.
	 */
#if defined(SUN4C) || defined(SUN4D) || defined(SUN4E) || defined(SUN4M)
	if (ca->ca_bustype == BUS_SBUS &&
	    strcmp("ptscII", ca->ca_ra.ra_name) == 0) {
		sc->sc_glue = &esp_glue1;
	} else
#endif
		sc->sc_glue = &esp_glue;

	/*
	 * XXX More of this should be in ncr53c9x_attach(), but
	 * XXX should we really poke around the chip that much in
	 * XXX the MI code?  Think about this more...
	 */

	/*
	 * It is necessary to try to load the 2nd config register here,
	 * to find out what rev the esp chip is, else the ncr53c9x_reset
	 * will not set up the defaults correctly.
	 */
	sc->sc_cfg1 = sc->sc_id | NCRCFG1_PARENB;
	sc->sc_cfg2 = NCRCFG2_SCSI2 | NCRCFG2_RPE;
	sc->sc_cfg3 = NCRCFG3_CDB;
	NCR_WRITE_REG(sc, NCR_CFG2, sc->sc_cfg2);

	if ((NCR_READ_REG(sc, NCR_CFG2) & ~NCRCFG2_RSVD) !=
	    (NCRCFG2_SCSI2 | NCRCFG2_RPE)) {
		sc->sc_rev = NCR_VARIANT_ESP100;
	} else {
		sc->sc_cfg2 = NCRCFG2_SCSI2;
		NCR_WRITE_REG(sc, NCR_CFG2, sc->sc_cfg2);
		sc->sc_cfg3 = 0;
		NCR_WRITE_REG(sc, NCR_CFG3, sc->sc_cfg3);
		sc->sc_cfg3 = (NCRCFG3_CDB | NCRCFG3_FCLK);
		NCR_WRITE_REG(sc, NCR_CFG3, sc->sc_cfg3);
		if (NCR_READ_REG(sc, NCR_CFG3) !=
		    (NCRCFG3_CDB | NCRCFG3_FCLK)) {
			sc->sc_rev = NCR_VARIANT_ESP100A;
		} else {
			/* NCRCFG2_FE enables > 64K transfers */
			sc->sc_cfg2 |= NCRCFG2_FE;
			sc->sc_cfg3 = 0;
			NCR_WRITE_REG(sc, NCR_CFG3, sc->sc_cfg3);
			sc->sc_rev = NCR_VARIANT_ESP200;

			/* XXX spec says it's valid after power up or chip reset */
			uid = NCR_READ_REG(sc, NCR_UID);
			if (((uid & 0xf8) >> 3) == 0x0a) /* XXX */
				sc->sc_rev = NCR_VARIANT_FAS366;
		}
	}

	/*
	 * XXX minsync and maxxfer _should_ be set up in MI code,
	 * XXX but it appears to have some dependency on what sort
	 * XXX of DMA we're hooked up to, etc.
	 */

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

	/*
	 * Alas, we must now modify the value a bit, because it's
	 * only valid when can switch on FASTCLK and FASTSCSI bits  
	 * in config register 3... 
	 */
	switch (sc->sc_rev) {
	case NCR_VARIANT_ESP100:
		sc->sc_maxxfer = 64 * 1024;
		sc->sc_minsync = 0;	/* No synch on old chip? */
		break;

	case NCR_VARIANT_ESP100A:
		sc->sc_maxxfer = 64 * 1024;
		/* Min clocks/byte is 5 */
		sc->sc_minsync = ncr53c9x_cpb2stp(sc, 5);
		break;

	case NCR_VARIANT_ESP200:
	case NCR_VARIANT_FAS366:
		sc->sc_maxxfer = 16 * 1024 * 1024;
		/* XXX - do actually set FAST* bits */
		break;
	}

	/* and the interrupts */
	esc->sc_ih.ih_fun = (void *) ncr53c9x_intr;
	esc->sc_ih.ih_arg = sc;
	intr_establish(esc->sc_pri, &esc->sc_ih, IPL_BIO, self->dv_xname);

	/*
	 * If the boot path is "esp" at the moment and it's me, then
	 * walk our pointer to the sub-device, ready for the config
	 * below.
	 */
	bp = ca->ca_ra.ra_bp;
	if (bp != NULL && (strcmp(bp->name, "esp") == 0 ||
	    strcmp(bp->name, ca->ca_ra.ra_name) == 0)) {
		switch (ca->ca_bustype) {
		case BUS_SBUS:
			if (SAME_ESP(sc, bp, ca))
				bootpath_store(1, bp + 1);
			break;
		default:
			if (bp->val[0] == -1 &&
			    bp->val[1] == sc->sc_dev.dv_unit)
				bootpath_store(1, bp + 1);
			break;
		}
	}

	/* Turn on target selection using the `dma' method */
	if (sc->sc_rev != NCR_VARIANT_FAS366)
		sc->sc_features |= NCR_F_DMASELECT;

	/* Do the common parts of attachment. */
	ncr53c9x_attach(sc);

	bootpath_store(1, NULL);
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

	return (esc->sc_reg[reg * 4]);
}

void
esp_write_reg(sc, reg, val)
	struct ncr53c9x_softc *sc;
	int reg;
	u_char val;
{
	struct esp_softc *esc = (struct esp_softc *)sc;
	u_char v = val;

	esc->sc_reg[reg * 4] = v;
}

#if defined(SUN4C) || defined(SUN4D) || defined(SUN4E) || defined(SUN4M)
u_char
esp_rdreg1(sc, reg)
	struct ncr53c9x_softc *sc;
	int reg;
{
	struct esp_softc *esc = (struct esp_softc *)sc;

	return (esc->sc_reg[reg]);
}

void
esp_wrreg1(sc, reg, val)
	struct ncr53c9x_softc *sc;
	int reg;
	u_char val;
{
	struct esp_softc *esc = (struct esp_softc *)sc;
	u_char v = val;

	esc->sc_reg[reg] = v;
}
#endif

int
esp_dma_isintr(sc)
	struct ncr53c9x_softc *sc;
{
	struct esp_softc *esc = (struct esp_softc *)sc;

	return (DMA_ISINTR(esc->sc_dma));
}

void
esp_dma_reset(sc)
	struct ncr53c9x_softc *sc;
{
	struct esp_softc *esc = (struct esp_softc *)sc;

	DMA_RESET(esc->sc_dma);
}

int
esp_dma_intr(sc)
	struct ncr53c9x_softc *sc;
{
	struct esp_softc *esc = (struct esp_softc *)sc;

	return (DMA_INTR(esc->sc_dma));
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

	return (DMA_SETUP(esc->sc_dma, addr, len, datain, dmasize));
}

void
esp_dma_go(sc)
	struct ncr53c9x_softc *sc;
{
	struct esp_softc *esc = (struct esp_softc *)sc;

	DMA_GO(esc->sc_dma);
}

void
esp_dma_stop(sc)
	struct ncr53c9x_softc *sc;
{
	struct esp_softc *esc = (struct esp_softc *)sc;

	DMACSR(esc->sc_dma) &= ~D_EN_DMA;
}

int
esp_dma_isactive(sc)
	struct ncr53c9x_softc *sc;
{
	struct esp_softc *esc = (struct esp_softc *)sc;

	return (DMA_ISACTIVE(esc->sc_dma));
}
