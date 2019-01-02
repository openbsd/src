/*	$OpenBSD: ofw_regulator.c,v 1.10 2019/01/02 18:50:15 patrick Exp $	*/
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

LIST_HEAD(, regulator_device) regulator_devices =
	LIST_HEAD_INITIALIZER(regulator_devices);

uint32_t regulator_gpio_get_voltage(int);
int regulator_gpio_set_voltage(int, uint32_t);

void
regulator_register(struct regulator_device *rd)
{
	rd->rd_min = OF_getpropint(rd->rd_node, "regulator-min-microvolt", 0);
	rd->rd_max = OF_getpropint(rd->rd_node, "regulator-max-microvolt", ~0);
	KASSERT(rd->rd_min <= rd->rd_max);

	rd->rd_ramp_delay =
	    OF_getpropint(rd->rd_node, "regulator-ramp-delay", 0);

	if (rd->rd_get_voltage && rd->rd_set_voltage) {
		uint32_t voltage = rd->rd_get_voltage(rd->rd_cookie);
		if (voltage < rd->rd_min)
			rd->rd_set_voltage(rd->rd_cookie, rd->rd_min);
		if (voltage > rd->rd_max)
			rd->rd_set_voltage(rd->rd_cookie, rd->rd_max);
	}

	rd->rd_phandle = OF_getpropint(rd->rd_node, "phandle", 0);
	if (rd->rd_phandle == 0)
		return;

	LIST_INSERT_HEAD(&regulator_devices, rd, rd_list);
}

int
regulator_fixed_set(int node, int enable)
{
	uint32_t *gpio;
	uint32_t startup_delay;
	int active;
	int len;

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
regulator_set(uint32_t phandle, int enable)
{
	struct regulator_device *rd;
	int node;

	node = OF_getnodebyphandle(phandle);
	if (node == 0)
		return ENODEV;

	/* Don't mess around with regulators that are always on. */
	if (OF_getproplen(node, "regulator-always-on") == 0)
		return 0;

	LIST_FOREACH(rd, &regulator_devices, rd_list) {
		if (rd->rd_phandle == phandle)
			break;
	}

	if (rd && rd->rd_enable)
		return rd->rd_enable(rd->rd_cookie, enable);

	if (OF_is_compatible(node, "regulator-fixed"))
		return regulator_fixed_set(node, enable);

	return ENODEV;
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

uint32_t
regulator_get_voltage(uint32_t phandle)
{
	struct regulator_device *rd;
	int node;

	LIST_FOREACH(rd, &regulator_devices, rd_list) {
		if (rd->rd_phandle == phandle)
			break;
	}

	if (rd && rd->rd_get_voltage)
		return rd->rd_get_voltage(rd->rd_cookie);

	node = OF_getnodebyphandle(phandle);
	if (node == 0)
		return 0;

	if (OF_is_compatible(node, "regulator-fixed"))
		return OF_getpropint(node, "regulator-min-microvolt", 0);

	if (OF_is_compatible(node, "regulator-gpio"))
		return regulator_gpio_get_voltage(node);

	return 0;
}

int
regulator_set_voltage(uint32_t phandle, uint32_t voltage)
{
	struct regulator_device *rd;
	uint32_t old, delta;
	int error, node;

	LIST_FOREACH(rd, &regulator_devices, rd_list) {
		if (rd->rd_phandle == phandle)
			break;
	}

	/* Check limits. */
	if (rd && (voltage < rd->rd_min || voltage > rd->rd_max))
		return EINVAL;

	if (rd && rd->rd_set_voltage) {
		old = rd->rd_get_voltage(rd->rd_cookie);
		error = rd->rd_set_voltage(rd->rd_cookie, voltage);
		if (voltage > old && rd->rd_ramp_delay > 0) {
			delta = voltage - old;
			delay(howmany(delta, rd->rd_ramp_delay));
		}
		return error;
	}

	node = OF_getnodebyphandle(phandle);
	if (node == 0)
		return ENODEV;

	if (OF_is_compatible(node, "regulator-fixed") &&
	    OF_getpropint(node, "regulator-min-microvolt", 0) == voltage)
		return 0;

	if (OF_is_compatible(node, "regulator-gpio"))
		return regulator_gpio_set_voltage(node, voltage);

	return ENODEV;
}

uint32_t
regulator_gpio_get_voltage(int node)
{
	uint32_t *gpio, *gpios, *states;
	uint32_t idx, voltage;
	size_t glen, slen;
	int i;

	pinctrl_byname(node, "default");

	if ((glen = OF_getproplen(node, "gpios")) <= 0)
		return EINVAL;
	if ((slen = OF_getproplen(node, "states")) <= 0)
		return EINVAL;

	if (slen % (2 * sizeof(uint32_t)) != 0)
		return EINVAL;

	gpios = malloc(glen, M_TEMP, M_WAITOK);
	states = malloc(slen, M_TEMP, M_WAITOK);

	OF_getpropintarray(node, "gpios", gpios, glen);
	OF_getpropintarray(node, "states", states, slen);

	i = 0;
	idx = 0;
	gpio = gpios;
	while (gpio && gpio < gpios + (glen / sizeof(uint32_t))) {
		idx |= (1 << i);
		gpio = gpio_controller_next_pin(gpio);
		i++;
	}

	voltage = 0;
	for (i = 0; i < slen / (2 * sizeof(uint32_t)); i++) {
		if (states[2 * i + 1] == idx) {
			voltage = states[2 * i];
			break;
		}
	}
	if (i >= slen / (2 * sizeof(uint32_t)))
		return 0;

	free(gpios, M_TEMP, glen);
	free(states, M_TEMP, slen);

	return voltage;
}

int
regulator_gpio_set_voltage(int node, uint32_t voltage)
{
	uint32_t *gpio, *gpios, *states;
	size_t glen, slen;
	uint32_t min, max;
	uint32_t idx;
	int i;

	pinctrl_byname(node, "default");

	/* Check limits. */
	min = OF_getpropint(node, "regulator-min-microvolt", 0);
	max = OF_getpropint(node, "regulator-max-microvolt", 0);
	if (voltage < min || voltage > max)
		return EINVAL;

	if ((glen = OF_getproplen(node, "gpios")) <= 0)
		return EINVAL;
	if ((slen = OF_getproplen(node, "states")) <= 0)
		return EINVAL;

	if (slen % (2 * sizeof(uint32_t)) != 0)
		return EINVAL;

	gpios = malloc(glen, M_TEMP, M_WAITOK);
	states = malloc(slen, M_TEMP, M_WAITOK);

	OF_getpropintarray(node, "gpios", gpios, glen);
	OF_getpropintarray(node, "states", states, slen);

	idx = 0;
	for (i = 0; i < slen / (2 * sizeof(uint32_t)); i++) {
		if (states[2 * i] < min || states[2 * i] > max)
			continue;
		if (states[2 * i] == voltage) {
			idx = states[2 * i + 1];
			break;
		}
	}
	if (i >= slen / (2 * sizeof(uint32_t)))
		return EINVAL;

	i = 0;
	gpio = gpios;
	while (gpio && gpio < gpios + (glen / sizeof(uint32_t))) {
		gpio_controller_set_pin(gpio, !!(idx & (1 << i)));
		gpio = gpio_controller_next_pin(gpio);
		i++;
	}

	free(gpios, M_TEMP, glen);
	free(states, M_TEMP, slen);

	return 0;
}
