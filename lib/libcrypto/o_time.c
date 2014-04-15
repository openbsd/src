/* crypto/o_time.c -*- mode:C; c-file-style: "eay" -*- */
/* Written by Richard Levitte (richard@levitte.org) for the OpenSSL
 * project 2001.
 */
/* Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project 2008.
 */
/* ====================================================================
 * Copyright (c) 2001 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.OpenSSL.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    licensing@OpenSSL.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.OpenSSL.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com).
 *
 */

#include <openssl/e_os2.h>
#include <string.h>
#include "o_time.h"

struct tm
*OPENSSL_gmtime(const time_t *timer, struct tm *result) {
	struct tm *ts = NULL;

#if defined(OPENSSL_THREADS) && !defined(OPENSSL_SYS_WIN32) && !defined(OPENSSL_SYS_OS2) && (!defined(OPENSSL_SYS_VMS) || defined(gmtime_r)) && !defined(OPENSSL_SYS_MACOSX) && !defined(OPENSSL_SYS_SUNOS)
	/* should return &data, but doesn't on some systems,
	   so we don't even look at the return value */
	gmtime_r(timer, result);
	ts = result;
#else
	ts = gmtime(timer);
	if (ts == NULL)
		return NULL;

	memcpy(result, ts, sizeof(struct tm));
	ts = result;
#endif
	return ts;
}

/* Take a tm structure and add an offset to it. This avoids any OS issues
 * with restricted date types and overflows which cause the year 2038
 * problem.
 */

#define SECS_PER_DAY (24 * 60 * 60)

static long date_to_julian(int y, int m, int d);
static void julian_to_date(long jd, int *y, int *m, int *d);

int
OPENSSL_gmtime_adj(struct tm *tm, int off_day, long offset_sec)
{
	int offset_hms, offset_day;
	long time_jd;
	int time_year, time_month, time_day;
	/* split offset into days and day seconds */
	offset_day = offset_sec / SECS_PER_DAY;
	/* Avoid sign issues with % operator */
	offset_hms = offset_sec - (offset_day * SECS_PER_DAY);
	offset_day += off_day;
	/* Add current time seconds to offset */
	offset_hms += tm->tm_hour * 3600 + tm->tm_min * 60 + tm->tm_sec;
	/* Adjust day seconds if overflow */
	if (offset_hms >= SECS_PER_DAY) {
		offset_day++;
		offset_hms -= SECS_PER_DAY;
	} else if (offset_hms < 0) {
		offset_day--;
		offset_hms += SECS_PER_DAY;
	}

	/* Convert date of time structure into a Julian day number.
	 */

	time_year = tm->tm_year + 1900;
	time_month = tm->tm_mon + 1;
	time_day = tm->tm_mday;

	time_jd = date_to_julian(time_year, time_month, time_day);

	/* Work out Julian day of new date */
	time_jd += offset_day;

	if (time_jd < 0)
		return 0;

	/* Convert Julian day back to date */

	julian_to_date(time_jd, &time_year, &time_month, &time_day);

	if (time_year < 1900 || time_year > 9999)
		return 0;

	/* Update tm structure */

	tm->tm_year = time_year - 1900;
	tm->tm_mon = time_month - 1;
	tm->tm_mday = time_day;

	tm->tm_hour = offset_hms / 3600;
	tm->tm_min = (offset_hms / 60) % 60;
	tm->tm_sec = offset_hms % 60;

	return 1;

}

/* Convert date to and from julian day
 * Uses Fliegel & Van Flandern algorithm
 */
static long
date_to_julian(int y, int m, int d)
{
	return (1461 * (y + 4800 + (m - 14) / 12)) / 4 +
	    (367 * (m - 2 - 12 * ((m - 14) / 12))) / 12 -
	    (3 * ((y + 4900 + (m - 14) / 12) / 100)) / 4 +
	    d - 32075;
}

static void
julian_to_date(long jd, int *y, int *m, int *d)
{
	long  L = jd + 68569;
	long  n = (4 * L) / 146097;
	long  i, j;

	L = L - (146097 * n + 3) / 4;
	i = (4000 * (L + 1)) / 1461001;
	L = L - (1461 * i) / 4 + 31;
	j = (80 * L) / 2447;
	*d = L - (2447 * j) / 80;
	L = j / 11;
	*m = j + 2 - (12 * L);
	*y = 100 * (n - 49) + i + L;
}

#ifdef OPENSSL_TIME_TEST

#include <stdio.h>

/* Time checking test code. Check times are identical for a wide range of
 * offsets. This should be run on a machine with 64 bit time_t or it will
 * trigger the very errors the routines fix.
 */

int
main(int argc, char **argv)
{
	long offset;
	for (offset = 0; offset < 1000000; offset++) {
		check_time(offset);
		check_time(-offset);
		check_time(offset * 1000);
		check_time(-offset * 1000);
	}
}

int
check_time(long offset)
{
	struct tm tm1, tm2;
	time_t t1, t2;
	time(&t1);
	t2 = t1 + offset;
	OPENSSL_gmtime(&t2, &tm2);
	OPENSSL_gmtime(&t1, &tm1);
	OPENSSL_gmtime_adj(&tm1, 0, offset);
	if ((tm1.tm_year == tm2.tm_year) &&
		(tm1.tm_mon == tm2.tm_mon) &&
	(tm1.tm_mday == tm2.tm_mday) &&
	(tm1.tm_hour == tm2.tm_hour) &&
	(tm1.tm_min == tm2.tm_min) &&
	(tm1.tm_sec == tm2.tm_sec))
	return 1;
	fprintf(stderr, "TIME ERROR!!\n");
	fprintf(stderr, "Time1: %d/%d/%d, %d:%02d:%02d\n",
	tm2.tm_mday, tm2.tm_mon + 1, tm2.tm_year + 1900,
	tm2.tm_hour, tm2.tm_min, tm2.tm_sec);
	fprintf(stderr, "Time2: %d/%d/%d, %d:%02d:%02d\n",
	tm1.tm_mday, tm1.tm_mon + 1, tm1.tm_year + 1900,
	tm1.tm_hour, tm1.tm_min, tm1.tm_sec);
	return 0;
}

#endif
