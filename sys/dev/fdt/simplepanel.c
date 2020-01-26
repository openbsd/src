/*	$OpenBSD: simplepanel.c,v 1.1 2020/01/26 06:20:30 patrick Exp $	*/
/*
 * Copyright (c) 2020 Patrick Wildt <patrick@blueri.se>
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
#include <sys/malloc.h>

#include <machine/fdt.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_gpio.h>
#include <dev/ofw/ofw_misc.h>
#include <dev/ofw/ofw_pinctrl.h>
#include <dev/ofw/ofw_regulator.h>

int simplepanel_match(struct device *, void *, void *);
void simplepanel_attach(struct device *, struct device *, void *);

struct cfattach	simplepanel_ca = {
	sizeof (struct device), simplepanel_match, simplepanel_attach
};

struct cfdriver simplepanel_cd = {
	NULL, "simplepanel", DV_DULL
};

int
simplepanel_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return OF_is_compatible(faa->fa_node, "simple-panel");
}

void
simplepanel_attach(struct device *parent, struct device *self, void *aux)
{
	struct fdt_attach_args *faa = aux;
	uint32_t power_supply;
	uint32_t *gpios;
	int len;

	printf("\n");

	pinctrl_byname(faa->fa_node, "default");

	power_supply = OF_getpropint(faa->fa_node, "power-supply", 0);
	if (power_supply)
		regulator_enable(power_supply);

	len = OF_getproplen(faa->fa_node, "enable-gpios");
	if (len > 0) {
		gpios = malloc(len, M_TEMP, M_WAITOK);
		OF_getpropintarray(faa->fa_node, "enable-gpios", gpios, len);
		gpio_controller_config_pin(&gpios[0], GPIO_CONFIG_OUTPUT);
		gpio_controller_set_pin(&gpios[0], 1);
		free(gpios, M_TEMP, len);
	}
}
