/*	$OpenBSD: locore.s,v 1.47 1999/02/26 04:34:31 art Exp $	*/
/*	$NetBSD: locore.s,v 1.145 1996/05/03 19:41:19 christos Exp $	*/

/*-
 * Copyright (c) 1993, 1994, 1995 Charles M. Hannum.  All rights reserved.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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

#include "npx.h"
#include "assym.h"
#include "apm.h"
#include "pctr.h"
#include "ksyms.h"

#include <sys/errno.h>
#include <sys/syscall.h>
#ifdef COMPAT_SVR4
#include <compat/svr4/svr4_syscall.h>
#endif
#ifdef COMPAT_LINUX
#include <compat/linux/linux_syscall.h>
#endif
#ifdef COMPAT_FREEBSD
#include <compat/freebsd/freebsd_syscall.h>
#endif

#include <machine/cputypes.h>
#include <machine/param.h>
#include <machine/pte.h>
#include <machine/segments.h>
#include <machine/specialreg.h>
#include <machine/trap.h>

#include <dev/isa/isareg.h>

/*
 * override user-land alignment before including asm.h
 */
#define	ALIGN_DATA	.align	2
#define	ALIGN_TEXT	.align	2,0x90	/* 4-byte boundaries, NOP-filled */
#define	SUPERALIGN_TEXT	.align	4,0x90	/* 16-byte boundaries better for 486 */
#define _ALIGN_TEXT	ALIGN_TEXT
#include <machine/asm.h>

/* NB: NOP now preserves registers so NOPs can be inserted anywhere */
/* XXX: NOP and FASTER_NOP are misleadingly named */
#ifdef DUMMY_NOPS	/* this will break some older machines */
#define	FASTER_NOP
#define	NOP
#else
#define	FASTER_NOP	pushl %eax ; inb $0x84,%al ; popl %eax
#define	NOP	pushl %eax ; inb $0x84,%al ; inb $0x84,%al ; popl %eax
#endif

/*
 * These are used on interrupt or trap entry or exit.
 */
#define	INTRENTRY \
	pushl	%eax		; \
	pushl	%ecx		; \
	pushl	%edx		; \
	pushl	%ebx		; \
	pushl	%ebp		; \
	pushl	%esi		; \
	pushl	%edi		; \
	pushl	%ds		; \
	pushl	%es		; \
	movl	$GSEL(GDATA_SEL, SEL_KPL),%eax	; \
	movl	%ax,%ds		; \
	movl	%ax,%es
#define	INTRFASTEXIT \
	popl	%es		; \
	popl	%ds		; \
	popl	%edi		; \
	popl	%esi		; \
	popl	%ebp		; \
	popl	%ebx		; \
	popl	%edx		; \
	popl	%ecx		; \
	popl	%eax		; \
	addl	$8,%esp		; \
	iret


/*
 * PTmap is recursive pagemap at top of virtual address space.
 * Within PTmap, the page directory can be found (third indirection).
 */
	.globl	_PTmap,_PTD,_PTDpde,_Sysmap
	.set	_PTmap,(PTDPTDI << PDSHIFT)
	.set	_PTD,(_PTmap + PTDPTDI * NBPG)
	.set	_PTDpde,(_PTD + PTDPTDI * 4)		# XXX 4 == sizeof pde
	.set	_Sysmap,(_PTmap + KPTDI * NBPG)

/*
 * APTmap, APTD is the alternate recursive pagemap.
 * It's used when modifying another process's page tables.
 */
	.globl	_APTmap,_APTD,_APTDpde
	.set	_APTmap,(APTDPTDI << PDSHIFT)
	.set	_APTD,(_APTmap + APTDPTDI * NBPG)
	.set	_APTDpde,(_PTD + APTDPTDI * 4)		# XXX 4 == sizeof pde

/*
 * Initialization
 */
	.data

	.globl	_cpu,_cpu_id,_cpu_vendor,_cpuid_level,_cpu_feature
	.globl	_cold,_cnvmem,_extmem,_esym
	.globl	_boothowto,_bootdev,_atdevbase
	.globl	_proc0paddr,_curpcb,_PTDpaddr,_dynamic_gdt
	.globl	_bootapiver, _bootargc, _bootargv

_cpu:		.long	0	# are we 386, 386sx, 486, 586 or 686
_cpu_id:	.long	0	# saved from `cpuid' instruction
_cpu_feature:	.long	0	# feature flags from 'cpuid' instruction
_cpuid_level:	.long	-1	# max. level accepted by 'cpuid' instruction
_cpu_vendor:	.space	16	# vendor string returned by `cpuid' instruction
_cold:		.long	1	# cold till we are not
_esym:		.long	0	# ptr to end of syms
_cnvmem:	.long	0	# conventional memory size
_extmem:	.long	0	# extended memory size
_boothowto:	.long	0	# boot flags
_atdevbase:	.long	0	# location of start of iomem in virtual
_bootapiver:	.long	0	# /boot API version
_bootargc:	.long	0	# /boot argc
_bootargv:	.long	0	# /boot argv
_proc0paddr:	.long	0
_PTDpaddr:	.long	0	# paddr of PTD, for libkvm

	.space 512
tmpstk:


#define	RELOC(x)	((x) - KERNBASE)

	.text
	.globl	start
	.globl	_kernel_text
	_kernel_text = KERNTEXTOFF
start:	movw	$0x1234,0x472			# warm boot

	/*
	 * Load parameters from stack (howto, bootdev, unit, bootapiver, esym).
	 * note: (%esp) is return address of boot
	 * (If we want to hold onto /boot, it's physical %esp up to _end.)
	 */
	movl	4(%esp),%eax
	movl	%eax,RELOC(_boothowto)
	movl	8(%esp),%eax
	movl	%eax,RELOC(_bootdev)
 	movl	16(%esp),%eax
	testl	%eax,%eax
	jz	1f
	addl	$KERNBASE,%eax
1: 	movl	%eax,RELOC(_esym)

	movl	20(%esp),%eax
	movl	%eax,RELOC(_extmem)
	movl	24(%esp),%eax
	movl	%eax,RELOC(_cnvmem)

	movl	12(%esp),%eax
	movl	%eax,RELOC(_bootapiver)
	movl	28(%esp), %eax
	movl	%eax, RELOC(_bootargc)
	movl	32(%esp), %eax
	movl	%eax, RELOC(_bootargv)

	/* First, reset the PSL. */
	pushl	$PSL_MBO
	popfl

	/* Find out our CPU type. */

try386:	/* Try to toggle alignment check flag; does not exist on 386. */
	pushfl
	popl	%eax
	movl	%eax,%ecx
	orl	$PSL_AC,%eax
	pushl	%eax
	popfl
	pushfl
	popl	%eax
	xorl	%ecx,%eax
	andl	$PSL_AC,%eax
	pushl	%ecx
	popfl

	testl	%eax,%eax
	jnz	try486

	/*
	 * Try the test of a NexGen CPU -- ZF will not change on a DIV
	 * instruction on a NexGen, it will on an i386.  Documented in
	 * Nx586 Processor Recognition Application Note, NexGen, Inc.
	 */
	movl	$0x5555,%eax
	xorl	%edx,%edx
	movl	$2,%ecx
	divl	%ecx
	jnz	is386

isnx586:
	/*
	 * Don't try cpuid, as Nx586s reportedly don't support the
	 * PSL_ID bit.
	 */
	movl	$CPU_NX586,RELOC(_cpu)
	jmp	2f

is386:
	movl	$CPU_386,RELOC(_cpu)
	jmp	2f

try486:	/* Try to toggle identification flag; does not exist on early 486s. */
	pushfl
	popl	%eax
	movl	%eax,%ecx
	xorl	$PSL_ID,%eax
	pushl	%eax
	popfl
	pushfl
	popl	%eax
	xorl	%ecx,%eax
	andl	$PSL_ID,%eax
	pushl	%ecx
	popfl

	testl	%eax,%eax
	jnz	try586
is486:	movl	$CPU_486,RELOC(_cpu)

	/*
	 * Check Cyrix CPU
	 * Cyrix CPUs do not change the undefined flags following
	 * execution of the divide instruction which divides 5 by 2.
	 *
	 * Note: CPUID is enabled on M2, so it passes another way.
	 */
	pushfl
	movl	$0x5555, %eax
	xorl	%edx, %edx
	movl	$2, %ecx
	clc
	divl	%ecx
	jnc	trycyrix486
	popfl
	jmp	2f
trycyrix486:
	movl	$CPU_6x86,RELOC(_cpu)	# set CPU type
	/*
	 * Check for Cyrix 486 CPU by seeing if the flags change during a
	 * divide.  This is documented in the Cx486SLC/e SMM Programmer's
	 * Guide.
	 */
	xorl	%edx,%edx
	cmpl	%edx,%edx		# set flags to known state
	pushfl
	popl	%ecx			# store flags in ecx
	movl	$-1,%eax
	movl	$4,%ebx
	divl	%ebx			# do a long division
	pushfl
	popl	%eax
	xorl	%ecx,%eax		# are the flags different?
	testl	$0x8d5,%eax		# only check C|PF|AF|Z|N|V
	jne	2f			# yes; must not be Cyrix CPU
	movl	$CPU_486DLC,RELOC(_cpu) 	# set CPU type

#ifndef CYRIX_CACHE_WORKS
	/* Disable caching of the ISA hole only. */
	invd
	movb	$CCR0,%al		# Configuration Register index (CCR0)
	outb	%al,$0x22
	inb	$0x23,%al
	orb	$(CCR0_NC1|CCR0_BARB),%al
	movb	%al,%ah
	movb	$CCR0,%al
	outb	%al,$0x22
	movb	%ah,%al
	outb	%al,$0x23
	invd
#else /* CYRIX_CACHE_WORKS */
	/* Set cache parameters */
	invd				# Start with guaranteed clean cache
	movb	$CCR0,%al		# Configuration Register index (CCR0)
	outb	%al,$0x22
	inb	$0x23,%al
	andb	$~CCR0_NC0,%al
#ifndef CYRIX_CACHE_REALLY_WORKS
	orb	$(CCR0_NC1|CCR0_BARB),%al
#else
	orb	$CCR0_NC1,%al
#endif
	movb	%al,%ah
	movb	$CCR0,%al
	outb	%al,$0x22
	movb	%ah,%al
	outb	%al,$0x23
	/* clear non-cacheable region 1	*/
	movb	$(NCR1+2),%al
	outb	%al,$0x22
	movb	$NCR_SIZE_0K,%al
	outb	%al,$0x23
	/* clear non-cacheable region 2	*/
	movb	$(NCR2+2),%al
	outb	%al,$0x22
	movb	$NCR_SIZE_0K,%al
	outb	%al,$0x23
	/* clear non-cacheable region 3	*/
	movb	$(NCR3+2),%al
	outb	%al,$0x22
	movb	$NCR_SIZE_0K,%al
	outb	%al,$0x23
	/* clear non-cacheable region 4	*/
	movb	$(NCR4+2),%al
	outb	%al,$0x22
	movb	$NCR_SIZE_0K,%al
	outb	%al,$0x23
	/* enable caching in CR0 */
	movl	%cr0,%eax
	andl	$~(CR0_CD|CR0_NW),%eax
	movl	%eax,%cr0
	invd
#endif /* CYRIX_CACHE_WORKS */

	jmp	2f

try586:	/* Use the `cpuid' instruction. */
	xorl	%eax,%eax
	cpuid
	movl	%eax,RELOC(_cpuid_level)
	movl	%ebx,RELOC(_cpu_vendor)	# store vendor string
	movl	%edx,RELOC(_cpu_vendor)+4
	movl	%ecx,RELOC(_cpu_vendor)+8
	movl	$0,  RELOC(_cpu_vendor)+12

	movl	$1,%eax
	cpuid
	movl	%eax,RELOC(_cpu_id)	# store cpu_id and features
	movl	%edx,RELOC(_cpu_feature)

2:
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
	movl	$RELOC(tmpstk),%esp	# bootstrap stack end location

/*
 * Virtual address space of kernel:
 *
 * text | data | bss | [syms] | page dir | proc0 kstack | Sysmap
 *			      0          1       2      3
 */
#define	PROC0PDIR	((0)              * NBPG)
#define	PROC0STACK	((1)              * NBPG)
#define	SYSMAP		((1+UPAGES)       * NBPG)
#define	TABLESIZE	((1+UPAGES+NKPDE) * NBPG)

	/* Clear the BSS. */
	movl	$RELOC(_edata),%edi
	movl	$_end,%ecx
	subl	$_edata,%ecx
	addl	$3,%ecx
	shrl	$2,%ecx
	xorl	%eax,%eax
	cld
	rep
	stosl

	/* Find end of kernel image. */
	movl	$RELOC(_end),%esi
#if (defined(DDB) || NKSYMS > 0) && !defined(SYMTAB_SPACE)
	/* Save the symbols (if loaded). */
	movl	RELOC(_esym),%eax
	testl	%eax,%eax
	jz	1f
	subl	$KERNBASE,%eax
	movl	%eax,%esi
1:
#endif

	/* Calculate where to start the bootstrap tables. */
	addl	$PGOFSET, %esi			# page align up
	andl	$~PGOFSET, %esi

	/* Clear memory for bootstrap tables. */
	movl	%esi, %edi
	movl	$((TABLESIZE + 3) >> 2), %ecx	# size of tables
	xorl	%eax, %eax
	cld
	rep
	stosl

/*
 * fillkpt
 *	eax = pte (page frame | control | status)
 *	ebx = page table address
 *	ecx = number of pages to map
 */
#define	fillkpt		\
1:	movl	%eax,(%ebx)	; \
	addl	$NBPG,%eax	; /* increment physical address */ \
	addl	$4,%ebx		; /* next pte */ \
	loop	1b		;

/*
 * Build initial page tables.
 */
	/* Calculate end of text segment, rounded to a page. */
	leal	(RELOC(_etext)+PGOFSET),%edx
	andl	$~PGOFSET,%edx
	
	/* Skip over the first 1MB. */
	movl	$RELOC(KERNTEXTOFF),%eax
	movl	%eax,%ecx
	shrl	$PGSHIFT,%ecx
	leal	(SYSMAP)(%esi,%ecx,4),%ebx

	/* Map the kernel text read-only. */
	movl	%edx,%ecx
	subl	%eax,%ecx
	shrl	$PGSHIFT,%ecx
#ifdef DDB
	orl	$(PG_V|PG_KW),%eax
#else
	orl	$(PG_V|PG_KR),%eax
#endif
	fillkpt

	/* Map the data, BSS, and bootstrap tables read-write. */
	leal	(PG_V|PG_KW)(%edx),%eax
	leal	(TABLESIZE)(%esi),%ecx			# end of tables
	subl	%edx,%ecx				# subtract end of text
	shrl	$PGSHIFT,%ecx
	fillkpt

	/* Map ISA I/O memory. */
	movl	$(IOM_BEGIN|PG_V|PG_KW/*|PG_N*/),%eax	# having these bits set
	movl	$(IOM_SIZE>>PGSHIFT),%ecx		# for this many pte s,
	fillkpt

/*
 * Construct a page table directory.
 *
 * Install a PDE for temporary double map of kernel text.
 * Maps two pages, in case the kernel is larger than 4M.
 * XXX: should the number of pages to map be decided at run-time?
 */
	leal	(SYSMAP+PG_V|PG_KW)(%esi),%eax		# calc Sysmap physaddr
	movl	%eax,(PROC0PDIR+0*4)(%esi)		# map it in
	addl	$NBPG, %eax				# 2nd Sysmap page
	movl	%eax,(PROC0PDIR+1*4)(%esi)		# map it too
	/* code below assumes %eax == sysmap physaddr, so we adjust it back */
	subl	$NBPG, %eax

/*
 * Map kernel PDEs: this is the real mapping used 
 * after the temp mapping outlives its usefulness.
 */
	movl	$NKPDE,%ecx				# count of pde's
	leal	(PROC0PDIR+KPTDI*4)(%esi),%ebx		# map them high
	fillkpt

	/* Install a PDE recursively mapping page directory as a page table! */
	leal	(PROC0PDIR+PG_V|PG_KW)(%esi),%eax	# pte for ptd
	movl	%eax,(PROC0PDIR+PTDPTDI*4)(%esi)	# phys addr from above

	/* Save phys. addr of PTD, for libkvm. */
	movl	%esi,RELOC(_PTDpaddr)

	/* Load base of page directory and enable mapping. */
	movl	%esi,%eax		# phys address of ptd in proc 0
	movl	%eax,%cr3		# load ptd addr into mmu
	movl	%cr0,%eax		# get control word
					# enable paging & NPX emulation
	orl	$(CR0_PE|CR0_PG|CR0_NE|CR0_TS|CR0_EM|CR0_MP),%eax
	movl	%eax,%cr0		# and let's page NOW!

	pushl	$begin			# jump to high mem
	ret

begin:
	/* Now running relocated at KERNBASE.  Remove double mapping. */
	movl	$0,(PROC0PDIR+0*4)(%esi)
	movl	$0,(PROC0PDIR+1*4)(%esi)

	/* Relocate atdevbase. */
	leal	(TABLESIZE+KERNBASE)(%esi),%edx
	movl	%edx,_atdevbase

	/* Set up bootstrap stack. */
	leal	(PROC0STACK+KERNBASE)(%esi),%eax
	movl	%eax,_proc0paddr
	leal	(USPACE-FRAMESIZE)(%eax),%esp
	movl	%esi,PCB_CR3(%eax)	# pcb->pcb_cr3
	xorl	%ebp,%ebp               # mark end of frames

	leal	(TABLESIZE)(%esi),%eax	# skip past stack and page tables
	pushl	%eax
	call	_init386		# wire 386 chip for unix operation
	addl	$4,%esp

	/* Clear segment registers; always null in proc0. */
	xorl	%ecx,%ecx
	movl	%cx,%fs
	movl	%cx,%gs

	call 	_main

NENTRY(proc_trampoline)
	pushl	%ebx
	call	%esi
	addl	$4,%esp
	INTRFASTEXIT
	/* NOTREACHED */

/*****************************************************************************/

/*
 * Signal trampoline; copied to top of user stack.
 */
NENTRY(sigcode)
	call	SIGF_HANDLER(%esp)
	leal	SIGF_SC(%esp),%eax	# scp (the call may have clobbered the
					# copy at SIGF_SCP(%esp))
#ifdef VM86
	testl	$PSL_VM,SC_EFLAGS(%eax)
	jnz	1f
#endif
	movl	SC_FS(%eax),%ecx
	movl	SC_GS(%eax),%edx
	movl	%cx,%fs
	movl	%dx,%gs
1:	pushl	%eax
	pushl	%eax			# junk to fake return address
	movl	$SYS_sigreturn,%eax
	int	$0x80	 		# enter kernel with args on stack
	movl	$SYS_exit,%eax
	int	$0x80			# exit if sigreturn fails
	.globl	_esigcode
_esigcode:

/*****************************************************************************/

#ifdef COMPAT_SVR4
NENTRY(svr4_sigcode)
	call	SVR4_SIGF_HANDLER(%esp)
	leal	SVR4_SIGF_UC(%esp),%eax	# ucp (the call may have clobbered the
					# copy at SIGF_UCP(%esp))
#ifdef VM86
	testl	$PSL_VM,SVR4_UC_EFLAGS(%eax)
	jnz	1f
#endif
	movl	SVR4_UC_FS(%eax),%ecx
	movl	SVR4_UC_GS(%eax),%edx
	movl	%cx,%fs
	movl	%dx,%gs
1:	pushl	%eax
	pushl	$1			# setcontext(p) == syscontext(1, p) 
	pushl	%eax			# junk to fake return address
	movl	$SVR4_SYS_context,%eax
	int	$0x80	 		# enter kernel with args on stack
	movl	$SVR4_SYS_exit,%eax
	int	$0x80			# exit if sigreturn fails
	.globl	_svr4_esigcode
_svr4_esigcode:
#endif

/*****************************************************************************/

#ifdef COMPAT_LINUX
/*
 * Signal trampoline; copied to top of user stack.
 */
NENTRY(linux_sigcode)
	call	LINUX_SIGF_HANDLER(%esp)
	leal	LINUX_SIGF_SC(%esp),%ebx # scp (the call may have clobbered the
					# copy at SIGF_SCP(%esp))
#ifdef VM86
	testl	$PSL_VM,LINUX_SC_EFLAGS(%ebx)
	jnz	1f
#endif
	movl	LINUX_SC_FS(%ebx),%ecx
	movl	LINUX_SC_GS(%ebx),%edx
	movl	%cx,%fs
	movl	%dx,%gs
1:	pushl	%eax			# junk to fake return address
	movl	$LINUX_SYS_sigreturn,%eax
	int	$0x80	 		# enter kernel with args on stack
	movl	$LINUX_SYS_exit,%eax
	int	$0x80			# exit if sigreturn fails
	.globl	_linux_esigcode
_linux_esigcode:
#endif

/*****************************************************************************/

#ifdef COMPAT_FREEBSD
/*
 * Signal trampoline; copied to top of user stack.
 */
NENTRY(freebsd_sigcode)
	call	FREEBSD_SIGF_HANDLER(%esp)
	leal	FREEBSD_SIGF_SC(%esp),%eax # scp (the call may have clobbered
					# the copy at SIGF_SCP(%esp))
	pushl	%eax
	pushl	%eax			# junk to fake return address
	movl	$FREEBSD_SYS_sigreturn,%eax
	int	$0x80	 		# enter kernel with args on stack
	movl	$FREEBSD_SYS_exit,%eax
	int	$0x80			# exit if sigreturn fails
	.globl	_freebsd_esigcode
_freebsd_esigcode:
#endif

/*****************************************************************************/

/*
 * The following primitives are used to fill and copy regions of memory.
 */

/*
 * fillw(short pattern, caddr_t addr, size_t len);
 * Write len copies of pattern at addr.
 */
ENTRY(fillw)
	pushl	%edi
	movl	8(%esp),%eax
	movl	12(%esp),%edi
	movw	%ax,%cx
	rorl	$16,%eax
	movw	%cx,%ax
	cld
	movl	16(%esp),%ecx
	shrl	%ecx			# do longwords
	rep
	stosl
	movl	16(%esp),%ecx
	andl	$1,%ecx			# do remainder
	rep
	stosw
	popl	%edi
	ret

/*
 * bcopyb(caddr_t from, caddr_t to, size_t len);
 * Copy len bytes, one byte at a time.
 */
ENTRY(bcopyb)
	pushl	%esi
	pushl	%edi
	movl	12(%esp),%esi
	movl	16(%esp),%edi
	movl	20(%esp),%ecx
	cmpl	%esi,%edi		# potentially overlapping?
	jnb	1f
	cld				# no; copy forward
	rep
	movsb
	popl	%edi
	popl	%esi
	ret

	ALIGN_TEXT
1:	addl	%ecx,%edi		# copy backward
	addl	%ecx,%esi
	std
	decl	%edi
	decl	%esi
	rep
	movsb
	popl	%edi
	popl	%esi
	cld
	ret

#if defined(UVM)
/*
 * kcopy(caddr_t from, caddr_t to, size_t len);
 * Copy len bytes, abort on fault.
 */
ENTRY(kcopy)
	pushl	%esi
	pushl	%edi
	movl	_C_LABEL(curpcb),%eax	# load curpcb into eax and set on-fault
	pushl	PCB_ONFAULT(%eax)
	movl	$_C_LABEL(copy_fault), PCB_ONFAULT(%eax)

	movl	16(%esp),%esi
	movl	20(%esp),%edi
	movl	24(%esp),%ecx
	movl	%edi,%eax
	subl	%esi,%eax
	cmpl	%ecx,%eax		# overlapping?
	jb	1f
	cld				# nope, copy forward
	shrl	$2,%ecx			# copy by 32-bit words
	rep
	movsl
	movl	24(%esp),%ecx
	andl	$3,%ecx			# any bytes left?
	rep
	movsb

	movl	_C_LABEL(curpcb),%edx
	popl	PCB_ONFAULT(%edx)
	popl	%edi
	popl	%esi
	xorl	%eax,%eax
	ret

	ALIGN_TEXT
1:	addl	%ecx,%edi		# copy backward
	addl	%ecx,%esi
	std
	andl	$3,%ecx			# any fractional bytes?
	decl	%edi
	decl	%esi
	rep
	movsb
	movl	24(%esp),%ecx		# copy remainder by 32-bit words
	shrl	$2,%ecx
	subl	$3,%esi
	subl	$3,%edi
	rep
	movsl
	cld

	movl	_C_LABEL(curpcb),%edx
	popl	PCB_ONFAULT(%edx)
	popl	%edi
	popl	%esi
	xorl	%eax,%eax
	ret
#endif
	
/*
 * bcopyw(caddr_t from, caddr_t to, size_t len);
 * Copy len bytes, two bytes at a time.
 */
ENTRY(bcopyw)
	pushl	%esi
	pushl	%edi
	movl	12(%esp),%esi
	movl	16(%esp),%edi
	movl	20(%esp),%ecx
	cmpl	%esi,%edi		# potentially overlapping?
	jnb	1f
	cld				# no; copy forward
	shrl	$1,%ecx			# copy by 16-bit words
	rep
	movsw
	adc	%ecx,%ecx		# any bytes left?
	rep
	movsb
	popl	%edi
	popl	%esi
	ret

	ALIGN_TEXT
1:	addl	%ecx,%edi		# copy backward
	addl	%ecx,%esi
	std
	andl	$1,%ecx			# any fractional bytes?
	decl	%edi
	decl	%esi
	rep
	movsb
	movl	20(%esp),%ecx		# copy remainder by 16-bit words
	shrl	$1,%ecx
	decl	%esi
	decl	%edi
	rep
	movsw
	popl	%edi
	popl	%esi
	cld
	ret

/*
 * bcopy(caddr_t from, caddr_t to, size_t len);
 * Copy len bytes.
 */
ALTENTRY(ovbcopy)
ENTRY(bcopy)
	pushl	%esi
	pushl	%edi
	movl	12(%esp),%esi
	movl	16(%esp),%edi
	movl	20(%esp),%ecx
	movl	%edi,%eax
	subl	%esi,%eax
	cmpl	%ecx,%eax		# overlapping?
	jb	1f
	cld				# nope, copy forward
	shrl	$2,%ecx			# copy by 32-bit words
	rep
	movsl
	movl	20(%esp),%ecx
	andl	$3,%ecx			# any bytes left?
	rep
	movsb
	popl	%edi
	popl	%esi
	ret

	ALIGN_TEXT
1:	addl	%ecx,%edi		# copy backward
	addl	%ecx,%esi
	std
	andl	$3,%ecx			# any fractional bytes?
	decl	%edi
	decl	%esi
	rep
	movsb
	movl	20(%esp),%ecx		# copy remainder by 32-bit words
	shrl	$2,%ecx
	subl	$3,%esi
	subl	$3,%edi
	rep
	movsl
	popl	%edi
	popl	%esi
	cld
	ret

/*****************************************************************************/

/*
 * The following primitives are used to copy data in and out of the user's
 * address space.
 */

/*
 * copyout(caddr_t from, caddr_t to, size_t len);
 * Copy len bytes into the user's address space.
 */
ENTRY(copyout)
	pushl	%esi
	pushl	%edi
	movl	_curpcb,%eax
	movl	$_copy_fault,PCB_ONFAULT(%eax)
	
	movl	12(%esp),%esi
	movl	16(%esp),%edi
	movl	20(%esp),%eax

	/*
	 * We check that the end of the destination buffer is not past the end
	 * of the user's address space.  If it's not, then we only need to
	 * check that each page is writable.  The 486 will do this for us; the
	 * 386 will not.  (We assume that pages in user space that are not
	 * writable by the user are not writable by the kernel either.)
	 */
	movl	%edi,%edx
	addl	%eax,%edx
	jc	_copy_fault
	cmpl	$VM_MAXUSER_ADDRESS,%edx
	ja	_copy_fault

#if defined(I386_CPU)
#if defined(I486_CPU) || defined(I586_CPU) || defined(I686_CPU)
	cmpl	$CPUCLASS_386,_cpu_class
	jne	3f
#endif /* I486_CPU || I586_CPU || I686_CPU */

	testl	%eax,%eax		# anything to do?
	jz	3f

	/*
	 * We have to check each PTE for (write) permission, since the CPU
	 * doesn't do it for us.
	 */

	/* Compute number of pages. */
	movl	%edi,%ecx
	andl	$PGOFSET,%ecx
	addl	%eax,%ecx
	decl	%ecx
	shrl	$PGSHIFT,%ecx

	/* Compute PTE offset for start address. */
	shrl	$PGSHIFT,%edi

1:	/* Check PTE for each page. */
	testb	$PG_RW,_PTmap(,%edi,4)
	jz	2f
	
4:	incl	%edi
	decl	%ecx
	jns	1b

	movl	16(%esp),%edi
	jmp	3f
	
2:	/* Simulate a trap. */
	pushl	%eax
	pushl	%ecx
	movl	%edi,%eax
	shll	$PGSHIFT,%eax
	pushl	%eax
	call	_trapwrite		# trapwrite(addr)
	addl	$4,%esp			# pop argument
	popl	%ecx
	testl	%eax,%eax		# if not ok, return EFAULT
	popl	%eax
	jz	4b
	jmp	_copy_fault
#endif /* I386_CPU */

3:	/* bcopy(%esi, %edi, %eax); */
	cld
	movl	%eax,%ecx
	shrl	$2,%ecx
	rep
	movsl
	movb	%al,%cl
	andb	$3,%cl
	rep
	movsb
	xorl	%eax,%eax

	popl	%edi
	popl	%esi
	movl	_curpcb,%edx
	movl	%eax,PCB_ONFAULT(%edx)
	ret

/*
 * copyin(caddr_t from, caddr_t to, size_t len);
 * Copy len bytes from the user's address space.
 */
ENTRY(copyin)
	pushl	%esi
	pushl	%edi
	movl	_curpcb,%eax
	movl	$_copy_fault,PCB_ONFAULT(%eax)
	
	movl	12(%esp),%esi
	movl	16(%esp),%edi
	movl	20(%esp),%eax

	/*
	 * We check that the end of the destination buffer is not past the end
	 * of the user's address space.  If it's not, then we only need to
	 * check that each page is readable, and the CPU will do that for us.
	 */
	movl	%esi,%edx
	addl	%eax,%edx
	jc	_copy_fault
	cmpl	$VM_MAXUSER_ADDRESS,%edx
	ja	_copy_fault

3:	/* bcopy(%esi, %edi, %eax); */
	cld
	movl	%eax,%ecx
	shrl	$2,%ecx
	rep
	movsl
	movb	%al,%cl
	andb	$3,%cl
	rep
	movsb
	xorl	%eax,%eax

	popl	%edi
	popl	%esi
	movl	_curpcb,%edx
	movl	%eax,PCB_ONFAULT(%edx)
	ret

ENTRY(copy_fault)
	popl	%edi
	popl	%esi
	movl	_curpcb,%edx
	movl	$0,PCB_ONFAULT(%edx)
	movl	$EFAULT,%eax
	ret

/*
 * copyoutstr(caddr_t from, caddr_t to, size_t maxlen, size_t *lencopied);
 * Copy a NUL-terminated string, at most maxlen characters long, into the
 * user's address space.  Return the number of characters copied (including the
 * NUL) in *lencopied.  If the string is too long, return ENAMETOOLONG; else
 * return 0 or EFAULT.
 */
ENTRY(copyoutstr)
	pushl	%esi
	pushl	%edi
	movl	_curpcb,%ecx
	movl	$_copystr_fault,PCB_ONFAULT(%ecx)

	movl	12(%esp),%esi		# esi = from
	movl	16(%esp),%edi		# edi = to
	movl	20(%esp),%edx		# edx = maxlen

#if defined(I386_CPU)
#if defined(I486_CPU) || defined(I586_CPU) || defined(I686_CPU)
	cmpl	$CPUCLASS_386,_cpu_class
	jne	5f
#endif /* I486_CPU || I586_CPU || I686_CPU */

	/* Compute number of bytes in first page. */
	movl	%edi,%eax
	andl	$PGOFSET,%eax
	movl	$NBPG,%ecx
	subl	%eax,%ecx		# ecx = NBPG - (src % NBPG)

	/* Compute PTE offset for start address. */
	movl	%edi,%eax
	shrl	$PGSHIFT,%eax		# calculate pte address

1:	/*
	 * Once per page, check that we are still within the bounds of user
	 * space, and check for a write fault.
	 */
	cmpl	$VM_MAXUSER_ADDRESS,%edi
	jae	_copystr_fault

	testb	$PG_RW,_PTmap(,%eax,4)
	jnz	2f

	/* Simulate a trap. */
	pushl	%eax
	pushl	%edx
	pushl	%edi
	call	_trapwrite		# trapwrite(addr)
	addl	$4,%esp			# clear argument from stack
	popl	%edx
	testl	%eax,%eax
	popl	%eax
	jnz	_copystr_fault

2:	/* Copy up to end of this page. */
	subl	%ecx,%edx		# predecrement total count
	jnc	6f
	addl	%edx,%ecx		# ecx += (edx - ecx) = edx
	xorl	%edx,%edx

6:	pushl	%eax			# save PT index
3:	decl	%ecx
	js	4f
	lodsb
	stosb
	testb	%al,%al
	jnz	3b

	/* Success -- 0 byte reached. */
	addl	$4,%esp			# discard PT index
	addl	%ecx,%edx		# add back residual for this page
	xorl	%eax,%eax
	jmp	copystr_return

4:	/* Go to next page, if any. */
	popl	%eax			# restore PT index
	movl	$NBPG,%ecx
	incl	%eax
	testl	%edx,%edx
	jnz	1b

	/* edx is zero -- return ENAMETOOLONG. */
	movl	$ENAMETOOLONG,%eax
	jmp	copystr_return
#endif /* I386_CPU */

#if defined(I486_CPU) || defined(I586_CPU) || defined(I686_CPU)
5:	/*
	 * Get min(%edx, VM_MAXUSER_ADDRESS-%edi).
	 */
	movl	$VM_MAXUSER_ADDRESS,%eax
	subl	%edi,%eax
	cmpl	%edx,%eax
	jae	1f
	movl	%eax,%edx
	movl	%eax,20(%esp)

1:	incl	%edx
	cld

1:	decl	%edx
	jz	2f
	lodsb
	stosb
	testb	%al,%al
	jnz	1b

	/* Success -- 0 byte reached. */
	decl	%edx
	xorl	%eax,%eax
	jmp	copystr_return

2:	/* edx is zero -- return EFAULT or ENAMETOOLONG. */
	cmpl	$VM_MAXUSER_ADDRESS,%edi
	jae	_copystr_fault
	movl	$ENAMETOOLONG,%eax
	jmp	copystr_return
#endif /* I486_CPU || I586_CPU || I686_CPU */

/*
 * copyinstr(caddr_t from, caddr_t to, size_t maxlen, size_t *lencopied);
 * Copy a NUL-terminated string, at most maxlen characters long, from the
 * user's address space.  Return the number of characters copied (including the
 * NUL) in *lencopied.  If the string is too long, return ENAMETOOLONG; else
 * return 0 or EFAULT.
 */
ENTRY(copyinstr)
	pushl	%esi
	pushl	%edi
	movl	_curpcb,%ecx
	movl	$_copystr_fault,PCB_ONFAULT(%ecx)

	movl	12(%esp),%esi		# %esi = from
	movl	16(%esp),%edi		# %edi = to
	movl	20(%esp),%edx		# %edx = maxlen

	/*
	 * Get min(%edx, VM_MAXUSER_ADDRESS-%esi).
	 */
	movl	$VM_MAXUSER_ADDRESS,%eax
	subl	%esi,%eax
	cmpl	%edx,%eax
	jae	1f
	movl	%eax,%edx
	movl	%eax,20(%esp)

1:	incl	%edx
	cld

1:	decl	%edx
	jz	2f
	lodsb
	stosb
	testb	%al,%al
	jnz	1b

	/* Success -- 0 byte reached. */
	decl	%edx
	xorl	%eax,%eax
	jmp	copystr_return

2:	/* edx is zero -- return EFAULT or ENAMETOOLONG. */
	cmpl	$VM_MAXUSER_ADDRESS,%esi
	jae	_copystr_fault
	movl	$ENAMETOOLONG,%eax
	jmp	copystr_return

ENTRY(copystr_fault)
	movl	$EFAULT,%eax

copystr_return:	
	/* Set *lencopied and return %eax. */
	movl	_curpcb,%ecx
	movl	$0,PCB_ONFAULT(%ecx)
	movl	20(%esp),%ecx
	subl	%edx,%ecx
	movl	24(%esp),%edx
	testl	%edx,%edx
	jz	8f
	movl	%ecx,(%edx)

8:	popl	%edi
	popl	%esi
	ret

/*
 * copystr(caddr_t from, caddr_t to, size_t maxlen, size_t *lencopied);
 * Copy a NUL-terminated string, at most maxlen characters long.  Return the
 * number of characters copied (including the NUL) in *lencopied.  If the
 * string is too long, return ENAMETOOLONG; else return 0.
 */
ENTRY(copystr)
	pushl	%esi
	pushl	%edi

	movl	12(%esp),%esi		# esi = from
	movl	16(%esp),%edi		# edi = to
	movl	20(%esp),%edx		# edx = maxlen
	incl	%edx
	cld

1:	decl	%edx
	jz	4f
	lodsb
	stosb
	testb	%al,%al
	jnz	1b

	/* Success -- 0 byte reached. */
	decl	%edx
	xorl	%eax,%eax
	jmp	6f

4:	/* edx is zero -- return ENAMETOOLONG. */
	movl	$ENAMETOOLONG,%eax

6:	/* Set *lencopied and return %eax. */
	movl	20(%esp),%ecx
	subl	%edx,%ecx
	movl	24(%esp),%edx
	testl	%edx,%edx
	jz	7f
	movl	%ecx,(%edx)

7:	popl	%edi
	popl	%esi
	ret

/*
 * fuword(caddr_t uaddr);
 * Fetch an int from the user's address space.
 */
ENTRY(fuword)
	movl	4(%esp),%edx
	cmpl	$VM_MAXUSER_ADDRESS-4,%edx
	ja	_fusuaddrfault
	movl	_curpcb,%ecx
	movl	$_fusufault,PCB_ONFAULT(%ecx)
	movl	(%edx),%eax
	movl	$0,PCB_ONFAULT(%ecx)
	ret
	
/*
 * fusword(caddr_t uaddr);
 * Fetch a short from the user's address space.
 */
ENTRY(fusword)
	movl	4(%esp),%edx
	cmpl	$VM_MAXUSER_ADDRESS-2,%edx
	ja	_fusuaddrfault
	movl	_curpcb,%ecx
	movl	$_fusufault,PCB_ONFAULT(%ecx)
	movzwl	(%edx),%eax
	movl	$0,PCB_ONFAULT(%ecx)
	ret
	
/*
 * fuswintr(caddr_t uaddr);
 * Fetch a short from the user's address space.  Can be called during an
 * interrupt.
 */
ENTRY(fuswintr)
	movl	4(%esp),%edx
	cmpl	$VM_MAXUSER_ADDRESS-2,%edx
	ja	_fusuaddrfault
	movl	_curpcb,%ecx
	movl	$_fusubail,PCB_ONFAULT(%ecx)
	movzwl	(%edx),%eax
	movl	$0,PCB_ONFAULT(%ecx)
	ret
	
/*
 * fubyte(caddr_t uaddr);
 * Fetch a byte from the user's address space.
 */
ENTRY(fubyte)
	movl	4(%esp),%edx
	cmpl	$VM_MAXUSER_ADDRESS-1,%edx
	ja	_fusuaddrfault
	movl	_curpcb,%ecx
	movl	$_fusufault,PCB_ONFAULT(%ecx)
	movzbl	(%edx),%eax
	movl	$0,PCB_ONFAULT(%ecx)
	ret

/*
 * Handle faults from [fs]u*().  Clean up and return -1.
 */
ENTRY(fusufault)
	movl	$0,PCB_ONFAULT(%ecx)
	movl	$-1,%eax
	ret

/*
 * Handle faults from [fs]u*().  Clean up and return -1.  This differs from
 * fusufault() in that trap() will recognize it and return immediately rather
 * than trying to page fault.
 */
ENTRY(fusubail)
	movl	$0,PCB_ONFAULT(%ecx)
	movl	$-1,%eax
	ret

/*
 * Handle earlier faults from [fs]u*(), due to our of range addresses.
 */
ENTRY(fusuaddrfault)
	movl	$-1,%eax
	ret

/*
 * suword(caddr_t uaddr, int x);
 * Store an int in the user's address space.
 */
ENTRY(suword)
	movl	4(%esp),%edx
	cmpl	$VM_MAXUSER_ADDRESS-4,%edx
	ja	_fusuaddrfault
	movl	_curpcb,%ecx
	movl	$_fusufault,PCB_ONFAULT(%ecx)

#if defined(I386_CPU)
#if defined(I486_CPU) || defined(I586_CPU) || defined(I686_CPU)
	cmpl	$CPUCLASS_386,_cpu_class
	jne	2f
#endif /* I486_CPU || I586_CPU || I686_CPU */

	movl	%edx,%eax
	shrl	$PGSHIFT,%eax		# calculate pte address
	testb	$PG_RW,_PTmap(,%eax,4)
	jnz	1f

	/* Simulate a trap. */
	pushl	%edx
	pushl	%edx
	call	_trapwrite		# trapwrite(addr)
	addl	$4,%esp			# clear parameter from the stack
	popl	%edx
	movl	_curpcb,%ecx
	testl	%eax,%eax
	jnz	_fusufault

1:	/* XXX also need to check the following 3 bytes for validity! */
#endif

2:	movl	8(%esp),%eax
	movl	%eax,(%edx)
	xorl	%eax,%eax
	movl	%eax,PCB_ONFAULT(%ecx)
	ret
	
/*
 * susword(caddr_t uaddr, short x);
 * Store a short in the user's address space.
 */
ENTRY(susword)
	movl	4(%esp),%edx
	cmpl	$VM_MAXUSER_ADDRESS-2,%edx
	ja	_fusuaddrfault
	movl	_curpcb,%ecx
	movl	$_fusufault,PCB_ONFAULT(%ecx)

#if defined(I386_CPU)
#if defined(I486_CPU) || defined(I586_CPU) || defined(I686_CPU)
	cmpl	$CPUCLASS_386,_cpu_class
	jne	2f
#endif /* I486_CPU || I586_CPU || I686_CPU */

	movl	%edx,%eax
	shrl	$PGSHIFT,%eax		# calculate pte address
	testb	$PG_RW,_PTmap(,%eax,4)
	jnz	1f

	/* Simulate a trap. */
	pushl	%edx
	pushl	%edx
	call	_trapwrite		# trapwrite(addr)
	addl	$4,%esp			# clear parameter from the stack
	popl	%edx
	movl	_curpcb,%ecx
	testl	%eax,%eax
	jnz	_fusufault

1:	/* XXX also need to check the following byte for validity! */
#endif

2:	movl	8(%esp),%eax
	movw	%ax,(%edx)
	xorl	%eax,%eax
	movl	%eax,PCB_ONFAULT(%ecx)
	ret

/*
 * suswintr(caddr_t uaddr, short x);
 * Store a short in the user's address space.  Can be called during an
 * interrupt.
 */
ENTRY(suswintr)
	movl	4(%esp),%edx
	cmpl	$VM_MAXUSER_ADDRESS-2,%edx
	ja	_fusuaddrfault
	movl	_curpcb,%ecx
	movl	$_fusubail,PCB_ONFAULT(%ecx)

#if defined(I386_CPU)
#if defined(I486_CPU) || defined(I586_CPU) || defined(I686_CPU)
	cmpl	$CPUCLASS_386,_cpu_class
	jne	2f
#endif /* I486_CPU || I586_CPU || I686_CPU */

	movl	%edx,%eax
	shrl	$PGSHIFT,%eax		# calculate pte address
	testb	$PG_RW,_PTmap(,%eax,4)
	jnz	1f

	/* Simulate a trap. */
	jmp	_fusubail

1:	/* XXX also need to check the following byte for validity! */
#endif

2:	movl	8(%esp),%eax
	movw	%ax,(%edx)
	xorl	%eax,%eax
	movl	%eax,PCB_ONFAULT(%ecx)
	ret

/*
 * subyte(caddr_t uaddr, char x);
 * Store a byte in the user's address space.
 */
ENTRY(subyte)
	movl	4(%esp),%edx
	cmpl	$VM_MAXUSER_ADDRESS-1,%edx
	ja	_fusuaddrfault
	movl	_curpcb,%ecx
	movl	$_fusufault,PCB_ONFAULT(%ecx)

#if defined(I386_CPU)
#if defined(I486_CPU) || defined(I586_CPU) || defined(I686_CPU)
	cmpl	$CPUCLASS_386,_cpu_class
	jne	2f
#endif /* I486_CPU || I586_CPU || I686_CPU */

	movl	%edx,%eax
	shrl	$PGSHIFT,%eax		# calculate pte address
	testb	$PG_RW,_PTmap(,%eax,4)
	jnz	1f

	/* Simulate a trap. */
	pushl	%edx
	pushl	%edx
	call	_trapwrite		# trapwrite(addr)
	addl	$4,%esp			# clear parameter from the stack
	popl	%edx
	movl	_curpcb,%ecx
	testl	%eax,%eax
	jnz	_fusufault

1:
#endif

2:	movb	8(%esp),%al
	movb	%al,(%edx)
	xorl	%eax,%eax
	movl	%eax,PCB_ONFAULT(%ecx)
	ret

/*****************************************************************************/

/*
 * The following is i386-specific nonsense.
 */

/*
 * void lgdt(struct region_descriptor *rdp);
 * Change the global descriptor table.
 */
NENTRY(lgdt)
	/* Reload the descriptor table. */
	movl	4(%esp),%eax
	lgdt	(%eax)
	/* Flush the prefetch q. */
	jmp	1f
	nop
1:	/* Reload "stale" selectors. */
	movl	$GSEL(GDATA_SEL, SEL_KPL),%eax
	movl	%ax,%ds
	movl	%ax,%es
	movl	%ax,%ss
	/* Reload code selector by doing intersegment return. */
	popl	%eax
	pushl	$GSEL(GCODE_SEL, SEL_KPL)
	pushl	%eax
	lret

ENTRY(setjmp)
	movl	4(%esp),%eax
	movl	%ebx,(%eax)		# save ebx
	movl	%esp,4(%eax)		# save esp
	movl	%ebp,8(%eax)		# save ebp
	movl	%esi,12(%eax)		# save esi
	movl	%edi,16(%eax)		# save edi
	movl	(%esp),%edx		# get rta
	movl	%edx,20(%eax)		# save eip
	xorl	%eax,%eax		# return (0);
	ret

ENTRY(longjmp)
	movl	4(%esp),%eax
	movl	(%eax),%ebx		# restore ebx
	movl	4(%eax),%esp		# restore esp
	movl	8(%eax),%ebp		# restore ebp
	movl	12(%eax),%esi		# restore esi
	movl	16(%eax),%edi		# restore edi
	movl	20(%eax),%edx		# get rta
	movl	%edx,(%esp)		# put in return frame
	xorl	%eax,%eax		# return (1);
	incl	%eax
	ret

/*****************************************************************************/

/*
 * The following primitives manipulate the run queues.
 * _whichqs tells which of the 32 queues _qs
 * have processes in them.  Setrq puts processes into queues, Remrq
 * removes them from queues.  The running process is on no queue,
 * other processes are on a queue related to p->p_pri, divided by 4
 * actually to shrink the 0-127 range of priorities into the 32 available
 * queues.
 */
#ifdef UVM
	.globl	_C_LABEL(whichqs),_C_LABEL(qs),_C_LABEL(uvmexp),_C_LABEL(panic)
#else
	.globl	_whichqs,_qs,_cnt,_panic
#endif
	
/*
 * setrunqueue(struct proc *p);
 * Insert a process on the appropriate queue.  Should be called at splclock().
 */
NENTRY(setrunqueue)
	movl	4(%esp),%eax
#ifdef DIAGNOSTIC
	cmpl	$0,P_BACK(%eax)	# should not be on q already
	jne	1f
	cmpl	$0,P_WCHAN(%eax)
	jne	1f
	cmpb	$SRUN,P_STAT(%eax)
	jne	1f
#endif /* DIAGNOSTIC */
	movzbl	P_PRIORITY(%eax),%edx
	shrl	$2,%edx
	btsl	%edx,_whichqs		# set q full bit
	leal	_qs(,%edx,8),%edx	# locate q hdr
	movl	P_BACK(%edx),%ecx
	movl	%edx,P_FORW(%eax)	# link process on tail of q
	movl	%eax,P_BACK(%edx)
	movl	%eax,P_FORW(%ecx)
	movl	%ecx,P_BACK(%eax)
	ret
#ifdef DIAGNOSTIC
1:	pushl	$2f
	call	_panic
	/* NOTREACHED */
2:	.asciz	"setrunqueue"
#endif /* DIAGNOSTIC */

/*
 * remrunqueue(struct proc *p);
 * Remove a process from its queue.  Should be called at splclock().
 */
NENTRY(remrunqueue)
	movl	4(%esp),%ecx
	movzbl	P_PRIORITY(%ecx),%eax
#ifdef DIAGNOSTIC
	shrl	$2,%eax
	btl	%eax,_whichqs
	jnc	1f
#endif /* DIAGNOSTIC */
	movl	P_BACK(%ecx),%edx	# unlink process
	movl	$0,P_BACK(%ecx)		# zap reverse link to indicate off list
	movl	P_FORW(%ecx),%ecx
	movl	%ecx,P_FORW(%edx)
	movl	%edx,P_BACK(%ecx)
	cmpl	%ecx,%edx		# q still has something?
	jne	2f
#ifndef DIAGNOSTIC
	shrl	$2,%eax
#endif
	btrl	%eax,_whichqs		# no; clear bit
2:	ret
#ifdef DIAGNOSTIC
1:	pushl	$3f
	call	_panic
	/* NOTREACHED */
3:	.asciz	"remrunqueue"
#endif /* DIAGNOSTIC */

#if NAPM > 0
	.globl _apm_cpu_idle,_apm_cpu_busy,_apm_dobusy
#endif
/*
 * When no processes are on the runq, cpu_switch() branches to here to wait for
 * something to come ready.
 */
ENTRY(idle)
	cli
	movl	_whichqs,%ecx
	testl	%ecx,%ecx
	jnz	sw1
	sti
#if NAPM > 0
	call	_apm_cpu_idle
	cmpl	$0,_apm_dobusy
	je	1f
	call	_apm_cpu_busy
1:
#endif
#if NPCTR > 0 && NAPM == 0
	addl	$1,_pctr_idlcnt
	adcl	$0,_pctr_idlcnt+4
#else
	hlt
#endif
	jmp	_idle

#ifdef DIAGNOSTIC
NENTRY(switch_error)
	pushl	$1f
	call	_panic
	/* NOTREACHED */
1:	.asciz	"cpu_switch"
#endif /* DIAGNOSTIC */

/*
 * cpu_switch(void);
 * Find a runnable process and switch to it.  Wait if necessary.  If the new
 * process is the same as the old one, we short-circuit the context save and
 * restore.
 */
ENTRY(cpu_switch)
	pushl	%ebx
	pushl	%esi
	pushl	%edi
	pushl	_cpl

	movl	_curproc,%esi

	/*
	 * Clear curproc so that we don't accumulate system time while idle.
	 * This also insures that schedcpu() will move the old process to
	 * the correct queue if it happens to get called from the spllower()
	 * below and changes the priority.  (See corresponding comment in
	 * userret()).
	 */
	movl	$0,_curproc

	movl	$0,_cpl			# spl0()
	call	_Xspllower		# process pending interrupts

switch_search:
	/*
	 * First phase: find new process.
	 *
	 * Registers:
	 *   %eax - queue head, scratch, then zero
	 *   %ebx - queue number
	 *   %ecx - cached value of whichqs
	 *   %edx - next process in queue
	 *   %esi - old process
	 *   %edi - new process
	 */

	/* Wait for new process. */
	cli				# splhigh doesn't do a cli
	movl	_whichqs,%ecx

sw1:	bsfl	%ecx,%ebx		# find a full q
	jz	_idle			# if none, idle

	leal	_qs(,%ebx,8),%eax	# select q

	movl	P_FORW(%eax),%edi	# unlink from front of process q
#ifdef	DIAGNOSTIC
	cmpl	%edi,%eax		# linked to self (i.e. nothing queued)?
	je	_switch_error		# not possible
#endif /* DIAGNOSTIC */
	movl	P_FORW(%edi),%edx
	movl	%edx,P_FORW(%eax)
	movl	%eax,P_BACK(%edx)

	cmpl	%edx,%eax		# q empty?
	jne	3f

	btrl	%ebx,%ecx		# yes, clear to indicate empty
	movl	%ecx,_whichqs		# update q status

3:	/* We just did it. */
	xorl	%eax,%eax
	movl	%eax,_want_resched

#ifdef	DIAGNOSTIC
	cmpl	%eax,P_WCHAN(%edi)	# Waiting for something?
	jne	_switch_error		# Yes; shouldn't be queued.
	cmpb	$SRUN,P_STAT(%edi)	# In run state?
	jne	_switch_error		# No; shouldn't be queued.
#endif /* DIAGNOSTIC */

	/* Isolate process.  XXX Is this necessary? */
	movl	%eax,P_BACK(%edi)

	/* Record new process. */
	movl	%edi,_curproc

	/* It's okay to take interrupts here. */
	sti

	/* Skip context switch if same process. */
	cmpl	%edi,%esi
	je	switch_return

	/* If old process exited, don't bother. */
	testl	%esi,%esi
	jz	switch_exited

	/*
	 * Second phase: save old context.
	 *
	 * Registers:
	 *   %eax, %ecx - scratch
	 *   %esi - old process, then old pcb
	 *   %edi - new process
	 */

	movl	P_ADDR(%esi),%esi

	/* Save segment registers. */
	movl	%fs,%ax
	movl	%gs,%cx
	movl	%eax,PCB_FS(%esi)
	movl	%ecx,PCB_GS(%esi)

	/* Save stack pointers. */
	movl	%esp,PCB_ESP(%esi)
	movl	%ebp,PCB_EBP(%esi)

switch_exited:
	/*
	 * Third phase: restore saved context.
	 *
	 * Registers:
	 *   %eax, %ecx, %edx - scratch
	 *   %esi - new pcb
	 *   %edi - new process
	 */

	/* No interrupts while loading new state. */
	cli
	movl	P_ADDR(%edi),%esi

	/* Restore stack pointers. */
	movl	PCB_ESP(%esi),%esp
	movl	PCB_EBP(%esi),%ebp

#if 0
	/* Don't bother with the rest if switching to a system process. */
	testl	$P_SYSTEM,P_FLAG(%edi)
	jnz	switch_restored
#endif

	/* Load TSS info. */
	movl	_dynamic_gdt,%eax
	movl	PCB_TSS_SEL(%esi),%edx

	/* Switch address space. */
	movl	PCB_CR3(%esi),%ecx
	movl	%ecx,%cr3

	/* Switch TSS. */
	andl	$~0x0200,4-SEL_KPL(%eax,%edx,1)
	ltr	%dx

#ifdef USER_LDT
	/*
	 * Switch LDT.
	 *
	 * XXX
	 * Always do this, because the LDT could have been swapped into a
	 * different selector after a process exited.  (See gdt_compact().)
	 */
	movl	PCB_LDT_SEL(%esi),%edx
	lldt	%dx
#endif /* USER_LDT */

	/* Restore segment registers. */
	movl	PCB_FS(%esi),%eax
	movl	PCB_GS(%esi),%ecx
	movl	%ax,%fs
	movl	%cx,%gs

switch_restored:
	/* Restore cr0 (including FPU state). */
	movl	PCB_CR0(%esi),%ecx
	movl	%ecx,%cr0

	/* Record new pcb. */
	movl	%esi,_curpcb

	/* Interrupts are okay again. */
	sti

switch_return:
	/*
	 * Restore old cpl from stack.  Note that this is always an increase,
	 * due to the spl0() on entry.
	 */
	popl	_cpl

	movl	%edi,%eax		# return (p);
	popl	%edi
	popl	%esi
	popl	%ebx
	ret

/*
 * switch_exit(struct proc *p);
 * Switch to proc0's saved context and deallocate the address space and kernel
 * stack for p.  Then jump into cpu_switch(), as if we were in proc0 all along.
 */
#if defined(UVM)
	.globl	_C_LABEL(proc0),_C_LABEL(uvmspace_free),_C_LABEL(kernel_map)
	.globl	_C_LABEL(uvm_km_free),_C_LABEL(tss_free)
#else
	.globl	_proc0,_vmspace_free,_kernel_map,_kmem_free,_tss_free
#endif
ENTRY(switch_exit)
	movl	4(%esp),%edi		# old process
	movl	$_proc0,%ebx

	/* In case we fault... */
	movl	$0,_curproc

	/* Restore proc0's context. */
	cli
	movl	P_ADDR(%ebx),%esi

	/* Restore stack pointers. */
	movl	PCB_ESP(%esi),%esp
	movl	PCB_EBP(%esi),%ebp

	/* Load TSS info. */
	movl	_dynamic_gdt,%eax
	movl	PCB_TSS_SEL(%esi),%edx

	/* Switch address space. */
	movl	PCB_CR3(%esi),%ecx
	movl	%ecx,%cr3

	/* Switch TSS. */
	andl	$~0x0200,4-SEL_KPL(%eax,%edx,1)
	ltr	%dx

	/* We're always in the kernel, so we don't need the LDT. */

	/* Clear segment registers; always null in proc0. */
	xorl	%ecx,%ecx
	movl	%cx,%fs
	movl	%cx,%gs

	/* Restore cr0 (including FPU state). */
	movl	PCB_CR0(%esi),%ecx
	movl	%ecx,%cr0

	/* Record new pcb. */
	movl	%esi,_curpcb

	/* Interrupts are okay again. */
	sti

	/* Thoroughly nuke the old process's resources. */
	pushl	P_ADDR(%edi)
	call	_tss_free
	pushl	P_VMSPACE(%edi)
#if defined(UVM)
	call	_C_LABEL(uvmspace_free)
#else
	call	_vmspace_free
#endif
	pushl	$USPACE
	pushl	P_ADDR(%edi)
	pushl	_kernel_map
#if defined(UVM)
	call	_C_LABEL(uvm_km_free)
#else
	call	_kmem_free
#endif
	addl	$20,%esp

	/* Jump into cpu_switch() with the right state. */
	movl	%ebx,%esi
	movl	$0,_curproc
	jmp	switch_search

/*
 * savectx(struct pcb *pcb);
 * Update pcb, saving current processor state.
 */
ENTRY(savectx)
	movl	4(%esp),%edx		# edx = p->p_addr
  
	/* Save segment registers. */
	movl	%fs,%ax
	movl	%gs,%cx
	movl	%eax,PCB_FS(%edx)
	movl	%ecx,PCB_GS(%edx)

	/* Save stack pointers. */
	movl	%esp,PCB_ESP(%edx)
	movl	%ebp,PCB_EBP(%edx)

	ret

/*****************************************************************************/

/*
 * Trap and fault vector routines
 *
 * On exit from the kernel to user mode, we always need to check for ASTs.  In
 * addition, we need to do this atomically; otherwise an interrupt may occur
 * which causes an AST, but it won't get processed until the next kernel entry
 * (possibly the next clock tick).  Thus, we disable interrupt before checking,
 * and only enable them again on the final `iret' or before calling the AST
 * handler.
 *
 * XXX - debugger traps are now interrupt gates so at least bdb doesn't lose
 * control.  The sti's give the standard losing behaviour for ddb and kgdb.
 */ 
#define	IDTVEC(name)	ALIGN_TEXT; .globl _X/**/name; _X/**/name:

#define	TRAP(a)		pushl $(a) ; jmp _alltraps
#define	ZTRAP(a)	pushl $0 ; TRAP(a)
#ifdef KGDB
#define	BPTTRAP(a)	testb $(PSL_I>>8),13(%esp) ; jz 1f ; sti ; 1: ; \
			pushl $(a) ; jmp _bpttraps
#else
#define	BPTTRAP(a)	testb $(PSL_I>>8),13(%esp) ; jz 1f ; sti ; 1: ; \
			TRAP(a)
#endif

	.text
IDTVEC(div)
	ZTRAP(T_DIVIDE)
IDTVEC(dbg)
	subl	$4,%esp
	pushl	%eax
	movl	%dr6,%eax
	movl	%eax,4(%esp)
	andb	$~0xf,%al
	movl	%eax,%dr6
	popl	%eax
	BPTTRAP(T_TRCTRAP)
IDTVEC(nmi)
	ZTRAP(T_NMI)
IDTVEC(bpt)
	pushl	$0
	BPTTRAP(T_BPTFLT)
IDTVEC(ofl)
	ZTRAP(T_OFLOW)
IDTVEC(bnd)
	ZTRAP(T_BOUND)
IDTVEC(ill)
	ZTRAP(T_PRIVINFLT)
IDTVEC(dna)
#if NNPX > 0
	pushl	$0			# dummy error code
	pushl	$T_DNA
	INTRENTRY
	pushl	_curproc
	call	_npxdna
	addl	$4,%esp
	testl	%eax,%eax
	jz	calltrap
	INTRFASTEXIT
#else
	ZTRAP(T_DNA)
#endif
IDTVEC(dble)
	TRAP(T_DOUBLEFLT)
IDTVEC(fpusegm)
	ZTRAP(T_FPOPFLT)
IDTVEC(tss)
	TRAP(T_TSSFLT)
IDTVEC(missing)
	TRAP(T_SEGNPFLT)
IDTVEC(stk)
	TRAP(T_STKFLT)
IDTVEC(prot)
	TRAP(T_PROTFLT)
#ifdef I586_CPU
IDTVEC(f00f_redirect)
	pushl	$T_PAGEFLT
	INTRENTRY
	testb	$PGEX_U,TF_ERR(%esp)
	jnz	calltrap
	movl	%cr2,%eax
	subl	_idt,%eax
	cmpl	$(6*8),%eax
	jne	calltrap
	movb	$T_PRIVINFLT,TF_TRAPNO(%esp)
	jmp	calltrap
#endif
IDTVEC(page)
	TRAP(T_PAGEFLT)
IDTVEC(rsvd)
	ZTRAP(T_RESERVED)
IDTVEC(fpu)
#if NNPX > 0
	/*
	 * Handle like an interrupt so that we can call npxintr to clear the
	 * error.  It would be better to handle npx interrupts as traps but
	 * this is difficult for nested interrupts.
	 */
	pushl	$0			# dummy error code
	pushl	$T_ASTFLT
	INTRENTRY
	pushl	_cpl			# if_ppl in intrframe
	pushl	%esp			# push address of intrframe
#if defined(UVM)
	incl	_C_LABEL(uvmexp)+V_TRAP
#else
	incl	_cnt+V_TRAP
#endif
	call	_npxintr
	addl	$8,%esp			# pop address and if_ppl
	INTRFASTEXIT
#else
	ZTRAP(T_ARITHTRAP)
#endif
IDTVEC(align)
	ZTRAP(T_ALIGNFLT)
	/* 18 - 31 reserved for future exp */

/*
 * If an error is detected during trap, syscall, or interrupt exit, trap() will
 * change %eip to point to one of these labels.  We clean up the stack, if
 * necessary, and resume as if we were handling a general protection fault.
 * This will cause the process to get a SIGBUS.
 */
NENTRY(resume_iret)
	ZTRAP(T_PROTFLT)
NENTRY(resume_pop_ds)
	movl	$GSEL(GDATA_SEL, SEL_KPL),%eax
	movl	%ax,%es
NENTRY(resume_pop_es)
	movl	$T_PROTFLT,TF_TRAPNO(%esp)
	jmp	calltrap

NENTRY(alltraps)
	INTRENTRY
calltrap:
#ifdef DIAGNOSTIC
	movl	_cpl,%ebx
#endif /* DIAGNOSTIC */
	call	_trap
2:	/* Check for ASTs on exit to user mode. */
	cli
	cmpb	$0,_astpending
	je	1f
	testb	$SEL_RPL,TF_CS(%esp)
#ifdef VM86
	jnz	5f
	testl	$PSL_VM,TF_EFLAGS(%esp)
#endif
	jz	1f
5:	movb	$0,_astpending
	sti
	movl	$T_ASTFLT,TF_TRAPNO(%esp)
	call	_trap
	jmp	2b
#ifndef DIAGNOSTIC
1:	INTRFASTEXIT
#else /* DIAGNOSTIC */
1:	cmpl	_cpl,%ebx
	jne	3f
	INTRFASTEXIT
3:	sti
	pushl	$4f
	call	_printf
	addl	$4,%esp
#ifdef DDB
	int	$3
#endif /* DDB */
	movl	%ebx,_cpl
	jmp	2b
4:	.asciz	"WARNING: SPL NOT LOWERED ON TRAP EXIT\n"
#endif /* DIAGNOSTIC */

#ifdef KGDB
/*
 * This code checks for a kgdb trap, then falls through
 * to the regular trap code.
 */
NENTRY(bpttraps)
	INTRENTRY
	testb	$SEL_RPL,TF_CS(%esp)
	jne	calltrap
	call	_kgdb_trap_glue		
	jmp	calltrap
#endif /* KGDB */

/*
 * Old call gate entry for syscall
 */
IDTVEC(osyscall)
	/* Set eflags in trap frame. */
	pushfl
	popl	8(%esp)
	/* Turn off trace flag and nested task. */
	pushfl
	andb	$~((PSL_T|PSL_NT)>>8),1(%esp)
	popfl
	pushl	$7		# size of instruction for restart
	jmp	syscall1
IDTVEC(osyscall_end)

/*
 * Trap gate entry for syscall
 */
IDTVEC(syscall)
	pushl	$2		# size of instruction for restart
syscall1:
	pushl	$T_ASTFLT	# trap # for doing ASTs
	INTRENTRY
#ifdef DIAGNOSTIC
	movl	_cpl,%ebx
#endif /* DIAGNOSTIC */
	call	_syscall
2:	/* Check for ASTs on exit to user mode. */
	cli
	cmpb	$0,_astpending
	je	1f
	/* Always returning to user mode here. */
	movb	$0,_astpending
	sti
	/* Pushed T_ASTFLT into tf_trapno on entry. */
	call	_trap
	jmp	2b
#ifndef DIAGNOSTIC
1:	INTRFASTEXIT
#else /* DIAGNOSTIC */
1:	cmpl	_cpl,%ebx
	jne	3f
	INTRFASTEXIT
3:	sti
	pushl	$4f
	call	_printf
	addl	$4,%esp
#ifdef DDB
	int	$3
#endif /* DDB */
	movl	%ebx,_cpl
	jmp	2b
4:	.asciz	"WARNING: SPL NOT LOWERED ON SYSCALL EXIT\n"
#endif /* DIAGNOSTIC */

#include <i386/isa/vector.s>
#include <i386/isa/icu.s>

/*
 * bzero (void *b, size_t len)
 *	write len zero bytes to the string b.
 */

ENTRY(bzero)
	pushl	%edi
	movl	8(%esp),%edi
	movl	12(%esp),%edx

	cld				/* set fill direction forward */
	xorl	%eax,%eax		/* set fill data to 0 */

	/*
	 * if the string is too short, it's really not worth the overhead
	 * of aligning to word boundries, etc.  So we jump to a plain
	 * unaligned set.
	 */
	cmpl	$16,%edx
	jb	7f

	movl	%edi,%ecx		/* compute misalignment */
	negl	%ecx
	andl	$3,%ecx
	subl	%ecx,%edx
	rep				/* zero until word aligned */
	stosb

#if defined(I486_CPU)
#if defined(I386_CPU) || defined(I586_CPU) || defined(I686_CPU)
	cmpl	$CPUCLASS_486,_cpu_class
	jne	8f
#endif

	movl	%edx,%ecx
	shrl	$6,%ecx
	jz	8f
	andl	$63,%edx
1:	movl	%eax,(%edi)
	movl	%eax,4(%edi)
	movl	%eax,8(%edi)
	movl	%eax,12(%edi)
	movl	%eax,16(%edi)
	movl	%eax,20(%edi)
	movl	%eax,24(%edi)
	movl	%eax,28(%edi)
	movl	%eax,32(%edi)
	movl	%eax,36(%edi)
	movl	%eax,40(%edi)
	movl	%eax,44(%edi)
	movl	%eax,48(%edi)
	movl	%eax,52(%edi)
	movl	%eax,56(%edi)
	movl	%eax,60(%edi)
	addl	$64,%edi
	decl	%ecx
	jnz	1b
#endif

8:	movl	%edx,%ecx		/* zero by words */
	shrl	$2,%ecx
	andl	$3,%edx
	rep
	stosl

7:	movl	%edx,%ecx		/* zero remainder bytes */
	rep
	stosb

	popl	%edi
	ret
