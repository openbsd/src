/*	$OpenBSD: mptramp.s,v 1.27 2022/12/08 01:25:44 guenther Exp $	*/

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by RedBack Networks Inc.
 *
 * Author: Bill Sommerfeld
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

/*
 * Copyright (c) 1999 Stefan Grefen
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
 *      This product includes software developed by the NetBSD
 *      Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR AND CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*
 * MP startup ...
 * the stuff from cpu_spinup_trampoline to mp_startup
 * is copied into the first 640 KB
 *
 * We startup the processors now when the kthreads become ready.
 * The steps are:
 *	1) Get the processors running kernel-code from a special
 *	   page-table and stack page, do chip identification.
 *	2) halt the processors waiting for them to be enabled
 *	   by a idle-thread
 */

#include "assym.h"
#include <machine/param.h>
#include <machine/asm.h>
#include <machine/specialreg.h>
#include <machine/segments.h>
#include <machine/gdt.h>
#include <machine/mpbiosvar.h>
#include <machine/i82489reg.h>

#ifdef __clang__
#define addr32
#endif

#define GDTE(a,b)	.byte	0xff,0xff,0x0,0x0,0x0,a,b,0x0
#define _RELOC(x)	((x) - KERNBASE)
#define RELOC(x)	_RELOC(x)

#define _TRMP_LABEL(a)	a = . - cpu_spinup_trampoline + MP_TRAMPOLINE
#define _TRMP_OFFSET(a)	a = . - cpu_spinup_trampoline
#define _TRMP_DATA_LABEL(a)	a = . - mp_tramp_data_start + MP_TRAMP_DATA
#define _TRMP_DATA_OFFSET(a)	a = . - mp_tramp_data_start

	.globl	cpu_id,cpu_vendor
	.globl	cpuid_level,cpu_feature

	.global cpu_spinup_trampoline
	.global cpu_spinup_trampoline_end
	.global cpu_hatch
	.global mp_pdirpa
	.global mp_tramp_data_start
	.global mp_tramp_data_end
	.global gdt, local_apic

	.text
	.align 4, 0xcc
	.code16
cpu_spinup_trampoline:
	cli
	movw	$(MP_TRAMP_DATA >> 4), %ax
	movw	%ax, %ds
	movw	%cs, %ax
	movw	%ax, %es
	movw	%ax, %ss
	addr32 lgdtl (.Lgdt_desc)	# load flat descriptor table
	movl	%cr0, %eax	# get cr0
	orl	$0x1, %eax	# enable protected mode
	movl	%eax, %cr0	# doit
	ljmpl	$0x8, $.Lmp_startup

_TRMP_LABEL(.Lmp_startup)
	.code32

	movl	$0x10, %eax	# data segment
	movw	%ax, %ds
	movw	%ax, %ss
	movw	%ax, %es
	movw	%ax, %fs
	movw	%ax, %gs
	movl	$(MP_TRAMP_DATA+NBPG-16),%esp	# bootstrap stack end,
						# with scratch space..

	/* First, reset the PSL. */
	pushl	$PSL_MBO
	popfl

	movl	RELOC(mp_pdirpa),%ecx

	/* Load base of page directory and enable mapping. */
	movl	%ecx,%cr3		# load ptd addr into mmu
#ifndef SMALL_KERNEL
	testl	$0x1, RELOC(cpu_pae)
	jz	nopae

	movl	%cr4,%eax
	orl	$CR4_PAE,%eax
	movl	%eax, %cr4
	
	movl	$MSR_EFER,%ecx
	rdmsr
	orl	$EFER_NXE, %eax
	wrmsr

nopae:
#endif
	movl	%cr0,%eax		# get control word
					# enable paging & NPX emulation
	orl	$(CR0_PE|CR0_PG|CR0_NE|CR0_TS|CR0_EM|CR0_MP|CR0_WP),%eax
	movl	%eax,%cr0		# and let's page NOW!

# ok, we're now running with paging enabled and sharing page tables with cpu0.
# figure out which processor we really are, what stack we should be on, etc.

	movl	local_apic+LAPIC_ID,%eax
	shrl	$LAPIC_ID_SHIFT,%eax
	xorl	%ebx,%ebx
1:
	leal	0(,%ebx,4),%ecx
	incl	%ebx
	movl	cpu_info(%ecx),%ecx
	movl	CPU_INFO_APICID(%ecx),%edx
	cmpl	%eax,%edx
	jne 1b

# %ecx points at our cpu_info structure..

	movw	$(GDT_SIZE-1), 6(%esp)		# prepare segment descriptor
	movl	CPU_INFO_GDT(%ecx), %eax	# for real gdt
	movl	%eax, 8(%esp)
	lgdt	6(%esp)
	jmp	1f
	nop
1:
	movl	$GSEL(GDATA_SEL, SEL_KPL),%eax	#switch to new segment
	movw	%ax,%ds
	movw	%ax,%es
	movw	%ax,%ss
	pushl	$GSEL(GCODE_SEL, SEL_KPL)
	pushl	$mp_cont
	lret

cpu_spinup_trampoline_end:		#end of code copied to MP_TRAMPOLINE
mp_cont:

	movl	CPU_INFO_IDLE_PCB(%ecx),%esi

# %esi now points at our PCB.

	movl	PCB_ESP(%esi),%esp
	movl	PCB_EBP(%esi),%ebp

	/* Switch address space. */
	movl	PCB_CR3(%esi),%eax
	movl	%eax,%cr3
	/* Load segment registers. */
	movl	$GSEL(GCPU_SEL, SEL_KPL),%eax
	movl	%eax,%fs
	xorl	%eax,%eax
	movl	%eax,%gs
	movl	PCB_CR0(%esi),%eax
	movl	%eax,%cr0
	pushl	%ecx
	call	cpu_hatch
	/* NOTREACHED */

	.section .rodata
mp_tramp_data_start:
_TRMP_DATA_LABEL(.Lgdt_table)
	.word	0x0,0x0,0x0,0x0			# null GDTE
	 GDTE(0x9f,0xcf)			# Kernel text
	 GDTE(0x93,0xcf)			# Kernel data
_TRMP_DATA_OFFSET(.Lgdt_desc)
	.word	0x17				# limit 3 entries
	.long	.Lgdt_table			# where is gdt
mp_tramp_data_end:
