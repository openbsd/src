/*	$OpenBSD: gpioleds.c,v 1.6 2026/07/17 11:02:30 jsg Exp $	*/
/*
 * Copyright (c) 2021 Klemens Nanni <kn@openbsd.org>
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
#include <sys/malloc.h>

#include <machine/bus.h>
#include <machine/fdt.h>

#include <dev/gpio/gpiovar.h>
#include <dev/ofw/ofw_gpio.h>
#include <dev/ofw/ofw_pinctrl.h>
#include <dev/ofw/openfirm.h>
#include <dev/ofw/fdt.h>

struct gpioleds_softc {
	struct device	 sc_dev;

	struct blink_led sc_blink;
	uint32_t	 *sc_blink_gpios;
};

int	gpioleds_match(struct device *, void *, void *);
void	gpioleds_attach(struct device *, struct device *, void *);

const struct cfattach gpioleds_ca = {
	sizeof (struct gpioleds_softc), gpioleds_match, gpioleds_attach
};

struct cfdriver gpioleds_cd = {
	NULL, "gpioleds", DV_DULL
};

void	gpioleds_blink(void *, int);

int
gpioleds_match(struct device *parent, void *match, void *aux)
{
	const struct fdt_attach_args	*faa = aux;

	return OF_is_compatible(faa->fa_node, "gpio-leds");
}

void
gpioleds_attach(struct device *parent, struct device *self, void *aux)
{
	struct gpioleds_softc	*sc = (struct gpioleds_softc *)self;
	struct fdt_attach_args	*faa = aux;
	uint32_t		*led_pin;
	char			*function, *default_state, *trigger;
	char			*function_prop = "function";
	int			 function_len, default_state_len, gpios_len;
	int			 trigger_len;
	int			 node, leds = 0;

	pinctrl_byname(faa->fa_node, "default");

	for (node = OF_child(faa->fa_node); node; node = OF_peer(node)) {
		function_len = OF_getproplen(node, function_prop);
		if (function_len <= 0) {
			function_prop = "label";
			function_len = OF_getproplen(node, function_prop);
			if (function_len <= 0)
				continue;
		}

		default_state_len = OF_getproplen(node, "default-state");
		if (default_state_len <= 0)
			continue;
		gpios_len = OF_getproplen(node, "gpios");
		if (gpios_len <= 0)
			continue;

		function = malloc(function_len, M_TEMP, M_WAITOK);
		OF_getprop(node, function_prop, function, function_len);
		default_state = malloc(default_state_len, M_TEMP, M_WAITOK);
		OF_getprop(node, "default-state", default_state, default_state_len);
		led_pin = malloc(gpios_len, M_DEVBUF, M_WAITOK);
		OF_getpropintarray(node, "gpios", led_pin, gpios_len);
		gpio_controller_config_pin(led_pin, GPIO_CONFIG_OUTPUT);
		if (strcmp(default_state, "on") == 0)
			gpio_controller_set_pin(led_pin, 1);
		else if (strcmp(default_state, "off") == 0)
			gpio_controller_set_pin(led_pin, 0);

		printf("%s \"%s\"", leds++ ? "," : ":", function);

		trigger_len = OF_getproplen(node, "linux,default-trigger");
		if (trigger_len > 0) {
			trigger = malloc(function_len, M_TEMP, M_WAITOK);
			OF_getprop(node, "linux,default-trigger", trigger,
			    trigger_len);
			if (strcmp(trigger, "heartbeat") == 0 &&
			    sc->sc_blink.bl_func == NULL) {
				printf(" (trigger)");
				sc->sc_blink.bl_func = gpioleds_blink;
				sc->sc_blink.bl_arg = led_pin;
				blink_led_register(&sc->sc_blink);
				led_pin = NULL;
			}
			free(trigger, M_TEMP, trigger_len);
		}

		free(function, M_TEMP, function_len);
		free(default_state, M_TEMP, default_state_len);
		free(led_pin, M_DEVBUF, gpios_len);
	}

	if (leds == 0)
		printf(": no LEDs");
	printf("\n");
}

void
gpioleds_blink(void *arg, int on)
{
	uint32_t *led_pin = arg;

	gpio_controller_set_pin(led_pin, on);
}
