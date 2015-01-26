/* $OpenBSD: exgpiovar.h,v 1.1 2015/01/26 02:48:24 bmercer Exp $ */
/*
 * Copyright (c) 2007,2009 Dale Rahn <drahn@openbsd.org>
 * Copyright (c) 2012-2013 Patrick Wildt <patrick@blueri.se>
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

#ifndef EXGPIOVAR_H
#define EXGPIOVAR_H

#define EXGPIO_DIR_IN	0
#define EXGPIO_DIR_OUT	1

unsigned int exgpio_get_function(unsigned int gpio, unsigned int fn);
void exgpio_set_function(unsigned int gpio, unsigned int fn);
unsigned int exgpio_get_bit(unsigned int gpio);
void exgpio_set_bit(unsigned int gpio);
void exgpio_clear_bit(unsigned int gpio);
void exgpio_set_dir(unsigned int gpio, unsigned int dir);

/* interrupts */
void exgpio_clear_intr(unsigned int gpio);
void exgpio_intr_mask(unsigned int gpio);
void exgpio_intr_unmask(unsigned int gpio);
void exgpio_intr_level(unsigned int gpio, unsigned int level);
void *exgpio_intr_establish(unsigned int gpio, int level, int spl,
    int (*func)(void *), void *arg, char *name);
void exgpio_intr_disestablish(void *cookie);

#endif /* EXGPIOVAR_H */
