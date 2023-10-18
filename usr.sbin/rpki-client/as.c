/*	$OpenBSD: as.c,v 1.15 2023/10/18 07:10:24 tb Exp $ */
/*
 * Copyright (c) 2019 Kristaps Dzonsons <kristaps@bsd.lv>
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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "extern.h"

/* Parse a uint32_t AS identifier from an ASN1_INTEGER. */
int
as_id_parse(const ASN1_INTEGER *v, uint32_t *out)
{
	uint64_t res = 0;

	if (!ASN1_INTEGER_get_uint64(&res, v))
		return 0;
	if (res > UINT32_MAX)
		return 0;
	*out = res;
	return 1;
}

/*
 * Given a newly-parsed AS number or range "a", make sure that "a" does
 * not overlap with any other numbers or ranges in the "as" array.
 * This is defined by RFC 3779 section 3.2.3.4.
 * Returns zero on failure, non-zero on success.
 */
int
as_check_overlap(const struct cert_as *a, const char *fn,
    const struct cert_as *as, size_t asz, int quiet)
{
	size_t	 i;

	/* We can have only one inheritance statement. */

	if (asz &&
	    (a->type == CERT_AS_INHERIT || as[0].type == CERT_AS_INHERIT)) {
		if (!quiet) {
			warnx("%s: RFC 3779 section 3.2.3.3: "
			    "cannot have inheritance and multiple ASnum or "
			    "multiple inheritance", fn);
		}
		return 0;
	}

	/* Now check for overlaps between singletons/ranges. */

	for (i = 0; i < asz; i++) {
		switch (as[i].type) {
		case CERT_AS_ID:
			switch (a->type) {
			case CERT_AS_ID:
				if (a->id != as[i].id)
					continue;
				break;
			case CERT_AS_RANGE:
				if (as->range.min > as[i].id ||
				    as->range.max < as[i].id)
					continue;
				break;
			default:
				abort();
			}
			break;
		case CERT_AS_RANGE:
			switch (a->type) {
			case CERT_AS_ID:
				if (as[i].range.min > a->id ||
				    as[i].range.max < a->id)
					continue;
				break;
			case CERT_AS_RANGE:
				if (a->range.max < as[i].range.min ||
				    a->range.min > as[i].range.max)
					continue;
				break;
			default:
				abort();
			}
			break;
		default:
			abort();
		}
		if (!quiet) {
			warnx("%s: RFC 3779 section 3.2.3.4: "
			    "cannot have overlapping ASnum", fn);
		}
		return 0;
	}

	return 1;
}

/*
 * See if a given AS range (which may be the same number, in the case of
 * singleton AS identifiers) is covered by the AS numbers or ranges
 * specified in the "as" array.
 * Return <0 if there is no cover, 0 if we're inheriting, >0 if there is.
 */
int
as_check_covered(uint32_t min, uint32_t max,
    const struct cert_as *as, size_t asz)
{
	size_t	 i;
	uint32_t amin, amax;

	for (i = 0; i < asz; i++) {
		if (as[i].type == CERT_AS_INHERIT)
			return 0;
		amin = as[i].type == CERT_AS_RANGE ?
		    as[i].range.min : as[i].id;
		amax = as[i].type == CERT_AS_RANGE ?
		    as[i].range.max : as[i].id;
		if (min >= amin && max <= amax)
			return 1;
	}

	return -1;
}

void
as_warn(const char *fn, const struct cert_as *cert, const char *msg)
{
	switch (cert->type) {
	case CERT_AS_ID:
		warnx("%s: AS %u: %s", fn, cert->id, msg);
		break;
	case CERT_AS_RANGE:
		warnx("%s: AS range %u--%u: %s", fn, cert->range.min,
		    cert->range.max, msg);
		break;
	case CERT_AS_INHERIT:
		warnx("%s: AS (inherit): %s", fn, msg);
		break;
	default:
		warnx("%s: corrupt cert", fn);
		break;
	}
}
