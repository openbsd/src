/* $OpenBSD: esp.c,v 1.6 2010/06/26 23:24:43 guenther Exp $ */

/*-
 * Copyright (c) 1997, 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum and by Jason R. Thorpe of the Numerical Aerospace
 * Simulation Facility, NASA Ames Research Center.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
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

#include <sys/cdefs.h>

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

#include <uvm/uvm_extern.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>
#include <scsi/scsi_message.h>

#include <dev/ofw/openfirm.h>

#include <machine/cpu.h>
#include <machine/autoconf.h>
#include <machine/pio.h>

#include <dev/ic/ncr53c9xreg.h>
#include <dev/ic/ncr53c9xvar.h>

#include <macppc/dev/dbdma.h>

struct esp_softc {
	struct ncr53c9x_softc sc_ncr53c9x;	/* glue to MI code */

	struct intrhand sc_ih;			/* intr handler */

	volatile u_char *sc_reg;		/* the registers */

	bus_dmamap_t sc_dmamap;
	bus_dma_tag_t sc_dmat;
	dbdma_t sc_dbdma;
	dbdma_regmap_t *sc_dmareg;		/* DMA registers */
	dbdma_command_t *sc_dmacmd;		/* command area for DMA */

	int sc_node;				/* node ID */
	int sc_intr;

	size_t	sc_dmasize;
	caddr_t *sc_dmaaddr;
	size_t  *sc_dmalen;
	int sc_dmaactive;
	int sc_dma_direction;
};

#define D_WRITE 1
#define ESP_DMALIST_MAX	20

void espattach(struct device *, struct device *, void *);
int espmatch(struct device *, void *, void *);

/* Linkup to the rest of the kernel */
struct cfattach esp_ca = {
	sizeof(struct esp_softc), espmatch, espattach
};

struct scsi_adapter esp_switch = {
	/* no max at this level; handled by DMA code */
	ncr53c9x_scsi_cmd, scsi_minphys, NULL, NULL,
};

struct scsi_device esp_dev = {
	NULL, NULL, NULL, NULL,
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

static int espdmaintr(struct esp_softc *);
static void esp_shutdownhook(void *);

int
espmatch(struct device *parent, void *cf, void *aux)
{
	struct confargs *ca = aux;

	if (strcmp(ca->ca_name, "53c94") != 0)
		return 0;

	if (ca->ca_nreg != 16)
		return 0;
	if (ca->ca_nintr != 8)
		return 0;

	return 1;
}

/*
 * Attach this instance, and then all the sub-devices
 */
void
espattach(struct device *parent, struct device *self, void *aux)
{
	struct confargs *ca = aux;
	struct esp_softc *esc = (void *)self;
	struct ncr53c9x_softc *sc = &esc->sc_ncr53c9x;
	u_int *reg;
	int sz, error;

	/*
	 * Set up glue for MI code early; we use some of it here.
	 */
	sc->sc_glue = &esp_glue;

	esc->sc_node = ca->ca_node;
	esc->sc_intr = ca->ca_intr[0];
	printf(" irq %d", esc->sc_intr);

	/*
	 * Map my registers in.
	 */
	reg = ca->ca_reg;
	esc->sc_reg = mapiodev(ca->ca_baseaddr + reg[0], reg[1]);
	esc->sc_dmareg = mapiodev(ca->ca_baseaddr + reg[2], reg[3]);

	esc->sc_dmat = ca->ca_dmat;
	if ((error = bus_dmamap_create(esc->sc_dmat,
	    ESP_DMALIST_MAX * DBDMA_COUNT_MAX, ESP_DMALIST_MAX,
	    DBDMA_COUNT_MAX, NBPG, BUS_DMA_NOWAIT, &esc->sc_dmamap)) != 0) {
		printf(": cannot create dma map, error = %d\n", error);
		return;
	}

	/* Allocate 16-byte aligned DMA command space */
	esc->sc_dbdma = dbdma_alloc(esc->sc_dmat, ESP_DMALIST_MAX);
	esc->sc_dmacmd = esc->sc_dbdma->d_addr;

	/* Other settings */
	sc->sc_id = 7;
	sz = OF_getprop(ca->ca_node, "clock-frequency",
		&sc->sc_freq, sizeof(int));
	if (sz != sizeof(int))
		sc->sc_freq = 25000000;

	/* gimme MHz */
	sc->sc_freq /= 1000000;

	/*
	 * Set up static configuration info.
	 */
	sc->sc_cfg1 = sc->sc_id | NCRCFG1_PARENB;
	sc->sc_cfg2 = NCRCFG2_SCSI2; /* | NCRCFG2_FE */
	sc->sc_cfg3 = NCRCFG3_CDB;
	sc->sc_rev = NCR_VARIANT_NCR53C94;

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

	sc->sc_maxxfer = 64 * 1024;

	/* and the interuppts */
	mac_intr_establish(parent, esc->sc_intr, IST_LEVEL, IPL_BIO,
	    ncr53c9x_intr, sc, sc->sc_dev.dv_xname);

	/* Reset SCSI bus when halt. */
	shutdownhook_establish(esp_shutdownhook, sc);

	/* Turn on target selection using the `DMA' method */
	sc->sc_features |= NCR_F_DMASELECT;

	ncr53c9x_attach(sc, &esp_switch, &esp_dev);

}

/*
 * Glue functions.
 */

u_char
esp_read_reg(struct ncr53c9x_softc *sc, int reg)
{
	struct esp_softc *esc = (struct esp_softc *)sc;

	return in8(&esc->sc_reg[reg * 16]);
}

void
esp_write_reg(struct ncr53c9x_softc *sc, int reg, u_char val)
{
	struct esp_softc *esc = (struct esp_softc *)sc;
	u_char v = val;

	out8(&esc->sc_reg[reg * 16], v);
}

int
esp_dma_isintr(struct ncr53c9x_softc *sc)
{
	return esp_read_reg(sc, NCR_STAT) & NCRSTAT_INT;
}

void
esp_dma_reset(struct ncr53c9x_softc *sc)
{
	struct esp_softc *esc = (struct esp_softc *)sc;

	dbdma_stop(esc->sc_dmareg);
	esc->sc_dmaactive = 0;
}

int
esp_dma_intr(struct ncr53c9x_softc *sc)
{
	struct esp_softc *esc = (struct esp_softc *)sc;

	return (espdmaintr(esc));
}

int
esp_dma_setup(struct ncr53c9x_softc *sc, caddr_t *addr, size_t *len,
     int datain, size_t *dmasize)
{
	struct esp_softc *esc = (struct esp_softc *)sc;
	dbdma_command_t *cmdp;
	u_int cmd;
	int i, error;

	cmdp = esc->sc_dmacmd;
	cmd = datain ? DBDMA_CMD_IN_MORE : DBDMA_CMD_OUT_MORE;

	esc->sc_dmaaddr = addr;
	esc->sc_dmalen = len;
	esc->sc_dmasize = *dmasize;

	if ((error = bus_dmamap_load(esc->sc_dmat, esc->sc_dmamap, *addr,
	    *dmasize, NULL, BUS_DMA_NOWAIT)) != 0)
		return (error);

	for (i = 0; i < esc->sc_dmamap->dm_nsegs; i++, cmdp++) {
		if (i + 1 == esc->sc_dmamap->dm_nsegs)
			cmd = datain ? DBDMA_CMD_IN_LAST : DBDMA_CMD_OUT_LAST;
		DBDMA_BUILD(cmdp, cmd, 0, esc->sc_dmamap->dm_segs[i].ds_len,
		    esc->sc_dmamap->dm_segs[i].ds_addr, DBDMA_INT_NEVER,
		    DBDMA_WAIT_NEVER, DBDMA_BRANCH_NEVER);
	}
	DBDMA_BUILD(cmdp, DBDMA_CMD_STOP, 0, 0, 0,
	    DBDMA_INT_NEVER, DBDMA_WAIT_NEVER, DBDMA_BRANCH_NEVER);

	esc->sc_dma_direction = datain ? D_WRITE : 0;

	return 0;
}

void
esp_dma_go(struct ncr53c9x_softc *sc)
{
	struct esp_softc *esc = (struct esp_softc *)sc;

	dbdma_start(esc->sc_dmareg, esc->sc_dbdma);
	esc->sc_dmaactive = 1;
}

void
esp_dma_stop(struct ncr53c9x_softc *sc)
{
	struct esp_softc *esc = (struct esp_softc *)sc;

	dbdma_stop(esc->sc_dmareg);
	bus_dmamap_unload(esc->sc_dmat, esc->sc_dmamap);
	esc->sc_dmaactive = 0;
}

int
esp_dma_isactive(struct ncr53c9x_softc *sc)
{
	struct esp_softc *esc = (struct esp_softc *)sc;

	return (esc->sc_dmaactive);
}


/*
 * Pseudo (chained) interrupt from the esp driver to kick the
 * current running DMA transfer. I am replying on espintr() to
 * pickup and clean errors for now
 *
 * return 1 if it was a DMA continue.
 */
int
espdmaintr(struct esp_softc *sc)
{
	struct ncr53c9x_softc *nsc = (struct ncr53c9x_softc *)sc;
	int trans, resid;
	u_long csr = sc->sc_dma_direction;

	/* This is an "assertion" :) */
	if (sc->sc_dmaactive == 0)
		panic("dmaintr: DMA wasn't active");

	/* DMA has stopped */
	dbdma_stop(sc->sc_dmareg);
	bus_dmamap_unload(sc->sc_dmat, sc->sc_dmamap);
	sc->sc_dmaactive = 0;

	if (sc->sc_dmasize == 0) {
		/* A "Transfer Pad" operation completed */
		NCR_DMA(("dmaintr: discarded %d bytes (tcl=%d, tcm=%d)\n",
		    NCR_READ_REG(nsc, NCR_TCL) |
		    (NCR_READ_REG(nsc, NCR_TCM) << 8),
		    NCR_READ_REG(nsc, NCR_TCL),
		    NCR_READ_REG(nsc, NCR_TCM)));
		return 0;
	}

	resid = 0;
	/*
	 * If a transfer onto the SCSI bus gets interrupted by the device
	 * (e.g. for a SAVEPOINTER message), the data in the FIFO counts
	 * as residual since the ESP counter registers get decremented as
	 * bytes are clocked into the FIFO.
	 */
	if (!(csr & D_WRITE) &&
	    (resid = (NCR_READ_REG(nsc, NCR_FFLAG) & NCRFIFO_FF)) != 0) {
		NCR_DMA(("dmaintr: empty esp FIFO of %d ", resid));
	}

	if ((nsc->sc_espstat & NCRSTAT_TC) == 0) {
		/*
		 * `Terminal count' is off, so read the residue
		 * out of the ESP counter registers.
		 */
		resid += (NCR_READ_REG(nsc, NCR_TCL) |
		    (NCR_READ_REG(nsc, NCR_TCM) << 8) |
		    ((nsc->sc_cfg2 & NCRCFG2_FE)
		    ? (NCR_READ_REG(nsc, NCR_TCH) << 16) : 0));

		if (resid == 0 && sc->sc_dmasize == 65536 &&
		    (nsc->sc_cfg2 & NCRCFG2_FE) == 0)
			/* A transfer of 64K is encoded as `TCL=TCM=0' */
			resid = 65536;
	}

	trans = sc->sc_dmasize - resid;
	if (trans < 0) {			/* transferred < 0 ? */
		trans = sc->sc_dmasize;
	}

	NCR_DMA(("dmaintr: tcl=%d, tcm=%d, tch=%d; trans=%d, resid=%d\n",
	    NCR_READ_REG(nsc, NCR_TCL), NCR_READ_REG(nsc, NCR_TCM),
	    (nsc->sc_cfg2 & NCRCFG2_FE) ? NCR_READ_REG(nsc, NCR_TCH) : 0,
	    trans, resid));


	*sc->sc_dmalen -= trans;
	*sc->sc_dmaaddr += trans;

	return 0;
}

void
esp_shutdownhook(void *arg)
{
	struct ncr53c9x_softc *sc = arg;

	NCRCMD(sc, NCRCMD_RSTSCSI);
}
