/*	$OpenBSD: unixsys.S,v 1.4 2004/01/13 23:54:07 millert Exp $	*/

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

#include <machine/asm.h>
#include <sys/syscall.h>


#ifdef __STDC__
#define RSYSCALL(x) \
	ENTRY(u##x); \
		movl	 $(SYS_##x), %eax; \
		jmp	 1f
#else
#define RSYSCALL(x) \
	ENTRY(u/**/x); \
		movl	 $(SYS_/**/x), %eax; \
		jmp	 1f
#endif

	.globl	_errno

RSYSCALL(open)
RSYSCALL(read)
RSYSCALL(write)
RSYSCALL(ioctl)
RSYSCALL(close)
RSYSCALL(exit)

1:
	int	$0x80
2:	jnc	3f
	movl	%eax, _errno
	movl	$-1, %eax
	movl	$-1, %edx
3:	ret

ENTRY(__syscall)
	movl	$(SYS___syscall), %eax
	jmp	1b

ENTRY(syscall)
	popl	%ecx	/* rta */
	popl	%eax	/* syscall number */
	pushl	%ecx
	int	$0x80
	pushl	%ecx
	jmp	2b
