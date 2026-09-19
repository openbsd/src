/*	$OpenBSD: hibernate_asm.S,v 1.2 2026/09/19 17:27:09 kettenis Exp $ */

/*
 * Copyright (c) 2026 Marcus Glocker <mglocker@openbsd.org>
 * Copyright (c) 2026 Mark Kettenis <kettenis@openbsd.org>
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

#include <machine/asm.h>
#include <machine/param.h>
#include <machine/hibernate_var.h>

	.text

/*
 * Restore the suspending kernel's retguard area and MMU context,
 * then restart the OS by long-jumping back into gosleep().
 *
 * x0 in: source VA (global_piglet_va + 110*PAGE_SIZE).
 *
 * DAIF is intentionally NOT restored -- the outer caller
 * re-establishes IRQ mask state.
 */
ENTRY(hibernate_resume_machdep)
	bl	hibernate_retguard_copy_machdep

	adrp	x0, hibernate_tcr
	ldr	x0, [x0, :lo12:hibernate_tcr]
	msr	tcr_el1, x0
	adrp	x0, hibernate_ttbr0
	ldr	x0, [x0, :lo12:hibernate_ttbr0]
	msr	ttbr1_el1, x0
	adrp	x0, hibernate_ttbr1
	ldr	x0, [x0, :lo12:hibernate_ttbr1]
	msr	ttbr1_el1, x0
	isb

	dsb	ish
	tlbi	vmalle1is
	dsb	ish
	isb

	ic	ialluis
	dsb	ish
	isb

	bl	hibernate_dcache_flush_machdep

	adrp	x0, hibernate_jmpbuf
	add	x0, x0, :lo12:hibernate_jmpbuf
	b	longjmp
END(hibernate_resume_machdep)

/*
 * Switch to the resume-time page table.  Force T0SZ = 16 to match the
 * 4-level walk the resume PT was built with.
 */
ENTRY(hibernate_activate_resume_pt_machdep)
	RETGUARD_SETUP(hibernate_activate_resume_pt_machdep, x15)

	mrs	x1, tcr_el1
	bic	x1, x1, #0x3f
	orr	x1, x1, #16
	msr	tcr_el1, x1
	isb

	adrp	x0, hibernate_resume_ttbr0
	ldr	x0, [x0, :lo12:hibernate_resume_ttbr0]
	dsb	ish
	msr	ttbr0_el1, x0
	isb

	adrp	x0, hibernate_resume_ttbr1
	ldr	x0, [x0, :lo12:hibernate_resume_ttbr1]
	dsb	ish
	msr	ttbr1_el1, x0
	isb

	dsb	ish
	tlbi	vmalle1is
	dsb	ish
	isb

	RETGUARD_CHECK(hibernate_activate_resume_pt_machdep, x15)
	ret
END(hibernate_activate_resume_pt_machdep)

/* Switch to the private resume-time hibernate stack */
ENTRY(hibernate_switch_stack_machdep)
	RETGUARD_SETUP(hibernate_switch_stack_machdep, x15)
	adrp	x0, hibernate_stack_top_va
	ldr	x0, [x0, :lo12:hibernate_stack_top_va]
	mov	sp, x0
	RETGUARD_CHECK(hibernate_switch_stack_machdep, x15)
	ret
END(hibernate_switch_stack_machdep)

/* Broadcast TLBI (no ASID-less single-page primitive on arm64) */
ENTRY(hibernate_flush)
	RETGUARD_SETUP(hibernate_flush, x15)
	dsb	ish
	tlbi	vmalle1is
	dsb	ish
	isb
	RETGUARD_CHECK(hibernate_flush, x15)
	ret
END(hibernate_flush)

/*
 * Clean+invalidate all D-cache levels (set/way) so unpack's TTBR0-side
 * writes are visible to subsequent TTBR1-side reads.  Trashes x0..x10.
 */
ENTRY(hibernate_dcache_flush_machdep)
	RETGUARD_SETUP(hibernate_dcache_flush_machdep, x15)
	mrs	x0, clidr_el1
	and	x3, x0, #0x7000000
	lsr	x3, x3, #23
	cbz	x3, 5f
	mov	x10, #0
1:      add	x2, x10, x10, lsr #1
	lsr	x1, x0, x2
	and	x1, x1, #7
	cmp	x1, #2
	b.lt	4f
	msr	csselr_el1, x10
	isb
	mrs	x1, ccsidr_el1
	and	x2, x1, #7
	add	x2, x2, #4
	mov	x4, #0x3ff
	and	x4, x4, x1, lsr #3
	clz	w5, w4
	mov	x6, #0x7fff
	and	x6, x6, x1, lsr #13
2:      mov	x7, x4
3:      lsl	x8, x7, x5
	orr	x8, x10, x8
	lsl	x9, x6, x2
	orr	x8, x8, x9
	dc	cisw, x8
	subs	x7, x7, #1
	b.ge	3b
	subs	x6, x6, #1
	b.ge	2b
4:      add	x10, x10, #2
	cmp	x3, x10
	b.gt	1b
5:      dsb	sy
	isb
	RETGUARD_CHECK(hibernate_dcache_flush_machdep, x15)
	ret
END(hibernate_dcache_flush_machdep)

/*
 * Copy the suspender's retguard snapshot from its piglet holding slot
 * back into the retguard PAs that HIB_MOVE preserved during unpack.
 * Called from hibernate_unpack_image after the inflate loop and before
 * hibernate_resume_machdep().  Mirrors the inline block inside amd64's
 * hibernate_resume_machdep (acpi_wakecode.S:365-400).
 *
 * x0 in: source VA (global_piglet_va + 110*PAGE_SIZE).
 *
 * Runs with the resume PT active: retguard PAs are identity-mapped R/W
 * via TTBR0 (see hibernate_populate_resume_pt), so retguard_start_phys
 * is used directly as the write address.
 */
ENTRY(hibernate_retguard_copy_machdep)
	adrp	x1, __retguard_start
	add	x1, x1, :lo12:__retguard_start
	adrp	x2, __retguard_end
	add	x2, x2, :lo12:__retguard_end
	sub	x2, x2, x1				/* byte count */
	lsr	x2, x2, #3				/* quadword count */
	cbz	x2, 2f
1:	ldr	x3, [x0], #8
	str	x3, [x1], #8
	subs	x2, x2, #1
	b.ne	1b
2:	dsb	sy
	isb
	ret
END(hibernate_retguard_copy_machdep)
