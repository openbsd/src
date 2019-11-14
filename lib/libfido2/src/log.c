/*
 * Copyright (c) 2018 Yubico AB. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include "fido.h"

#ifndef FIDO_NO_DIAGNOSTIC

#ifndef TLS
#define TLS
#endif

static TLS int logging;

void
log_init()
{
	logging = 1;
}

void
log_xxd(const void *buf, size_t count)
{
	const uint8_t	*ptr = buf;
	size_t		 i;

	if (!logging)
		return;

	fprintf(stderr, "  ");

	for (i = 0; i < count; i++) {
		fprintf(stderr, "%02x ", *ptr++);
		if ((i + 1) % 16 == 0 && i + 1 < count)
			fprintf(stderr, "\n  ");
	}

	fprintf(stderr, "\n");
	fflush(stderr);
}

void
log_debug(const char *fmt, ...)
{
	va_list	 ap;

	if (!logging)
		return;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	fprintf(stderr, "\n");
	fflush(stderr);
}

#endif /* !FIDO_NO_DIAGNOSTIC */
