/*	$OpenBSD: mptramp.S,v 1.6 2010/04/01 19:47:59 kettenis Exp $	*/
/*	$NetBSD: mptramp.S,v 1.1 2003/04/26 18:39:30 fvdl Exp $	*/

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
 *        1)   Get the processors running kernel-code from a special
 *                  page-table and stack page, do chip identification.
 *        2)   halt the processors waiting for them to be enabled
 *              by a idle-thread 
 */
		
#include "assym.h"
#include <machine/param.h>
#include <machine/asm.h>
#include <machine/specialreg.h>
#include <machine/segments.h>
#include <machine/mpbiosvar.h>
#include <machine/i82489reg.h>
#include <machine/gdt.h>

#define _RELOC(x)       ((x) - KERNBASE)
#define RELOC(x)        _RELOC(_C_LABEL(x))

#define _TRMP_LABEL(a)  a = . - _C_LABEL(cpu_spinup_trampoline) + MP_TRAMPOLINE

	.globl	_C_LABEL(mpidle)
	.global _C_LABEL(cpu_spinup_trampoline)
	.global _C_LABEL(cpu_spinup_trampoline_end)
	.global _C_LABEL(cpu_hatch)
	.global _C_LABEL(local_apic)
	.global _C_LABEL(mp_pdirpa)

	.global	gdt64

#define HALT	1: jmp 1b

	.text
	.align 4,0x0
	.code16
_C_LABEL(cpu_spinup_trampoline):
	cli
	xorw    %ax,%ax
	movw    %ax, %ds
	movw    %ax, %es
	movw    %ax, %ss
	data32 addr32 lgdt (mptramp_gdt32_desc)   # load flat descriptor table
	movl    %cr0, %eax       # get cr0
	orl     $0x1, %eax      # enable protected mode
	movl    %eax, %cr0      # doit
	ljmpl	$0x8, $mp_startup

_TRMP_LABEL(mp_startup)
	.code32

	movl    $0x10, %eax     # data segment
	movw    %ax, %ds
	movw    %ax, %ss
	movw    %ax, %es
	movw    %ax, %fs
	movw    %ax, %gs
	
	movl    $ (MP_TRAMPOLINE+NBPG-16),%esp       # bootstrap stack end,
						     # with scratch space..

	/* First, reset the PSL. */
	pushl   $PSL_MBO
	popfl

	movl	%cr4,%eax
	orl	$(CR4_DEFAULT),%eax
	movl	%eax,%cr4

	movl	$MSR_EFER,%ecx
	rdmsr
	movl	%edx, %edi		# %edx is needed by wrmsr below

	# Check if we need to enable NXE
	movl	$0x80000001, %eax
	cpuid
	andl	$CPUID_NXE, %edx
	xorl	%eax,%eax
	testl	%edx, %edx
	jz	1f
	orl	$EFER_NXE, %eax
1:
	orl	$(EFER_LME|EFER_SCE), %eax

	movl	%edi, %edx		# Restore saved %edx
	movl	$MSR_EFER,%ecx
	wrmsr

	movl	$mp_tramp_pdirpa, %ecx
	movl	(%ecx), %ecx
        movl    %ecx,%cr3               # load ptd addr into mmu

	movl    $GSEL(GDATA_SEL, SEL_KPL),%eax 	#switch to new segment
	movl    %eax,%ds
	movl    %eax,%es
	movl    %eax,%ss

	movl	$mptramp_gdt64_desc,%eax
	lgdt	(%eax)
	movl	$mptramp_jmp64,%eax

	movl    %cr0,%ecx               # get control word
	orl     $(CR0_PE|CR0_PG|CR0_NE|CR0_TS|CR0_MP|CR0_WP),%ecx
	movl	%ecx, %cr0

	ljmp	*(%eax)

_TRMP_LABEL(mptramp_jmp64)
	.long   mptramp_longmode
	.word   GSEL(GCODE_SEL, SEL_KPL)

_TRMP_LABEL(mptramp_gdt32)
	.quad 0x0000000000000000
	.quad 0x00cf9f000000ffff
	.quad 0x00cf93000000ffff
_TRMP_LABEL(mptramp_gdt32_desc)
	.word 0x17
	.long mptramp_gdt32

_TRMP_LABEL(mptramp_gdt64)
	.quad 0x0000000000000000
	.quad 0x00af9a000000ffff
	.quad 0x00cf92000000ffff
_TRMP_LABEL(mptramp_gdt64_desc)
	.word 0x17
	.long mptramp_gdt64

	.global mp_pdirpa
_TRMP_LABEL(mp_pdirpa)
_TRMP_LABEL(mp_tramp_pdirpa)
	.long 0

_TRMP_LABEL(mptramp_longmode)
	.code64
	movabsq	$_C_LABEL(cpu_spinup_trampoline_end),%rax
	jmp	*%rax
	

	
_C_LABEL(cpu_spinup_trampoline_end):	#end of code copied to MP_TRAMPOLINE
	movl	_C_LABEL(local_apic)+LAPIC_ID,%eax
	shrl	$LAPIC_ID_SHIFT,%eax
	xorq	%rcx,%rcx
1:
	movq	_C_LABEL(cpu_info)(,%rcx,8),%rdi
	incq	%rcx
	movl	CPU_INFO_APICID(%rdi),%edx
	cmpl	%eax,%edx
	jne	1b

	movq	CPU_INFO_IDLE_PCB(%rdi),%rsi

	movq	PCB_RSP(%rsi),%rsp
	movq	PCB_RBP(%rsi),%rbp

	movq	CPU_INFO_GDT(%rdi),%rax
	movw	$(MAXGDTSIZ-1),-10(%rsp)
	movq	%rax,-8(%rsp)
	lgdt	-10(%rsp)

	/* Switch address space. */
	movq	PCB_CR3(%rsi),%rax
	movq	%rax,%cr3
	movl    PCB_CR0(%rsi),%eax
	movq    %rax,%cr0
	call	_C_LABEL(cpu_hatch)
	/* NOTREACHED */
