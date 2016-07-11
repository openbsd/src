/*	$OpenBSD: ofw_gpio.c,v 1.1 2016/07/11 14:49:41 kettenis Exp $	*/
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

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_gpio.h>

LIST_HEAD(, gpio_controller) gpio_controllers =
	LIST_HEAD_INITIALIZER(gpio_controllers);

void
gpio_controller_register(struct gpio_controller *gc)
{
	gc->gc_cells = OF_getpropint(gc->gc_node, "#gpio-cells", 2);
	gc->gc_phandle = OF_getpropint(gc->gc_node, "phandle", 0);
	if (gc->gc_phandle == 0)
		return;

	LIST_INSERT_HEAD(&gpio_controllers, gc, gc_list);
}

void
gpio_controller_config_pin(uint32_t *cells, int config)
{
	struct gpio_controller *gc;
	uint32_t phandle = cells[0];

	LIST_FOREACH(gc, &gpio_controllers, gc_list) {
		if (gc->gc_phandle == phandle)
			break;
	}

	if (gc && gc->gc_config_pin)
		gc->gc_config_pin(gc->gc_cookie, &cells[1], config);
}

int
gpio_controller_get_pin(uint32_t *cells)
{
	struct gpio_controller *gc;
	uint32_t phandle = cells[0];
	int val = 0;

	LIST_FOREACH(gc, &gpio_controllers, gc_list) {
		if (gc->gc_phandle == phandle)
			break;
	}

	if (gc && gc->gc_get_pin)
		val = gc->gc_get_pin(gc->gc_cookie, &cells[1]);

	return val;
}

void
gpio_controller_set_pin(uint32_t *cells, int val)
{
	struct gpio_controller *gc;
	uint32_t phandle = cells[0];

	LIST_FOREACH(gc, &gpio_controllers, gc_list) {
		if (gc->gc_phandle == phandle)
			break;
	}

	if (gc && gc->gc_set_pin)
		gc->gc_set_pin(gc->gc_cookie, &cells[1], val);
}
