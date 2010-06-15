/*	$OpenBSD: util.c,v 1.2 2010/06/15 19:30:26 martinh Exp $ */

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

#include <assert.h>
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

int
ber2db(struct ber_element *root, struct btval *val, int compression_level)
{
	int			 rc;
	ssize_t			 len;
	uLongf			 destlen;
	Bytef			*dest;
	void			*buf;
	struct ber		 ber;

	bzero(val, sizeof(*val));

	bzero(&ber, sizeof(ber));
	ber.fd = -1;
	ber_write_elements(&ber, root);

	if ((len = ber_get_writebuf(&ber, &buf)) == -1)
		return -1;

	if (compression_level > 0) {
		val->size = compressBound(len);
		val->data = malloc(val->size + sizeof(uint32_t));
		if (val->data == NULL) {
			log_warn("malloc(%u)", val->size + sizeof(uint32_t));
			ber_free(&ber);
			return -1;
		}
		dest = (char *)val->data + sizeof(uint32_t);
		destlen = val->size - sizeof(uint32_t);
		if ((rc = compress2(dest, &destlen, buf, len,
		    compression_level)) != Z_OK) {
			log_warn("compress returned %i", rc);
			free(val->data);
			ber_free(&ber);
			return -1;
		}
		log_debug("compressed entry from %u -> %u byte",
		    len, destlen + sizeof(uint32_t));

		*(uint32_t *)val->data = len;
		val->size = destlen + sizeof(uint32_t);
		val->free_data = 1;
	} else {
		val->data = buf;
		val->size = len;
		val->free_data = 1;	/* XXX: take over internal br_wbuf */
		ber.br_wbuf = NULL;
	}

	ber_free(&ber);

	return 0;
}

struct ber_element *
db2ber(struct btval *val, int compression_level)
{
	int			 rc;
	uLongf			 len;
	void			*buf;
	Bytef			*src;
	uLong			 srclen;
	struct ber_element	*elm;
	struct ber		 ber;

	assert(val != NULL);

	bzero(&ber, sizeof(ber));
	ber.fd = -1;

	if (compression_level > 0) {
		if (val->size < sizeof(uint32_t))
			return NULL;

		len = *(uint32_t *)val->data;
		if ((buf = malloc(len)) == NULL) {
			log_warn("malloc(%u)", len);
			return NULL;
		}

		src = (char *)val->data + sizeof(uint32_t);
		srclen = val->size - sizeof(uint32_t);
		rc = uncompress(buf, &len, src, srclen);
		if (rc != Z_OK) {
			log_warnx("dbt_to_ber: uncompress returned %i", rc);
			free(buf);
			return NULL;
		}

		log_debug("uncompressed entry from %u -> %u byte",
		    val->size, len);

		ber_set_readbuf(&ber, buf, len);
		elm = ber_read_elements(&ber, NULL);
		free(buf);
		return elm;
	} else {
		ber_set_readbuf(&ber, val->data, val->size);
		return ber_read_elements(&ber, NULL);
	}
}

