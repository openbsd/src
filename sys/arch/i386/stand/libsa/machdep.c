/*	$OpenBSD: machdep.c,v 1.8 1997/08/12 21:51:30 mickey Exp $	*/

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

#include "libsa.h"
#include <machine/biosvar.h>
#include <machine/apmvar.h>
#include "debug.h"

struct apm_connect_info apminfo;

void
machdep()
{
#ifdef DEBUG
	*(u_int16_t*)0xb8148 = 0x4730;
#endif
	gateA20(1);
#ifdef DEBUG
	*(u_int16_t*)0xb8148 = 0x4731;
#endif
	debug_init();
#ifdef DEBUG
	*(u_int16_t*)0xb8148 = 0x4732;
#endif
	cninit();	/* call console init before doing any io */
#ifdef DEBUG
	*(u_int16_t*)0xb8148 = 0x4733;
#endif
#ifndef _TEST
	memprobe();
#endif
#ifdef DEBUG
	*(u_int16_t*)0xb8148 = 0x4f34;
#endif
#ifdef BOOT_APM
	printf("apm_init: ");
	switch(apminfo.apm_detail = apm_init()) {
	case APMINI_CANTFIND:
		printf("not supported");
		break;

	case APMINI_NOT32BIT:
		printf("no 32 bit interface");
		break;

	case APMINI_CONNECTERR:
		printf("connect error");
		break;

	case APMINI_BADVER:
		printf("bad version");
		break;

	default:
		/* valid: detail, dx, bx */
		apminfo.apm_code32_seg_base = (BIOS_regs.biosr_ax & 0xffff)<< 4;
		apminfo.apm_code16_seg_base = (BIOS_regs.biosr_cx & 0xffff)<< 4;
		apminfo.apm_data_seg_base   = (BIOS_regs.biosr_dx & 0xffff)<< 4;
#if 0
		apminfo.apm_code32_seg_len  = BIOS_regs.biosr_si & 0xffff;
		apminfo.apm_data_seg_len    = BIOS_regs.biosr_di & 0xffff;
#else
		apminfo.apm_code32_seg_len  = 0x10000;
		apminfo.apm_data_seg_len    = 0x10000;
#endif
		apminfo.apm_entrypt         = BIOS_regs.biosr_bx;
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
		printf("APM detected");
#endif
	}
	putchar('\n');
#endif
}
