/*	$OpenBSD: syscon.c,v 1.17 2004/04/14 23:27:11 miod Exp $ */
/*
 * Copyright (c) 1999 Steve Murphree, Jr.
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
 * VME188 SYSCON
 */

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>

#include <machine/autoconf.h>
#include <machine/cpu.h>
#include <machine/board.h>
#include <machine/frame.h>

#include <mvme88k/dev/sysconfunc.h>
#include <mvme88k/dev/sysconreg.h>

struct sysconreg syscon_reg = {
   (unsigned int *volatile)IEN0_REG,	(unsigned int *volatile)IEN1_REG,
   (unsigned int *volatile)IEN2_REG,	(unsigned int *volatile)IEN3_REG,
   (unsigned int *volatile)IENALL_REG,	(unsigned int *volatile)IST_REG,
   (unsigned int *volatile)SETSWI_REG,	(unsigned int *volatile)CLRSWI_REG,
   (unsigned int *volatile)ISTATE_REG,	(unsigned int *volatile)CLRINT_REG,
   (unsigned char *volatile)GLB0,	(unsigned char *volatile)GLB1,
   (unsigned char *volatile)GLB2,	(unsigned char *volatile)GLB3,
   (unsigned int *volatile)UCSR_REG,	(unsigned int *volatile)GLBRES_REG,
   (unsigned int *volatile)CCSR_REG,	(unsigned int *volatile)ERROR_REG,
   (unsigned int *volatile)PCNFA_REG,	(unsigned int *volatile)PCNFB_REG,
   (unsigned int *volatile)EXTAD_REG,	(unsigned int *volatile)EXTAM_REG,
   (unsigned int *volatile)WHOAMI_REG,	(unsigned int *volatile)WMAD_REG,
   (unsigned int *volatile)RMAD_REG,	(unsigned int *volatile)WVAD_REG,
   (unsigned int *volatile)RVAD_REG,	(unsigned int *volatile)CIO_PORTC,
   (unsigned int *volatile)CIO_PORTB,	(unsigned int *volatile)CIO_PORTA,
   (unsigned int *volatile)CIO_CTRL
   };

struct sysconsoftc {
	struct device	sc_dev;
	void		*sc_vaddr;	/* Utility I/O space */
	void		*sc_paddr;
	struct sysconreg *sc_syscon;	/* the actual registers */
	struct intrhand sc_abih;	/* `abort' switch */
	struct intrhand sc_acih;	/* `ac fail' */
	struct intrhand sc_sfih;	/* `sys fail' */
	struct intrhand sc_m188ih;	/* `m188 interrupt' */
};

void sysconattach(struct device *, struct device *, void *);
int  sysconmatch(struct device *, void *, void *);
void setupiackvectors(void);
int  sysconabort(void *);
int  sysconacfail(void *);
int  sysconsysfail(void *);
int  sysconm188(void *);

struct cfattach syscon_ca = {
	sizeof(struct sysconsoftc), sysconmatch, sysconattach
};

struct cfdriver syscon_cd = {
	NULL, "syscon", DV_DULL
};

struct sysconreg *sys_syscon;

int syscon_print(void *args, const char *bus);
int syscon_scan(struct device *parent, void *child, void *args);

int
sysconmatch(parent, vcf, args)
	struct device *parent;
	void *vcf, *args;
{
	struct confargs *ca = args;
	struct sysconreg *syscon;

	/* Don't match if wrong cpu */
	if (brdtyp != BRD_188)
		return (0);

	/* Only allow one instance */
	if (sys_syscon != NULL)
		return (0);

	/*
	 * Uh, MVME188 better have on of these, so always match if it
	 * is a MVME188...
	 */
	syscon = (struct sysconreg *)(IIOV(ca->ca_paddr));
	return (1);
}

int
syscon_print(args, bus)
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
syscon_scan(parent, child, args)
	struct device *parent;
	void *child, *args;
{
	struct cfdata *cf = child;
	struct sysconsoftc *sc = (struct sysconsoftc *)parent;
	struct confargs oca;

	bzero(&oca, sizeof oca);
	oca.ca_offset = cf->cf_loc[0];
	oca.ca_ipl = cf->cf_loc[1];
	if ((oca.ca_offset != -1) && ISIIOVA(sc->sc_vaddr + oca.ca_offset)) {
		oca.ca_vaddr = sc->sc_vaddr + oca.ca_offset;
		oca.ca_paddr = sc->sc_paddr + oca.ca_offset;
	} else {
		oca.ca_vaddr = (void *)-1;
		oca.ca_paddr = (void *)-1;
	}
	oca.ca_bustype = BUS_SYSCON;
	oca.ca_master = (void *)sc->sc_syscon;
	oca.ca_name = cf->cf_driver->cd_name;
	if ((*cf->cf_attach->ca_match)(parent, cf, &oca) == 0)
		return (0);
	config_attach(parent, cf, &oca, syscon_print);
	return (1);
}

void
sysconattach(parent, self, args)
	struct device *parent, *self;
	void *args;
{
	struct confargs *ca = args;
	struct sysconsoftc *sc = (struct sysconsoftc *)self;

	/*
	 * since we know ourself to land in intiobase land,
	 * we must adjust our address
	 */
	sc->sc_paddr = ca->ca_paddr;
	sc->sc_vaddr = (void *)IIOV(sc->sc_paddr);
	sc->sc_syscon = &syscon_reg;
	sys_syscon = sc->sc_syscon;

	printf("\n");

	/*
	 * pseudo driver, abort interrupt handler
	 */
	sc->sc_abih.ih_fn = sysconabort;
	sc->sc_abih.ih_arg = 0;
	sc->sc_abih.ih_wantframe = 1;
	sc->sc_abih.ih_ipl = IPL_ABORT;

	sc->sc_acih.ih_fn = sysconacfail;
	sc->sc_acih.ih_arg = 0;
	sc->sc_acih.ih_wantframe = 1;
	sc->sc_acih.ih_ipl = IPL_ABORT;

	sc->sc_sfih.ih_fn = sysconsysfail;
	sc->sc_sfih.ih_arg = 0;
	sc->sc_sfih.ih_wantframe = 1;
	sc->sc_sfih.ih_ipl = IPL_ABORT;

	sc->sc_m188ih.ih_fn = sysconm188;
	sc->sc_m188ih.ih_arg = 0;
	sc->sc_m188ih.ih_wantframe = 1;
	sc->sc_m188ih.ih_ipl = IPL_ABORT;

	intr_establish(SYSCV_ABRT, &sc->sc_abih);
	intr_establish(SYSCV_ACF, &sc->sc_acih);
	intr_establish(SYSCV_SYSF, &sc->sc_sfih);
	intr_establish(M188_IVEC, &sc->sc_m188ih);

	config_search(syscon_scan, self, args);
}

int
sysconintr_establish(vec, ih)
	int vec;
	struct intrhand *ih;
{
	return (intr_establish(vec, ih));
}

int
sysconabort(eframe)
	void *eframe;
{
	ISR_RESET_NMI;
	nmihand((struct frame *)eframe);
	return (1);
}

int
sysconsysfail(eframe)
	void *eframe;
{
	ISR_RESET_SYSFAIL;
	nmihand((struct frame *)eframe);
	return (1);
}

int
sysconacfail(eframe)
	void *eframe;
{
	ISR_RESET_ACFAIL;
	nmihand((struct frame *)eframe);
	return (1);
}

int
sysconm188(eframe)
	void *eframe;
{
	printf("MVME188 interrupting?\n");
	return (1);
}
