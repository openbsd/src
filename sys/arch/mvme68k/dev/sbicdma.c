/*	$OpenBSD: sbicdma.c,v 1.12 2004/09/29 19:17:40 miod Exp $ */

/*
 * Copyright (c) 1995 Theo de Raadt
 * Copyright (c) 1995 Dale Rahn. All rights reserved.
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
 * as you may guess by some of the routines in this file,
 * dma is not yet working. - its on the TODO list
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <uvm/uvm_extern.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#include <machine/autoconf.h>
#include <mvme68k/dev/pccreg.h>
#include <mvme68k/dev/sbicreg.h>
#include <mvme68k/dev/sbicvar.h>
#include <mvme68k/dev/dmavar.h>

void	sbicdmaattach(struct device *, struct device *, void *);
int	sbicdmamatch(struct device *, void *, void *);
int	sbicdmaprint(void *auxp, const char *);

void	sbicdma_dmafree(struct sbic_softc *);
void	sbicdma_dmastop(struct sbic_softc *);
int	sbicdma_dmanext(struct sbic_softc *);
int	sbicdma_dmago(struct sbic_softc *, char *, int, int);
int	sbicdma_dmaintr(struct sbic_softc *);
int	sbicdma_scintr(struct sbic_softc *);

struct scsi_adapter sbicdma_scsiswitch = {
	sbic_scsicmd,
	sbic_minphys,
	0,		      /* no lun support */
	0,		      /* no lun support */
};

struct scsi_device sbicdma_scsidev = {
	NULL,	   /* use default error handler */
	NULL,	   /* have a queue served by this ??? */
	NULL,	   /* have no async handler ??? */
	NULL,	   /* Use default done routine */
}; 

#ifdef DEBUG
int	sbicdma_debug = 1;
#endif

int	sbicdma_maxdma = 0;    /* Maximum size per DMA transfer */
int	sbicdma_dmamask = 0; 
int	sbicdma_dmabounce = 0; 
 
struct cfdriver sbiccd = {
	NULL, "sbic", sbicdmamatch, sbicdmaattach,
	DV_DULL, sizeof(struct sbic_softc),
};

int
sbicdmamatch(parent, vcf, args)
	struct device *parent;
	void *vcf, *args;
{
	/*
	 * XXX finish to properly probe
	 */
	return (1);
}

void
sbicdmaattach(parent, self, args)
	struct device *parent, *self;
	void *args;
{
	struct	sbic_softc *sc = (struct sbic_softc *)self;
	struct	confargs *ca = args;

	sc->sc_dmafree = sbicdma_dmafree;
	sc->sc_dmago = sbicdma_dmago;
	sc->sc_dmanext = sbicdma_dmanext;
	sc->sc_dmastop = sbicdma_dmastop;
	sc->sc_dmacmd = 0;

	sc->sc_flags |= SBICF_BADDMA;
	sc->sc_dmamask = 0;
	sc->sc_sbicp = (sbic_regmap_p)ca->ca_vaddr;
	sc->sc_clkfreq = 100;

	sc->sc_link.adapter_softc = sc;
	sc->sc_link.adapter_target = 7;
	sc->sc_link.adapter = &sbicdma_scsiswitch;
	sc->sc_link.device = &sbicdma_scsidev;
	sc->sc_link.openings = 1;

	printf(": target %d\n", sc->sc_link.adapter_target);

	/* connect the interrupts */
	sc->sc_ih.ih_fn = sbicdma_scintr;
	sc->sc_ih.ih_arg = sc;
	sc->sc_ih.ih_ipl = ca->ca_ipl;
	pccintr_establish(PCCV_SBIC, &sc->sc_ih, self->dv_xname);

	sc->sc_dmaih.ih_fn = sbicdma_dmaintr;
	sc->sc_dmaih.ih_arg = sc;
	sc->sc_dmaih.ih_ipl = ca->ca_ipl;
	pccintr_establish(PCCV_DMA, &sc->sc_dmaih, self->dv_xname);

	sys_pcc->pcc_dmairq = sc->sc_dmaih.ih_ipl | PCC_IRQ_INT;
	sys_pcc->pcc_sbicirq = sc->sc_ih.ih_ipl | PCC_SBIC_RESETIRQ;

	sbicreset(sc);

	config_found(self, &sc->sc_link, sbicdmaprint);
}

/*
 * print diag if pnp is NULL else just extra
 */
int
sbicdmaprint(auxp, pnp)
	void		*auxp;
	const char	*pnp;
{
	if (pnp == NULL)
		return (UNCONF);
	return (QUIET);
}

int
sbicdma_dmago(sc, va, count, flags)
	struct	sbic_softc *sc;
	char	*va;
	int	count, flags;
{
	u_char	csr;
	u_long	pa;

	pmap_extract(pmap_kernel(), (vm_offset_t)va, &pa);
#ifdef DEBUG
	if (sbicdma_debug)
		printf("%s: dmago: va 0x%x pa 0x%x cnt %d flags %x\n",
		    sc->sc_dev.dv_xname, va, pa, count, flags);
#endif

	sc->sc_flags |= SBICF_INTR;
	sys_pcc->pcc_dmadaddr = (u_long)pa;
	if (count & PCC_DMABCNT_CNTMASK) {
		printf("%s: dma count 0x%x too large\n",
		    sc->sc_dev.dv_xname, count);
		return (0);
	}
	sys_pcc->pcc_dmabcnt = (PCC_DMABCNT_MAKEFC(FC_USERD)) |
	    (count & PCC_DMABCNT_CNTMASK);

	/* make certain interrupts are disabled first, and reset */
	sys_pcc->pcc_dmairq = sc->sc_dmaih.ih_ipl | PCC_IRQ_IEN | PCC_IRQ_INT;
	sys_pcc->pcc_sbicirq = sc->sc_ih.ih_ipl | PCC_SBIC_RESETIRQ | PCC_IRQ_IEN;
	sys_pcc->pcc_dmacsr = 0;
	sys_pcc->pcc_dmacsr = PCC_DMACSR_DEN |
	    ((flags & DMAGO_READ) == 0) ? PCC_DMACSR_TOSCSI : 0;

	return (sc->sc_tcnt);
}

int
sbicdma_dmaintr(sc)
	struct sbic_softc *sc;
{
	u_char	stat;
	int	ret = 0;

	/* DMA done */
	stat = sys_pcc->pcc_dmacsr;
#ifdef DEBUG
printf("dmaintr%d ", stat);
#endif
	if (stat & PCC_DMACSR_DONE) {
		sys_pcc->pcc_dmacsr = 0;
		sys_pcc->pcc_dmairq = 0;	/* ack and remove intr */
		if (stat & PCC_DMACSR_ERR8BIT) {
			printf("%s: 8 bit error\n", sc->sc_dev.dv_xname);
		}
		if (stat & PCC_DMACSR_DMAERRDATA) {
			printf("%s: DMA bus error\n", sc->sc_dev.dv_xname);
		}
		return (1);
	}
	return (0);
}

int
sbicdma_scintr(sc)
	struct sbic_softc *sc;
{
	u_char	stat;
	int	ret = 0;

#ifdef DEBUG
printf("scintr%d ", stat);
#endif
	stat = sys_pcc->pcc_sbicirq;
	if (stat & PCC_SBIC_RESETIRQ) {
		printf("%s: scintr: a scsi device pulled reset\n",
		    sc->sc_dev.dv_xname);
		sys_pcc->pcc_sbicirq = sys_pcc->pcc_sbicirq | PCC_SBIC_RESETIRQ;
	} else if (stat & PCC_IRQ_INT) {
		sys_pcc->pcc_sbicirq = 0;
		sbicintr(sc);
		ret = 1;
	}
	return (ret);
}

void
sbicdma_dmastop(sc)
	struct sbic_softc *sc;
{
#ifdef DEBUG
	printf("sbicdma_dmastop called\n");
#endif
	/* XXX do nothing */
}

int
sbicdma_dmanext(sc)
	struct sbic_softc *sc;
{
#ifdef DEBUG
	printf("sbicdma_dmanext called\n");
#endif
}

void
sbicdma_dmafree(sc)
	struct sbic_softc *sc;
{
#ifdef DEBUG
	printf("sbicdma_dmafree called\n");
#endif
	/* make certain interrupts are disabled first, reset */
	sys_pcc->pcc_dmairq = 0;
}
