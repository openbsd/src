/*	$OpenBSD: lcore_access.S,v 1.7 2004/09/27 19:16:06 pefo Exp $ */

/*
 * Copyright (c) 2001-2003 Opsycon AB  (www.opsycon.se / www.opsycon.com)
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
#include <sys/errno.h>
#include <sys/syscall.h>

#include <machine/param.h>
#include <machine/psl.h>
#include <machine/asm.h>
#include <machine/cpu.h>
#include <machine/regnum.h>
#include <machine/cpustate.h>
#include <machine/trap.h>

#include "assym.h"

	.set	mips3

	.set	noreorder		# Noreorder is default style!

/*
 * Primitives
 */

/*
 * This table is indexed by u.u_pcb.pcb_onfault in trap().
 * The reason for using this table rather than storing an address in
 * u.u_pcb.pcb_onfault is simply to make the code faster.
 * This table must match with definitions in trap.h.
 */
	.globl	onfault_table
	.data
	.align	3
onfault_table:
	PTR_VAL	0		# invalid index number
	PTR_VAL	baderr
	PTR_VAL	_copyerr
	PTR_VAL	_copyerr
#if defined(DDB) || defined(DEBUG)
	PTR_VAL	kt_ddberr
#else
	PTR_VAL	0
#endif
	.text

/*
 * See if access to addr with a len type instruction causes a machine check.
 * len is length of access (1=byte, 2=short, 4=long)
 *
 * badaddr(addr, len)
 *	char *addr;
 *	int len;
 */
LEAF(badaddr, 0)
	li	v0, KT_BADERR
	PTR_L	t3, curprocpaddr
	bne	a1, 1, 2f
	sw	v0, U_PCB_ONFAULT(t3)
	lbu	v0, (a0)		# don't put in bd-slot!
	b	5f
	nop
2:
	bne	a1, 2, 4f
	nop
	lhu	v0, (a0)		# don't put in bd-slot!
	b	5f
	nop
4:
	lw	v0, (a0)
5:
	sync
	sw	zero, U_PCB_ONFAULT(t3)
	j	ra
	move	v0, zero		# made it w/o errors
baderr:
	j	ra
	li	v0, 1			# trap sends us here
END(badaddr)

/*
 * This code is copied the user's stack for returning from signal handlers
 * (see sendsig() and sigreturn()). We have to compute the address
 * of the sigcontext struct for the sigreturn call.
 */
	.globl	sigcode
sigcode:
	PTR_ADDU a0, sp, 4*REGSZ		# address of sigcontext
	LI	v0, SYS_sigreturn	# sigreturn(scp)
	syscall
	break	0			# just in case sigreturn fails
	.globl	esigcode
esigcode:

/* Mips o32 ABI sigcode. 32 bit pointers. */
	.globl	sigcode_o32
sigcode_o32:
	addu	a0, sp, 16		# address of sigcontext
	li	v0, SYS_sigreturn	# sigreturn(scp)
	syscall
	break	0			# just in case sigreturn fails
	.globl	esigcode_o32
esigcode_o32:

/*
 * Copy a null terminated string within the kernel address space.
 * Maxlength may be null if count not wanted.
 *	copystr(fromaddr, toaddr, maxlength, &lencopied)
 *		caddr_t fromaddr;
 *		caddr_t toaddr;
 *		u_int maxlength;
 *		u_long *lencopied;
 */
LEAF(copystr, 0)
	move	t2, a2			# Save the number of bytes
1:
	lbu	t0, 0(a0)
	PTR_SUBU a2, a2, 1
	beq	t0, zero, 2f
	sb	t0, 0(a1)
	PTR_ADDU a0, a0, 1
	bne	a2, zero, 1b
	PTR_ADDU a1, a1, 1
2:
	beq	a3, zero, 3f
	PTR_SUBU a2, t2, a2		# compute length copied
	REG_S	a2, 0(a3)
3:
	j	ra
	move	v0, zero
END(copystr)

#ifndef __LP64__
/*
 *  Read 64 bits from bus in non LP64 mode.
 *  XXX ints should be disabled!
 */
LEAF(lp32_read8, 0)
#if defined(__MIPSEB__)
	ld	v1, 0(a0)
	jr	ra
	dsrl	v0, v1, 32
#else
	ld	v0, 0(a0)
	jr	ra
	dsrl	v1, v0, 32
#endif
END(lp32_read8)

/*
 *  Write 64 bits to bus in non LP64 mode.
 *  XXX ints should be disabled!
 */
LEAF(lp32_write8, 0)
#if defined(__MIPSEB__)
	dsll	a2, 32
	dsll	a3, 32
	dsrl	a3, 32
	or	a2, a3
#else
	dsll	a3, 32
	dsll	a2, 32
	dsrl	a2, 32
	or	a3, a2
#endif
	jr	ra
	sd	a2, 0(a0)
END(lp32_write8)
#endif

/*
 * fillw(pat, addr, count)
 */
LEAF(fillw, 0)
1:
	PTR_ADDU a2, a2, -1
	sh	a0, 0(a1)
	bne	a2,zero, 1b
	PTR_ADDU a1, a1, 2

	jr	ra
	nop
END(fillw)

/*
 * Optimized memory zero code.
 * mem_zero_page(addr);
 */
LEAF(mem_zero_page, 0)
	LI	v0, NBPG
1:
	dsubu	v0, 8
	sd	zero, 0(a0)
	bne	zero, v0, 1b
	daddu	a0, 8
	jr	ra
	nop
END(mem_zero_page)

/*
 *	Block I/O routines mainly used by I/O drivers.
 *
 *	Args as:	a0 = port
 *			a1 = memory address
 *			a2 = count
 */
LEAF(insb, 0)
	beq	a2, zero, 2f
	PTR_ADDU a2, a1
1:
	lbu	v0, 0(a0)
	PTR_ADDU a1, 1
	bne	a1, a2, 1b
	sb	v0, -1(a1)
2:
	jr	ra
	nop
END(insb)

LEAF(insw, 0)
	beq	a2, zero, 2f
	PTR_ADDU a2, a2
	PTR_ADDU a2, a1
1:
	lhu	v0, 0(a0)
	PTR_ADDU a1, 2
	bne	a1, a2, 1b
	sh	v0, -2(a1)
2:
	jr	ra
	nop
END(insw)

LEAF(insl, 0)
	beq	a2, zero, 2f
	PTR_SLL	a2, 2
	PTR_ADDU a2, a1
1:
	lw	v0, 0(a0)
	PTR_ADDU a1, 4
	bne	a1, a2, 1b
	sw	v0, -4(a1)
2:
	jr	ra
	nop
END(insl)

LEAF(outsb, 0)
	beq	a2, zero, 2f
	PTR_ADDU a2, a1
1:
	lbu	v0, 0(a1)
	PTR_ADDU a1, 1
	bne	a1, a2, 1b
	sb	v0, 0(a0)
2:
	jr	ra
	nop
END(outsb)

LEAF(outsw, 0)
	beq	a2, zero, 2f
	PTR_ADDU a2, a2
	LI	v0, 1
	and	v0, a1
	bne	v0, zero, 3f		# arghh, unaligned.
	PTR_ADDU a2, a1
1:
	lhu	v0, 0(a1)
	PTR_ADDU a1, 2
	bne	a1, a2, 1b
	sh	v0, 0(a0)
2:
	jr	ra
	nop
3:
	LWHI	v0, 0(a1)
	LWLO	v0, 3(a1)
	PTR_ADDU a1, 2
	bne	a1, a2, 3b
	sh	v0, 0(a0)

	jr	ra
	nop
END(outsw)

LEAF(outsl, 0)
	beq	a2, zero, 2f
	PTR_SLL	a2, 2
	LI	v0, 3
	and	v0, a1
	bne	v0, zero, 3f		# arghh, unaligned.
	PTR_ADDU a2, a1
1:
	lw	v0, 0(a1)
	PTR_ADDU a1, 4
	bne	a1, a2, 1b
	sw	v0, 0(a0)
2:
	jr	ra
	nop
3:
	LWHI	v0, 0(a1)
	LWLO	v0, 3(a1)
	PTR_ADDU a1, 4
	bne	a1, a2, 3b
	sw	v0, 0(a0)

	jr	ra
	nop
END(outsl)

/*
 * Copy a null terminated string from the user address space into
 * the kernel address space.
 *
 *	copyinstr(fromaddr, toaddr, maxlength, &lencopied)
 *		caddr_t fromaddr;
 *		caddr_t toaddr;
 *		u_int maxlength;
 *		u_int *lencopied;
 */
NON_LEAF(copyinstr, FRAMESZ(CF_SZ), ra)
	PTR_SUBU sp, sp, FRAMESZ(CF_SZ)
	.mask	0x80000000, (CF_RA_OFFS - FRAMESZ(CF_SZ))
	PTR_S	ra, CF_RA_OFFS(sp)
	blt	a0, zero, _copyerr	# make sure address is in user space
	li	v0, KT_COPYERR
	PTR_L	t3, curprocpaddr
	jal	copystr
	sw	v0, U_PCB_ONFAULT(t3)

	PTR_L	ra, CF_RA_OFFS(sp)
	PTR_L	t3, curprocpaddr
	sw	zero, U_PCB_ONFAULT(t3)
	PTR_ADDU sp, sp, FRAMESZ(CF_SZ)
	j	ra
	move	v0, zero
END(copyinstr)

/*
 * Copy a null terminated string from the kernel address space into
 * the user address space.
 *
 *	copyoutstr(fromaddr, toaddr, maxlength, &lencopied)
 *		caddr_t fromaddr;
 *		caddr_t toaddr;
 *		u_int maxlength;
 *		u_int *lencopied;
 */
NON_LEAF(copyoutstr, FRAMESZ(CF_SZ), ra)
	PTR_SUBU sp, sp, FRAMESZ(CF_SZ)
	.mask	0x80000000, (CF_RA_OFFS - FRAMESZ(CF_SZ))
	PTR_S	ra, CF_RA_OFFS(sp)
	blt	a1, zero, _copyerr	# make sure address is in user space
	li	v0, KT_COPYERR
	PTR_L	t3, curprocpaddr
	jal	copystr
	sw	v0, U_PCB_ONFAULT(t3)

	PTR_L	ra, CF_RA_OFFS(sp)
	PTR_L	t3, curprocpaddr
	sw	zero, U_PCB_ONFAULT(t3)
	PTR_ADDU sp, sp, FRAMESZ(CF_SZ)
	j	ra
	move	v0, zero
END(copyoutstr)

/*
 * Copy specified amount of data from user space into the kernel
 *	copyin(from, to, len)
 *		caddr_t *from;	(user source address)
 *		caddr_t *to;	(kernel destination address)
 *		unsigned len;
 */
NON_LEAF(copyin, FRAMESZ(CF_SZ), ra)
	PTR_SUBU sp, sp, FRAMESZ(CF_SZ)
	.mask	0x80000000, (CF_RA_OFFS - FRAMESZ(CF_SZ))
	PTR_S	ra, CF_RA_OFFS(sp)
	blt	a0, zero, _copyerr	# make sure address is in user space
	li	v0, KT_COPYERR
	PTR_L	t3, curprocpaddr
	jal	bcopy
	sw	v0, U_PCB_ONFAULT(t3)

	PTR_L	ra, CF_RA_OFFS(sp)
	PTR_L	t3, curprocpaddr
	sw	zero, U_PCB_ONFAULT(t3)
	PTR_ADDU sp, sp, FRAMESZ(CF_SZ)
	j	ra
	move	v0, zero
END(copyin)

/*
 * Copy specified amount of data from kernel to the user space
 *	copyout(from, to, len)
 *		caddr_t *from;	(kernel source address)
 *		caddr_t *to;	(user destination address)
 *		unsigned len;
 */
NON_LEAF(copyout, FRAMESZ(CF_SZ), ra)
	PTR_SUBU sp, sp, FRAMESZ(CF_SZ)
	.mask	0x80000000, (CF_RA_OFFS - FRAMESZ(CF_SZ))
	PTR_S	ra, CF_RA_OFFS(sp)
	blt	a1, zero, _copyerr	# make sure address is in user space
	li	v0, KT_COPYERR
	PTR_L	t3, curprocpaddr
	jal	bcopy
	sw	v0, U_PCB_ONFAULT(t3)

	PTR_L	ra, CF_RA_OFFS(sp)
	PTR_L	t3, curprocpaddr
	sw	zero, U_PCB_ONFAULT(t3)
	PTR_ADDU sp, sp, FRAMESZ(CF_SZ)
	j	ra
	move	v0, zero
END(copyout)

_copyerr:
	PTR_L	ra, CF_RA_OFFS(sp)
	PTR_L	t3, curprocpaddr
	sw	zero, U_PCB_ONFAULT(t3)
	PTR_ADDU sp, sp, FRAMESZ(CF_SZ)
	j	ra
	li	v0, EFAULT		# return error

/*
 *  kcopy is a wrapper around bcopy that catches bad memory references.
 */
NON_LEAF(kcopy, FRAMESZ(CF_SZ), ra)
	PTR_SUBU sp, sp, FRAMESZ(CF_SZ)
	.mask	0x80000000, (CF_RA_OFFS - FRAMESZ(CF_SZ))
	PTR_S	ra, CF_RA_OFFS(sp)
	li	v0, KT_KCOPYERR
	PTR_L	t3, curprocpaddr
	jal	bcopy
	sw	v0, U_PCB_ONFAULT(t3)

	PTR_L	ra, CF_RA_OFFS(sp)
	PTR_L	t3, curprocpaddr
	sw	zero, U_PCB_ONFAULT(t3)
	PTR_ADDU sp, sp, FRAMESZ(CF_SZ)
	j	ra
	move	v0, zero
END(kcopy)
