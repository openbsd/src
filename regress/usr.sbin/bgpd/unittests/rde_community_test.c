/*	$OpenBSD: rde_community_test.c,v 1.5 2022/05/31 09:46:54 claudio Exp $ */

/*
 * Copyright (c) 2019 Claudio Jeker <claudio@openbsd.org>
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
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rde.h"
#include "log.h"

#include "rde_community_test.h"

struct rde_memstats rdemem;
struct rde_community comm;
struct rde_peer peer = {
	.conf.remote_as = 22512,
	.conf.local_as = 42,
};

static void
dump(uint8_t *b, size_t len)
{
	size_t l;

	printf("\n\t{\n\t\t.data = \"");
	for (l = 0; l < len; l++) {
		printf("\\x%02x", b[l]);
		if (l % 12 == 0 && l != 0)
			printf("\"\n\t\t    \"");
	}
	printf("\",\n\t\t.size = %zu\n\t},\n", len);
}

static int
test_parsing(size_t num, uint8_t *in, size_t inlen)
{
	const char *func = "community";
	uint8_t flags, type, attr[256];
	size_t skip = 2;
	uint16_t attr_len;
	int r;

	communities_clean(&comm);

	flags = in[0];
	type = in[1];
	if (flags & ATTR_EXTLEN) {
		memcpy(&attr_len, in + 2, sizeof(attr_len));
		attr_len = ntohs(attr_len);
		skip += 2;
	} else {
		attr_len = in[2];
		skip += 1;
	}

	switch (type) {
	case ATTR_COMMUNITIES:
		r = community_add(&comm, flags, in + skip, attr_len);
		break;
	case ATTR_EXT_COMMUNITIES:
		r = community_ext_add(&comm, flags, 0, in + skip, attr_len);
		break;
	case ATTR_LARGE_COMMUNITIES:
		r = community_large_add(&comm, flags, in + skip, attr_len);
		break;
	}
	if (r == -1) {
		printf("Test %zu: %s_add failed\n", num, func);
		return -1;
	}

	switch (type) {
	case ATTR_COMMUNITIES:
		r = community_write(&comm, attr, sizeof(attr));
		break;
	case ATTR_EXT_COMMUNITIES:
		r = community_ext_write(&comm, 0, attr, sizeof(attr));
		break;
	case ATTR_LARGE_COMMUNITIES:
		r = community_large_write(&comm, attr, sizeof(attr));
		break;
	}

	if (r != inlen) {
		printf("Test %zu: %s_write return value %d != %zd\n",
		    num, func, r, inlen);
		return -1;
	}
	if (r != -1 && memcmp(attr, in, inlen) != 0) {
		printf("Test %zu: %s_write unexpected encoding: ", num, func);
		dump(attr, inlen);
		printf("expected: ");
		dump(in, inlen);
		return -1;
	}

	return 0;
}

static int
test_filter(size_t num, struct testfilter *f)
{
	size_t l;
	int r;

	communities_clean(&comm);
	for (l = 0; f->in[l] != -1; l++) {
		r = community_set(&comm, &filters[f->in[l]], &peer);
		if (r != 1) {
			printf("Test %zu: community_set %zu "
			    "unexpected return %d != 1\n",
			    num, l, r);
			return -1;
		}
	}

	if (f->match != -1) {
		r = community_match(&comm, &filters[f->match], &peer);
		if (r != f->mout) {
			printf("Test %zu: community_match "
			    "unexpected return %d != %d\n", num, r, f->mout);
			return -1;
		}
	}

	if (f->delete != -1) {
		community_delete(&comm, &filters[f->delete], &peer);

		if (community_match(&comm, &filters[f->delete], &peer) != 0) {
			printf("Test %zu: community_delete still around\n",
			    num);
			return -1;
		}
	}

	if (f->ncomm != 0) {
		if (community_count(&comm, COMMUNITY_TYPE_BASIC) !=
		    f->ncomm - 1) {
			printf("Test %zu: community_count unexpected "
			    "return %d != %d\n", num, r, f->ncomm - 1);
			return -1;
		}
	}

	if (f->next != 0) {
		if (community_count(&comm, COMMUNITY_TYPE_EXT) !=
		    f->next - 1) {
			printf("Test %zu: community_count unexpected "
			    "return %d != %d\n", num, r, f->next - 1);
			return -1;
		}
	}

	if (f->nlarge != 0) {
		if (community_count(&comm, COMMUNITY_TYPE_LARGE) !=
		    f->nlarge - 1) {
			printf("Test %zu: community_count unexpected "
			    "return %d != %d\n", num, r, f->nlarge - 1);
			return -1;
		}
	}

	return 0;
}

int
main(int argc, char *argv[])
{
	size_t t;
	int error = 0;

	for (t = 0; t < sizeof(vectors) / sizeof(*vectors); t++) {
		if (test_parsing(t, vectors[t].data, vectors[t].size) == -1)
			error = 1;
	}

	for (t = 0; t < sizeof(testfilters) / sizeof(*testfilters); t++) {
		if (test_filter(t, &testfilters[t]) == -1)
			error = 1;
	}

	if (!error)
		printf("OK\n");
	return error;
}

__dead void
fatalx(const char *emsg, ...)
{
	va_list ap;
	va_start(ap, emsg);
	verrx(2, emsg, ap);
}

__dead void
fatal(const char *emsg, ...)
{
	va_list ap;
	va_start(ap, emsg);
	verr(2, emsg, ap);
}

void
log_warnx(const char *emsg, ...)
{
	va_list  ap;
	va_start(ap, emsg);
	vwarnx(emsg, ap);
	va_end(ap);
}

int
attr_write(void *p, uint16_t p_len, uint8_t flags, uint8_t type,
    void *data, uint16_t data_len)
{
	u_char		*b = p;
	uint16_t	 tmp, tot_len = 2; /* attribute header (without len) */

	flags &= ~ATTR_DEFMASK;
	if (data_len > 255) {
		tot_len += 2 + data_len;
		flags |= ATTR_EXTLEN;
	} else {
		tot_len += 1 + data_len;
	}

	if (tot_len > p_len)
		return (-1);

	*b++ = flags;
	*b++ = type;
	if (data_len > 255) {
		tmp = htons(data_len);
		memcpy(b, &tmp, sizeof(tmp));
		b += 2;
	} else
		*b++ = (u_char)data_len;

	if (data == NULL)
		return (tot_len - data_len);

	if (data_len != 0)
		memcpy(b, data, data_len);

	return (tot_len);
}

int
attr_writebuf(struct ibuf *buf, uint8_t flags, uint8_t type, void *data,
    uint16_t data_len)
{
	return (-1);
}
