/*	$OpenBSD: iofvar.h,v 1.4 2010/04/06 19:12:34 miod Exp $	*/

/*
 * Copyright (c) 2009, 2010 Miodrag Vallat.
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

struct iof_attach_args {
	const char			*iaa_name;

	bus_space_tag_t			 iaa_memt;
	bus_space_handle_t		 iaa_memh;
	bus_dma_tag_t			 iaa_dmat;

	bus_addr_t			 iaa_base;
	uint				 iaa_dev;
	uint				 iaa_clock;

	struct sgi_device_location	 iaa_location;
};

void   *iof_intr_establish(void *, uint, int, int (*)(void *), void *, char *);
