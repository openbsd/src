/*	$OpenBSD: sti_sgc.c,v 1.6 2005/12/31 18:13:41 miod Exp $	*/

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

int
sti_sgc_match(struct device *parent, void *match, void *aux)
{
	struct sgc_attach_args *saa = aux;
	bus_space_tag_t iot;

	/*
	 * If we already probed it succesfully as a console device, go ahead,
	 * since we will not be able to bus_space_map() again.
	 */
	if (SGC_SLOT_TO_CONSCODE(saa->saa_slot) == conscode)
		return (1);

	iot = HP300_BUS_TAG(HP300_BUS_SGC, saa->saa_slot);

	return (sti_sgc_probe(iot, saa->saa_slot));
}

void
sti_sgc_attach(struct device *parent, struct device *self, void *aux)
{
	struct sti_softc *sc = (void *)self;
	struct sgc_attach_args *saa = aux;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	int devtype;
	u_int32_t romend;

	/*
	 * If we already probed it succesfully as a console device, go ahead,
	 * since we will not be able to bus_space_map() again.
	 */
	if (SGC_SLOT_TO_CONSCODE(saa->saa_slot) == conscode) {
		extern struct sti_screen stifb_cn;

		sc->sc_flags |= STI_CONSOLE | STI_ATTACHED;
		sc->sc_scr = &stifb_cn;

		sti_describe(sc);
	} else {
		iot = HP300_BUS_TAG(HP300_BUS_SGC, saa->saa_slot);
		sc->base = (bus_addr_t)sgc_slottopa(saa->saa_slot);

		if (bus_space_map(iot, sc->base, PAGE_SIZE, 0, &ioh)) {
			printf(": can't map frame buffer");
			return;
		}

		/*
		 * Compute real PROM size
		 */
		devtype = bus_space_read_1(iot, ioh, 3);
		if (devtype == STI_DEVTYPE4) {
			romend = bus_space_read_4(iot, ioh, 0x18);
		} else {
			romend =
			    (bus_space_read_1(iot, ioh, 0x50 +  3) << 24) |
			    (bus_space_read_1(iot, ioh, 0x50 +  7) << 16) |
			    (bus_space_read_1(iot, ioh, 0x50 + 11) <<  8) |
			    (bus_space_read_1(iot, ioh, 0x50 + 15));
		}

		bus_space_unmap(iot, ioh, PAGE_SIZE);

		if (bus_space_map(iot, sc->base, round_page(romend), 0,
		    &ioh)) {
			printf(": can't map frame buffer");
			return;
		}

		sc->memt = sc->iot = iot;
		sc->romh = ioh;

		sti_attach_common(sc, STI_CODEBASE_M68K);
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
	bus_space_tag_t iot;

	iot = HP300_BUS_TAG(HP300_BUS_SGC, slot);

	return (sti_sgc_probe(iot, slot));
}

void
sticninit()
{
	extern struct sti_screen stifb_cn;
	bus_space_tag_t iot;
	bus_addr_t base;

	/*
	 * We are not interested by the *first* console pass.
	 */
	if (consolepass == 0)
		return;

	iot = HP300_BUS_TAG(HP300_BUS_SGC, CONSCODE_TO_SGC_SLOT(conscode));
	base = (bus_addr_t)sgc_slottopa(CONSCODE_TO_SGC_SLOT(conscode));

	sti_cnattach(&stifb_cn, iot, base, STI_CODEBASE_M68K);

	/*
	 * Since the copyright notice could not be displayed before,
	 * display it again now.
	 */
	printf("%s\n", copyright);
}
