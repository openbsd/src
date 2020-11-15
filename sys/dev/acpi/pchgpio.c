/*	$OpenBSD: pchgpio.c,v 1.1 2020/11/15 16:47:12 kettenis Exp $	*/
/*
 * Copyright (c) 2020 Mark Kettenis
 * Copyright (c) 2020 James Hastings
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

#define PCHGPIO_MAXCOM		4

#define PCHGPIO_CONF_TXSTATE	0x00000001
#define PCHGPIO_CONF_RXSTATE	0x00000002
#define PCHGPIO_CONF_RXINV	0x00800000
#define PCHGPIO_CONF_RXEV_EDGE	0x02000000
#define PCHGPIO_CONF_RXEV_ZERO	0x04000000
#define PCHGPIO_CONF_RXEV_MASK	0x06000000

#define PCHGPIO_PADBAR		0x00c

struct pchgpio_group {
	uint8_t		bar;
	uint8_t		bank;
	uint16_t	base;
	uint16_t	limit;
	uint16_t	offset;
	int16_t		gpiobase;
};

struct pchgpio_device {
	uint16_t	pad_size;
	uint16_t	gpi_is;
	uint16_t	gpi_ie;
	struct pchgpio_group *groups;
	int		ngroups;
	int		npins;
};

struct pchgpio_match {
	const char	*hid;
	struct pchgpio_device *device;
};

struct pchgpio_intrhand {
	int (*ih_func)(void *);
	void *ih_arg;
};

struct pchgpio_softc {
	struct device sc_dev;
	struct acpi_softc *sc_acpi;
	struct aml_node *sc_node;

	bus_space_tag_t sc_memt[PCHGPIO_MAXCOM];
	bus_space_handle_t sc_memh[PCHGPIO_MAXCOM];
	void *sc_ih;
	int sc_naddr;

	struct pchgpio_device *sc_device;
	uint16_t sc_padbar[PCHGPIO_MAXCOM];
	int sc_padsize;

	int sc_npins;
	struct pchgpio_intrhand *sc_pin_ih;

	struct acpi_gpio sc_gpio;
};

int	pchgpio_match(struct device *, void *, void *);
void	pchgpio_attach(struct device *, struct device *, void *);

struct cfattach pchgpio_ca = {
	sizeof(struct pchgpio_softc), pchgpio_match, pchgpio_attach
};

struct cfdriver pchgpio_cd = {
	NULL, "pchgpio", DV_DULL
};

const char *pchgpio_hids[] = {
	"INT34BB",
	NULL
};

struct pchgpio_group cnl_lp_groups[] =
{
	/* Community 0 */
	{ 0, 0, 0, 24, 0, 0 },		/* GPP_A */
	{ 0, 1, 25, 50, 25, 32 },	/* GPP_B */
	{ 0, 2, 51, 58, 51, 64 },	/* GPP_G */

	/* Community 1 */
	{ 1, 0, 68, 92, 0, 96 },	/* GPP_D */
	{ 1, 1, 93, 116, 24, 128 },	/* GPP_F */
	{ 1, 2, 117, 140, 48, 160 },	/* GPP_H */

	/* Community 4 */
	{ 2, 0, 181, 204, 0, 256 },	/* GPP_C */
	{ 2, 1, 205, 228, 24, 288 },	/* GPP_E */
};

struct pchgpio_device cnl_lp_device =
{
	.pad_size = 16,
	.gpi_is = 0x100,
	.gpi_ie = 0x120,
	.groups = cnl_lp_groups,
	.ngroups = nitems(cnl_lp_groups),
	.npins = 320,
};

struct pchgpio_match pchgpio_devices[] = {
	{ "INT34BB", &cnl_lp_device },
};

int	pchgpio_read_pin(void *, int);
void	pchgpio_write_pin(void *, int, int);
void	pchgpio_intr_establish(void *, int, int, int (*)(), void *);
int	pchgpio_intr(void *);

int
pchgpio_match(struct device *parent, void *match, void *aux)
{
	struct acpi_attach_args *aaa = aux;
	struct cfdata *cf = match;

	return acpi_matchhids(aaa, pchgpio_hids, cf->cf_driver->cd_name);
}

void
pchgpio_attach(struct device *parent, struct device *self, void *aux)
{
	struct pchgpio_softc *sc = (struct pchgpio_softc *)self;
	struct acpi_attach_args *aaa = aux;
	int i;

	sc->sc_acpi = (struct acpi_softc *)parent;
	sc->sc_node = aaa->aaa_node;
	printf(" %s", sc->sc_node->name);

	if (aaa->aaa_naddr < 1) {
		printf(": no registers\n");
		return;
	}

	if (aaa->aaa_nirq < 1) {
		printf(": no interrupt\n");
		return;
	}

	printf(" addr");

	for (i = 0; i < aaa->aaa_naddr; i++) {
		printf(" 0x%llx/0x%llx", aaa->aaa_addr[i], aaa->aaa_size[i]);

		sc->sc_memt[i] = aaa->aaa_bst[i];
		if (bus_space_map(sc->sc_memt[i], aaa->aaa_addr[i],
		    aaa->aaa_size[i], 0, &sc->sc_memh[i])) {
			printf(": can't map registers\n");
			goto unmap;
		}

		sc->sc_padbar[i] = bus_space_read_4(sc->sc_memt[i],
		    sc->sc_memh[i], PCHGPIO_PADBAR);
		sc->sc_naddr++;
	}

	printf(" irq %d", aaa->aaa_irq[0]);

	for (i = 0; i < nitems(pchgpio_devices); i++) {
		if (strcmp(pchgpio_devices[i].hid, aaa->aaa_dev) == 0) {
			sc->sc_device = pchgpio_devices[i].device;
			break;
		}
	}
	KASSERT(sc->sc_device);

	sc->sc_padsize = sc->sc_device->pad_size;
	sc->sc_npins = sc->sc_device->npins;
	sc->sc_pin_ih = mallocarray(sc->sc_npins, sizeof(*sc->sc_pin_ih),
	    M_DEVBUF, M_WAITOK | M_ZERO);

	sc->sc_ih = acpi_intr_establish(aaa->aaa_irq[0], aaa->aaa_irq_flags[0],
	    IPL_BIO, pchgpio_intr, sc, sc->sc_dev.dv_xname);
	if (sc->sc_ih == NULL) {
		printf(": can't establish interrupt\n");
		goto unmap;
	}

	sc->sc_gpio.cookie = sc;
	sc->sc_gpio.read_pin = pchgpio_read_pin;
	sc->sc_gpio.write_pin = pchgpio_write_pin;
	sc->sc_gpio.intr_establish = pchgpio_intr_establish;
	sc->sc_node->gpio = &sc->sc_gpio;

	printf(", %d pins\n", sc->sc_npins);

	acpi_register_gpio(sc->sc_acpi, sc->sc_node);
	return;

unmap:
	free(sc->sc_pin_ih, M_DEVBUF, sc->sc_npins * sizeof(*sc->sc_pin_ih));
	for (i = 0; i < sc->sc_naddr; i++)
		bus_space_unmap(sc->sc_memt[i], sc->sc_memh[i],
		    aaa->aaa_size[i]);
}

struct pchgpio_group *
pchgpio_find_group(struct pchgpio_softc *sc, int pin)
{
	int i, npads;

	for (i = 0; i < sc->sc_device->ngroups; i++) {
		npads = 1 + sc->sc_device->groups[i].limit -
		    sc->sc_device->groups[i].base;

		if (pin >= sc->sc_device->groups[i].gpiobase &&
		    pin < sc->sc_device->groups[i].gpiobase + npads)
			return &sc->sc_device->groups[i];
	}
	return NULL;
}

int
pchgpio_read_pin(void *cookie, int pin)
{
	struct pchgpio_softc *sc = cookie;
	struct pchgpio_group *group;
	uint32_t reg;
	uint16_t offset;
	uint8_t bar;

	group = pchgpio_find_group(sc, pin);
	offset = group->offset + (pin - group->gpiobase);
	bar = group->bar;

	reg = bus_space_read_4(sc->sc_memt[bar], sc->sc_memh[bar],
	    sc->sc_padbar[bar] + offset * sc->sc_padsize);

	return !!(reg & PCHGPIO_CONF_RXSTATE);
}

void
pchgpio_write_pin(void *cookie, int pin, int value)
{
	struct pchgpio_softc *sc = cookie;
	struct pchgpio_group *group;
	uint32_t reg;
	uint16_t offset;
	uint8_t bar;

	group = pchgpio_find_group(sc, pin);
	offset = group->offset + (pin - group->gpiobase);
	bar = group->bar;

	reg = bus_space_read_4(sc->sc_memt[bar], sc->sc_memh[bar],
	    sc->sc_padbar[bar] + offset * sc->sc_padsize);
	if (value)
		reg |= PCHGPIO_CONF_TXSTATE;
	else
		reg &= ~PCHGPIO_CONF_TXSTATE;
	bus_space_write_4(sc->sc_memt[bar], sc->sc_memh[bar],
	    sc->sc_padbar[bar] + offset * sc->sc_padsize, reg);
}

void
pchgpio_intr_establish(void *cookie, int pin, int flags,
    int (*func)(void *), void *arg)
{
	struct pchgpio_softc *sc = cookie;
	struct pchgpio_group *group;
	uint32_t reg;
	uint16_t offset;
	uint8_t bank, bar;
	
	KASSERT(pin >= 0 && pin < sc->sc_npins);

	if ((group = pchgpio_find_group(sc, pin)) == NULL)
		return;

	offset = group->offset + (pin - group->gpiobase);
	bar = group->bar;
	bank = group->bank;

	sc->sc_pin_ih[pin].ih_func = func;
	sc->sc_pin_ih[pin].ih_arg = arg;

	reg = bus_space_read_4(sc->sc_memt[bar], sc->sc_memh[bar],
	    sc->sc_padbar[bar] + offset * sc->sc_padsize);
	reg &= ~(PCHGPIO_CONF_RXEV_MASK | PCHGPIO_CONF_RXINV);
	if ((flags & LR_GPIO_MODE) == 1)
		reg |= PCHGPIO_CONF_RXEV_EDGE;
	if ((flags & LR_GPIO_POLARITY) == LR_GPIO_ACTLO)
		reg |= PCHGPIO_CONF_RXINV;
	if ((flags & LR_GPIO_POLARITY) == LR_GPIO_ACTBOTH)
		reg |= PCHGPIO_CONF_RXEV_EDGE | PCHGPIO_CONF_RXEV_ZERO;
	bus_space_write_4(sc->sc_memt[bar], sc->sc_memh[bar],
	    sc->sc_padbar[bar] + offset * sc->sc_padsize, reg);

	reg = bus_space_read_4(sc->sc_memt[bar], sc->sc_memh[bar],
	    sc->sc_device->gpi_ie + bank * 4);
	reg |= (1 << (pin - group->gpiobase));
	bus_space_write_4(sc->sc_memt[bar], sc->sc_memh[bar],
	    sc->sc_device->gpi_ie + bank * 4, reg);
}

int
pchgpio_intr(void *arg)
{
	struct pchgpio_softc *sc = arg;
	uint32_t status, enable;
	int gpiobase, group, bit, pin, handled = 0;
	uint16_t base, limit;
	uint16_t offset;
	uint8_t bank, bar;

	for (group = 0; group < sc->sc_device->ngroups; group++) {
		bar = sc->sc_device->groups[group].bar;
		bank = sc->sc_device->groups[group].bank;
		base = sc->sc_device->groups[group].base;
		limit = sc->sc_device->groups[group].limit;
		offset = sc->sc_device->groups[group].offset;
		gpiobase = sc->sc_device->groups[group].gpiobase;

		status = bus_space_read_4(sc->sc_memt[bar], sc->sc_memh[bar],
		    sc->sc_device->gpi_is + bank * 4);
		bus_space_write_4(sc->sc_memt[bar], sc->sc_memh[bar],
		    sc->sc_device->gpi_is + bank * 4, status);
		enable = bus_space_read_4(sc->sc_memt[bar], sc->sc_memh[bar],
		    sc->sc_device->gpi_ie + bank * 4);
		status &= enable;
		if (status == 0)
			continue;

		for (bit = 0; bit <= (limit - base); bit++) {
			pin = gpiobase + bit;
			if (status & (1 << bit) && sc->sc_pin_ih[pin].ih_func)
				sc->sc_pin_ih[pin].ih_func(sc->sc_pin_ih[pin].ih_arg);
			handled = 1;
		}
	}

	return handled;
}
