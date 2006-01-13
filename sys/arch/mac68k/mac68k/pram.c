/*	$OpenBSD: pram.c,v 1.9 2006/01/13 19:36:47 miod Exp $	*/
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
#include <sys/param.h>

#include <machine/viareg.h>

#include <mac68k/mac68k/pram.h>
#include <mac68k/dev/adbvar.h>

unsigned long
pram_readtime(void)
{
	return (getPramTime());
}

void
pram_settime(unsigned long time)
{
	return setPramTime(time);
}

extern int adbHardware;         /* from adb.c */

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
