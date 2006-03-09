/*	$OpenBSD: rcstime.c,v 1.1 2006/03/09 10:56:33 xsa Exp $	*/
/*
 * Copyright (c) 2006 Joris Vink <joris@openbsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL  DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "includes.h"

#include "log.h"
#include "rcs.h"


struct tm *
rcs_set_tz(char *tz, struct rcs_delta *rdp)
{
	int tzone;
	char *h, *m;
	struct tm *tb, ltb;
	time_t now;

	tb = &rdp->rd_date;

	if (!strcmp(tz, "LT")) {
		now = mktime(&rdp->rd_date);
		tb = localtime(&now);
		tb->tm_hour += ((int)tb->tm_gmtoff/3600);
	} else {
		switch (*tz) {
		case '-':
		case '+':
			break;
		default:
			fatal("%s: not a known time zone", tz);
		}

		h = tz;
		if ((m = strrchr(tz, ':')) != NULL)
			*(m++) = '\0';

		ltb = rdp->rd_date;
		tb = &ltb;

		tzone = atoi(h);
		if ((tzone >= 24) && (tzone <= -24))
			fatal("%s: not a known time zone", tz);

		tb->tm_hour += tzone;
		if ((tb->tm_hour >= 24) && (tb->tm_hour <= -24))
			tb->tm_hour = 0;

		tb->tm_gmtoff += (tzone*3600);

		if (m != NULL) {
			tzone = atoi(m);
			if (tzone >= 60)
				fatal("%s: not a known time zone", tz);

			if ((tb->tm_min + tzone) >= 60) {
				tb->tm_hour++;
				tb->tm_min -= tzone;	
			} else
				tb->tm_min += tzone;

			tb->tm_gmtoff += (tzone*60);
		}
	}

	return (tb);
}
