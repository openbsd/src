/*	$OpenBSD: com_ebus.c,v 1.8 2003/02/17 01:29:20 henric Exp $	*/
/*	$NetBSD: com_ebus.c,v 1.6 2001/07/24 19:27:10 eeh Exp $	*/

/*
 * Copyright (c) 1999, 2000 Matthew R. Green
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
 * NS Super I/O PC87332VLJ "com" to ebus attachment
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/tty.h>
#include <sys/conf.h>

#include <machine/bus.h>
#include <machine/autoconf.h>
#include <machine/openfirm.h>

#include <sparc64/dev/ebusreg.h>
#include <sparc64/dev/ebusvar.h>

#include <dev/cons.h>
#include <dev/ic/comvar.h>

cdev_decl(com); /* XXX this belongs elsewhere */

int	com_ebus_match(struct device *, void *, void *);
void	com_ebus_attach(struct device *, struct device *, void *);

struct cfattach com_ebus_ca = {
	sizeof(struct com_softc), com_ebus_match, com_ebus_attach
};

static char *com_names[] = {
	"su",
	"su_pnp",
	NULL
};

int
com_ebus_match(parent, match, aux)
	struct device *parent;
	void *match;
	void *aux;
{
	struct ebus_attach_args *ea = aux;
	int i;

	for (i=0; com_names[i]; i++)
		if (strcmp(ea->ea_name, com_names[i]) == 0)
			return (1);

	if (strcmp(ea->ea_name, "serial") == 0) {
		char compat[80];

		/* Could be anything. */
		if ((i = OF_getproplen(ea->ea_node, "compatible")) &&
			OF_getprop(ea->ea_node, "compatible", compat,
				sizeof(compat)) == i) {
			if (strcmp(compat, "su16550") == 0 || 
				strcmp(compat, "su") == 0) {
				return (1);
			}
		}
	}
	return (0);
}

/* XXXART - was 1846200 */
#define BAUD_BASE       (1843200)

void
com_ebus_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct com_softc *sc = (void *)self;
	struct ebus_attach_args *ea = aux;
	int i, com_is_input, com_is_output;

	sc->sc_iobase = EBUS_PADDR_FROM_REG(&ea->ea_regs[0]);
	/*
	 * Addresses that should be supplied by the prom:
	 *	- normal com registers
	 *	- ns873xx configuration registers
	 *	- DMA space
	 * The `com' driver does not use DMA accesses, so we can
	 * ignore that for now.  We should enable the com port in
	 * the ns873xx registers here. XXX
	 *
	 * Use the prom address if there.
	 */
	if (ea->ea_nvaddrs) {
		if (bus_space_map(ea->ea_memtag, ea->ea_vaddrs[0], 0,
		    BUS_SPACE_MAP_PROMADDRESS, &sc->sc_ioh) == 0) {
			printf(": can't map register space\n");
			return;
		}
		sc->sc_iot = ea->ea_memtag;
	} else if (ebus_bus_map(ea->ea_memtag, 0,
	    EBUS_PADDR_FROM_REG(&ea->ea_regs[0]),
	    ea->ea_regs[0].size, 0, 0, &sc->sc_ioh) == 0) {
		sc->sc_iot = ea->ea_memtag;
	} else if (ebus_bus_map(ea->ea_iotag, 0,
	    EBUS_PADDR_FROM_REG(&ea->ea_regs[0]),
	    ea->ea_regs[0].size, 0, 0, &sc->sc_ioh) == 0) {
		sc->sc_iot = ea->ea_iotag;
	} else {
		printf(": can't map register space\n");
               	return;
	}
	sc->sc_hwflags = 0;
	sc->sc_swflags = 0;
	sc->sc_frequency = BAUD_BASE;

	for (i = 0; i < ea->ea_nintrs; i++)
		bus_intr_establish(sc->sc_iot, ea->ea_intrs[i],
		    IPL_TTY, 0, comintr, sc);

	/* Figure out if we're the console. */
	com_is_input = (ea->ea_node == OF_instance_to_package(OF_stdin()));
	com_is_output = (ea->ea_node == OF_instance_to_package(OF_stdout()));

	if (com_is_input || com_is_output) {
		struct consdev *cn_orig;

		comconsioh = sc->sc_ioh;
		cn_orig = cn_tab;
		/* Attach com as the console. */
		if (comcnattach(sc->sc_iot, sc->sc_iobase, 9600,
		    sc->sc_frequency,
		    ((TTYDEF_CFLAG & ~(CSIZE | PARENB))|CREAD | CS8 | HUPCL))) {
			printf("Error: comcnattach failed\n");
		}
		cn_tab = cn_orig;
		if (com_is_input) {
			cn_tab->cn_dev = /*XXX*/makedev(36, sc->sc_dev.dv_unit);
			cn_tab->cn_probe = comcnprobe;
			cn_tab->cn_init = comcninit;
			cn_tab->cn_getc = comcngetc;
			cn_tab->cn_pollc = comcnpollc;
		}
		if (com_is_output)
			cn_tab->cn_putc = comcnputc;
	}

        if (OF_getproplen(ea->ea_node, "keyboard") == 0)
		printf(", keyboard");
	else if (OF_getproplen(ea->ea_node, "mouse") == 0)
		printf(", mouse");

	/* Now attach the driver */
	com_attach_subr(sc);
}
