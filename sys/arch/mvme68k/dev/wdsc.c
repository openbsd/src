/*	$OpenBSD: wdsc.c,v 1.19 2013/11/30 20:25:47 miod Exp $ */

/*
 * Copyright (c) 2013 Miodrag Vallat.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
/*
 * Copyright (c) 2001 Wayne Knowles
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Wayne Knowles
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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
 * Copyright (c) 1996 Steve Woodford
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
 * 3. Neither the name of the University nor the names of its contributors
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
 *  @(#)wdsc.c
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>

#include <uvm/uvm_extern.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#include <machine/autoconf.h>

#include <mvme68k/dev/pccreg.h>
#include <mvme68k/dev/wdscreg.h>

#include <dev/ic/wd33c93reg.h>
#include <dev/ic/wd33c93var.h>

struct dma_table_entry {
	uint32_t	dc_paddr;
	uint32_t	dc_cnt;
};

struct wdsc_softc {
	struct wd33c93_softc	sc_wd33c93;
	bus_dma_tag_t		sc_dmat;
	bus_dmamap_t		sc_dmamap;
	bus_dmamap_t		sc_tablemap;
	bus_dma_segment_t	sc_tableseg;
	vaddr_t			sc_tableva;
	struct intrhand		sc_dmaih;
	struct intrhand		sc_wdscih;
	int			sc_ipl;
	int			sc_flags;
#define	WDSC_DMA_ACTIVE			0x01
#define	WDSC_DMA_MAPLOADED		0x02
	u_short			sc_dmacmd;
};

int	wdscmatch(struct device *, void *, void *);
void	wdscattach(struct device *, struct device *, void *);

const struct cfattach wdsc_ca = {
	sizeof(struct wdsc_softc), wdscmatch, wdscattach
};

struct cfdriver wdsc_cd = {
	NULL, "wdsc", DV_DULL
};

int	wdsc_dmasetup(struct wd33c93_softc *, void **, size_t *, int, size_t *);
int	wdsc_dmago(struct wd33c93_softc *);
void	wdsc_dmastop(struct wd33c93_softc *);

int	wdsc_alloc_physical(struct wdsc_softc *, bus_dmamap_t *,
	    bus_dma_segment_t *, vaddr_t *, bus_size_t, const char *);
int	wdsc_dmaintr(void *);
int	wdsc_scsiintr(void *);

struct scsi_adapter wdsc_switch = {
	wd33c93_scsi_cmd,
	scsi_minphys,
	NULL,
	NULL
};

/*
 * Match for SCSI devices on the onboard WD33C93 chip
 */
int
wdscmatch(struct device *parent, void *vcf, void *aux)
{
	struct confargs *ca = (struct confargs *)aux;

	if (strcmp(ca->ca_name, wdsc_cd.cd_name) != 0)
		return 0;
	return 1;
}

/*
 * Attach the wdsc driver
 */
void
wdscattach(struct device *parent, struct device *self, void *aux)
{
	struct wdsc_softc *wsc = (struct wdsc_softc *)self;
	struct confargs *ca = (struct confargs *)aux;
	struct wd33c93_softc *sc = &wsc->sc_wd33c93;
	int tmp;

	/*
	 * Map address and data registers.
	 */

	sc->sc_regt = ca->ca_iot;
	if (bus_space_map(ca->ca_iot, ca->ca_paddr + 0, 1, 0,
	    &sc->sc_asr_regh) != 0) {
		printf(": failed to map asr register\n");
		return;
	}
	if (bus_space_map(ca->ca_iot, ca->ca_paddr + 1, 1, 0,
	    &sc->sc_data_regh) != 0) {
		printf(": failed to map data register\n");
		return;
	}

	/*
	 * Allocate DMA map for up to MAXPHYS bytes.
	 */

	wsc->sc_dmat = ca->ca_dmat;
	if (bus_dmamap_create(ca->ca_dmat, MAXPHYS, 1 + atop(MAXPHYS),
	    0, 0, BUS_DMA_WAITOK, &wsc->sc_dmamap) != 0) {
		printf(": failed to create dmamap\n");
		return;
	}

	/*
	 * Allocate table walk memory.
	 */
	if (wdsc_alloc_physical(wsc, &wsc->sc_tablemap, &wsc->sc_tableseg,
	    &wsc->sc_tableva,
	    sizeof(struct dma_table_entry) * (1 + atop(MAXPHYS)),
	    "dma table") != 0) {
		bus_dmamap_destroy(ca->ca_dmat, wsc->sc_dmamap);
		return;
	}

	sc->sc_dmasetup = wdsc_dmasetup;
	sc->sc_dmago = wdsc_dmago;
	sc->sc_dmastop = wdsc_dmastop;
	sc->sc_reset = NULL;

	/*
	 * The onboard WD33C93 of the MVME147 is usually clocked at 10MHz...
	 */
	sc->sc_clkfreq = 100;
	sc->sc_id = 7;
	sc->sc_dmamode = SBIC_CTL_DMA;

	wsc->sc_dmacmd  = 0;
	wsc->sc_ipl = ca->ca_ipl;

	sys_pcc->pcc_sbicirq = ca->ca_ipl | PCC_IRQ_INT;
	sys_pcc->pcc_dmairq = ca->ca_ipl | PCC_IRQ_INT;
	sys_pcc->pcc_dmacsr  = 0;

	/*
	 * Register interrupt handlers for DMA and WDSC
	 */
	wsc->sc_dmaih.ih_fn = wdsc_dmaintr;
	wsc->sc_dmaih.ih_arg = wsc;
	wsc->sc_dmaih.ih_ipl = ca->ca_ipl;
	pccintr_establish(PCCV_DMA, &wsc->sc_dmaih, self->dv_xname);

	wsc->sc_wdscih.ih_fn = wdsc_scsiintr;
	wsc->sc_wdscih.ih_arg = wsc;
	wsc->sc_wdscih.ih_ipl = ca->ca_ipl;
	pccintr_establish(PCCV_SBIC, &wsc->sc_wdscih, self->dv_xname);

	/*
	 * Attach all SCSI devices on us, watching for boot device
	 * (see device_register).
	 */
	tmp = bootpart;
	if (ca->ca_paddr != bootaddr) 
		bootpart = -1;
	wd33c93_attach(sc, &wdsc_switch);
	bootpart = tmp;		/* restore old value */

	sys_pcc->pcc_sbicirq = ca->ca_ipl | PCC_IRQ_IEN | PCC_IRQ_INT;
}

/*
 * Prime the hardware for a DMA transfer
 */
int
wdsc_dmasetup(struct wd33c93_softc *sc, void **addr, size_t *len, int datain,
    size_t *dmasize)
{
	struct wdsc_softc *wsc = (struct wdsc_softc *)sc;
	int count;
	int rc;

	KASSERT((wsc->sc_flags & WDSC_DMA_ACTIVE) == 0);

	count = *len;
	if (count) {
		KASSERT((wsc->sc_flags & WDSC_DMA_MAPLOADED) == 0);

		if (datain)
			wsc->sc_dmacmd = DMAC_CSR_ENABLE;
		else
			wsc->sc_dmacmd = DMAC_CSR_ENABLE | DMAC_CSR_WRITE;

		rc = bus_dmamap_load(wsc->sc_dmat, wsc->sc_dmamap,
		    *addr, count, NULL, BUS_DMA_NOWAIT);
		if (rc != 0)
			panic("%s: bus_dmamap_load failed, rc=%d",
			    sc->sc_dev.dv_xname, rc);

		bus_dmamap_sync(wsc->sc_dmat, wsc->sc_dmamap, 0,
		    wsc->sc_dmamap->dm_mapsize, datain ?
		      BUS_DMASYNC_PREREAD : BUS_DMASYNC_PREWRITE);

		/*
		 * Build dma table, unless transfer fits in one
		 * contiguous chunk
		 */
		if (wsc->sc_dmamap->dm_nsegs > 1) {
			struct dma_table_entry *entry;
			bus_dma_segment_t *seg;
			int i;

			entry = (struct dma_table_entry *)wsc->sc_tableva;
			seg = wsc->sc_dmamap->dm_segs;
			for (i = wsc->sc_dmamap->dm_nsegs; i != 0; i--) {
				entry->dc_paddr = seg->ds_addr;
				entry->dc_cnt = seg->ds_len |
				    (FC_SUPERD << 24) | (1UL << 31);
				seg++;
				entry++;
			}
			(--entry)->dc_cnt &= ~(1UL << 31);

			wsc->sc_dmacmd |= DMAC_CSR_TABLE;
		}

		wsc->sc_flags |= WDSC_DMA_MAPLOADED;
	}

	return count;
}

/*
 * Trigger a DMA transfer
 */
int
wdsc_dmago(struct wd33c93_softc *sc)
{
	struct wdsc_softc *wsc = (struct wdsc_softc *)sc;

	KASSERT((wsc->sc_flags & WDSC_DMA_ACTIVE) == 0);
	KASSERT((wsc->sc_flags & WDSC_DMA_MAPLOADED));

	wsc->sc_flags |= WDSC_DMA_ACTIVE;

	sys_pcc->pcc_dmacsr = 0;
	sys_pcc->pcc_dmairq = wsc->sc_ipl | PCC_IRQ_IEN | PCC_IRQ_INT;
	if (wsc->sc_dmamap->dm_nsegs > 1) {
		sys_pcc->pcc_tafcr = FC_SUPERD;
		sys_pcc->pcc_dmataddr = wsc->sc_tableseg.ds_addr;
	} else {
		sys_pcc->pcc_dmadaddr =
		    (unsigned long)wsc->sc_dmamap->dm_segs[0].ds_addr;
		sys_pcc->pcc_dmabcnt = (FC_SUPERD << 24) |
		    (unsigned long)wsc->sc_dmamap->dm_segs[0].ds_len;
	}
	sys_pcc->pcc_dmacsr = wsc->sc_dmacmd;

	return wsc->sc_dmamap->dm_mapsize;
}

/*
 * Stop DMA, and disable DMA interrupts
 */
void
wdsc_dmastop(struct wd33c93_softc *sc)
{
	struct wdsc_softc *wsc = (struct wdsc_softc *)sc;

	if (wsc->sc_flags & WDSC_DMA_ACTIVE) {
		sys_pcc->pcc_dmacsr = 0;
		sys_pcc->pcc_dmairq = wsc->sc_ipl | PCC_IRQ_INT;

		bus_dmamap_sync(wsc->sc_dmat, wsc->sc_dmamap, 0,
		    wsc->sc_dmamap->dm_mapsize,
		    wsc->sc_dmacmd & DMAC_CSR_WRITE ?
		      BUS_DMASYNC_POSTWRITE : BUS_DMASYNC_POSTREAD);
	}
	if (wsc->sc_flags & WDSC_DMA_MAPLOADED)
		bus_dmamap_unload(wsc->sc_dmat, wsc->sc_dmamap);
	wsc->sc_flags &= ~(WDSC_DMA_ACTIVE | WDSC_DMA_MAPLOADED);
}

/*
 * DMA completion interrupt
 */
int
wdsc_dmaintr(void *arg)
{
	struct wdsc_softc *wsc = (struct wdsc_softc *)arg;
	int rc = -1;

	/*
	 * Really a DMA interrupt?
	 */
	if ((sys_pcc->pcc_dmairq & PCC_IRQ_INT) == 0)
		return 0;

	if (sys_pcc->pcc_dmacsr & DMAC_CSR_DONE) {
		rc = 1;
		/* acknowledge interrupt... */
		sys_pcc->pcc_dmairq = wsc->sc_ipl | PCC_IRQ_IEN | PCC_IRQ_INT;
		if (sys_pcc->pcc_dmacsr & (DMAC_CSR_TBUSERR | DMAC_CSR_DBUSERR |
					DMAC_CSR_TSIZE | DMAC_CSR_8BITS)) {
			printf("%s: DMA error, CSR=%02x\n",
			    wsc->sc_wd33c93.sc_dev.dv_xname,
			    sys_pcc->pcc_dmacsr);
		}
		sys_pcc->pcc_dmacsr = 0;
		sys_pcc->pcc_dmairq = wsc->sc_ipl | PCC_IRQ_INT;
	}

	return rc;
}

/*
 * SCSI interrupt
 */
int
wdsc_scsiintr(void *arg)
{
	struct wdsc_softc *wsc = (struct wdsc_softc *)arg;
	int rc;

	/*
	 * Really a SCSI interrupt?
	 */
	if ((sys_pcc->pcc_sbicirq & PCC_IRQ_INT) == 0)
		return 0;

	rc = wd33c93_intr(&wsc->sc_wd33c93);

	/*
	 * Acknowledge and clear the interrupt
	 */
	sys_pcc->pcc_sbicirq = wsc->sc_ipl | PCC_IRQ_IEN | PCC_IRQ_INT;

	return rc;
}

/*
 * Allocate contiguous physical memory.
 */
int
wdsc_alloc_physical(struct wdsc_softc *wsc, bus_dmamap_t *dmamap,
    bus_dma_segment_t *dmaseg, vaddr_t *va, bus_size_t len, const char *what)
{
	int nseg;
	int rc;

	len = round_page(len);

	rc = bus_dmamem_alloc(wsc->sc_dmat, len, 0, 0, dmaseg, 1, &nseg,
	    BUS_DMA_NOWAIT);
	if (rc != 0) {
		printf("%s: unable to allocate %s memory: error %d\n",
		    wsc->sc_wd33c93.sc_dev.dv_xname, what, rc);
		goto fail1;
	}

	rc = bus_dmamem_map(wsc->sc_dmat, dmaseg, nseg, len,
	    (caddr_t *)va, BUS_DMA_NOWAIT | BUS_DMA_COHERENT);
	if (rc != 0) {
		printf("%s: unable to map %s memory: error %d\n",
		    wsc->sc_wd33c93.sc_dev.dv_xname, what, rc);
		goto fail2;
	}

	rc = bus_dmamap_create(wsc->sc_dmat, len, 1, len, 0,
	    BUS_DMA_NOWAIT /* | BUS_DMA_ALLOCNOW */, dmamap);
	if (rc != 0) {
		printf("%s: unable to create %s dma map: error %d\n",
		    wsc->sc_wd33c93.sc_dev.dv_xname, what, rc);
		goto fail3;
	}

	rc = bus_dmamap_load(wsc->sc_dmat, *dmamap, (void *)*va, len, NULL,
	    BUS_DMA_NOWAIT);
	if (rc != 0) {
		printf("%s: unable to load %s dma map: error %d\n",
		    wsc->sc_wd33c93.sc_dev.dv_xname, what, rc);
		goto fail4;
	}

	return 0;

fail4:
	bus_dmamap_destroy(wsc->sc_dmat, *dmamap);
fail3:
	bus_dmamem_unmap(wsc->sc_dmat, (caddr_t)*va, PAGE_SIZE);
fail2:
	bus_dmamem_free(wsc->sc_dmat, dmaseg, 1);
fail1:
	return rc;
}
