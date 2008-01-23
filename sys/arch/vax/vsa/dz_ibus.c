/*	$OpenBSD: dz_ibus.c,v 1.24 2008/01/23 16:37:57 jsing Exp $	*/
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
 *     This product includes software developed at Ludd, University of 
 *     Lule}, Sweden and its contributors.
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
#include <machine/nexus.h>
#include <machine/ka420.h>

#include <vax/vax/gencons.h>

#include <vax/qbus/dzreg.h>
#include <vax/qbus/dzvar.h>

#include <vax/dec/dzkbdvar.h>

#include "dzkbd.h"
#include "dzms.h"

static  int     dz_vsbus_match(struct device *, struct cfdata *, void *);
static  void    dz_vsbus_attach(struct device *, struct device *, void *);

static	vaddr_t dz_regs; /* Used for console */

struct  cfattach dz_vsbus_ca = {
	sizeof(struct dz_softc), (cfmatch_t)dz_vsbus_match, dz_vsbus_attach
};

#define REG(name)     short name; short X##name##X;
static volatile struct ss_dz {/* base address of DZ-controller: 0x200a0000 */
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
cdev_decl(dz);

int	dz_can_have_kbd(void);

extern int getmajor(void *);	/* conf.c */

#if NDZKBD > 0 || NDZMS > 0
static int
dz_print(void *aux, const char *name)
{
	struct dzkm_attach_args *dz_args = aux;

	if (name != NULL)
		printf(dz_args->daa_line == 0 ? "lkkbd at %s" : "lkms at %s",
		    name);
	else
		printf(" line %d", dz_args->daa_line);

	return (UNCONF);
}
#endif

static int
dz_vsbus_match(parent, cf, aux)
	struct device *parent;
	struct cfdata *cf;
	void *aux;
{
	struct vsbus_attach_args *va = aux;
	struct ss_dz *dzP;
	short i;

#if VAX53 || VAX49
	if (vax_boardtype == VAX_BTYP_49 ||
	    vax_boardtype == VAX_BTYP_1303)
		if (cf->cf_loc[0] != 0x25000000)
			return 0; /* don't probe unnecessarily */
#endif

	dzP = (struct ss_dz *)va->va_addr;
	i = dzP->tcr;
	dzP->csr = DZ_CSR_MSE|DZ_CSR_TXIE;
	dzP->tcr = 0;
	DELAY(1000);
	dzP->tcr = 1;
	DELAY(100000);
	dzP->tcr = i;

	/* If the device doesn't exist, no interrupt has been generated */
	
	return 1;
}

static void
dz_vsbus_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct dz_softc *sc = (void *)self;
	struct vsbus_attach_args *va = aux;
#if NDZKBD > 0 || NDZMS > 0
	struct dzkm_attach_args daa;
#endif

	printf(": ");

	/* 
	 * XXX - This is evil and ugly, but...
	 * due to the nature of how bus_space_* works on VAX, this will
	 * be perfectly good until everything is converted.
	 */

	if (dz_regs == 0) /* This isn't console */
		dz_regs = vax_map_physmem(va->va_paddr, 1);
	else
		printf("console, ");

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

	sc->sc_dsr = 0x0f; /* XXX check if VS has modem ctrl bits */

	sc->sc_rcvec = va->va_cvec;
	scb_vecalloc(sc->sc_rcvec, dzxint, sc, SCB_ISTACK,
	    &sc->sc_tintrcnt);
	sc->sc_tcvec = va->va_cvec - 4;
	scb_vecalloc(sc->sc_tcvec, dzrint, sc, SCB_ISTACK,
	    &sc->sc_rintrcnt);
	evcount_attach(&sc->sc_rintrcnt, sc->sc_dev.dv_xname,
	    (void *)&sc->sc_rcvec, &evcount_intr);
	evcount_attach(&sc->sc_tintrcnt, sc->sc_dev.dv_xname,
	    (void *)&sc->sc_tcvec, &evcount_intr);

	printf("4 lines");

	dzattach(sc);

	if (dz_can_have_kbd()) {
#if NDZKBD > 0
		extern struct consdev wsdisplay_cons;

		dz->rbuf = DZ_LPR_RX_ENABLE | (DZ_LPR_B4800 << 8) 
		    | DZ_LPR_8_BIT_CHAR;
		daa.daa_line = 0;
		daa.daa_flags =
		    (cn_tab == &wsdisplay_cons ? DZKBD_CONSOLE : 0);
		config_found(self, &daa, dz_print);
#endif
#if NDZMS > 0
		dz->rbuf = DZ_LPR_RX_ENABLE | (DZ_LPR_B4800 << 8) |
		    DZ_LPR_8_BIT_CHAR | DZ_LPR_PARENB | DZ_LPR_OPAR |
		    1 /* line */;
		daa.daa_line = 1;
		daa.daa_flags = 0;
		config_found(self, &daa, dz_print);
#endif
	}

#if 0
	s = spltty();
	dzrint(sc);
	dzxint(sc);
	splx(s);
#endif
}

int
dzcngetc(dev) 
	dev_t dev;
{
	int c = 0, s;
	int mino = minor(dev);
	u_short rbuf;

	s = spltty();
	do {
		while ((dz->csr & DZ_CSR_RX_DONE) == 0)
			; /* Wait for char */
		rbuf = dz->rbuf;
		if (((rbuf >> 8) & 3) != mino)
			continue;
		c = rbuf & 0x7f;
	} while (c == 17 || c == 19);		/* ignore XON/XOFF */
	splx(s);

	if (c == 13)
		c = 10;

	return (c);
}

int
dz_can_have_kbd()
{
	switch (vax_boardtype) {
	case VAX_BTYP_410:
	case VAX_BTYP_420:
	case VAX_BTYP_43:
		if ((vax_confdata & KA420_CFG_MULTU) == 0)
			return (1);
		break;

	case VAX_BTYP_46:
		if ((vax_siedata & 0xff) == VAX_VTYP_46)
			return (1);
		break;
	case VAX_BTYP_48:
		if (((vax_siedata >> 8) & 0xff) == VAX_STYP_48)
			return (1);
		break;

	case VAX_BTYP_49:
		return (1);

	default:
		break;
	}

	return (0);
}

void
dzcnprobe(cndev)
	struct	consdev *cndev;
{
	extern	vaddr_t iospace;
	int diagcons, major;
	paddr_t ioaddr = 0x200a0000;

	if ((major = getmajor(dzopen)) < 0)
		return;

	switch (vax_boardtype) {
	case VAX_BTYP_410:
	case VAX_BTYP_420:
	case VAX_BTYP_43:
		diagcons = (vax_confdata & KA420_CFG_L3CON ? 3 : 0);
		break;

	case VAX_BTYP_46:
	case VAX_BTYP_48:
		diagcons = (vax_confdata & 0x100 ? 3 : 0);
		break;

	case VAX_BTYP_49:
		ioaddr = 0x25000000;
		diagcons = (vax_confdata & 8 ? 3 : 0);
		break;

	case VAX_BTYP_1303:
		ioaddr = 0x25000000;
		diagcons = 3;
		break;

	default:
		return;
	}
	cndev->cn_pri = diagcons != 0 ? CN_HIGHPRI : CN_LOWPRI;
	cndev->cn_dev = makedev(major, dz_can_have_kbd() ? 3 : diagcons);
	dz_regs = iospace;
	dz = (void *)dz_regs;
	ioaccess(iospace, ioaddr, 1);
}

void
dzcninit(cndev)
	struct	consdev *cndev;
{
	dz = (void *)dz_regs;

	dz->csr = 0;    /* Disable scanning until initting is done */
	dz->tcr = (1 << minor(cndev->cn_dev));    /* Turn on xmitter */
	dz->csr = DZ_CSR_MSE; /* Turn scanning back on */
}

void
dzcnputc(dev,ch)
	dev_t	dev;
	int	ch;
{
	int timeout = 1<<15;       /* don't hang the machine! */
	int s;
	int mino = minor(dev);
	u_short tcr;

	if (mfpr(PR_MAPEN) == 0)
		return;

	/*
	 * If we are past boot stage, dz* will interrupt,
	 * therefore we block.
	 */
	s = spltty(); 
	tcr = dz->tcr;	/* remember which lines to scan */
	dz->tcr = (1 << mino);

	while ((dz->csr & DZ_CSR_TX_READY) == 0) /* Wait until ready */
		if (--timeout < 0)
			break;
	dz->tdr = ch;                    /* Put the character */
	timeout = 1<<15;
	while ((dz->csr & DZ_CSR_TX_READY) == 0) /* Wait until ready */
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

#if NDZKBD > 0 || NDZMS > 0
int
dzgetc(struct dz_linestate *ls)
{
	int line;
	int s;
	u_short rbuf;

	if (ls != NULL)
		line = ls->dz_line;
	else
		line = 0;	/* keyboard */

	s = spltty();
	for (;;) {
		for(; (dz->csr & DZ_CSR_RX_DONE) == 0;)
			;
		rbuf = dz->rbuf;
		if (((rbuf >> 8) & 3) == line) {
			splx(s);
			return (rbuf & 0xff);
		}
	}
}

void
dzputc(struct dz_linestate *ls, int ch)
{
	int line;
	u_short tcr;
	int s;

	/* if the dz has already been attached, the MI
	   driver will do the transmitting: */
	if (ls && ls->dz_sc) {
		s = spltty();
		line = ls->dz_line;
		putc(ch, &ls->dz_tty->t_outq);
		tcr = dz->tcr;
		if (!(tcr & (1 << line)))
			dz->tcr = tcr | (1 << line);
		dzxint(ls->dz_sc);
		splx(s);
		return;
	}
	/* use dzcnputc to do the transmitting: */
	dzcnputc(makedev(getmajor(dzopen), 0), ch);
}
#endif /* NDZKBD > 0 || NDZMS > 0 */
