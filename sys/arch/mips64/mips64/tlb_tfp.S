/*	$OpenBSD: tlb_tfp.S,v 1.4 2015/09/23 19:13:55 miod Exp $	*/

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

#include <machine/param.h>
#include <machine/asm.h>
#include <machine/cpu.h>
#include <mips64/mips_cpu.h>
#include <machine/pte.h>

#include "assym.h"

#ifdef MIPS_PTE64
#error "R8000 doesn't need 64-bit PTE"
#endif

#define	TLBR	.align 4; .word 0x43000001; SSNOP; SSNOP; SSNOP; SSNOP
#define	TLBW	.align 4; .word 0x43000002; SSNOP; SSNOP; SSNOP
#define	TLBP	.align 4; .word 0x43000008; SSNOP; SSNOP; SSNOP; SSNOP

#ifdef TLB_DEBUG
#define TLBOP(crounha)	Xtlb_##crounha
#else
#define TLBOP(crounha)	tlb_##crounha
#endif

	.set	mips4
	.set	noreorder

/*
 * void tlb_flush(int);
 * Flush all the TLB entries. Argument is ignored (128x3 layout is hardcoded).
 * Assumes there are no wired entries.
 */
LEAF(TLBOP(flush), 0)	/* { */
	DMFC0	v1, COP_0_STATUS_REG	# Save the status register.
	MFC0_HAZARD
	ori	v0, v1, SR_INT_ENAB
	xori	v0, SR_INT_ENAB
	DMTC0	v0, COP_0_STATUS_REG	# Disable interrupts
	MTC0_SR_IE_HAZARD

	LI	v0, 01
	dsll	v0, v0, 62		# KV0 base
	DMFC0	a2, COP_0_TLB_HI	# Read current ASID
	MFC0_HAZARD
	LI	a1, 2			# TLB set counter
1:
	DMTC0	a1, COP_0_TLB_SET
	MTC0_HAZARD
	LI	a0, 127			# TLB index counter
2:
	DMTC0	zero, COP_0_TLB_LO
	MTC0_HAZARD
	DMTC0	v0, COP_0_TLB_HI
	MTC0_HAZARD
	DMTC0	v0, COP_0_VADDR
	MTC0_HAZARD

	TLBW
	beqz	a0, 3f
	 addu	v0, PAGE_SIZE		# Make sure no duplicates will exist
	b	2b
	 subu	a0, 1
3:
	beqz	a1, 4f
	 NOP
	b	1b
	 subu	a1, 1
4:
	DMTC0	a2, COP_0_TLB_HI	# Restore current ASID
	MTC0_HAZARD
	MTC0_SR_IE_HAZARD
	DMTC0	v1, COP_0_STATUS_REG	# Restore the status register
	MTC0_SR_IE_HAZARD

	j	ra
	 NOP
END(TLBOP(flush))	/* } */

/*
 * void tlb_flush_addr(vaddr_t);
 * Flush matching TLB entry, if any, for the given address and ASID (the
 * ASID bits are in the same position as in EntryHi).
 */
LEAF(TLBOP(flush_addr), 0)	/* { */
	DMFC0	v1, COP_0_STATUS_REG	# Save the status register.
	MFC0_HAZARD
	ori	v0, v1, SR_INT_ENAB
	xori	v0, v0, SR_INT_ENAB
	DMTC0	v0, COP_0_STATUS_REG	# Disable interrupts
	MTC0_SR_IE_HAZARD
	DMFC0	a2, COP_0_TLB_HI	# Read current ASID
	MFC0_HAZARD

	/*
	 * NOTE: I used to only write the ASID bits of TLB_HI and not bother
	 * about the VPN bits (since we are about to invalidate that entry
	 * anyway), with this:
	 *	and	a1, a0, PG_ASID_MASK
	 *	DMTC0	a1, COP_0_TLB_HI
	 *
	 * It turns out that this was a VERY BAD idea. Doing this seems to
	 * work, but leaves the TLB in an inconsistent state (although
	 * reading the TLB entry afterwards does not report anything wrong),
	 * and eventually causes TLBX exceptions - although, at the time of
	 * the exception, the TLB values obtained by TLBR are consistent and
	 * no duplicate is to be found.
	 * -- miod
	 */
	DMTC0	a0, COP_0_TLB_HI
	MTC0_HAZARD
	DMTC0	a0, COP_0_VADDR
	MTC0_HAZARD

	TLBP
	DMFC0	v0, COP_0_TLB_SET
	MFC0_HAZARD

	bltz	v0, 1f			# Missing
	 NOP

	DMTC0	zero, COP_0_TLB_LO
	MTC0_HAZARD
	TLBW

1:
	DMTC0	a2, COP_0_TLB_HI	# restore PID
	MTC0_HAZARD
	DMTC0	v1, COP_0_STATUS_REG	# Restore the status register
	MTC0_SR_IE_HAZARD
	j	ra
	 NOP
END(TLBOP(flush_addr))	/* } */

/*
 * void tlb_update(vaddr_t, register_t);
 * Update (or create) TLB entry for given address and ASID, and PTE.
 */
LEAF(TLBOP(update), 0)	/* { */
	DMFC0	v1, COP_0_STATUS_REG	# Save the status register.
	MFC0_HAZARD
	ori	v0, v1, SR_INT_ENAB
	xori	v0, v0, SR_INT_ENAB
	DMTC0	v0, COP_0_STATUS_REG	# Disable interrupts
	MTC0_SR_IE_HAZARD

	DMFC0	a2, COP_0_TLB_HI	# Read current ASID
	MFC0_HAZARD
	/*
	 * If setting up a kernel mapping (va < 0), merge the ASID field
	 * so that we do not need to write it back.
	 */
	bgez	a0, 1f
	 andi	a2, PG_ASID_MASK
	or	a0, a2
1:

	DMTC0	a0, COP_0_TLB_HI
	MTC0_HAZARD
	DMTC0	a0, COP_0_VADDR
	MTC0_HAZARD

	TLBP
	DMFC0	v0, COP_0_TLB_SET
	MFC0_HAZARD

	bgez	v0, 2f

	/*
	 * Page not found: pick a random set. This algorithm is biased as it
	 * will select set #0 with a 1/2 probability, and sets #1 and #2 with
	 * 1/4 probability. But it's simple, fast, and still better than
	 * always picking set #0.
	 */
	 li	a3, 1			# delay slot from above
	DMFC0	v0, COP_0_COUNT
	MFC0_HAZARD
	and	v0, 3
	movz	a3, zero, v0		# a3 = v0 == 0 ? 0 : 1
	subu	v0, a3
	DMTC0	v0, COP_0_TLB_SET
	MTC0_HAZARD

2:
	DMTC0	a1, COP_0_TLB_LO
	MTC0_HAZARD
	TLBW

	blez	a0, 3f
	 NOP
	DMTC0	a2, COP_0_TLB_HI	# restore ASID
	MTC0_HAZARD
3:
	DMTC0	v1, COP_0_STATUS_REG	# Restore the status register
	MTC0_SR_IE_HAZARD

	j	ra
	 NOP
END(TLBOP(update))	/* } */

/*
 * void tlb_read(int, struct tlb_entry *);
 * Read the given TLB entry. On R8000, only the tlb_hi and tlb_lo0 fields are
 * filled.
 */
LEAF(tlb_read, 0)	/* { */
	DMFC0	v1, COP_0_STATUS_REG	# Save the status register.
	MFC0_HAZARD
	ori	v0, v1, SR_INT_ENAB
	xori	v0, v0, SR_INT_ENAB
	DMTC0	v0, COP_0_STATUS_REG	# Disable interrupts
	MTC0_SR_IE_HAZARD

	DMFC0	a3, COP_0_TLB_HI	# Get current PID
	MFC0_HAZARD

	DMTC0	zero, COP_0_TLB_HI	# Force ASID to zero so we don't need
	MTC0_HAZARD			# to hash the index

	dsrl	a2, a0, 7
	DMTC0	a2, COP_0_TLB_SET
	MTC0_HAZARD
	dsll	a0, PAGE_SHIFT
	DMTC0	a0, COP_0_VADDR
	MTC0_HAZARD

	TLBR

	DMFC0	a0, COP_0_TLB_HI
	MFC0_HAZARD
	DMFC0	a2, COP_0_TLB_LO
	MFC0_HAZARD

	DMTC0	a3, COP_0_TLB_HI
	MTC0_HAZARD
	DMTC0	v1, COP_0_STATUS_REG	# Restore the status register
	MTC0_SR_IE_HAZARD

	sd	a0, 8(a1)		# tlb_hi
	j	ra
	 sd	a2, 16(a1)		# tlb_lo0
END(tlb_read)	/* } */

/*
 * uint tlb_get_pid(void);
 * Read the current TLB ASID.
 */
LEAF(tlb_get_pid, 0)	/* { */
	DMFC0	v0, COP_0_TLB_HI	# get PID
	MFC0_HAZARD
	and	v0, PG_ASID_MASK
	j	ra
	 dsrl	v0, PG_ASID_SHIFT
END(tlb_get_pid)	/* } */

/*
 * void tlb_set_pid(uint);
 * Set the current TLB ASID.
 */
LEAF(TLBOP(set_pid), 0)	/* { */
	DMFC0	v1, COP_0_STATUS_REG	# Save the status register.
	MFC0_HAZARD
	ori	v0, v1, SR_INT_ENAB
	xori	v0, v0, SR_INT_ENAB
	DMTC0	v0, COP_0_STATUS_REG	# Disable interrupts
	MTC0_SR_IE_HAZARD

	dsll	a1, a0, ICACHE_ASID_SHIFT
	dsll	a0, PG_ASID_SHIFT
	DMTC0	a0, COP_0_TLB_HI
	MTC0_HAZARD
	DMTC0	a1, COP_0_ICACHE
	MTC0_HAZARD

	DMTC0	v1, COP_0_STATUS_REG	# Restore the status register
	MTC0_SR_IE_HAZARD

	j	ra
	 NOP
END(TLBOP(set_pid))	/* } */

/*
 * void tlb_set_wired(uint32_t);
 */
LEAF(tlb_set_wired, 0)	/* { */
	DMTC0	a0, COP_0_TFP_TLB_WIRED
	MTC0_HAZARD
	j	ra
	 NOP
END(tlb_set_wired)	/* } */

/*
 * void tlb_set_gbase(vaddr_t, vsize_t);
 */
LEAF(tlb_set_gbase, 0)	/* { */
	DMTC0	a0, COP_0_GBASE
	MTC0_HAZARD
	DMTC0	a1, COP_0_WORK1
	MTC0_HAZARD
	j	ra
	 NOP
END(tlb_set_gbase)	/* } */
