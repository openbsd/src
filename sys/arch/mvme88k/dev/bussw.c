/*	$OpenBSD: bussw.c,v 1.12 2004/04/14 13:43:13 miod Exp $ */

/*
 * Copyright (c) 1999 Steve Murphree, Jr.
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
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/ioctl.h>
#include <sys/device.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include <sys/malloc.h>
#include <machine/psl.h>
#include <machine/autoconf.h>
#include <machine/cpu.h>
#include <machine/mioctl.h>
#include <machine/vmparam.h>

#include <mvme88k/dev/busswreg.h>
#include <mvme88k/dev/busswfunc.h>

struct bussw_softc {
	struct device		sc_dev;
	void *			sc_paddr;
	void *			sc_vaddr;
	struct intrhand 	sc_abih;	/* `abort' switch */
	struct bussw_reg        *sc_bussw;
};

void    bussw_attach(struct device *, struct device *, void *);
int     bussw_match(struct device *, void *, void *);

struct cfattach bussw_ca = {
	sizeof(struct bussw_softc), bussw_match, bussw_attach
};

struct cfdriver bussw_cd = {
	NULL, "bussw", DV_DULL
};

int bussw_print(void *args, const char *bus);
int bussw_scan(struct device *parent, void *child, void *args);
int busswabort(void *);

int
bussw_match(parent, vcf, args)
	struct device *parent;
	void *vcf, *args;
{
	struct confargs *ca = args;
	struct bussw_reg *bussw;

	/* Don't match if wrong cpu */
	if (brdtyp != BRD_197)
		return (0);

	bussw = (struct bussw_reg *)(IIOV(ca->ca_paddr));
	if (badvaddr((vaddr_t)bussw, 4))
		return (0);

	return (1);
}

void
bussw_attach(parent, self, args)
	struct device *parent, *self;
	void *args;
{
	struct confargs *ca = args;
	struct bussw_softc *sc = (struct bussw_softc *)self;
        struct bussw_reg *bs;

	sc->sc_vaddr = sc->sc_paddr = ca->ca_paddr;
	bs = sc->sc_bussw = (struct bussw_reg *)sc->sc_vaddr;
	bs->bs_intr2 |= BS_VECBASE;
	bs->bs_gcsr |= BS_GCSR_XIPL;
	/*
	 * pseudo driver, abort interrupt handler
   */
	sc->sc_abih.ih_fn = busswabort;
	sc->sc_abih.ih_arg = 0;
	sc->sc_abih.ih_wantframe = 1;
	sc->sc_abih.ih_ipl = IPL_NMI;	/* level 8!! */
	busswintr_establish(BS_ABORTIRQ, &sc->sc_abih);
	bs->bs_intr1 |= BS_INTR1_ABORT_IEN;

	printf(": rev %ld\n", BS_CHIPREV(bs));
	config_search(bussw_scan, self, args);
}

int
bussw_print(args, bus)
	void *args;
	const char *bus;
{
	struct confargs *ca = args;

	if (ca->ca_offset != -1)
		printf(" offset 0x%x", ca->ca_offset);
	if (ca->ca_ipl > 0)
		printf(" ipl %d", ca->ca_ipl);
	return (UNCONF);
}

int
bussw_scan(parent, child, args)
	struct device *parent;
	void *child, *args;
{
	struct cfdata *cf = child;
	struct bussw_softc *sc = (struct bussw_softc *)parent;
	struct confargs oca;

	bzero(&oca, sizeof oca);
	oca.ca_offset = cf->cf_loc[0];
	oca.ca_ipl = cf->cf_loc[1];
	if (((int)oca.ca_offset != -1) && ISIIOVA(sc->sc_vaddr + oca.ca_offset)) {
		oca.ca_vaddr = sc->sc_vaddr + oca.ca_offset;
		oca.ca_paddr = sc->sc_paddr + oca.ca_offset;
	} else {
		oca.ca_vaddr = (void *)-1;
		oca.ca_paddr = (void *)-1;
	}
	oca.ca_bustype = BUS_BUSSWITCH;
	oca.ca_master = (void *)sc->sc_bussw;
	oca.ca_name = cf->cf_driver->cd_name;
	if ((*cf->cf_attach->ca_match)(parent, cf, &oca) == 0)
		return (0);
	config_attach(parent, cf, &oca, bussw_print);
	return (1);
}

int
busswintr_establish(vec, ih)
	int vec;
	struct intrhand *ih;
{
	if (vec >= BS_NVEC) {
		printf("bussw: illegal vector: 0x%x\n", vec);
		panic("busswintr_establish");
	}
	return (intr_establish(BS_VECBASE+vec, ih));
}

int
busswabort(eframe)
	void *eframe;
{
	struct frame *frame = eframe;

	struct bussw_softc *sc = (struct bussw_softc *)bussw_cd.cd_devs[0];
        struct bussw_reg *bs  = sc->sc_bussw;

	if (bs->bs_intr1 & BS_INTR1_ABORT_INT) {
		bs->bs_intr1 |= BS_INTR1_ABORT_ICLR;
		nmihand(frame);
		return 1;
	}
	return 0;
}

