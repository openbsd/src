/*	$OpenBSD: locore.S,v 1.71 2015/07/17 06:13:44 mlarkin Exp $	*/
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
#include "ioapic.h"
#include "ksyms.h"
#include "acpi.h"

#include <sys/errno.h>
#include <sys/syscall.h>

#include <machine/param.h>
#include <machine/segments.h>
#include <machine/specialreg.h>
#include <machine/trap.h>
#include <machine/frameasm.h>

#if NLAPIC > 0
#include <machine/i82489reg.h>
#endif

/*
 * override user-land alignment before including asm.h
 */
#define	ALIGN_DATA	.align	8
#define ALIGN_TEXT	.align 16,0x90
#define _ALIGN_TEXT	ALIGN_TEXT

#include <machine/asm.h>

#define SET_CURPROC(proc,cpu)			\
	movq	CPUVAR(SELF),cpu	;	\
	movq	proc,CPUVAR(CURPROC)      ;	\
	movq	cpu,P_CPU(proc)

#define GET_CURPCB(reg)			movq	CPUVAR(CURPCB),reg      
#define SET_CURPCB(reg)			movq	reg,CPUVAR(CURPCB)


/* XXX temporary kluge; these should not be here */
/* Get definitions for IOM_BEGIN, IOM_END, and IOM_SIZE */
#include <dev/isa/isareg.h>


/*
 * Initialization
 */
	.data

#if NLAPIC > 0 
	.align  NBPG
	.globl _C_LABEL(local_apic), _C_LABEL(lapic_id), _C_LABEL(lapic_tpr)
_C_LABEL(local_apic):
	.space  LAPIC_ID
_C_LABEL(lapic_id):
	.long   0x00000000
	.space  LAPIC_TPRI-(LAPIC_ID+4)
_C_LABEL(lapic_tpr):
	.space  LAPIC_PPRI-LAPIC_TPRI
_C_LABEL(lapic_ppr):
	.space  LAPIC_ISR-LAPIC_PPRI 
_C_LABEL(lapic_isr):
	.space  NBPG-LAPIC_ISR
#endif

	.globl	_C_LABEL(cpu_id),_C_LABEL(cpu_vendor)
	.globl	_C_LABEL(cpuid_level),_C_LABEL(cpu_feature)
	.globl	_C_LABEL(cpu_ecxfeature),_C_LABEL(ecpu_ecxfeature)
	.globl	_C_LABEL(cpu_perf_eax)
	.globl	_C_LABEL(cpu_perf_ebx)
	.globl	_C_LABEL(cpu_perf_edx)
	.globl	_C_LABEL(cpu_apmi_edx)
	.globl	_C_LABEL(ssym),_C_LABEL(esym),_C_LABEL(boothowto),_C_LABEL(bootdev)
	.globl	_C_LABEL(bootinfo), _C_LABEL(bootinfo_size), _C_LABEL(atdevbase)
	.globl	_C_LABEL(proc0paddr),_C_LABEL(PTDpaddr)
	.globl	_C_LABEL(biosbasemem),_C_LABEL(biosextmem)
	.globl	_C_LABEL(bootapiver)
	.globl	_C_LABEL(pg_nx)
_C_LABEL(cpu_id):	.long	0	# saved from `cpuid' instruction
_C_LABEL(cpu_feature):	.long	0	# feature flags from 'cpuid'
					#   instruction
_C_LABEL(cpu_ecxfeature):.long	0	# extended feature flags from 'cpuid'
_C_LABEL(ecpu_ecxfeature):.long	0	# extended ecx feature flags
_C_LABEL(cpu_perf_eax):	.long	0	# arch. perf. mon. flags from 'cpuid'
_C_LABEL(cpu_perf_ebx):	.long	0	# arch. perf. mon. flags from 'cpuid'
_C_LABEL(cpu_perf_edx):	.long	0	# arch. perf. mon. flags from 'cpuid'
_C_LABEL(cpu_apmi_edx):	.long	0	# adv. power mgmt. info. from 'cpuid'
_C_LABEL(cpuid_level):	.long	-1	# max. level accepted by 'cpuid'
					#   instruction
_C_LABEL(cpu_vendor):	.space	16	# vendor string returned by `cpuid'
					#   instruction
_C_LABEL(ssym):		.quad	0	# ptr to start of syms
_C_LABEL(esym):		.quad	0	# ptr to end of syms
_C_LABEL(atdevbase):	.quad	0	# location of start of iomem in virtual
_C_LABEL(bootapiver):	.long	0	# /boot API version
_C_LABEL(bootdev):	.long	0	# device we booted from
_C_LABEL(proc0paddr):	.quad	0
_C_LABEL(PTDpaddr):	.quad	0	# paddr of PTD, for libkvm
#ifndef REALBASEMEM
_C_LABEL(biosbasemem):	.long	0	# base memory reported by BIOS
#else
_C_LABEL(biosbasemem):	.long	REALBASEMEM
#endif
#ifndef REALEXTMEM
_C_LABEL(biosextmem):	.long	0	# extended memory reported by BIOS
#else
_C_LABEL(biosextmem):	.long	REALEXTMEM
#endif
_C_LABEL(pg_nx):	.quad	0	# NX PTE bit (if CPU supports)

#define	_RELOC(x)	((x) - KERNBASE)
#define	RELOC(x)	_RELOC(_C_LABEL(x))

	.globl	gdt64

gdt64:
	.word	gdt64_end-gdt64_start-1
	.quad	_RELOC(gdt64_start)
.align 64

gdt64_start:
	.quad 0x0000000000000000	/* always empty */
	.quad 0x00af9a000000ffff	/* kernel CS */
	.quad 0x00cf92000000ffff	/* kernel DS */
gdt64_end:

farjmp64:
	.long	longmode-KERNBASE
	.word	GSEL(GCODE_SEL, SEL_KPL)

	.space 512
tmpstk:

	.globl _C_LABEL(cpu_private)
	.comm _C_LABEL(cpu_private),NBPG,NBPG

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
	 * (howto, bootdev, bootapiver, esym, extmem, cnvmem, ac, av)
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
	testl   %eax,%eax
	jz      1f
	addl    $KERNBASE_LO,%eax
	movl    $RELOC(esym),%ebp
	movl    %eax,(%ebp)
	movl    $KERNBASE_HI,4(%ebp)
1:
	movl	20(%esp), %eax
	movl	%eax, RELOC(biosextmem)
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
	movl	$0,  12(%ebp)

	movl	$1,%eax
	cpuid
	movl	%eax,RELOC(cpu_id)
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
	orl	$0x80000000, %edx 		/* Bit 31 (really 63) */
	movl	%edx, RELOC((pg_nx + 4))
	popl	%edx
cont:
	orl     %edx, RELOC(cpu_feature)

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
1:	movl	%eax,(%ebx)	; 	/* store phys addr */ \
	movl	$0,4(%ebx)	; 	/* upper 32 bits 0 */ \
	addl	$8,%ebx		; 	/* next pte/pde */ \
	addl	$NBPG,%eax	; 	/* next phys page */ \
	loop	1b		;	/* till finished */


#define fillkpt_nx \
	pushl	%ebp				;	/* save */ \
1:	movl	%eax,(%ebx)			; 	/* store phys addr */ \
	movl	RELOC((pg_nx + 4)), %ebp	;	/* NX bit? */ \
	movl	%ebp,4(%ebx)			; 	/* upper 32 bits */ \
	addl	$8,%ebx				; 	/* next pte/pde */ \
	addl	$NBPG,%eax			; 	/* next phys page */ \
	loop	1b				;	/* till finished */ \
	popl	%ebp

	/* Find end of kernel image. */
	movl	$RELOC(end),%edi
#if (NKSYMS || defined(DDB)) && !defined(SYMTAB_SPACE)
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
	orl     $(PG_V|PG_KR),%eax
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
	leal    (PROC0_PTP2_OFF)(%esi),%ebx
	leal	(PROC0_PTP1_OFF)(%esi),%eax
	orl	$(PG_V|PG_KW), %eax
	movl	$(NKL2_KIMG_ENTRIES+1),%ecx
	fillkpt

#if L2_SLOT_KERNBASE > 0
	/* If needed, set up L2 entries for actual kernel mapping (RWX) */
	leal	(PROC0_PTP2_OFF+ L2_SLOT_KERNBASE*8)(%esi),%ebx
	leal    (PROC0_PTP1_OFF)(%esi),%eax
	orl     $(PG_V|PG_KW), %eax
	movl    $(NKL2_KIMG_ENTRIES+1),%ecx
	fillkpt
#endif

	/* Set up level 3 pages (RWX) */
	leal    (PROC0_PTP3_OFF)(%esi),%ebx
	leal	(PROC0_PTP2_OFF)(%esi),%eax
	orl	$(PG_V|PG_KW), %eax
	movl	$NKL3_KIMG_ENTRIES,%ecx
	fillkpt

#if L3_SLOT_KERNBASE > 0
	/* If needed, set up L3 entries for actual kernel mapping (RWX) */
	leal	(PROC0_PTP3_OFF+ L3_SLOT_KERNBASE*8)(%esi),%ebx
	leal    (PROC0_PTP2_OFF)(%esi),%eax
	orl     $(PG_V|PG_KW), %eax
	movl    $NKL3_KIMG_ENTRIES,%ecx
	fillkpt
#endif

	/* Set up top level entries for identity mapping (RWX) */
	leal    (PROC0_PML4_OFF)(%esi),%ebx
	leal	(PROC0_PTP3_OFF)(%esi),%eax
	orl	$(PG_V|PG_KW), %eax
	movl	$NKL4_KIMG_ENTRIES,%ecx
	fillkpt

	/* Set up top level entries for actual kernel mapping (RWX) */
	leal    (PROC0_PML4_OFF + L4_SLOT_KERNBASE*8)(%esi),%ebx
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
1:	orl	$(PG_V|PG_KW|PG_PS|PG_G), %eax
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
	popl 	%ebp

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
	movl    $MSR_EFER,%ecx
	rdmsr
	xorl	%eax,%eax	/* XXX */
	orl	$(EFER_LME|EFER_SCE),%eax
	movl	RELOC((pg_nx + 4)), %ebx
	cmpl	$0, %ebx
	je 	write_efer
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
	orl	$(CR0_PE|CR0_PG|CR0_NE|CR0_TS|CR0_MP|CR0_WP),%eax
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
	leaq	(PROC0_PML4_OFF)(%rsi),%rbx	# old, phys  address of PML4
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
	xorq	%rbp,%rbp               # mark end of frames

	xorw	%ax,%ax
	movw	%ax,%gs
	movw	%ax,%fs

	/* XXX merge these */
	leaq	TABLESIZE(%rsi),%rdi
	call	_C_LABEL(init_x86_64)

	call 	_C_LABEL(main)

/*****************************************************************************/

/*
 * Signal trampoline; copied to top of user stack.
 * gdb's backtrace logic matches against the instructions in this.
 */
NENTRY(sigcode)
	call	*%rax

	movq	%rsp,%rdi
	pushq	%rdi			/* fake return address */
	movq	$SYS_sigreturn,%rax
	syscall
	movq	$SYS_exit,%rax
	syscall
	.globl	_C_LABEL(esigcode)
_C_LABEL(esigcode):

/*
 * void lgdt(struct region_descriptor *rdp);
 * Change the global descriptor table.
 */
NENTRY(lgdt)
	/* Reload the descriptor table. */
	movq	%rdi,%rax
	lgdt	(%rax)
	/* Flush the prefetch q. */
	jmp	1f
	nop
1:	/* Reload "stale" selectors. */
	movl	$GSEL(GDATA_SEL, SEL_KPL),%eax
	movl	%eax,%ds
	movl	%eax,%es
	movl	%eax,%ss
	/* Reload code selector by doing intersegment return. */
	popq	%rax
	pushq	$GSEL(GCODE_SEL, SEL_KPL)
	pushq	%rax
	lretq

ENTRY(setjmp)
	/*
	 * Only save registers that must be preserved across function
	 * calls according to the ABI (%rbx, %rsp, %rbp, %r12-%r15)
	 * and %rip.
	 */
	movq	%rdi,%rax
	movq	%rbx,(%rax)
	movq	%rsp,8(%rax)
	movq	%rbp,16(%rax)
	movq	%r12,24(%rax)
	movq	%r13,32(%rax)
	movq	%r14,40(%rax)
	movq	%r15,48(%rax)
	movq	(%rsp),%rdx
	movq	%rdx,56(%rax)
	xorl	%eax,%eax
	ret

ENTRY(longjmp)
	movq	%rdi,%rax
	movq	(%rax),%rbx
	movq	8(%rax),%rsp
	movq	16(%rax),%rbp
	movq	24(%rax),%r12
	movq	32(%rax),%r13
	movq	40(%rax),%r14
	movq	48(%rax),%r15
	movq	56(%rax),%rdx
	movq	%rdx,(%rsp)
	xorl	%eax,%eax
	incl	%eax
	ret

/*****************************************************************************/

/*
 * int cpu_switchto(struct proc *old, struct proc *new)
 * Switch from "old" proc to "new".
 */
ENTRY(cpu_switchto)
	pushq	%rbx
	pushq	%rbp
	pushq	%r12
	pushq	%r13
	pushq	%r14
	pushq	%r15

	movq	%rdi, %r13
	movq	%rsi, %r12

	/* Record new proc. */
	movb	$SONPROC,P_STAT(%r12)	# p->p_stat = SONPROC
	SET_CURPROC(%r12,%rcx)

	movl	CPUVAR(CPUID),%edi

	/* If old proc exited, don't bother. */
	testq	%r13,%r13
	jz	switch_exited

	/*
	 * Save old context.
	 *
	 * Registers:
	 *   %rax, %rcx - scratch
	 *   %r13 - old proc, then old pcb
	 *   %r12 - new proc
	 *   %edi - cpuid
	 */

	movq	P_ADDR(%r13),%r13

	/* clear the old pmap's bit for the cpu */
	movq	PCB_PMAP(%r13),%rcx
	lock
	btrq	%rdi,PM_CPUS(%rcx)

	/* Save stack pointers. */
	movq	%rsp,PCB_RSP(%r13)
	movq	%rbp,PCB_RBP(%r13)

switch_exited:
	/* did old proc run in userspace?  then reset the segment regs */
	btrl	$CPUF_USERSEGS_BIT, CPUVAR(FLAGS)
	jnc	restore_saved

	/* set %ds, %es, and %fs to expected value to prevent info leak */
	movw	$(GSEL(GUDATA_SEL, SEL_UPL)),%ax
	movw	%ax,%ds
	movw	%ax,%es
	movw	%ax,%fs

restore_saved:
	/*
	 * Restore saved context.
	 *
	 * Registers:
	 *   %rax, %rcx, %rdx - scratch
	 *   %r13 - new pcb
	 *   %r12 - new process
	 */

	/* No interrupts while loading new state. */
	cli
	movq	P_ADDR(%r12),%r13

	/* Restore stack pointers. */
	movq	PCB_RSP(%r13),%rsp
	movq	PCB_RBP(%r13),%rbp

	movq	CPUVAR(TSS),%rcx
	movq	PCB_KSTACK(%r13),%rdx
	movq	%rdx,TSS_RSP0(%rcx)

	movq	PCB_CR3(%r13),%rax
	movq	%rax,%cr3

	/* Don't bother with the rest if switching to a system process. */
	testl	$P_SYSTEM,P_FLAG(%r12)
	jnz	switch_restored

	/* set the new pmap's bit for the cpu */
	movl	CPUVAR(CPUID),%edi
	movq	PCB_PMAP(%r13),%rcx
	lock
	btsq	%rdi,PM_CPUS(%rcx)
#ifdef DIAGNOSTIC
	jc	_C_LABEL(switch_pmcpu_set)
#endif

switch_restored:
	/* Restore cr0 (including FPU state). */
	movl	PCB_CR0(%r13),%ecx
#ifdef MULTIPROCESSOR
	movq	PCB_FPCPU(%r13),%r8
	cmpq	CPUVAR(SELF),%r8
	jz	1f
	orl	$CR0_TS,%ecx
1:
#endif
	movq	%rcx,%cr0

	SET_CURPCB(%r13)

	/* Interrupts are okay again. */
	sti

switch_return:

	popq	%r15
	popq	%r14
	popq	%r13
	popq	%r12
	popq	%rbp
	popq	%rbx
	ret

ENTRY(cpu_idle_enter)
	movq	_C_LABEL(cpu_idle_enter_fcn),%rax
	cmpq	$0,%rax
	je	1f
	jmpq	*%rax
1:
	ret

ENTRY(cpu_idle_cycle)
	movq	_C_LABEL(cpu_idle_cycle_fcn),%rax
	cmpq	$0,%rax
	je	1f
	call	*%rax
	ret
1:
	sti
	hlt
	ret

ENTRY(cpu_idle_leave)
	movq	_C_LABEL(cpu_idle_leave_fcn),%rax
	cmpq	$0,%rax
	je	1f
	jmpq	*%rax
1:
	ret

	.globl	_C_LABEL(panic)

#ifdef DIAGNOSTIC
NENTRY(switch_pmcpu_set)
	movabsq	$1f,%rdi
	call	_C_LABEL(panic)
	/* NOTREACHED */
1:	.asciz	"activate already active pmap"
#endif /* DIAGNOSTIC */

/*
 * savectx(struct pcb *pcb);
 * Update pcb, saving current processor state.
 */
ENTRY(savectx)
	/* Save stack pointers. */
	movq	%rsp,PCB_RSP(%rdi)
	movq	%rbp,PCB_RBP(%rdi)

	ret

IDTVEC(syscall32)
	sysret		/* go away please */

/*
 * syscall insn entry. This currently isn't much faster, but
 * it can be made faster in the future.
 */
IDTVEC(syscall)
	/*
	 * Enter here with interrupts blocked; %rcx contains the caller's
	 * %rip and the original rflags has been copied to %r11.  %cs and
	 * %ss have been updated to the kernel segments, but %rsp is still
	 * the user-space value.
	 * First order of business is to swap to the kernel gs.base so that
	 * we can access our struct cpu_info and use the scratch space there
	 * to switch to our kernel stack.  Once that's in place we can
	 * unblock interrupts and save the rest of the syscall frame.
	 */
	swapgs
	movq	%r15,CPUVAR(SCRATCH)
	movq	CPUVAR(CURPCB),%r15
	movq	PCB_KSTACK(%r15),%r15
	xchgq	%r15,%rsp
	sti

	/*
	 * XXX don't need this whole frame, split of the
	 * syscall frame and trapframe is needed.
	 * First, leave some room for the trapno, error,
	 * ss:rsp, etc, so that all GP registers can be
	 * saved. Then, fill in the rest.
	 */
	pushq	$(GSEL(GUDATA_SEL, SEL_UPL))
	pushq	%r15
	subq	$(TF_RSP-TF_TRAPNO),%rsp
	movq	CPUVAR(SCRATCH),%r15
	subq	$32,%rsp
	INTR_SAVE_GPRS
	movq	%r11, TF_RFLAGS(%rsp)	/* old rflags from syscall insn */
	movq	$(GSEL(GUCODE_SEL, SEL_UPL)), TF_CS(%rsp)
	movq	%rcx,TF_RIP(%rsp)
	movq	$2,TF_ERR(%rsp)		/* ignored */

	movq	CPUVAR(CURPROC),%r14
	movq	%rsp,P_MD_REGS(%r14)	# save pointer to frame
	andl	$~MDP_IRET,P_MD_FLAGS(%r14)
	movq	%rsp,%rdi
	call	_C_LABEL(syscall)

.Lsyscall_check_asts:
	/* Check for ASTs on exit to user mode. */
	cli
	CHECK_ASTPENDING(%r11)
	je	2f
	CLEAR_ASTPENDING(%r11)
	sti
	movq	%rsp,%rdi
	call	_C_LABEL(ast)
	jmp	.Lsyscall_check_asts

2:
#ifdef DIAGNOSTIC
	cmpl	$IPL_NONE,CPUVAR(ILEVEL)
	jne	.Lsyscall_spl_not_lowered
#endif /* DIAGNOSTIC */

	/* Could registers have been changed that require an iretq? */
	testl	$MDP_IRET, P_MD_FLAGS(%r14)
	jne	intr_fast_exit

	movq	TF_RDI(%rsp),%rdi
	movq	TF_RSI(%rsp),%rsi
	movq	TF_R8(%rsp),%r8
	movq	TF_R9(%rsp),%r9
	movq	TF_R10(%rsp),%r10
	movq	TF_R12(%rsp),%r12
	movq	TF_R13(%rsp),%r13
	movq	TF_R14(%rsp),%r14
	movq	TF_R15(%rsp),%r15
	movq	TF_RBP(%rsp),%rbp
	movq	TF_RBX(%rsp),%rbx

	INTR_RESTORE_SELECTORS

	movq	TF_RDX(%rsp),%rdx
	movq	TF_RAX(%rsp),%rax

	movq	TF_RIP(%rsp),%rcx
	movq	TF_RFLAGS(%rsp),%r11
	movq	TF_RSP(%rsp),%rsp
	sysretq

#ifdef DIAGNOSTIC
.Lsyscall_spl_not_lowered:
	movabsq	$4f, %rdi
	movl	TF_RAX(%rsp),%esi
	movl	TF_RDI(%rsp),%edx
	movl	%ebx,%ecx
	movl	CPUVAR(ILEVEL),%r8d
	xorq	%rax,%rax
	call	_C_LABEL(printf)
#ifdef DDB
	int	$3
#endif /* DDB */
	movl	$IPL_NONE,CPUVAR(ILEVEL)
	jmp	.Lsyscall_check_asts
4:	.asciz	"WARNING: SPL NOT LOWERED ON SYSCALL %d %d EXIT %x %x\n"
#endif


NENTRY(proc_trampoline)
#ifdef MULTIPROCESSOR
	call	_C_LABEL(proc_trampoline_mp)
#endif
	movl	$IPL_NONE,CPUVAR(ILEVEL)
	movq	%r13,%rdi
	call	*%r12
	movq	CPUVAR(CURPROC),%r14
	jmp	.Lsyscall_check_asts


/*
 * Return via iretq, for real interrupts and signal returns
 */
NENTRY(intr_fast_exit)
	movq	TF_RDI(%rsp),%rdi
	movq	TF_RSI(%rsp),%rsi
	movq	TF_R8(%rsp),%r8
	movq	TF_R9(%rsp),%r9
	movq	TF_R10(%rsp),%r10
	movq	TF_R12(%rsp),%r12
	movq	TF_R13(%rsp),%r13
	movq	TF_R14(%rsp),%r14
	movq	TF_R15(%rsp),%r15
	movq	TF_RBP(%rsp),%rbp
	movq	TF_RBX(%rsp),%rbx

	testq	$SEL_UPL,TF_CS(%rsp)
	je	5f

	INTR_RESTORE_SELECTORS

5:	movq	TF_RDX(%rsp),%rdx
	movq	TF_RCX(%rsp),%rcx
	movq	TF_R11(%rsp),%r11
	movq	TF_RAX(%rsp),%rax
	addq	$TF_RIP,%rsp

	.globl	_C_LABEL(doreti_iret)
_C_LABEL(doreti_iret):
	iretq


ENTRY(pagezero)
	movq    $-PAGE_SIZE,%rdx
	subq    %rdx,%rdi
	xorq    %rax,%rax
1:
	movnti  %rax,(%rdi,%rdx)
	movnti  %rax,8(%rdi,%rdx)
	movnti  %rax,16(%rdi,%rdx)
	movnti  %rax,24(%rdi,%rdx)
	addq    $32,%rdx
	jne     1b
	sfence
	ret

	.section .codepatch,"a"
	.align  8
	.globl _C_LABEL(codepatch_begin)
_C_LABEL(codepatch_begin):
	.previous

	.section .codepatchend,"a"
	.globl _C_LABEL(codepatch_end)
_C_LABEL(codepatch_end):
	.previous
