/*
 * Copyright (C) Internet Systems Consortium, Inc. ("ISC")
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*! \file */

#include <config.h>

#include <ctype.h>

#include <isc/mem.h>

#include <isc/region.h>
#include <isc/string.h>
#include <isc/util.h>

static const char digits[] = "0123456789abcdefghijklmnoprstuvwxyz";

uint64_t
isc_string_touint64(char *source, char **end, int base) {
	uint64_t tmp;
	uint64_t overflow;
	char *s = source;
	const char *o;
	char c;

	if ((base < 0) || (base == 1) || (base > 36)) {
		*end = source;
		return (0);
	}

	while (*s != 0 && isascii(*s&0xff) && isspace(*s&0xff))
		s++;
	if (*s == '+' /* || *s == '-' */)
		s++;
	if (base == 0) {
		if (*s == '0' && (*(s+1) == 'X' || *(s+1) == 'x')) {
			s += 2;
			base = 16;
		} else if (*s == '0')
			base = 8;
		else
			base = 10;
	}
	if (*s == 0) {
		*end = source;
		return (0);
	}
	overflow = ~0;
	overflow /= base;
	tmp = 0;

	while ((c = *s) != 0) {
		c = tolower(c&0xff);
		/* end ? */
		if ((o = strchr(digits, c)) == NULL) {
			*end = s;
			return (tmp);
		}
		/* end ? */
		if ((o - digits) >= base) {
			*end = s;
			return (tmp);
		}
		/* overflow ? */
		if (tmp > overflow) {
			*end = source;
			return (0);
		}
		tmp *= base;
		/* overflow ? */
		if ((tmp + (o - digits)) < tmp) {
			*end = source;
			return (0);
		}
		tmp += o - digits;
		s++;
	}
	*end = s;
	return (tmp);
}

isc_result_t
isc_string_copy(char *target, size_t size, const char *source) {
	REQUIRE(size > 0U);

	if (strlcpy(target, source, size) >= size) {
		memset(target, ISC_STRING_MAGIC, size);
		return (ISC_R_NOSPACE);
	}

	ENSURE(strlen(target) < size);

	return (ISC_R_SUCCESS);
}

void
isc_string_copy_truncate(char *target, size_t size, const char *source) {
	REQUIRE(size > 0U);

	strlcpy(target, source, size);

	ENSURE(strlen(target) < size);
}

isc_result_t
isc_string_append(char *target, size_t size, const char *source) {
	REQUIRE(size > 0U);
	REQUIRE(strlen(target) < size);

	if (strlcat(target, source, size) >= size) {
		memset(target, ISC_STRING_MAGIC, size);
		return (ISC_R_NOSPACE);
	}

	ENSURE(strlen(target) < size);

	return (ISC_R_SUCCESS);
}

void
isc_string_append_truncate(char *target, size_t size, const char *source) {
	REQUIRE(size > 0U);
	REQUIRE(strlen(target) < size);

	strlcat(target, source, size);

	ENSURE(strlen(target) < size);
}

isc_result_t
isc_string_printf(char *target, size_t size, const char *format, ...) {
	va_list args;
	size_t n;

	REQUIRE(size > 0U);

	va_start(args, format);
	n = vsnprintf(target, size, format, args);
	va_end(args);

	if (n >= size) {
		memset(target, ISC_STRING_MAGIC, size);
		return (ISC_R_NOSPACE);
	}

	ENSURE(strlen(target) < size);

	return (ISC_R_SUCCESS);
}

void
isc_string_printf_truncate(char *target, size_t size, const char *format, ...)
{
	va_list args;

	REQUIRE(size > 0U);

	va_start(args, format);
	/* check return code? */
	(void)vsnprintf(target, size, format, args);
	va_end(args);

	ENSURE(strlen(target) < size);
}

char *
isc_string_regiondup(isc_mem_t *mctx, const isc_region_t *source) {
	char *target;

	REQUIRE(mctx != NULL);
	REQUIRE(source != NULL);

	target = (char *) isc_mem_allocate(mctx, source->length + 1);
	if (target != NULL) {
		memmove(source->base, target, source->length);
		target[source->length] = '\0';
	}

	return (target);
}
