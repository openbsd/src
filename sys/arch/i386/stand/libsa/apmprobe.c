/*	$OpenBSD: apmprobe.c,v 1.5 1998/04/18 07:39:40 deraadt Exp $	*/

/*
 * Copyright (c) 1997 Michael Shalayeff
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
 *	This product includes software developed by Michael Shalayeff.
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
/*
 * APM derived from: apm_init.S, LP (Laptop Package)
 * wich contained this:
 * Copyright (C) 1994 by HOSOKAWA, Tatsumi <hosokawa@mt.cs.keio.ac.jp>
 *
 */
/*
 * If you want to know the specification of APM BIOS, see the following
 * documentations,
 *
 * [1] Intel Corporation and Microsoft Corporation, "Advanced Power 
 *     Management, The Next Generation, Version 1.0", Feb.,1992.
 *
 * [2] Intel Corporation and Microsoft Corporation, "Advanced Power
 *     Management (APM) BIOS Interface Specification Revision 1.1",
 *     Sep.,1993, Intel Order Number: 241704-001, Microsoft Part
 *     Number: 781-110-X01
 *
 * or contact
 *
 * APM Support Desk (Intel Corporation, US)
 *   TEL: (800)628-8686 
 *   FAX: (916)356-6100.
 */

#include "libsa.h"
#include <stand/boot/bootarg.h>
#include <machine/apmvar.h>
#include <machine/biosvar.h>
#include "debug.h"

extern int debug;

static __inline u_int
apm_check()
{
	register u_int detail;
	register u_int8_t f;
	__asm __volatile(DOINT(0x15) "\n\t"
			 "setc %b1\n\t"
			 "movzwl %%ax, %0\n\t"
			 "shll $16, %%ecx\n\t"
			 "orl %%ecx, %0"
			 : "=a" (detail), "=b" (f)
			 : "0" (APM_INSTCHECK), "1" (APM_DEV_APM_BIOS)
			 : "%ecx", "cc");
	if (f || BIOS_regs.biosr_bx != 0x504d /* "PM" */ ) {
#ifdef DEBUG
		if (debug)
			printf("apm_check: %x, %x, %x\n",
			    f, BIOS_regs.biosr_bx, detail);
#endif
		return 0;
	} else
		return detail;
}

static __inline int
apm_disconnect()
{
	register u_int16_t rv;
	__asm __volatile(DOINT(0x15) "\n\t"
			 "setc %b0"
			 : "=a" (rv)
			 : "0" (APM_DISCONNECT), "b" (APM_DEV_APM_BIOS)
			 : "%ecx", "%edx", "cc");
	return (rv & 0xff)? rv >> 8 : 0;
}

static __inline int
apm_connect(ai)
	bios_apminfo_t *ai;
{
	register u_int16_t f;
	__asm __volatile (DOINT(0x15) "\n\t"
			  "setc %b1\n\t"
			  "movb %%ah, %h1\n\t"
			  "movzwl %%ax, %%eax\n\tshll $4, %0\n\t"
			  "movzwl %%cx, %%ecx\n\tshll $4, %2\n\t"
			  "movzwl %%dx, %%edx\n\tshll $4, %3\n\t"
			  : "=a" (ai->apm_code32_base),
			    "=b" (f),
			    "=c" (ai->apm_code16_base),
			    "=d" (ai->apm_data_base)
			  : "0" (APM_PROT32_CONNECT), "1" (APM_DEV_APM_BIOS)
			  : "cc");
	ai->apm_entry    = BIOS_regs.biosr_bx;
#if 0
	ai->apm_code_len = BIOS_regs.biosr_si & 0xffff;
	ai->apm_data_len = BIOS_regs.biosr_di & 0xffff;
#else
	ai->apm_code_len = 0x10000;
	ai->apm_data_len = 0x10000;
#endif
	return (f & 0xff)? f >> 8 : 0;
}

void
apmprobe()
{
	bios_apminfo_t ai;

	if ((ai.apm_detail = apm_check())) {

		apm_disconnect();
		if (apm_connect(&ai) != 0)
			printf(": connect error\n");
#ifdef DEBUG
		if (debug)
			printf(": %x text=%x/%x[%x] data=%x[%x] @ %x",
			       ai.apm_detail, ai.apm_code32_base,
			       ai.apm_code16_base, ai.apm_code_len,
			       ai.apm_data_base, ai.apm_data_len,
			       ai.apm_entry);
		else
			printf(" apm");
#else
		printf(" apm");
#endif
		addbootarg(BOOTARG_APMINFO, sizeof(ai), &ai);
	}
}

