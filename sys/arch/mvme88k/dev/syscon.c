/*	$OpenBSD: syscon.c,v 1.29 2010/09/20 06:33:47 matthew Exp $ */
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

#include <machine/mvme188.h>
#include <mvme88k/dev/sysconvar.h>

struct sysconsoftc {
	struct device	sc_dev;

	struct intrhand sc_abih;	/* `abort' switch */
	struct intrhand sc_acih;	/* `ac fail' */
	struct intrhand sc_sfih;	/* `sys fail' */
};

void	sysconattach(struct device *, struct device *, void *);
int	sysconmatch(struct device *, void *, void *);

int	syscon_print(void *, const char *);
int	syscon_scan(struct device *, void *, void *);
int	sysconabort(void *);
int	sysconacfail(void *);
int	sysconsysfail(void *);

struct cfattach syscon_ca = {
	sizeof(struct sysconsoftc), sysconmatch, sysconattach
};

struct cfdriver syscon_cd = {
	NULL, "syscon", DV_DULL
};

int
sysconmatch(struct device *parent, void *cf, void *args)
{
	/* Don't match if wrong cpu */
	if (brdtyp != BRD_188)
		return (0);

	return (syscon_cd.cd_ndevs == 0);
}

void
sysconattach(struct device *parent, struct device *self, void *args)
{
	struct sysconsoftc *sc = (struct sysconsoftc *)self;
	int i;

	printf("\n");

	/*
	 * Set up interrupt handlers.
	 */
	for (i = 0; i < INTSRC_VME; i++)
		SLIST_INIT(&sysconintr_handlers[i]);

	/*
	 * Clear SYSFAIL if lit.
	 */
	*(volatile u_int32_t *)MVME188_UCSR |= UCSR_DRVSFBIT;

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

	sysconintr_establish(INTSRC_ABORT, &sc->sc_abih, "abort");
	sysconintr_establish(INTSRC_ACFAIL, &sc->sc_acih, "acfail");
	sysconintr_establish(INTSRC_SYSFAIL, &sc->sc_sfih, "sysfail");

	config_search(syscon_scan, self, args);
}

int
syscon_scan(struct device *parent, void *child, void *args)
{
	struct cfdata *cf = child;
	struct confargs oca, *ca = args;

	bzero(&oca, sizeof oca);
	oca.ca_iot = ca->ca_iot;
	oca.ca_dmat = ca->ca_dmat;
	oca.ca_offset = cf->cf_loc[0];
	oca.ca_ipl = cf->cf_loc[1];
	if (oca.ca_offset != -1)
		oca.ca_paddr = ca->ca_paddr + oca.ca_offset;
	else
		oca.ca_paddr = -1;
	oca.ca_bustype = BUS_SYSCON;
	oca.ca_name = cf->cf_driver->cd_name;

	if ((*cf->cf_attach->ca_match)(parent, cf, &oca) == 0)
		return (0);

	config_attach(parent, cf, &oca, syscon_print);
	return (1);
}

int
syscon_print(void *args, const char *bus)
{
	struct confargs *ca = args;

	if (ca->ca_offset != -1)
		printf(" offset 0x%x", ca->ca_offset);
	if (ca->ca_ipl > 0)
		printf(" ipl %d", ca->ca_ipl);
	return (UNCONF);
}

/*
 * Interrupt related code
 */

intrhand_t sysconintr_handlers[INTSRC_VME];

int
sysconintr_establish(u_int intsrc, struct intrhand *ih, const char *name)
{
	intrhand_t *list;

	list = &sysconintr_handlers[intsrc];
	if (!SLIST_EMPTY(list)) {
#ifdef DIAGNOSTIC
		printf("%s: interrupt source %u already registered\n",
		    __func__, intsrc);
#endif
		return (EINVAL);
	}

	evcount_attach(&ih->ih_count, name, &ih->ih_ipl);
	SLIST_INSERT_HEAD(list, ih, ih_link);

	syscon_intsrc_enable(intsrc, ih->ih_ipl);

	return (0);
}

void
sysconintr_disestablish(u_int intsrc, struct intrhand *ih)
{
	intrhand_t *list;

	list = &sysconintr_handlers[intsrc];
	evcount_detach(&ih->ih_count);
	SLIST_REMOVE(list, ih, intrhand, ih_link);

	syscon_intsrc_disable(intsrc);
}

/* Interrupt masks per logical interrupt source */
const u_int32_t syscon_intsrc[] = {
	0,
	IRQ_ABORT,
	IRQ_ACF,
	IRQ_SF,
	IRQ_CIOI,
	IRQ_DTI,
	IRQ_DI,
	IRQ_VME1,
	IRQ_VME2,
	IRQ_VME3,
	IRQ_VME4,
	IRQ_VME5,
	IRQ_VME6,
	IRQ_VME7
};

void
syscon_intsrc_enable(u_int intsrc, int ipl)
{
	u_int32_t psr;
	u_int32_t intmask = syscon_intsrc[intsrc];
	int i;

	psr = get_psr();
	set_psr(psr | PSR_IND);

	for (i = IPL_NONE; i < ipl; i++)
		int_mask_val[i] |= intmask;

	setipl(getipl());

	set_psr(psr);
}

void
syscon_intsrc_disable(u_int intsrc)
{
	u_int32_t psr;
	u_int32_t intmask = syscon_intsrc[intsrc];
	int i;

	psr = get_psr();
	set_psr(psr | PSR_IND);

	for (i = 0; i < NIPLS; i++)
		int_mask_val[i] &= ~intmask;

	setipl(getipl());

	set_psr(psr);
}

int
sysconabort(void *eframe)
{
	*(volatile u_int32_t *)MVME188_CLRINT = ISTATE_ABORT;
	nmihand(eframe);
	return (1);
}

int
sysconsysfail(void *eframe)
{
	*(volatile u_int32_t *)MVME188_CLRINT = ISTATE_SYSFAIL;
	printf("WARNING: SYSFAIL* ASSERTED\n");
	return (1);
}

int
sysconacfail(void *eframe)
{
	*(volatile u_int32_t *)MVME188_CLRINT = ISTATE_ACFAIL;
	printf("WARNING: ACFAIL* ASSERTED\n");
	return (1);
}
