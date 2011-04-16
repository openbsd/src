/*	$OpenBSD: debug.c,v 1.1 2011/04/16 10:52:22 ratchov Exp $	*/
/*
 * Copyright (c) 2011 Alexandre Ratchov <alex@caoua.org>
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "debug.h"

#ifdef DEBUG
/*
 * debug level, -1 means uninitialized
 */
int sndio_debug = -1;

void
sndio_debug_init(void)
{
	char *dbg;

	if (sndio_debug < 0) {
		dbg = issetugid() ? NULL : getenv("SNDIO_DEBUG");
		if (!dbg || sscanf(dbg, "%u", &sndio_debug) != 1)
			sndio_debug = 0;
	}
}
#endif
