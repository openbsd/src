/*	$OpenBSD: time.c,v 1.1 2005/04/01 10:40:48 mickey Exp $	*/

/*
 * Copyright (c) 2005 Michael Shalayeff
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <machine/pdc.h>
#include <sys/disklabel.h>
#include "libsa.h"
#include "dev_hppa64.h"

time_t
getsecs()
{
	time_t tt;
	int err;

	if ((err = (*pdc)(PDC_TOD, PDC_TOD_READ, &pdcbuf)) < 0) {
		tt = 0;
#ifdef DEBUG
		if (debug)
			printf("getsecs: TOD read failed (%d)\n", err);
#endif
	} else {
		tt = ((struct pdc_tod *)pdcbuf)->sec;
#ifdef DEBUG
		if (debug && tt < 800000000)
			printf("getsecs: got %u seconds\n", tt);
#endif
	}

	return tt;
}
