/*	$OpenBSD: pram.c,v 1.8 2006/01/04 20:39:05 miod Exp $	*/
/*	$NetBSD: pram.c,v 1.11 1996/10/21 05:42:29 scottr Exp $	*/

/*-
 * Copyright (C) 1993	Allen K. Briggs, Chris P. Caputo,
 *			Michael L. Finch, Bradley A. Grantham, and
 *			Lawrence A. Kesteloot
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
 *	This product includes software developed by the Alice Group.
 * 4. The names of the Alice Group or any of its members may not be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE ALICE GROUP ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE ALICE GROUP BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


/* #include "stand.h"  */
#include <sys/types.h>
#ifdef DEBUG
#include <sys/systm.h>
#endif
#include <sys/param.h>

#include <machine/viareg.h>

#include <mac68k/mac68k/pram.h>
#ifdef MRG_ADB
#include <mac68k/mac68k/macrom.h>
#else
#include <mac68k/dev/adbvar.h>
#endif

#if DEBUG
static char *convtime(unsigned long t)
{
  static long daypmon[] = { 31,28,31,30,31,30,31,31,30,31,30,31 };
  static char *monstr[] = {"January","February","March","April","May","June",
    "July","August","September","October","November","December" };
  static char s[200];
  long year,month,day,hour,minute,seconds,i,dayperyear;

  year=1904;
  month=0;  /* Jan */
  day=1;
  hour=0;
  minute=0;
  seconds=0;

  if(t == 0xffffffff)
     return("<time value is -1>");

  while (t > 0)
  {
    if (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0))
    {
      dayperyear=366;
      daypmon[1]=29;
    }
    else
    {
      dayperyear=365;
      daypmon[1]=28;
    }
    i=dayperyear*60*60*24;
    if (t >= i)
    {
      t-=i;
      year++;
      continue;
    }
    i=daypmon[month]*60*60*24;
    if (t >= i)
    {
      t-=i;
      month++;
      continue;
    }
    i=60*60*24;
    if (t >= i)
    {
      t-=i;
      day++;
      continue;
    }
    i=60*60;
    if (t >= i)
    {
      t-=i;
      hour++;
      continue;
    }
    i=60;
    if (t >= i)
    {
      t-=i;
      minute++;
      continue;
    }
    seconds=t;
    t=0;
  }

  snprintf(s, sizeof s, "%s %ld, %ld   %ld:%ld:%ld",
    monstr[month],day,year,hour,minute,seconds);

  return s;
}
#endif

unsigned long
pram_readtime(void)
{
   unsigned long	timedata;

   if (0 == jClkNoMem)
	timedata = 0;	/* cause comparision of MacOS boottime */
			/* and PRAM time to fail */
   else
	timedata = getPramTime();
#if DEBUG
   printf("time read from PRAM: 0x%lx\n", timedata);
   printf("Date and time: %s\n",convtime(timedata));
#endif

   return(timedata);
}

void
pram_settime(unsigned long time)
{
   if (0 == jClkNoMem)
	return;
   else
	return setPramTime(time);
}

#ifndef MRG_ADB         /* These routines are defined here only
                         * when the MRG_ADB method for accessing
                         * the ADB/PRAM/RTC isn't enabled. */

extern int adbHardware;         /* from newadb.c */

/*
 * getPramTime
 * This function can be called regrardless of the machine
 * type. It calls the correct hardware-specific code.
 * (It's sort of redundant with the above, but it was
 * added later.)
 */
unsigned long
getPramTime(void)
{
        unsigned long time;

        switch (adbHardware) {
        case ADB_HW_II:         /* access PRAM via VIA interface */
                time=(long)getPramTimeII();
                return time;

        case ADB_HW_IISI:       /* access PRAM via pseudo-adb functions */
	case ADB_HW_CUDA:
                if (0 != adb_read_date_time(&time))
                        return 0;
                else
                        return time;

        case ADB_HW_PB:         /* don't know how to access this yet */
                return 0;

        case ADB_HW_UNKNOWN:
        default:
                return 0;
        }
}

/*
 * setPramTime
 * This function can be called regrardless of the machine
 * type. It calls the correct hardware-specific code.
 * (It's sort of redundant with the above, but it was
 * added later.)
 */
void
setPramTime(unsigned long time)
{
        switch (adbHardware) {
        case ADB_HW_II:         /* access PRAM via ADB interface */
                setPramTimeII(time);
                return;

        case ADB_HW_IISI:       /* access PRAM via pseudo-adb functions */
	case ADB_HW_CUDA:
                adb_set_date_time(time);
                return;

        case ADB_HW_PB:         /* don't know how to access this yet */
                return;

        case ADB_HW_UNKNOWN:
                return;
        }

}

#endif  /* ifndef MRG_ADB */
