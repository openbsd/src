/*	$OpenBSD: ofw_gpio.h,v 1.4 2020/04/27 12:15:30 kettenis Exp $	*/
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

#ifndef _DEV_OFW_GPIO_H_
#define _DEV_OFW_GPIO_H_

#define GPIO_ACTIVE_HIGH	0
#define GPIO_ACTIVE_LOW		1

struct gpio_controller {
	int	gc_node;
	void	*gc_cookie;
	void	(*gc_config_pin)(void *, uint32_t *, int);
	int	(*gc_get_pin)(void *, uint32_t *);
	void	(*gc_set_pin)(void *, uint32_t *, int);

	LIST_ENTRY(gpio_controller) gc_list;
	uint32_t gc_phandle;
	uint32_t gc_cells;
};

void	gpio_controller_register(struct gpio_controller *);

#define GPIO_CONFIG_INPUT	0x0000
#define GPIO_CONFIG_OUTPUT	0x0001
#define GPIO_CONFIG_PULL_UP	0x0010
#define GPIO_CONFIG_PULL_DOWN	0x0020
#define GPIO_CONFIG_MD0		0x1000
#define GPIO_CONFIG_MD1		0x2000
#define GPIO_CONFIG_MD2		0x4000
#define GPIO_CONFIG_MD3		0x8000
void	gpio_controller_config_pin(uint32_t *, int);

int	gpio_controller_get_pin(uint32_t *);
void	gpio_controller_set_pin(uint32_t *, int);
uint32_t *gpio_controller_next_pin(uint32_t *);

#endif /* _DEV_OFW_GPIO_H_ */
