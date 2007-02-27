/*	$OpenBSD: rcstime.c,v 1.3 2007/02/27 07:59:13 xsa Exp $	*/
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

#include <err.h>
#include <stdlib.h>
#include <string.h>

#include "rcs.h"

void
rcs_set_tz(char *tz, struct rcs_delta *rdp, struct tm *tb)
{
	int tzone;
	int pos;
	char *h, *m;
	struct tm *ltb;
	time_t now;

	if (!strcmp(tz, "LT")) {
		now = mktime(&rdp->rd_date);
		ltb = localtime(&now);
		ltb->tm_hour += ((int)ltb->tm_gmtoff/3600);
		memcpy(tb, ltb, sizeof(*tb));
	} else {
		pos = 0;
		switch (*tz) {
		case '-':
			break;
		case '+':
			pos = 1;
			break;
		default:
			errx(1, "%s: not a known time zone", tz);
		}

		h = (tz + 1);
		if ((m = strrchr(tz, ':')) != NULL)
			*(m++) = '\0';

		memcpy(tb, &rdp->rd_date, sizeof(*tb));

		tzone = atoi(h);
		if ((tzone >= 24) && (tzone <= -24))
			errx(1, "%s: not a known time zone", tz);

		if (pos) {
			tb->tm_hour += tzone;
			tb->tm_gmtoff += (tzone * 3600);
		} else {
			tb->tm_hour -= tzone;
			tb->tm_gmtoff -= (tzone * 3600);
		}

		if ((tb->tm_hour >= 24) || (tb->tm_hour <= -24))
			tb->tm_hour = 0;

		if (m != NULL) {
			tzone = atoi(m);
			if (tzone >= 60)
				errx(1, "%s: not a known time zone", tz);

			if ((tb->tm_min + tzone) >= 60) {
				tb->tm_hour++;
				tb->tm_min -= (60 - tzone);
			} else
				tb->tm_min += tzone;

			tb->tm_gmtoff += (tzone*60);
		}
	}
}
