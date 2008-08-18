/*	$OpenBSD: cvax.c,v 1.1 2008/08/18 23:07:26 miod Exp $	*/
/*	$NetBSD: ka650.c,v 1.25 2001/04/27 15:02:37 ragge Exp $	*/
/*
 * Copyright (c) 1988 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Mt. Xinu.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)ka650.c	7.7 (Berkeley) 12/16/90
 */

/*
 * CVAX-specific code.
 */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <uvm/uvm_extern.h>

#include <machine/cvax.h>
#include <machine/clock.h>
#include <machine/cpu.h>
#include <machine/psl.h>
#include <machine/mtpr.h>
#include <machine/sid.h>

struct	cvax_ssc *cvax_ssc_ptr;

/*
 * Machine Check descriptions
 */
const char *cvax_mcheck[] = {
	NULL,			/* 00 */
	"FPA proto err",	/* 01 */
	"FPA resv inst",	/* 02 */
	"FPA Ill Stat 2",	/* 03 */
	"FPA Ill Stat 1",	/* 04 */
	"PTE in P0, TB miss",	/* 05 */
	"PTE in P1, TB miss",	/* 06 */
	"PTE in P0, Mod",	/* 07 */
	"PTE in P1, Mod",	/* 08 */
	"Illegal intr IPL",	/* 09 */
	"MOVC state error",	/* 0a */

	"bus read error",	/* 80 */
	"SCB read error",	/* 81 */
	"bus write error",	/* 82 */
	"PCB write error"	/* 83 */
};

const char *
cvax_mchk_descr(int summary)
{
	if ((unsigned int)summary < 11)
		return cvax_mcheck[summary];

	if (summary >= 0x80 && summary <= 0x83)
		return cvax_mcheck[summary - 0x80 + 11];

	return NULL;
}

/*
 * CVAX Mailbox routines
 */

void
cvax_halt()
{
	cvax_ssc_ptr->ssc_cpmbx = CPMB_CVAX_DOTHIS | CPMB_CVAX_HALT;
	asm("halt");
}

void
cvax_reboot(arg)
	int arg;
{
	cvax_ssc_ptr->ssc_cpmbx = CPMB_CVAX_DOTHIS | CPMB_CVAX_REBOOT;
}
