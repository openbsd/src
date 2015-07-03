/* $OpenBSD: md_init.h,v 1.3 2015/07/03 11:17:25 miod Exp $ */

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

#define MD_SECT_CALL_FUNC(section, func) \
	__asm (".section "#section", \"ax\"	\n" \
	"	call " #func ", 0		\n" \
	"	 nop				\n" \
	"	.previous")

#define MD_SECTION_PROLOGUE(sect, entry_pt)	\
	__asm (					\
	".section "#sect",\"ax\",@progbits	\n" \
	"	.globl " #entry_pt "		\n" \
	"	.type " #entry_pt ",@function	\n" \
	#entry_pt":				\n" \
	"	save	%sp, -96, %sp		\n" \
	"	.align 4			\n" \
	"	.previous")


#define MD_SECTION_EPILOGUE(sect)		\
	__asm (					\
	".section "#sect",\"ax\",@progbits	\n" \
	"	ret				\n" \
	"	 restore			\n" \
	"	.previous")


#define	MD_CRT0_START				\
	__asm(					\
	".text					\n" \
	"	.align	4			\n" \
	"	.global	__start			\n" \
	"	.global	_start			\n" \
	"__start:				\n" \
	"_start:				\n" \
	"	mov	0, %fp			\n" \
	"	ld	[%sp + 64], %o0	! get argc\n" \
	"	add	%sp, 68, %o1	! get argv\n" \
	"	sll	%o0, 2,	%o2		\n" \
	"	add	%o2, 4,	%o2	! envp = argv + (argc << 2) + 4\n" \
	"	add	%o1, %o2, %o2		\n" \
	"	andn	%sp, 7,	%sp	! align	\n" \
	"	sub	%sp, 24, %sp	! expand to standard frame size\n" \
	"	call	___start		\n" \
	"	 mov	%g1, %o3		\n" \
	"	.previous")


#define	MD_RCRT0_START							\
	__asm(								\
	".text								\n" \
	"	.align	4						\n" \
	"	.global	__start						\n" \
	"	.global	_start						\n" \
	"__start:							\n" \
	"_start:							\n" \
	"	mov	0, %fp						\n" \
									\
	"	sub	%sp, 96, %sp					\n" \
	"	add	%sp, 96, %l3					\n" \
	"	add	%l3, 64, %o0					\n" \
	"	mov	%o0, %l0					\n" \
	"	call	0f						\n" \
	"	 nop							\n" \
	"	call	_DYNAMIC + 8					\n" \
	"0:	ld	[%o7 + 8], %o2					\n" \
	"	sll	%o2, 2, %o2					\n" \
	"	sra	%o2, 0, %o2					\n" \
	"	add	%o2, %o7, %o2					\n" \
	"	call	_dl_boot_bind					\n" \
	"	 mov	%l3, %o1					\n" \
	"	add	%sp, 96, %sp					\n" \
									\
	"	ld	[%sp + 64], %o0	! get argc			\n" \
	"	add	%sp, 68, %o1	! get argv			\n" \
	"	sll	%o0, 2,	%o2					\n" \
	"	add	%o2, 4,	%o2	! envp = argv + (argc << 2) + 4	\n" \
	"	add	%o1, %o2, %o2					\n" \
	"	andn	%sp, 7,	%sp	! align				\n" \
	"	sub	%sp, 24, %sp	! expand to standard frame size	\n" \
	"	call	___start					\n" \
	"	 clr	%o3						\n" \
									\
	"	.global	_dl_mul_fixup					\n" \
	"	.type	_dl_mul_fixup,@function				\n" \
	"_dl_mul_fixup:							\n" \
	"	retl							\n" \
	"	 nop							\n" \
									\
	"	.global	_dl_printf					\n" \
	"	.type	_dl_printf,@function				\n" \
	"_dl_printf:							\n" \
	"	retl							\n" \
	"	 nop							\n" \
									\
	"	.global _dl_exit					\n" \
	"	.type	_dl_exit,@function				\n" \
	"_dl_exit:							\n" \
	"	mov	0x401, %g1					\n" \
	"	add	%o7, 8, %g2					\n" \
	"	ta	0						\n" \
	"	retl							\n" \
	"	 neg	%o0						\n" \
									\
	"	.previous")
