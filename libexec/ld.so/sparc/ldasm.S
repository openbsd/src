/*	$OpenBSD: ldasm.S,v 1.4 2002/07/29 15:20:38 art Exp $	*/

/*
 * Copyright (c) 2001 Jason L. Wright (jason@thought.net)
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
 *	This product includes software developed by Jason L. Wright
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*-
 * Copyright (c) 2000 Eduardo Horvath.
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas and Paul Kranenburg.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
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

#include <sys/syscall.h>
#include <machine/trap.h>
#include <machine/asm.h>

#define DL_DATA_SIZE	(16*4)
#define ARGC		(16*4)

	.section	".text"
	.align	4
	.global	_dl_start
	.type	_dl_start,@function
_dl_start:
	sub	%g0, %g0, %fp		! clear frame
	mov	%g1, %l1		! save ps_strings
	sub	%sp, DL_DATA_SIZE, %sp	! make room for dl_data
	add	%sp, ARGC, %l3

	add	%l3, DL_DATA_SIZE, %o0
	mov	0, %o2			! dynp = 0
	mov	%o0, %l0
	call	_dl_boot_bind		! _dl_boot_bind(sp,loff,dynp,dl_data)
	 mov	%l3, %o3

	mov	%l3, %o4
	ld	[%l0], %l3		! argc = *sp
	sll	%l3, 2, %l3		! argc *= sizeof(long)
	add	%l0, 4, %o0		! argv = [sp + argc]
	add	%l0, 8, %o1		! envp = sp + 8 +
	add	%o1, %l3, %o1		!	+ argc

	add	%o4, (7*4), %l2
	ld	[%l2], %o2		! loff = dl_data[AUX_base];

	call	_dl_boot		! _dl_boot(argv,envp,loff,dynp,dl_data)
	 mov	0, %o3			! dynp = 0

	add	%sp, DL_DATA_SIZE, %sp	! restore stack
	
	jmp	%o0
	 mov	%l1, %g1		! restore ps_strings


	.section	".text"
	.align	4
	.global	_dl_bind_start
	.type	_dl_bind_start,@function
_dl_bind_start:	# (obj, reloff)
	save	%sp, -96, %sp		/* setup standard stack frame */
	ld	[%i7 + 8], %o0		/* obj id is in second PLT slot */
	srl	%g1, 10, %o1		/* offset is in high 22 bits */
	call	_dl_bind		/* Call _rtld_bind(obj, offset) */
	 sub	%o1, 12*4, %o1		/* first 4 `pltrel' entries missing! */

	mov	%o0, %g1		/* return value == function address */
	restore				/* get rid of our context */
	 jmp	%g1			/* and the jmpslot context, then go. */
	restore

	.section ".text"
	.align 4
	.global _dl_close
	.type _dl_close,@function
_dl_close:
	mov SYS_close | SYSCALL_G2RFLAG, %g1	! call sys_close
	add %o7, 8, %g2				! just return on success
	t ST_SYSCALL				! off to wonderland
	retl
	 sub %g0, %o0, %o0			! error: result = -errno


	.section ".text"
	.align 4
	.global _dl_exit
	.type _dl_exit,@function
_dl_exit:
	mov SYS_exit | SYSCALL_G2RFLAG, %g1	! call sys_exit
	add %o7, 8, %g2				! just return on success
	t ST_SYSCALL				! off to wonderland
	retl
	 sub %g0, %o0, %o0			! error: result = -errno


	.section ".text"
	.align 4
	.global _dl_issetugid
	.type _dl_issetugid,@function
_dl_issetugid:
	mov SYS_issetugid | SYSCALL_G2RFLAG, %g1
	add %o7, 8, %g2
	t ST_SYSCALL
	retl
	 sub %g0, %o0, %o0


	.section ".text"
	.align 4
	.global _dl__syscall
	.type _dl__syscall,@function
_dl__syscall:
	mov SYS___syscall | SYSCALL_G2RFLAG, %g1	! call sys_exit
	add %o7, 8, %g2				! just return on success
	t ST_SYSCALL				! off to wonderland
	retl
	 sub %g0, %o0, %o0			! error: result = -errno


	.section ".text"
	.align 4
	.global _dl_munmap
	.type _dl_munmap,@function
_dl_munmap:
	mov SYS_munmap | SYSCALL_G2RFLAG, %g1	! calling sys_mmap
	add %o7, 8, %g2				! just return on success
	t ST_SYSCALL				! off to wonderland
	retl
	 sub %g0, %o0, %o0			! error: result = -errno


	.section ".text"
	.align 4
	.global _dl_mprotect
	.type _dl_mprotect,@function
_dl_mprotect:
	mov SYS_mprotect | SYSCALL_G2RFLAG, %g1	! calling sys_mprotect
	add %o7, 8, %g2				! just return on success
	t ST_SYSCALL				! off to wonderland
	retl
	 sub %g0, %o0, %o0			! error: result = -errno


	.section ".text"
	.align 4
	.global _dl_open
	.type _dl_open,@function
_dl_open:
	mov SYS_open | SYSCALL_G2RFLAG, %g1	! calling sys_open
	add %o7, 8, %g2				! just return on success
	t ST_SYSCALL				! off to wonderland
	retl
	 sub %g0, %o0, %o0			! error: result = -errno


	.section ".text"
	.align 4
	.global _dl_read
	.type _dl_read,@function
_dl_read:
	mov SYS_read | SYSCALL_G2RFLAG, %g1	! calling sys_read
	add %o7, 8, %g2				! just return on success
	t ST_SYSCALL				! off to wonderland
	retl
	 sub %g0, %o0, %o0			! error: result = -errno


	.section ".text"
	.align 4
	.global _dl_write
	.type _dl_write,@function
_dl_write:
	mov SYS_write | SYSCALL_G2RFLAG, %g1	! calling sys_write
	add %o7, 8, %g2				! just return on success
	t ST_SYSCALL				! off to wonderland
	retl
	 sub %g0, %o0, %o0			! error: result = -errno


	.section ".text"
	.align 4
	.global _dl_stat
	.type _dl_stat,@function
_dl_stat:
	mov SYS_stat | SYSCALL_G2RFLAG, %g1	! call sys_stat
	add %o7, 8, %g2				! just return on success
	t ST_SYSCALL				! off to wonderland
	retl
	 sub %g0, %o0, %o0			! error: result = -errno


	.section ".text"
	.align 4
	.globl _dl_fstat
	.type _dl_fstat,@function
_dl_fstat:
	mov  SYS_fstat | SYSCALL_G2RFLAG, %g1	! call sys_fstat
	add %o7, 8, %g2				! just return on success
	t ST_SYSCALL				! off to wonderland
	retl
	 sub %g0, %o0, %o0                      ! error: result = -errno


	.section ".text"
	.align 4
	.globl _dl_fcntl
	.type _dl_fcntl,@function
_dl_fcntl:
	mov  SYS_fcntl | SYSCALL_G2RFLAG, %g1	! call sys_fstat
	add %o7, 8, %g2				! just return on success
	t ST_SYSCALL				! off to wonderland
	retl
	 sub %g0, %o0, %o0                      ! error: result = -errno


	.section ".text"
	.align 4
	.globl _dl_getdirentries
	.type _dl_getdirentries,@function
_dl_getdirentries:
	mov  SYS_getdirentries | SYSCALL_G2RFLAG, %g1	! call sys_fstat
	add %o7, 8, %g2					! just return on success
	t ST_SYSCALL					! off to wonderland
	retl
	 sub %g0, %o0, %o0                      ! error: result = -errno

