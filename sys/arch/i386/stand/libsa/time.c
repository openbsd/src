/*	$OpenBSD: time.c,v 1.4 1997/07/08 03:41:00 mickey Exp $	*/

/*
 * Copyright (c) 1997 Tobias Weingartner
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Tobias Weingartner.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR 
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <libsa.h>
#include <sys/time.h>
#include "biosdev.h"

#define isleap(y) ((((y) % 4) == 0 && ((y) % 100) != 0) || ((y) % 400) == 0)

/*
 * Convert from bcd (packed) to int
 */
static int
bcdtoint(register char c){

	return ((c & 0xf0) >> 4) * 10 + (c & 0x0f);
}

/* Number of days per month */
static char monthcount[] = {
	31, 28, 31, 30, 31, 30, 31,
	31, 30, 31, 30, 31, 30, 31
};

/*
 * Quick compute of time in seconds since the Epoch
 */
static time_t
compute(int year, int month, int day, int hour, int min, int sec) {
	register time_t tt;
	register int i;

	/* Compute years of seconds */
	tt = (year - 1970) * (365 * 24 * 60 * 60);

	/* Compute days of seconds */
	for(i = 1; i < month; i++)
		day += monthcount[i];

	/* Compute for leap year */
	for(i = 1970; i < year; i++)
		if(isleap(i))
			day++;
	tt += day * (24 * 60 * 60);

	/* Plus the time */
	tt += sec + 60 * (min + 60 * hour);

	return tt;
}


/*
 * Return time since epoch
 */
time_t
getsecs(void) {

	char timebuf[4], datebuf[4];

	/* Query BIOS for time & date */
	if(!biostime(timebuf) && !biosdate(datebuf)) {
#ifdef notdef
		int dst;

		dst = bcdtoint(timebuf[3]);
#endif
		/* Convert to seconds since Epoch */
		return compute(bcdtoint(datebuf[0])*100 + bcdtoint(datebuf[1]),
			       bcdtoint(datebuf[2]), bcdtoint(datebuf[3]),
			       bcdtoint(timebuf[0]), bcdtoint(timebuf[1]),
			       bcdtoint(timebuf[2]));
	} else
		errno = EIO;

	return(1);
}


/*
 * Return time since epoch
 */
void
time_print(void) {
	char timebuf[4], datebuf[4];

	/* Query BIOS for time & date */
	if(!biostime(timebuf) && !biosdate(datebuf)) {
#ifdef notdef
		int dst;

		dst = bcdtoint(timebuf[3]);
#endif
		/* Convert to sane values */
		printf("%d/%d/%d - %d:%d:%d\n",
		       bcdtoint(datebuf[3]), bcdtoint(datebuf[2]),
		       bcdtoint(datebuf[0]) * 100 + bcdtoint(datebuf[1]),
		       bcdtoint(timebuf[0]), bcdtoint(timebuf[1]),
		       bcdtoint(timebuf[2]));
	} else
		printf("Error in biostime() or biosdate().\n");

	return;
}

u_int
sleep(i)
	u_int i;
{
	register time_t t;

	/* loop for that number of seconds, polling BIOS,
	   so that it may handle interrupts */
	for (t = getsecs() + i; getsecs() < t; ischar());

	return 0;
}
