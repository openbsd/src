/*	$OpenBSD: auxio.c,v 1.2 2001/08/29 02:47:58 jason Exp $	*/
/*	$NetBSD: auxio.c,v 1.1 2000/04/15 03:08:13 mrg Exp $	*/

/*
 * Copyright (c) 2000 Matthew R. Green
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * AUXIO registers support on the sbus & ebus2.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <machine/autoconf.h>
#include <machine/cpu.h>

#include <sparc64/dev/ebusreg.h>
#include <sparc64/dev/ebusvar.h>
#include <sparc64/dev/sbusvar.h>
#include <sparc64/dev/auxioreg.h>
#include <sparc64/dev/auxiovar.h>

#define	AUXIO_ROM_NAME		"auxio"

/*
 * ebus code.
 */
int	auxio_ebus_match __P((struct device *, void *, void *));
void	auxio_ebus_attach __P((struct device *, struct device *, void *));
int	auxio_sbus_match __P((struct device *, void *, void *));
void	auxio_sbus_attach __P((struct device *, struct device *, void *));

struct cfattach auxio_ebus_ca = {
	sizeof(struct auxio_softc), auxio_ebus_match, auxio_ebus_attach
};

struct cfattach auxio_sbus_ca = {
	sizeof(struct auxio_softc), auxio_sbus_match, auxio_sbus_attach
};

struct cfdriver auxio_cd = {
	NULL, "auxio", DV_DULL
};

int
auxio_ebus_match(parent, cf, aux)
	struct device *parent;
	void *cf;
	void *aux;
{
	struct ebus_attach_args *ea = aux;

	return (strcmp(AUXIO_ROM_NAME, ea->ea_name) == 0);
}

void
auxio_ebus_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct auxio_softc *sc = (struct auxio_softc *)self;
	struct ebus_attach_args *ea = aux;

	if (ea->ea_nregs < 1 || ea->ea_nvaddrs < 1) {
		printf(": no registers??\n");
		return;
	}

	if (ea->ea_nregs != 5 || ea->ea_nvaddrs != 5) {
		printf(": not 5 (%d) registers, only setting led",
		    ea->ea_nregs);
		sc->sc_flags = AUXIO_LEDONLY|AUXIO_EBUS;
	} else {
		sc->sc_flags = AUXIO_EBUS;
		sc->sc_registers.auxio_pci = (u_int32_t *)(u_long)ea->ea_vaddrs[1];
		sc->sc_registers.auxio_freq = (u_int32_t *)(u_long)ea->ea_vaddrs[2];
		sc->sc_registers.auxio_scsi = (u_int32_t *)(u_long)ea->ea_vaddrs[3];
		sc->sc_registers.auxio_temp = (u_int32_t *)(u_long)ea->ea_vaddrs[4];
	}
	sc->sc_registers.auxio_led = (u_int32_t *)(u_long)ea->ea_vaddrs[0];
	
	if (!auxio_reg)
		auxio_reg = (u_char *)(u_long)ea->ea_vaddrs[0];

	printf("\n");
}

int
auxio_sbus_match(parent, cf, aux)
	struct device *parent;
	void *cf;
	void *aux;
{
	struct sbus_attach_args *sa = aux;

	return (strcmp(AUXIO_ROM_NAME, sa->sa_name) == 0);
}

void
auxio_sbus_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct auxio_softc *sc = (struct auxio_softc *)self;
	struct sbus_attach_args *sa = aux;

	if (sa->sa_nreg < 1 || sa->sa_npromvaddrs < 1) {
		printf(": no registers??\n");
		return;
	}

	if (sa->sa_nreg != 1 || sa->sa_npromvaddrs != 1) {
		printf(": not 1 (%d/%d) registers??", sa->sa_nreg, sa->sa_npromvaddrs);
		return;
	}

	/* sbus auxio only has one set of registers */
	sc->sc_flags = AUXIO_LEDONLY|AUXIO_SBUS;
	sc->sc_registers.auxio_led = (u_int32_t *)(u_long)sa->sa_promvaddr;

	if (!auxio_reg)
		auxio_reg = (u_char *)(u_long)sa->sa_promvaddr;

	printf("\n");
}

/*
 * old interface; used by fd driver for now
 */
unsigned int
auxregbisc(bis, bic)
	int bis, bic;
{
	register int v, s = splhigh();

	v = *auxio_reg;
	*auxio_reg = ((v | bis) & ~bic) | AUXIO_LED_MB1;
	splx(s);
	return v;
}
