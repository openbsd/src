/* $OpenBSD: a_time_tm.c,v 1.1 2015/10/02 15:04:45 beck Exp $ */
/*
 * Copyright (c) 2015 Bob Beck <beck@openbsd.org>
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

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <sys/limits.h>

#include <openssl/asn1t.h>
#include <openssl/err.h>

#include "o_time.h"
#include "asn1_locl.h"

char *
gentime_string_from_tm(struct tm *tm)
{
	char *ret = NULL;
	int year;
	year = tm->tm_year + 1900;
	if (year < 0 || year > 9999)
		return (NULL);
	if (asprintf(&ret, "%04u%02u%02u%02u%02u%02uZ", year,
	    tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min,
	    tm->tm_sec) == -1)
		ret = NULL;
	return (ret);
}

char *
utctime_string_from_tm(struct tm *tm)
{
	char *ret = NULL;
	if (tm->tm_year >= 150 || tm->tm_year < 50)
		return (NULL);
	if (asprintf(&ret, "%02u%02u%02u%02u%02u%02uZ",
	    tm->tm_year % 100,  tm->tm_mon + 1, tm->tm_mday,
	    tm->tm_hour, tm->tm_min, tm->tm_sec) == -1)
		ret = NULL;
	return (ret);
}

/*
 * Parse an ASN.1 time string.
 *
 * mode must be:
 * 0 if we expect to parse a time as specified in RFC 5280 from an
 * X509 certificate.
 * V_ASN1_UTCTIME if we wish to parse a legacy ASN1 UTC time.
 * V_ASN1_GENERALIZEDTIME if we wish to parse a legacy ASN1
 * Generalizd time.
 *
 * Returns:
 * -1 if the string was invalid.
 * V_ASN1_UTCTIME if the string validated as a UTC time string.
 * V_ASN1_GENERALIZEDTIME if the string validated as a Generalized time string.
 *
 * Fills in *tm with the corresponding time if tm is non NULL.
 */
#define RFC5280 0
#define	ATOI2(ar)	((ar) += 2, ((ar)[-2] - '0') * 10 + ((ar)[-1] - '0'))
int asn1_time_parse(const char * bytes, size_t len, struct tm *tm, int mode)
{
	char *p, *buf = NULL, *dot = NULL, *tz = NULL;
	int i, offset, noseconds = 0, type = 0;
	struct tm ltm;
	struct tm *lt;
	size_t tlen;
	char tzc;

	if (bytes == NULL)
		goto err;

	if (len > INT_MAX)
		goto err;

	/* Constrain the RFC5280 case within max/min valid lengths. */
	if (mode == RFC5280 && (len > 15 || len < 13))
		goto err;

	if ((buf = strndup(bytes, len)) == NULL)
		goto err;
	lt = tm;
	if (lt == NULL) {
		time_t t = time(NULL);
		lt = gmtime_r(&t, &ltm);
		if (lt == NULL)
			goto err;
	}

	/*
	 * Find position of the optional fractional seconds, and the
	 * start of the timezone, while ensuring everything else is
	 * digits.
	 */
	for (i = 0; i < len; i++) {
		char *t = buf + i;
		if (isdigit((unsigned char)*t))
			continue;
		if (*t == '.' && dot == NULL) {
			dot = t;
			continue;
		}
		if ((*t == 'Z' || *t == '+' || *t == '-')  && tz == NULL) {
			tz = t;
			continue;
		}
		goto err;
	}

	/*
	 * Timezone is required. For the non-RFC case it may be
	 * either Z or +- HHMM, but for RFC5280 it may be only Z.
	 */
	if (tz == NULL)
		goto err;
	tzc = *tz;
	*tz++ = '\0';
	if (tzc == 'Z') {
		if (*tz != '\0')
			goto err;
		offset = 0;
	} else if (mode != RFC5280 && (tzc == '+' || tzc == '-') &&
	    (strlen(tz) == 4)) {
		int hours, mins;
		hours = ATOI2(tz);
		mins = ATOI2(tz);
		if (hours > 12 || mins > 59)
			goto err;
		offset = hours * 3600 + mins * 60;
		if (tzc == '-')
			offset = -offset;
	} else
	    goto err;

	if (mode != RFC5280) {
		/* XXX - yuck - OPENSSL_gmtime_adj should go away */
		if (!OPENSSL_gmtime_adj(lt, 0, offset))
			goto err;
	}

	/*
	 * We only allow fractional seconds to be present if we are in
	 * the non-RFC case of a Generalized time. RFC 5280 forbids
	 * fractional seconds.
	 */
	if (dot != NULL) {
		if (mode != V_ASN1_GENERALIZEDTIME)
			goto err;
		*dot++ = '\0';
		if (!isdigit((unsigned char)*dot))
			goto err;
	}

	/*
	 * Validate and convert the time
	 */
	p = buf;
	tlen = strlen(buf);
	switch (tlen) {
	case 14:
		lt->tm_year = (ATOI2(p) * 100) - 1900;	/* cc */
		if (mode == RFC5280 || mode == V_ASN1_GENERALIZEDTIME)
			type = V_ASN1_GENERALIZEDTIME;
		else
			goto err;
		/* FALLTHROUGH */
	case 12:
		if (type == 0 && mode == V_ASN1_GENERALIZEDTIME) {
			/*
			 * In the non-RFC case of a Generalized time
			 * seconds may not have been provided.  RFC
			 * 5280 mandates that seconds must be present.
			 */
			noseconds = 1;
			lt->tm_year = (ATOI2(p) * 100) - 1900;	/* cc */
			type = V_ASN1_GENERALIZEDTIME;
		}
		/* FALLTHROUGH */
	case 10:
		if (type == 0) {
			/*
			 * At this point we must have a UTC time.
			 * In the RFC 5280 case it must have the
			 * seconds present. In the non-RFC case
			 * may have no seconds.
			 */
			if (mode == V_ASN1_GENERALIZEDTIME)
				goto err;
			if (tlen == 10) {
				if (mode == V_ASN1_UTCTIME)
					noseconds = 1;
				else
					goto err;
			}
			type = V_ASN1_UTCTIME;
		}
		lt->tm_year += ATOI2(p);		/* yy */
		if (type == V_ASN1_UTCTIME) {
			if (lt->tm_year < 50)
				lt->tm_year += 100;
		}
		lt->tm_mon = ATOI2(p);			/* mm */
		if ((lt->tm_mon > 12) || !lt->tm_mon)
			goto err;
		--lt->tm_mon;			/* struct tm is 0 - 11 */
		lt->tm_mday = ATOI2(p);			/* dd */
		if ((lt->tm_mday > 31) || !lt->tm_mday)
			goto err;
		lt->tm_hour = ATOI2(p);			/* HH */
		if (lt->tm_hour > 23)
			goto err;
		lt->tm_min = ATOI2(p);			/* MM */
		if (lt->tm_min > 59)
			goto err;
		lt->tm_sec = 0;				/* SS */
		if (noseconds)
			break;
		lt->tm_sec = ATOI2(p);
		/* Leap second 60 is not accepted. Reconsider later? */
		if (lt->tm_sec > 59)
			goto err;
		break;
	default:
		goto err;
	}

	/* RFC 5280 section 4.1.2.5 */
	if (mode == RFC5280 && lt->tm_year < 150 &&
	    type != V_ASN1_UTCTIME)
		goto err;
	if (mode == RFC5280 && lt->tm_year >= 150 &&
	    type != V_ASN1_GENERALIZEDTIME)
		goto err;

	free(buf);
	return type;

err:
	free(buf);
	return -1;
}
