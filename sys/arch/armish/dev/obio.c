/*	$OpenBSD: obio.c,v 1.3 2006/06/01 17:06:16 drahn Exp $	*/
/*	$NetBSD: obio.c,v 1.14 2005/12/11 12:17:09 christos Exp $	*/

/*
 * Copyright (c) 2001, 2002, 2003 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
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
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * On-board device autoconfiguration support for Intel IQ80321
 * evaluation boards.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/bus.h>

#include <arm/mainbus/mainbus.h>
#include <arm/xscale/i80321reg.h>
#include <armish/dev/iq80321reg.h>
#include <armish/dev/iq80321var.h>
#include <armish/dev/obiovar.h>

int	obio_match(struct device *, void *, void *);
void	obio_attach(struct device *, struct device *, void *);

struct cfattach obio_ca = {
	sizeof(struct device), obio_match, obio_attach
};

struct cfdriver obio_cd = {
	NULL, "obio", DV_DULL
};

int	obio_print(void *, const char *);
int	obio_search(struct device *, void *, void *);

/* there can be only one */
int	obio_found;

int
obio_match(struct device *parent, void *match, void *aux)
{
	struct mainbus_attach_args *ma = aux;
	struct cfdata *cf = match;

	if (obio_found)
		return (0);

	if (strcmp(cf->cf_driver->cd_name, ma->ma_name) == 0)
		return (1);

	return (0);
}

void
obio_attach(struct device *parent, struct device *self, void *aux)
{
        struct device *sc = self;


	obio_found = 1;

	printf("\n");

	/*
	 * Attach all the on-board devices as described in the kernel
	 * configuration file.
	 */
	config_search(obio_search, self, sc);
}

int
obio_print(void *aux, const char *pnp)
{
	struct obio_attach_args *oba = aux;

	printf(" addr 0x%08lx", oba->oba_addr);

        if (oba->oba_irq != -1)
                printf(" intr %d", oba->oba_irq);


	return (UNCONF);
}

int
obio_search(struct device *parent, void *v, void *aux)
{
	struct obio_attach_args oba;
	struct cfdata   *cf = v;

	oba.oba_st = &obio_bs_tag;
	oba.oba_addr = cf->cf_loc[0];
	oba.oba_size = cf->cf_loc[1];
	oba.oba_width = cf->cf_loc[2];
	if (cf->cf_loc[3] != -1)
		oba.oba_irq = ICU_INT_XINT(cf->cf_loc[3]);
	else
		oba.oba_irq = -1;

	config_found(parent, &oba, obio_print);


	return (0);
}
