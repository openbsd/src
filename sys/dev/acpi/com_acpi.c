/*	$OpenBSD: com_acpi.c,v 1.1 2018/07/01 10:29:30 kettenis Exp $	*/
/*
 * Copyright (c) 2018 Mark Kettenis
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

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/systm.h>
#include <sys/tty.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>
#include <dev/acpi/acpidev.h>
#include <dev/acpi/amltypes.h>
#include <dev/acpi/dsdt.h>

#undef DEVNAME
#include <dev/ic/comreg.h>
#include <dev/ic/comvar.h>
#include <dev/cons.h>

#define com_usr 31	/* Synopsys DesignWare UART */

struct com_acpi_softc {
	struct com_softc sc;
	struct acpi_softc *sc_acpi;
	struct aml_node *sc_node;

	bus_addr_t sc_addr;
	bus_size_t sc_size;

	int sc_irq;
	int sc_irq_flags;
	void *sc_ih;
};

int	com_acpi_match(struct device *, void *, void *);
void	com_acpi_attach(struct device *, struct device *, void *);

struct cfattach com_acpi_ca = {
	sizeof(struct com_acpi_softc), com_acpi_match, com_acpi_attach
};

const char *com_hids[] = {
	"HISI0031",
	NULL
};

int	com_acpi_parse_resources(int, union acpi_resource *, void *);
int	com_acpi_is_console(struct com_acpi_softc *);
int	com_acpi_intr_designware(void *);

int
com_acpi_match(struct device *parent, void *match, void *aux)
{
	struct acpi_attach_args *aaa = aux;
	struct cfdata *cf = match;

	return acpi_matchhids(aaa, com_hids, cf->cf_driver->cd_name);
}

void
com_acpi_attach(struct device *parent, struct device *self, void *aux)
{
	struct acpi_attach_args *aaa = aux;
	struct com_acpi_softc *sc = (struct com_acpi_softc *)self;
	struct aml_value res;
	uint32_t freq;

	sc->sc_acpi = (struct acpi_softc *)parent;
	sc->sc_node = aaa->aaa_node;
	printf(" %s", sc->sc_node->name);

	if (aml_evalname(sc->sc_acpi, sc->sc_node, "_CRS", 0, NULL, &res)) {
		printf(": can't find registers\n");
		return;
	}

	aml_parse_resource(&res, com_acpi_parse_resources, sc);
	printf(" addr 0x%lx/0x%lx", sc->sc_addr, sc->sc_size);
	if (sc->sc_addr == 0 || sc->sc_size == 0) {
		printf("\n");
		return;
	}

	printf(" irq %d", sc->sc_irq);

	freq = acpi_getpropint(sc->sc_node, "clock-frequency", 0);

	sc->sc.sc_iot = aaa->aaa_memt;
	sc->sc.sc_iobase = sc->sc_addr;
	sc->sc.sc_uarttype = COM_UART_16550;
	sc->sc.sc_frequency = freq ? freq : COM_FREQ;

	sc->sc.sc_reg_width = acpi_getpropint(sc->sc_node, "reg-io-width", 4);
	sc->sc.sc_reg_shift = acpi_getpropint(sc->sc_node, "reg-shift", 2);

	if (com_acpi_is_console(sc)) {
		SET(sc->sc.sc_hwflags, COM_HW_CONSOLE);
		SET(sc->sc.sc_swflags, COM_SW_SOFTCAR);
		comconsfreq = sc->sc.sc_frequency;
		comconsrate = B115200;
	}

	if (bus_space_map(sc->sc.sc_iot, sc->sc_addr, sc->sc_size, 0,
	    &sc->sc.sc_ioh)) {
		printf(": can't map registers\n");
		return;
	}

	sc->sc_ih = acpi_intr_establish(sc->sc_irq, sc->sc_irq_flags, IPL_TTY,
	    com_acpi_intr_designware, sc, sc->sc.sc_dev.dv_xname);
	if (sc->sc_ih == NULL) {
		printf(": can't establish interrupt\n");
		return;
	}

	com_attach_subr(&sc->sc);
}

int
com_acpi_parse_resources(int crsidx, union acpi_resource *crs, void *arg)
{
	struct com_acpi_softc *sc = arg;
	int type = AML_CRSTYPE(crs);

	switch (type) {
	case LR_MEM32FIXED:
		sc->sc_addr = crs->lr_m32fixed._bas;
		sc->sc_size = crs->lr_m32fixed._len;
		break;
	case LR_EXTIRQ:
		sc->sc_irq = crs->lr_extirq.irq[0];
		sc->sc_irq_flags = crs->lr_extirq.flags;
		break;
	}

	return 0;
}

int
com_acpi_is_console(struct com_acpi_softc *sc)
{
	struct acpi_table_header *hdr;
	struct acpi_spcr *spcr;
	struct acpi_gas *base;
	struct acpi_q *entry;

	SIMPLEQ_FOREACH(entry, &sc->sc_acpi->sc_tables, q_next) {
		hdr = entry->q_table;
		if (strncmp(hdr->signature, SPCR_SIG,
		    sizeof(hdr->signature)) == 0) {
			spcr = entry->q_table;
			base = &spcr->base_address;
			if (base->address_space_id == GAS_SYSTEM_MEMORY &&
			    base->address == sc->sc_addr)
				return 1;
		}
	}

	return 0;
}

int
com_acpi_intr_designware(void *cookie)
{
	struct com_softc *sc = cookie;

	com_read_reg(sc, com_usr);

	return comintr(sc);
}
