/*	$OpenBSD: exception_tfp.S,v 1.3 2014/04/09 21:10:35 miod Exp $	*/

/*
 * Copyright (c) 2012 Miodrag Vallat.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
/*
 * k_general and u_general are heavily based upon their counterparts in
 * exception.S under the following licence terms:
 */
/*
 * Copyright (c) 2002-2003 Opsycon AB  (www.opsycon.se / www.opsycon.com)
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

/*
 * Trap handling subpage for R8000 systems.
 */

#include <machine/param.h>
#include <machine/asm.h>
#include <machine/cpu.h>
#include <mips64/mips_cpu.h>
#include <machine/pte.h>
#include <machine/regnum.h>
#include <machine/cpustate.h>

#include "assym.h"

#define	TLBW	.align 4; .word 0x43000002

	.set	mips4
	.set	noreorder

	.text

	.align	12
	.globl	tfp_trapbase
tfp_trapbase:

	/*
	 * 000: tlb miss handler for U region
	 */
	.align	10

	.ent	utlb_miss, 0
utlb_miss:
	.set	noat
	.align	4
	PRE_MFC0_ADDR_HAZARD
	DMFC0	k0, COP_0_VADDR
	MFC0_HAZARD
	DMTC0	k0, COP_0_WORK0
	MTC0_HAZARD
	PTR_SRL	k0, SEGSHIFT
	sltiu	k1, k0, PMAP_SEGTABSIZE
	beqz	k1, _inv_seg
	 NOP
	DMFC0	k1, COP_0_UBASE		# PCB_SEGTAB(CI_CURPROCPADDR(curcpu))
	MFC0_HAZARD
	PTR_SLL	k0, LOGREGSZ
	PTR_ADDU k1, k0
	PTR_L	k1, 0(k1)		# get pointer to page table
	DMFC0	k0, COP_0_WORK0		# saved COP_0_VADDR
	MFC0_HAZARD
	beqz	k1, _inv_seg
	 PTR_SRL k0, PAGE_SHIFT - 2
	andi	k0, ((NPTEPG / 2) - 1) << 2
	PTR_ADDU k1, k0
	lwu	k0, 0(k1)		# get pte
	DMTC0	k0, COP_0_TLB_LO
	MTC0_HAZARD
	TLBW
	ERET

_inv_seg:
	DMFC0	k0, COP_0_STATUS_REG
	MFC0_HAZARD
	andi	k0, SR_KSU_USER
	bnez	k0, u_general
	 NOP
	b	k_general
	 NOP

	.set	at
	.end	utlb_miss

	/*
	 * 400: tlb miss handler for KV0 region
	 */
	.align	10

	.ent	kv0tlb_miss, 0
kv0tlb_miss:
	.set	noat
	/*
	 * Since we do not set up KV0 mappings, fall through directly
	 * into the `invalid address' fault handling.
	 */
	.align	4
	b	_inv_seg
	 NOP
	.set	at
	.end	kv0tlb_miss

	/*
	 * 800: tlb miss handler for KV1 region
	 */
	.align	10

	.ent	kv1tlb_miss, 0
kv1tlb_miss:
	.set	noat
	.align	4
	PRE_MFC0_ADDR_HAZARD
	DMFC0	k0, COP_0_VADDR
	MFC0_HAZARD
	DMFC0	k1, COP_0_STATUS_REG
	MFC0_HAZARD
	andi	k1, SR_KSU_USER
	bnez	k1, u_general
	 LA	k1, VM_MIN_KERNEL_ADDRESS
	PTR_SUBU k0, k1
	DMFC0	k1, COP_0_WORK1		# Sysmapsize
	MFC0_HAZARD
	PTR_SRL	k0, PAGE_SHIFT
	sltu	k1, k0, k1
	beqz	k1, k_general
	 PTR_SLL k0, 2
	DMFC0	k1, COP_0_GBASE		# Sysmap
	MFC0_HAZARD
	PTR_ADDU k1, k0
	lwu	k0, 0(k1)		# get pte
	DMTC0	k0, COP_0_TLB_LO
	MTC0_HAZARD
	TLBW
	ERET
	.set	at
	.end	kv1tlb_miss

	/*
	 * C00: tlb miss exception while servicing an exception
	 *	tlb invalid exception
	 *	tlb modified exception
	 *	all other exceptions
	 */
	.align	10
	.ent	exception, 0
exception:
	.set	noat
	.align	4
	DMFC0	k0, COP_0_STATUS_REG
	MFC0_HAZARD
	andi	k0, SR_KSU_USER
	DMFC0	k1, COP_0_CAUSE_REG
	MFC0_HAZARD
	bnez	k0, u_general
	 and	k1, CR_EXC_CODE

	LA	k0, k_exception_table
	PTR_ADDU k0, k1
	PTR_L	k1, 0(k0)
	j	k1
	 NOP

	.set	at
	.end	exception

	/*
	 * Handle a TLB invalid exception from kernel mode. This implies there
	 * is a TLB match, but currently not valid. Check if the pte is now
	 * valid and update, or pass the mess to the regular exception handler.
	 */
k_tlb_inv:
	.set	noat
	.align	4
	PRE_MFC0_ADDR_HAZARD
	DMFC0	k0, COP_0_VADDR
	MFC0_HAZARD
	LA	k1, VM_MIN_KERNEL_ADDRESS
	PTR_SUBU k0, k1
	DMFC0	k1, COP_0_WORK1		# Sysmapsize
	MFC0_HAZARD
	PTR_SRL	k0, PAGE_SHIFT
	sltu	k1, k0, k1
	beqz	k1, k_general
	 PTR_SLL k0, 2
	DMFC0	k1, COP_0_GBASE		# Sysmap
	MFC0_HAZARD
	PTR_ADDU k1, k0
	lwu	k0, 0(k1)		# get pte
	andi	k1, k0, PG_V
	beqz	k1, k_general		# if not valid
	 NOP

	DMTC0	k0, COP_0_TLB_LO
	MTC0_HAZARD
	TLBW
	ERET
	.set	at

k_exception_table:
	PTR_VAL	k_general	# interrupt
	PTR_VAL	k_general	# TLB modification
	PTR_VAL	k_tlb_inv
	PTR_VAL	k_tlb_inv
	PTR_VAL	k_general
	PTR_VAL	k_general
	PTR_VAL	k_general
	PTR_VAL	k_general
	PTR_VAL	k_general
	PTR_VAL	k_general
	PTR_VAL	k_general
	PTR_VAL	k_general
	PTR_VAL	k_general
	PTR_VAL	k_general
	PTR_VAL	k_general
	PTR_VAL	k_general
	PTR_VAL	k_general
	PTR_VAL	k_general
	PTR_VAL	k_general
	PTR_VAL	k_general
	PTR_VAL	k_general
	PTR_VAL	k_general
	PTR_VAL	k_general
	PTR_VAL	k_general
	PTR_VAL	k_general
	PTR_VAL	k_general
	PTR_VAL	k_general
	PTR_VAL	k_general
	PTR_VAL	k_general
	PTR_VAL	k_general
	PTR_VAL	k_general
	PTR_VAL	k_general

	.align	5
NNON_LEAF(u_general, FRAMESZ(CF_SZ), ra)
	.set	noat
	.mask	0x80000000, (CF_RA_OFFS - FRAMESZ(CF_SZ))

	GET_CPU_INFO(k1, k0)
	PTR_L	k0, CI_CURPROCPADDR(k1)
	SAVE_CPU(k0, 0)
	SAVE_CPU_SREG(k0, 0)
	PTR_ADDU sp, k0, USPACE-FRAMESZ(CF_SZ)
	LA	gp, _gp
	.set	at
	and	t0, a1, ~(SR_COP_1_BIT | SR_EXL | SR_INT_ENAB | SR_KSU_MASK)
	DMTC0	t0, COP_0_STATUS_REG
	MTC0_SR_IE_HAZARD

	jal	trap
	 PTR_S	a3, CF_RA_OFFS(sp)		# for debugging

	DMFC0	t0, COP_0_STATUS_REG	# enable interrupts before checking
	ori	t0, SR_INT_ENAB		# for ast.
	DMTC0	t0, COP_0_STATUS_REG
	MTC0_SR_IE_HAZARD

0:
	GET_CPU_INFO(t1, t0)
	PTR_L	t0, CI_CURPROC(t1)
	lw	v0, P_ASTPENDING(t0)	# any pending AST?
	beq	v0, zero, 4f
	 NOP

	jal	ast
	 NOP

	b	0b
	 NOP

4:
	DMFC0	t0, COP_0_STATUS_REG	# disable interrupts
	MFC0_HAZARD
	ori	t0, SR_INT_ENAB
	xori	t0, SR_INT_ENAB
	DMTC0	t0, COP_0_STATUS_REG
	MTC0_SR_IE_HAZARD

	ori	t0, SR_EXL		# restoring to user mode.
	DMTC0	t0, COP_0_STATUS_REG	# must set exception level bit.
	MTC0_SR_IE_HAZARD

	# t1 is curcpu() from earlier
	move	k1, t1
	PTR_L	k0, CI_CURPROCPADDR(k1)
	RESTORE_REG(a3, CPL, k0, 0)
	sw	a3, CI_IPL(k1)
	.set	noat
	RESTORE_CPU_SREG(k0, 0)
	RESTORE_REG(a0, PC, k0, 0)
	RESTORE_CPU(k0, 0)
	RESTORE_REG(sp, SP, k0, 0)
	LI	k0, 0
	LI	k1, 0
	ERET
	.set	at
END(u_general)

	.align	5
NNON_LEAF(k_general, FRAMESZ(KERN_EXC_FRAME_SIZE), ra)
	.globl	k_intr				# for trap.c peace of mind
k_intr:
	.set	noat
	.mask	0x80000000, (CF_RA_OFFS - FRAMESZ(KERN_EXC_FRAME_SIZE))

#ifdef DEBUG
	GET_CPU_INFO(k1, k0)
	PTR_L	k1, CI_CURPROCPADDR(k1)
	PTR_SUBU k0, sp, k1
	sltiu	k0, 2048
	beqz	k0, 8f
	 NOP

	LA	a0, start - FRAMESZ(CF_SZ) - 4 * REGSZ
	move	a6, sp
	move	sp, a0
	DMFC0	a1, COP_0_EXC_PC
	MFC0_HAZARD
	LA	a0, 1f
	PRE_MFC0_ADDR_HAZARD
	DMFC0	a3, COP_0_VADDR
	MFC0_HAZARD
	DMFC0	a4, COP_0_STATUS_REG
	MFC0_HAZARD
	DMFC0	a5, COP_0_CAUSE_REG
	MFC0_HAZARD
	jal	printf
	 move	a2, ra

	LA	sp, start-FRAMESZ(CF_SZ)
#ifdef DDB
	LA	a1, db_printf
	LA	a0, 2f
	jal	trapDump
	 NOP
#endif
	PANIC("kernel stack overflow")

	.data
1:
	.asciiz "\rtfptrap: PC %p RA %p ADR %p\nSR %p CR %p SP %p\n"
2:
	.asciiz	"stack oflow"

	.text
8:
#endif

	PTR_SUB	k0, sp, FRAMESZ(KERN_EXC_FRAME_SIZE)
	SAVE_CPU(k0, CF_RA_OFFS)
#if defined(DDB)
	SAVE_CPU_SREG(k0, CF_RA_OFFS)
#endif
	.set	at
	move	sp, k0			# Already on kernel stack
	LA	gp, _gp
	and	t0, a1, ~(SR_COP_1_BIT | SR_EXL | SR_INT_ENAB | SR_KSU_MASK)
	DMTC0	t0, COP_0_STATUS_REG
	MTC0_SR_IE_HAZARD
	PTR_S	a0, 0(sp)
	jal	trap
	 PTR_S	a3, CF_RA_OFFS + KERN_REG_SIZE(sp)

	DMFC0	t0, COP_0_STATUS_REG	# disable interrupts
	MFC0_HAZARD
	ori	t0, SR_INT_ENAB
	xori	t0, SR_INT_ENAB
	DMTC0	t0, COP_0_STATUS_REG
	MTC0_SR_IE_HAZARD

	.set	noat
	RESTORE_REG(a0, PC, sp, CF_RA_OFFS)
	RESTORE_CPU(sp, CF_RA_OFFS)
	PTR_ADDU sp, sp, FRAMESZ(KERN_EXC_FRAME_SIZE)
	ERET
	.set	at
END(k_general)
