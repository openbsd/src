/*	$NetBSD$ */

/*
 * Copyright (c) 1995 Theo de Raadt
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
 *      This product includes software developed by Theo de Raadt
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
 * VME16x PCC2 chip
 */
#include <sys/param.h>
#include <sys/conf.h>
#include <sys/ioctl.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/tty.h>
#include <sys/uio.h>
#include <sys/callout.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <sys/fcntl.h>
#include <sys/device.h>
#include <machine/cpu.h>
#include <machine/autoconf.h>
#include <dev/cons.h>
#include <mvme68k/mvme68k/isr.h>

#include <mvme68k/dev/pcctworeg.h>

struct pcctwosoftc {
	struct device	sc_dev;
	caddr_t		sc_vaddr;
	caddr_t		sc_paddr;
	struct pcctworeg *sc_pcc2;
	struct intrhand	sc_nmiih;
};

void pcctwoattach __P((struct device *, struct device *, void *));
int  pcctwomatch __P((struct device *, void *, void *));
int  pcctwoabort __P((struct frame *));

struct cfdriver pcctwocd = {
	NULL, "pcctwo", pcctwomatch, pcctwoattach,
	DV_DULL, sizeof(struct pcctwosoftc), 0
};

struct pcctworeg *sys_pcc2 = NULL;

struct intrhand *pcctwointrs[PCC2_NVEC];

int
pcctwomatch(parent, vcf, args)
	struct device *parent;
	void *vcf, *args;
{
	struct cfdata *cf = vcf;
	struct confargs *ca = args;
	struct pcctworeg *pcc2;

	/* the PCC2 only exists on MVME16x's except the 162, right? */
	if (cputyp == CPU_162 || cputyp == CPU_147)
		return (0);
	pcc2 = (struct pcctworeg *)(IIOV(ca->ca_paddr) + PCC2_PCC2CHIP_OFF);
	if (badbaddr(pcc2))
		return (0);
	if (pcc2->pcc2_chipid != PCC2_CHIPID)
		return (0);
	return (1);
}

int
pcctwo_print(args, bus)
	void *args;
	const char *bus;
{
	struct confargs *ca = args;

	printf(" offset 0x%x", ca->ca_offset);
	if (ca->ca_ipl > 0)
		printf(" ipl %d", ca->ca_ipl);
	return (UNCONF);
}

int
pcctwo_scan(parent, child, args)
	struct device *parent;
	void *child, *args;
{
	struct cfdata *cf = child;
	struct pcctwosoftc *sc = (struct pcctwosoftc *)parent;
	struct confargs *ca = args;
	struct confargs oca;

	if (parent->dv_cfdata->cf_driver->cd_indirect) {
                printf(" indirect devices not supported\n");
                return 0;
        }

	bzero(&oca, sizeof oca);
	oca.ca_paddr = sc->sc_paddr + cf->cf_loc[0];
	if (ISIIOVA(sc->sc_vaddr + cf->cf_loc[0]))
		oca.ca_vaddr = sc->sc_vaddr + cf->cf_loc[0];
	else
		oca.ca_vaddr = (caddr_t)-1;
	oca.ca_offset = cf->cf_loc[0];
	oca.ca_ipl = cf->cf_loc[1];
	oca.ca_bustype = BUS_PCCTWO;
	oca.ca_master = (void *)sc->sc_pcc2;
	oca.ca_name = cf->cf_driver->cd_name;
	if ((*cf->cf_driver->cd_match)(parent, cf, &oca) == 0)
		return (0);
	config_attach(parent, cf, &oca, pcctwo_print);
	return (1);
}

void
pcctwoattach(parent, self, args)
	struct device *parent, *self;
	void *args;
{
	struct confargs *ca = args;
	struct pcctwosoftc *sc = (struct pcctwosoftc *)self;
	extern u_long vectab[], pcctwotrap;
	int i;

	if (sys_pcc2)
		panic("pcc2 already attached!");

	/*
	 * since we know ourself to land in intiobase land,
	 * we must adjust our address
	 */
	sc->sc_paddr = ca->ca_paddr;
	sc->sc_vaddr = (caddr_t)IIOV(sc->sc_paddr);
	sc->sc_pcc2 = (struct pcctworeg *)(sc->sc_vaddr + PCC2_PCC2CHIP_OFF);
	sys_pcc2 = sc->sc_pcc2;

	printf(": rev %d\n", sc->sc_pcc2->pcc2_chiprev);

	/*
	 * make the PCCTWO interrupt range point to the pcc2 trap routine.
	 */
	for (i = 0; i < PCC2_NVEC; i++) {
		vectab[PCC2_VECBASE+i] = (u_long)&pcctwotrap;
	}

	sc->sc_pcc2->pcc2_genctl |= PCC2_GENCTL_IEN;	/* global irq enable */

	sys_pcc2->pcc2_gpioirq  = PCC2_GPIO_PLTY | PCC2_IRQ_IEN | 0x7;/*lvl7*/ 
	sys_pcc2->pcc2_gpio = 0; /* do not turn on CR_O or CR_OE */
	sc->sc_nmiih.ih_fn = pcctwoabort;
	sc->sc_nmiih.ih_arg = 0;
	sc->sc_nmiih.ih_lvl = 7;
	sc->sc_nmiih.ih_wantframe = 1;
	/*sc->sc_mc->mc_abort .... enable at ipl 7 */
	pcctwointr_establish(PCC2V_GPIO, &sc->sc_nmiih);

	sc->sc_pcc2->pcc2_vecbase = PCC2_VECBASE;
	config_search(pcctwo_scan, self, args);
}

#ifndef PCCTWOINTR_ASM
/*
 * pcctwointr: called from locore with the PC and evec from the trap frame.
 */
int
pcctwointr(pc, evec, frame)
	int pc;
	int evec;
	void *frame;
{
	int vec = (evec & 0xfff) >> 2;	/* XXX should be m68k macro? */
	extern u_long intrcnt[];	/* XXX from locore */
	struct intrhand *ih;
	int r;

	vec = vec & 0xf;
	if (vec >= PCC2_NVEC)
		goto bail;

	cnt.v_intr++;
	for (ih = pcctwointrs[vec]; ih; ih = ih->ih_next) {
		if (ih->ih_wantframe)
			r = (*ih->ih_fn)(frame);
		else
			r = (*ih->ih_fn)(ih->ih_arg);
		if (r > 0)
			return;
	}
bail:
	return (straytrap(pc, evec));
}
#endif /* !PCCTWOINTR_ASM */

/*
 * pcctwointr_establish: establish pcctwo interrupt
 */
int
pcctwointr_establish(vec, ih)
	int vec;
	struct intrhand *ih;
{
	if (vec >= PCC2_NVEC) {
		printf("pcctwo: illegal vector: 0x%x\n", vec);
		panic("pcctwointr_establish");
	}

	/* XXX should attach at tail */
	ih->ih_next = pcctwointrs[vec];
	pcctwointrs[vec] = ih;
}
int
pcctwoabort(frame)
	struct frame *frame;
{
#ifdef REALLY_CARE_ABOUT_DEBOUNCE
	/* wait for it to debounce */
	while (sys_pcc2->pcc2_abortirq & PCC2_ABORT_ABS)
		;
#endif

	sys_pcc2->pcc2_gpioirq = sys_pcc2->pcc2_gpioirq | PCC2_IRQ_ICLR;

	nmihand(frame);
	return (1);
}
