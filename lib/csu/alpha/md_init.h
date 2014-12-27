/* $OpenBSD: md_init.h,v 1.3 2014/12/27 13:17:52 kettenis Exp $ */
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
	__asm (".section "#section", \"ax\"\n"	\
	"	jsr 	$26," #func "		\n" \
	"	ldgp	$29,0($26)		\n" \
	"	.previous")

#define MD_SECTION_PROLOGUE(sect, entry_pt)	\
	__asm (					\
	".section "#sect",\"ax\",@progbits	\n" \
	"	.globl " #entry_pt "		\n" \
	"	.type " #entry_pt ",@function	\n" \
	#entry_pt":				\n" \
	"	ldgp	$29, 0($27)		\n" \
	"	lda	$30, -16($30)		\n" \
	"	stq	$26, 0($30)		\n" \
	"	stq	$15, 8($30)		\n" \
	"	.align	5			\n" \
	"	/* fall thru */			\n" \
	"	.previous")


#define MD_SECTION_EPILOGUE(sect)		\
	__asm (					\
	".section "#sect",\"ax\",@progbits	\n" \
	"	ldq	$15, 8($30)		\n" \
	"	ldq	$26, 0($30)		\n" \
	"	lda	$30, 16($30)		\n" \
	"	ret				\n" \
	"	.previous")


#define	MD_CRT0_START				\
	__asm (					\
	"	.globl __start			\n" \
	"	.type __start@function		\n" \
	"__start = ___start")

#define	MD_RCRT0_START				\
	__asm (					\
	"	.globl __start			\n" \
	"	.type __start@function		\n" \
	"__start:				\n" \
	"	.set	noreorder		\n" \
	"	br	$27, L1			\n" \
	"L1:					\n" \
	"	ldgp	$gp, 0($27)		\n" \
	"	mov	$16, $9			\n" \
	"	br	$11, L2			\n" \
	"L2:	ldiq	$12, L2			\n" \
	"	subq	$11, $12, $11		\n" \
	"	mov	$11, $17		\n" \
	"	lda	$6, _DYNAMIC		\n" \
	"	addq	$11, $6, $16		\n" \
	"	bsr	$26, _reloc_alpha_got	\n" \
	"	lda	$sp, -80($sp)		\n" \
	"	mov	$9, $16			\n" \
	"	lda	$11, 0($sp)		\n" \
	"	mov	$11, $17		\n" \
	"	mov	0, $18			\n" \
	"	jsr	$26, _dl_boot_bind	\n" \
	"	ldgp	$gp, 0($26)		\n" \
	"	mov	$9, $16			\n" \
	"	mov	0, $17			\n" \
	"	jsr	$26, ___start		\n" \
	".globl _dl_exit			\n" \
	".type _dl_exit@function		\n" \
	"_dl_exit:				\n" \
	"	lda	$0, 1			\n" \
	"	callsys				\n" \
	"	ret				\n" \
	".globl _dl_printf			\n" \
	".type _dl_printf@function		\n" \
	"_dl_printf:				\n" \
	"	ret")

#define	MD_START		___start
#define	MD_START_ARGS		char **sp, void (*cleanup)(void)
#define	MD_START_SETUP				\
	char **argv, **envp;			\
	long argc;				\
						\
	argc = *(long *)sp;			\
	argv = sp + 1;				\
	environ = envp = sp + 2 + argc;	/* 2: argc + NULL ending argv */
