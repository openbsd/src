/*	$OpenBSD: voyager.c,v 1.2 2010/02/24 22:14:54 miod Exp $	*/

/*
 * Copyright (c) 2010 Miodrag Vallat.
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
 * Silicon Motion SM501/SM502 (VoyagerGX) master driver.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/gpio.h>

#include <machine/bus.h>
#include <machine/cpu.h>

#include <dev/gpio/gpiovar.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <loongson/dev/voyagerreg.h>
#include <loongson/dev/voyagervar.h>

struct voyager_softc {
	struct device		 sc_dev;

	bus_space_tag_t		 sc_fbt;
	bus_space_handle_t	 sc_fbh;
	bus_size_t		 sc_fbsize;

	bus_space_tag_t		 sc_mmiot;
	bus_space_handle_t	 sc_mmioh;
	bus_size_t		 sc_mmiosize;

	struct gpio_chipset_tag	 sc_gpio_tag;
	gpio_pin_t		 sc_gpio_pins[VOYAGER_NGPIO];
};

int	voyager_match(struct device *, void *, void *);
void	voyager_attach(struct device *, struct device *, void *);

const struct cfattach voyager_ca = {
	sizeof(struct voyager_softc), voyager_match, voyager_attach
};

struct cfdriver voyager_cd = {
	NULL, "voyager", DV_DULL
};

void	voyager_attach_gpio(struct voyager_softc *);
int	voyager_print(void *, const char *);
int	voyager_search(struct device *, void *, void *);

const struct pci_matchid voyager_devices[] = {
	/*
	 * 502 shares the same device ID as 501, but uses a different
	 * revision number.
	 */
	{ PCI_VENDOR_SMI, PCI_PRODUCT_SMI_SM501 }
};

int
voyager_match(struct device *parent, void *vcf, void *aux)
{
	struct pci_attach_args *pa = (struct pci_attach_args *)aux;

	return pci_matchbyid(pa, voyager_devices, nitems(voyager_devices));
}

void
voyager_attach(struct device *parent, struct device *self, void *aux)
{
	struct voyager_softc *sc = (struct voyager_softc *)self;
	struct pci_attach_args *pa = (struct pci_attach_args *)aux;

	if (pci_mapreg_map(pa, PCI_MAPREG_START, PCI_MAPREG_TYPE_MEM,
	    BUS_SPACE_MAP_LINEAR, &sc->sc_fbt, &sc->sc_fbh,
	    NULL, &sc->sc_fbsize, 0) != 0) {
		printf(": can't map frame buffer\n");
		return;
	}

	if (pci_mapreg_map(pa, PCI_MAPREG_START + 0x04, PCI_MAPREG_TYPE_MEM,
	    BUS_SPACE_MAP_LINEAR, &sc->sc_mmiot, &sc->sc_mmioh,
	    NULL, &sc->sc_mmiosize, 0) != 0) {
		printf(": can't map mmio\n");
		bus_space_unmap(sc->sc_fbt, sc->sc_fbh, sc->sc_fbsize);
		return;
	}

	printf("\n");

	/*
	 * Attach GPIO devices.
	 */
	voyager_attach_gpio(sc);

	/*
	 * Attach child devices.
	 */
	config_search(voyager_search, self, pa);
}

int
voyager_print(void *args, const char *parentname)
{
	struct voyager_attach_args *vaa = (struct voyager_attach_args *)args;

	if (parentname != NULL)
		printf("%s at %s", vaa->vaa_name, parentname);

	return UNCONF;
}

int
voyager_search(struct device *parent, void *vcf, void *args)
{
	struct voyager_softc *sc = (struct voyager_softc *)parent;
	struct cfdata *cf = (struct cfdata *)vcf;
	struct pci_attach_args *pa = (struct pci_attach_args *)args;
	struct voyager_attach_args vaa;

	/*
	 * Caller should have attached gpio already. If it didn't, bail
	 * out here.
	 */
	if (strcmp(cf->cf_driver->cd_name, "gpio") == 0)
		return 0;

	vaa.vaa_name = cf->cf_driver->cd_name;
	vaa.vaa_pa = pa;
	vaa.vaa_fbt = sc->sc_fbt;
	vaa.vaa_fbh = sc->sc_fbh;
	vaa.vaa_mmiot = sc->sc_mmiot;
	vaa.vaa_mmioh = sc->sc_mmioh;

	if (cf->cf_attach->ca_match(parent, cf, &vaa) == 0)
		return 0;

	config_attach(parent, cf, &vaa, voyager_print);
	return 1;
}

/*
 * GPIO support code
 */

int	voyager_gpio_pin_read(void *, int);
void	voyager_gpio_pin_write(void *, int, int);
void	voyager_gpio_pin_ctl(void *, int, int);

static const struct gpio_chipset_tag voyager_gpio_tag = {
	.gp_pin_read = voyager_gpio_pin_read,
	.gp_pin_write = voyager_gpio_pin_write,
	.gp_pin_ctl = voyager_gpio_pin_ctl
};

int
voyager_gpio_pin_read(void *cookie, int pin)
{
	struct voyager_softc *sc = (struct voyager_softc *)cookie;
	bus_addr_t reg;
	int32_t data, mask;

	if (pin >= 32) {
		pin -= 32;
		reg = VOYAGER_GPIO_DATA_HIGH;
	} else {
		reg = VOYAGER_GPIO_DATA_LOW;
	}
	mask = 1 << pin;

	data = bus_space_read_4(sc->sc_mmiot, sc->sc_mmioh, reg);
	return data & mask ? GPIO_PIN_HIGH : GPIO_PIN_LOW;
}

void
voyager_gpio_pin_write(void *cookie, int pin, int val)
{
	struct voyager_softc *sc = (struct voyager_softc *)cookie;
	bus_addr_t reg;
	int32_t data, mask;

	if (pin >= 32) {
		pin -= 32;
		reg = VOYAGER_GPIO_DATA_HIGH;
	} else {
		reg = VOYAGER_GPIO_DATA_LOW;
	}
	mask = 1 << pin;
	data = bus_space_read_4(sc->sc_mmiot, sc->sc_mmioh, reg);
	if (val)
		data |= mask;
	else
		data &= ~mask;
	bus_space_write_4(sc->sc_mmiot, sc->sc_mmioh, reg, data);
	(void)bus_space_read_4(sc->sc_mmiot, sc->sc_mmioh, reg);
}

void
voyager_gpio_pin_ctl(void *cookie, int pin, int flags)
{
	struct voyager_softc *sc = (struct voyager_softc *)cookie;
	bus_addr_t reg;
	int32_t data, mask;

	if (pin >= 32) {
		pin -= 32;
		reg = VOYAGER_GPIO_DIR_HIGH;
	} else {
		reg = VOYAGER_GPIO_DIR_LOW;
	}
	mask = 1 << pin;
	data = bus_space_read_4(sc->sc_mmiot, sc->sc_mmioh, reg);
	if (ISSET(flags, GPIO_PIN_OUTPUT))
		data |= mask;
	else
		data &= ~mask;
	bus_space_write_4(sc->sc_mmiot, sc->sc_mmioh, reg, data);
	(void)bus_space_read_4(sc->sc_mmiot, sc->sc_mmioh, reg);
}

void
voyager_attach_gpio(struct voyager_softc *sc)
{
	struct gpiobus_attach_args gba;
	int pin;

	bcopy(&voyager_gpio_tag, &sc->sc_gpio_tag, sizeof voyager_gpio_tag);
	sc->sc_gpio_tag.gp_cookie = sc;

	for (pin = 0; pin < VOYAGER_NGPIO; pin++) {
		sc->sc_gpio_pins[pin].pin_num = pin;
		sc->sc_gpio_pins[pin].pin_caps =
		    GPIO_PIN_INPUT | GPIO_PIN_OUTPUT;
		sc->sc_gpio_pins[pin].pin_state =
		    voyager_gpio_pin_read(sc, pin);
	}

	gba.gba_name = "gpio";
	gba.gba_gc = &sc->sc_gpio_tag;
	gba.gba_pins = sc->sc_gpio_pins;
	gba.gba_npins = VOYAGER_NGPIO;

	config_found(&sc->sc_dev, &gba, voyager_print);
}
