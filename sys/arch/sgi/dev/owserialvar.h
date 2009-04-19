/*	$OpenBSD: owserialvar.h,v 1.2 2009/04/19 18:33:53 miod Exp $	*/

/*
 * Copyright (c) 2008 Miodrag Vallat.
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

/*
 * DS2505 1-Wire Add-only memory driver, for SGI machines.
 *
 * SGI seems to use DS2505 (or compatibles) to store serial numbers.
 */

#define	OWSERIAL_NAME_LEN	14
#define	OWSERIAL_PRODUCT_LEN	12
#define	OWSERIAL_SERIAL_LEN	20

struct owserial_softc {
	struct device	 sc_dev;

	void		*sc_onewire;
	uint64_t	 sc_rom;

	int		 sc_npages;		/* number of pages */
	uint8_t		 sc_redir[256];		/* redirection table */

	char		 sc_name[1 + OWSERIAL_NAME_LEN];
	char		 sc_product[1 + OWSERIAL_PRODUCT_LEN];
	char		 sc_serial[1 + OWSERIAL_SERIAL_LEN];
};
