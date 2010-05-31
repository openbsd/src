/*	$OpenBSD: util.c,v 1.1 2010/05/31 17:36:31 martinh Exp $ */

/*
 * Copyright (c) 2009 Martin Hedenfalk <martin@bzero.se>
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

#include <sys/queue.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "ldapd.h"

int
bsnprintf(char *str, size_t size, const char *format, ...)
{
	int ret;
	va_list ap;

	va_start(ap, format);
	ret = vsnprintf(str, size, format, ap);
	va_end(ap);
	if (ret == -1 || ret >= (int)size)
		return 0;

	return 1;
}

/* Normalize a DN in preparation for searches.
 * Modifies its argument.
 * Currently only made lowercase, and spaces around comma is removed.
 * TODO: unescape backslash escapes, handle UTF-8.
 */
void
normalize_dn(char *dn)
{
	size_t		 n;
	char		*s, *p;

	for (s = p = dn; *s != '\0'; s++) {
		if (*s == ' ') {
			if (p == dn || p[-1] == ',')
				continue;
			n = strspn(s, " ");
			if (s[n] == '\0' || s[n] == ',')
				continue;
		}
		*p++ = tolower(*s);
	}
	*p = '\0';
}

/* Returns true (1) if key ends with suffix.
 */
int
has_suffix(struct btval *key, const char *suffix)
{
	size_t		slen;

	slen = strlen(suffix);

	if (key->size < slen)
		return 0;
	return (bcmp((char *)key->data + key->size - slen, suffix, slen) == 0);
}

/* Returns true (1) if key begins with prefix.
 */
int
has_prefix(struct btval *key, const char *prefix)
{
	size_t		 pfxlen;

	pfxlen = strlen(prefix);
	if (pfxlen > key->size)
		return 0;
	return (memcmp(key->data, prefix, pfxlen) == 0);
}

