/*	$OpenBSD: machdep.c,v 1.9 1997/08/13 03:23:39 mickey Exp $	*/

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
#include <machine/apmvar.h>
#undef APM_DISCONNECT /* XXX temp hack */
#include <machine/biosvar.h>
#include "debug.h"

struct apm_connect_info apminfo;

#ifdef DEBUG
#define CKPT(c)	(*(u_int16_t*)0xb8148 = 0x4700 + (c))
#else
#define CKPT(c) /* c */
#endif

#ifdef BOOT_APM
static u_int
apm_init()
{
	u_int detail;
	u_int8_t f;
	__asm __volatile(DOINT(0x15) "\n\t"
			 "setc %b0\n\t"
			 "shll $16, %%ecx\n\t"
			 "movzwl %%ax, %1\n\t"
			 "orl %%ecx, %1"
			 : "=d" (f), "=a" (detail)
			 : "1" (APM_INSTCHECK), "b" (PMDV_APMBIOS)
			 : "%ecx", "cc");
	if (f || BIOS_regs.biosr_bx != 0x504d /* "PM" */ ) {
#ifdef DEBUG
		printf("apm_init: %x, %x, %x\n", f, BIOS_regs.biosr_bx, detail);
#endif
		return 0;
	} else
		return detail;
}

static __inline int
apm_disconnect() {
	register u_int16_t rv;
	__asm __volatile(DOINT(0x15) "\n\t"
			 "setc %b0"
			 : "=a" (rv)
			 : "a" (APM_DISCONNECT), "b" (PMDV_APMBIOS)
			 : "%ecx", "%edx", "cc");
	return (rv & 0xff)? rv >> 8 : 0;
}

static __inline int
apm_connect(struct apm_connect_info *apminfo)
{
	register u_int16_t f;
	__asm __volatile (DOINT(0x15) "\n\t"
			  "setc %b0\n\t"
			  "movb %%ah, %h0\n\t"
			  "movzwl %%ax, %%eax\n\tshll $4, %1\n\t"
			  "movzwl %%cx, %%ecx\n\tshll $4, %2\n\t"
			  "movzwl %%dx, %%edx\n\tshll $4, %3\n\t"
			  : "=b" (f),
			    "=a" (apminfo->apm_code32_seg_base),
			    "=c" (apminfo->apm_code16_seg_base),
			    "=d" (apminfo->apm_data_seg_base)
			  : "a" (APM_PROT32CONNECT), "b" (PMDV_APMBIOS)
			  : "cc");
	apminfo->apm_entrypt         = BIOS_regs.biosr_bx;
#if 0
	apminfo->apm_code32_seg_len  = BIOS_regs.biosr_si & 0xffff;
	apminfo->apm_data_seg_len    = BIOS_regs.biosr_di & 0xffff;
#else
	apminfo->apm_code32_seg_len  = 0x10000;
	apminfo->apm_data_seg_len    = 0x10000;
#endif
	return (f & 0xff)? f >> 8 : 0;
}
#endif

void
machdep()
{
	/* here */    CKPT('0');
	gateA20(1);   CKPT('1');
	debug_init(); CKPT('2');
	/* call console init before doing any io */
	cninit();     CKPT('3');
#ifndef _TEST
	memprobe();   CKPT('4');
#endif

#ifdef BOOT_APM
	if ((apminfo.apm_detail = apm_init())) {

		printf("apm: ");
		apm_disconnect();
		if (apm_connect(&apminfo) != 0)
			printf("connect error\n");
#ifdef DEBUG
		printf("%x text=%x/%x[%x] data=%x[%x] @ %x",
		       apminfo.apm_detail,
		       apminfo.apm_code32_seg_base,
		       apminfo.apm_code16_seg_base,
		       apminfo.apm_code32_seg_len,
		       apminfo.apm_data_seg_base,
		       apminfo.apm_data_seg_len,
		       apminfo.apm_entrypt);
#else
		printf("present");
#endif
		putchar('\n');
	}
#endif
	CKPT('9');
}
