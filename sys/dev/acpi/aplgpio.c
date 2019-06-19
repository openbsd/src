/*	$OpenBSD: aplgpio.c,v 1.1 2019/06/17 18:28:18 patrick Exp $	*/
/*
 * Copyright (c) 2016 Mark Kettenis
 * Copyright (c) 2019 James Hastings
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

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>
#include <dev/acpi/acpidev.h>
#include <dev/acpi/amltypes.h>
#include <dev/acpi/dsdt.h>

#define APLGPIO_CONF_TXSTATE	0x00000001
#define APLGPIO_CONF_RXSTATE	0x00000002
#define APLGPIO_CONF_RXINV	0x00800000
#define APLGPIO_CONF_RXEV_EDGE	0x02000000
#define APLGPIO_CONF_RXEV_ZERO	0x04000000
#define APLGPIO_CONF_RXEV_MASK	0x06000000

#define APLGPIO_IRQ_STS		0x100
#define APLGPIO_IRQ_EN		0x110
#define APLGPIO_PAD_CFG0	0x500

struct aplgpio_intrhand {
	int (*ih_func)(void *);
	void *ih_arg;
};

struct aplgpio_softc {
	struct device sc_dev;
	struct acpi_softc *sc_acpi;
	struct aml_node *sc_node;

	bus_space_tag_t sc_memt;
	bus_space_handle_t sc_memh;
	bus_addr_t sc_addr;
	bus_size_t sc_size;

	int sc_irq;
	int sc_irq_flags;
	void *sc_ih;

	int sc_npins;
	struct aplgpio_intrhand *sc_pin_ih;

	struct acpi_gpio sc_gpio;
};

int	aplgpio_match(struct device *, void *, void *);
void	aplgpio_attach(struct device *, struct device *, void *);

struct cfattach aplgpio_ca = {
	sizeof(struct aplgpio_softc), aplgpio_match, aplgpio_attach
};

struct cfdriver aplgpio_cd = {
	NULL, "aplgpio", DV_DULL
};

const char *aplgpio_hids[] = {
	"INT3452",
	NULL
};

int	aplgpio_parse_resources(int, union acpi_resource *, void *);
int	aplgpio_read_pin(void *, int);
void	aplgpio_write_pin(void *, int, int);
void	aplgpio_intr_establish(void *, int, int, int (*)(), void *);
int	aplgpio_intr(void *);

int
aplgpio_match(struct device *parent, void *match, void *aux)
{
	struct acpi_attach_args *aaa = aux;
	struct cfdata *cf = match;

	return acpi_matchhids(aaa, aplgpio_hids, cf->cf_driver->cd_name);
}

void
aplgpio_attach(struct device *parent, struct device *self, void *aux)
{
	struct acpi_attach_args *aaa = aux;
	struct aplgpio_softc *sc = (struct aplgpio_softc *)self;
	struct aml_value res;
	int64_t uid;
	int i;

	sc->sc_acpi = (struct acpi_softc *)parent;
	sc->sc_node = aaa->aaa_node;
	printf(": %s", sc->sc_node->name);

	if (aml_evalinteger(sc->sc_acpi, sc->sc_node, "_UID", 0, NULL, &uid)) {
		printf(", can't find uid\n");
		return;
	}

	printf(" uid %lld", uid);

	switch (uid) {
	case 1:
		sc->sc_npins = 78;
		break;
	case 2:
		sc->sc_npins = 77;
		break;
	case 3:
		sc->sc_npins = 47;
		break;
	case 4:
		sc->sc_npins = 43;
		break;
	default:
		printf("\n");
		return;
	}

	if (aml_evalname(sc->sc_acpi, sc->sc_node, "_CRS", 0, NULL, &res)) {
		printf(", can't find registers\n");
		return;
	}

	aml_parse_resource(&res, aplgpio_parse_resources, sc);
	printf(" addr 0x%lx/0x%lx", sc->sc_addr, sc->sc_size);
	if (sc->sc_addr == 0 || sc->sc_size == 0) {
		printf("\n");
		return;
	}
	aml_freevalue(&res);

	sc->sc_pin_ih = mallocarray(sc->sc_npins, sizeof(*sc->sc_pin_ih),
	    M_DEVBUF, M_NOWAIT | M_ZERO);
	if (sc->sc_pin_ih == NULL) {
		printf("\n");
		return;
	}

	printf(" irq %d", sc->sc_irq);

	sc->sc_memt = aaa->aaa_memt;
	if (bus_space_map(sc->sc_memt, sc->sc_addr, sc->sc_size, 0,
	    &sc->sc_memh)) {
		printf(", can't map registers\n");
		goto free;
	}

	sc->sc_ih = acpi_intr_establish(sc->sc_irq, sc->sc_irq_flags, IPL_BIO,
	    aplgpio_intr, sc, sc->sc_dev.dv_xname);
	if (sc->sc_ih == NULL) {
		printf(", can't establish interrupt\n");
		goto unmap;
	}

	sc->sc_gpio.cookie = sc;
	sc->sc_gpio.read_pin = aplgpio_read_pin;
	sc->sc_gpio.write_pin = aplgpio_write_pin;
	sc->sc_gpio.intr_establish = aplgpio_intr_establish;
	sc->sc_node->gpio = &sc->sc_gpio;

	/* Mask and clear all interrupts. */
	for (i = 0; i < sc->sc_npins; i++) {
		if (i % 32 == 0) {
			bus_space_write_4(sc->sc_memt, sc->sc_memh,
			    APLGPIO_IRQ_EN + (i / 32) * 4, 0x00000000);
			bus_space_write_4(sc->sc_memt, sc->sc_memh,
			    APLGPIO_IRQ_STS + (i / 32) * 4, 0xffffffff);
		}
	}

	printf(", %d pins\n", sc->sc_npins);

	acpi_register_gpio(sc->sc_acpi, sc->sc_node);
	return;

unmap:
	bus_space_unmap(sc->sc_memt, sc->sc_memh, sc->sc_size);
free:
	free(sc->sc_pin_ih, M_DEVBUF, sc->sc_npins * sizeof(*sc->sc_pin_ih));
}

int
aplgpio_parse_resources(int crsidx, union acpi_resource *crs, void *arg)
{
	struct aplgpio_softc *sc = arg;
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
	default:
		printf(" type 0x%x\n", type);
		break;
	}

	return 0;
}

int
aplgpio_read_pin(void *cookie, int pin)
{
	struct aplgpio_softc *sc = cookie;
	uint32_t reg;

	reg = bus_space_read_4(sc->sc_memt, sc->sc_memh,
	    APLGPIO_PAD_CFG0 + pin * 8);

	return !!(reg & APLGPIO_CONF_RXSTATE);
}

void
aplgpio_write_pin(void *cookie, int pin, int value)
{
	struct aplgpio_softc *sc = cookie;
	uint32_t reg;

	reg = bus_space_read_4(sc->sc_memt, sc->sc_memh,
	    APLGPIO_PAD_CFG0 + pin * 8);
	if (value)
		reg |= APLGPIO_CONF_TXSTATE;
	else
		reg &= ~APLGPIO_CONF_TXSTATE;
	bus_space_write_4(sc->sc_memt, sc->sc_memh,
	    APLGPIO_PAD_CFG0 + pin * 8, reg);
}

void
aplgpio_intr_establish(void *cookie, int pin, int flags,
    int (*func)(void *), void *arg)
{
	struct aplgpio_softc *sc = cookie;
	uint32_t reg;

	KASSERT(pin >= 0 && pin < sc->sc_npins);

	sc->sc_pin_ih[pin].ih_func = func;
	sc->sc_pin_ih[pin].ih_arg = arg;

	reg = bus_space_read_4(sc->sc_memt, sc->sc_memh,
	    APLGPIO_PAD_CFG0 + pin * 8);
	reg &= ~(APLGPIO_CONF_RXEV_MASK | APLGPIO_CONF_RXINV);
	if ((flags & LR_GPIO_MODE) == 1)
		reg |= APLGPIO_CONF_RXEV_EDGE;
	if ((flags & LR_GPIO_POLARITY) == LR_GPIO_ACTLO)
		reg |= APLGPIO_CONF_RXINV;
	if ((flags & LR_GPIO_POLARITY) == LR_GPIO_ACTBOTH)
		reg |= APLGPIO_CONF_RXEV_EDGE | APLGPIO_CONF_RXEV_ZERO;
	bus_space_write_4(sc->sc_memt, sc->sc_memh,
	    APLGPIO_PAD_CFG0 + pin * 8, reg);

	reg = bus_space_read_4(sc->sc_memt, sc->sc_memh,
	    APLGPIO_IRQ_EN + (pin / 32) * 4);
	reg |= (1 << (pin % 32));
	bus_space_write_4(sc->sc_memt, sc->sc_memh,
	    APLGPIO_IRQ_EN + (pin / 32) * 4, reg);
}

int
aplgpio_intr(void *arg)
{
	struct aplgpio_softc *sc = arg;
	uint32_t status, enable;
	int rc = 0;
	int pin;

	for (pin = 0; pin < sc->sc_npins; pin++) {
		if (pin % 32 == 0) {
			status = bus_space_read_4(sc->sc_memt, sc->sc_memh,
			    APLGPIO_IRQ_STS + (pin / 32) * 4);
			bus_space_write_4(sc->sc_memt, sc->sc_memh,
			    APLGPIO_IRQ_STS + (pin / 32) * 4, status);
			enable = bus_space_read_4(sc->sc_memt, sc->sc_memh,
			    APLGPIO_IRQ_EN + (pin / 32) * 4);
			status &= enable;
		}
		if (status & (1 << (pin % 32))) {
			if (sc->sc_pin_ih[pin].ih_func)
				sc->sc_pin_ih[pin].ih_func(sc->sc_pin_ih[pin].ih_arg);
			rc = 1;
		}
	}
	return rc;
}
