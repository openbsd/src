/*	$OpenBSD: crt0.c,v 1.5 2002/07/22 19:15:40 art Exp $	*/
/*	$NetBSD: crt0.c,v 1.15 1995/06/15 21:41:55 pk Exp $	*/
/*
 * Copyright (c) 1993 Paul Kranenburg
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
 *      This product includes software developed by Paul Kranenburg.
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
 *
 */


#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: crt0.c,v 1.5 2002/07/22 19:15:40 art Exp $";
#endif /* LIBC_SCCS and not lint */

#include <sys/param.h>
#include <stdlib.h>

#include "common.h"

extern	unsigned char	etext;
extern	unsigned char	eprol asm ("eprol");
extern void		start(void) asm("start");

#undef mmap
#define mmap(addr, len, prot, flags, fd, off)	\
    __syscall2((quad_t)SYS_mmap, (addr), (len), (prot), (flags), \
	(fd), 0, (off_t)(off))
extern int		__syscall2(quad_t, ...);

asm ("	.global start");
asm ("	.text");
asm ("	start:");

/* Set up `argc', `argv', and `envp' into local registers (from GNU Emacs). */
asm ("	mov	0, %fp");
asm ("	ld	[%sp + 64], %l0");	/* argc */
asm ("	add	%sp, 68, %l1");		/* argv */
asm ("	sll	%l0, 2,	%l2");		/**/
asm ("	add	%l2, 4,	%l2");		/* envp = argv + (argc << 2) + 4 */
asm ("	add	%l1, %l2, %l2");	/**/
asm ("	sethi	%hi(_environ), %l3");
asm ("	st	%l2, [%l3+%lo(_environ)]");	/* *environ = l2 */

/* Finish diddling with stack. */
asm ("	andn	%sp, 7,	%sp");
asm ("	sub	%sp, 24, %sp");

/*
 * Set __progname:
 *	if (argv[0])
 *		if ((__progname = _strrchr(argv[0], '/')) == NULL)
 *			__progname = argv[0];
 *		else
 *			++__progname;
 */
asm ("	ld	[%l1], %o0");
asm ("	cmp	%o0, 0");
asm ("	mov	%o0, %l6");
asm ("	be	1f");
asm ("	sethi	%hi(___progname), %l7");
#ifdef DYNAMIC
asm ("	call	__strrchr");
#else
asm ("	call	_strrchr");
#endif
asm ("	mov	47, %o1");
asm ("	cmp	%o0, 0");
asm ("	be,a	1f");
asm ("	st	%l6, [%l7+%lo(___progname)]");
asm ("	add	%o0, 1, %o0");
asm ("	st	%o0, [%l7+%lo(___progname)]");
asm ("1:");

#ifdef DYNAMIC
/* Resolve symbols in dynamic libraries */
asm ("	sethi	%hi(__DYNAMIC), %o0");
asm ("	orcc	%o0, %lo(__DYNAMIC), %o0");
asm ("	be	1f");
asm ("	nop");
asm ("	call	___load_rtld");
asm ("	nop");
asm ("1:");
#endif

/* From here, all symbols should have been resolved, so we can use libc */
#ifdef MCRT0
/*
 * atexit(_mcleanup);
 * monstartup((u_long)&eprol, (u_long)&etext);
 */
asm ("	sethi	%hi(__mcleanup), %o0");
asm ("	call	_atexit");
asm ("	or	%o0, %lo(__mcleanup), %o0");
asm ("	sethi	%hi(_eprol), %o0");
asm ("	or	%o0, %lo(_eprol), %o0");
asm ("	sethi	%hi(_etext), %o1");
asm ("	call	_monstartup");
asm ("	or	%o1, %lo(_etext), %o1");
#endif

/* Move `argc', `argv', and `envp' from locals to parameters for `main'.  */
asm ("	mov	%l0,%o0");
asm ("	mov	%l1,%o1");
asm ("__callmain:");		/* Defined for the benefit of debuggers */
asm ("	call	_main");
asm ("	mov	%l2,%o2");

asm ("	call	_exit");
asm ("	nop");

#ifdef DYNAMIC
/* System call entry */
asm("	.set	SYSCALL_G2RFLAG, 0x400");
asm("	.set	SYS___syscall, 198");
asm("___syscall2:");
asm("	sethi	%hi(SYS___syscall), %g1");	/* `SYS___syscall' */
asm("	ba	1f");
asm("	or	%g1, %lo(SYS___syscall), %g1");
asm("___syscall:");
asm("	clr	%g1");				/* `SYS_syscall' */
asm("1:");
asm("	or	%g1, SYSCALL_G2RFLAG, %g1");	/* Use quick return */
asm("	add	%o7, 8, %g2");
asm("	ta	%g0");
asm("	mov	-0x1, %o0");			/* Note: no `errno' */
asm("	jmp	%o7 + 0x8");
asm("	mov	-0x1, %o1");
#endif

#include "common.c"

#ifdef MCRT0
asm ("	.text");
asm ("_eprol:");
#endif
