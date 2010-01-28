/*	$OpenBSD: tlbhandler.S,v 1.28 2010/01/28 05:27:16 miod Exp $ */

/*
 * Copyright (c) 1995-2004 Opsycon AB  (www.opsycon.se / www.opsycon.com)
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
 *  This code handles TLB exceptions and updates.
 */

#include <machine/param.h>
#include <machine/pte.h>
#include <machine/asm.h>
#include <machine/cpu.h>
#include <machine/regnum.h>
#include <machine/cpustate.h>
#ifdef CPU_LOONGSON2
#include <machine/loongson2.h>
#endif

#include "assym.h"

	.set	mips3

	.set	noreorder	/* Default reorder mode */

/*---------------------------------------------------------------- tlb_miss
 *	Low level TLB exception handler. TLB and XTLB share some
 *	code for the moment and is copied down at the same time.
 *	This code must be PIC and not more than 64 instructions
 *	for both TLB and XTLB handling or it will overflow the
 *	available storage. If the startup code finds out that it
 *	is larger, the trampoline code is copied instead of panicing.
 */
	.set	noat

#if defined(CPU_R5000) || defined(CPU_RM7000)
	.globl	tlb_miss_err_r5k
	.ent	tlb_miss_err_r5k, 0
tlb_miss_err_r5k:
	/*
	 * R5000 errata: edge cases can trigger a TLB miss exception
	 * instead of an invalid TLB exception. Servicing the TLB miss
	 * exception would cause a duplicate TLB to be inserted, which
	 * causes the processor operation to become unpredictable (but
	 * very bad for the kernel's health, ten times out of nine).
	 *
	 * More details about the problem can be found in:
	 *  http://www.linux-mips.org/archives/linux-mips/2000-02/msg00040.html
	 *
	 * We work around the issue by checking for an existing TLB entry,
	 * and handling this as an invalid TLB exception (as it was intended
	 * to be), in this case.
	 */
	tlbp
	mfc0	k1, COP_0_TLB_INDEX
	bltz	k1, tlb_miss		# missing!
	 nop
	j	k_tlb_inv
	 nop
	.end	tlb_miss_err_r5k
#endif	/* CPU_R5000 || CPU_RM7000 */

	.globl	tlb_miss
	.ent	tlb_miss, 0
tlb_miss:
	GET_CPU_INFO(k1, k0)
	PTR_L	k1, CI_CURPROCPADDR(k1)
	dmfc0	k0, COP_0_BAD_VADDR
	bltz	k0, _k_miss		# kernel address space
	PTR_SRL	k0, k0, SEGSHIFT - LOGREGSZ
	PTR_L	k1, PCB_SEGTAB(k1)
	andi	k0, k0, (PMAP_SEGTABSIZE - 1) << LOGREGSZ
	PTR_ADDU k1, k1, k0
	PTR_L	k1, 0(k1)		# get pointer to page table
	dmfc0	k0, COP_0_BAD_VADDR
	PTR_SRL	k0, k0, PGSHIFT - 2
	andi	k0, k0, ((NPTEPG/2) - 1) << 3
	beqz	k1, _inv_seg		# invalid segment map
	PTR_ADDU k1, k1, k0		# index into segment map
	lw	k0, 0(k1)		# get page PTE
tlb_load:
	lw	k1, 4(k1)
	dsll	k0, k0, 34
	dsrl	k0, k0, 34
	dmtc0	k0, COP_0_TLB_LO0
	dsll	k1, k1, 34
	dsrl	k1, k1, 34
	dmtc0	k1, COP_0_TLB_LO1
	nop				# RM7000 needs 4 nops
	nop
	nop
	nop
	tlbwr				# update TLB
	nop
	nop
	nop
	nop

#ifdef CPU_LOONGSON2
	li	k0, COP_0_DIAG_ITLB_CLEAR | COP_0_DIAG_BTB_CLEAR
	dmtc0	k0, COP_0_DIAG
#endif

	eret				# RM7000 need 4 for JTLB usage.
	.end	tlb_miss

/*---------------------------------------------------------------- xtlb_miss
 *	Low level XTLB exception handler.
 */
#if defined(CPU_R5000) || defined(CPU_RM7000)
	.globl	xtlb_miss_err_r5k
	.ent	xtlb_miss_err_r5k, 0
xtlb_miss_err_r5k:
	/* See errata comments in tlb_miss above */
	tlbp
	mfc0	k1, COP_0_TLB_INDEX
	bltz	k1, xtlb_miss		# missing!
	 nop
	j	k_tlb_inv
	 nop
	.end	xtlb_miss_err_r5k
#endif	/* CPU_R5000 || CPU_RM7000 */

	.globl	xtlb_miss
	.ent	xtlb_miss, 0
xtlb_miss:
	dmfc0	k0, COP_0_BAD_VADDR
	bltz	k0, _k_miss		# kernel address space
	PTR_SRL	k0, k0, SEGSHIFT
	sltiu	k1, k0, PMAP_SEGTABSIZE
	beqz	k1, _inv_seg		# wrong if outside pm_segtab
	nop
	GET_CPU_INFO(k1, k0)
	PTR_L	k1, CI_CURPROCPADDR(k1)
	dmfc0	k0, COP_0_BAD_VADDR
	PTR_SRL	k0, k0, SEGSHIFT
	PTR_SLL	k0, k0, LOGREGSZ
	PTR_L	k1, PCB_SEGTAB(k1)
	PTR_ADDU k1, k1, k0
	PTR_L	k1, 0(k1)		# get pointer to page table
	dmfc0	k0, COP_0_BAD_VADDR
	PTR_SRL	k0, k0, PGSHIFT - 2
	andi	k0, k0, ((NPTEPG/2) - 1) << 3
	beqz	k1, _inv_seg
	PTR_ADDU k1, k1, k0
	b	tlb_load		# rest is same as 'tlb_miss'
	lw	k0, 0(k1)

_inv_seg:
	j	tlb_miss_nopt		# No page table for this segment.
	nop

_k_miss:
	j	k_tlb_miss		# kernel tlbmiss.
	dmfc0	k0, COP_0_BAD_VADDR	# must reload.
	.end	xtlb_miss

	.globl	tlb_miss_nopt
	.ent	tlb_miss_nopt, 0
tlb_miss_nopt:
	mfc0	k0, COP_0_STATUS_REG
	andi	k0, SR_KSU_USER
	bne	k0, zero, go_u_general
	nop
	j	k_general
	nop
	.end	tlb_miss_nopt

	.set	at

/*---------------------------------------------------------------- k_tlb_inv
 *	Handle a TLB invalid exception from kernel mode in kernel
 *	space. This happens when we have a TLB match but an invalid
 *	entry. Try to reload.
 */
NLEAF(k_tlb_inv, 0)
	.set	noat
	LA	k1, (VM_MIN_KERNEL_ADDRESS)	# compute index
	dmfc0	k0, COP_0_BAD_VADDR	# get the fault address
	PTR_SUBU k0, k0, k1
	lw	k1, Sysmapsize			# index within range?
	PTR_SRL	k0, k0, PGSHIFT
	sltu	k1, k0, k1
	beq	k1, zero, sys_stk_chk		# No. check for valid stack
	nop

	PTR_L	k1, Sysmap
	PTR_SLL	k0, k0, 2			# compute offset from index
	tlbp					# Probe the invalid entry
	PTR_ADDU k1, k1, k0
	and	k0, k0, 4			# check even/odd page
	bne	k0, zero, k_tlb_inv_odd
	nop

	mfc0	k0, COP_0_TLB_INDEX
	bltz	k0, sys_stk_chk			# probe fail
	lw	k0, 0(k1)			# get PTE entry

	dsll	k0, k0, 34			# get rid of "wired" bit
	dsrl	k0, k0, 34
	dmtc0	k0, COP_0_TLB_LO0		# load PTE entry
	and	k0, k0, PG_V			# check for valid entry
	beq	k0, zero, go_k_general		# PTE invalid
	lw	k0, 4(k1)			# get odd PTE entry
	dsll	k0, k0, 34
	dsrl	k0, k0, 34
	dmtc0	k0, COP_0_TLB_LO1		# load PTE entry
	nop
	nop
	nop
	nop
	tlbwi					# write TLB
	nop
	nop
	nop
	nop

#ifdef CPU_LOONGSON2
	li	k0, COP_0_DIAG_ITLB_CLEAR | COP_0_DIAG_BTB_CLEAR
	dmtc0	k0, COP_0_DIAG
#endif

	eret

k_tlb_inv_odd:
	mfc0	k0, COP_0_TLB_INDEX
	bltz	k0, sys_stk_chk			# probe fail
	lw	k0, 0(k1)			# get PTE entry

	dsll	k0, k0, 34			# get rid of wired bit
	dsrl	k0, k0, 34
	dmtc0	k0, COP_0_TLB_LO1		# save PTE entry
	and	k0, k0, PG_V			# check for valid entry
	beq	k0, zero, go_k_general	# PTE invalid
	lw	k0, -4(k1)			# get even PTE entry
	dsll	k0, k0, 34
	dsrl	k0, k0, 34
	dmtc0	k0, COP_0_TLB_LO0		# save PTE entry
	nop
	nop
	nop
	nop
	tlbwi					# update TLB
	nop
	nop
	nop
	nop

#ifdef CPU_LOONGSON2
	li	k0, COP_0_DIAG_ITLB_CLEAR | COP_0_DIAG_BTB_CLEAR
	dmtc0	k0, COP_0_DIAG
#endif

	eret
END(k_tlb_inv)

/*---------------------------------------------------------------- k_tlb_miss
 *
 *	Handle a TLB miss exception from kernel mode in kernel space.
 *	We must check that this is coming from kernel mode. If not
 *	it's a bad address from user mode so handle properly.
 *	Load up the correct entry contents from the kernel map.
 *	k0 has bad address.
 */
NLEAF(k_tlb_miss, 0)
	.set	noat
	mfc0	k1, COP_0_STATUS_REG
	andi	k1, SR_KSU_USER
	bne	k1, zero, go_u_general
	LA	k1, (VM_MIN_KERNEL_ADDRESS)	# compute index
	PTR_SUBU k0, k0, k1
	lw	k1, Sysmapsize			# index within range?
	PTR_SRL	k0, k0, PGSHIFT
	sltu	k1, k0, k1
	beq	k1, zero, sys_stk_chk		# No. check for valid stack
	PTR_SRL	k0, k0, 1
	PTR_L	k1, Sysmap
	PTR_SLL	k0, k0, 3			# compute offset from index
	PTR_ADDU k1, k1, k0
	lw	k0, 0(k1)			# get PTE entry
	lw	k1, 4(k1)			# get odd PTE entry
	dsll	k0, k0, 34			# get rid of "wired" bit
	dsrl	k0, k0, 34
	dmtc0	k0, COP_0_TLB_LO0		# load PTE entry
	dsll	k1, k1, 34
	dsrl	k1, k1, 34
	dmtc0	k1, COP_0_TLB_LO1		# load PTE entry
	nop
	nop
	nop
	nop
	tlbwr					# write TLB
	nop
	nop
	nop
	nop

#ifdef CPU_LOONGSON2
	li	k0, COP_0_DIAG_ITLB_CLEAR | COP_0_DIAG_BTB_CLEAR
	dmtc0	k0, COP_0_DIAG
#endif

	eret

sys_stk_chk:
	GET_CPU_INFO(k1, k0)
	PTR_L	k1, CI_CURPROCPADDR(k1)
	PTR_SUBU k0, sp, k1			# check to see if we have a
	sltiu	k0, 2048			#  valid kernel stack
	beqz	k0, go_k_general		# yes, handle.
	nop

	LA	a0, start-FRAMESZ(CF_SZ)-4*REGSZ # set sp to a valid place
#ifdef __mips_n64
	mfc0	a4, COP_0_STATUS_REG
	mfc0	a5, COP_0_CAUSE_REG
	move	a6, sp
#else
	mfc0	a2, COP_0_STATUS_REG
	mfc0	a3, COP_0_CAUSE_REG
	REG_S	a2, CF_ARGSZ+0*REGSZ(sp)
	REG_S	a3, CF_ARGSZ+1*REGSZ(sp)
	PTR_S	sp, CF_ARGSZ+2*REGSZ(a0)
#endif
	move	sp, a0
	dmfc0	a1, COP_0_EXC_PC
	move	a2, ra
	LA	a0, 1f
	jal	printf
	dmfc0	a3, COP_0_BAD_VADDR

	LA	sp, start-FRAMESZ(CF_SZ)	# set sp to a valid place

#ifdef DDB
	LA	a0, 2f
	jal	trapDump
	nop
#endif

	PANIC("kernel stack overflow")
	/*noreturn*/

go_k_general:
	j	k_general
	nop

go_u_general:
	j	u_general
	nop


	.data
1:
	.asciiz	"\rktlbmiss: PC %p RA %p ADR %p\nSR %p CR %p SP %p\n"
2:
	.asciiz	"stack ovf"
	.text

	.set	at
END(k_tlb_miss)

#if 0
/*---------------------------------------------------------------- tlb_write_i
 *	Write the given entry into the TLB at the given index.
 */
LEAF(tlb_write_indexed, 0)
	mfc0	v1, COP_0_STATUS_REG		# Save the status register.
	ori	v0, v1, SR_INT_ENAB
	xori	v0, v0, SR_INT_ENAB
	mtc0	v0, COP_0_STATUS_REG		# Disable interrupts
	ITLBNOPFIX
	ld	a2, 16(a1)
	ld	a3, 24(a1)
	dmfc0	ta0, COP_0_TLB_HI		# Save the current PID.

	dmtc0	a2, COP_0_TLB_LO0		# Set up entry low0.
	dmtc0	a3, COP_0_TLB_LO1		# Set up entry low1.
	ld	a2, 0(a1)
	ld	a3, 8(a1)
	mtc0	a0, COP_0_TLB_INDEX		# Set the index.
	mtc0	a2, COP_0_TLB_PG_MASK		# Set up entry mask.
	dmtc0	a3, COP_0_TLB_HI		# Set up entry high.
	nop
	nop
	nop
	nop
	tlbwi					# Write the TLB
	nop
	nop					# Delay for effect
	nop
	nop

#ifdef CPU_LOONGSON2
	li	v0, COP_0_DIAG_ITLB_CLEAR | COP_0_DIAG_BTB_CLEAR
	dmtc0	v0, COP_0_DIAG
#endif

	dmtc0	ta0, COP_0_TLB_HI		# Restore the PID.
	nop
	li	a0, TLB_PAGE_MASK
	mtc0	a0, COP_0_TLB_PG_MASK		# Restore default mask value.
	mtc0	v1, COP_0_STATUS_REG		# Restore the status register
	ITLBNOPFIX
	j	ra
	nop
END(tlb_write_indexed)
#endif

/*---------------------------------------------------------------- tlb_flush
 *	Flush the "random" entries from the TLB.
 *	Uses "wired" register to determine what register to start with.
 *	Arg "tlbsize" is the number of entries to flush.
 */
LEAF(tlb_flush, 0)
	mfc0	v1, COP_0_STATUS_REG	# Save the status register.
	ori	v0, v1, SR_INT_ENAB
	xori	v0, v0, SR_INT_ENAB
	mtc0	v0, COP_0_STATUS_REG	# Disable interrupts
	ITLBNOPFIX
	mfc0	ta1, COP_0_TLB_WIRED
	LA	v0, CKSEG0_BASE		# invalid address
	dmfc0	ta0, COP_0_TLB_HI	# Save the PID

	dmtc0	v0, COP_0_TLB_HI	# Mark entry high as invalid
	dmtc0	zero, COP_0_TLB_LO0	# Zero out low entry0.
	dmtc0	zero, COP_0_TLB_LO1	# Zero out low entry1.
	mtc0	zero, COP_0_TLB_PG_MASK	# Zero out mask entry.
/*
 * Align the starting value (ta1) and the upper bound (a0).
 */
1:
	mtc0	ta1, COP_0_TLB_INDEX	# Set the index register.
	addu	ta1, ta1, 1		# Increment index.
	nop
	nop
	nop
	tlbwi				# Write the TLB entry.
	nop
	nop
	bne	ta1, a0, 1b
	nop

#ifdef CPU_LOONGSON2
	li	v0, COP_0_DIAG_ITLB_CLEAR | COP_0_DIAG_BTB_CLEAR
	dmtc0	v0, COP_0_DIAG
#endif

	dmtc0	ta0, COP_0_TLB_HI	# Restore the PID
	li	a0, TLB_PAGE_MASK
	mtc0	a0, COP_0_TLB_PG_MASK	# Restore default mask value.
	mtc0	v1, COP_0_STATUS_REG	# Restore the status register
	ITLBNOPFIX
	j	ra
	nop
END(tlb_flush)

/*--------------------------------------------------------------- tlb_flush_addr
 *	Flush any TLB entries for the given address and TLB PID.
 */
LEAF(tlb_flush_addr, 0)
	mfc0	v1, COP_0_STATUS_REG	# Save the status register.
	ori	v0, v1, SR_INT_ENAB
	xori	v0, v0, SR_INT_ENAB
	mtc0	v0, COP_0_STATUS_REG	# Disable interrupts
	ITLBNOPFIX
	dli	v0, (PG_HVPN | PG_ASID)
	and	a0, a0, v0		# Make sure valid hi value.
	dmfc0	ta0, COP_0_TLB_HI	# Get current PID
	dmtc0	a0, COP_0_TLB_HI	# look for addr & PID
	nop
	nop
	nop
	nop
	tlbp				# Probe for the entry.
	nop
	nop				# Delay for effect
	nop
	LA	ta1, CKSEG0_BASE	# Load invalid entry.
	mfc0	v0, COP_0_TLB_INDEX	# See what we got
	bltz	v0, 1f			# index < 0 => !found
	nop
	dmtc0	ta1, COP_0_TLB_HI	# Mark entry high as invalid

	dmtc0	zero, COP_0_TLB_LO0	# Zero out low entry.
	dmtc0	zero, COP_0_TLB_LO1	# Zero out low entry.
	nop
	nop
	nop
	nop
	tlbwi
	nop
	nop
	nop
	nop

#ifdef CPU_LOONGSON2
	li	v0, COP_0_DIAG_ITLB_CLEAR | COP_0_DIAG_BTB_CLEAR
	dmtc0	v0, COP_0_DIAG
#endif

1:
	dmtc0	ta0, COP_0_TLB_HI	# restore PID
	mtc0	v1, COP_0_STATUS_REG	# Restore the status register
	ITLBNOPFIX
	j	ra
	nop
END(tlb_flush_addr)

/*---------------------------------------------------------------- tlb_update
 *	Update the TLB if highreg is found; otherwise, enter the data.
 */
LEAF(tlb_update, 0)
	mfc0	v1, COP_0_STATUS_REG	# Save the status register.
	ori	v0, v1, SR_INT_ENAB
	xori	v0, v0, SR_INT_ENAB
	mtc0	v0, COP_0_STATUS_REG	# Disable interrupts
	ITLBNOPFIX
	and	ta1, a0, PG_ODDPG	# ta1 = Even/Odd flag
	dli	v0, (PG_HVPN | PG_ASID)
	and	a0, a0, v0
	dmfc0	ta0, COP_0_TLB_HI	# Save current PID
	dmtc0	a0, COP_0_TLB_HI	# Init high reg
	and	a2, a1, PG_G		# Copy global bit
	li	a3, TLB_PAGE_MASK
	nop
	nop
	nop
	tlbp				# Probe for the entry.
	dsll	a1, a1, 34
	dsrl	a1, a1, 34
	bne	ta1, zero, 2f		# Decide even odd
	mfc0	v0, COP_0_TLB_INDEX	# See what we got
# EVEN
	bltz	v0, 1f			# index < 0 => !found
	nop

	tlbr				# update, read entry first
	nop
	nop
	nop
	dmtc0	a1, COP_0_TLB_LO0	# init low reg0.
	nop
	nop
	nop
	nop
	tlbwi				# update slot found
	b	4f
	li	v0, 1
1:
	mtc0	a3, COP_0_TLB_PG_MASK	# init mask.
	dmtc0	a0, COP_0_TLB_HI	# init high reg.
	dmtc0	a1, COP_0_TLB_LO0	# init low reg0.
	dmtc0	a2, COP_0_TLB_LO1	# init low reg1.
	nop
	nop
	nop
	nop
	tlbwr				# enter into a random slot
	b	4f
	li	v0, 0
# ODD
2:
	nop
	bltz	v0, 3f			# index < 0 => !found
	nop

	tlbr				# read the entry first
	nop
	nop
	nop
	dmtc0	a1, COP_0_TLB_LO1	# init low reg1.
	nop
	nop
	nop
	nop
	tlbwi				# update slot found
	b	4f
	li	v0, 1
3:
	mtc0	a3, COP_0_TLB_PG_MASK	# init mask.
	dmtc0	a0, COP_0_TLB_HI	# init high reg.
	dmtc0	a2, COP_0_TLB_LO0	# init low reg0.
	dmtc0	a1, COP_0_TLB_LO1	# init low reg1.
	nop
	nop
	nop
	nop
	tlbwr				# enter into a random slot
	nop
	li	v0, 0

4:					# Make sure pipeline
	nop				# advances before we
	nop				# use the tlb.

#ifdef CPU_LOONGSON2
	li	v0, COP_0_DIAG_ITLB_CLEAR | COP_0_DIAG_BTB_CLEAR
	dmtc0	v0, COP_0_DIAG
#endif

	dmtc0	ta0, COP_0_TLB_HI	# restore PID
	mtc0	v1, COP_0_STATUS_REG	# Restore the status register
	ITLBNOPFIX
	j	ra
	nop
END(tlb_update)

/*---------------------------------------------------------------- tlb_read
 *	Read the TLB entry.
 */
LEAF(tlb_read, 0)
	mfc0	v1, COP_0_STATUS_REG	# Save the status register.
	ori	v0, v1, SR_INT_ENAB
	xori	v0, v0, SR_INT_ENAB
	mtc0	v0, COP_0_STATUS_REG	# Disable interrupts
	ITLBNOPFIX
	dmfc0	v0, COP_0_TLB_HI	# Get current PID

	mtc0	a0, COP_0_TLB_INDEX	# Set the index register
	nop
	nop
	nop
	nop
	tlbr				# Read from the TLB
	nop
	nop
	nop
	dmfc0	ta0, COP_0_TLB_PG_MASK	# fetch the hi entry
	dmfc0	ta1, COP_0_TLB_HI	# fetch the hi entry
	dmfc0	ta2, COP_0_TLB_LO0	# See what we got
	dmfc0	ta3, COP_0_TLB_LO1	# See what we got
	dmtc0	v0, COP_0_TLB_HI	# restore PID
	nop
	nop
	nop				# wait for PID active
	li	a0, TLB_PAGE_MASK
	mtc0	a0, COP_0_TLB_PG_MASK	# Restore default mask value.
	mtc0	v1, COP_0_STATUS_REG	# Restore the status register
	ITLBNOPFIX
	sd	ta0, 0(a1)
	sd	ta1, 8(a1)
	sd	ta2, 16(a1)
	j	ra
	sd	ta3, 24(a1)
END(tlb_read)

/*---------------------------------------------------------------- tlb_get_pid
 *	Read the tlb pid value.
 */
LEAF(tlb_get_pid, 0)
	dmfc0	v0, COP_0_TLB_HI	# get PID
	li	v1, VMTLB_PID		# mask off PID
	j	ra
	and	v0, v0, v1		# mask off PID
END(tlb_get_pid)

/*---------------------------------------------------------------- tlb_set_pid
 *	Write the given pid into the TLB pid reg.
 */
LEAF(tlb_set_pid, 0)
	dmtc0	a0, COP_0_TLB_HI		# Write the hi reg value
	j	ra
	nop
END(tlb_set_pid)

/*---------------------------------------------------------------- tlb_get_wired
 *	Get the value from the TLB wired reg.
 */
LEAF(tlb_get_wired, 0)
	mfc0	v0, COP_0_TLB_WIRED
	j	ra
	nop
END(tlb_get_wired)

/*---------------------------------------------------------------- tlb_set_wired
 *	Write the given value into the TLB wired reg.
 */
LEAF(tlb_set_wired, 0)
	mtc0	a0, COP_0_TLB_WIRED
	j	ra
	nop
END(tlb_set_wired)

/*
 *	Initialize the TLB page mask.
 */
LEAF(tlb_set_page_mask, 0)
	mtc0	a0, COP_0_TLB_PG_MASK
	j	ra
	nop
END(tlb_set_page_mask)
