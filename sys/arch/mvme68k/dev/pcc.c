/*	$OpenBSD: pcc.c,v 1.18 2010/06/26 23:24:43 guenther Exp $ */

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
 * VME147 peripheral channel controller
 */
#include <sys/param.h>
#include <sys/conf.h>
#include <sys/ioctl.h>
#include <sys/proc.h>
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

#include <mvme68k/dev/pccreg.h>

struct pccsoftc {
	struct device	sc_dev;
	vaddr_t		sc_vaddr;
	paddr_t		sc_paddr;
	struct pccreg	*sc_pcc;
	struct intrhand	sc_nmiih;
};

void pccattach(struct device *, struct device *, void *);
int  pccmatch(struct device *, void *, void *);
int  pccabort(void *);
int  pcc_print(void *, const char *);
int  pcc_scan(struct device *, void *, void *);

struct cfattach pcc_ca = {
	sizeof(struct pccsoftc), pccmatch, pccattach
};

struct cfdriver pcc_cd = {
	NULL, "pcc", DV_DULL
};

struct pccreg *sys_pcc = NULL;

int
pccmatch(parent, vcf, args)
	struct device *parent;
	void *vcf, *args;
{
	struct confargs *ca = args;

	/* the pcc only exist on vme147's */
	if (cputyp != CPU_147)
		return (0);
	return (!badvaddr(IIOV(ca->ca_paddr) + PCCSPACE_PCCCHIP_OFF, 1));
}

int
pcc_print(args, bus)
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
pcc_scan(parent, child, args)
	struct device *parent;
	void *child, *args;
{
	struct cfdata *cf = child;
	struct pccsoftc *sc = (struct pccsoftc *)parent;
	struct confargs *ca = args;
	struct confargs oca;

	bzero(&oca, sizeof oca);
	oca.ca_iot = ca->ca_iot;
	oca.ca_dmat = ca->ca_dmat;
	oca.ca_offset = cf->cf_loc[0];
	oca.ca_ipl = cf->cf_loc[1];
	if (oca.ca_offset != -1) {
		oca.ca_vaddr = sc->sc_vaddr + oca.ca_offset;
		oca.ca_paddr = sc->sc_paddr + oca.ca_offset;
	} else {
		oca.ca_vaddr = (vaddr_t)-1;
		oca.ca_paddr = (paddr_t)-1;
	}	
	oca.ca_bustype = BUS_PCC;
	oca.ca_name = cf->cf_driver->cd_name;
	if ((*cf->cf_attach->ca_match)(parent, cf, &oca) == 0)
		return (0);
	config_attach(parent, cf, &oca, pcc_print);
	return (1);
}

void
pccattach(parent, self, args)
	struct device *parent, *self;
	void *args;
{
	struct confargs *ca = args;
	struct pccsoftc *sc = (struct pccsoftc *)self;

	if (sys_pcc)
		panic("pcc already attached!");

	/*
	 * since we know ourself to land in intiobase land,
	 * we must adjust our address
	 */
	sc->sc_paddr = ca->ca_paddr;
	sc->sc_vaddr = IIOV(sc->sc_paddr);
	sc->sc_pcc = (struct pccreg *)(sc->sc_vaddr + PCCSPACE_PCCCHIP_OFF);
	sys_pcc = sc->sc_pcc;

	printf(": rev %d\n", sc->sc_pcc->pcc_chiprev);

	sc->sc_nmiih.ih_fn = pccabort;
	sc->sc_nmiih.ih_ipl = 7;
	sc->sc_nmiih.ih_wantframe = 1;
	pccintr_establish(PCCV_ABORT, &sc->sc_nmiih, self->dv_xname);

	sc->sc_pcc->pcc_vecbase = PCC_VECBASE;
	sc->sc_pcc->pcc_abortirq = PCC_ABORT_IEN | PCC_ABORT_ACK;
	sc->sc_pcc->pcc_genctl |= PCC_GENCTL_IEN;

	/* XXX further init of PCC chip? */

	config_search(pcc_scan, self, args);
}

/*
 * PCC interrupts land in a PCC_NVEC sized hole starting at PCC_VECBASE
 */
int
pccintr_establish(vec, ih, name)
	int vec;
	struct intrhand *ih;
	const char *name;
{
#ifdef DIAGNOSTIC
	if (vec < 0 || vec >= PCC_NVEC)
		panic("pccintr_establish: illegal vector for %s: 0x%x",
		    name, vec);
#endif

	return intr_establish(PCC_VECBASE + vec, ih, name);
}

int
pccabort(frame)
	void *frame;
{
#if 0
	/* XXX wait for it to debounce -- there is something wrong here */
	while (sys_pcc->pcc_abortirq & PCC_ABORT_ABS)
		;
	delay(2);
#endif
	sys_pcc->pcc_abortirq = PCC_ABORT_IEN | PCC_ABORT_ACK;
	nmihand(frame);
	return (1);
}

int
pccspeed(pcc)
	struct pccreg *pcc;
{
	volatile u_short lim = pcc_timer_us2lim(400);
	volatile u_short tmp;
	volatile int cnt;
	int speed;

	pcc->pcc_t1irq = 0;		/* just in case */
	pcc->pcc_t1pload = 0;
	pcc->pcc_t1ctl = PCC_TIMERCLEAR;
	pcc->pcc_t1ctl = PCC_TIMERSTART;
	
	cnt = 0;
	for (;;) {
		tmp = pcc->pcc_t1count;
		if (tmp > lim)
			break;
		tmp = lim;
		cnt++;
	}

	pcc->pcc_t1ctl = PCC_TIMERCLEAR;
	printf("pccspeed cnt=%d\n", cnt);

	/*
	 * Empirically determined. Unfortunately, because of various
	 * memory board effects and such, it is rather unlikely that
	 * we will find a nice formula.
	 */
	if (cnt > 280000)
		speed = 50;
	else if (cnt > 210000)
		speed = 33;
	else if (cnt > 190000)
		speed = 25;
	else if (cnt > 170000)	/* 171163, 170335 */
		speed = 20;
	else
		speed = 16;
	return (speed);
}
