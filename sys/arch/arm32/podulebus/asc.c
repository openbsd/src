/* $NetBSD: asc.c,v 1.5 1996/04/19 20:13:56 mark Exp $ */

/*
 * Copyright (c) 1996 Mark Brinicombe
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *	from:ahsc.c
 */

/*
 * Ok this driver is not wonderful yet. It only supports POLLING mode
 * The Acorn SCSI card (or any WD3393 based card) does not support
 * DMA so the DMA section of this driver and the sbic driver needs
 * to be rewritten.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>
#include <machine/io.h>
#include <machine/irqhandler.h>
#include <machine/katelib.h>
#include <arm32/podulebus/podulebus.h>
#include <arm32/podulebus/sbicreg.h>
#include <arm32/podulebus/sbicvar.h>
#include <arm32/podulebus/ascreg.h>
#include <arm32/podulebus/ascvar.h>

int ascprint __P((void *auxp, const char *));
void ascattach __P((struct device *, struct device *, void *));
int ascmatch __P((struct device *, void *, void *));

void asc_enintr __P((struct sbic_softc *));
void asc_dmastop __P((struct sbic_softc *));
int asc_dmanext __P((struct sbic_softc *));
int asc_dmaintr __P((struct sbic_softc *));
int asc_dmago __P((struct sbic_softc *, char *, int, int));
int asc_scsicmd __P((struct scsi_xfer *xs));
int asc_intr __P((struct asc_softc *));

char *strstr		__P((char */*s1*/, char */*s2*/));

struct scsi_adapter asc_scsiswitch = {
	asc_scsicmd,
	sbic_minphys,
	0,			/* no lun support */
	0,			/* no lun support */
};

struct scsi_device asc_scsidev = {
	NULL,		/* use default error handler */
	NULL,		/* do not have a start functio */
	NULL,		/* have no async handler */
	NULL,		/* Use default done routine */
};


#ifdef DEBUG
int	asc_dmadebug = 0;
#endif

struct cfattach asc_ca = {
	sizeof(struct asc_softc), ascmatch, ascattach
};

struct cfdriver asc_cd = {
	NULL, "asc", DV_DULL, NULL, 0
};

u_long scsi_nosync;
int shift_nosync;

#if ASC_POLL > 0
int asc_poll = 1;

extern char *boot_args;
#endif

int
ascmatch(pdp, match, auxp)
	struct device *pdp;
	void *match, *auxp;
{
	struct podule_attach_args *pa = (struct podule_attach_args *)auxp;
	int podule;

	podule = findpodule(0x00, 0x02, pa->pa_podule_number);

	if (podule == -1)
	  return(0);

	pa->pa_podule_number = podule;
	pa->pa_podule = &podules[podule];

	return(1);
}

void
ascattach(pdp, dp, auxp)
	struct device *pdp, *dp;
	void *auxp;
{
	volatile struct sdmac *rp;
	struct asc_softc *sc;
	struct sbic_softc *sbic;
	struct podule_attach_args *pa;

	sc = (struct asc_softc *)dp;

	pa = (struct podule_attach_args *)auxp;
	sc->sc_podule_number = pa->pa_podule_number;
	if (sc->sc_podule_number == -1)
		panic("Podule has disappeared !");

	sc->sc_podule = &podules[sc->sc_podule_number];
	podules[sc->sc_podule_number].attached = 1;

#if ASC_POLL > 0
        if (boot_args) {
        	char *ptr;
       
		ptr = strstr(boot_args, "noascpoll");
		if (ptr)
			asc_poll = 0;
	}

	if (asc_poll)
		printf(" polling");
	else
		printf(" using interrupts");
#endif
	printf("\n");

	sbic = &sc->sc_softc;

	sbic->sc_enintr = asc_enintr;
	sbic->sc_dmago = asc_dmago;
	sbic->sc_dmanext = asc_dmanext;
	sbic->sc_dmastop = asc_dmastop;
	sbic->sc_dmacmd = 0;

	/*
	 * eveything is a valid dma address
	 */
	sbic->sc_dmamask = 0;
	sbic->sc_sbicp = (sbic_regmap_p) (sc->sc_podule->mod_base + ASC_SBIC);
	sbic->sc_clkfreq = sbic_clock_override ? sbic_clock_override : 143;
	
	sbic->sc_link.adapter_softc = sbic;
	sbic->sc_link.adapter_target = 7;
	sbic->sc_link.adapter = &asc_scsiswitch;
	sbic->sc_link.device = &asc_scsidev;
	sbic->sc_link.openings = 1;	/* was 2 */

	sc->sc_pagereg = sc->sc_podule->fast_base + ASC_PAGEREG;
        sc->sc_intstat = sc->sc_podule->fast_base + ASC_INTSTATUS;

	/* Reset the card */

	WriteByte(sc->sc_pagereg, 0x80);
	DELAY(500000);
	WriteByte(sc->sc_pagereg, 0x00);
	DELAY(250000);

	sbicinit(sbic);

/* If we are polling only then we don't need a interrupt handler */

	sc->sc_ih.ih_func = asc_intr;
	sc->sc_ih.ih_arg = sc;
	sc->sc_ih.ih_level = IPL_BIO;
	sc->sc_ih.ih_name = "asc";

#ifdef ASC_POLL
	if (!asc_poll)
#endif
	if (irq_claim(IRQ_PODULE, &sc->sc_ih))
		panic("asc: Cannot claim podule IRQ");

	/*
	 * attach all scsi units on us
	 */
	config_found(dp, &sbic->sc_link, ascprint);
}

/*
 * print diag if pnp is NULL else just extra
 */
int
ascprint(auxp, pnp)
	void *auxp;
	const char *pnp;
{
	if (pnp == NULL)
		return(UNCONF);
	return(QUIET);
}


void
asc_enintr(sbicsc)
	struct sbic_softc *sbicsc;
{
	struct asc_softc *sc = (struct asc_softc *)sbicsc;
/*	printf("asc_enintr\n");*/
/*
	volatile struct sdmac *sdp;

	sdp = dev->sc_cregs;

	dev->sc_flags |= SBICF_INTR;
	sdp->CNTR = CNTR_PDMD | CNTR_INTEN;
*/
	sbicsc->sc_flags |= SBICF_INTR;
	WriteByte(sc->sc_pagereg, 0x40);
}


int
asc_dmago(dev, addr, count, flags)
	struct sbic_softc *dev;
	char *addr;
	int count, flags;
{
	printf("asc_dmago\n");
#ifdef DDB
	Debugger();
#else
	panic("Hit a brick wall");
#endif
#if 0
	volatile struct sdmac *sdp;

	sdp = dev->sc_cregs;
	/*
	 * Set up the command word based on flags
	 */
	dev->sc_dmacmd = CNTR_PDMD | CNTR_INTEN;
	if ((flags & DMAGO_READ) == 0)
		dev->sc_dmacmd |= CNTR_DDIR;
#ifdef DEBUG
	if (ahsc_dmadebug & DDB_IO)
		printf("ahsc_dmago: cmd %x\n", dev->sc_dmacmd);
#endif

	dev->sc_flags |= SBICF_INTR;
	sdp->CNTR = dev->sc_dmacmd;
	sdp->ACR = (u_int) dev->sc_cur->dc_addr;
	sdp->ST_DMA = 1;

	return(dev->sc_tcnt);
#endif
}

void
asc_dmastop(dev)
	struct sbic_softc *dev;
{
/*	printf("asc_dmastop\n");*/
#if 0
	volatile struct sdmac *sdp;
	int s;

	sdp = dev->sc_cregs;

#ifdef DEBUG
	if (ahsc_dmadebug & DDB_FOLLOW)
		printf("ahsc_dmastop()\n");
#endif
	if (dev->sc_dmacmd) {
		s = splbio();
		if ((dev->sc_dmacmd & (CNTR_TCEN | CNTR_DDIR)) == 0) {
			/*
			 * only FLUSH if terminal count not enabled,
			 * and reading from peripheral
			 */
			sdp->FLUSH = 1;
			while ((sdp->ISTR & ISTR_FE_FLG) == 0)
				;
		}
		/* 
		 * clear possible interrupt and stop dma
		 */
		sdp->CINT = 1;
		sdp->SP_DMA = 1;
		dev->sc_dmacmd = 0;
		splx(s);
	}
#endif
}

int
asc_dmaintr(dev)
	struct sbic_softc *dev;
{
	panic("asc_dmaintr");
#if 0
	volatile struct sdmac *sdp;
	int stat, found;

	sdp = dev->sc_cregs;
	stat = sdp->ISTR;

	if ((stat & (ISTR_INT_F|ISTR_INT_P)) == 0)
		return (0);

#ifdef DEBUG
	if (ahsc_dmadebug & DDB_FOLLOW)
		printf("%s: dmaintr 0x%x\n", dev->sc_dev.dv_xname, stat);
#endif

	/*
	 * both, SCSI and DMA interrupts arrive here. I chose
	 * arbitrarily that DMA interrupts should have higher
	 * precedence than SCSI interrupts.
	 */
	found = 0;
	if (stat & ISTR_E_INT) {
		++found;

		sdp->CINT = 1;	/* clear possible interrupt */

		/*
		 * check for SCSI ints in the same go and 
		 * eventually save an interrupt
		 */
	}

	if (dev->sc_flags & SBICF_INTR && stat & ISTR_INTS)
		found += sbicintr(dev);
	return(found);
#endif
}


int
asc_dmanext(dev)
	struct sbic_softc *dev;
{
	printf("asc_dmanext\n");
#ifdef DDB
	Debugger();
#else
	panic("Hit a brick wall");
#endif
#if 0
	volatile struct sdmac *sdp;
	int i, stat;

	sdp = dev->sc_cregs;

	if (dev->sc_cur > dev->sc_last) {
		/* shouldn't happen !! */
		printf("ahsc_dmanext at end !!!\n");
		asc_dmastop(dev);
		return(0);
	}
	if ((dev->sc_dmacmd & (CNTR_TCEN | CNTR_DDIR)) == 0) {
		  /* 
		   * only FLUSH if terminal count not enabled,
		   * and reading from peripheral
		   */
		sdp->FLUSH = 1;
		while ((sdp->ISTR & ISTR_FE_FLG) == 0)
			;
        }
	/* 
	 * clear possible interrupt and stop dma
	 */
	sdp->CINT = 1;	/* clear possible interrupt */
	sdp->SP_DMA = 1;	/* stop dma */
	sdp->CNTR = dev->sc_dmacmd;
	sdp->ACR = (u_int)dev->sc_cur->dc_addr;
	sdp->ST_DMA = 1;

	dev->sc_tcnt = dev->sc_cur->dc_count << 1;
	return(dev->sc_tcnt);
#endif
}

void
asc_dump()
{
	int i;

	for (i = 0; i < asc_cd.cd_ndevs; ++i)
		if (asc_cd.cd_devs[i])
			sbic_dump(asc_cd.cd_devs[i]);
}

int
asc_scsicmd(xs)
	struct scsi_xfer *xs;
{
	struct scsi_link *sc_link = xs->sc_link;

	/* ensure command is polling for the moment */
#if ASC_POLL > 0
	if (asc_poll)
		xs->flags |= SCSI_POLL;
#endif

/*	printf("id=%d lun=%dcmdlen=%d datalen=%d opcode=%02x flags=%08x status=%02x blk=%02x %02x\n",
	    sc_link->target, sc_link->lun, xs->cmdlen, xs->datalen, xs->cmd->opcode,
	    xs->flags, xs->status, xs->cmd->bytes[0], xs->cmd->bytes[1]);*/

	return(sbic_scsicmd(xs));
}


int
asc_intr(sc)
	struct asc_softc *sc;
{
	int intr;
	
/*	printf("ascintr:");*/
       	intr = ReadByte(sc->sc_intstat);
/*       	printf("%02x\n", intr);*/

	if (intr & IS_SBIC_IRQ)
		sbicintr((struct sbic_softc *)sc);
	return(0);
}


int kvtop()
{
	printf("kvtop\n");
#ifdef DDB
	Debugger();
#else
	panic("Hit a brick wall");
#endif
	return(0);
}

void alloc_z2mem()
{
	panic("allocz2mem");
}

void isztwomem()
{
	panic("isz2mem");
}

void PREP_DMA_MEM()
{
	panic("PREP_DMA_MEM");
}
