/*	$OpenBSD: sti_dio.c,v 1.1 2011/08/18 20:02:57 miod Exp $	*/

/*
 * Copyright (c) 2005, 2011, Miodrag Vallat
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

#include <hp300/dev/dioreg.h>
#include <hp300/dev/diovar.h>
#include <hp300/dev/diodevs.h>

#include <dev/wscons/wsdisplayvar.h>
#include <dev/wscons/wsconsio.h>

#include <dev/ic/stireg.h>
#include <dev/ic/stivar.h>
#include <hp300/dev/sti_machdep.h>

#include <uvm/uvm_extern.h>

void	sti_dio_attach(struct device *, struct device *, void *);
int	sti_dio_match(struct device *, void *, void *);

struct cfattach sti_dio_ca = {
	sizeof(struct sti_softc), sti_dio_match, sti_dio_attach
};

extern struct hp300_bus_space_tag hp300_mem_tag;

int
sti_dio_match(struct device *parent, void *match, void *aux)
{
	struct dio_attach_args *da = aux;

	if (da->da_id != DIO_DEVICE_ID_FRAMEBUFFER ||
	    (da->da_secid != DIO_DEVICE_SECID_FB3X2_A &&
	     da->da_secid != DIO_DEVICE_SECID_FB3X2_B))
		return (0);

	/*
	 * If we already probed it successfully as a console device, go ahead,
	 * since we will not be able to bus_space_map() again.
	 */
	if (da->da_scode == conscode)
		return (1);

	return (sti_dio_probe(da->da_scode));
}

void
sti_dio_attach(struct device *parent, struct device *self, void *aux)
{
	struct sti_softc *sc = (void *)self;
	struct dio_attach_args *da = aux;
	bus_addr_t base;
	bus_space_tag_t iot;
	bus_space_handle_t romh;
	u_int romend;
	int i;

	/*
	 * If we already probed it successfully as a console device, go ahead,
	 * since we will not be able to bus_space_map() again.
	 */
	if (da->da_scode == conscode) {
		sc->sc_flags |= STI_CONSOLE | STI_ATTACHED;
		sc->sc_rom = &sticn_rom;
		sc->sc_scr = &sticn_scr;
		bcopy(sticn_bases, sc->bases, sizeof(sc->bases));

		sti_describe(sc);
	} else {
		base = (bus_addr_t)
		    dio_scodetopa(da->da_scode + STI_DIO_SCODE_OFFSET);
		iot = &hp300_mem_tag;

		if (bus_space_map(iot, base, PAGE_SIZE, 0, &romh)) {
			printf(": can't map frame buffer");
			return;
		}

		/*
		 * Compute real PROM size
		 */
		romend = sti_rom_size(iot, romh);

		bus_space_unmap(iot, romh, PAGE_SIZE);

		if (bus_space_map(iot, base, romend, 0, &romh)) {
			printf(": can't map frame buffer");
			return;
		}

		sc->bases[0] = romh;
		for (i = 1; i < STI_REGION_MAX; i++)
			sc->bases[i] = base;

		if (sti_attach_common(sc, iot, iot, romh,
		    STI_CODEBASE_M68K) != 0)
			return;
	}

	sti_end_attach(sc);
}

int
sti_dio_probe(int scode)
{
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	int devtype;
	uint span;

	iot = &hp300_mem_tag;

	/*
	 * Sanity checks:
	 * these devices provide both a DIO and an STI ROM. We expect the
	 * DIO ROM to be a DIO-II ROM (i.e. to be at a DIO-II select code)
	 * and report the device as spanning at least four select codes.
	 */

	if (!DIO_ISDIOII(scode))
		return 0;

	if (bus_space_map(iot, (bus_addr_t)dio_scodetopa(scode),
	    PAGE_SIZE, 0, &ioh))
		return 0;
	span = bus_space_read_1(iot, ioh, DIOII_SIZEOFF);
	bus_space_unmap(iot, ioh, PAGE_SIZE);

	if (span < STI_DIO_SIZE - 1)
		return 0;

	if (bus_space_map(iot,
	    (bus_addr_t)dio_scodetopa(scode + STI_DIO_SCODE_OFFSET),
	    PAGE_SIZE, 0, &ioh))
		return 0;
	devtype = bus_space_read_1(iot, ioh, 3);
	bus_space_unmap(iot, ioh, PAGE_SIZE);

	if (devtype != STI_DEVTYPE1 && devtype != STI_DEVTYPE4)
		return 0;

	return 1;
}
