/*	$OpenBSD: gpio.h,v 1.1 2004/06/03 18:08:00 grange Exp $	*/
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

#ifndef _SYS_GPIO_H_
#define _SYS_GPIO_H_

/* GPIO pin states */
#define GPIO_PIN_LOW		0x00	/* low level (logical 0) */
#define GPIO_PIN_HIGH		0x01	/* high level (logical 1) */

/* GPIO pin configuration flags */
#define GPIO_PIN_INPUT		0x0001	/* input direction */
#define GPIO_PIN_OUTPUT		0x0002	/* output direction */
#define GPIO_PIN_INOUT		0x0004	/* bi-directional */
#define GPIO_PIN_OPENDRAIN	0x0008	/* open-drain output */
#define GPIO_PIN_PUSHPULL	0x0010	/* push-pull output */
#define GPIO_PIN_TRISTATE	0x0020	/* output disabled */
#define GPIO_PIN_PULLUP		0x0040	/* internal pull-up enabled */

/* GPIO controller description */
struct gpio_info {
	int gpio_npins;		/* total number of pins available */
};

/* GPIO pin operation (read/write/toggle) */
struct gpio_pin_op {
	int gp_pin;		/* pin number */
	int gp_value;		/* value */
};

/* GPIO pin control */
struct gpio_pin_ctl {
	int gp_pin;		/* pin number */
	int gp_caps;		/* pin capabilities (read-only) */
	int gp_flags;		/* pin configuration flags */
};

#define GPIOINFO		_IOR('G', 0, struct gpio_info)
#define GPIOPINREAD		_IOWR('G', 1, struct gpio_pin_op)
#define GPIOPINWRITE		_IOWR('G', 2, struct gpio_pin_op)
#define GPIOPINTOGGLE		_IOWR('G', 3, struct gpio_pin_op)
#define GPIOPINCTL		_IOWR('G', 4, struct gpio_pin_ctl)

#endif	/* !_SYS_GPIO_H_ */
