/*	$OpenBSD: apm_init.S,v 1.3 1997/07/29 16:48:39 flipk Exp $	*/

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
 * LP (Laptop Package)
 *
 * Copyright (C) 1994 by HOSOKAWA, Tatsumi <hosokawa@mt.cs.keio.ac.jp>
 *
 * This software may be used, modified, copied, and distributed, in
 * both source and binary form provided that the above copyright and
 * these terms are retained. Under no circumstances is the author 
 * responsible for the proper functioning of this software, nor does 
 * the author assume any responsibility for damages incurred with its 
 * use.
 *
 * Sep., 1994	Implemented on FreeBSD 1.1.5.1R (Toshiba AVS001WD)
 * Oct., 1994	NetBSD port (1.0 BETA 10/2) by ukai
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

	.file	"apm_init.S"

#include <machine/asm.h>
#define _LOCORE
#include <machine/biosvar.h>
#include <machine/apmvar.h>
#undef _LOCORE
#include <assym.h>
	
	.globl	_C_LABEL(BIOS_regs)
	.text
ENTRY(apm_init)
	pushl	%ebx

	/*
	 * APM installation check
	 */
	movb	$(APM_BIOS), %ah
	movb	$(APM_INSTCHECK), %al
	movl	$(PMDV_APMBIOS), %ebx
	DOINT(SYSTEM_BIOS)		/* call system BIOS */
	jnc	1f			/* if found, goto 1f */
	
	movl	$(APMINI_CANTFIND), %eax
	jmp	5f			/* can't find APM BIOS */
1:
	shll	$16, %eax
	orw	%cx, %ax
	rorl	$16, %eax
	movl	_C_LABEL(BIOS_regs)+BIOSR_BX, %ebx
	cmpb	$'P', %bh
	jne	1f
	cmpb	$'M', %bl
	je	2f

1:	movl	$(APMINI_BADVER), %eax
	jmp	5f			/* can't find APM BIOS */

2:	testl	$(APM_32BIT_SUPPORT), %ecx
	jnz	1f			/* supports 32bit connection? */

	movl	$(APMINI_NOT32BIT), %eax
	jmp	5f			/* don't support 32bit connection */
1:
	pushl	%eax
	/*
	 * APM Protected Mode 32-bit Interface Connect
	 */
	movb	$(APM_BIOS), %ah
	movb	$(APM_DISCONNECT), %al	/* just in case bootloader connected*/
	movl	$(PMDV_APMBIOS), %ebx
	DOINT(SYSTEM_BIOS)
	movb	$(APM_BIOS), %ah
	movb	$(APM_PROT32CONNECT), %al
	movl	$(PMDV_APMBIOS), %ebx
	DOINT(SYSTEM_BIOS)
	popl	%eax
	jnc	5f			/* if successed, go to 1f */
	movl	$(APMINI_CONNECTERR), %eax
5:	popl	%ebx
	ret
