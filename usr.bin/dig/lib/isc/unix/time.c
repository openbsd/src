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

/* $Id: time.c,v 1.12 2020/02/16 21:07:33 florian Exp $ */

/*! \file */



#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <time.h>

#include <sys/time.h>	/* Required for struct timeval on some platforms. */


#include <string.h>
#include <isc/time.h>

#include <isc/util.h>

#define NS_PER_S	1000000000	/*%< Nanoseconds per second. */
#define NS_PER_US	1000		/*%< Nanoseconds per microsecond. */

/*
 * All of the INSIST()s checks of nanoseconds < NS_PER_S are for
 * consistency checking of the type. In lieu of magic numbers, it
 * is the best we've got.  The check is only performed on functions which
 * need an initialized type.
 */

/***
 *** Absolute Times
 ***/

isc_result_t
isc_time_now(struct timespec *t) {
	REQUIRE(t != NULL);

	if (clock_gettime(CLOCK_REALTIME, t) == -1) {
		UNEXPECTED_ERROR(__FILE__, __LINE__, "%s", strerror(errno));
		return (ISC_R_UNEXPECTED);
	}
	return (ISC_R_SUCCESS);
}

int
isc_time_compare(const struct timespec *t1, const struct timespec *t2) {
	REQUIRE(t1 != NULL && t2 != NULL);
	INSIST(t1->tv_nsec < NS_PER_S && t2->tv_nsec < NS_PER_S);

	if (t1->tv_sec < t2->tv_sec)
		return (-1);
	if (t1->tv_sec > t2->tv_sec)
		return (1);
	if (t1->tv_nsec < t2->tv_nsec)
		return (-1);
	if (t1->tv_nsec > t2->tv_nsec)
		return (1);
	return (0);
}

isc_result_t
isc_time_add(const struct timespec *t, const struct timespec *i, struct timespec *result)
{
	REQUIRE(t != NULL && i != NULL && result != NULL);
	INSIST(t->tv_nsec < NS_PER_S && i->tv_nsec < NS_PER_S);

	timespecadd(t, i, result);
	return (ISC_R_SUCCESS);
}

isc_result_t
isc_time_subtract(const struct timespec *t, const struct timespec *i,
		  struct timespec *result)
{
	REQUIRE(t != NULL && i != NULL && result != NULL);
	INSIST(t->tv_nsec < NS_PER_S && i->tv_nsec < NS_PER_S);

	timespecsub(t, i, result);
	return (ISC_R_SUCCESS);
}

uint64_t
isc_time_microdiff(const struct timespec *t1, const struct timespec *t2) {
	struct timespec res;

	REQUIRE(t1 != NULL && t2 != NULL);

	timespecsub(t1, t2, &res);
	if (res.tv_sec < 0)
		return 0;

	return ((res.tv_sec * NS_PER_S + res.tv_nsec) / NS_PER_US);
}

void
isc_time_formattimestamp(const struct timespec *t, char *buf, unsigned int len) {
	unsigned int flen;

	REQUIRE(t != NULL);
	INSIST(t->tv_nsec < NS_PER_S);
	REQUIRE(buf != NULL);
	REQUIRE(len > 0);

	flen = strftime(buf, len, "%d-%b-%Y %X", localtime(&t->tv_sec));
	INSIST(flen < len);
	if (flen != 0) {
		snprintf(buf + flen, len - flen,
			 ".%03ld", t->tv_nsec / 1000000);
	} else {
		strlcpy(buf, "99-Bad-9999 99:99:99.999", len);
	}
}

void
isc_time_formathttptimestamp(const struct timespec *t, char *buf, unsigned int len) {
	unsigned int flen;

	REQUIRE(t != NULL);
	INSIST(t->tv_nsec < NS_PER_S);
	REQUIRE(buf != NULL);
	REQUIRE(len > 0);

	/*
	 * 5 spaces, 1 comma, 3 GMT, 2 %d, 4 %Y, 8 %H:%M:%S, 3+ %a, 3+ %b (29+)
	 */

	flen = strftime(buf, len, "%a, %d %b %Y %H:%M:%S GMT",
	    gmtime(&t->tv_sec));
	INSIST(flen < len);
}
