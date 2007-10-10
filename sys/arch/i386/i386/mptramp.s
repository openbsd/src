/*	$OpenBSD: mptramp.s,v 1.9 2007/10/10 15:53:51 art Exp $	*/

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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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

#define GDTE(a,b)	.byte	0xff,0xff,0x0,0x0,0x0,a,b,0x0
#define _RELOC(x)	((x) - KERNBASE)
#define RELOC(x)	_RELOC(_C_LABEL(x))

#define _TRMP_LABEL(a)  a = . - _C_LABEL(cpu_spinup_trampoline) + MP_TRAMPOLINE

/*
 * Debug code to stop aux. processors in various stages based on the
 * value in cpu_trace.
 *
 * %edi points at cpu_trace;  cpu_trace[0] is the "hold point";
 * cpu_trace[1] is the point which the cpu has reached.
 * cpu_trace[2] is the last value stored by HALTT.
 */


#ifdef MPDEBUG
#define HALT(x)	1: movl (%edi),%ebx;cmpl $ x,%ebx ; jle 1b ; movl $x,4(%edi)
#define HALTT(x,y)	movl y,8(%edi); HALT(x)
#else
#define HALT(x)	/**/
#define HALTT(x,y) /**/
#endif

	.globl	_C_LABEL(cpu),_C_LABEL(cpu_id),_C_LABEL(cpu_vendor)
	.globl	_C_LABEL(cpuid_level),_C_LABEL(cpu_feature)

	.global _C_LABEL(cpu_spinup_trampoline)
	.global _C_LABEL(cpu_spinup_trampoline_end)
	.global _C_LABEL(cpu_hatch)
	.global _C_LABEL(mp_pdirpa)
	.global _C_LABEL(gdt), _C_LABEL(local_apic)

	.text
	.align 4,0x0
	.code16
_C_LABEL(cpu_spinup_trampoline):
	cli
	xorw	%ax, %ax
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %ss
	data32 addr32 lgdt	(gdt_desc)	# load flat descriptor table
	movl	%cr0, %eax	# get cr0
	orl	$0x1, %eax	# enable protected mode
	movl	%eax, %cr0	# doit
	ljmp	$0x8, $mp_startup

_TRMP_LABEL(mp_startup)
	.code32

	movl	$0x10, %eax	# data segment
	movw	%ax, %ds
	movw	%ax, %ss
	movw	%ax, %es
	movw	%ax, %fs
	movw	%ax, %gs
	movl	$(MP_TRAMPOLINE+NBPG-16),%esp	# bootstrap stack end,
						# with scratch space..

#ifdef MPDEBUG
	leal	RELOC(cpu_trace),%edi
#endif

	HALT(0x1)
	/* First, reset the PSL. */
	pushl	$PSL_MBO
	popfl

	movl	RELOC(mp_pdirpa),%ecx
	HALTT(0x5,%ecx)

	/* Load base of page directory and enable mapping. */
	movl	%ecx,%cr3		# load ptd addr into mmu
	movl	%cr0,%eax		# get control word
					# enable paging & NPX emulation
	orl	$(CR0_PE|CR0_PG|CR0_NE|CR0_TS|CR0_EM|CR0_MP|CR0_WP),%eax
	movl	%eax,%cr0		# and let's page NOW!

#ifdef MPDEBUG
	leal	_C_LABEL(cpu_trace),%edi
#endif
	HALT(0x6)

# ok, we're now running with paging enabled and sharing page tables with cpu0.
# figure out which processor we really are, what stack we should be on, etc.

	movl	_C_LABEL(local_apic)+LAPIC_ID,%ecx
	shrl	$LAPIC_ID_SHIFT,%ecx
	leal	0(,%ecx,4),%ecx
	movl	_C_LABEL(cpu_info)(%ecx),%ecx

	HALTT(0x7, %ecx)

# %ecx points at our cpu_info structure..

	movw	$((MAXGDTSIZ*8) - 1), 6(%esp)	# prepare segment descriptor
	movl	CPU_INFO_GDT(%ecx), %eax	# for real gdt
	movl	%eax, 8(%esp)
	HALTT(0x8, %eax)
	lgdt	6(%esp)
	HALT(0x9)
	jmp	1f
	nop
1:
	HALT(0xa)
	movl	$GSEL(GDATA_SEL, SEL_KPL),%eax	#switch to new segment
	HALTT(0x10, %eax)
	movw	%ax,%ds
	HALT(0x11)
	movw	%ax,%es
	HALT(0x12)
	movw	%ax,%ss
	HALT(0x13)
	pushl	$GSEL(GCODE_SEL, SEL_KPL)
	pushl	$mp_cont
	HALT(0x14)
	lret
	.align 4,0x0
_TRMP_LABEL(gdt_table)
	.word	0x0,0x0,0x0,0x0			# null GDTE
	 GDTE(0x9f,0xcf)			# Kernel text
	 GDTE(0x93,0xcf)			# Kernel data
_TRMP_LABEL(gdt_desc)
	.word	0x17				# limit 3 entries
	.long	gdt_table			# where is gdt

_C_LABEL(cpu_spinup_trampoline_end):	#end of code copied to MP_TRAMPOLINE
mp_cont:

	movl	CPU_INFO_IDLE_PCB(%ecx),%esi

# %esi now points at our PCB.

	HALTT(0x19, %esi)

	movl	PCB_ESP(%esi),%esp
	movl	PCB_EBP(%esi),%ebp

	HALT(0x20)
	/* Switch address space. */
	movl	PCB_CR3(%esi),%eax
	HALTT(0x22, %eax)
	movl	%eax,%cr3
	HALT(0x25)
	/* Load segment registers. */
	movl	$GSEL(GCPU_SEL, SEL_KPL),%eax
	HALTT(0x26,%eax)
	movl	%eax,%fs
	xorl	%eax,%eax
	HALTT(0x27,%eax)
	movl	%eax,%gs
	movl	PCB_CR0(%esi),%eax
	HALTT(0x28,%eax)
	movl	%eax,%cr0
	HALTT(0x30,%ecx)
	pushl	%ecx
	call	_C_LABEL(cpu_hatch)
	/* NOTREACHED */

	.data
_C_LABEL(mp_pdirpa):
	.long	0
#ifdef MPDEBUG
	.global _C_LABEL(cpu_trace)
_C_LABEL(cpu_trace):
	.long	0x40
	.long	0xff
	.long	0xff
#endif
