/*	$OpenBSD: sti_sgc.c,v 1.1 2005/01/14 22:39:26 miod Exp $	*/

/*
 * Copyright (c) 2005, Miodrag Vallat
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
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/device.h>

#include <machine/autoconf.h>
#include <machine/bus.h>
#include <machine/cpu.h>

#include <dev/cons.h>

#include <hp300/dev/sgcreg.h>
#include <hp300/dev/sgcvar.h>

#include <dev/wscons/wsdisplayvar.h>
#include <dev/wscons/wsconsio.h>

#include <dev/ic/stireg.h>
#include <dev/ic/stivar.h>

#include "wsdisplay.h"

/*
 * This is a safe upper bound of the amount of memory we will need to
 * bus_space_map() for sti frame buffers.
 */
#define	STI_MEMSIZE		0x0003f000	/* XXX */

int  sti_sgc_match(struct device *, void *, void *);
void sti_sgc_attach(struct device *, struct device *, void *);

struct cfattach sti_sgc_ca = {
	sizeof(struct sti_softc), sti_sgc_match, sti_sgc_attach
};

int
sti_sgc_match(struct device *parent, void *match, void *aux)
{
	struct sgc_attach_args *saa = aux;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	int devtype;

#ifdef SGC_CONSOLE
	/*
	 * If we already probed it succesfully as a console device, go ahead,
	 * since we will not be able to bus_space_map() again.
	 */
	if (SGC_SLOT_TO_CONSCODE(saa->saa_slot) == conscode)
		return (1);
#endif

	iot = HP300_BUS_TAG(HP300_BUS_SGC, saa->saa_slot);

	if (bus_space_map(iot, (bus_addr_t)sgc_slottopa(saa->saa_slot),
	    PAGE_SIZE, 0, &ioh))
		return (0);

	devtype = bus_space_read_1(iot, ioh, 3);

	bus_space_unmap(iot, ioh, PAGE_SIZE);

	/*
	 * This might not be reliable enough. On the other hand, non-STI
	 * SGC cards will apparently not initialize in an hp300, to the
	 * point of not even answering bus probes (checked with an
	 * Harmony/FDDI SGC card).
	 */
	if (devtype != STI_DEVTYPE1 && devtype != STI_DEVTYPE4)
		return (0);

	return (1);
}

void
sti_sgc_attach(struct device *parent, struct device *self, void *aux)
{
	struct sti_softc *sc = (void *)self;
	struct sgc_attach_args *saa = aux;

	sc->memt = sc->iot = HP300_BUS_TAG(HP300_BUS_SGC, saa->saa_slot);
	sc->base = (bus_addr_t)sgc_slottopa(saa->saa_slot);

	if (bus_space_map(sc->iot, sc->base, STI_MEMSIZE, 0, &sc->romh)) {
		printf(": can't map frame buffer");
		return;
	}

	if (SGC_SLOT_TO_CONSCODE(saa->saa_slot) == conscode)
		sc->sc_flags |= STI_CONSOLE;

	sti_attach_common(sc, STI_CODEBASE_M68K);
}

/*
 * Console code
 */

int sti_console_scan(int, caddr_t, void *);
cons_decl(sti);

int
sti_console_scan(int slot, caddr_t va, void *arg)
{
#ifdef SGC_CONSOLE
	struct consdev *cp = arg;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	int force = 0, pri;
	int devtype;

	iot = HP300_BUS_TAG(HP300_BUS_SGC, slot);

	if (bus_space_map(iot, (bus_addr_t)sgc_slottopa(slot), STI_MEMSIZE, 0,
	    &ioh))
		return (0);

	devtype = bus_space_read_1(iot, ioh, 3);

	bus_space_unmap(iot, ioh, STI_MEMSIZE);

	/* XXX this is not reliable enough */
	if (devtype != STI_DEVTYPE1 && devtype != STI_DEVTYPE4)
		return (0);

	pri = CN_INTERNAL;

#ifdef	CONSCODE
	/*
	 * Raise our priority, if appropriate.
	 */
	if (SGC_SLOT_TO_CONSCODE(slot) == CONSCODE) {
		pri = CN_REMOTE;
		force = conforced = 1;
	}
#endif

	/* Only raise priority. */
	if (pri > cp->cn_pri)
		cp->cn_pri = pri;

	/*
	 * If our priority is higher than the currently-remembered
	 * console, stash our priority.
	 */
	if (((cn_tab == NULL) || (cp->cn_pri > cn_tab->cn_pri)) || force) {
		cn_tab = cp;
		return (1);
	}

	return (0);
#else
	/* Do not allow console to be on an SGC device (yet). */
	return (0);
#endif
}

void
sticnprobe(struct consdev *cp)
{
	int maj;

	/* Abort early if the console is already forced. */
	if (conforced)
		return;

	for (maj = 0; maj < nchrdev; maj++) {
		if (cdevsw[maj].d_open == wsdisplayopen)
			break;
	}

	if (maj == nchrdev)
		return;

	cp->cn_dev = makedev(maj, 0);
	cp->cn_pri = CN_DEAD;

	/* Search for an sti device */
	console_scan(sti_console_scan, cp, HP300_BUS_SGC);
}

void
sticninit(struct consdev *cp)
{
	/*
	 * We should theoretically initialize the sti driver here.
	 * However, since it relies upon the vm system being initialized,
	 * which is not the case at this point, postpone the initialization.
	 *
	 * We can't even use the PROM output services, since they are not
	 * available AT ALL when we are running virtual.
	 */
}
