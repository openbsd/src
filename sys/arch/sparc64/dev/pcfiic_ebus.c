/*	$OpenBSD: pcfiic_ebus.c,v 1.3 2006/02/09 12:16:25 dlg Exp $ */

/*
 * Copyright (c) 2006 David Gwynne <dlg@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Device specific driver for the EBus i2c devices found on some sun4u
 * systems. On systems not having a boot-bus controller the i2c devices
 * are PCF8584.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/kernel.h>

#include <machine/bus.h>
#include <machine/openfirm.h>

#include <sparc64/dev/ebusreg.h>
#include <sparc64/dev/ebusvar.h>

#include <dev/i2c/i2cvar.h>
#include <sparc64/dev/ofwi2cvar.h>

#include <dev/ic/pcf8584var.h>

int	pcfiic_ebus_match(struct device *, void *, void *);
void	pcfiic_ebus_attach(struct device *, struct device *, void *);

struct pcfiic_ebus_softc {
	struct pcfiic_softc	esc_sc;

	int			esc_node;
	void			*esc_ih;
};

struct cfattach pcfiic_ebus_ca = {
	sizeof(struct pcfiic_ebus_softc), pcfiic_ebus_match, pcfiic_ebus_attach
};

int
pcfiic_ebus_match(struct device *parent, void *match, void *aux)
{
	struct ebus_attach_args		*ea = aux;
	char				compat[32];

	if (strcmp(ea->ea_name, "i2c") != 0)
		return (0);

	if (OF_getprop(ea->ea_node, "compatible", compat, sizeof(compat)) == -1)
		return (0);

	if (strcmp(compat, "i2cpcf,8584") != 0)
		return (0);

	return (1);
}

void
pcfiic_ebus_attach(struct device *parent, struct device *self, void *aux)
{
	struct pcfiic_ebus_softc	*esc = (struct pcfiic_ebus_softc *)self;
	struct pcfiic_softc		*sc = &esc->esc_sc;
        struct ebus_attach_args		*ea = aux;
	u_int64_t			addr;

	sc->sc_iot = ea->ea_memtag;

	if (ea->ea_nregs != 1) {
		printf(": expected 1 register, got %d\n", ea->ea_nregs);
		return;
	}

	if (OF_getprop(ea->ea_node, "own-address", &addr, sizeof(addr)) == -1) {
		printf(": unable to get own address\n");
		return;
	}
	if (addr == 0x00 || addr > 0xff) {
		printf(": invalid address on I2C bus\n");
		return;
	}

	if (ebus_bus_map(sc->sc_iot, 0, EBUS_PADDR_FROM_REG(&ea->ea_regs[0]),
	    ea->ea_regs[0].size, 0, 0, &sc->sc_ioh) != 0) {
                printf(": can't map register space\n");
                return;
        }

	if (ea->ea_nintrs >= 1)
		esc->esc_ih = bus_intr_establish(sc->sc_iot, ea->ea_intrs[0],
		    IPL_BIO, 0, pcfiic_intr, sc, self->dv_xname);
	else
		esc->esc_ih = NULL;


	if (esc->esc_ih == NULL)
		sc->sc_poll = 1;

	pcfiic_attach(sc, (i2c_addr_t)(addr >> 1), ofwiic_scan, &ea->ea_node);
	/* the rest of the attach line is printed by pcfiic_attach() */
}
