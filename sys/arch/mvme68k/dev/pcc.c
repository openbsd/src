/*	$OpenBSD: pcc.c,v 1.6 2000/03/26 23:31:59 deraadt Exp $ */

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
 * VME147 peripheral channel controller
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

#include <mvme68k/dev/pccreg.h>

struct pccsoftc {
	struct device	sc_dev;
	void		*sc_vaddr;
	void		*sc_paddr;
	struct pccreg	*sc_pcc;
	struct intrhand	sc_nmiih;
};

void pccattach __P((struct device *, struct device *, void *));
int  pccmatch __P((struct device *, void *, void *));
int  pccabort __P((struct frame *));

struct cfattach pcc_ca = {
	sizeof(struct pccsoftc), pccmatch, pccattach
};

struct cfdriver pcc_cd = {
	NULL, "pcc", DV_DULL, 0
};

struct pccreg *sys_pcc = NULL;

int
pccmatch(parent, vcf, args)
	struct device *parent;
	void *vcf, *args;
{
	struct cfdata *cf = vcf;
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

	if (parent->dv_cfdata->cf_driver->cd_indirect) {
                printf(" indirect devices not supported\n");
                return 0;
        }

	bzero(&oca, sizeof oca);
	oca.ca_offset = cf->cf_loc[0];
	oca.ca_ipl = cf->cf_loc[1];
	if (oca.ca_offset != -1) {
		oca.ca_vaddr = sc->sc_vaddr + oca.ca_offset;
		oca.ca_paddr = sc->sc_paddr + oca.ca_offset;
	} else {
		oca.ca_vaddr = (void *)-1;
		oca.ca_paddr = (void *)-1;
	}	
	oca.ca_bustype = BUS_PCC;
	oca.ca_master = (void *)sc->sc_pcc;
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
	int i;

	if (sys_pcc)
		panic("pcc already attached!");

	/*
	 * since we know ourself to land in intiobase land,
	 * we must adjust our address
	 */
	sc->sc_paddr = ca->ca_paddr;
	sc->sc_vaddr = (void *)IIOV(sc->sc_paddr);
	sc->sc_pcc = (struct pccreg *)(sc->sc_vaddr + PCCSPACE_PCCCHIP_OFF);
	sys_pcc = sc->sc_pcc;

	printf(": rev %d\n", sc->sc_pcc->pcc_chiprev);

	sc->sc_nmiih.ih_fn = pccabort;
	sc->sc_nmiih.ih_arg = 0;
	sc->sc_nmiih.ih_ipl = 7;
	sc->sc_nmiih.ih_wantframe = 1;
	pccintr_establish(PCCV_ABORT, &sc->sc_nmiih);

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
pccintr_establish(vec, ih)
	int vec;
	struct intrhand *ih;
{
	if (vec >= PCC_NVEC) {
		printf("pcc: illegal vector: 0x%x\n", vec);
		panic("pccintr_establish");
	}
	return (intr_establish(PCC_VECBASE+vec, ih));
}

int
pccabort(frame)
	struct frame *frame;
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

	/*printf("counting...lim = %d\n", lim);*/

	pcc->pcc_t1irq = 0;		/* just in case */
	pcc->pcc_t1pload = 0;
	pcc->pcc_t1ctl = PCC_TIMERCLEAR;
	pcc->pcc_t1ctl = PCC_TIMERSTART;
	
	cnt = 0;
	while (1) {
		tmp = pcc->pcc_t1count;
		if (tmp > lim)
			break;
		tmp = lim;
		cnt++;
	}

	pcc->pcc_t1ctl = PCC_TIMERCLEAR;
	printf("pccspeed cnt=%d\n", cnt);

	/*
	 * Imperically determined. Unfortunately, because of various
	 * memory board effects and such, it is rather unlikely that
	 * we will find a nice formula.
	 */
	if (cnt > 230000)
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
