/*	$OpenBSD: profile.h,v 1.1 2004/02/01 05:09:49 drahn Exp $	*/
/*	$NetBSD: profile.h,v 1.5 2002/03/24 15:49:40 bjh21 Exp $	*/

/*
 * Copyright (c) 2001 Ben Harris
 * Copyright (c) 1995-1996 Mark Brinicombe
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
 *	This product includes software developed by Mark Brinicombe.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define	_MCOUNT_DECL void _mcount

/*
 * Cannot implement mcount in C as GCC will trash the ip register when it
 * pushes a trapframe. Pity we cannot insert assembly before the function
 * prologue.
 */

#ifdef __ELF__
#define MCOUNT_ASM_NAME "__mcount"
#ifdef PIC
#define	PLTSYM		"(PLT)"
#endif
#else
#define MCOUNT_ASM_NAME "mcount"
#endif

#ifndef PLTSYM
#define	PLTSYM
#endif

#define	MCOUNT								\
	__asm__(".text");						\
	__asm__(".align	0");						\
	__asm__(".type	" MCOUNT_ASM_NAME ",%function");		\
	__asm__(".global	" MCOUNT_ASM_NAME);			\
	__asm__(MCOUNT_ASM_NAME ":");					\
	/*								\
	 * Preserve registers that are trashed during mcount		\
	 */								\
	__asm__("stmfd	sp!, {r0-r3, ip, lr}");				\
	/* Check what mode we're in.  EQ => 32, NE => 26 */		\
	__asm__("teq	r0, r0");					\
	__asm__("teq	pc, r15");					\
	/*								\
	 * find the return address for mcount,				\
	 * and the return address for mcount's caller.			\
	 *								\
	 * frompcindex = pc pushed by call into self.			\
	 */								\
	__asm__("moveq	r0, ip");					\
	__asm__("bicne	r0, ip, #0xfc000003");	       			\
	/*								\
	 * selfpc = pc pushed by mcount call				\
	 */								\
	__asm__("moveq	r1, lr");					\
	__asm__("bicne	r1, lr, #0xfc000003");				\
	/*								\
	 * Call the real mcount code					\
	 */								\
	__asm__("bl	" __STRING(_mcount) PLTSYM);		\
	/*								\
	 * Restore registers that were trashed during mcount		\
	 */								\
	__asm__("ldmfd	sp!, {r0-r3, lr, pc}");

#ifdef _KERNEL
#ifdef __PROG26
extern int int_off_save(void);
extern void int_restore(int);
#define	MCOUNT_ENTER	(s = int_off_save())
#define	MCOUNT_EXIT	int_restore(s)
#else
#include <arm/cpufunc.h>
/*
 * splhigh() and splx() are heavyweight, and call mcount().  Therefore
 * we disabled interrupts (IRQ, but not FIQ) directly on the CPU.
 *
 * We're lucky that the CPSR and 's' both happen to be 'int's.
 */
#define	MCOUNT_ENTER	s = __set_cpsr_c(0x0080, 0x0080);	/* kill IRQ */
#define	MCOUNT_EXIT	__set_cpsr_c(0xffffffff, s);	/* restore old value */
#endif /* !acorn26 */
#endif /* _KERNEL */
