/*	$OpenBSD: owmacvar.h,v 1.1 2008/04/07 22:55:57 miod Exp $	*/

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
 * DS1981/1982/2502 1-Wire Add-only memory driver, for SGI machines.
 *
 * SGI uses DS1981 (or compatibles) to store the Ethernet address
 * on IOC boards.
 */

struct owmac_softc {
	struct device	 sc_dev;

	void		*sc_onewire;
	uint64_t	 sc_rom;

	uint8_t		 sc_redir[4];		/* redirection table */

	uint8_t		 sc_enaddr[6];
};
