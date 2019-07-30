/*	$OpenBSD: bytgpio.c,v 1.12 2016/10/25 06:48:58 pirofti Exp $	*/
/*
 * Copyright (c) 2016 Mark Kettenis
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

#define BYTGPIO_CONF_GD_LEVEL	0x01000000
#define BYTGPIO_CONF_GD_TPE	0x02000000
#define BYTGPIO_CONF_GD_TNE	0x04000000
#define BYTGPIO_CONF_GD_MASK	0x07000000
#define BYTGPIO_CONF_DIRECT_IRQ_EN	0x08000000

#define BYTGPIO_PAD_VAL		0x00000001

#define BYTGPIO_IRQ_TS_0	0x800
#define BYTGPIO_IRQ_TS_1	0x804
#define BYTGPIO_IRQ_TS_2	0x808

struct bytgpio_intrhand {
	int (*ih_func)(void *);
	void *ih_arg;
};

struct bytgpio_softc {
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

	const int *sc_pins;
	int sc_npins;
	struct bytgpio_intrhand *sc_pin_ih;

	struct acpi_gpio sc_gpio;
};

int	bytgpio_match(struct device *, void *, void *);
void	bytgpio_attach(struct device *, struct device *, void *);

struct cfattach bytgpio_ca = {
	sizeof(struct bytgpio_softc), bytgpio_match, bytgpio_attach
};

struct cfdriver bytgpio_cd = {
	NULL, "bytgpio", DV_DULL
};

const char *bytgpio_hids[] = {
	"INT33FC",
	NULL
};

/*
 * The pads for the pins are randomly ordered.
 */

const int byt_score_pins[] = {
	85, 89, 93, 96, 99, 102, 98, 101, 34, 37, 36, 38, 39, 35, 40,
	84, 62, 61, 64, 59, 54, 56, 60, 55, 63, 57, 51, 50, 53, 47,
	52, 49, 48, 43, 46, 41, 45, 42, 58, 44, 95, 105, 70, 68, 67,
	66, 69, 71, 65, 72, 86, 90, 88, 92, 103, 77, 79, 83, 78, 81,
	80, 82, 13, 12, 15, 14, 17, 18, 19, 16, 2, 1, 0, 4, 6, 7, 9,
	8, 33, 32, 31, 30, 29, 27, 25, 28, 26, 23, 21, 20, 24, 22, 5,
	3, 10, 11, 106, 87, 91, 104, 97, 100
};

const int byt_ncore_pins[] = {
	19, 18, 17, 20, 21, 22, 24, 25, 23, 16, 14, 15, 12, 26, 27,
	1, 4, 8, 11, 0, 3, 6, 10, 13, 2, 5, 9, 7
};

const int byt_sus_pins[] = {
        29, 33, 30, 31, 32, 34, 36, 35, 38, 37, 18, 7, 11, 20, 17, 1,
	8, 10, 19, 12, 0, 2, 23, 39, 28, 27, 22, 21, 24, 25, 26, 51,
	56, 54, 49, 55, 48, 57, 50, 58, 52, 53, 59, 40
};

int	bytgpio_parse_resources(int, union acpi_resource *, void *);
int	bytgpio_read_pin(void *, int);
void	bytgpio_write_pin(void *, int, int);
void	bytgpio_intr_establish(void *, int, int, int (*)(), void *);
int	bytgpio_intr(void *);

int
bytgpio_match(struct device *parent, void *match, void *aux)
{
	struct acpi_attach_args *aaa = aux;
	struct cfdata *cf = match;

	return acpi_matchhids(aaa, bytgpio_hids, cf->cf_driver->cd_name);
}

void
bytgpio_attach(struct device *parent, struct device *self, void *aux)
{
	struct acpi_attach_args *aaa = aux;
	struct bytgpio_softc *sc = (struct bytgpio_softc *)self;
	struct aml_value res;
	struct aml_value arg[2];
	struct aml_node *node;
	int64_t uid;
	uint32_t reg;
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
		sc->sc_pins = byt_score_pins;
		sc->sc_npins = nitems(byt_score_pins);
		break;
	case 2:
		sc->sc_pins = byt_ncore_pins;
		sc->sc_npins = nitems(byt_ncore_pins);
		break;
	case 3:
		sc->sc_pins = byt_sus_pins;
		sc->sc_npins = nitems(byt_sus_pins);
		break;
	default:
		printf("\n");
		return;
	}

	if (aml_evalname(sc->sc_acpi, sc->sc_node, "_CRS", 0, NULL, &res)) {
		printf(", can't find registers\n");
		return;
	}

	aml_parse_resource(&res, bytgpio_parse_resources, sc);
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
	    bytgpio_intr, sc, sc->sc_dev.dv_xname);
	if (sc->sc_ih == NULL) {
		printf(", can't establish interrupt\n");
		goto unmap;
	}

	sc->sc_gpio.cookie = sc;
	sc->sc_gpio.read_pin = bytgpio_read_pin;
	sc->sc_gpio.write_pin = bytgpio_write_pin;
	sc->sc_gpio.intr_establish = bytgpio_intr_establish;
	sc->sc_node->gpio = &sc->sc_gpio;

	/* Mask all interrupts. */
	for (i = 0; i < sc->sc_npins; i++) {
		reg = bus_space_read_4(sc->sc_memt, sc->sc_memh, sc->sc_pins[i] * 16);

		/*
		 * Skip pins configured as direct IRQ.  Those are tied
		 * directly to the APIC.
		 */
		if (reg & BYTGPIO_CONF_DIRECT_IRQ_EN)
			continue;

		reg &= ~BYTGPIO_CONF_GD_MASK;
		bus_space_write_4(sc->sc_memt, sc->sc_memh, sc->sc_pins[i] * 16, reg);
	}

	printf(", %d pins\n", sc->sc_npins);

	/* Register address space. */
	memset(&arg, 0, sizeof(arg));
	arg[0].type = AML_OBJTYPE_INTEGER;
	arg[0].v_integer = ACPI_OPREG_GPIO;
	arg[1].type = AML_OBJTYPE_INTEGER;
	arg[1].v_integer = 1;
	node = aml_searchname(sc->sc_node, "_REG");
	if (node && aml_evalnode(sc->sc_acpi, node, 2, arg, NULL))
		printf("%s: _REG failed\n", sc->sc_dev.dv_xname);
		
	return;

unmap:
	bus_space_unmap(sc->sc_memt, sc->sc_memh, sc->sc_size);
free:
	free(sc->sc_pin_ih, M_DEVBUF, sc->sc_npins * sizeof(*sc->sc_pin_ih));
}

int
bytgpio_parse_resources(int crsidx, union acpi_resource *crs, void *arg)
{
	struct bytgpio_softc *sc = arg;
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
bytgpio_read_pin(void *cookie, int pin)
{
	struct bytgpio_softc *sc = cookie;
	uint32_t reg;

	reg = bus_space_read_4(sc->sc_memt, sc->sc_memh, sc->sc_pins[pin] * 16 + 8);
	return (reg & BYTGPIO_PAD_VAL);
}

void
bytgpio_write_pin(void *cookie, int pin, int value)
{
	struct bytgpio_softc *sc = cookie;
	uint32_t reg;

	reg = bus_space_read_4(sc->sc_memt, sc->sc_memh, sc->sc_pins[pin] * 16 + 8);
	if (value)
		reg |= BYTGPIO_PAD_VAL;
	else
		reg &= ~BYTGPIO_PAD_VAL;
	bus_space_write_4(sc->sc_memt, sc->sc_memh, sc->sc_pins[pin] * 16 + 8, reg);
}

void
bytgpio_intr_establish(void *cookie, int pin, int flags,
    int (*func)(void *), void *arg)
{
	struct bytgpio_softc *sc = cookie;
	uint32_t reg;

	KASSERT(pin >= 0 && pin < sc->sc_npins);

	sc->sc_pin_ih[pin].ih_func = func;
	sc->sc_pin_ih[pin].ih_arg = arg;

	reg = bus_space_read_4(sc->sc_memt, sc->sc_memh, sc->sc_pins[pin] * 16);
	reg &= ~BYTGPIO_CONF_GD_MASK;
	if ((flags & LR_GPIO_MODE) == 0)
		reg |= BYTGPIO_CONF_GD_LEVEL;
	if ((flags & LR_GPIO_POLARITY) == LR_GPIO_ACTLO)
		reg |= BYTGPIO_CONF_GD_TNE;
	if ((flags & LR_GPIO_POLARITY) == LR_GPIO_ACTHI)
		reg |= BYTGPIO_CONF_GD_TPE;
	if ((flags & LR_GPIO_POLARITY) == LR_GPIO_ACTBOTH)
		reg |= BYTGPIO_CONF_GD_TNE | BYTGPIO_CONF_GD_TPE;
	bus_space_write_4(sc->sc_memt, sc->sc_memh, sc->sc_pins[pin] * 16, reg);
}

int
bytgpio_intr(void *arg)
{
	struct bytgpio_softc *sc = arg;
	uint32_t reg;
	int rc = 0;
	int pin;

	for (pin = 0; pin < sc->sc_npins; pin++) {
		if (pin % 32 == 0) {
			reg = bus_space_read_4(sc->sc_memt, sc->sc_memh,
			    BYTGPIO_IRQ_TS_0 + (pin / 8));
			bus_space_write_4(sc->sc_memt, sc->sc_memh,
			    BYTGPIO_IRQ_TS_0 + (pin / 8), reg);
		}
		if (reg & (1 << (pin % 32))) {
			if (sc->sc_pin_ih[pin].ih_func)
				sc->sc_pin_ih[pin].ih_func(sc->sc_pin_ih[pin].ih_arg);
			rc = 1;
		}
	}

	return rc;
}
