/*	$NetBSD: dma.c,v 1.8 1995/02/01 12:37:21 pk Exp $ */

/*
 * Copyright (c) 1995 Theo de Raadt
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
 *	This product includes software developed under OpenBSD by
 *	Theo de Raadt.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
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

#include <sparc/autoconf.h>
#include <sparc/cpu.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#include <sparc/dev/sbusvar.h>
#include <sparc/dev/sbusreg.h>
#include <sparc/dev/dmareg.h>
#include <sparc/dev/dmavar.h>
#include <sparc/dev/espreg.h>
#include <sparc/dev/espvar.h>

#include <sparc/sparc/cache.h>

/*#define DMA_TEST*/

extern int sbus_print	__P((void *, char *));

void dmaattach		__P((struct device *, struct device *, void *));
int dmamatch		__P((struct device *, void *, void *));

struct cfdriver dmacd = {
	NULL, "dma", dmamatch, dmaattach,
	DV_DULL, sizeof(struct dma_softc)
};

struct cfdriver ledmacd = {
	NULL, "ledma", dmamatch, dmaattach,
	DV_DULL, sizeof(struct dma_softc)
};

struct cfdriver espdmacd = {
	NULL, "espdma", dmamatch, dmaattach,
	DV_DULL, sizeof(struct dma_softc)
};

int
dmamatch(parent, vcf, aux)
	struct	device *parent;
	void	*vcf, *aux;
{
	struct	cfdata *cf = vcf;
	register struct confargs *ca = aux;
	register struct romaux *ra = &ca->ca_ra;

	if (strcmp(cf->cf_driver->cd_name, ra->ra_name))
		return (0);
	if (ca->ca_bustype == BUS_SBUS)
		return (1);
	ra->ra_len = NBPG;
	return (probeget(ra->ra_vaddr, 1) != -1);
}

/*
 * Attach all the sub-devices we can find
 */
void
dmaattach(parent, self, aux)
	struct	device *parent, *self;
	void	*aux;
{
	register struct confargs *ca = aux;
	struct	confargs oca;
	struct	dma_softc *sc = (void *)self;
	int	node, base, slot;
	char	*name;

	/* XXX modifying ra_vaddr is bad! */
	if (ca->ca_ra.ra_vaddr == NULL)
		ca->ca_ra.ra_vaddr = mapiodev(ca->ca_ra.ra_reg, 0,
		    ca->ca_ra.ra_len, ca->ca_bustype);
	if ((u_long)ca->ca_ra.ra_paddr & PGOFSET)
		(u_long)ca->ca_ra.ra_vaddr |= ((u_long)ca->ca_ra.ra_paddr & PGOFSET);
	sc->sc_regs = (struct dma_regs *) ca->ca_ra.ra_vaddr;

	/*
	 * What happens here is that if the esp driver has not been
	 * configured, then this returns a NULL pointer. Then when the
	 * esp actually gets configured, it does the opposing test, and
	 * if the sc->sc_dma field in it's softc is NULL, then tries to
	 * find the matching dma driver.
	 */
	sc->sc_esp = ((struct esp_softc *)
	    getdevunit("esp", sc->sc_dev.dv_unit));
	if (sc->sc_esp)
		sc->sc_esp->sc_dma = sc;

	printf(": rev ");
	sc->sc_rev = sc->sc_regs->csr & D_DEV_ID;
	switch (sc->sc_rev) {
	case DMAREV_4300:
		printf("4/300\n");
		break;
	case DMAREV_1:
		printf("1\n");
		break;
	case DMAREV_ESC1:
		printf("ESC1\n");
		break;
	case DMAREV_PLUS:
		printf("1+\n");
		break;
	case DMAREV_2:
		printf("2\n");
		break;
	default:
		printf("unknown\n");
	}

#if defined(SUN4C) || defined(SUN4M)
	if (ca->ca_bustype == BUS_SBUS) {
		sc->sc_node = ca->ca_ra.ra_node;
		sbus_establish(&sc->sc_sd, &sc->sc_dev);

		/*
		 * If the device is in an SBUS slave slot, report
		 * it (but we don't care, because the corresponding
		 * ESP will also realize the same thing.)
		 */
		(void) sbus_slavecheck(self, ca);

		/*
		 * if our name is not "dma", we may have subdevices
		 * below us in the device tree (like an esp)
		 * XXX: TDR: should we do this even if it is "dma"?
		 */
		if (strcmp(ca->ca_ra.ra_name, "dma") == 0)
			return;

		/* search through children */
		for (node = firstchild(sc->sc_node); node;
		    node = nextsibling(node)) {
			name = getpropstring(node, "name");
			if (!romprop(&oca.ca_ra, name, node))
				continue;

			/*
			 * advance bootpath if it currently points to us
			 * XXX There appears to be strangeness in the unit
			 * number on at least one espdma system (SS5 says
			 * espdma5, but nothing is available to compare
			 * against that digit 5...
			 */
			if (ca->ca_ra.ra_bp &&
			    !strcmp(ca->ca_ra.ra_bp->name, ca->ca_ra.ra_name) &&
			    ca->ca_ra.ra_bp->val[1] == (int)ca->ca_ra.ra_paddr)
				oca.ca_ra.ra_bp = ca->ca_ra.ra_bp + 1;
			else
				oca.ca_ra.ra_bp = NULL;

			base = (int)oca.ca_ra.ra_paddr;
			if (SBUS_ABS(base)) {
				oca.ca_slot = SBUS_ABS_TO_SLOT(base);
				oca.ca_offset = SBUS_ABS_TO_OFFSET(base);
			} else {
				oca.ca_slot = slot = ca->ca_ra.ra_iospace;
				oca.ca_offset = base;
				oca.ca_ra.ra_paddr = (void *)SBUS_ADDR(slot, base);
			}
			oca.ca_bustype = BUS_SBUS;
			(void) config_found(&sc->sc_dev, (void *)&oca, sbus_print);
		}

	}
#endif /* SUN4C || SUN4M */
}

void
dmareset(sc)
	struct	dma_softc *sc;
{
	DMAWAIT_PEND(sc);
	DMACSR(sc) |= D_RESET;			/* reset DMA */
	DELAY(200);				/* what should this be ? */
	DMACSR(sc) &= ~D_RESET;			/* de-assert reset line */

	switch (sc->sc_rev) {
	case DMAREV_1:
	case DMAREV_4300:
	case DMAREV_ESC1:
		break;
	case DMAREV_PLUS:
	case DMAREV_2:
		if (sc->sc_esp->sc_rev >= ESP100A)
			DMACSR(sc) |= D_FASTER;
		break;
	}

	sc->sc_active = 0;			/* and of course we aren't */
}

int
dmapending(sc)
	struct dma_softc *sc;
{
	return (sc->sc_regs->csr & (D_INT_PEND|D_ERR_PEND));
}

/* bytes between loc and the end of this 16M region of memory */
#define DMAMAX(loc)	(0x01000000 - ((loc) & 0x00ffffff))

#define ESPMAX	((sc->sc_esp->sc_rev > ESP100A) ? \
		    (16 * 1024 * 1024) : (64 * 1024))

/*
 * Start a dma transfer or keep it going.
 * We do the loading of the transfer counter.
 * XXX: what about write-back caches?
 */
void
dmastart(sc, addr, len, datain, poll)
	struct	dma_softc *sc;
	void	*addr;
	size_t	*len;
	int	datain, poll;
{
	struct espregs *espr = sc->sc_esp->sc_regs;
	int	size;

	if (sc->sc_active)
		panic("dma: dmastart() called while active\n");

	sc->sc_dmaaddr = addr;
	sc->sc_dmalen = len;
	sc->sc_dmapolling = poll;
	sc->sc_dmadev2mem = datain;

	/*
	 * the rules say we cannot transfer more than the limit
	 * of this DMA chip (64k for old and 16Mb for new),
	 * and we cannot cross a 16Mb boundary.
	 */
	size = min(*sc->sc_dmalen, ESPMAX);
	size = min(size, DMAMAX((size_t) *sc->sc_dmaaddr));
	sc->sc_segsize = size;

#ifdef DMA_TEST
	printf("%s: start %d@0x%08x [%s scsi] [chunk=%d] %d\n",
	    sc->sc_dev.dv_xname,
	    *sc->sc_dmalen, *sc->sc_dmaaddr,
	    datain ? "read from" : "write to",
	    sc->sc_segsize, poll);
#endif

	espr->espr_tcl = size;
	espr->espr_tcm = size >> 8;
	if (sc->sc_esp->sc_rev > ESP100A)
		espr->espr_tch = size >> 16;
	espr->espr_cmd = ESPCMD_DMA|ESPCMD_NOP;	/* load the count in */

	DMADDR(sc) = *sc->sc_dmaaddr;
	DMACSR(sc) = (DMACSR(sc) & ~(D_WRITE|D_INT_EN)) | D_EN_DMA |
	    (datain ? D_WRITE : 0) | (poll ? 0 : D_INT_EN);

	/* and kick the SCSI */
	espr->espr_cmd = ESPCMD_DMA|ESPCMD_TRANS;

	sc->sc_active = 1;
}

/*
 * Pseudo (chained) interrupt from the esp driver to kick the
 * current running DMA transfer. espintr() cleans up errors.
 *
 * return 1 if a dma operation is being continued (for when all
 * the data could not be transferred in one dma operation).
 */
int
dmaintr(sc, restart)
	struct	dma_softc *sc;
	int restart;
{
	struct espregs *espr = sc->sc_esp->sc_regs;
	int	trans = 0, resid;

#ifdef DMA_TEST
	printf("%s: intr\n", sc->sc_dev.dv_xname);
#endif

	if (DMACSR(sc) & D_ERR_PEND) {
		printf("%s: error", sc->sc_dev.dv_xname);
		if (sc->sc_rev == DMAREV_4300)
			DMAWAIT_PEND(sc);
		DMACSR(sc) |= D_INVALIDATE;
		return (0);
	}

	if (sc->sc_active == 0)
		panic("dmaintr: DMA wasn't active");

	/* DMA has stopped, but flush it first */
	dmadrain(sc);
	if (DMACSR(sc) & D_DRAINING)
		printf("drain failed %d left\n", DMACSR(sc) & D_DRAINING);
	DMACSR(sc) &= ~D_EN_DMA;
	sc->sc_active = 0;

	/* 
	 * XXX: The subtracting of resid and throwing away up to 31
	 * bytes cannot be the best/right way to do this. There's got
	 * to be a better way, such as letting the DMA take control
	 * again and putting it into memory, or pulling it out and
	 * putting it in memory by ourselves.
	 * XXX in the meantime, just do this job silently
	 */
	resid = 0;
	if (!sc->sc_dmadev2mem &&
	    (resid = (espr->espr_fflag & ESPFIFO_FF)) != 0) {
#if 0
		printf("empty FIFO of %d ", resid);
#endif
		espr->espr_cmd = ESPCMD_FLUSH;
		DELAY(1);
	}
	resid += espr->espr_tcl | (espr->espr_tcm << 8) |
	    (sc->sc_esp->sc_rev > ESP100A ? (espr->espr_tch << 16) : 0);
	trans = sc->sc_segsize - resid;
	if (trans < 0) {			/* transferred < 0 ? */
		printf("%s: xfer (%d) > req (%d)\n",
		    sc->sc_dev.dv_xname, trans, sc->sc_segsize);
		trans = sc->sc_segsize;
	}

#ifdef DMA_TEST
	printf("dmaintr: resid=%d, trans=%d\n", resid, trans);
#endif

	if (sc->sc_dmadev2mem && vactype != VAC_NONE)
		cache_flush(*sc->sc_dmaaddr, trans);

	sc->sc_segsize -= trans;
	*sc->sc_dmalen -= trans;
	*sc->sc_dmaaddr += trans;

#ifdef DMA_TEST
	printf("%s: %d/%d bytes left\n", sc->sc_dev.dv_xname,
	    *sc->sc_dmalen, sc->sc_segsize);
#endif

	/* completely finished with the DMA transaction */
	if (sc->sc_segsize == 0 && *sc->sc_dmalen == 0)
		return (0);

	if (restart == 0)
		return (1);
	/* and again */
	dmastart(sc, sc->sc_dmaaddr, sc->sc_dmalen, sc->sc_dmadev2mem,
	    sc->sc_dmapolling);
	return (1);
}

/*
 * We have to ask rev 1 and 4/300 dma controllers to drain their fifo.
 * Apparently the other chips have drained by the time we get an
 * interrupt, but we check anyways.
 */
void
dmadrain(sc)
	struct dma_softc *sc;
{
	switch (sc->sc_rev) {
	case DMAREV_1:
	case DMAREV_4300:
	case DMAREV_PLUS:
	case DMAREV_2:
		if (DMACSR(sc) & D_DRAINING)
			DMAWAIT_DRAIN(sc);
		break;
	case DMAREV_ESC1:
		DMAWAIT_PEND(sc)
		DMAWAIT_DRAIN(sc);		/* XXX: needed? */
		break;
	}
	DMACSR(sc) |= D_INVALIDATE;
}

/*
 * XXX
 * During autoconfig we are in polled mode and we execute some commands.
 * eventually we execute the last polled command. esp interrupts go through
 * the dma chip's D_INT_EN gate. thus, because we have our data, we're happy
 * and return. the esp chip has not, however, become unbusy. as soon as we
 * execute our first non-polled command, we find the esp state machine is
 * non-idle. it has not finished getting off the scsi bus, because it didn't
 * get interrupts (and the polled code has long since gone it's merry way.)
 * 
 * therefore, whenever we finish with a polled mode command, we enable
 * interrupts so that we can get our data. it is probably safe to do so,
 * since the scsi transfer has happened without error. the interrupts that
 * will happen have no bearing on the higher level scsi subsystem, since it
 * just functions to let the esp chip "clean up" it's state.
 */
void
dmaenintr(sc)
	struct dma_softc *sc;
{
	DMACSR(sc) |= D_INT_EN;
}

int
dmadisintr(sc)
	struct dma_softc *sc;
{
	int x = DMACSR(sc) & D_INT_EN;

	DMACSR(sc) &= ~D_INT_EN;
	return x;
}
