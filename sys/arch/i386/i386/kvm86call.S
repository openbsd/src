/* $OpenBSD: kvm86call.S,v 1.5 2010/04/01 19:48:50 kettenis Exp $ */
/* $NetBSD: kvm86call.S,v 1.7 2006/04/11 17:14:07 drochner Exp $ */

/*-
 * Copyright (c) 1998 Jonathan Lemon
 * All rights reserved.
 * Copyright (c) 2002
 *	Matthias Drochner. All rights reserved.
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
 * $FreeBSD: src/sys/i386/i386/vm86bios.s,v 1.28 2001/12/11 23:33:40 jhb Exp $
 */

#include <machine/asm.h>
#include <machine/param.h>
#include "assym.h"

	.data
	.align 4

/* flag for trap() */
	.globl	kvm86_incall
kvm86_incall:		.long	0

/* XXX arguments for kvm86call() */
	.globl vm86pcb, vm86newptd, vm86frame, vm86pgtableva
	.globl	vm86tssd0, vm86tssd1

/* locals */
SCRARGFRAME:		.long	0
SCRSTACK:		.long	0
SCRTSS0:		.long	0
SCRTSS1:		.long	0

	.text

#if defined MULTIPROCESSOR
#include <machine/i82489reg.h>
#endif

/* int kvm86_call(struct trapframe *) */
ENTRY(kvm86_call)
	pushl	%ebp
	movl	%esp,%ebp		/* set up frame ptr */
	pushl	%esi
	pushl	%edi
	pushl	%ebx
	pushl	%fs
	pushl	%gs

	movl	8(%ebp),%eax
	movl	%eax,SCRARGFRAME	/* save argument pointer */

	movl	vm86frame,%edi		/* target frame location */
	movl    SCRARGFRAME,%esi	/* source (set on entry) */
	movl	$FRAMESIZE/4,%ecx	/* sizeof(struct trapframe)/4 */
	cld
	rep
	movsl				/* copy frame to new stack */

#ifdef MULTIPROCESSOR
	movl	_C_LABEL(local_apic)+LAPIC_ID,%eax
	shrl	$LAPIC_ID_SHIFT,%eax
	xorl	%ebx,%ebx
1:
	leal	0(,%ebx,4),%ecx
	incl	%ebx
	movl	_C_LABEL(cpu_info)(%ecx),%ecx
	movl	CPU_INFO_APICID(%ecx),%edx
	cmpl	%eax,%edx
	jne 1b
#else
	leal	_C_LABEL(cpu_info_primary),%ecx
#endif

	movl	CPU_INFO_CURPCB(%ecx),%eax
	pushl	%eax			/* save curpcb */
	
	movl	vm86pcb,%eax		
	movl	%eax,CPU_INFO_CURPCB(%ecx) /* set curpcb to vm86pcb */
	movl	CPU_INFO_CURPROC(%ecx),%ecx
	movl	P_MD_TSS_SEL(%ecx),%edi

#ifdef	MULTIPROCESSOR
	movl	CPU_INFO_GDT(%edx),%eax
#else
	movl	_C_LABEL(gdt),%eax
#endif
	andl	$~0x0200,4(%eax,%edi,1)	/* reset "task busy" */

	movl	0(%eax,%edi,1),%edx
	movl	%edx,SCRTSS0		/* save first word */
	movl	4(%eax,%edi,1),%edx
	movl	%edx,SCRTSS1		/* save second word */

	movl	vm86tssd0,%edx		/* vm86 tssd entry */
	movl	%edx,0(%eax,%edi,1)
	movl	vm86tssd1,%edx		/* vm86 tssd entry */
	movl	%edx,4(%eax,%edi,1)
	ltr	%di

	movl	%cr3,%eax
	pushl	%eax			/* save address space */
	movl	PTDpaddr,%ecx
	movl	%ecx,%ebx
	addl	$KERNBASE,%ebx		/* va of Idle PDP */
	movl	0(%ebx),%eax
	pushl	%eax			/* old pde */
	pushl	%ebx			/* keep address for reuse */

	movl	%esp,SCRSTACK		/* save current stack location */

	movl	vm86newptd,%eax		/* mapping for vm86 page table */
	movl	%eax,0(%ebx)		/* ... install as PDP entry 0 */

	movl	%ecx,%cr3		/* new page tables */
	movl	vm86frame,%esp		/* switch to new stack */

	movl	$1,kvm86_incall		/* set flag for trap() */

	/* INTRFASTEXIT */
	popl	%gs
	popl	%fs
	popl	%es
	popl	%ds
	popl	%edi
	popl	%esi
	popl	%ebp
	popl	%ebx
	popl	%edx
	popl	%ecx
	popl	%eax
	addl	$8,%esp
	iret


/* void kvm86_ret(struct trapframe *, int) */
ENTRY(kvm86_ret)
	pushl	%ebp
	movl	%esp,%ebp		/* set up frame ptr */

	movl	8(%ebp),%esi		/* source */
	movl	SCRARGFRAME,%edi	/* destination */
	movl	$FRAMESIZE/4,%ecx	/* size */
	cld
	rep
	movsl				/* copy frame to original frame */

	movl	SCRSTACK,%esp		/* back to old stack */
	popl	%ebx			/* saved va of Idle PDP */
	popl	%eax
	movl	%eax,0(%ebx)		/* restore old pde */
	popl	%eax
	movl	%eax,%cr3		/* install old page table */

	movl	$0,kvm86_incall		/* reset trapflag */

	
#ifdef MULTIPROCESSOR
	movl	_C_LABEL(local_apic)+LAPIC_ID,%eax
	shrl	$LAPIC_ID_SHIFT,%eax
	xorl	%ebx,%ebx
1:
	leal	0(,%ebx,4),%ecx
	incl	%ebx
	movl	_C_LABEL(cpu_info)(%ecx),%ecx
	movl	CPU_INFO_APICID(%ecx),%edx
	cmpl	%eax,%edx
	jne 1b
	movl	CPU_INFO_GDT(%ecx),%eax
#else
	leal	_C_LABEL(cpu_info_primary),%ecx
	movl	_C_LABEL(gdt),%eax
#endif
	movl	CPU_INFO_CURPROC(%ecx),%ecx
	movl	P_MD_TSS_SEL(%ecx),%edi
	movl	SCRTSS0, %edx
	movl	%edx, 0(%eax,%edi,1)	/* restore first word */
	movl	SCRTSS1, %edx
	movl	%edx, 4(%eax,%edi,1)	/* restore second word */
	ltr	%di


#ifdef MULTIPROCESSOR	
	movl	_C_LABEL(local_apic)+LAPIC_ID,%eax
	shrl	$LAPIC_ID_SHIFT,%eax
	xorl	%ebx,%ebx
1:
	leal	0(,%ebx,4),%ecx
	incl	%ebx
	movl	_C_LABEL(cpu_info)(%ecx),%ecx
	movl	CPU_INFO_APICID(%ecx),%edx
	cmpl	%eax,%edx
	jne 1b
#else
	leal	_C_LABEL(cpu_info_primary),%ecx
#endif
	popl	%eax			/* restore curpcb */
	movl	%eax,CPU_INFO_CURPCB(%ecx)

	movl	12(%ebp), %eax		/* will be kvm86_call()'s retval */

	popl	%gs
	popl	%fs
	popl	%ebx
	popl	%edi
	popl	%esi
	popl	%ebp
	ret				/* back to kvm86_call()'s caller */
