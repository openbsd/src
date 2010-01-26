/*      $OpenBSD: glxpcib.c,v 1.2 2010/01/26 05:14:11 miod Exp $	*/

/*
 * Copyright (c) 2007 Marc Balmer <mbalmer@openbsd.org>
 * Copyright (c) 2007 Michael Shalayeff
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * AMD CS5536 series LPC bridge also containing timer, watchdog, and GPIO.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/gpio.h>
#include <sys/sysctl.h>
#if 0
#include <sys/timetc.h>
#endif

#include <machine/bus.h>

#include <dev/gpio/gpiovar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <loongson/dev/glxreg.h>
#include <loongson/dev/glxvar.h>

#include "gpio.h"

#define	AMD5536_REV		GLCP_CHIP_REV_ID
#define	AMD5536_REV_MASK	0xff
#define	AMD5536_TMC		PMC_LTMR

/* Multi-Functional General Purpose Timer */
#define	MSR_LBAR_MFGPT		DIVIL_LBAR_MFGPT
#define	AMD5536_MFGPT0_CMP1	0x00000000
#define	AMD5536_MFGPT0_CMP2	0x00000002
#define	AMD5536_MFGPT0_CNT	0x00000004
#define	AMD5536_MFGPT0_SETUP	0x00000006
#define	AMD5536_MFGPT_DIV_MASK	0x000f	/* div = 1 << mask */
#define	AMD5536_MFGPT_CLKSEL	0x0010
#define	AMD5536_MFGPT_REV_EN	0x0020
#define	AMD5536_MFGPT_CMP1DIS	0x0000
#define	AMD5536_MFGPT_CMP1EQ	0x0040
#define	AMD5536_MFGPT_CMP1GE	0x0080
#define	AMD5536_MFGPT_CMP1EV	0x00c0
#define	AMD5536_MFGPT_CMP2DIS	0x0000
#define	AMD5536_MFGPT_CMP2EQ	0x0100
#define	AMD5536_MFGPT_CMP2GE	0x0200
#define	AMD5536_MFGPT_CMP2EV	0x0300
#define	AMD5536_MFGPT_STOP_EN	0x0800
#define	AMD5536_MFGPT_SET	0x1000
#define	AMD5536_MFGPT_CMP1	0x2000
#define	AMD5536_MFGPT_CMP2	0x4000
#define	AMD5536_MFGPT_CNT_EN	0x8000
#define	AMD5536_MFGPT_IRQ	MFGPT_IRQ
#define	AMD5536_MFGPT0_C1_IRQM	0x00000001
#define	AMD5536_MFGPT1_C1_IRQM	0x00000002
#define	AMD5536_MFGPT2_C1_IRQM	0x00000004
#define	AMD5536_MFGPT3_C1_IRQM	0x00000008
#define	AMD5536_MFGPT4_C1_IRQM	0x00000010
#define	AMD5536_MFGPT5_C1_IRQM	0x00000020
#define	AMD5536_MFGPT6_C1_IRQM	0x00000040
#define	AMD5536_MFGPT7_C1_IRQM	0x00000080
#define	AMD5536_MFGPT0_C2_IRQM	0x00000100
#define	AMD5536_MFGPT1_C2_IRQM	0x00000200
#define	AMD5536_MFGPT2_C2_IRQM	0x00000400
#define	AMD5536_MFGPT3_C2_IRQM	0x00000800
#define	AMD5536_MFGPT4_C2_IRQM	0x00001000
#define	AMD5536_MFGPT5_C2_IRQM	0x00002000
#define	AMD5536_MFGPT6_C2_IRQM	0x00004000
#define	AMD5536_MFGPT7_C2_IRQM	0x00008000
#define	AMD5536_MFGPT_NR	MFGPT_NR
#define	AMD5536_MFGPT0_C1_NMIM	0x00000001
#define	AMD5536_MFGPT1_C1_NMIM	0x00000002
#define	AMD5536_MFGPT2_C1_NMIM	0x00000004
#define	AMD5536_MFGPT3_C1_NMIM	0x00000008
#define	AMD5536_MFGPT4_C1_NMIM	0x00000010
#define	AMD5536_MFGPT5_C1_NMIM	0x00000020
#define	AMD5536_MFGPT6_C1_NMIM	0x00000040
#define	AMD5536_MFGPT7_C1_NMIM	0x00000080
#define	AMD5536_MFGPT0_C2_NMIM	0x00000100
#define	AMD5536_MFGPT1_C2_NMIM	0x00000200
#define	AMD5536_MFGPT2_C2_NMIM	0x00000400
#define	AMD5536_MFGPT3_C2_NMIM	0x00000800
#define	AMD5536_MFGPT4_C2_NMIM	0x00001000
#define	AMD5536_MFGPT5_C2_NMIM	0x00002000
#define	AMD5536_MFGPT6_C2_NMIM	0x00004000
#define	AMD5536_MFGPT7_C2_NMIM	0x00008000
#define	AMD5536_NMI_LEG		0x00010000
#define	AMD5536_MFGPT0_C2_RSTEN	0x01000000
#define	AMD5536_MFGPT1_C2_RSTEN	0x02000000
#define	AMD5536_MFGPT2_C2_RSTEN	0x04000000
#define	AMD5536_MFGPT3_C2_RSTEN	0x08000000
#define	AMD5536_MFGPT4_C2_RSTEN	0x10000000
#define	AMD5536_MFGPT5_C2_RSTEN	0x20000000
#define	AMD5536_MFGPT_SETUP	MFGPT_SETUP

/* GPIO */
#define	MSR_LBAR_GPIO		DIVIL_LBAR_GPIO
#define	AMD5536_GPIO_NPINS	32
#define	AMD5536_GPIOH_OFFSET	0x80	/* high bank register offset */
#define	AMD5536_GPIO_OUT_VAL	0x00	/* output value */
#define	AMD5536_GPIO_OUT_EN	0x04	/* output enable */
#define	AMD5536_GPIO_OD_EN	0x08	/* open-drain enable */
#define AMD5536_GPIO_OUT_INVRT_EN 0x0c	/* invert output */
#define	AMD5536_GPIO_PU_EN	0x18	/* pull-up enable */
#define	AMD5536_GPIO_PD_EN	0x1c	/* pull-down enable */
#define	AMD5536_GPIO_IN_EN	0x20	/* input enable */
#define AMD5536_GPIO_IN_INVRT_EN 0x24	/* invert input */
#define	AMD5536_GPIO_READ_BACK	0x30	/* read back value */

struct glxpcib_softc {
	struct device		sc_dev;

#if 0
	struct timecounter	sc_timecounter;
#endif
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;

#if NGPIO > 0
	/* GPIO interface */
	bus_space_tag_t		sc_gpio_iot;
	bus_space_handle_t	sc_gpio_ioh;
	struct gpio_chipset_tag	sc_gpio_gc;
	gpio_pin_t		sc_gpio_pins[AMD5536_GPIO_NPINS];
#endif
};

struct cfdriver glxpcib_cd = {
	NULL, "glxpcib", DV_DULL
};

int	glxpcib_match(struct device *, void *, void *);
void	glxpcib_attach(struct device *, struct device *, void *);

struct cfattach glxpcib_ca = {
	sizeof(struct glxpcib_softc), glxpcib_match, glxpcib_attach
};

/* from arch/<*>/pci/pcib.c */
void	pcibattach(struct device *parent, struct device *self, void *aux);

#if 0
u_int	glxpcib_get_timecount(struct timecounter *tc);
#endif

#if NGPIO > 0
void	glxpcib_gpio_pin_ctl(void *, int, int);
int	glxpcib_gpio_pin_read(void *, int);
void	glxpcib_gpio_pin_write(void *, int, int);
int     glxpcib_wdogctl_cb(void *, int);
#endif

const struct pci_matchid glxpcib_devices[] = {
	{ PCI_VENDOR_AMD,	PCI_PRODUCT_AMD_CS5536_PCIB }
};

int
glxpcib_match(struct device *parent, void *match, void *aux)
{ 
	if (pci_matchbyid((struct pci_attach_args *)aux, glxpcib_devices,
	    sizeof(glxpcib_devices) / sizeof(glxpcib_devices[0])))
		return 2;

	return 0;
}

void
glxpcib_attach(struct device *parent, struct device *self, void *aux)
{
#if (NGPIO > 0) || 0
	struct glxpcib_softc *sc = (struct glxpcib_softc *)self;
#endif
#if 0
	struct timecounter *tc = &sc->sc_timecounter;
#endif
#if NGPIO > 0
	u_int64_t wa, ga;
	struct gpiobus_attach_args gba;
	int i, gpio = 0;
#endif

	printf(": rev %d",
	    (int)rdmsr(AMD5536_REV) & AMD5536_REV_MASK);
#if 0
	tc->tc_get_timecount = glxpcib_get_timecount;
	tc->tc_counter_mask = 0xffffffff;
	tc->tc_frequency = 3579545;
	tc->tc_name = "CS5536";
	tc->tc_quality = 1000;
	tc->tc_priv = sc;
	tc_init(tc);

	printf(", 32-bit %lluHz timer",
	    tc->tc_frequency);
#endif

#if NGPIO > 0
	/* Attach the watchdog timer */
	sc->sc_iot = pa->pa_iot;
	wa = rdmsr(MSR_LBAR_MFGPT);
	if (wa & 0x100000000ULL &&
	    !bus_space_map(sc->sc_iot, wa & 0xffff, 64, 0, &sc->sc_ioh)) {

		/* count in seconds (as upper level desires) */
		bus_space_write_2(sc->sc_iot, sc->sc_ioh, AMD5536_MFGPT0_SETUP,
		    AMD5536_MFGPT_CNT_EN | AMD5536_MFGPT_CMP2EV |
		    AMD5536_MFGPT_CMP2 | AMD5536_MFGPT_DIV_MASK);
		wdog_register(sc, glxpcib_wdogctl_cb);
		printf(", watchdog");
	}

	/* map GPIO I/O space */
	sc->sc_gpio_iot = pa->pa_iot;
	ga = rdmsr(MSR_LBAR_GPIO);
	if (ga & 0x100000000ULL &&
	    !bus_space_map(sc->sc_gpio_iot, ga & 0xffff, 0xff, 0,
	    &sc->sc_gpio_ioh)) {
		printf(", gpio");

		/* initialize pin array */
		for (i = 0; i < AMD5536_GPIO_NPINS; i++) {
			sc->sc_gpio_pins[i].pin_num = i;
			sc->sc_gpio_pins[i].pin_caps = GPIO_PIN_INPUT |
			    GPIO_PIN_OUTPUT | GPIO_PIN_OPENDRAIN |
			    GPIO_PIN_PULLUP | GPIO_PIN_PULLDOWN |
			    GPIO_PIN_INVIN | GPIO_PIN_INVOUT;

			/* read initial state */
			sc->sc_gpio_pins[i].pin_state =
			    glxpcib_gpio_pin_read(sc, i);
		}

		/* create controller tag */
		sc->sc_gpio_gc.gp_cookie = sc;
		sc->sc_gpio_gc.gp_pin_read = glxpcib_gpio_pin_read;
		sc->sc_gpio_gc.gp_pin_write = glxpcib_gpio_pin_write;
		sc->sc_gpio_gc.gp_pin_ctl = glxpcib_gpio_pin_ctl;

		gba.gba_name = "gpio";
		gba.gba_gc = &sc->sc_gpio_gc;
		gba.gba_pins = sc->sc_gpio_pins;
		gba.gba_npins = AMD5536_GPIO_NPINS;
		gpio = 1;

	}
#endif
	pcibattach(parent, self, aux);

#if NGPIO > 0
	if (gpio)
		config_found(&sc->sc_dev, &gba, gpiobus_print);
#endif
}

#if 0
u_int
glxpcib_get_timecount(struct timecounter *tc)
{
        return rdmsr(AMD5536_TMC);
}
#endif

#if NGPIO > 0
int
glxpcib_wdogctl_cb(void *v, int period)
{
	struct glxpcib_softc *sc = v;

	if (period > 0xffff)
		period = 0xffff;

	bus_space_write_2(sc->sc_iot, sc->sc_ioh, AMD5536_MFGPT0_SETUP,
	    AMD5536_MFGPT_CNT_EN | AMD5536_MFGPT_CMP2);
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, AMD5536_MFGPT0_CNT, 0);
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, AMD5536_MFGPT0_CMP2, period);

	if (period)
		wrmsr(AMD5536_MFGPT_NR,
		    rdmsr(AMD5536_MFGPT_NR) | AMD5536_MFGPT0_C2_RSTEN);
	else
		wrmsr(AMD5536_MFGPT_NR,
		    rdmsr(AMD5536_MFGPT_NR) & ~AMD5536_MFGPT0_C2_RSTEN);

	return period;
}

int
glxpcib_gpio_pin_read(void *arg, int pin)
{
	struct glxpcib_softc *sc = arg;
	u_int32_t data;
	int reg, off = 0;

	reg = AMD5536_GPIO_IN_EN;
	if (pin > 15) {
		pin &= 0x0f;
		off = AMD5536_GPIOH_OFFSET;
	}
	reg += off;
	data = bus_space_read_4(sc->sc_gpio_iot, sc->sc_gpio_ioh, reg);

	if (data & (1 << pin))
		reg = AMD5536_GPIO_READ_BACK + off;
	else
		reg = AMD5536_GPIO_OUT_VAL + off;

	data = bus_space_read_4(sc->sc_gpio_iot, sc->sc_gpio_ioh, reg);

	return data & 1 << pin ? GPIO_PIN_HIGH : GPIO_PIN_LOW;
}

void
glxpcib_gpio_pin_write(void *arg, int pin, int value)
{
	struct glxpcib_softc *sc = arg;
	u_int32_t data;
	int reg;

	reg = AMD5536_GPIO_OUT_VAL;
	if (pin > 15) {
		pin &= 0x0f;
		reg += AMD5536_GPIOH_OFFSET;
	}
	if (value == 1)
		data = 1 << pin;
	else
		data = 1 << (pin + 16);

	bus_space_write_4(sc->sc_gpio_iot, sc->sc_gpio_ioh, reg, data);
}

void
glxpcib_gpio_pin_ctl(void *arg, int pin, int flags)
{
	struct glxpcib_softc *sc = arg;
	int n, reg[7], val[7], nreg = 0, off = 0;

	if (pin > 15) {
		pin &= 0x0f;
		off = AMD5536_GPIOH_OFFSET;
	}

	reg[nreg] = AMD5536_GPIO_IN_EN + off;
	if (flags & GPIO_PIN_INPUT)
		val[nreg++] = 1 << pin;
	else
		val[nreg++] = 1 << (pin + 16);

	reg[nreg] = AMD5536_GPIO_OUT_EN + off;
	if (flags & GPIO_PIN_OUTPUT)
		val[nreg++] = 1 << pin;
	else
		val[nreg++] = 1 << (pin + 16);

	reg[nreg] = AMD5536_GPIO_OD_EN + off;
	if (flags & GPIO_PIN_OPENDRAIN)
		val[nreg++] = 1 << pin;
	else
		val[nreg++] = 1 << (pin + 16);

	reg[nreg] = AMD5536_GPIO_PU_EN + off;
	if (flags & GPIO_PIN_PULLUP)
		val[nreg++] = 1 << pin;
	else
		val[nreg++] = 1 << (pin + 16);

	reg[nreg] = AMD5536_GPIO_PD_EN + off;
	if (flags & GPIO_PIN_PULLDOWN)
		val[nreg++] = 1 << pin;
	else
		val[nreg++] = 1 << (pin + 16);

	reg[nreg] = AMD5536_GPIO_IN_INVRT_EN + off;
	if (flags & GPIO_PIN_INVIN)
		val[nreg++] = 1 << pin;
	else
		val[nreg++] = 1 << (pin + 16);

	reg[nreg] = AMD5536_GPIO_OUT_INVRT_EN + off;
	if (flags & GPIO_PIN_INVOUT)
		val[nreg++] = 1 << pin;
	else
		val[nreg++] = 1 << (pin + 16);

	/* set flags */
	for (n = 0; n < nreg; n++)
		bus_space_write_4(sc->sc_gpio_iot, sc->sc_gpio_ioh, reg[n],
		    val[n]);
} 

#endif
