/* $OpenBSD: locore.S,v 1.15 2017/02/17 19:14:58 patrick Exp $ */
/*-
 * Copyright (c) 2012-2014 Andrew Turner
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: head/sys/arm64/arm64/locore.S 282867 2015-05-13 18:57:03Z zbb $
 */

#include "assym.h"
#include <sys/syscall.h>
#include <machine/asm.h>
#include <machine/armreg.h>
#include <machine/hypervisor.h>
#include <machine/param.h>
#include <machine/pte.h>

#define	VIRT_BITS	39

#define	DEVICE_MEM	0
#define	NORMAL_UNCACHED	1
#define	NORMAL_MEM	2

/*
 * We assume:
 *  MMU      on with an identity map, or off
 *  D-Cache: off
 *  I-Cache: on or off
 *  We are loaded at a 2MiB aligned address
 */

#define	INIT_STACK_SIZE	(PAGE_SIZE * 4)

	.text
	.globl _start
_start:
	mov x21, x0
	mov x22, x1
	mov x23, x2

	/* Drop to EL1 */
	bl	drop_to_el1

	/*
	 * Disable the MMU. We may have entered the kernel with it on and
	 * will need to update the tables later. If this has been set up
	 * with anything other than a VA == PA map then this will fail,
	 * but in this case the code to find where we are running from
	 * would have also failed.
	 */
	dsb	sy
	mrs	x2, sctlr_el1
	bic	x2, x2, SCTLR_M
	msr	sctlr_el1, x2
	isb

	/* Set the context id */
	msr	contextidr_el1, xzr

	/* Get the virt -> phys offset */
	bl	get_virt_delta

	/* Store symbol value. */
	adr	x0, .Lesym
	ldr	x0, [x0]
	sub	x0, x0, x29
	add	x21, x21, x29
	str	x21, [x0]

	/*
	 * At this point:
	 * x29 = PA - VA
	 * x28 = Our physical load address
	 */

	/* Create the page tables */
	bl	create_pagetables

	/*
	 * At this point:
	 * x27 = TTBR0 table
	 * x26 = TTBR1 table
	 */

	/* Enable the mmu */
	bl	start_mmu

	/* Jump to the virtual address space */
	ldr	x15, .Lvirtdone
	br	x15

.Linitstack:
	.xword initstack
.Linitstack_end:
	.xword initstack_end
virtdone:
	/* Set up the stack */
	adr	x25, .Linitstack_end
	ldr	x25, [x25]
	mov	sp, x25
	mov	x8, #TRAPFRAME_SIZEOF
	sub     x8, x8, (STACKALIGNBYTES)
	and     x8, x8, ~(STACKALIGNBYTES)

	// pass base of kernel stack as proc0
	adr	x25, .Linitstack
	ldr	x25, [x25]

	sub	sp, sp, x8

	/* Zero the BSS */
	ldr	x15, .Lbss
	ldr	x14, .Lend
1:
	str	xzr, [x15], #8
	cmp	x15, x14
	b.lo	1b

	/* Backup the module pointer */
	mov	x1, x0

	/* Make the page table base a virtual address */
	sub	x26, x26, x29

	// XXX - shouldn't this be 8 * 5 (struct grew from 4 -> 5)
	sub	sp, sp, #(64 * 4)
	mov	x0, sp

	/* Negate the delta so it is VA -> PA */
	neg	x29, x29

	str	x1,  [x0]	/* modulep */
	str	x26, [x0, 8]	/* kern_l1pt */
	str	x29, [x0, 16]	/* kern_delta */
	str	x25, [x0, 24]	/* kern_stack */
	str	x21, [x0, 32]	/* ? (x0 arg on boot) */
	str	x22, [x0, 40]	/* ? (x1 arg on boot) */
	str	x23, [x0, 48]	/* fdt (x2 arg on boot) */

	/* trace back starts here */
	mov	fp, #0
	/* Branch to C code */
	bl	initarm
	bl	_C_LABEL(main)

	/* We should not get here */
	brk	0

	.align 3
.Lvirtdone:
	.quad	virtdone
.Lbss:
	.quad	__bss_start
.Lstart:
	.quad	_start
.Lend:
	.quad	_end
.Lcpu_info_primary:
	.quad	_C_LABEL(cpu_info_primary)

/*
 * If we are started in EL2, configure the required hypervisor
 * registers and drop to EL1.
 */
drop_to_el1:
	mrs	x1, CurrentEL
	lsr	x1, x1, #2
	cmp	x1, #0x2
	b.eq	1f
	ret
1:
	/* Configure the Hypervisor */
	mov	x2, #(HCR_RW)
	msr	hcr_el2, x2

	/* Load the Virtualization Process ID Register */
	mrs	x2, midr_el1
	msr	vpidr_el2, x2

	/* Load the Virtualization Multiprocess ID Register */
	mrs	x2, mpidr_el1
	msr	vmpidr_el2, x2

	/* Set the bits that need to be 1 in sctlr_el1 */
	ldr	x2, .Lsctlr_res1
	msr	sctlr_el1, x2

	/* Don't trap to EL2 for exceptions */
	mov	x2, #CPTR_RES1
	msr	cptr_el2, x2

	/* Don't trap to EL2 for CP15 traps */
	msr	hstr_el2, xzr

	/* Enable access to the physical timers at EL1 */
	mrs	x2, cnthctl_el2
	orr	x2, x2, #(CNTHCTL_EL1PCTEN | CNTHCTL_EL1PCEN)
	msr	cnthctl_el2, x2

	/* Set the counter offset to a known value */
	msr	cntvoff_el2, xzr

	/* Hypervisor trap functions */
	adr	x2, hyp_vectors
	msr	vbar_el2, x2

	mov	x2, #(PSR_F | PSR_I | PSR_A | PSR_D | PSR_M_EL1h)
	msr	spsr_el2, x2

	/* Configure GICv3 CPU interface */
	mrs	x2, id_aa64pfr0_el1
	/* Extract GIC bits from the register */
	ubfx	x2, x2, #ID_AA64PFR0_GIC_SHIFT, #ID_AA64PFR0_GIC_BITS
	/* GIC[3:0] == 0001 - GIC CPU interface via special regs. supported */
	cmp	x2, #(ID_AA64PFR0_GIC_CPUIF_EN >> ID_AA64PFR0_GIC_SHIFT)
	b.ne	2f

	mrs	x2, icc_sre_el2
	orr	x2, x2, #ICC_SRE_EL2_EN	/* Enable access from insecure EL1 */
	orr	x2, x2, #ICC_SRE_EL2_SRE	/* Enable system registers */
	msr	icc_sre_el2, x2
2:

	/* Set the address to return to our return address */
	msr	elr_el2, x30
	isb

	eret

	.align 3
.Lsctlr_res1:
	.quad SCTLR_RES1

#define	VECT_EMPTY	\
	.align 7;	\
	1:	b	1b

	.align 11
hyp_vectors:
	VECT_EMPTY	/* Synchronous EL2t */
	VECT_EMPTY	/* IRQ EL2t */
	VECT_EMPTY	/* FIQ EL2t */
	VECT_EMPTY	/* Error EL2t */

	VECT_EMPTY	/* Synchronous EL2h */
	VECT_EMPTY	/* IRQ EL2h */
	VECT_EMPTY	/* FIQ EL2h */
	VECT_EMPTY	/* Error EL2h */

	VECT_EMPTY	/* Synchronous 64-bit EL1 */
	VECT_EMPTY	/* IRQ 64-bit EL1 */
	VECT_EMPTY	/* FIQ 64-bit EL1 */
	VECT_EMPTY	/* Error 64-bit EL1 */

	VECT_EMPTY	/* Synchronous 32-bit EL1 */
	VECT_EMPTY	/* IRQ 32-bit EL1 */
	VECT_EMPTY	/* FIQ 32-bit EL1 */
	VECT_EMPTY	/* Error 32-bit EL1 */

/*
 * Get the delta between the physical address we were loaded to and the
 * virtual address we expect to run from. This is used when building the
 * initial page table.
 */
	.globl get_virt_delta
get_virt_delta:
	/* Load the physical address of virt_map */
	adr	x28, virt_map
	/* Load the virtual address of virt_map stored in virt_map */
	ldr	x29, [x28]
	/* Find PA - VA as PA' = VA' - VA + PA = VA' + (PA - VA) = VA' + x29 */
	sub	x29, x29, x28
	and	x28, x28, #~0x0003ffff // should be 2MB?

	ret

	.align 3
virt_map:
	.quad	virt_map

/*
 * This builds the page tables containing the identity map, and the kernel
 * virtual map.
 *
 * It relys on:
 *  We were loaded to an address that is on a 2MiB boundary
 *  All the memory must not cross a 1GiB boundaty
 *  x28 contains the physical address we were loaded from
 *
 *  There are 3 pages before that address for the page tables
 *  These pages are allocated aligned in .data
 *   The pages used are:
 *    - The identity (PA = VA) table (TTBR0)
 *    - The Kernel L1 table          (TTBR1)
 *    -  The PA == VA L2 table for kernel
 */
.Lpagetable:
	.xword pagetable
.Lpagetable_end:
	.xword pagetable_end

.Lesym:
	.xword esym

create_pagetables:
	/* Save the Link register */
	mov	x5, x30

	/* Clean the page table */
	adr	x6, .Lpagetable
	ldr	x6, [x6]
	sub	x6, x6, x29 // VA -> PA
	mov	x26, x6
	adr	x27, .Lpagetable_end
	ldr	x27, [x27]
	sub	x27, x27, x29 // VA -> PA
1:
	stp	xzr, xzr, [x6], #16
	stp	xzr, xzr, [x6], #16
	stp	xzr, xzr, [x6], #16
	stp	xzr, xzr, [x6], #16
	cmp	x6, x27
	b.lo	1b

	/*
	 * Build the TTBR1 maps.
	 */

	/* Find the size of the kernel */
	adr	x6, .Lstart
	ldr	x6, [x6]
	sub	x6, x6, x29

	/* End is the symbol address */
	adr	x7, .Lesym
	ldr	x7, [x7]
	sub	x7, x7, x29
	ldr	x7, [x7]
	sub	x7, x7, x29

	/* Find the end - begin */
	sub	x8, x7, x6
	/* Get the number of l2 pages to allocate, rounded down */
	lsr	x10, x8, #(L2_SHIFT)
	/* Add 4 MiB for any rounding above and the module data */
	add	x10, x10, #2

	/* Create the kernel space L2 table */
	mov	x6, x26				// pagetable:
	mov	x7, #NORMAL_MEM
	add	x8, x28, x29
	mov	x9, x28
	bl	build_l2_block_pagetable

	/* Move to the l1 table */
	add	x26, x26, #PAGE_SIZE*2		// pagetable_l1_ttbr1:

	/* Link the l1 -> l2 table */
	mov	x9, x6
	mov	x6, x26
	bl	link_l1_pagetable

	/*
	 * Build the TTBR0 maps.
	 */
	add	x27, x26, #PAGE_SIZE * 2	// pagetable_l1_ttbr0:

	mov	x6, x27		/* The initial page table */
#if defined(SOCDEV_PA) && defined(SOCDEV_VA)
	/* Create a table for the UART */
	mov	x7, #DEVICE_MEM
	mov	x8, #(SOCDEV_VA)	/* VA start */
	mov	x9, #(SOCDEV_PA)	/* PA start */
	mov	x10, #1
	bl	build_l1_block_pagetable
#endif

	/* Create the VA = PA map */
	mov	x7, #NORMAL_MEM // #NORMAL
	mov	x9, x27
	mov	x8, x9		/* VA start (== PA start) */
	mov	x10, #1
	bl	build_l1_block_pagetable

	/* Create a mapping for the FDT */
	mov	x7, #NORMAL_MEM // #NORMAL
	mov	x9, x23
	mov	x8, x9		/* VA start (== PA start) */
	mov	x10, #1
	bl	build_l1_block_pagetable

	/* Move to the l0 table */
	add	x27, x27, #PAGE_SIZE * 2	// pagetable_l0_ttbr0:

	/* Link the l0 -> l1 table */
	mov	x9, x6
	mov	x6, x27
	mov	x10, #1
	bl	link_l0_pagetable

	/* Restore the Link register */
	mov	x30, x5
	ret

/*
 * Builds an L0 -> L1 table descriptor
 *
 * This is a link for a 512GiB block of memory with up to 1GiB regions mapped
 * within it by build_l1_block_pagetable.
 *
 *  x6  = L0 table
 *  x8  = Virtual Address
 *  x9  = L1 PA (trashed)
 *  x10 = Entry count
 *  x11, x12 and x13 are trashed
 */
link_l0_pagetable:
	/*
	 * Link an L0 -> L1 table entry.
	 */
	/* Find the table index */
	lsr	x11, x8, #L0_SHIFT
	and	x11, x11, #Ln_ADDR_MASK

	/* Build the L0 block entry */
	mov	x12, #L0_TABLE

	/* Only use the output address bits */
	lsr	x9, x9, #PAGE_SHIFT
1:	orr	x13, x12, x9, lsl #PAGE_SHIFT

	/* Store the entry */
	str	x13, [x6, x11, lsl #3]

	sub	x10, x10, #1
	add	x11, x11, #1
	add	x9, x9, #1
	cbnz	x10, 1b

	ret

/*
 * Builds an L1 -> L2 table descriptor
 *
 * This is a link for a 1GiB block of memory with up to 2MiB regions mapped
 * within it by build_l2_block_pagetable.
 *
 *  x6  = L1 table
 *  x8  = Virtual Address
 *  x9  = L2 PA (trashed)
 *  x11, x12 and x13 are trashed
 */
link_l1_pagetable:
	/*
	 * Link an L1 -> L2 table entry.
	 */
	/* Find the table index */
	lsr	x11, x8, #L1_SHIFT
	and	x11, x11, #Ln_ADDR_MASK

	/* Build the L1 block entry */
	mov	x12, #L1_TABLE

	/* Only use the output address bits */
	lsr	x9, x9, #PAGE_SHIFT
	orr	x13, x12, x9, lsl #PAGE_SHIFT

	/* Store the entry */
	str	x13, [x6, x11, lsl #3]

	ret

/*
 * Builds count 1 GiB page table entry
 *  x6  = L1 table
 *  x7  = Type (0 = Device, 1 = Normal)
 *  x8  = VA start
 *  x9  = PA start (trashed)
 *  x10 = Entry count
 *  x11, x12 and x13 are trashed
 */
build_l1_block_pagetable:
	/*
	 * Build the L1 table entry.
	 */
	/* Find the table index */
	lsr	x11, x8, #L1_SHIFT
	and	x11, x11, #Ln_ADDR_MASK

	/* Build the L1 block entry */
	lsl	x12, x7, #2
	orr	x12, x12, #L1_BLOCK
	orr	x12, x12, #(ATTR_AF)
	orr	x12, x12, ATTR_SH(SH_INNER)

	/* Only use the output address bits */
	lsr	x9, x9, #L1_SHIFT

	/* Set the physical address for this virtual address */
1:	orr	x13, x12, x9, lsl #L1_SHIFT

	/* Store the entry */
	str	x13, [x6, x11, lsl #3]

	sub	x10, x10, #1
	add	x11, x11, #1
	add	x9, x9, #1
	cbnz	x10, 1b

	ret

/*
 * Builds count 2 MiB page table entry
 *  x6  = L2 table
 *  x7  = Type (0 = Device, 1 = Normal)
 *  x8  = VA start
 *  x9  = PA start (trashed)
 *  x10 = Entry count
 *  x11, x12 and x13 are trashed
 */
build_l2_block_pagetable:
	/*
	 * Build the L2 table entry.
	 */
	/* Find the table index */
	lsr	x11, x8, #L2_SHIFT
	and	x11, x11, #Ln_ADDR_MASK

	/* Build the L2 block entry */
	lsl	x12, x7, #2
	orr	x12, x12, #L2_BLOCK
	orr	x12, x12, #(ATTR_AF)
	orr	x12, x12, ATTR_SH(SH_INNER)

	/* Only use the output address bits */
	lsr	x9, x9, #L2_SHIFT

	/* Set the physical address for this virtual address */
1:	orr	x13, x12, x9, lsl #L2_SHIFT

	/* Store the entry */
	str	x13, [x6, x11, lsl #3]

	sub	x10, x10, #1
	add	x11, x11, #1
	add	x9, x9, #1
	cbnz	x10, 1b

	ret

start_mmu:
	dsb	sy

	/* Load the exception vectors */
	ldr	x2, =exception_vectors
	msr	vbar_el1, x2

	/* Load ttbr0 and ttbr1 */
	msr	ttbr0_el1, x27
	msr	ttbr1_el1, x26
	isb

	/* Clear the Monitor Debug System control register */
	msr	mdscr_el1, xzr

	/* Invalidate the TLB */
	tlbi	vmalle1is

	ldr	x2, mair
	msr	mair_el1, x2

	/*
	 * Setup TCR according to PARange bits from ID_AA64MMFR0_EL1.
	 * Some machines have physical memory mapped >512GiB, which can not
	 * be identity-mapped using the default 39 VA bits. Thus, use
	 * 48 VA bits for now and switch back to 39 after the VA jump.
	 */
	ldr	x2, tcr
	mrs	x3, id_aa64mmfr0_el1
	bfi	x2, x3, #32, #3
	msr	tcr_el1, x2

	/* Setup SCTLR */
	ldr	x2, sctlr_set
	ldr	x3, sctlr_clear
	mrs	x1, sctlr_el1
	bic	x1, x1, x3	/* Clear the required bits */
	orr	x1, x1, x2	/* Set the required bits */
	msr	sctlr_el1, x1
	isb

	ret
	.globl switch_mmu_kernel
switch_mmu_kernel:
	dsb	sy
	/* Invalidate the TLB */
	tlbi	vmalle1is
	/* Load ttbr1 (kernel) */
	msr	ttbr1_el1, x0
	isb
	ret



	.align 3
mair:
	/* Device | Normal (no cache, write-back, write-through) */
	.quad	MAIR_ATTR(0x00, 0) |	\
		MAIR_ATTR(0x44, 1) |	\
		MAIR_ATTR(0xff, 2) |	\
		MAIR_ATTR(0x88, 3)
tcr:
	.quad (TCR_T1SZ(64 - VIRT_BITS) | TCR_T0SZ(64 - 48) | \
	    TCR_ASID_16 | TCR_TG1_4K | TCR_CACHE_ATTRS | TCR_SMP_ATTRS)
sctlr_set:
	/* Bits to set */
	.quad (SCTLR_UCI | SCTLR_nTWE | SCTLR_nTWI | SCTLR_UCT | SCTLR_DZE | \
	    SCTLR_I | SCTLR_SED | SCTLR_C | SCTLR_M)
sctlr_clear:
	/* Bits to clear */
	.quad (SCTLR_EE | SCTLR_EOE | SCTLR_WXN | SCTLR_UMA | SCTLR_ITD | \
	    SCTLR_THEE | SCTLR_CP15BEN | SCTLR_SA0 | SCTLR_SA | SCTLR_A)

	.globl abort
abort:
	b abort

	// First entries in data must be esym
	// so that bootloader can find them easily.
	.data
	.global _C_LABEL(esym)
_C_LABEL(esym): .xword   _C_LABEL(end)

	//.section .init_pagetable
data_align_pad:
	.space 32
	.align 12 /* 4KiB aligned */
	/*
	 * 3 initial tables (in the following order):
	 *           L2 for kernel (High addresses)
	 *           L1 for kernel
	 *           L1 for user   (Low addresses)
	 */
	.globl	pagetable
pagetable:
	.space	PAGE_SIZE * 2	// allocate 2 pages for pmapvp2
pagetable_l1_ttbr1:
	.space	PAGE_SIZE * 2	// allocate 2 pages for pmapvp1
pagetable_l1_ttbr0:
	.space	PAGE_SIZE * 2	// allocate 2 pages, reused later in pmap
pagetable_l0_ttbr0:
	.space	PAGE_SIZE
pagetable_end:

	.text
#if 0
	.globl init_pt_va
init_pt_va:
	.quad pagetable		/* XXX: Keep page tables VA */
#endif

	.bss
	.align	4
	.globl initstack
initstack:
	.space	USPACE
initstack_end:

	.text
ENTRY(sigcode)
	mov     x0, sp
	add     x0, x0, #SF_SC

1:
	mov     x8, #SYS_sigreturn
	svc     0
	.globl  _C_LABEL(sigcoderet)
_C_LABEL(sigcoderet):

	/* sigreturn failed, exit */
	mov     x8, #SYS_exit
	svc     0

	b       1b
END(sigcode)
	/* This may be copied to the stack, keep it 16-byte aligned */
	.align  3
        .global _C_LABEL(esigcode)
_C_LABEL(esigcode):

	.globl	sigfill
sigfill:
	.word	0xa000f7f0		/* FIXME: illegal on all cpus? */
esigfill:

	.data
	.globl	sigfillsiz
sigfillsiz:
	.word	esigfill - sigfill

	.text
