/*	$OpenBSD: dz_ibus.c,v 1.1 2000/04/27 02:34:50 bjc Exp $	*/
/*	$NetBSD: dz_ibus.c,v 1.15 1999/08/27 17:50:42 ragge Exp $ */
/*
 * Copyright (c) 1998 Ludd, University of Lule}, Sweden.
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
 *      This product includes software developed at Ludd, University of 
 *      Lule}, Sweden and its contributors.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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



#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/file.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/reboot.h>

#include <dev/cons.h>

#include <machine/mtpr.h>
#include <machine/sid.h>
#include <machine/uvax.h>
#include <machine/vsbus.h>
#include <machine/cpu.h>
#include <machine/scb.h>

#include <machine/../vax/gencons.h>

#include "../qbus/dzreg.h"
#include "../qbus/dzvar.h"

#include "lkc.h"

static  int     dz_vsbus_match __P((struct device *, struct cfdata *, void *));
static  void    dz_vsbus_attach __P((struct device *, struct device *, void *));
static	int	dz_print __P((void *, const char *));

static	vaddr_t dz_regs; /* Used for console */

struct  cfattach dz_vsbus_ca = {
	sizeof(struct dz_softc), (cfmatch_t)dz_vsbus_match, dz_vsbus_attach
};

#define REG(name)     short name; short X##name##X;
static volatile struct ss_dz {/* base address of DZ-controller: 0x200A0000 */
	REG(csr);	/* 00 Csr: control/status register */
	REG(rbuf);	/* 04 Rbuf/Lpr: receive buffer/line param reg. */
	REG(tcr);	/* 08 Tcr: transmit console register */
	REG(tdr);	/* 0C Msr/Tdr: modem status reg/transmit data reg */
	REG(lpr0);	/* 10 Lpr0: */
	REG(lpr1);	/* 14 Lpr0: */
	REG(lpr2);	/* 18 Lpr0: */
	REG(lpr3);	/* 1C Lpr0: */
} *dz;
#undef REG

cons_decl(dz);

int
dz_print(aux, name)
	void *aux;
	const char *name;
{
	if (name)
		printf ("lkc at %s", name);
	return (UNCONF);
}

static int
dz_vsbus_match(parent, cf, aux)
	struct device *parent;
	struct cfdata *cf;
	void *aux;
{
	struct vsbus_attach_args *va = aux;
	struct ss_dz *dzP;
	short i;
	unsigned int n;

	dzP = (struct ss_dz *)va->va_addr;
	i = dzP->tcr;
	dzP->csr = DZ_CSR_MSE|DZ_CSR_TXIE;
	dzP->tcr = 0;
	DELAY(1000);
	dzP->tcr = 1;
	DELAY(100000);
	dzP->csr = DZ_CSR_MSE;
	DELAY(1000);
	dzP->tcr = 0;

	va->va_ivec = dzxint;

	/* If the device doesn't exist, no interrupt has been generated */
	
	return 1;
}

static void
dz_vsbus_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct  dz_softc *sc = (void *)self;
	struct vsbus_softc *vsc = (struct vsbus_softc *)parent;
	struct vsbus_attach_args *va = aux;

	/* 
	 * XXX - This is evil and ugly, but...
	 * due to the nature of how bus_space_* works on VAX, this will
	 * be perfectly good until everything is converted.
	 */
	sc->sc_ioh = dz_regs;
	sc->sc_dr.dr_csr = 0;
	sc->sc_dr.dr_rbuf = 4;
	sc->sc_dr.dr_dtr = 9;
	sc->sc_dr.dr_break = 13;
	sc->sc_dr.dr_tbuf = 12;
	sc->sc_dr.dr_tcr = 8;
	sc->sc_dr.dr_dcd = 13;
	sc->sc_dr.dr_ring = 13;

	sc->sc_type = DZ_DZV;

	vsc->sc_mask |= 1 << (va->va_maskno);
	vsc->sc_mask |= (1 << (va->va_maskno-1));

	sc->sc_dsr = 0x0f; /* XXX check if VS has modem ctrl bits */
	scb_vecalloc(va->va_cvec, dzxint, sc, SCB_ISTACK);
	scb_vecalloc(va->va_cvec - 4, dzrint, sc, SCB_ISTACK);
	printf("\n%s: 4 lines", self->dv_xname);

	dzattach(sc);

	if (((vax_confdata & 0x80) == 0) ||/* workstation, have lkc */
	    (vax_boardtype == VAX_BTYP_48))
		if (cn_tab->cn_pri > CN_NORMAL) /* Passed cnsl detect */
			config_found(self, 0, dz_print);
}

int
dzcngetc(dev) 
	dev_t dev;
{
	int c = 0;
	int mino = minor(dev);
	u_short rbuf;

	do {
		while ((dz->csr & 0x80) == 0)
			; /* Wait for char */
		rbuf = dz->rbuf;
		if (((rbuf >> 8) & 3) != mino)
			continue;
		c = rbuf & 0x7f;
	} while (c == 17 || c == 19);		/* ignore XON/XOFF */

	if (c == 13)
		c = 10;

	return (c);
}

#define	DZMAJOR	1

void
dzcnprobe(cndev)
	struct	consdev *cndev;
{
	extern	vaddr_t iospace;
	int diagcons;
	paddr_t ioaddr = 0x200A0000;

	switch (vax_boardtype) {
	case VAX_BTYP_410:
	case VAX_BTYP_420:
	case VAX_BTYP_43:
		diagcons = (vax_confdata & 0x20 ? 3 : 0);
		break;

	case VAX_BTYP_46:
	case VAX_BTYP_48:
		diagcons = (vax_confdata & 0x100 ? 3 : 0);
		break;

	case VAX_BTYP_49:
		ioaddr = 0x25000000;
		diagcons = 3;
		break;

	default:
		cndev->cn_pri = CN_DEAD;
		return;
	}
	if (diagcons)
		cndev->cn_pri = CN_REMOTE;
	else
		cndev->cn_pri = CN_NORMAL;
	cndev->cn_dev = makedev(DZMAJOR, diagcons);
	dz_regs = iospace;
	ioaccess(iospace, ioaddr, 1);
}

void
dzcninit(cndev)
	struct	consdev *cndev;
{
	dz = (void*)dz_regs;

	dz->csr = 0;    /* Disable scanning until initting is done */
	dz->tcr = (1 << minor(cndev->cn_dev));    /* Turn on xmitter */
	dz->csr = 0x20; /* Turn scanning back on */
}

void
dzcnputc(dev,ch)
	dev_t	dev;
	int	ch;
{
	register int timeout = 1<<15;       /* don't hang the machine! */
	register int s;
	int mino = minor(dev);
	u_short tcr;

	if (mfpr(PR_MAPEN) == 0)
		return;

	/*
	 * If we are past boot stage, dz* will interrupt,
	 * therefore we block.
	 */
	s = splhigh(); 
	tcr = dz->tcr;	/* remember which lines to scan */
	dz->tcr = (1 << mino);

	while ((dz->csr & 0x8000) == 0) /* Wait until ready */
		if (--timeout < 0)
			break;
	dz->tdr = ch;                    /* Put the character */
	timeout = 1<<15;
	while ((dz->csr & 0x8000) == 0) /* Wait until ready */
		if (--timeout < 0)
			break;

	dz->tcr = tcr;
	splx(s);
}

void 
dzcnpollc(dev, pollflag)
	dev_t dev;
	int pollflag;
{
	static	u_char mask;

	if (pollflag)
		mask = vsbus_setmask(0);
	else
		vsbus_setmask(mask);
}

#if NLKC
cons_decl(lkc);

void
lkccninit(cndev)
	struct	consdev *cndev;
{
	dz = (void*)dz_regs;

	dz->csr = 0;    /* Disable scanning until initting is done */
	dz->tcr = 1;    /* Turn off all but line 0's xmitter */
	dz->rbuf = 0x1c18; /* XXX */
	dz->csr = 0x20; /* Turn scanning back on */
}

int
lkccngetc(dev) 
	dev_t dev;
{
	int lkc_decode(int);
	int c;
#if 0
	u_char mask;

	
	mask = vsbus_setmask(0);	/* save old state */
#endif

loop:
	while ((dz->csr & 0x80) == 0)
		; /* Wait for char */

	c = lkc_decode(dz->rbuf & 255);
	if (c < 1)
		goto loop;

#if 0
	vsbus_clrintr(0x80); /* XXX */
	vsbus_setmask(mask);
#endif

	return (c);
}
#endif
