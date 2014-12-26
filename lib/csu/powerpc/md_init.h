/* $OpenBSD: md_init.h,v 1.3 2014/12/26 13:52:01 kurt Exp $ */

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
	"	bl " #func "\n"		\
	"	.previous")

#define MD_SECTION_PROLOGUE(sect, entry_pt)	\
	__asm (					\
	".section "#sect",\"ax\",@progbits	\n" \
	"	.globl " #entry_pt "		\n" \
	"	.type " #entry_pt ",@function	\n" \
	"	.align 4			\n" \
	#entry_pt":				\n" \
	"	stwu	%r1,-16(%r1)		\n" \
	"	mflr	%r0			\n" \
	"	stw	%r0,12(%r1)		\n" \
	"	/* fall thru */			\n" \
	"	.previous")


#define MD_SECTION_EPILOGUE(sect)		\
	__asm (					\
	".section "#sect",\"ax\",@progbits	\n" \
	"	lwz	%r0,12(%r1)		\n" \
	"	mtlr	%r0			\n" \
	"	addi	%r1,%r1,16		\n" \
	"	blr				\n" \
	"	.previous")

#include <sys/syscall.h>	/* for SYS_mprotect */

#define STR(x) __STRING(x)
#define	MD_CRT0_START							\
__asm(									\
"	.text								\n" \
"	.section	\".text\"					\n" \
"	.align 2							\n" \
"	.size	__got_start, 0						\n" \
" 	.type	__got_start, @object					\n" \
"	.size	__got_end, 0						\n" \
" 	.type	__got_end, @object					\n" \
"	.weak	__got_start						\n" \
"	.weak	__got_end						\n" \
"	.globl	_start							\n" \
"	.type	_start, @function					\n" \
"	.globl	__start							\n" \
"	.type	__start, @function					\n" \
"_start:								\n" \
"__start:								\n" \
"	# move argument registers to saved registers for startup flush	\n" \
"	# ...except r6 (auxv) as ___start() doesn't need it		\n" \
"	mr %r25, %r3							\n" \
"	mr %r24, %r4							\n" \
"	mr %r23, %r5							\n" \
"	mr %r22, %r7							\n" \
"	mflr	%r27	/* save off old link register */		\n" \
"	bl	1f							\n" \
"	# this instruction never gets executed but can be used		\n" \
"	# to find the virtual address where the page is loaded.		\n" \
"	bl _GLOBAL_OFFSET_TABLE_@local-4				\n" \
"1:									\n" \
"	mflr	%r6		# this stores where we are (+4)		\n" \
"	lwz	%r18, 0(%r6)	# load the instruction at offset_sym	\n" \
"				# it contains an offset to the location	\n" \
"				# of the GOT.				\n" \
"									\n" \
"	rlwinm %r18,%r18,0,8,30 # mask off the offset portion of the instr. \n" \
"									\n" \
"	/*								\n" \
"	 * these adds effectively calculate the value the		\n" \
"	 * bl _GLOBAL_OFFSET_TABLE_@local-4				\n" \
"	 * operation that would be below would calculate.		\n" \
"	 */								\n" \
"	add	%r28, %r18, %r6						\n" \
"									\n" \
"	addi	%r3,%r28,4		# calculate the actual got addr \n" \
"	lwz	%r0,__got_start@got(%r3)				\n" \
"	cmpwi	%r0,0							\n" \
"	beq	4f							\n" \
"	cmpw	%r0,%r28						\n" \
"	bne	4f							\n" \
"	lwz	%r4,__got_end@got(%r3)					\n" \
"	cmpwi	%r4,0							\n" \
"	beq	2f							\n" \
"									\n" \
"	sub	%r4, %r4, %r0						\n" \
"	b 3f								\n" \
"2:									\n" \
"	li	%r4, 4							\n" \
"3:									\n" \
"									\n" \
"	/* mprotect GOT to eliminate W+X regions in static binaries */	\n" \
"	li	%r0, " STR(SYS_mprotect) "				\n" \
"	mr	%r3, %r28						\n" \
"	li	%r5, 5	/* (PROT_READ|PROT_EXEC) */			\n" \
"	sc								\n" \
"									\n" \
"4:									\n" \
"	li	%r0, 0							\n" \
"	# flush the blrl instruction out of the data cache		\n" \
"	dcbf	%r6, %r18						\n" \
"	sync								\n" \
"	isync								\n" \
"	# make certain that the got table addr is not in the icache	\n" \
"	icbi	%r6, %r18						\n" \
"	sync								\n" \
"	isync								\n" \
"	mtlr %r27							\n" \
"	# move argument registers back from saved registers		\n" \
"	# putting cleanup in r6 instead of r7				\n" \
"	mr %r3, %r25							\n" \
"	mr %r4, %r24							\n" \
"	mr %r5, %r23							\n" \
"	mr %r6, %r22							\n" \
"	b ___start							\n" \
)

#define	MD_RCRT0_START							\
__asm(									\
"	.text								\n" \
"	.section	\".text\"					\n" \
"	.align 2							\n" \
"	.globl	_start							\n" \
"	.type	_start, @function					\n" \
"	.globl	__start							\n" \
"	.type	__start, @function					\n" \
"_start:								\n" \
"__start:								\n" \
"	mr	%r19, %r1		# save stack in r19		\n" \
"	stwu	1, (-16 -((9+3)*4))(%r1) # allocate dl_data		\n" \
"									\n" \
"	# move argument registers to saved registers for startup flush	\n" \
"	mr %r20, %r3			# argc				\n" \
"	mr %r21, %r4			# argv				\n" \
"	mr %r22, %r5			# envp				\n" \
"	mflr	%r27	/* save off old link register */		\n" \
"	stw	%r27, 4(%r19)		# save in normal location	\n" \
"	bl	1f							\n" \
"	# this instruction never gets executed but can be used		\n" \
"	# to find the virtual address where the page is loaded.		\n" \
"	bl _GLOBAL_OFFSET_TABLE_@local-4				\n" \
"	bl _DYNAMIC@local						\n" \
"1:									\n" \
"	mflr	%r6		# this stores where we are (+4)		\n" \
"	lwz	%r18, 0(%r6)	# load the instruction at offset_sym	\n" \
"				# it contains an offset to the location	\n" \
"				# of the GOT.				\n" \
"									\n" \
"	rlwinm %r18,%r18,0,8,30 # mask off offset portion of the instr. \n" \
"									\n" \
"	/*								\n" \
"	 * these adds effectively calculate the value the		\n" \
"	 * bl _GLOBAL_OFFSET_TABLE_@local-4				\n" \
"	 * operation that would be below would calculate.		\n" \
"	 */								\n" \
"	add	%r28, %r18, %r6						\n" \
"									\n" \
"	/* mprotect GOT-4 for correct execution of blrl instruction */	\n" \
"	li	%r0, " STR(SYS_mprotect) "				\n" \
"	mr	%r3, %r28						\n" \
"	li	%r4, 4							\n" \
"	li	%r5, 7	/* (PROT_READ|PROT_WRITE|PROT_EXEC) */		\n" \
"	sc								\n" \
"									\n" \
"	li	%r0, 0							\n" \
"	# flush the blrl instruction out of the data cache		\n" \
"	dcbf	%r6, %r18						\n" \
"	sync								\n" \
"	isync								\n" \
"	# make certain that the got table addr is not in the icache	\n" \
"	icbi	%r6, %r18						\n" \
"	sync								\n" \
"	isync								\n" \
"									\n" \
"	/* This calculates the address of _DYNAMIC the same way		\n" \
"	 * that the GLOBAL_OFFSET_TABLE was calculated.			\n" \
"	 */								\n" \
"	lwz	%r18, 4(%r6)						\n" \
"	rlwinm	%r18,%r18,0,8,30 # mask off offset portion of the instr. \n" \
"	add	%r8, %r18, %r6	# address of _DYNAMIC (arg6 for _dl_boot) \n" \
"	addi	%r18, %r8, 4	# correction.				\n" \
"	lwz	%r4, 4(%r28)	# load addr of _DYNAMIC according to got. \n" \
"	sub	%r4, %r18, %r4	# determine load offset			\n" \
"									\n" \
"	subi	%r3, %r21, 4	# Get stack pointer (arg0 for _dl_boot). \n" \
"	addi	%r4, %r1, 8	# dl_data				\n" \
"	mr	%r5, %r18	# dynamicp				\n" \
"									\n" \
"	bl	_dl_boot_bind@local					\n" \
"									\n" \
"	mtlr %r27							\n" \
"	# move argument registers back from saved registers		\n" \
"	mr %r3, %r20							\n" \
"	mr %r4, %r21							\n" \
"	mr %r5, %r22							\n" \
"	li %r6, 0							\n" \
"	b ___start							\n" \
"									\n" \
"	.text								\n" \
"	.align 2							\n" \
"	.globl	_dl_exit						\n" \
"	.type	_dl_exit, @function					\n" \
"_dl_exit:								\n" \
"	li	%r0, " STR(SYS_exit) "					\n" \
"	sc								\n" \
"	blr								\n" \
"									\n" \
"	.text								\n" \
"	.align 2							\n" \
"	.globl	_dl_printf						\n" \
"	.type	_dl_printf, @function					\n" \
"_dl_printf:								\n" \
"	blr								\n" \
)
