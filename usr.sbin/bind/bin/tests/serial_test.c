/*
 * Copyright (C) 1999-2001  Internet Software Consortium.
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

/* $ISC: serial_test.c,v 1.10 2001/01/09 21:41:37 bwelling Exp $ */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>

#include <isc/serial.h>

int
main() {
	isc_uint32_t a, b;
	char buf[1024];
	char *s, *e;

	while (fgets(buf, sizeof buf, stdin) != NULL) {
		buf[sizeof buf - 1] = '\0';
		s = buf;
		a = strtoul(s, &e, 0);
		if (s == e)
			continue;
		s = e;
		b = strtoul(s, &e, 0);
		if (s == e)
			continue;
		fprintf(stdout, "%u %u gt:%d lt:%d ge:%d le:%d eq:%d ne:%d\n",
			a, b,
			isc_serial_gt(a,b), isc_serial_lt(a,b),
			isc_serial_ge(a,b), isc_serial_le(a,b),
			isc_serial_eq(a,b), isc_serial_ne(a,b));
	}
	return (0);
}
