/*	$OpenBSD: ofw_regulator.c,v 1.1 2016/08/13 10:52:21 kettenis Exp $	*/
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

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/malloc.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_gpio.h>
#include <dev/ofw/ofw_pinctrl.h>
#include <dev/ofw/ofw_regulator.h>

int
regulator_set(uint32_t phandle, int enable)
{
	uint32_t *gpio;
	uint32_t startup_delay;
	int active;
	int node;
	int len;

	node = OF_getnodebyphandle(phandle);
	if (node == 0)
		return -1;

	if (!OF_is_compatible(node, "regulator-fixed"))
		return -1;

	pinctrl_byname(node, "default");

	if (OF_getproplen(node, "enable-active-high") == 0)
		active = 1;
	else
		active = 0;

	/* The "gpio" property is optional. */
	len = OF_getproplen(node, "gpio");
	if (len < 0)
		return 0;

	gpio = malloc(len, M_TEMP, M_WAITOK);
	OF_getpropintarray(node, "gpio", gpio, len);
	gpio_controller_config_pin(gpio, GPIO_CONFIG_OUTPUT);
	if (enable)
		gpio_controller_set_pin(gpio, active);
	else
		gpio_controller_set_pin(gpio, !active);
	free(gpio, M_TEMP, len);

	startup_delay = OF_getpropint(node, "startup-delay-us", 0);
	if (enable && startup_delay > 0)
		delay(startup_delay);

	return 0;
}

int
regulator_enable(uint32_t phandle)
{
	return regulator_set(phandle, 1);
}

int
regulator_disable(uint32_t phandle)
{
	return regulator_set(phandle, 0);
}
