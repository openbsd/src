/*	$OpenBSD: drsupio.c,v 1.3 2002/06/11 03:25:42 miod Exp $ */
/*	$NetBSD: drsupio.c,v 1.1 1997/08/27 19:32:53 is Exp $ */

/*
 * Copyright (c) 1997 Ignatios Souvatzis
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
 *      This product includes software developed by Ignatios Souvatzis
 *      for the NetBSD Project.
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

/*
 * DraCo multi-io chip bus space stuff
 */

#include <sys/types.h>

#include <sys/device.h>
#include <sys/systm.h>
#include <sys/param.h>

#include <machine/bus.h>
#include <machine/conf.h>

#include <amiga/include/cpu.h>

#include <amiga/amiga/device.h>
#include <amiga/amiga/drcustom.h>

#include <amiga/dev/supio.h>


struct drsupio_softc {
	struct device sc_dev;
	struct amiga_bus_space sc_bst;
};

int	drsupiomatch(struct device *, void *, void *);
void	drsupioattach(struct device *, struct device *, void *);
int	drsupprint(void *auxp, const char *);

struct cfattach drsupio_ca = {
	sizeof(struct drsupio_softc), drsupiomatch, drsupioattach
};

struct cfdriver drsupio_cd = {
	NULL, "drsupio", DV_DULL
};

int	drsupio_map(bus_space_tag_t, bus_addr_t, bus_size_t, int,
	    bus_space_handle_t *);
int	drsupio_unmap(bus_space_tag_t, bus_space_handle_t, bus_size_t);

int
drsupiomatch(parent, match, auxp)
	struct device *parent;
	void *match;
	void *auxp;
{
	struct cfdata *cfp = match;

	/* Exactly one of us lives on the DraCo */

	if (is_draco() && matchname(auxp, "drsupio") && (cfp->cf_unit == 0))
		return 1;

	return 0;
}

struct drsupio_devs {
	char *name;
	int off;
} drsupiodevs[] = {
	{ "com", 0x3f8 },
	{ "com", 0x2f8 },
	{ "lpt", 0x378 },
	{ "fdc", 0x3f0 },
	/* WD port? */
	{ 0 }
};

int
drsupio_map(bst, addr, sz, cacheable, handle)
	bus_space_tag_t bst;
	bus_addr_t addr;
	bus_size_t sz;
	int cacheable;
	bus_space_handle_t *handle;
{
	*handle = DRCCADDR + NBPG * DRSUPIOPG + 1 + (addr << bst->bs_shift);
	return (0);
}

int
drsupio_unmap(bst, handle, sz)
	bus_space_tag_t bst;
	bus_space_handle_t handle;
	bus_size_t sz;
{
	return (0);
}

void
drsupioattach(parent, self, auxp)
	struct device *parent, *self;
	void *auxp;
{
	struct drsupio_softc *drsc;
	struct drsupio_devs  *drsd;
	struct supio_attach_args supa;

	drsc = (struct drsupio_softc *)self;
	drsd = drsupiodevs;

	if (parent)
		printf("\n");

	drsc->sc_bst.bs_map = drsupio_map;
	drsc->sc_bst.bs_unmap = drsupio_unmap;
	drsc->sc_bst.bs_swapped = 0;
	drsc->sc_bst.bs_shift = 2;
	
	supa.supio_iot = &drsc->sc_bst;

	while (drsd->name) {
		supa.supio_name = drsd->name;
		supa.supio_iobase = drsd->off;
		config_found(self, &supa, drsupprint); /* XXX */
		++drsd;
	}
}

int
drsupprint(auxp, pnp)
	void *auxp;
	const char *pnp;
{
	struct supio_attach_args *supa;
	supa = auxp;

	if (pnp == NULL)
		return(QUIET);

	printf("%s at %s port 0x%02x",
	    supa->supio_name, pnp, supa->supio_iobase);

	return(UNCONF);
}
