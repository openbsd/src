/*	$OpenBSD: locore.S,v 1.12 2021/06/29 21:27:53 kettenis Exp $	*/

/*-
 * Copyright (c) 2015-2018 Ruslan Bukin <br@bsdpad.com>
 * All rights reserved.
 *
 * Portions of this software were developed by SRI International and the
 * University of Cambridge Computer Laboratory under DARPA/AFRL contract
 * FA8750-10-C-0237 ("CTSRD"), as part of the DARPA CRASH research programme.
 *
 * Portions of this software were developed by the University of Cambridge
 * Computer Laboratory as part of the CTSRD Project, with support from the
 * UK Higher Education Innovation Fund (HEIF).
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
 */

#include <assym.h>
#include <sys/syscall.h>
#include <machine/asm.h>
#include <machine/param.h>
#include <machine/vmparam.h>
#include <machine/trap.h>
#include <machine/riscvreg.h>
#include <machine/pte.h>


	.globl	kernbase
	.set	kernbase, KERNBASE

	/* Reset vector */
	.text
	.globl _start_kern_bootstrap
_start_kern_bootstrap:

	/* Set the global pointer */
.option push
.option norelax
	lla	gp, __global_pointer$
.option pop

	/* Get the physical address kernel loaded to */
	lla	t0, virt_map	//virt_map is a phy addr where its own va is stored.
	ld	t1, 0(t0)
	sub	t1, t1, t0	//t1 = t1 - t0 = va - pa, va<-->pa offset
	li	t2, KERNBASE	//KERNBASE is virtual addr
	sub	s9, t2, t1	//s9 = physmem base of kernel

	/*
	 * EFIBOOT passes:
	 * a0 = esym
	 * a1 = 0
	 * a2 = dtb
	 */

	la t0, esym
	add a3, a0, t1
	sd a3, 0(t0)
	mv a1, a2

	/*
	 * Page tables
	 */

	/* step0) Identity map kernel @ 0x80000000 */
	/* Build PTE for 1 GiB identity-mapped gigapage */
	lla	s1, pagetable_l1	//phy addr
	mv	s2, s9			//phy addr
	li	t0, 0xffffffffc0000000
	and	s2, s2, t0
	srli	s2, s2, PAGE_SHIFT	//12, 4K page
	slli	t5, s2, PTE_PPN0_S	//10bit Phys Mem Attribute
	li	t4, (PTE_KERN | PTE_X)	//flag bit
	or	t6, t4, t5		//t6 now is PTE for a 1 GiB gigapage

	/* Calculate VPN[2] for 1 GiB identity-mapped gigapage */
	mv	a5, s9			//va -- identity mapped
	srli	a5, a5, L1_SHIFT	//30, remaining va[63:30]
	andi	a5, a5, 0x1ff		//only use va[38:30] as VPN[2], =0x002

	/* Store L1 PTE entry */
	li	a6, PTE_SIZE
	mulw	a5, a5, a6
	add	t0, s1, a5
	mv	s8, t0		// Store address in L1 Page Table to unmap later
	sd	t6, (t0)

	/* step1) Add L1 entry for kernel */

	//calc PTE based on pa
	lla	s1, pagetable_l1	//phy addr
	lla	s2, pagetable_l2
	srli	s2, s2, PAGE_SHIFT	//12, 4K page
	slli	t5, s2, PTE_PPN0_S	//10bit Phys Mem Attribute
	li	t4, PTE_V		//PTE valid
	or	t6, t4, t5		//t6 now is the PTE for a level 2 page table

	//calc VPN[2] based on va
	li	a5, KERNBASE		//va
	srli	a5, a5, L1_SHIFT	//30
	andi	a5, a5, 0x1ff		//va[38:30] as VPN[2],==0x100

	/* Store L1 PTE entry */
	li	a6, PTE_SIZE		//8 Bytes
	mulw	a5, a5, a6		//distance in unit of bytes
	add	t0, s1, a5		//s1 is L1 table base pa
	sd	t6, (t0)		//PTE of a l2 page table is populated to l1 page table


	/* step2) Level 2 superpages (512 x 2MiB) */ //mega pages: two-level page table
	lla	s1, pagetable_l2
	//calc PTE
	srli	t4, s9, L2_SHIFT	/* Div physmem base by 2 MiB */
	li	t2, 512			/* Build totally 512 entries */
	add	t3, t4, t2
	li	t5, 0
2:
	li	t0, (PTE_KERN | PTE_X)
	slli	t2, t4, PTE_PPN1_S	//19
	or	t5, t0, t2		//PTE contructed
	sd	t5, (s1)		/* Store PTE entry to position */

	//iterating
	addi	s1, s1, PTE_SIZE
	addi	t4, t4, 1
	bltu	t4, t3, 2b		//512 entries


	/* step3) Create an L1 entry for early devmap */
	lla	s1, pagetable_l1	//pa

	//calculate PTE based on pa: (PPN) + privilege/permission + ...
	lla	s2, pagetable_l2_devmap	/* Link to next level PN */ //pa
	srli	s2, s2, PAGE_SHIFT	//12, --> PPN
	slli	t5, s2, PTE_PPN0_S	//10, PMA
	li	t4, PTE_V
	or	t6, t4, t5		//PTE contructed

	//calculate VPN[2] index based on va
	li	a5, (VM_MAX_KERNEL_ADDRESS - L2_SIZE)//devmap is at kernel mem top, va
	srli	a5, a5, L1_SHIFT	//30
	andi	a5, a5, 0x1ff		//index using va[38:30]

	/* Store single level1 PTE entry to position */
	li	a6, PTE_SIZE
	mulw	a5, a5, a6		//offset in Bytes
	add	t0, s1, a5		//find the physical add to write
	sd	t6, (t0)


	/* step4) Create ONE L2 superpage 2MB for DTB */
	lla	s1, pagetable_l2_devmap

	//calc PTE based on pa
	mv	s2, a1			//dtb
	li	t0, 0xffffffffffe00000
	and	s2, s2, t0
	srli	s2, s2, PAGE_SHIFT	//12
	slli	t2, s2, PTE_PPN0_S	//10
	li	t0, (PTE_KERN)
	or	t0, t0, t2		//PTE contructed

	/* Store PTE entry to position */
	li	a6, PTE_SIZE
	li	a5, 510
	mulw	a5, a5, a6
	add	t1, s1, a5
	sd	t0, (t1)

	/* Page tables END */


	/* Calculate virtual address of the first instruction after enable paging */
	lla	s3, va			//va is a physical addr!
	sub	s3, s3, s9		//offset comparing to phymem base
	li	t0, KERNBASE		//virt addr
	add	s3, s3, t0

	/* Set page tables base register */
	lla	s2, pagetable_l1	//pa
	srli	s2, s2, PAGE_SHIFT	//12, --> PPN
	li	t0, SATP_MODE_SV39	//satp[63:60] = 1000b, enable paging!
	or	s2, s2, t0
	sfence.vma			//Supervisor Fence for Virtual Memory, to flush TLB
	csrw	satp, s2
	jr	s3

	.align 2
va:
	/* Set the global pointer again, this time with the virtual address. */
.option push
.option norelax
	lla	gp, __global_pointer$
.option pop

	/* Unmap the identity mapped kernel gigapage */
	sd	x0, (s8)	// s8 is addr of pte for identity mapped kernel
	sfence.vma		// Flush the TLB. Goodbye identity mapped kernel!

	/* Setup supervisor trap vector */
	la	t0, cpu_exception_handler
	csrw	stvec, t0

	/* Ensure sscratch is zero */
	li	t0, 0
	csrw	sscratch, t0

	/* Initialize stack pointer */
	la	s3, initstack_end
	mv	sp, s3

	/* Allocate space for thread0 PCB and riscv_bootparams */
	addi	sp, sp, -(PCB_SIZEOF + RISCV_BOOTPARAMS_SIZEOF) & ~STACKALIGNBYTES

	/* Clear BSS */
	la	s0, _C_LABEL(__bss_start)
	la	s1, _C_LABEL(_end)
1:
	sd	zero, 0(s0)
	addi	s0, s0, 8
	bltu	s0, s1, 1b

	/* Fill riscv_bootparams */
	addi	sp, sp, -RISCV_BOOTPARAMS_SIZEOF

	la	t0, pagetable_l1
	sd	t0, KERN_L1PT(sp)

	li	t0, KERNBASE
	sub	t0, s9, t0	//offset: PA - VA, used in pmap_*
	sd	t0, KERN_DELTA(sp)

	la	t0, initstack
	sd	t0, KERN_STACK(sp)

	li	t0, (VM_MAX_KERNEL_ADDRESS - 2 * L2_SIZE) // XXX Why 2?
	li	t1, 0x1fffff
	and	t1, a1, t1
	add	t0, t0, t1
	sd	t0, DTBP_VIRT(sp)
	sd	a1, DTBP_PHYS(sp)

	mv	a0, sp			//stack setup, can call C now!
	call	_C_LABEL(initriscv)	// Off we go, defined in machdep.c
	call	_C_LABEL(main)		//defined in openbsd/kern/init_main.c

/*
 * Get the physical address the kernel is loaded to. Returned in s9.
 */
get_physmem:
	lla	t0, virt_map	/* physical address of virt_map */
	ld	t1, 0(t0)	/* virtual address of virt_map */
	sub	t1, t1, t0	/* calculate phys->virt delta */
	li	t2, KERNBASE
	sub	s9, t2, t1	/* s9 = physmem base */
	ret

	.data
	.align	4
initstack:
	.space	USPACE
initstack_end:

	.globl	sigfill
sigfill:
	unimp
esigfill:
	.globl	sigfillsiz
sigfillsiz:
	.data
	.word	esigfill - sigfill

	.text
ENTRY(sigcode)
	mv	a0, sp
	addi	a0, a0, SF_SC //actual saved context
	li	t0, SYS_sigreturn
	ecall
	.globl _C_LABEL(sigcoderet)
_C_LABEL(sigcoderet):
	/* sigreturn failed, exit */
	li	t0, SYS_exit
	ecall
END(sigcode)
	.globl _C_LABEL(esigcode)
_C_LABEL(esigcode):


	.data
	.global _C_LABEL(esym)
_C_LABEL(esym): .quad	_C_LABEL(end)


	.align	12
pagetable_l1:
	.space	PAGE_SIZE
	.globl	pagetable_l2
pagetable_l2:
	.space	PAGE_SIZE
pagetable_l2_devmap:
	.space	PAGE_SIZE

	.align 3
virt_map:
	.quad	virt_map

	.globl init_pt_va
init_pt_va:
	.quad pagetable_l2	/* XXX: Keep page tables VA */

#ifdef MULTIPROCESSOR

	.text
	.globl cpu_hatch
cpu_hatch:
	ld	tp, CI_SELF(a1)
	ld	a2, CI_SATP(a1)

	/* Set the global pointer */
.option push
.option norelax
	lla	gp, __global_pointer$
.option pop

	/* Setup stack pointer */
	ld	sp, CI_INITSTACK_END(a1)

	/* Get the kernel's load address */
	jal	get_physmem

	/* Setup supervisor trap vector */
	lla	t0, mpva
	sub	t0, t0, s9
	li	t1, KERNBASE
	add	t0, t0, t1
	csrw	stvec, t0

	/* Set page tables base register */
	lla	s2, pagetable_l1
	srli	s2, s2, PAGE_SHIFT
	li	t0, SATP_MODE_SV39
	or	s2, s2, t0
	sfence.vma
	csrw	satp, s2

	.align 2
mpva:
	/* Set the global pointer again, this time with the virtual address. */
.option push
.option norelax
	lla	gp, __global_pointer$
.option pop

	/* Setup supervisor trap vector */
	la	t0, cpu_exception_handler
	csrw	stvec, t0

	/* Ensure sscratch is zero */
	li	t0, 0
	csrw	sscratch, t0

	/* Switch to real kernel page tables */
	csrw	satp, a2
	sfence.vma

	call	_C_LABEL(cpu_start_secondary)

#endif
