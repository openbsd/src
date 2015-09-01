/* $OpenBSD: md_init.h,v 1.5 2015/09/01 05:40:06 guenther Exp $ */

/*-
 * Copyright (c) 2001 Ross Harvey
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
 *      This product includes software developed by the NetBSD
 *      Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef __PIC__
	/* This nastyness derived from gcc3 output */
#define MD_SECT_CALL_FUNC(section, func) \
	__asm (".section "#section", \"ax\"		\n" \
	"	bl " #func "(PLT)			\n" \
	"	.previous")
#else
#define MD_SECT_CALL_FUNC(section, func) \
	__asm (".section "#section", \"ax\"	\n" \
	"	adr r0, 1f			\n" \
	"	ldr r0, [r0]			\n" \
	"	adr lr, 2f			\n" \
	"	mov pc,	r0			\n" \
	"1:	.word " #func "			\n" \
	"2:					\n" \
	"	.previous")
#endif

#define MD_SECTION_PROLOGUE(sect, entry_pt)	\
	__asm (					\
	".section "#sect",\"ax\",%progbits	\n" \
	"	.globl " #entry_pt "		\n" \
	"	.type " #entry_pt ",%function	\n" \
	"	.align 4			\n" \
	#entry_pt":				\n" \
	"	mov ip, sp			\n" \
	"	stmfd sp!, {fp, ip, lr, pc}	\n" \
	"	sub fp, ip, #4			\n" \
	"	/* fall thru */			\n" \
	"	.previous")


#define MD_SECTION_EPILOGUE(sect)		\
	__asm (					\
	".section "#sect",\"ax\",%progbits	\n" \
	"	ldmea	fp, {fp, sp, pc}	\n" \
	"	.previous")


#define	MD_CRT0_START				\
	__asm(					\
	".text					\n" \
	"	.align	0			\n" \
	"	.globl	_start			\n" \
	"	.globl	__start			\n" \
	"_start:				\n" \
	"__start:				\n" \
	"	mov	r3, r0	/* cleanup */	\n" \
	"/* Get argc/argv/envp from stack */	\n" \
	"	ldr	r0, [sp, #0x0000]	\n" \
	"	add	r1, sp, #0x0004		\n" \
	"	add	r2, r1, r0, lsl #2	\n" \
	"	add	r2, r2, #0x0004		\n" \
	"					\n" \
	"/*					\n" \
	" * Ensure the stack is properly	\n" \
	" * aligned before calling C code.	\n" \
	" */					\n" \
	/* #if 1 */				\
	"	bic	sp, sp, #7" /*__STRING(STACKALIGNBYTES)*/ "	\n" \
	/* #endif */				\
	"	sub	sp, sp, #8		\n" \
	"	str	r5, [sp, #4]		\n" \
	"	str	r4, [sp, #0]		\n" \
	"					\n" \
	"	b	___start		\n" \
	".previous");

#include <sys/syscall.h>
#define	MD_DISABLE_KBIND						\
	do {								\
		register long syscall_num __asm("r12") = SYS_kbind;	\
		register void *arg1 __asm("r0") = NULL;			\
		__asm volatile("swi 0" : "+r" (arg1)			\
		    : "r" (syscall_num) : "r1", "cc");			\
	} while (0)
