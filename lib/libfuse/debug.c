/*	$OpenBSD: debug.c,v 1.1 2013/06/03 16:00:50 tedu Exp $	*/
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

#include <stdlib.h>
#include <unistd.h>

#include "debug.h"

#ifdef DEBUG
/*
 * debug level, -1 means uninitialized
 */
int ifuse_debug = -1;

void
ifuse_debug_init(void)
{
	char *dbg;

	if (ifuse_debug < 0) {
		dbg = issetugid() ? NULL : getenv("FUSE_DEBUG");
		if (!dbg || sscanf(dbg, "%u", &ifuse_debug) != 1)
			ifuse_debug = 0;
	}
}
#endif
