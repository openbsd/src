/*	$OpenBSD: sbicdma.c,v 1.6 2001/11/06 19:53:15 miod Exp $ */

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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Dale Rahn.
 *    and
 *      This product includes software developed under OpenBSD by
 *	Theo de Raadt for Willowglen Singapore.
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

void	sbicdmaattach	__P((struct device *, struct device *, void *));
int	sbicdmamatch	__P((struct device *, void *, void *));
int	sbicdmaprint	__P((void *auxp, const char *));

void	sbicdma_dmafree	__P((struct sbic_softc *));
void	sbicdma_dmastop	__P((struct sbic_softc *));
int	sbicdma_dmanext	__P((struct sbic_softc *));
int	sbicdma_dmago	__P((struct sbic_softc *, char *, int, int));
int	sbicdma_dmaintr	__P((struct sbic_softc *));
int	sbicdma_scintr	__P((struct sbic_softc *));

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
	struct	pccreg *pcc;
	struct	confargs *ca = args;

	sc->sc_cregs = (struct pccreg *)ca->ca_master;
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
	pccintr_establish(PCCV_SBIC, &sc->sc_ih);

	sc->sc_dmaih.ih_fn = sbicdma_dmaintr;
	sc->sc_dmaih.ih_arg = sc;
	sc->sc_dmaih.ih_ipl = ca->ca_ipl;
	pccintr_establish(PCCV_DMA, &sc->sc_dmaih);

	pcc = (struct pccreg *)sc->sc_cregs;
	pcc->pcc_dmairq = sc->sc_dmaih.ih_ipl | PCC_IRQ_INT;
	pcc->pcc_sbicirq = sc->sc_ih.ih_ipl | PCC_SBIC_RESETIRQ;

	sbicreset(sc);

	evcnt_attach(&sc->sc_dev, "intr", &sc->sc_intrcnt);
	evcnt_attach(&sc->sc_dev, "intr", &sc->sc_dmaintrcnt);

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
	struct	pccreg *pcc = (struct pccreg *)sc->sc_cregs;
	u_char	csr;
	u_long	pa;

	pmap_extract(pmap_kernel(), (vm_offset_t)va, &pa);
#ifdef DEBUG
	if (sbicdma_debug)
		printf("%s: dmago: va 0x%x pa 0x%x cnt %d flags %x\n",
		    sc->sc_dev.dv_xname, va, pa, count, flags);
#endif

	sc->sc_flags |= SBICF_INTR;
	pcc->pcc_dmadaddr = (u_long)pa;
	if (count & PCC_DMABCNT_CNTMASK) {
		printf("%s: dma count 0x%x too large\n",
		    sc->sc_dev.dv_xname, count);
		return (0);
	}
	pcc->pcc_dmabcnt = (PCC_DMABCNT_MAKEFC(FC_USERD)) |
	    (count & PCC_DMABCNT_CNTMASK);

	/* make certain interupts are disabled first, and reset */
	pcc->pcc_dmairq = sc->sc_dmaih.ih_ipl | PCC_IRQ_IEN | PCC_IRQ_INT;
	pcc->pcc_sbicirq = sc->sc_ih.ih_ipl | PCC_SBIC_RESETIRQ | PCC_IRQ_IEN;
	pcc->pcc_dmacsr = 0;
	pcc->pcc_dmacsr = PCC_DMACSR_DEN |
	    ((flags & DMAGO_READ) == 0) ? PCC_DMACSR_TOSCSI : 0;

	return (sc->sc_tcnt);
}

int
sbicdma_dmaintr(sc)
	struct sbic_softc *sc;
{
	struct	pccreg *pcc = (struct pccreg *)sc->sc_cregs;
	u_char	stat;
	int	ret = 0;

	/* DMA done */
	stat = pcc->pcc_dmacsr;
printf("dmaintr%d ", stat);
	if (stat & PCC_DMACSR_DONE) {
		pcc->pcc_dmacsr = 0;
		pcc->pcc_dmairq = 0;	/* ack and remove intr */
		if (stat & PCC_DMACSR_ERR8BIT) {
			printf("%s: 8 bit error\n", sc->sc_dev.dv_xname);
		}
		if (stat & PCC_DMACSR_DMAERRDATA) {
			printf("%s: DMA bus error\n", sc->sc_dev.dv_xname);
		}
		sc->sc_dmaintrcnt.ev_count++;
		return (1);
	}
	return (0);
}

int
sbicdma_scintr(sc)
	struct sbic_softc *sc;
{
	struct	pccreg *pcc = (struct pccreg *)sc->sc_cregs;
	u_char	stat;
	int	ret = 0;

printf("scintr%d ", stat);
	stat = pcc->pcc_sbicirq;
	if (stat & PCC_SBIC_RESETIRQ) {
		printf("%s: scintr: a scsi device pulled reset\n",
		    sc->sc_dev.dv_xname);
		pcc->pcc_sbicirq = pcc->pcc_sbicirq | PCC_SBIC_RESETIRQ;
	} else if (stat & PCC_IRQ_INT) {
		pcc->pcc_sbicirq = 0;
		sbicintr(sc);
		ret = 1;
		sc->sc_intrcnt.ev_count++;
	}
	return (ret);
}

void
sbicdma_dmastop(sc)
	struct sbic_softc *sc;
{
	printf("sbicdma_dmastop called\n");
	/* XXX do nothing */
}

int
sbicdma_dmanext(sc)
	struct sbic_softc *sc;
{
	printf("sbicdma_dmanext called\n");
}

void
sbicdma_dmafree(sc)
	struct sbic_softc *sc;
{
	struct pccreg *pcc = (struct pccreg *)sc->sc_cregs;

	printf("sbicdma_dmafree called\n");
	/* make certain interupts are disabled first, reset */
	pcc->pcc_dmairq = 0;
}
