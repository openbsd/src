/*	$OpenBSD: locore0.S,v 1.10 2018/12/18 21:11:09 guenther Exp $	*/
/*	$NetBSD: locore.S,v 1.13 2004/03/25 18:33:17 drochner Exp $	*/

/*
 * Copyright-o-rama!
 */

/*
 * Copyright (c) 2001 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Frank van der Linden for Wasabi Systems, Inc.
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
 *      This product includes software developed for the NetBSD Project by
 *      Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */


/*-
 * Copyright (c) 1998, 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum.
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

/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)locore.s	7.3 (Berkeley) 5/13/91
 */

#include "assym.h"
#include "lapic.h"
#include "ksyms.h"

#include <machine/param.h>
#include <machine/segments.h>
#include <machine/specialreg.h>

/*
 * override user-land alignment before including asm.h
 */
#define	ALIGN_DATA	.align	8,0xcc

#include <machine/asm.h>

/* XXX temporary kluge; these should not be here */
/* Get definitions for IOM_BEGIN, IOM_END, and IOM_SIZE */
#include <dev/isa/isareg.h>

#define	_RELOC(x)	((x) - KERNBASE)
#define	RELOC(x)	_RELOC(_C_LABEL(x))

/*
 * Some hackage to deal with 64bit symbols in 32 bit mode.
 * This may not be needed if things are cleaned up a little.
 */

	.text
	.globl	_C_LABEL(kernel_text)
	.set	_C_LABEL(kernel_text),KERNTEXTOFF

	.code32

	.globl	start
start:	movw	$0x1234,0x472			# warm boot

	/*
	 * Load parameters from stack
	 * (howto, bootdev, bootapiver, esym, extmem (unused), cnvmem, ac, av)
	 */
	movl	4(%esp),%eax
	movl	%eax, RELOC(boothowto)
	movl	8(%esp),%eax
	movl	%eax, RELOC(bootdev)

	/*
	 * Syms are placed after last load and bss of the kernel.
	 * XXX Boot ignores 2MB roundup of _end, so esyms can be < _end.
	 */
	movl	16(%esp), %eax
	testl	%eax,%eax
	jz	1f
	addl	$KERNBASE_LO,%eax
	movl	$RELOC(esym),%ebp
	movl	%eax,(%ebp)
	movl	$KERNBASE_HI,4(%ebp)
1:
	movl	24(%esp), %eax
	movl	%eax, RELOC(biosbasemem)

	movl	12(%esp), %eax
	movl	%eax, RELOC(bootapiver)

	/*
	 * Copy the boot arguments to bootinfo[] in machdep.c.
	 *
	 * We are passed the size of the data /boot passed to us in
	 * 28(%esp). We copy up to bootinfo_size bytes of data into
	 * bootinfo and report back how much we copied in bootinfo_size.
	 *
	 * machdep.c can then take action if bootinfo_size >= bootinfo[]
	 * (which would meant that we may have been passed too much data).
	 */
	movl	28(%esp), %eax
	movl	%eax, %ecx
	cmpl	RELOC(bootinfo_size), %ecx	/* Too much? */
	jb	bi_size_ok
	movl	RELOC(bootinfo_size), %ecx	/* Only copy this much */
bi_size_ok:
	movl	%eax, RELOC(bootinfo_size)	/* Report full amount */

	movl	$RELOC(bootinfo), %edi		/* Destination */
	movl	32(%esp), %esi			/* Source */
	rep movsb				/* Copy this many bytes */

	/* First, reset the PSL. */
	pushl	$PSL_MBO
	popfl

	xorl	%eax,%eax
	cpuid
	movl	%eax,RELOC(cpuid_level)
	movl	$RELOC(cpu_vendor),%ebp
	movl	%ebx,(%ebp)
	movl	%edx,4(%ebp)
	movl	%ecx,8(%ebp)
	movl	$0, 12(%ebp)

	/*
	 * Determine if CPU has meltdown. Certain Intel CPUs do not properly
	 * respect page permissions when speculatively loading data into
	 * the cache ("Meltdown" CVE). These CPUs must utilize a secondary
	 * sanitized page table lacking kernel mappings when executing user
	 * processes, and may not use PG_G global PTEs for kernel VAs.
	 */
	movl	$0x1, RELOC(cpu_meltdown)	/* assume insecure at first */
	movl	$0x0, RELOC(pg_g_kern)

	cmpl	$0x756e6547, %ebx	# "Genu"
	jne	.Lcpu_secure
	cmpl	$0x6c65746e, %ecx	# "ntel"
	jne	.Lcpu_secure
	cmpl	$0x49656e69, %edx	# "ineI"
	jne	.Lcpu_secure

	/*
	 * Intel CPU, now check if IA32_ARCH_CAPABILITIES is supported and
	 * if it says this CPU is safe.
	 */
	cmpl	$0x7,	%eax
	jl	.Lcpu_check_finished

	movl	$0x7,	%eax
	xorl	%ecx,%ecx
	cpuid
	testl	$SEFF0EDX_ARCH_CAP, %edx
	jz	.Lcpu_check_finished

	/* IA32_ARCH_CAPABILITIES MSR avaialble, use it to check CPU security */
	movl	$MSR_ARCH_CAPABILITIES, %ecx
	rdmsr
	testl	$ARCH_CAPABILITIES_RDCL_NO, %eax
	jz	.Lcpu_check_finished

.Lcpu_secure:
	movl	$0x0, RELOC(cpu_meltdown)
	movl	$PG_G, RELOC(pg_g_kern)

.Lcpu_check_finished:
	movl	$1,%eax
	cpuid
	movl	%eax,RELOC(cpu_id)
	movl	%ebx,RELOC(cpu_ebxfeature)
	movl	%ecx,RELOC(cpu_ecxfeature)
	movl	%edx,RELOC(cpu_feature)

	movl	$0x0a,%eax
	cpuid
	movl	%eax,RELOC(_C_LABEL(cpu_perf_eax))
	movl	%ebx,RELOC(_C_LABEL(cpu_perf_ebx))
	movl	%edx,RELOC(_C_LABEL(cpu_perf_edx))

	movl	$0x80000001, %eax
	cpuid
	andl	$CPUID_NXE, %edx	/* other bits may clash */
	jz	cont

	/*
	 * We have NX, set pg_nx accordingly.
	 * NX bit is bit 63 (bit 31 of the second 32 bit dword) - need
	 * to use 32 bit registers here
	 */
	pushl	%edx
	movl	RELOC((pg_nx + 4)), %edx	/* Second dword */
	orl	$0x80000000, %edx		/* Bit 31 (really 63) */
	movl	%edx, RELOC((pg_nx + 4))
	popl	%edx
cont:
	orl	%edx, RELOC(cpu_feature)

	movl	$0x80000007,%eax
	cpuid
	movl	%edx,RELOC(_C_LABEL(cpu_apmi_edx))

	/*
	 * Finished with old stack; load new %esp now instead of later so we
	 * can trace this code without having to worry about the trace trap
	 * clobbering the memory test or the zeroing of the bss+bootstrap page
	 * tables.
	 *
	 * The boot program should check:
	 *	text+data <= &stack_variable - more_space_for_stack
	 *	text+data+bss+pad+space_for_page_tables <= end_of_memory
	 * Oops, the gdt is in the carcass of the boot program so clearing
	 * the rest of memory is still not possible.
	 */
	movl	$RELOC(tmpstk),%esp

/*
 * Virtual address space of kernel:
 *
 * text | data | bss | [syms] | page dir | proc0 kstack | L1 ptp | L2 ptp | L3
 *			      0          1       2      3
 */

#if L2_SLOT_KERNBASE > 0
#define TABLE_L2_ENTRIES (2 * (NKL2_KIMG_ENTRIES + 1))
#else
#define TABLE_L2_ENTRIES (NKL2_KIMG_ENTRIES + 1)
#endif

#if L3_SLOT_KERNBASE > 0
#define TABLE_L3_ENTRIES (2 * NKL3_KIMG_ENTRIES)
#else
#define TABLE_L3_ENTRIES NKL3_KIMG_ENTRIES
#endif


#define PROC0_PML4_OFF	0
#define PROC0_STK_OFF	(PROC0_PML4_OFF + NBPG)
#define PROC0_PTP3_OFF	(PROC0_STK_OFF + UPAGES * NBPG)
#define PROC0_PTP2_OFF	(PROC0_PTP3_OFF + NKL4_KIMG_ENTRIES * NBPG)
#define PROC0_PTP1_OFF	(PROC0_PTP2_OFF + TABLE_L3_ENTRIES * NBPG)
#define	PROC0_DMP3_OFF	(PROC0_PTP1_OFF + TABLE_L2_ENTRIES * NBPG)
#define PROC0_DMP2_OFF	(PROC0_DMP3_OFF + NDML3_ENTRIES * NBPG)
#define TABLESIZE \
    ((NKL4_KIMG_ENTRIES + TABLE_L3_ENTRIES + TABLE_L2_ENTRIES + 1 + UPAGES + \
	NDML3_ENTRIES + NDML2_ENTRIES) * NBPG)

#define fillkpt \
1:	movl	%eax,(%ebx)	;	/* store phys addr */ \
	movl	$0,4(%ebx)	;	/* upper 32 bits 0 */ \
	addl	$8,%ebx		;	/* next pte/pde */ \
	addl	$NBPG,%eax	;	/* next phys page */ \
	loop	1b		;	/* till finished */


#define fillkpt_nx \
	pushl	%ebp				;	/* save */ \
1:	movl	%eax,(%ebx)			;	/* store phys addr */ \
	movl	RELOC((pg_nx + 4)), %ebp	;	/* NX bit? */ \
	movl	%ebp,4(%ebx)			;	/* upper 32 bits */ \
	addl	$8,%ebx				;	/* next pte/pde */ \
	addl	$NBPG,%eax			;	/* next phys page */ \
	loop	1b				;	/* till finished */ \
	popl	%ebp

	/* Find end of kernel image. */
	movl	$RELOC(end),%edi
#if (NKSYMS || defined(DDB))
	/* Save the symbols (if loaded). */
	movl	RELOC(esym),%eax
	testl	%eax,%eax
	jz	1f
	subl	$KERNBASE_LO,%eax	/* XXX */
	/* Page tables must be after symbols and after kernel image. */
	cmpl	%eax,%edi
	jg	1f
	movl	%eax,%edi
1:
#endif
	/* Clear tables */
	movl	%edi,%esi
	addl	$PGOFSET,%esi
	andl	$~PGOFSET,%esi

	movl	%esi,%edi
	xorl	%eax,%eax
	cld
	movl	$TABLESIZE,%ecx
	shrl	$2,%ecx
	rep
	stosl

	leal	(PROC0_PTP1_OFF)(%esi), %ebx

	/*
	 * Compute etext - KERNBASE. This can't be > 4G, or we can't deal
	 * with it anyway, since we can't load it in 32 bit mode. So use
	 * the bottom 32 bits.
	 */
	movl	$RELOC(etext),%edx
	addl	$PGOFSET,%edx
	andl	$~PGOFSET,%edx

	/*
	 * Skip the first MB.
	 */
	movl	$(KERNTEXTOFF_LO - KERNBASE_LO),%eax
	movl	%eax,%ecx
	shrl	$(PGSHIFT-3),%ecx	/* ((n >> PGSHIFT) << 3) for # pdes */
	addl	%ecx,%ebx

	/* Map kernel text RO, X */
	movl	%edx,%ecx
	subl	%eax,%ecx
	shrl	$PGSHIFT,%ecx
	orl	$(PG_V|PG_KR),%eax
	fillkpt

	/* Map .rodata RO, NX */
	movl	$RELOC(__rodata_start), %eax
	movl	$RELOC(erodata), %ecx
	addl	$PGOFSET, %ecx
	andl	$~PGOFSET, %ecx
	subl	%eax, %ecx
	shrl	$PGSHIFT, %ecx
	orl	$(PG_V|PG_KR), %eax
	fillkpt_nx

	/* Map the data and BSS sections RW, NX */
	movl	$RELOC(__data_start), %eax
	movl	$RELOC(__kernel_bss_end),%ecx
	addl	$PGOFSET, %ecx
	andl	$~PGOFSET, %ecx
	subl	%eax, %ecx
	shrl	$PGSHIFT,%ecx
	orl	$(PG_V|PG_KW), %eax
	fillkpt_nx

	/* Map "hole" at end of BSS RO, NX */
	movl	$RELOC(__kernel_bss_end), %eax
	movl	$RELOC(end), %ecx
	addl	$PGOFSET, %ecx
	andl	$~PGOFSET, %ecx
	cmpl	%eax, %ecx
	je	map_syms
	subl	%eax, %ecx
	shrl	$PGSHIFT, %ecx
	orl	$(PG_V|PG_KR), %eax
	fillkpt_nx

map_syms:
	/* Map symbol space RO, NX */
	movl	$RELOC(end), %eax
	movl	%esi, %ecx
	addl	$PGOFSET, %ecx
	andl	$~PGOFSET, %ecx
	cmpl	%eax, %ecx
	je	map_tables
	subl	%eax, %ecx
	shrl	$PGSHIFT, %ecx
	orl	$(PG_V|PG_KR), %eax
	fillkpt_nx

map_tables:
	/* Map the bootstrap tables RW, NX */
	movl	%esi, %edx
	leal	(PG_V|PG_KW)(%edx),%eax
	movl	$TABLESIZE,%ecx
	shrl	$PGSHIFT,%ecx
	fillkpt_nx

	/* Map ISA I/O mem (later atdevbase) RW, NX */
	movl	$(IOM_BEGIN|PG_V|PG_KW/*|PG_N*/),%eax
	movl	$(IOM_SIZE>>PGSHIFT),%ecx
	fillkpt_nx

	/* Set up level 2 pages (RWX) */
	leal	(PROC0_PTP2_OFF)(%esi),%ebx
	leal	(PROC0_PTP1_OFF)(%esi),%eax
	orl	$(PG_V|PG_KW), %eax
	movl	$(NKL2_KIMG_ENTRIES+1),%ecx
	fillkpt

#if L2_SLOT_KERNBASE > 0
	/* If needed, set up L2 entries for actual kernel mapping (RWX) */
	leal	(PROC0_PTP2_OFF+ L2_SLOT_KERNBASE*8)(%esi),%ebx
	leal	(PROC0_PTP1_OFF)(%esi),%eax
	orl	$(PG_V|PG_KW), %eax
	movl	$(NKL2_KIMG_ENTRIES+1),%ecx
	fillkpt
#endif

	/* Set up level 3 pages (RWX) */
	leal	(PROC0_PTP3_OFF)(%esi),%ebx
	leal	(PROC0_PTP2_OFF)(%esi),%eax
	orl	$(PG_V|PG_KW), %eax
	movl	$NKL3_KIMG_ENTRIES,%ecx
	fillkpt

#if L3_SLOT_KERNBASE > 0
	/* If needed, set up L3 entries for actual kernel mapping (RWX) */
	leal	(PROC0_PTP3_OFF+ L3_SLOT_KERNBASE*8)(%esi),%ebx
	leal	(PROC0_PTP2_OFF)(%esi),%eax
	orl	$(PG_V|PG_KW), %eax
	movl	$NKL3_KIMG_ENTRIES,%ecx
	fillkpt
#endif

	/* Set up top level entries for identity mapping (RWX) */
	leal	(PROC0_PML4_OFF)(%esi),%ebx
	leal	(PROC0_PTP3_OFF)(%esi),%eax
	orl	$(PG_V|PG_KW), %eax
	movl	$NKL4_KIMG_ENTRIES,%ecx
	fillkpt

	/* Set up top level entries for actual kernel mapping (RWX) */
	leal	(PROC0_PML4_OFF + L4_SLOT_KERNBASE*8)(%esi),%ebx
	leal	(PROC0_PTP3_OFF)(%esi),%eax
	orl	$(PG_V|PG_KW), %eax
	movl	$NKL4_KIMG_ENTRIES,%ecx
	fillkpt

	/*
	 * Map the first 4 GB with the direct map. We'll map the rest
	 * in pmap_bootstrap. But we always need the first 4GB during
	 * bootstrap. The direct map is mapped RW, NX. We also change
	 * the permissions on the 2MB pages corresponding to the kernel
	 * PAs to RO to prevent someone writing to the kernel area
	 * via the direct map.
	 */
	leal	(PROC0_DMP2_OFF)(%esi), %ebx
	xorl	%eax, %eax
	movl	$(NDML2_ENTRIES * NPDPG), %ecx
1:	orl	$(PG_V|PG_KW|PG_PS), %eax
	orl	RELOC(pg_g_kern), %eax
	cmpl	$__kernel_base_phys, %eax
	jl	store_pte
	cmpl	$__kernel_end_phys, %eax
	jg	store_pte
	andl	$(~PG_KW), %eax
store_pte:
	movl	%eax, (%ebx)
	pushl	%ebp
	movl	RELOC((pg_nx + 4)), %ebp
	movl	%ebp, 4(%ebx)
	popl	%ebp
	addl	$8, %ebx
	addl	$NBPD_L2, %eax
	loop	1b

	leal	(PROC0_DMP3_OFF)(%esi), %ebx
	leal	(PROC0_DMP2_OFF)(%esi), %eax
	orl	$(PG_V|PG_KW), %eax
	movl	$NDML2_ENTRIES, %ecx
	fillkpt_nx

	leal	(PROC0_PML4_OFF + PDIR_SLOT_DIRECT * 8)(%esi), %ebx
	leal	(PROC0_DMP3_OFF)(%esi), %eax
	orl	$(PG_V|PG_KW), %eax
	movl	$NDML3_ENTRIES, %ecx
	fillkpt_nx

	/* Install recursive top level PDE */
	leal	(PROC0_PML4_OFF + PDIR_SLOT_PTE*8)(%esi),%ebx
	leal	(PROC0_PML4_OFF)(%esi),%eax
	orl	$(PG_V|PG_KW),%eax
	movl	%eax,(%ebx)
	pushl	%ebp
	movl	RELOC((pg_nx + 4)), %ebp
	movl	%ebp, 4(%ebx)
	popl	%ebp

	/* Save phys. addr of PTD, for libkvm. */
	movl	$RELOC(PTDpaddr),%ebp
	movl	%esi,(%ebp)
	movl	$0,4(%ebp)

	/*
	 * Startup checklist:
	 * 1. Enable PAE (and SSE while here).
	 */
	movl	%cr4,%eax
	orl	$(CR4_DEFAULT),%eax
	movl	%eax,%cr4

	/*
	 * 2. Set Long Mode Enable in EFER. Also enable the
	 *    syscall extensions and NX (if available).
	 */
	movl	$MSR_EFER,%ecx
	rdmsr
	xorl	%eax,%eax	/* XXX */
	orl	$(EFER_LME|EFER_SCE),%eax
	movl	RELOC((pg_nx + 4)), %ebx
	cmpl	$0, %ebx
	je	write_efer
	orl	$(EFER_NXE), %eax
write_efer:
	wrmsr

	/*
	 * 3. Load %cr3 with pointer to PML4.
	 */
	movl	%esi,%eax
	movl	%eax,%cr3

	/*
	 * 4. Enable paging and the rest of it.
	 */
	movl	%cr0,%eax
	orl	$CR0_DEFAULT,%eax
	movl	%eax,%cr0
	jmp	compat
compat:

	/*
	 * 5.
	 * Not quite done yet, we're now in a compatibility segment,
	 * in legacy mode. We must jump to a long mode segment.
	 * Need to set up a temporary GDT with a long mode segment
	 * in it to do that.
	 */

	movl	$RELOC(gdt64),%eax
	lgdt	(%eax)
	movl	$RELOC(farjmp64),%eax
	ljmp	*(%eax)

.code64
longmode:
	/*
	 * 6.
	 * Finally, we're in long mode. However, we're still
	 * in the identity mapped area (could not jump out
	 * of that earlier because it would have been a > 32bit
	 * jump). We can do that now, so here we go.
	 */
	movabsq	$longmode_hi,%rax
	jmp	*%rax
longmode_hi:
	/*
	 * We have arrived.
	 * There's no need anymore for the identity mapping in low
	 * memory, remove it.
	 */
	movq	$KERNBASE,%r8

#if L2_SLOT_KERNBASE > 0
	movq	$(NKL2_KIMG_ENTRIES+1),%rcx
	leaq	(PROC0_PTP2_OFF)(%rsi),%rbx
	addq	%r8, %rbx
1:	movq	$0 ,(%rbx)
	addq	$8,%rbx
	loop	1b
#endif

#if L3_SLOT_KERNBASE > 0
	movq	$NKL3_KIMG_ENTRIES,%rcx
	leaq	(PROC0_PTP3_OFF)(%rsi),%rbx
	addq	%r8, %rbx
1:	movq	$0 ,(%rbx)
	addq	$8,%rbx
	loop	1b
#endif

	movq	$NKL4_KIMG_ENTRIES,%rcx
	leaq	(PROC0_PML4_OFF)(%rsi),%rbx	# old, phys address of PML4
	addq	%r8, %rbx			# new, virtual address of PML4
1:	movq	$0, (%rbx)
	addq	$8,%rbx
	loop	1b

	/* Relocate atdevbase. */
	movq	$(TABLESIZE+KERNBASE),%rdx
	addq	%rsi,%rdx
	movq	%rdx,_C_LABEL(atdevbase)(%rip)

	/* Record start of symbols */
	movq	$__kernel_bss_end, _C_LABEL(ssym)(%rip)

	/* Set up bootstrap stack. */
	leaq	(PROC0_STK_OFF)(%rsi),%rax
	addq	%r8,%rax
	movq	%rax,_C_LABEL(proc0paddr)(%rip)
	leaq	(USPACE-FRAMESIZE)(%rax),%rsp
	movq	%rsi,PCB_CR3(%rax)	# pcb->pcb_cr3
	xorq	%rbp,%rbp		# mark end of frames

	xorw	%ax,%ax
	movw	%ax,%gs
	movw	%ax,%fs

	/* XXX merge these */
	leaq	TABLESIZE(%rsi),%rdi
	call	_C_LABEL(init_x86_64)

	call	_C_LABEL(main)

	.section .codepatch,"a"
	.align	8, 0xcc
	.globl _C_LABEL(codepatch_begin)
_C_LABEL(codepatch_begin):
	.previous

	.section .codepatchend,"a"
	.globl _C_LABEL(codepatch_end)
_C_LABEL(codepatch_end):
	.previous

	.data

farjmp64:
	.long	longmode-KERNBASE
	.word	GSEL(GCODE_SEL, SEL_KPL)

	.align 8, 0xcc
	.space 512
tmpstk:

