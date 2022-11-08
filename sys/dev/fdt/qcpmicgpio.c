/*	$OpenBSD: qcpmicgpio.c,v 1.1 2022/11/08 19:42:10 patrick Exp $	*/
/*
 * Copyright (c) 2022 Patrick Wildt <patrick@blueri.se>
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

#include <machine/bus.h>
#include <machine/fdt.h>

#include <dev/fdt/spmivar.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_gpio.h>
#include <dev/ofw/fdt.h>

/* Registers. */
#define GPIO_TYPE		0x04
#define  GPIO_TYPE_VAL			0x10
#define GPIO_SUBTYPE		0x05
#define  GPIO_SUBTYPE_GPIO_4CH		0x1
#define  GPIO_SUBTYPE_GPIOC_4CH		0x5
#define  GPIO_SUBTYPE_GPIO_8CH		0x9
#define  GPIO_SUBTYPE_GPIOC_8CH		0xd
#define  GPIO_SUBTYPE_GPIO_LV		0x10
#define  GPIO_SUBTYPE_GPIO_MV		0x11
#define GPIO_PIN_OFF(x)		(0x100 * (x))
#define GPIO_PIN_STATUS		0x10
#define  GPIO_PIN_STATUS_ON		(1 << 0)

struct qcpmicgpio_softc {
	struct device		sc_dev;
	int			sc_node;

	spmi_tag_t		sc_tag;
	int8_t			sc_sid;
	uint16_t		sc_addr;

	int			sc_npins;
	struct gpio_controller	sc_gc;
};

int	qcpmicgpio_match(struct device *, void *, void *);
void	qcpmicgpio_attach(struct device *, struct device *, void *);

const struct cfattach qcpmicgpio_ca = {
	sizeof(struct qcpmicgpio_softc), qcpmicgpio_match, qcpmicgpio_attach
};

struct cfdriver qcpmicgpio_cd = {
	NULL, "qcpmicgpio", DV_DULL
};

void	qcpmicgpio_config_pin(void *, uint32_t *, int);
int	qcpmicgpio_get_pin(void *, uint32_t *);
void	qcpmicgpio_set_pin(void *, uint32_t *, int);

int
qcpmicgpio_match(struct device *parent, void *match, void *aux)
{
	struct spmi_attach_args *saa = aux;

	return OF_is_compatible(saa->sa_node, "qcom,spmi-gpio");
}

void
qcpmicgpio_attach(struct device *parent, struct device *self, void *aux)
{
	struct spmi_attach_args *saa = aux;
	struct qcpmicgpio_softc *sc = (struct qcpmicgpio_softc *)self;
	int reg;

	reg = OF_getpropint(saa->sa_node, "reg", -1);
	if (reg < 0) {
		printf(": can't find registers\n");
		return;
	}

	sc->sc_node = saa->sa_node;
	sc->sc_tag = saa->sa_tag;
	sc->sc_sid = saa->sa_sid;
	sc->sc_addr = reg;

	if (OF_is_compatible(saa->sa_node, "qcom,pm8350-gpio"))
		sc->sc_npins = 10;
	if (OF_is_compatible(saa->sa_node, "qcom,pm8350c-gpio"))
		sc->sc_npins = 9;
	if (OF_is_compatible(saa->sa_node, "qcom,pmr735a-gpio"))
		sc->sc_npins = 4;

	if (!sc->sc_npins) {
		printf(": no pins\n");
		return;
	}

	sc->sc_gc.gc_node = saa->sa_node;
	sc->sc_gc.gc_cookie = sc;
	sc->sc_gc.gc_config_pin = qcpmicgpio_config_pin;
	sc->sc_gc.gc_get_pin = qcpmicgpio_get_pin;
	sc->sc_gc.gc_set_pin = qcpmicgpio_set_pin;
	gpio_controller_register(&sc->sc_gc);

	printf("\n");
}

void
qcpmicgpio_config_pin(void *cookie, uint32_t *cells, int config)
{
	struct qcpmicgpio_softc *sc = cookie;
	uint32_t pin = cells[0];

	if (pin >= sc->sc_npins)
		return;

	printf("%s\n", __func__);
}

int
qcpmicgpio_get_pin(void *cookie, uint32_t *cells)
{
	struct qcpmicgpio_softc *sc = cookie;
	uint32_t pin = cells[0];
	uint32_t flags = cells[1];
	int error, val;
	uint32_t reg;

	if (pin >= sc->sc_npins)
		return 0;

	error = spmi_cmd_read(sc->sc_tag, sc->sc_sid, SPMI_CMD_EXT_READL,
	    sc->sc_addr + GPIO_PIN_OFF(pin) + GPIO_PIN_STATUS,
	    &reg, sizeof(reg));
	if (error) {
		printf("%s: error reading GPIO\n", sc->sc_dev.dv_xname);
		return error;
	}

	printf("%s: reg %x\n", sc->sc_dev.dv_xname, reg);
	val = !!(reg & GPIO_PIN_STATUS_ON);
	if (flags & GPIO_ACTIVE_LOW)
		val = !val;
	return val;
}

void
qcpmicgpio_set_pin(void *cookie, uint32_t *cells, int val)
{
	struct qcpmicgpio_softc *sc = cookie;
	uint32_t pin = cells[0];
	uint32_t flags = cells[1];

	if (pin >= sc->sc_npins)
		return;

	if (flags & GPIO_ACTIVE_LOW)
		val = !val;

	printf("%s\n", __func__);
}
