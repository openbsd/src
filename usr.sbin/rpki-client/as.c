/*	$OpenBSD: as.c,v 1.19 2026/05/02 10:36:21 tb Exp $ */
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

#include <assert.h>
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
 * Given a newly-parsed AS number or range "as", make sure that "as" does
 * not overlap with any other numbers or ranges in the "ases" array.
 * This is defined by RFC 3779 section 3.2.3.4.
 * Returns zero on failure, non-zero on success.
 */
int
as_check_overlap(const struct cert_as *as, const char *fn,
    const struct cert_as *ases, size_t num_ases, int quiet)
{
	size_t	 i;

	/* We can have only one inheritance statement. */

	if (num_ases &&
	    (as->type == CERT_AS_INHERIT || ases[0].type == CERT_AS_INHERIT)) {
		if (!quiet) {
			warnx("%s: RFC 3779 section 3.2.3.3: "
			    "cannot have inheritance and multiple ASnum or "
			    "multiple inheritance", fn);
		}
		return 0;
	}

	/* Now check for overlaps between singletons/ranges. */

	for (i = 0; i < num_ases; i++) {
		switch (ases[i].type) {
		case CERT_AS_ID:
			switch (as->type) {
			case CERT_AS_ID:
				if (as->id != ases[i].id)
					continue;
				break;
			case CERT_AS_RANGE:
				if (as->range.min > ases[i].id ||
				    as->range.max < ases[i].id)
					continue;
				break;
			default:
				abort();
			}
			break;
		case CERT_AS_RANGE:
			switch (as->type) {
			case CERT_AS_ID:
				if (ases[i].range.min > as->id ||
				    ases[i].range.max < as->id)
					continue;
				break;
			case CERT_AS_RANGE:
				if (as->range.max < ases[i].range.min ||
				    as->range.min > ases[i].range.max)
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
 * specified in the "ases" array.
 * Return <0 if there is no cover, 0 if we're inheriting, >0 if there is.
 */
int
as_check_covered(uint32_t min, uint32_t max,
    const struct cert_as *ases, size_t num_ases)
{
	size_t	 i;
	uint32_t amin, amax;

	for (i = 0; i < num_ases; i++) {
		if (ases[i].type == CERT_AS_INHERIT)
			return 0;
		amin = ases[i].type == CERT_AS_RANGE ?
		    ases[i].range.min : ases[i].id;
		amax = ases[i].type == CERT_AS_RANGE ?
		    ases[i].range.max : ases[i].id;
		if (min >= amin && max <= amax)
			return 1;
	}

	return -1;
}

void
as_warn(const char *fn, const char *msg, const struct cert_as *as)
{
	switch (as->type) {
	case CERT_AS_ID:
		warnx("%s: %s: AS %u", fn, msg, as->id);
		break;
	case CERT_AS_RANGE:
		warnx("%s: %s: AS range %u--%u", fn, msg, as->range.min,
		    as->range.max);
		break;
	case CERT_AS_INHERIT:
		warnx("%s: %s: AS (inherit)", fn, msg);
		break;
	default:
		warnx("%s: corrupt cert", fn);
		break;
	}
}

/*
 * Append an AS identifier structure to our list of results.
 * Makes sure that the identifiers do not overlap or improperly inherit
 * as defined by RFC 3779 section 3.3.
 */
static int
append_as(const char *fn, struct cert_as *ases, size_t *num_ases,
    const struct cert_as *as)
{
	if (!as_check_overlap(as, fn, ases, *num_ases, 0))
		return 0;
	ases[(*num_ases)++] = *as;
	return 1;
}

/*
 * Parse a range of AS identifiers as in 3.2.3.8.
 * Returns zero on failure, non-zero on success.
 */
int
sbgp_as_range(const char *fn, struct cert_as *ases, size_t *num_ases,
    const ASRange *range)
{
	struct cert_as as;

	memset(&as, 0, sizeof(struct cert_as));
	as.type = CERT_AS_RANGE;

	if (!as_id_parse(range->min, &as.range.min)) {
		warnx("%s: RFC 3779 section 3.2.3.8 (via RFC 1930): "
		    "malformed AS identifier", fn);
		return 0;
	}

	if (!as_id_parse(range->max, &as.range.max)) {
		warnx("%s: RFC 3779 section 3.2.3.8 (via RFC 1930): "
		    "malformed AS identifier", fn);
		return 0;
	}

	if (as.range.max == as.range.min) {
		warnx("%s: RFC 3379 section 3.2.3.8: ASRange: "
		    "range is singular", fn);
		return 0;
	} else if (as.range.max < as.range.min) {
		warnx("%s: RFC 3379 section 3.2.3.8: ASRange: "
		    "range is out of order", fn);
		return 0;
	}

	return append_as(fn, ases, num_ases, &as);
}

/*
 * Parse an entire 3.2.3.10 integer type.
 */
int
sbgp_as_id(const char *fn, struct cert_as *ases, size_t *num_ases,
    const ASN1_INTEGER *i)
{
	struct cert_as as;

	memset(&as, 0, sizeof(struct cert_as));
	as.type = CERT_AS_ID;

	if (!as_id_parse(i, &as.id)) {
		warnx("%s: RFC 3779 section 3.2.3.10 (via RFC 1930): "
		    "malformed AS identifier", fn);
		return 0;
	}
	if (as.id == 0) {
		warnx("%s: RFC 3779 section 3.2.3.10 (via RFC 1930): "
		    "AS identifier zero is reserved", fn);
		return 0;
	}

	return append_as(fn, ases, num_ases, &as);
}

static int
sbgp_as_inherit(const char *fn, struct cert_as *ases, size_t *num_ases)
{
	struct cert_as as;

	memset(&as, 0, sizeof(struct cert_as));
	as.type = CERT_AS_INHERIT;

	return append_as(fn, ases, num_ases, &as);
}

int
sbgp_parse_asids(const char *fn, const ASIdentifiers *asidentifiers,
    struct cert_as **out_as, size_t *out_num_ases)
{
	const ASIdOrRanges *aors = NULL;
	struct cert_as *as = NULL;
	size_t num_ases = 0, num;
	int i;

	assert(*out_as == NULL && *out_num_ases == 0);

	if (asidentifiers->rdi != NULL) {
		warnx("%s: RFC 6487 section 4.8.11: autonomousSysIds: "
		    "should not have RDI values", fn);
		goto out;
	}

	if (asidentifiers->asnum == NULL) {
		warnx("%s: RFC 6487 section 4.8.11: autonomousSysIds: "
		    "no AS number resource set", fn);
		goto out;
	}

	switch (asidentifiers->asnum->type) {
	case ASIdentifierChoice_inherit:
		num = 1;
		break;
	case ASIdentifierChoice_asIdsOrRanges:
		aors = asidentifiers->asnum->u.asIdsOrRanges;
		num = sk_ASIdOrRange_num(aors);
		break;
	default:
		warnx("%s: RFC 3779 section 3.2.3.2: ASIdentifierChoice: "
		    "unknown type %d", fn, asidentifiers->asnum->type);
		goto out;
	}

	if (num == 0) {
		warnx("%s: RFC 6487 section 4.8.11: empty asIdsOrRanges", fn);
		goto out;
	}
	if (num >= MAX_AS_SIZE) {
		warnx("%s: too many AS number entries: limit %d",
		    fn, MAX_AS_SIZE);
		goto out;
	}
	as = calloc(num, sizeof(struct cert_as));
	if (as == NULL)
		err(1, NULL);

	if (aors == NULL) {
		if (!sbgp_as_inherit(fn, as, &num_ases))
			goto out;
	}

	for (i = 0; i < sk_ASIdOrRange_num(aors); i++) {
		const ASIdOrRange *aor;

		aor = sk_ASIdOrRange_value(aors, i);
		switch (aor->type) {
		case ASIdOrRange_id:
			if (!sbgp_as_id(fn, as, &num_ases, aor->u.id))
				goto out;
			break;
		case ASIdOrRange_range:
			if (!sbgp_as_range(fn, as, &num_ases, aor->u.range))
				goto out;
			break;
		default:
			warnx("%s: RFC 3779 section 3.2.3.5: ASIdOrRange: "
			    "unknown type %d", fn, aor->type);
			goto out;
		}
	}

	*out_as = as;
	*out_num_ases = num_ases;

	return 1;

 out:
	free(as);

	return 0;
}
