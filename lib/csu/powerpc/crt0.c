/*	$OpenBSD: crt0.c,v 1.5 2009/06/16 16:37:14 drahn Exp $	*/
/*	$NetBSD: crt0.c,v 1.1 1996/09/12 16:59:02 cgd Exp $	*/
/*
 * Copyright (c) 1995 Christopher G. Demetriou
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
 *      This product includes software developed by Christopher G. Demetriou
 *	for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
#include <sys/param.h>
#include <stdlib.h>
#include <sys/syscall.h>

char **environ;
char * __progname = "";

char __progname_storage[NAME_MAX+1];

#ifdef MCRT0
extern void     monstartup(u_long, u_long);
extern void     _mcleanup(void);
extern unsigned char _etext, _eprol;
#endif /* MCRT0 */

static inline char * _strrchr(const char *p, char ch);

#define STR(x) __STRING(x) 
__asm(
"	.text								\n"
"	.section	\".text\"					\n"
"	.align 2							\n"
"	.size	__got_start, 0						\n"
" 	.type	__got_start, @object					\n"
"	.size	__got_end, 0						\n"
" 	.type	__got_end, @object					\n"
"	.weak	__got_start						\n"
"	.weak	__got_end						\n"
"	.globl	_start							\n"
"	.type	_start, @function					\n"
"	.globl	__start							\n"
"	.type	__start, @function					\n"
"_start:								\n"
"__start:								\n"
"	# move argument registers to saved registers for startup flush	\n"
"	mr %r26, %r3							\n"
"	mr %r25, %r4							\n"
"	mr %r24, %r5							\n"
"	mr %r23, %r6							\n"
"	mr %r22, %r7							\n"
"	mflr	%r27	/* save off old link register */		\n"
"	bl	1f							\n"
"	# this instruction never gets executed but can be used		\n"
"	# to find the virtual address where the page is loaded.		\n"
"	bl _GLOBAL_OFFSET_TABLE_@local-4				\n"
"1:									\n"
"	mflr	%r6		# this stores where we are (+4)		\n"
"	lwz	%r18, 0(%r6)	# load the instruction at offset_sym	\n"
"				# it contains an offset to the location	\n"
"				# of the GOT.				\n"
"									\n"
"	rlwinm %r18,%r18,0,8,30 # mask off the offset portion of the instr. \n"
"									\n"
"	/*								\n"
"	 * these adds effectively calculate the value the		\n"
"	 * bl _GLOBAL_OFFSET_TABLE_@local-4				\n"
"	 * operation that would be below would calulate.		\n"
"	 */								\n"
"	add	%r28, %r18, %r6						\n"
"									\n"
"	addi	%r3,%r28,4		# calculate the actual got addr \n"
"	lwz	%r0,__got_start@got(%r3)				\n"
"	cmpwi	%r0,0							\n"
"	beq	4f							\n"
"	cmpw	%r0,%r28						\n"
"	bne	4f							\n"
"	lwz	%r4,__got_end@got(%r3)					\n"
"	cmpwi	%r4,0							\n"
"	beq	2f							\n"
"									\n"
"	sub	%r4, %r4, %r0						\n"
"	b 3f								\n"
"2:									\n"
"	li	%r4, 4							\n"
"3:									\n"
"									\n"
"	/* mprotect GOT to eliminate W+X regions in static binaries */	\n"
"	li	%r0, " STR(SYS_mprotect) "				\n"
"	mr	%r3, %r28						\n"
"	li	%r5, 5	/* (PROT_READ|PROT_EXEC) */			\n"
"	sc								\n"
"									\n"
"4:									\n"
"	li	%r0, 0							\n"
"	# flush the blrl instruction out of the data cache		\n"
"	dcbf	%r6, %r18						\n"
"	sync								\n"
"	isync								\n"
"	# make certain that the got table addr is not in the icache	\n"
"	icbi	%r6, %r18						\n"
"	sync								\n"
"	isync								\n"
"	mtlr %r27							\n"
"	# move argument registers back from saved registers		\n"
"	mr %r3, %r26							\n"
"	mr %r4, %r25							\n"
"	mr %r5, %r24							\n"
"	mr %r6, %r23							\n"
"	mr %r7, %r22							\n"
"	b ___start							\n"
);

void
___start(int argc, char **argv, char **envp, void *aux, void (*cleanup)(void))
{
	char *s;

	environ = envp;

	if ((__progname = argv[0]) != NULL) {   /* NULL ptr if argc = 0 */
		if ((__progname = _strrchr(__progname, '/')) == NULL)
			__progname = argv[0];
		else
			__progname++;
		for (s = __progname_storage; *__progname &&
		   s < &__progname_storage[sizeof __progname_storage - 1]; )
			*s++ = *__progname++;
		*s = '\0';
		__progname = __progname_storage;
	}
#if 0
	atexit(cleanup);
#endif
#ifdef MCRT0
	atexit(_mcleanup);
	monstartup((u_long)&_eprol, (u_long)&_etext);
#endif

#ifndef SCRT0
	__init();
#endif

	exit(main(argc, argv, environ));
}

static inline char *
_strrchr(const char *p, char ch)
{
	char *save;
	
	for (save = NULL;; ++p) {
		if (*p == ch)
			save = (char *)p;
		if (!*p)
			return(save);
	}
/* NOTREACHED */
}
asm ("	.section \".text\" \n_eprol:");

