/*	$OpenBSD: ldasm.S,v 1.2 2002/10/21 16:01:55 drahn Exp $ */

/*
 * Copyright (c) 2002 Dale Rahn
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
 *	This product includes software developed under OpenBSD by
 *	Dale Rahn.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WsdfARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */


#define DL_DATA_SIZE      (16*4)
#include <sys/syscall.h>
#include <machine/asm.h>

	.text
	.align  4
	.globl  _dl_start
	.type   _dl_start,@function
_dl_start:
	movl	%esp,%eax		# save stack pointer for _rtld
	pushl	%ebx			# save ps_strings
	subl	$DL_DATA_SIZE,%esp	# allocate dl_data
	movl	%esp,%ebx
	movl	%ebx,%edi		# save dl_data arg for dl_boot
	pushl	%ebx			# push dl_data for dl_boot_bind

	mov	%eax, %esi		# save stack for dl_boot

	pushl	%eax			# load saved SP for dl_boot_bind

	call	_dl_boot_bind@PLT	# _dl_boot_bind(sp,dl_data)

	pushl	%edi			# push saved dl_data
	movl	%edi,%ebp
	movl	(7*4)(%ebp),%eax
	pushl	%eax			# push loff from dl_data

	movl	%esi,%ebp
	movl	$4,%eax
	imull	0(%ebp),%eax
	addl	$8,%eax
	addl	%ebp,%eax
	push	%eax			# push envp

	leal	4(%ebp),%eax
	push	%eax			# push argv

	call	_dl_boot@PLT		# _dl_boot(argv,envp,loff,dl_data)

	addl	$4*4,%esp			# pop args

	addl	$DL_DATA_SIZE,%esp	# return dl_data

	popl	%ebx			# %ebx = ps_strings - XXXDSR
	popl	%edx			# %edx = cleanup - XXXDSR
	popl	%ecx			# %ecx = obj_main - XXXDSR
	jmp	*%eax

	.section	".text"
	.align		4
	.global _dl_close
	.type _dl_close,@function
_dl_close:
	mov	$SYS_close, %eax
	int	$0x80
	jb	1f
	ret

	.section	".text"
	.align		4
	.global _dl_exit
	.type _dl_exit,@function
_dl_exit:
	mov	$SYS_exit, %eax
	int	$0x80
	ret


	.section	".text"
	.align		4
	.global _dl_issetugid
	.type _dl_issetugid,@function
_dl_issetugid:
	mov	$SYS_issetugid, %eax
	int	$0x80
	jb	1f		 /* error: result = -errno */
	ret


	.section	".text"
	.align		4
	.global _dl__syscall
	.type _dl__syscall,@function
_dl__syscall:
	mov	$SYS___syscall, %eax
	int	$0x80
	jb	1f		 /* error: result = -errno */
	ret


	.section	".text"
	.align		4
	.global _dl_munmap
	.type _dl_munmap,@function
_dl_munmap:
	mov	$SYS_munmap, %eax
	int	$0x80
	jb	1f		 /* error: result = -errno */
	ret


	.section	".text"
	.align		4
	.global _dl_mprotect
	.type _dl_mprotect,@function
_dl_mprotect:
	mov	$SYS_mprotect, %eax
	int	$0x80
	jb	1f		 /* error: result = -errno */
	ret


	.section	".text"
	.align		4
	.global _dl_open
	.type _dl_open,@function
_dl_open:
	mov	$SYS_open, %eax
	int	$0x80
	jb	1f		 /* error: result = -errno */
	ret


	.section	".text"
	.align		4
	.global _dl_read
	.type _dl_read,@function
_dl_read:
	mov	$SYS_read, %eax
	int	$0x80
	jb	1f		 /* error: result = -errno */
	ret


	.section	".text"
	.align		4
	.global _dl_write
	.type _dl_write,@function
_dl_write:
	mov	$SYS_write, %eax
	int	$0x80
	jb	1f		 /* error: result = -errno */
	ret


	.section	".text"
	.align		4
	.global _dl_stat
	.type _dl_stat,@function
_dl_stat:
	mov	$SYS_stat, %eax
	int	$0x80
	jb	1f		 /* error: result = -errno */
	ret


	.section	".text"
	.align		4
	.global _dl_fstat
	.type _dl_fstat,@function
_dl_fstat:
	mov	$SYS_fstat, %eax
	int	$0x80
	jb	1f		 /* error: result = -errno */
	ret


	.section	".text"
	.align		4
	.global _dl_fcntl
	.type _dl_fcntl,@function
_dl_fcntl:
	mov	$SYS_fcntl, %eax
	int	$0x80
	jb	1f		 /* error: result = -errno */
	ret


	.section	".text"
	.align		4
	.global _dl_getdirentries
	.type _dl_getdirentries,@function
_dl_getdirentries:
	mov	$SYS_getdirentries, %eax
	int	$0x80
	jb	1f		 /* error: result = -errno */
	ret

1:
	/* error: result = -errno; - handled here. */
	neg	%eax
	ret


	.align 4
	.global _dl_bind_start
	.type _dl_bind_start,@function
_dl_bind_start:
	pushf				# save registers
	pushl   %eax
	pushl   %ecx
	pushl   %edx
	pushl   %ebx
	pushl   %ebp
	pushl   %esi
	pushl   %edi
	pushl   %ds
	pushl   %es
 
	pushl   44(%esp)		# Copy of reloff
	pushl   44(%esp)		# Copy of obj
	call    _dl_bind@PLT		# Call the binder
	addl    $8,%esp			# pop binder args
	movl    %eax,44(%esp)		# Store function to be called in obj
 
	popl    %es			# restore registers
	popl    %ds
	popl    %edi
	popl    %esi
	popl    %ebp
	popl    %ebx
	popl    %edx
	popl    %ecx
	popl    %eax
	popf

	leal    4(%esp),%esp		# Discard reloff, do not change eflags
	ret

