/*	$OpenBSD: qcgpio.c,v 1.7 2022/11/06 15:33:58 patrick Exp $	*/
/*
 * Copyright (c) 2022 Mark Kettenis <kettenis@openbsd.org>
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

/* Registers. */
#define TLMM_GPIO_IN_OUT(pin)		(0x0004 + 0x1000 * (pin))
#define  TLMM_GPIO_IN_OUT_GPIO_IN			(1 << 0)
#define  TLMM_GPIO_IN_OUT_GPIO_OUT			(1 << 1)
#define TLMM_GPIO_INTR_CFG(pin)		(0x0008 + 0x1000 * (pin))
#define  TLMM_GPIO_INTR_CFG_TARGET_PROC_MASK		(0x7 << 5)
#define  TLMM_GPIO_INTR_CFG_TARGET_PROC_RPM		(0x3 << 5)
#define  TLMM_GPIO_INTR_CFG_INTR_RAW_STATUS_EN		(1 << 4)
#define  TLMM_GPIO_INTR_CFG_INTR_DECT_CTL_MASK		(0x3 << 2)
#define  TLMM_GPIO_INTR_CFG_INTR_DECT_CTL_LEVEL		(0x0 << 2)
#define  TLMM_GPIO_INTR_CFG_INTR_DECT_CTL_EDGE_POS	(0x1 << 2)
#define  TLMM_GPIO_INTR_CFG_INTR_DECT_CTL_EDGE_NEG	(0x2 << 2)
#define  TLMM_GPIO_INTR_CFG_INTR_DECT_CTL_EDGE_BOTH	(0x3 << 2)
#define  TLMM_GPIO_INTR_CFG_INTR_POL_CTL		(1 << 1)
#define  TLMM_GPIO_INTR_CFG_INTR_ENABLE			(1 << 0)
#define TLMM_GPIO_INTR_STATUS(pin)	(0x000c + 0x1000 * (pin))
#define  TLMM_GPIO_INTR_STATUS_INTR_STATUS		(1 << 0)

/* SC7180 has multiple tiles */
#define QCGPIO_SC7180_WEST	0x00100000
#define QCGPIO_SC7180_NORTH	0x00500000
#define QCGPIO_SC7180_SOUTH	0x00900000

#define HREAD4(sc, reg)							\
	(bus_space_read_4((sc)->sc_iot, (sc)->sc_ioh, (reg)))
#define HWRITE4(sc, reg, val)						\
	bus_space_write_4((sc)->sc_iot, (sc)->sc_ioh, (reg), (val))
#define HSET4(sc, reg, bits)						\
	HWRITE4((sc), (reg), HREAD4((sc), (reg)) | (bits))
#define HCLR4(sc, reg, bits)						\
	HWRITE4((sc), (reg), HREAD4((sc), (reg)) & ~(bits))

struct qcgpio_intrhand {
	int (*ih_func)(void *);
	void *ih_arg;
};

struct qcgpio_softc {
	struct device		sc_dev;
	struct acpi_softc	*sc_acpi;
	struct aml_node		*sc_node;

	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;

	void			*sc_ih;

	uint32_t		sc_npins;
	int			(*sc_pin_map)(int, bus_size_t *);
	struct qcgpio_intrhand	*sc_pin_ih;

	struct acpi_gpio sc_gpio;
};

int	qcgpio_acpi_match(struct device *, void *, void *);
void	qcgpio_acpi_attach(struct device *, struct device *, void *);

const struct cfattach qcgpio_acpi_ca = {
	sizeof(struct qcgpio_softc), qcgpio_acpi_match, qcgpio_acpi_attach
};

struct cfdriver qcgpio_cd = {
	NULL, "qcgpio", DV_DULL
};

const char *qcgpio_hids[] = {
	"QCOM060C",
	"QCOM080D",
	NULL
};

int	qcgpio_sc7180_pin_map(int, bus_size_t *);
int	qcgpio_sc8280xp_pin_map(int, bus_size_t *);

int	qcgpio_read_pin(void *, int);
void	qcgpio_write_pin(void *, int, int);
void	qcgpio_intr_establish(void *, int, int, int (*)(void *), void *);
void	qcgpio_intr_enable(void *, int);
void	qcgpio_intr_disable(void *, int);
int	qcgpio_pin_intr(struct qcgpio_softc *, int);
int	qcgpio_intr(void *);

int
qcgpio_acpi_match(struct device *parent, void *match, void *aux)
{
	struct acpi_attach_args *aaa = aux;
	struct cfdata *cf = match;

	if (aaa->aaa_naddr < 1 || aaa->aaa_nirq < 1)
		return 0;
	return acpi_matchhids(aaa, qcgpio_hids, cf->cf_driver->cd_name);
}

void
qcgpio_acpi_attach(struct device *parent, struct device *self, void *aux)
{
	struct acpi_attach_args *aaa = aux;
	struct qcgpio_softc *sc = (struct qcgpio_softc *)self;

	sc->sc_acpi = (struct acpi_softc *)parent;
	sc->sc_node = aaa->aaa_node;
	printf(" %s", sc->sc_node->name);

	printf(" addr 0x%llx/0x%llx", aaa->aaa_addr[0], aaa->aaa_size[0]);

	sc->sc_iot = aaa->aaa_bst[0];
	if (bus_space_map(sc->sc_iot, aaa->aaa_addr[0], aaa->aaa_size[0],
	    0, &sc->sc_ioh)) {
		printf(": can't map registers\n");
		return;
	}

	if (strcmp(aaa->aaa_dev, "QCOM080D") == 0) {
		sc->sc_npins = 119;
		sc->sc_pin_map = qcgpio_sc7180_pin_map;
	} else if (strcmp(aaa->aaa_dev, "QCOM060C") == 0) {
		sc->sc_npins = 228;
		sc->sc_pin_map = qcgpio_sc8280xp_pin_map;
	}
	KASSERT(sc->sc_npins != 0);

	sc->sc_pin_ih = mallocarray(sc->sc_npins, sizeof(*sc->sc_pin_ih),
	    M_DEVBUF, M_WAITOK | M_ZERO);

	printf(" irq %d", aaa->aaa_irq[0]);

	sc->sc_ih = acpi_intr_establish(aaa->aaa_irq[0],
	    aaa->aaa_irq_flags[0], IPL_BIO, qcgpio_intr,
	    sc, sc->sc_dev.dv_xname);
	if (sc->sc_ih == NULL) {
		printf(": can't establish interrupt\n");
		goto unmap;
	}

	sc->sc_gpio.cookie = sc;
	sc->sc_gpio.read_pin = qcgpio_read_pin;
	sc->sc_gpio.write_pin = qcgpio_write_pin;
	sc->sc_gpio.intr_establish = qcgpio_intr_establish;
	sc->sc_gpio.intr_enable = qcgpio_intr_enable;
	sc->sc_gpio.intr_disable = qcgpio_intr_disable;
	sc->sc_node->gpio = &sc->sc_gpio;

	printf("\n");

	acpi_register_gpio(sc->sc_acpi, sc->sc_node);
	return;

unmap:
	if (sc->sc_ih)
		acpi_intr_disestablish(sc->sc_ih);
	free(sc->sc_pin_ih, M_DEVBUF, sc->sc_npins * sizeof(*sc->sc_pin_ih));
	bus_space_unmap(sc->sc_iot, sc->sc_ioh, aaa->aaa_size[0]);
}

int
qcgpio_sc7180_pin_map(int pin, bus_size_t *off)
{
	switch (pin) {
	case 30:
		*off = QCGPIO_SC7180_SOUTH;
		return 30;
	case 32:
	case 0x140:
		*off = QCGPIO_SC7180_NORTH;
		return 32;
	case 33:
	case 0x180:
		*off = QCGPIO_SC7180_NORTH;
		return 33;
	case 94:
	case 0x1c0:
		*off = QCGPIO_SC7180_SOUTH;
		return 94;
	default:
		return -1;
	}
}

int
qcgpio_sc8280xp_pin_map(int pin, bus_size_t *off)
{
	switch (pin) {
	case 107:
	case 175:
		return pin;
	case 0x2c0:
		return 107;
	case 0x340:
		return 104;
	case 0x380:
		return 182;
	default:
		return -1;
	}
}

int
qcgpio_read_pin(void *cookie, int pin)
{
	struct qcgpio_softc *sc = cookie;
	bus_size_t off = 0;
	uint32_t reg;

	pin = sc->sc_pin_map(pin, &off);
	if (pin < 0 || pin >= sc->sc_npins)
		return 0;

	reg = HREAD4(sc, off + TLMM_GPIO_IN_OUT(pin));
	return !!(reg & TLMM_GPIO_IN_OUT_GPIO_IN);
}

void
qcgpio_write_pin(void *cookie, int pin, int val)
{
	struct qcgpio_softc *sc = cookie;
	bus_size_t off = 0;

	pin = sc->sc_pin_map(pin, &off);
	if (pin < 0 || pin >= sc->sc_npins)
		return;

	if (val) {
		HSET4(sc, off + TLMM_GPIO_IN_OUT(pin),
		    TLMM_GPIO_IN_OUT_GPIO_OUT);
	} else {
		HCLR4(sc, off + TLMM_GPIO_IN_OUT(pin),
		    TLMM_GPIO_IN_OUT_GPIO_OUT);
	}
}

void
qcgpio_intr_establish(void *cookie, int pin, int flags,
    int (*func)(void *), void *arg)
{
	struct qcgpio_softc *sc = cookie;
	bus_size_t off = 0;
	uint32_t reg;

	pin = sc->sc_pin_map(pin, &off);
	if (pin < 0 || pin >= sc->sc_npins)
		return;

	sc->sc_pin_ih[pin].ih_func = func;
	sc->sc_pin_ih[pin].ih_arg = arg;

	reg = HREAD4(sc, off + TLMM_GPIO_INTR_CFG(pin));
	reg &= ~TLMM_GPIO_INTR_CFG_INTR_DECT_CTL_MASK;
	reg &= ~TLMM_GPIO_INTR_CFG_INTR_POL_CTL;
	switch (flags & (LR_GPIO_MODE | LR_GPIO_POLARITY)) {
	case LR_GPIO_LEVEL | LR_GPIO_ACTLO:
		reg |= TLMM_GPIO_INTR_CFG_INTR_DECT_CTL_LEVEL;
		break;
	case LR_GPIO_LEVEL | LR_GPIO_ACTHI:
		reg |= TLMM_GPIO_INTR_CFG_INTR_DECT_CTL_LEVEL |
		    TLMM_GPIO_INTR_CFG_INTR_POL_CTL;
		break;
	case LR_GPIO_EDGE | LR_GPIO_ACTLO:
		reg |= TLMM_GPIO_INTR_CFG_INTR_DECT_CTL_EDGE_NEG |
		    TLMM_GPIO_INTR_CFG_INTR_POL_CTL;
		break;
	case LR_GPIO_EDGE | LR_GPIO_ACTHI:
		reg |= TLMM_GPIO_INTR_CFG_INTR_DECT_CTL_EDGE_POS |
		    TLMM_GPIO_INTR_CFG_INTR_POL_CTL;
		break;
	case LR_GPIO_EDGE | LR_GPIO_ACTBOTH:
		reg |= TLMM_GPIO_INTR_CFG_INTR_DECT_CTL_EDGE_BOTH;
		break;
	default:
		printf("%s: unsupported interrupt mode/polarity\n",
		    sc->sc_dev.dv_xname);
		break;
	}
	reg &= ~TLMM_GPIO_INTR_CFG_TARGET_PROC_MASK;
	reg |= TLMM_GPIO_INTR_CFG_TARGET_PROC_RPM;
	reg |= TLMM_GPIO_INTR_CFG_INTR_RAW_STATUS_EN;
	reg |= TLMM_GPIO_INTR_CFG_INTR_ENABLE;
	HWRITE4(sc, off + TLMM_GPIO_INTR_CFG(pin), reg);
}

void
qcgpio_intr_enable(void *cookie, int pin)
{
	struct qcgpio_softc *sc = cookie;
	bus_size_t off = 0;

	pin = sc->sc_pin_map(pin, &off);
	if (pin < 0 || pin >= sc->sc_npins)
		return;

	HSET4(sc, off + TLMM_GPIO_INTR_CFG(pin),
	    TLMM_GPIO_INTR_CFG_INTR_ENABLE);
}

void
qcgpio_intr_disable(void *cookie, int pin)
{
	struct qcgpio_softc *sc = cookie;
	bus_size_t off = 0;

	pin = sc->sc_pin_map(pin, &off);
	if (pin < 0 || pin >= sc->sc_npins)
		return;

	HCLR4(sc, off + TLMM_GPIO_INTR_CFG(pin),
	    TLMM_GPIO_INTR_CFG_INTR_ENABLE);
}

int
qcgpio_intr(void *arg)
{
	struct qcgpio_softc *sc = arg;
	int pin, handled = 0;
	bus_size_t off = 0;
	uint32_t stat;

	for (pin = 0; pin < sc->sc_npins; pin++) {
		if (sc->sc_pin_ih[pin].ih_func == NULL)
			continue;

		sc->sc_pin_map(pin, &off);

		stat = HREAD4(sc, off + TLMM_GPIO_INTR_STATUS(pin));
		if (stat & TLMM_GPIO_INTR_STATUS_INTR_STATUS) {
			sc->sc_pin_ih[pin].ih_func(sc->sc_pin_ih[pin].ih_arg);
			handled = 1;
		}
		HWRITE4(sc, off + TLMM_GPIO_INTR_STATUS(pin),
		    stat & ~TLMM_GPIO_INTR_STATUS_INTR_STATUS);
	}

	return handled;
}
