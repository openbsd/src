/*
 * Portions Copyright (C) 2008 Theo de Raadt
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
 * INTERNET SOFTWARE CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* $OpenBSD: shuffle.c,v 1.2 2008/03/02 22:39:12 djm Exp $ */

#include <config.h>

#include <stdlib.h>

#include <isc/shuffle.h>
#include <isc/random.h>
#include <isc/time.h>
#include <isc/util.h>

#define VALID_SHUFFLE(x)	(x != NULL)

void
isc_shuffle_init(isc_shuffle_t *shuffle)
{
	isc_uint16_t r;
	int i, i2;

	REQUIRE(VALID_SHUFFLE(shuffle));

	shuffle->isindex = 0;
	for (i = 0; i < 65536; ++i)
		shuffle->id_shuffle[i] = i;

	/* Initialize using a Durstenfeld shuffle */
	for (i = 65536; --i; ) {
		i2 = isc_random_uniform(i + 1);
		r = shuffle->id_shuffle[i];
		shuffle->id_shuffle[i] = shuffle->id_shuffle[i2];
		shuffle->id_shuffle[i2] = r;
	}
}

isc_uint16_t
isc_shuffle_generate16(isc_shuffle_t *shuffle)
{
	isc_uint32_t si;
	isc_uint16_t r;
	int i, i2;

	REQUIRE(VALID_SHUFFLE(shuffle));

	do {
		isc_random_get(&si);
		i = shuffle->isindex & 0xFFFF;
		i2 = (shuffle->isindex - (si & 0x7FFF)) & 0xFFFF;
		r = shuffle->id_shuffle[i];
		shuffle->id_shuffle[i] = shuffle->id_shuffle[i2];
		shuffle->id_shuffle[i2] = r;
		shuffle->isindex++;
	} while (r == 0);

	return (r);
}
