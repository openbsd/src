/*	$OpenBSD: pxa2x0_i2c.h,v 1.2 2005/05/26 03:52:07 pascoe Exp $	*/

/*
 * Copyright (c) 2005 Christopher Pascoe <pascoe@openbsd.org>
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

#ifndef _PXA2X0_I2C_H_
#define _PXA2X0_I2C_H_

#include <machine/bus.h>

struct pxa2x0_i2c_softc {
	struct device sc_dev;
	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh;
	bus_size_t sc_size;
};

int	pxa2x0_i2c_attach_sub(struct pxa2x0_i2c_softc *);
int	pxa2x0_i2c_detach_sub(struct pxa2x0_i2c_softc *);
void	pxa2x0_i2c_init(struct pxa2x0_i2c_softc *);
void	pxa2x0_i2c_open(struct pxa2x0_i2c_softc *);
void	pxa2x0_i2c_close(struct pxa2x0_i2c_softc *);
int	pxa2x0_i2c_read(struct pxa2x0_i2c_softc *sc, u_char, u_char *);
int	pxa2x0_i2c_write(struct pxa2x0_i2c_softc *, u_char, u_char);
int	pxa2x0_i2c_write_2(struct pxa2x0_i2c_softc *, u_char, u_short);

#endif
