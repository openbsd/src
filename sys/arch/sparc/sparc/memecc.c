/*	$OpenBSD: memecc.c,v 1.1 2014/11/22 22:48:38 miod Exp $	*/
/*	$NetBSD: memecc.c,v 1.16 2013/10/19 19:40:23 mrg Exp $	*/

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Paul Kranenburg.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * ECC memory control.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/autoconf.h>
#include <sparc/sparc/memeccreg.h>

struct memecc_softc {
	struct device	sc_dev;
	vaddr_t		sc_reg;
};

struct memecc_softc *memecc_sc;

/* autoconfiguration driver */
void	memecc_attach(struct device *, struct device *, void *);
int	memecc_match(struct device *, void *, void *);
int	memecc_error(void);

extern int (*memerr_handler)(void);

const struct cfattach eccmemctl_ca = {
	sizeof(struct memecc_softc),
	memecc_match,
	memecc_attach
};

struct cfdriver eccmemctl_cd = {
	0, "eccmemctl", DV_DULL
};

int
memecc_match(struct device *parent, void *vcf, void *aux)
{
	struct confargs *ca = aux;

	if (memerr_handler != NULL)
		return 0;

	return (strcmp("eccmemctl", ca->ca_ra.ra_name) == 0);
}

/*
 * Attach the device.
 */
void
memecc_attach(struct device *parent, struct device *self, void *aux)
{
	struct memecc_softc *sc = (struct memecc_softc *)self;
	struct confargs *ca = aux;
	struct romaux *ra = &ca->ca_ra;
	uint32_t reg;

	/* XXX getprop width, mc-type */
	sc->sc_reg = ra->ra_vaddr != NULL ? (vaddr_t)ra->ra_vaddr :
	    (vaddr_t)mapiodev(ra->ra_reg, 0, 0x20);

	reg = *(volatile uint32_t *)(sc->sc_reg + ECC_EN_REG);

	printf(": version 0x%x/0x%x\n",
		(reg & ECC_EN_VER) >> 24,
		(reg & ECC_EN_IMPL) >> 28);

	/* Enable checking & interrupts */
	reg |= ECC_EN_EE | ECC_EN_EI;
	*(volatile uint32_t *)(sc->sc_reg + ECC_EN_REG) = reg;
	memecc_sc = sc;

	memerr_handler = memecc_error;
}

/*
 * Called if the MEMORY ERROR bit is set after a level 25 interrupt.
 */
int
memecc_error(void)
{
	uint32_t efsr, efar0, efar1;

	efsr = *(volatile uint32_t *)(memecc_sc->sc_reg + ECC_FSR_REG);
	efar0 = *(volatile uint32_t *)(memecc_sc->sc_reg + ECC_AFR0_REG);
	efar1 = *(volatile uint32_t *)(memecc_sc->sc_reg + ECC_AFR1_REG);
	printf("memory error:\n\tEFSR: %08x\n", efsr);
	printf("\tMBus transaction: %08x\n", efar0 & ~ECC_AFR_PAH);
	printf("\taddress: 0x%x%x\n", efar0 & ECC_AFR_PAH, efar1);
#if 0
	printf("\tmodule location: %s\n",
		prom_pa_location(efar1, efar0 & ECC_AFR_PAH));
#endif

	/* Unlock registers and clear interrupt */
	*(volatile uint32_t *)(memecc_sc->sc_reg + ECC_FSR_REG) = efsr;

	/* Return 0 if this was a correctable error */
	return ((efsr & ECC_FSR_CE) == 0);
}
