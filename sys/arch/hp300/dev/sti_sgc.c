/*	$OpenBSD: sti_sgc.c,v 1.15 2009/02/06 22:51:03 miod Exp $	*/

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

#include <hp300/dev/sgcreg.h>
#include <hp300/dev/sgcvar.h>

#include <dev/wscons/wsdisplayvar.h>
#include <dev/wscons/wsconsio.h>

#include <dev/ic/stireg.h>
#include <dev/ic/stivar.h>

#include <uvm/uvm_extern.h>

void	sti_sgc_attach(struct device *, struct device *, void *);
int	sti_sgc_match(struct device *, void *, void *);
int	sti_sgc_probe(bus_space_tag_t, int);

struct cfattach sti_sgc_ca = {
	sizeof(struct sti_softc), sti_sgc_match, sti_sgc_attach
};

/* Console data */
struct sti_rom sticn_rom;
struct sti_screen sticn_scr;
bus_addr_t sticn_bases[STI_REGION_MAX];

int
sti_sgc_match(struct device *parent, void *match, void *aux)
{
	struct sgc_attach_args *saa = aux;

	/*
	 * If we already probed it successfully as a console device, go ahead,
	 * since we will not be able to bus_space_map() again.
	 */
	if (SGC_SLOT_TO_CONSCODE(saa->saa_slot) == conscode)
		return (1);

	return (sti_sgc_probe(saa->saa_iot, saa->saa_slot));
}

void
sti_sgc_attach(struct device *parent, struct device *self, void *aux)
{
	struct sti_softc *sc = (void *)self;
	struct sgc_attach_args *saa = aux;
	bus_addr_t base;
	bus_space_handle_t romh;
	u_int romend;
	int i;

	/*
	 * If we already probed it successfully as a console device, go ahead,
	 * since we will not be able to bus_space_map() again.
	 */
	if (SGC_SLOT_TO_CONSCODE(saa->saa_slot) == conscode) {
		sc->sc_flags |= STI_CONSOLE | STI_ATTACHED;
		sc->sc_rom = &sticn_rom;
		sc->sc_scr = &sticn_scr;
		bcopy(sticn_bases, sc->bases, sizeof(sc->bases));

		sti_describe(sc);
	} else {
		base = (bus_addr_t)sgc_slottopa(saa->saa_slot);

		if (bus_space_map(saa->saa_iot, base, PAGE_SIZE, 0, &romh)) {
			printf(": can't map frame buffer");
			return;
		}

		/*
		 * Compute real PROM size
		 */
		romend = sti_rom_size(saa->saa_iot, romh);

		bus_space_unmap(saa->saa_iot, romh, PAGE_SIZE);

		if (bus_space_map(saa->saa_iot, base, romend, 0, &romh)) {
			printf(": can't map frame buffer");
			return;
		}

		sc->bases[0] = romh;
		for (i = 1; i < STI_REGION_MAX; i++)
			sc->bases[i] = base;

		if (sti_attach_common(sc, saa->saa_iot, saa->saa_iot, romh,
		    STI_CODEBASE_M68K) != 0)
			return;
	}

	sti_end_attach(sc);
}

int
sti_sgc_probe(bus_space_tag_t iot, int slot)
{
	bus_space_handle_t ioh;
	int devtype;

	if (bus_space_map(iot, (bus_addr_t)sgc_slottopa(slot),
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

/*
 * Console code
 */

int	sti_console_scan(int);
void	sticninit(void);

int
sti_console_scan(int slot)
{
	extern struct hp300_bus_space_tag hp300_mem_tag;
	bus_space_tag_t iot;

	iot = &hp300_mem_tag;
	return (sti_sgc_probe(iot, slot));
}

void
sticninit()
{
	extern struct hp300_bus_space_tag hp300_mem_tag;
	bus_space_tag_t iot;
	bus_addr_t base;
	int i;

	/*
	 * We are not interested by the *first* console pass.
	 */
	if (consolepass == 0)
		return;

	iot = &hp300_mem_tag;
	base = (bus_addr_t)sgc_slottopa(CONSCODE_TO_SGC_SLOT(conscode));

	/* sticn_bases[0] will be fixed in sti_cnattach() */
	for (i = 0; i < STI_REGION_MAX; i++)
		sticn_bases[i] = base;

	sti_cnattach(&sticn_rom, &sticn_scr, iot, sticn_bases,
	    STI_CODEBASE_M68K);

	/*
	 * Since the copyright notice could not be displayed before,
	 * display it again now.
	 */
	printf("%s\n", copyright);
}
