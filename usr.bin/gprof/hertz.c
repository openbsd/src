/*	$OpenBSD: hertz.c,v 1.7 2015/01/16 06:40:08 deraadt Exp $	*/

/*
 * Copyright (c) 2005 Artur Grabowski <art@openbsd.org>
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


#include <sys/time.h>
#include <sys/sysctl.h>

#include "gprof.h"

/*
 * Return the tick frequency on the machine or 0 if we can't find out.
 */

int
hertz(void)
{
	struct clockinfo cinfo;
	int mib[2];
	size_t len;

	mib[0] = CTL_KERN;
	mib[1] = KERN_CLOCKRATE;
	len = sizeof(cinfo);
	if (sysctl(mib, 2, &cinfo, &len, NULL, 0) == -1)
		return (0);

	return (cinfo.hz);
}
