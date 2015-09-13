/*	$OpenBSD: ldasm.S,v 1.14 2015/09/13 17:08:04 guenther Exp $ */

/*
 * Copyright (c) 1998-2002 Opsycon AB, Sweden.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
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

#include <machine/asm.h>
#include <sys/syscall.h>
#include <SYS.h>

/* Stack at this stage is:
 * struct stack {
 *	int	kargc;
 *	char	*kargv[1];	size depends on kargc
 *	char	kargstr[1];	size varies
 *	char	kenvstr[1];	size varies
 * };
 */

FRAMESZ= MKFSIZ(4,16)
GPOFF= FRAMESZ-2*REGSZ
RAOFF= FRAMESZ-1*REGSZ

LEAF(_dl_start, FRAMESZ)		/* Not really LEAF, but we simplify */
	PTR_SUBU sp, FRAMESZ		# Some space.
	SETUP_GP64(GPOFF, _dl_start)

	LA	s1, 1f
	bgezal	zero, 1f
1:
	PTR_SUBU s0, ra, s1		# This is the load offset

	PTR_ADDU a0, sp, FRAMESZ	# Where stack info is.
	PTR_ADDU a1, sp, 0		# Where fast AUX info will be.
	LA	t9, _dl_boot_bind
	PTR_ADDU t9, s0
	jalr	t9			# Relocate ourself.

	REG_L	a3, FRAMESZ(sp)		# argc
	PTR_ADDU a0, sp, FRAMESZ+REGSZ	# argv
	PTR_ADDU a1, a0, REGSZ
	PTR_SLL  a3, a3, LOGREGSZ
	PTR_ADDU a1, a3
	PTR_ADDU a3, sp, 0		# Where fast AUX info will be.
	move	a2, s0			# Load offset
	jal	_dl_boot		# Go do the linking.

	move	t9, v0			# Entry address from _dl_boot.
	LA	v0, _dl_dtors		# cleanup

	RESTORE_GP64
	PTR_ADDU sp, FRAMESZ		# Restore stack pointer.
	move	ra, zero		# Mark last stack frame.
	j	t9			# Go execute the 'real' program.
END(_dl_start)

FRAMESZ= MKFSIZ(4,16)
GPOFF= FRAMESZ-2*REGSZ
RAOFF= FRAMESZ-1*REGSZ
A0OFF= FRAMESZ-3*REGSZ
A1OFF= FRAMESZ-4*REGSZ
A2OFF= FRAMESZ-5*REGSZ
A3OFF= FRAMESZ-6*REGSZ
A4OFF= FRAMESZ-7*REGSZ
A5OFF= FRAMESZ-8*REGSZ
A6OFF= FRAMESZ-9*REGSZ
A7OFF= FRAMESZ-10*REGSZ
S0OFF= FRAMESZ-11*REGSZ

	.globl	_dl_bind_start
	.ent	_dl_bind_start, 0
_dl_bind_start:
	ld	v1, -32744(gp)
	PTR_SUBU sp, FRAMESZ
	SETUP_GP64(GPOFF, _dl_bind_start)
	REG_S	a0, A0OFF(sp)
	REG_S	a1, A1OFF(sp)
	REG_S	a2, A2OFF(sp)
	REG_S	a3, A3OFF(sp)
	REG_S	a4, A4OFF(sp)
	REG_S	a5, A5OFF(sp)
	REG_S	a6, A6OFF(sp)
	REG_S	a7, A7OFF(sp)
	REG_S	$15, RAOFF(sp)
	REG_S	s0, S0OFF(sp)
	move	s0, sp
	move	a0, v1
	move	a1, t8
	jal	_dl_bind

	move	sp, s0
	REG_L	ra, RAOFF(sp)
	REG_L	s0, S0OFF(sp)
	REG_L	a0, A0OFF(sp)
	REG_L	a1, A1OFF(sp)
	REG_L	a2, A2OFF(sp)
	REG_L	a3, A3OFF(sp)
	REG_L	a4, A4OFF(sp)
	REG_L	a5, A5OFF(sp)
	REG_L	a6, A6OFF(sp)
	REG_L	a7, A7OFF(sp)
	RESTORE_GP64
	PTR_ADDU sp, FRAMESZ
	move	t9, v0
	jr	t9
	.end	_dl_bind_start

#define	DL_SYSCALL(n)		DL_SYSCALL2(n,n)
#define	DL_SYSCALL_NOERR(n)	DL_SYSCALL2_NOERR(n,n)
#define	DL_SYSCALL2(n,c)						\
NLEAF(_dl_##n,0)							\
	__DO_SYSCALL(c);						\
	bnez	a3, _dl_cerror;						\
	j	ra;							\
END(_dl_##n)
#define	DL_SYSCALL2_NOERR(n,c)						\
NLEAF(_dl_##n,0)							\
	__DO_SYSCALL(c);						\
	j	ra;							\
END(_dl_##n)

_dl_cerror:
	subu	v0, zero, v0
	j	ra

NLEAF(_dl_sigprocmask,0)
	/* _dl_sigprocmask does not support NULL as the new mask */
#if 0
	bnez	a1, 1f
	li	a0, 1	/* SIG_BLOCK */
	b	2f
1:
#endif
	lw	a1, 0(a1)
2:
	__DO_SYSCALL(sigprocmask)
	bnez	a3, _dl_cerror
	beqz	a2, 1f
	sw	v0, 0(a2)
1:
	li	v0, 0
	j	ra
END(_dl_sigprocmask)

DL_SYSCALL(close)
DL_SYSCALL_NOERR(exit)
DL_SYSCALL(fstat)
DL_SYSCALL2(getcwd,__getcwd)
DL_SYSCALL(getdents)
DL_SYSCALL(getentropy)
DL_SYSCALL(sendsyslog)
DL_SYSCALL(gettimeofday)
DL_SYSCALL_NOERR(issetugid)
DL_SYSCALL(lstat)
DL_SYSCALL(mprotect)
DL_SYSCALL(munmap)
DL_SYSCALL(open)
DL_SYSCALL(read)
DL_SYSCALL(readlink)
DL_SYSCALL2(_syscall,__syscall)
DL_SYSCALL(sysctl)
DL_SYSCALL(utrace)
DL_SYSCALL(write)
