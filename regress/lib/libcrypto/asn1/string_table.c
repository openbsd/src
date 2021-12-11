/* $OpenBSD: string_table.c,v 1.1 2021/12/11 22:58:48 schwarze Exp $ */
/*
 * Copyright (c) 2021 Ingo Schwarze <schwarze@openbsd.org>
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
#include <stdarg.h>
#include <openssl/asn1.h>
#include <openssl/objects.h>

static int errcount;

static void
report(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vwarnx(fmt, ap);
	va_end(ap);

	errcount++;
}

static void
stable_check(const char *testname, ASN1_STRING_TABLE *have,
    ASN1_STRING_TABLE *want, unsigned long want_flags)
{
	if (have == NULL) {
		report("%s returned NULL", testname);
		return;
	}
	if (have->nid != want->nid)
		report("%s nid %d, expected %d", testname,
		    have->nid, want->nid);
	if (have->minsize != want->minsize)
		report("%s minsize %ld, expected %ld", testname,
		    have->minsize, want->minsize);
	if (have->maxsize != want->maxsize)
		report("%s maxsize %ld, expected %ld", testname,
		    have->maxsize, want->maxsize);
	if (have->mask != want->mask)
		report("%s mask %lu, expected %lu", testname,
		    have->mask, want->mask);
	if (have->flags != want_flags)
		report("%s flags %lu, expected %lu", testname,
		    have->flags, want_flags);
}

int
main(void)
{
	ASN1_STRING_TABLE	 orig, mine, *have;
	int			 irc;

	orig.nid = NID_name;
	orig.minsize = 1;
	orig.maxsize = ub_name;
	orig.mask = DIRSTRING_TYPE;
	orig.flags = 0;

	mine.nid = NID_name;
	mine.minsize = 4;
	mine.maxsize = 64;
	mine.mask = B_ASN1_PRINTABLESTRING;
	mine.flags = STABLE_NO_MASK;

	/* Original entry. */

	have = ASN1_STRING_TABLE_get(orig.nid);
	stable_check("orig", have, &orig, 0);

	/* Copy, but don't really change. */

	irc = ASN1_STRING_TABLE_add(orig.nid, -1, -1, 0, 0);
	if (irc != 1)
		report("set noop returned %d, expected 1", irc);
	have = ASN1_STRING_TABLE_get(orig.nid);
	stable_check("noop", have, &orig, STABLE_FLAGS_MALLOC);

	/* Change entry. */

	irc = ASN1_STRING_TABLE_add(mine.nid, mine.minsize, mine.maxsize,
	    mine.mask, mine.flags);
	if (irc != 1)
		report("set returned %d, expected 1", irc);
	have = ASN1_STRING_TABLE_get(mine.nid);
	stable_check("set", have, &mine, STABLE_FLAGS_MALLOC | STABLE_NO_MASK);

	/* New entry. */

	mine.nid = NID_title;
	irc = ASN1_STRING_TABLE_add(mine.nid, mine.minsize, mine.maxsize,
	    mine.mask, mine.flags);
	if (irc != 1)
		report("new returned %d, expected 1", irc);
	have = ASN1_STRING_TABLE_get(mine.nid);
	stable_check("new", have, &mine, STABLE_FLAGS_MALLOC | STABLE_NO_MASK);

	/* Back to the initial state. */

	ASN1_STRING_TABLE_cleanup();
	have = ASN1_STRING_TABLE_get(orig.nid);
	stable_check("back", have, &orig, 0);
	if (ASN1_STRING_TABLE_get(mine.nid) != NULL)
		report("deleted entry is not NULL");

	switch (errcount) {
	case 0:
		return 0;
	case 1:
		errx(1, "one error");
	default:
		errx(1, "%d errors", errcount);
	}
}
