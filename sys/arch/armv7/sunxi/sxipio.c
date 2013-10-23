/*	$OpenBSD: sxipio.c,v 1.1 2013/10/23 17:08:48 jasper Exp $	*/
/*
 * Copyright (c) 2010 Miodrag Vallat.
 * Copyright (c) 2013 Artturi Alm
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
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/gpio.h>
#include <sys/evcount.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/gpio/gpiovar.h>

#include <armv7/sunxi/sunxivar.h>
#include <armv7/sunxi/sxipiovar.h>

#define	AWPIO_NPORT		9
#define	AWPIO_PA_NPIN		18
#define	AWPIO_PB_NPIN		24
#define	AWPIO_PC_NPIN		25
#define	AWPIO_PD_NPIN		28
#define	AWPIO_PE_NPIN		12
#define	AWPIO_PF_NPIN		6
#define	AWPIO_PG_NPIN		12
#define	AWPIO_PH_NPIN		28
#define	AWPIO_PI_NPIN		22
#define	AWPIO_NPIN	(AWPIO_PA_NPIN + AWPIO_PB_NPIN + AWPIO_PC_NPIN + \
	AWPIO_PD_NPIN + AWPIO_PE_NPIN + AWPIO_PF_NPIN + AWPIO_PG_NPIN + \
	AWPIO_PH_NPIN + AWPIO_PI_NPIN)
#define	AWPIO_PS_NPIN		84 /* for DRAM controller */


struct intrhand {
	int (*ih_func)(void *);		/* handler */
	void *ih_arg;			/* arg for handler */
	int ih_ipl;			/* IPL_* */
	int ih_irq;			/* IRQ number */
	int ih_gpio;			/* gpio pin */
	struct evcount ih_count;
	char *ih_name;
};

struct sxipio_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	void			*sc_ih_h;
	void			*sc_ih_l;
	int 			sc_max_il;
	int 			sc_min_il;
	int			sc_irq;

	struct gpio_chipset_tag	 sc_gpio_tag[AWPIO_NPORT];
	gpio_pin_t		 sc_gpio_pins[AWPIO_NPORT][32];

	struct intrhand		*sc_handlers[32];
};

#define	AWPIO_CFG(port, pin)	0x00 + ((port) * 0x24) + ((pin) << 2)
#define	AWPIO_DAT(port)		0x10 + ((port) * 0x24)
/* XXX add support for registers below */
#define	AWPIO_DRV(port, pin)	0x14 + ((port) * 0x24) + ((pin) << 2)
#define	AWPIO_PUL(port, pin)	0x1c + ((port) * 0x24) + ((pin) << 2)
#define	AWPIO_INT_CFG0(port)	0x0200 + ((port) * 0x04)
#define	AWPIO_INT_CTL		0x0210
#define	AWPIO_INT_STA		0x0214
#define	AWPIO_INT_DEB		0x0218 /* debounce register */

void sxipio_attach(struct device *, struct device *, void *);
void sxipio_attach_gpio(struct device *);

struct cfattach sxipio_ca = {
	sizeof (struct sxipio_softc), NULL, sxipio_attach
};

struct cfdriver sxipio_cd = {
	NULL, "sxipio", DV_DULL
};

struct sxipio_softc	*sxipio_sc = NULL;
bus_space_tag_t		 sxipio_iot;
bus_space_handle_t	 sxipio_ioh;

void
sxipio_attach(struct device *parent, struct device *self, void *args)
{
	struct sxipio_softc *sc = (struct sxipio_softc *)self;
	struct sxi_attach_args *sxi = args;

	/* XXX check unit, bail if != 0 */

	sc->sc_iot = sxipio_iot = sxi->sxi_iot;
	if (bus_space_map(sxipio_iot, sxi->sxi_dev->mem[0].addr,
	    sxi->sxi_dev->mem[0].size, 0, &sc->sc_ioh))
		panic("sxipio_attach: bus_space_map failed!");
	sxipio_ioh = sc->sc_ioh;

	sxipio_sc = sc;

	sc->sc_irq = sxi->sxi_dev->irq[0];
	sxipio_setcfg(AWPIO_LED_GREEN, AWPIO_OUTPUT);
	sxipio_setcfg(AWPIO_LED_BLUE, AWPIO_OUTPUT);
	sxipio_setpin(AWPIO_LED_GREEN);
	sxipio_setpin(AWPIO_LED_BLUE);

	config_defer(self, sxipio_attach_gpio);

	printf("\n");
}

/*
 * GPIO support code
 */

int	sxipio_pin_read(void *, int);
void	sxipio_pin_write(void *, int, int);
void	sxipio_pin_ctl(void *, int, int);

static const struct gpio_chipset_tag sxipio_gpio_tag = {
	.gp_pin_read = sxipio_pin_read,
	.gp_pin_write = sxipio_pin_write,
	.gp_pin_ctl = sxipio_pin_ctl
};

int
sxipio_pin_read(void *portno, int pin)
{
	return sxipio_getpin((*(uint32_t *)portno * 32) + pin)
	    ? GPIO_PIN_HIGH : GPIO_PIN_LOW;
}

void
sxipio_pin_write(void *portno, int pin, int val)
{
	if (val)
		sxipio_setpin((*(uint32_t *)portno * 32) + pin);
	else
		sxipio_clrpin((*(uint32_t *)portno * 32) + pin);
}

void
sxipio_pin_ctl(void *portno, int pin, int flags)
{
	if (ISSET(flags, GPIO_PIN_OUTPUT))
		sxipio_setcfg((*(uint32_t *)portno * 32) + pin, AWPIO_OUTPUT);
	else
		sxipio_setcfg((*(uint32_t *)portno * 32) + pin, AWPIO_INPUT);
}

/* XXX ugly, but cookie has no other purposeful use. */
static const uint32_t sxipio_ports[AWPIO_NPORT] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8
};

static const int sxipio_last_pin[AWPIO_NPORT] = {
	AWPIO_PA_NPIN,
	AWPIO_PB_NPIN,
	AWPIO_PC_NPIN,
	AWPIO_PD_NPIN,
	AWPIO_PE_NPIN,
	AWPIO_PF_NPIN,
	AWPIO_PG_NPIN,
	AWPIO_PH_NPIN,
	AWPIO_PI_NPIN
};

void
sxipio_attach_gpio(struct device *parent)
{
	struct sxipio_softc *sc = (struct sxipio_softc *)parent;
	struct gpiobus_attach_args gba;
	int cfg, pin, port;
	/* get value & state of enabled pins, and disable those in use */
	pin = 0;
	port = 0;
next:
	sc->sc_gpio_pins[port][pin].pin_num = pin;
	cfg = sxipio_getcfg((port * 32) + pin);
#if DEBUG
	printf("port %d pin %d cfg %d\n", port, pin, cfg);
#endif
	if (cfg < 2) {
		sc->sc_gpio_pins[port][pin].pin_caps =
		    GPIO_PIN_INPUT | GPIO_PIN_OUTPUT;
		sc->sc_gpio_pins[port][pin].pin_state =
		    sxipio_getpin((port * 32) + pin);
		sc->sc_gpio_pins[port][pin].pin_flags = GPIO_PIN_SET |
		    cfg ? GPIO_PIN_OUTPUT : GPIO_PIN_INPUT;
	} else {
		/* disable control of taken over pins */
		sc->sc_gpio_pins[port][pin].pin_caps = 0;
		sc->sc_gpio_pins[port][pin].pin_state = 0;
		sc->sc_gpio_pins[port][pin].pin_flags = 0;
	}

	if (++pin < sxipio_last_pin[port])
		goto next;

	bcopy(&sxipio_gpio_tag, &sc->sc_gpio_tag[port], sizeof(sxipio_gpio_tag));
	sc->sc_gpio_tag[port].gp_cookie = (void *)&sxipio_ports[port];
	gba.gba_name = "gpio";
	gba.gba_gc = &sc->sc_gpio_tag[port];
	gba.gba_pins = &sc->sc_gpio_pins[port][0];
	gba.gba_npins = pin;

	config_found(&sc->sc_dev, &gba, gpiobus_print);

	pin = 0;
	if (++port < AWPIO_NPORT)
		goto next;
}

/*
 * Lower Port I/O for MD code under arch/armv7/sunxi.
 * XXX see the comment in sxipiovar.h
 */
int
sxipio_getcfg(int pin)
{
	struct sxipio_softc *sc = sxipio_sc;
	uint32_t bit, data, off, reg, port;
	int s;

	port = pin >> 5;
	bit = pin - (port << 5);
	reg = AWPIO_CFG(port, bit >> 3);
	off = (bit & 7) << 2;

	s = splhigh();

	data = AWREAD4(sc, reg);

	splx(s);

	return data >> off & 7;
}

void
sxipio_setcfg(int pin, int mux)
{
	struct sxipio_softc *sc = sxipio_sc;
	uint32_t bit, cmask, mask, off, reg, port;
	int s;

	port = pin >> 5;
	bit = pin - (port << 5);
	reg = AWPIO_CFG(port, bit >> 3);
	off = (bit & 7) << 2;
	cmask = 7 << off;
	mask = mux << off;

	s = splhigh();

	AWCMS4(sc, reg, cmask, mask);

	splx(s);
}

int
sxipio_getpin(int pin)
{
	struct sxipio_softc *sc = sxipio_sc;
	uint32_t bit, data, mask, reg, port;
	int s;

	port = pin >> 5;
	bit = pin - (port << 5);
	reg = AWPIO_DAT(port);
	mask = 1 << (bit & 31);

	s = splhigh();

	data = AWREAD4(sc, reg);

	splx(s);

	return data & mask ? 1 : 0;
}

void
sxipio_setpin(int pin)
{
	struct sxipio_softc *sc = sxipio_sc;
	uint32_t bit, mask, reg, port;
	int s;

	port = pin >> 5;
	bit = pin - (port << 5);
	reg = AWPIO_DAT(port);
	mask = 1 << (bit & 31);

	s = splhigh();

	AWSET4(sc, reg, mask);

	splx(s);
}

void
sxipio_clrpin(int pin)
{
	struct sxipio_softc *sc = sxipio_sc;
	uint32_t bit, mask, reg, port;
	int s;

	port = pin >> 5;
	bit = pin - (port << 5);
	reg = AWPIO_DAT(port);
	mask = 1 << (bit & 31);

	s = splhigh();

	AWCLR4(sc, reg, mask);

	splx(s);
}

int
sxipio_togglepin(int pin)
{
	struct sxipio_softc *sc = sxipio_sc;
	uint32_t bit, data, mask, reg, port;
	int s;

	port = pin >> 5;
	bit = pin - (port << 5);
	reg = AWPIO_DAT(port);
	mask = 1 << (bit & 31);

	s = splhigh();

	data = AWREAD4(sc, reg);
	AWWRITE4(sc, reg, data ^ mask);

	splx(s);

	return data & mask ? GPIO_PIN_HIGH : GPIO_PIN_LOW;
}
