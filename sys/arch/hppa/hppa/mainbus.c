/*	$OpenBSD: mainbus.c,v 1.1 1998/10/30 18:54:11 mickey Exp $	*/

/*
 * Copyright (c) 1998 Michael Shalayeff
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
 *	This product includes software developed by Michael Shalayeff.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/reboot.h>

#include <machine/pdc.h>
#include <machine/iomod.h>
#include <machine/autoconf.h>

struct mainbus_softc {
	struct  device sc_dv;
};

int	mbmatch __P((struct device *, void *, void *));
void	mbattach __P((struct device *, struct device *, void *));

struct cfattach mainbus_ca = {
	sizeof(struct mainbus_softc), mbmatch, mbattach
};

struct cfdriver mainbus_cd = {
	NULL, "mainbus", DV_DULL
};

int
mbmatch(parent, cfdata, aux)   
	struct device *parent;
	void *cfdata;
	void *aux;
{
	struct cfdata *cf = cfdata;

	/* there will be only one */
	if (cf->cf_unit > 0)
		return 0;

	return 1;
}

void
mbattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct confargs nca;

	printf("\n");

	/* PDC first */
	nca.ca_name = "pdc";
	nca.ca_mod = -1;
	config_found(self, &nca, mbprint);

	nca.ca_name = "mainbus";
	nca.ca_mod = -1;
	pdc_scanbus(self, &nca, -1, MAXMODBUS);
}

int
mbprint(aux, pnp)
	void *aux;
	const char *pnp;
{
	struct confargs *ca = aux;

	if (pnp)
		printf("\"%s\" at %s (type %x, sv %x)", ca->ca_name, pnp,
		       ca->ca_type.iodc_type, ca->ca_type.iodc_sv_model);
	if (ca->ca_mod >= 0) {
		printf(" mod %d", ca->ca_mod);
		if (!pnp)
			printf(" \"%s\"", ca->ca_name);
	}

	return (UNCONF);
}
