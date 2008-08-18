/*	$OpenBSD: mbusvar.h,v 1.1 2008/08/18 23:19:25 miod Exp $	*/

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

struct mbus_attach_args {
	unsigned int	maa_mid;
	uint8_t		maa_class;
	uint8_t		maa_subclass;
	uint8_t		maa_interface;
	uint8_t		maa_revision;
	paddr_t		maa_addr;
	unsigned int	maa_vecbase;
};

extern unsigned int mbus_ioslot;

int	mbus_intr_establish(unsigned int, int, int (*)(void *), void *,
	    const char *);
uint32_t mbus_ddb_hook(int, uint32_t);
