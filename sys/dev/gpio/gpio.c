/*	$OpenBSD: gpio.c,v 1.4 2004/11/23 21:18:37 grange Exp $	*/
/*
 * Copyright (c) 2004 Alexander Yurchenko <grange@openbsd.org>
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
 * General Purpose Input/Output framework.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/gpio.h>
#include <sys/vnode.h>

#include <dev/gpio/gpiovar.h>

struct gpio_softc {
	struct device sc_dev;

	gpio_chipset_tag_t sc_gc;	/* our GPIO controller */
	gpio_pin_t *sc_pins;		/* pins array */
	int sc_npins;			/* total number of pins */

	int sc_opened;
};

int	gpio_match(struct device *, void *, void *);
void	gpio_attach(struct device *, struct device *, void *);
int	gpio_detach(struct device *, int);
int	gpio_search(struct device *, void *, void *);
int	gpio_print(void *, const char *);

struct cfattach gpio_ca = {
	sizeof (struct gpio_softc),
	gpio_match,
	gpio_attach,
	gpio_detach
};

struct cfdriver gpio_cd = {
	NULL, "gpio", DV_DULL
};

int
gpio_match(struct device *parent, void *match, void *aux)
{
	struct cfdata *cf = match;
	struct gpiobus_attach_args *gba = aux;

	if (strcmp(gba->gba_name, cf->cf_driver->cd_name) != 0)
		return (0);

	return (1);
}

void
gpio_attach(struct device *parent, struct device *self, void *aux)
{
	struct gpio_softc *sc = (struct gpio_softc *)self;
	struct gpiobus_attach_args *gba = aux;

	sc->sc_gc = gba->gba_gc;
	sc->sc_pins = gba->gba_pins;
	sc->sc_npins = gba->gba_npins;

	printf(": %d pins\n", sc->sc_npins);

	/*
	 * Attach all devices that can be connected to the GPIO pins
	 * described in the kernel configuration file.
	 */
	config_search(gpio_search, self, NULL);
}

int
gpio_detach(struct device *self, int flags)
{
	int maj, mn;

	/* Locate the major number */
	for (maj = 0; maj < nchrdev; maj++)
		if (cdevsw[maj].d_open == gpioopen)
			break;

	/* Nuke the vnodes for any open instances (calls close) */
	mn = self->dv_unit;
	vdevgone(maj, mn, mn, VCHR);

	return (0);
}

int
gpio_search(struct device *parent, void *arg, void *aux)
{
	struct cfdata *cf = arg;
	struct gpio_attach_args ga;

	ga.ga_pin = cf->cf_loc[0];
	ga.ga_mask = cf->cf_loc[1];

	if (cf->cf_attach->ca_match(parent, cf, &ga) > 0)
		config_attach(parent, cf, &ga, gpio_print);

	return (0);
}

int
gpio_print(void *aux, const char *pnp)
{
	struct gpio_attach_args *ga = aux;
	int i;

	printf(" pins");
	for (i = 0; i < 32; i++)
		if (ga->ga_mask & (1 << i))
			printf(" %d", ga->ga_pin + i);

	return (UNCONF);
}

int
gpiobus_print(void *aux, const char *pnp)
{
	struct gpiobus_attach_args *gba = aux;

	if (pnp != NULL)
		printf("%s at %s", gba->gba_name, pnp);

	return (UNCONF);
}

int
gpioopen(dev_t dev, int flag, int mode, struct proc *p)
{
	struct gpio_softc *sc;

	sc = (struct gpio_softc *)device_lookup(&gpio_cd, minor(dev));
	if (sc == NULL)
		return (ENXIO);

	if (sc->sc_opened)
		return (EBUSY);
	sc->sc_opened = 1;

	return (0);
}

int
gpioclose(dev_t dev, int flag, int mode, struct proc *p)
{
	struct gpio_softc *sc;

	sc = (struct gpio_softc *)device_lookup(&gpio_cd, minor(dev));
	sc->sc_opened = 0;

	return (0);
}

int
gpioioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	struct gpio_softc *sc;
	gpio_chipset_tag_t gc;
	struct gpio_info *info;
	struct gpio_pin_op *op;
	struct gpio_pin_ctl *ctl;
	int pin, value, flags;

	sc = (struct gpio_softc *)device_lookup(&gpio_cd, minor(dev));
	gc = sc->sc_gc;

	switch (cmd) {
	case GPIOINFO:
		info = (struct gpio_info *)data;

		info->gpio_npins = sc->sc_npins;
		break;
	case GPIOPINREAD:
		op = (struct gpio_pin_op *)data;

		pin = op->gp_pin;
		if (pin < 0 || pin >= sc->sc_npins)
			return (EINVAL);

		/* return read value */
		op->gp_value = gpiobus_pin_read(gc, pin);
		break;
	case GPIOPINWRITE:
		op = (struct gpio_pin_op *)data;

		pin = op->gp_pin;
		if (pin < 0 || pin >= sc->sc_npins)
			return (EINVAL);

		value = op->gp_value;
		if (value != GPIO_PIN_LOW && value != GPIO_PIN_HIGH)
			return (EINVAL);

		gpiobus_pin_write(gc, pin, value);
		/* return old value */
		op->gp_value = sc->sc_pins[pin].pin_state;
		/* update current value */
		sc->sc_pins[pin].pin_state = value;
		break;
	case GPIOPINTOGGLE:
		op = (struct gpio_pin_op *)data;

		pin = op->gp_pin;
		if (pin < 0 || pin >= sc->sc_npins)
			return (EINVAL);

		value = (sc->sc_pins[pin].pin_state == GPIO_PIN_LOW ?
		    GPIO_PIN_HIGH : GPIO_PIN_LOW);
		gpiobus_pin_write(gc, pin, value);
		/* return old value */
		op->gp_value = sc->sc_pins[pin].pin_state;
		/* update current value */
		sc->sc_pins[pin].pin_state = value;
		break;
	case GPIOPINCTL:
		ctl = (struct gpio_pin_ctl *)data;

		pin = ctl->gp_pin;
		if (pin < 0 || pin >= sc->sc_npins)
			return (EINVAL);

		flags = ctl->gp_flags;
		/* check that the controller supports all requested flags */
		if ((flags & sc->sc_pins[pin].pin_caps) != flags)
			return (ENODEV);

		ctl->gp_caps = sc->sc_pins[pin].pin_caps;
		/* return old value */
		ctl->gp_flags = sc->sc_pins[pin].pin_flags;
		if (flags > 0) {
			gpiobus_pin_ctl(gc, pin, flags);
			/* update current value */
			sc->sc_pins[pin].pin_flags = flags;
		}
		break;
	default:
		return (ENOTTY);
	}

	return (0);
}
