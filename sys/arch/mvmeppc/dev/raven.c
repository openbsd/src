/*	$OpenBSD: raven.c,v 1.2 2001/11/06 19:53:15 miod Exp $ */

/*
 * Copyright (c) 2001 Steve Murphree, Jr.
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
 *	This product includes software developed under OpenBSD for RTMX Inc
 *      by Per Fogelstrom, Opsycon AB.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/*
 * Motorola 'Raven' ASIC driver.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <uvm/uvm_extern.h>

#include <machine/autoconf.h>

#include <mvmeppc/dev/ravenreg.h>
#include <mvmeppc/dev/ravenvar.h>

int	 raven_match __P((struct device *, void *, void *));
void	 raven_attach __P((struct device *, struct device *, void *));

struct cfattach raven_ca = {
        sizeof(struct raven_softc), raven_match, raven_attach,
};

struct cfdriver raven_cd = {
	NULL, "raven", DV_DULL,
};

int
raven_match(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct confargs *ca = aux;
	unsigned *reg = (unsigned *)RAVEN_REG;

	/* check for a live address */
	if (badaddr((char *)reg, 4))
		return 0;
	
	/* now check and see if it's a raven ASIC */	
	if (*reg != RAVEN_MAGIC)
		return 0;
	
	return 1;
}

void
raven_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct raven_softc *sc = (struct raven_softc *)self;
	struct confargs *ca = aux;
	struct mpic_feature *feature = (struct mpic_feature *)MPCIC_FEATURE;

	/* set system type */
	system_type = MVME;		/* We are a Motorola MVME SBC */

	printf(": RAVEN, Version 0x%x.\n", feature->vid);
	while (config_found(self, NULL, NULL))
		;
}
                 

