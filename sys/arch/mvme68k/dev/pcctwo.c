
/*	$OpenBSD: pcctwo.c,v 1.15 2005/11/24 22:43:16 miod Exp $ */

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
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <sys/fcntl.h>
#include <sys/device.h>
#include <machine/cpu.h>
#include <machine/autoconf.h>
#include <dev/cons.h>

#include <mvme68k/dev/pcctworeg.h>

struct pcctwosoftc {
	struct device	sc_dev;
	vaddr_t		sc_vaddr;	/* PCC2 space */
	paddr_t		sc_paddr;
	struct pcctworeg *sc_pcc2;	/* the actual registers */
};

void pcctwoattach(struct device *, struct device *, void *);
int  pcctwomatch(struct device *, void *, void *);
int  pcctwo_print(void *, const char *);
int  pcctwo_scan(struct device *, void *, void *);

struct cfattach pcctwo_ca = {
	sizeof(struct pcctwosoftc), pcctwomatch, pcctwoattach
};

struct cfdriver pcctwo_cd = {
	NULL, "pcctwo", DV_DULL
};

struct pcctworeg *sys_pcc2 = NULL;

int
pcctwomatch(parent, vcf, args)
	struct device *parent;
	void *vcf, *args;
{
	struct confargs *ca = args;
	struct pcctworeg *pcc2;

	/* the PCC2 only exists on MVME16x's except the 162, right? */
	if (cputyp == CPU_162 || cputyp == CPU_147 || cputyp == CPU_172)
		return (0);
	pcc2 = (struct pcctworeg *)(IIOV(ca->ca_paddr) + PCC2_PCC2CHIP_OFF);
	if (badvaddr((vaddr_t)pcc2, 1) || pcc2->pcc2_chipid != PCC2_CHIPID)
		return (0);
	return (1);
}

int
pcctwo_print(args, bus)
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
pcctwo_scan(parent, child, args)
	struct device *parent;
	void *child, *args;
{
	struct cfdata *cf = child;
	struct pcctwosoftc *sc = (struct pcctwosoftc *)parent;
	struct confargs oca;

	bzero(&oca, sizeof oca);
	oca.ca_offset = cf->cf_loc[0];
	oca.ca_ipl = cf->cf_loc[1];
	if (oca.ca_offset != -1 && ISIIOVA(sc->sc_vaddr + oca.ca_offset)) {
		oca.ca_vaddr = sc->sc_vaddr + oca.ca_offset;
		oca.ca_paddr = sc->sc_paddr + oca.ca_offset;
	} else {
		oca.ca_vaddr = (vaddr_t)-1;
		oca.ca_paddr = (paddr_t)-1;
	}
	oca.ca_bustype = BUS_PCCTWO;
	oca.ca_name = cf->cf_driver->cd_name;
	if ((*cf->cf_attach->ca_match)(parent, cf, &oca) == 0)
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

	if (sys_pcc2)
		panic("pcc2 already attached!");

	/*
	 * since we know ourself to land in intiobase land,
	 * we must adjust our address
	 */
	sc->sc_paddr = ca->ca_paddr;
	sc->sc_vaddr = IIOV(sc->sc_paddr);
	sc->sc_pcc2 = (struct pcctworeg *)(sc->sc_vaddr + PCC2_PCC2CHIP_OFF);
	sys_pcc2 = sc->sc_pcc2;

	printf(": rev %d\n", sc->sc_pcc2->pcc2_chiprev);

	sc->sc_pcc2->pcc2_vecbase = PCC2_VECBASE;
	sc->sc_pcc2->pcc2_genctl |= PCC2_GENCTL_IEN;	/* global irq enable */

	config_search(pcctwo_scan, self, args);
}

/*
 * PCC2 interrupts land in a PCC2_NVEC sized hole starting at PCC2_VECBASE
 */
int
pcctwointr_establish(vec, ih, name)
	int vec;
	struct intrhand *ih;
	const char *name;
{
#ifdef DIAGNOSTIC
	if (vec < 0 || vec >= PCC2_NVEC)
		panic("pcctwointr_establish: illegal vector for %s: 0x%x",
		    name, vec);
#endif

	return intr_establish(PCC2_VECBASE + vec, ih, name);
}
