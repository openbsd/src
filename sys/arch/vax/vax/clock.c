/*	$NetBSD: clock.c,v 1.8 1995/11/30 00:59:32 jtc Exp $	*/
/*
 * Copyright (c) 1995 Ludd, University of Lule}, Sweden.
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
 *     This product includes software developed at Ludd, University of Lule}.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include <sys/param.h>
#include <sys/kernel.h>

#include "machine/mtpr.h"
#include "machine/sid.h"

#define SEC_PER_DAY (60*60*24)

extern int todrstopped;

static unsigned long year;     /*  start of current year in seconds */
static unsigned long year_len; /* length of current year in 100th of seconds */

/*
 * microtime() should return number of usecs in struct timeval.
 * We may get wrap-arounds, but that will be fixed with lasttime
 * check. This may fault within 10 msecs.
 */
void
microtime(tvp)
	struct timeval *tvp;
{
	u_int int_time, tmp_year;
	int s, i;
	static struct timeval lasttime;

	s = splhigh();
	int_time = mfpr(PR_TODR);

        asm ("movc3 %0,(%1),(%2)" 
                :
                : "r" (sizeof(struct timeval)),"r" (&time),"r"(tvp)
                :"r0","r1","r2","r3","r4","r5"); 

	i = mfpr(PR_ICR) + tick; /* Get current interval count */
	tvp->tv_usec += i;
	while (tvp->tv_usec > 1000000) {
		tvp->tv_sec++;
		tvp->tv_usec -= 1000000;
	}
	if (tvp->tv_sec == lasttime.tv_sec &&
	    tvp->tv_usec <= lasttime.tv_usec &&
	    (tvp->tv_usec = lasttime.tv_usec + 1) > 1000000) {
		tvp->tv_sec++;
		tvp->tv_usec -= 1000000;
	}
	bcopy(tvp, &lasttime, sizeof(struct timeval));
	if (int_time > year_len) {
		mtpr(mfpr(PR_TODR) - year_len, PR_TODR);
		year += year_len / 100;
		tmp_year = year / SEC_PER_DAY / 365 + 2;
		year_len = 100 * SEC_PER_DAY *
		    ((tmp_year % 4 && tmp_year != 32) ? 365 : 366);
	}
	splx(s);
}

/*
 * Sets year to the year in fs_time and then calculates the number of
 * 100th of seconds in the current year and saves that info in year_len.
 * fs_time contains the time set in the superblock in the root filesystem.
 * If the clock is started, it then checks if the time is valid
 * compared with the time in fs_time. If the clock is stopped, an
 * alert is printed and the time is temporary set to the time in fs_time.
 */

void
inittodr(fs_time) 
	time_t fs_time;
{

	unsigned long tmp_year, sluttid, year_ticks;
	int clock_stopped;

	sluttid = fs_time;
	year = (fs_time / SEC_PER_DAY / 365) * 365 * SEC_PER_DAY;
	tmp_year = year / SEC_PER_DAY / 365 + 2;
	year_len = 100 * SEC_PER_DAY *
	    ((tmp_year % 4 && tmp_year != 32) ? 365 : 366);

	switch (cpunumber) {
#if VAX750
	case VAX_750:
		year_ticks = mfpr(PR_TODR);
		clock_stopped = todrstopped;
		break;
#endif
#if VAX630 || VAX410
	case VAX_78032:
		year_ticks = uvaxII_gettodr(&clock_stopped);
		break;
#endif
	default:
		year_ticks = 0;
		clock_stopped = 1;
	};

	if (clock_stopped){
		printf(
	"Internal clock not started. Using time from file system.\n");
		switch (cpunumber) {
#if VAX750
		case VAX_750:
			/*+1 so the clock won't be stopped */
			mtpr((fs_time - year) * 100 + 1, PR_TODR);
			break;
#endif
#if VAX630 || VAX410
		case VAX_78032:
			uvaxII_settodr((fs_time - year) * 100 + 1);
			break;
#endif
		};
		todrstopped = 0;
	} else if (year_ticks / 100 > fs_time - year + SEC_PER_DAY * 3) {
		printf(
	"WARNING: Clock has gained %d days - CHECK AND RESET THE DATE.\n",
		    (year_ticks / 100 - (fs_time - year)) / SEC_PER_DAY);
		sluttid = year + (year_ticks / 100);
	} else if (year_ticks / 100 < fs_time - year) {
		printf(
		"WARNING: Clock has lost time - CHECK AND RESET THE DATE.\n");
	} else sluttid = year + (year_ticks / 100);
	time.tv_sec = sluttid;
}

/*   
 * Resettodr restores the time of day hardware after a time change.
 */

void
resettodr()
{

	unsigned long tmp_year;

	year = (time.tv_sec / SEC_PER_DAY / 365) * 365 * SEC_PER_DAY;
	tmp_year = year / SEC_PER_DAY / 365 + 2;
	year_len = 100 * SEC_PER_DAY *
	    ((tmp_year % 4 && tmp_year != 32) ? 365 : 366);
	switch (cpunumber) {
#if VAX750
	case VAX_750:
		mtpr((time.tv_sec - year) * 100 + 1, PR_TODR);
		break;
#endif
#if VAX630 || VAX410
	case VAX_78032:
		uvaxII_settodr((time.tv_sec - year) * 100 + 1);
		break;
#endif
	};
	todrstopped = 0;
}

/*
 * Unfortunately the 78032 cpu chip (MicroVAXII cpu) does not have a functional
 * todr register, so this function is necessary.
 * (the x and y variables are used to confuse the optimizer enough to ensure
 *  that the code actually loops:-)
 */
int
todr()
{
      int delaycnt, x = 4, y = 4;
      static int todr_val;

      if (cpunumber != VAX_78032)
	      return (mfpr(PR_TODR));

      /*
       * Loop for approximately 10msec and then return todr_val + 1.
       */
      delaycnt = 5000;
      while (delaycnt > 0)
	      delaycnt = delaycnt - x + 3 + y - 4;
      return (++todr_val);
}
