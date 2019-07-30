/*	$OpenBSD: locore.s,v 1.183 2018/03/22 19:30:18 bluhm Exp $	*/
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

#include "npx.h"
#include "assym.h"
#include "apm.h"
#include "lapic.h"
#include "ksyms.h"

#include <sys/errno.h>
#include <sys/syscall.h>

#include <machine/codepatch.h>
#include <machine/cputypes.h>
#include <machine/param.h>
#include <machine/pte.h>
#include <machine/segments.h>
#include <machine/specialreg.h>
#include <machine/trap.h>

#include <dev/isa/isareg.h>

#if NLAPIC > 0
#include <machine/i82489reg.h>
#endif

/*
 * As stac/clac SMAP instructions are 3 bytes, we want the fastest
 * 3 byte nop sequence possible here.  This will be replaced by
 * stac/clac instructions if SMAP is detected after booting.
 *
 * Intel documents multi-byte NOP sequences as being available
 * on all family 0x6 and 0xf processors (ie 686+)
 * So use 3 of the single byte nops for compatibility
 */
#define SMAP_NOP	.byte 0x90, 0x90, 0x90
#define SMAP_STAC	CODEPATCH_START			;\
			SMAP_NOP			;\
			CODEPATCH_END(CPTAG_STAC)
#define SMAP_CLAC	CODEPATCH_START			;\
			SMAP_NOP			;\
			CODEPATCH_END(CPTAG_CLAC)

/*
 * override user-land alignment before including asm.h
 */

#define	ALIGN_DATA	.align  4,0xcc
#define	ALIGN_TEXT	.align  4,0x90	/* 4-byte boundaries, NOP-filled */
#define _ALIGN_TEXT	ALIGN_TEXT
#include <machine/asm.h>

#define CPL _C_LABEL(lapic_tpr)

#define	GET_CURPCB(reg)					\
	movl	CPUVAR(CURPCB), reg

#define	CHECK_ASTPENDING(treg)				\
	movl 	CPUVAR(CURPROC),treg		;	\
	cmpl	$0, treg			;	\
	je	1f				;	\
	cmpl	$0,P_MD_ASTPENDING(treg)	;	\
	1:

#define	CLEAR_ASTPENDING(cpreg)				\
	movl	$0,P_MD_ASTPENDING(cpreg)

/*
 * These are used on interrupt or trap entry or exit.
 */
#define	INTRENTRY \
	cld			; \
	SMAP_CLAC		; \
	pushl	%eax		; \
	pushl	%ecx		; \
	pushl	%edx		; \
	pushl	%ebx		; \
	pushl	%ebp		; \
	pushl	%esi		; \
	pushl	%edi		; \
	pushl	%ds		; \
	pushl	%es		; \
	pushl	%gs		; \
	movl	$GSEL(GDATA_SEL, SEL_KPL),%eax	; \
	movw	%ax,%ds		; \
	movw	%ax,%es		; \
	xorl	%eax,%eax	; /* $GSEL(GNULL_SEL, SEL_KPL) == 0 */ \
	movw	%ax,%gs		; \
	pushl	%fs		; \
	movl	$GSEL(GCPU_SEL, SEL_KPL),%eax	; \
	movw	%ax,%fs

#define	INTR_RESTORE_ALL \
	popl	%fs		; \
	popl	%gs		; \
	popl	%es		; \
	popl	%ds		; \
	popl	%edi		; \
	popl	%esi		; \
	popl	%ebp		; \
	popl	%ebx		; \
	popl	%edx		; \
	popl	%ecx		; \
	popl	%eax

#define	INTRFASTEXIT \
	INTR_RESTORE_ALL	;\
	addl	$8,%esp		; \
	iret

#define	INTR_FAKE_TRAP	0xbadabada

/*
 * PTmap is recursive pagemap at top of virtual address space.
 * Within PTmap, the page directory can be found (third indirection).
 */
	.globl	_C_LABEL(PTmap), _C_LABEL(PTD)
	.set	_C_LABEL(PTmap), (PDSLOT_PTE << PDSHIFT)
	.set	_C_LABEL(PTD), (_C_LABEL(PTmap) + PDSLOT_PTE * NBPG)

/*
 * Initialization
 */
	.data

	.globl	_C_LABEL(cpu), _C_LABEL(cpu_id), _C_LABEL(cpu_vendor)
	.globl	_C_LABEL(cpu_brandstr)
	.globl	_C_LABEL(cpuid_level)
	.globl	_C_LABEL(cpu_miscinfo)
	.globl	_C_LABEL(cpu_feature), _C_LABEL(cpu_ecxfeature)
	.globl	_C_LABEL(ecpu_feature), _C_LABEL(ecpu_eaxfeature)
	.globl	_C_LABEL(ecpu_ecxfeature)
	.globl	_C_LABEL(cpu_cache_eax), _C_LABEL(cpu_cache_ebx)
	.globl	_C_LABEL(cpu_cache_ecx), _C_LABEL(cpu_cache_edx)
	.globl	_C_LABEL(cpu_perf_eax)
	.globl	_C_LABEL(cpu_perf_ebx)
	.globl	_C_LABEL(cpu_perf_edx)
	.globl	_C_LABEL(cpu_apmi_edx)
	.globl	_C_LABEL(cold), _C_LABEL(cnvmem), _C_LABEL(extmem)
	.globl	_C_LABEL(cpu_pae)
	.globl	_C_LABEL(esym)
	.globl	_C_LABEL(ssym)
	.globl	_C_LABEL(nkptp_max)
	.globl	_C_LABEL(boothowto), _C_LABEL(bootdev), _C_LABEL(atdevbase)
	.globl	_C_LABEL(proc0paddr), _C_LABEL(PTDpaddr), _C_LABEL(PTDsize)
	.globl	_C_LABEL(gdt)
	.globl	_C_LABEL(bootapiver), _C_LABEL(bootargc), _C_LABEL(bootargv)
	.globl	_C_LABEL(lapic_tpr)

#if NLAPIC > 0
	.align NBPG
	.globl _C_LABEL(local_apic), _C_LABEL(lapic_id)
_C_LABEL(local_apic):
	.space	LAPIC_ID
_C_LABEL(lapic_id):
	.long	0x00000000
	.space	LAPIC_TPRI-(LAPIC_ID+4)
_C_LABEL(lapic_tpr):
	.space	LAPIC_PPRI-LAPIC_TPRI
_C_LABEL(lapic_ppr):
	.space	LAPIC_ISR-LAPIC_PPRI
_C_LABEL(lapic_isr):
	.space	NBPG-LAPIC_ISR
#else
_C_LABEL(lapic_tpr):
	.long	0
#endif

_C_LABEL(cpu):		.long	0	# are we 386, 386sx, 486, 586 or 686
_C_LABEL(cpu_id):	.long	0	# saved from 'cpuid' instruction
_C_LABEL(cpu_pae):	.long	0	# are we using PAE paging mode?
_C_LABEL(cpu_miscinfo):	.long	0	# misc info (apic/brand id) from 'cpuid'
_C_LABEL(cpu_feature):	.long	0	# feature flags from 'cpuid' instruction
_C_LABEL(ecpu_feature): .long	0	# extended feature flags from 'cpuid'
_C_LABEL(cpu_ecxfeature):.long	0	# ecx feature flags from 'cpuid'
_C_LABEL(ecpu_eaxfeature): .long 0	# extended eax feature flags
_C_LABEL(ecpu_ecxfeature): .long 0	# extended ecx feature flags
_C_LABEL(cpuid_level):	.long	-1	# max. lvl accepted by 'cpuid' insn
_C_LABEL(cpu_cache_eax):.long	0
_C_LABEL(cpu_cache_ebx):.long	0
_C_LABEL(cpu_cache_ecx):.long	0
_C_LABEL(cpu_cache_edx):.long	0
_C_LABEL(cpu_perf_eax):	.long	0	# arch. perf. mon. flags from 'cpuid'
_C_LABEL(cpu_perf_ebx):	.long	0	# arch. perf. mon. flags from 'cpuid'
_C_LABEL(cpu_perf_edx):	.long	0	# arch. perf. mon. flags from 'cpuid'
_C_LABEL(cpu_apmi_edx):	.long	0	# adv. power management info. 'cpuid'
_C_LABEL(cpu_vendor): .space 16	# vendor string returned by 'cpuid' instruction
_C_LABEL(cpu_brandstr):	.space 48 # brand string returned by 'cpuid'
_C_LABEL(cold):		.long	1	# cold till we are not
_C_LABEL(ssym):		.long	0	# ptr to start of syms
_C_LABEL(esym):		.long	0	# ptr to end of syms
_C_LABEL(cnvmem):	.long	0	# conventional memory size
_C_LABEL(extmem):	.long	0	# extended memory size
_C_LABEL(atdevbase):	.long	0	# location of start of iomem in virtual
_C_LABEL(bootapiver):	.long	0	# /boot API version
_C_LABEL(bootargc):	.long	0	# /boot argc
_C_LABEL(bootargv):	.long	0	# /boot argv
_C_LABEL(bootdev):	.long	0	# device we booted from
_C_LABEL(proc0paddr):	.long	0
_C_LABEL(PTDpaddr):	.long	0	# paddr of PTD, for libkvm
_C_LABEL(PTDsize):	.long	NBPG	# size of PTD, for libkvm

	.text

NENTRY(proc_trampoline)
#ifdef MULTIPROCESSOR
	call	_C_LABEL(proc_trampoline_mp)
#endif
	movl	$IPL_NONE,CPL
	pushl	%ebx
	call	*%esi
	addl	$4,%esp
	INTRFASTEXIT
	/* NOTREACHED */

	/* This must come before any use of the CODEPATCH macros */
       .section .codepatch,"a"
       .align  8
       .globl _C_LABEL(codepatch_begin)
_C_LABEL(codepatch_begin):
       .previous

       .section .codepatchend,"a"
       .globl _C_LABEL(codepatch_end)
_C_LABEL(codepatch_end):
       .previous

/*****************************************************************************/

/*
 * Signal trampoline; copied to top of user stack.
 */
	.section .rodata
	.globl	_C_LABEL(sigcode)
_C_LABEL(sigcode):
	call	*SIGF_HANDLER(%esp)
	leal	SIGF_SC(%esp),%eax	# scp (the call may have clobbered the
					# copy at SIGF_SCP(%esp))
	pushl	%eax
	pushl	%eax			# junk to fake return address
	movl	$SYS_sigreturn,%eax
	int	$0x80			# enter kernel with args on stack
	.globl	_C_LABEL(sigcoderet)
_C_LABEL(sigcoderet):
	movl	$SYS_exit,%eax
	int	$0x80			# exit if sigreturn fails
	.globl	_C_LABEL(esigcode)
_C_LABEL(esigcode):

	.globl	_C_LABEL(sigfill)
_C_LABEL(sigfill):
	int3
_C_LABEL(esigfill):

	.data
	.globl	_C_LABEL(sigfillsiz)
_C_LABEL(sigfillsiz):
	.long	_C_LABEL(esigfill) - _C_LABEL(sigfill)

	.text

/*****************************************************************************/

/*
 * The following primitives are used to fill and copy regions of memory.
 */

/* Frame pointer reserve on stack. */
#ifdef DDB
#define FPADD 4
#else
#define FPADD 0
#endif

/*
 * kcopy(caddr_t from, caddr_t to, size_t len);
 * Copy len bytes, abort on fault.
 */
ENTRY(kcopy)
#ifdef DDB
	pushl	%ebp
	movl	%esp,%ebp
#endif
	pushl	%esi
	pushl	%edi
	GET_CURPCB(%eax)		# load curpcb into eax and set on-fault
	pushl	PCB_ONFAULT(%eax)
	movl	$_C_LABEL(copy_fault), PCB_ONFAULT(%eax)

	movl	16+FPADD(%esp),%esi
	movl	20+FPADD(%esp),%edi
	movl	24+FPADD(%esp),%ecx
	movl	%edi,%eax
	subl	%esi,%eax
	cmpl	%ecx,%eax		# overlapping?
	jb	1f
	shrl	$2,%ecx			# nope, copy forward by 32-bit words
	rep
	movsl
	movl	24+FPADD(%esp),%ecx
	andl	$3,%ecx			# any bytes left?
	rep
	movsb

	GET_CURPCB(%edx)		# XXX save curpcb?
	popl	PCB_ONFAULT(%edx)
	popl	%edi
	popl	%esi
	xorl	%eax,%eax
#ifdef DDB
	leave
#endif
	ret

	.align  4,0xcc
1:	addl	%ecx,%edi		# copy backward
	addl	%ecx,%esi
	std
	andl	$3,%ecx			# any fractional bytes?
	decl	%edi
	decl	%esi
	rep
	movsb
	movl	24+FPADD(%esp),%ecx	# copy remainder by 32-bit words
	shrl	$2,%ecx
	subl	$3,%esi
	subl	$3,%edi
	rep
	movsl
	cld

	GET_CURPCB(%edx)
	popl	PCB_ONFAULT(%edx)
	popl	%edi
	popl	%esi
	xorl	%eax,%eax
#ifdef DDB
	leave
#endif
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
#ifdef DDB
	pushl	%ebp
	movl	%esp,%ebp
#endif
	pushl	%esi
	pushl	%edi
	pushl	$0	
	
	movl	16+FPADD(%esp),%esi
	movl	20+FPADD(%esp),%edi
	movl	24+FPADD(%esp),%eax

	/*
	 * We check that the end of the destination buffer is not past the end
	 * of the user's address space.  If it's not, then we only need to
	 * check that each page is writable.  The 486 will do this for us; the
	 * 386 will not.  (We assume that pages in user space that are not
	 * writable by the user are not writable by the kernel either.)
	 */
	movl	%edi,%edx
	addl	%eax,%edx
	jc	_C_LABEL(copy_fault)
	cmpl	$VM_MAXUSER_ADDRESS,%edx
	ja	_C_LABEL(copy_fault)

	GET_CURPCB(%edx)
	movl	$_C_LABEL(copy_fault),PCB_ONFAULT(%edx)
	SMAP_STAC

	/* bcopy(%esi, %edi, %eax); */
	movl	%eax,%ecx
	shrl	$2,%ecx
	rep
	movsl
	movl	%eax,%ecx
	andl	$3,%ecx
	rep
	movsb

	SMAP_CLAC
	popl	PCB_ONFAULT(%edx)
	popl	%edi
	popl	%esi
	xorl	%eax,%eax
#ifdef DDB
	leave
#endif
	ret

/*
 * copyin(caddr_t from, caddr_t to, size_t len);
 * Copy len bytes from the user's address space.
 */
ENTRY(copyin)
#ifdef DDB
	pushl	%ebp
	movl	%esp,%ebp
#endif
	pushl	%esi
	pushl	%edi
	GET_CURPCB(%eax)
	pushl	$0
	movl	$_C_LABEL(copy_fault),PCB_ONFAULT(%eax)
	SMAP_STAC
	
	movl	16+FPADD(%esp),%esi
	movl	20+FPADD(%esp),%edi
	movl	24+FPADD(%esp),%eax

	/*
	 * We check that the end of the destination buffer is not past the end
	 * of the user's address space.  If it's not, then we only need to
	 * check that each page is readable, and the CPU will do that for us.
	 */
	movl	%esi,%edx
	addl	%eax,%edx
	jc	_C_LABEL(copy_fault)
	cmpl	$VM_MAXUSER_ADDRESS,%edx
	ja	_C_LABEL(copy_fault)

	/* bcopy(%esi, %edi, %eax); */
	movl	%eax,%ecx
	shrl	$2,%ecx
	rep
	movsl
	movb	%al,%cl
	andb	$3,%cl
	rep
	movsb

	SMAP_CLAC
	GET_CURPCB(%edx)
	popl	PCB_ONFAULT(%edx)
	popl	%edi
	popl	%esi
	xorl	%eax,%eax
#ifdef DDB
	leave
#endif
	ret

ENTRY(copy_fault)
	SMAP_CLAC
	GET_CURPCB(%edx)
	popl	PCB_ONFAULT(%edx)
	popl	%edi
	popl	%esi
	movl	$EFAULT,%eax
#ifdef DDB
	leave
#endif
	ret

/*
 * copyoutstr(caddr_t from, caddr_t to, size_t maxlen, size_t *lencopied);
 * Copy a NUL-terminated string, at most maxlen characters long, into the
 * user's address space.  Return the number of characters copied (including the
 * NUL) in *lencopied.  If the string is too long, return ENAMETOOLONG; else
 * return 0 or EFAULT.
 */
ENTRY(copyoutstr)
#ifdef DDB
	pushl	%ebp
	movl	%esp,%ebp
#endif
	pushl	%esi
	pushl	%edi

	movl	12+FPADD(%esp),%esi		# esi = from
	movl	16+FPADD(%esp),%edi		# edi = to
	movl	20+FPADD(%esp),%edx		# edx = maxlen

5:	GET_CURPCB(%eax)
	movl	$_C_LABEL(copystr_fault),PCB_ONFAULT(%eax)
	SMAP_STAC
	/*
	 * Get min(%edx, VM_MAXUSER_ADDRESS-%edi).
	 */
	movl	$VM_MAXUSER_ADDRESS,%eax
	subl	%edi,%eax
	jbe	_C_LABEL(copystr_fault)		# die if CF == 1 || ZF == 1
						# i.e. make sure that %edi
						# is below VM_MAXUSER_ADDRESS

	cmpl	%edx,%eax
	jae	1f
	movl	%eax,%edx
	movl	%eax,20+FPADD(%esp)

1:	incl	%edx

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
	jae	_C_LABEL(copystr_fault)
	movl	$ENAMETOOLONG,%eax
	jmp	copystr_return

/*
 * copyinstr(caddr_t from, caddr_t to, size_t maxlen, size_t *lencopied);
 * Copy a NUL-terminated string, at most maxlen characters long, from the
 * user's address space.  Return the number of characters copied (including the
 * NUL) in *lencopied.  If the string is too long, return ENAMETOOLONG; else
 * return 0 or EFAULT.
 */
ENTRY(copyinstr)
#ifdef DDB
	pushl	%ebp
	movl	%esp,%ebp
#endif
	pushl	%esi
	pushl	%edi
	GET_CURPCB(%ecx)
	movl	$_C_LABEL(copystr_fault),PCB_ONFAULT(%ecx)
	SMAP_STAC

	movl	12+FPADD(%esp),%esi		# %esi = from
	movl	16+FPADD(%esp),%edi		# %edi = to
	movl	20+FPADD(%esp),%edx		# %edx = maxlen

	/*
	 * Get min(%edx, VM_MAXUSER_ADDRESS-%esi).
	 */
	movl	$VM_MAXUSER_ADDRESS,%eax
	subl	%esi,%eax
	jbe	_C_LABEL(copystr_fault)		# Error if CF == 1 || ZF == 1
						# i.e. make sure that %esi
						# is below VM_MAXUSER_ADDRESS
	cmpl	%edx,%eax
	jae	1f
	movl	%eax,%edx
	movl	%eax,20+FPADD(%esp)

1:	incl	%edx

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
	jae	_C_LABEL(copystr_fault)
	movl	$ENAMETOOLONG,%eax
	jmp	copystr_return

ENTRY(copystr_fault)
	movl	$EFAULT,%eax

copystr_return:
	SMAP_CLAC
	/* Set *lencopied and return %eax. */
	GET_CURPCB(%ecx)
	movl	$0,PCB_ONFAULT(%ecx)
	movl	20+FPADD(%esp),%ecx
	subl	%edx,%ecx
	movl	24+FPADD(%esp),%edx
	testl	%edx,%edx
	jz	8f
	movl	%ecx,(%edx)

8:	popl	%edi
	popl	%esi
#ifdef DDB
	leave
#endif
	ret

/*
 * copystr(caddr_t from, caddr_t to, size_t maxlen, size_t *lencopied);
 * Copy a NUL-terminated string, at most maxlen characters long.  Return the
 * number of characters copied (including the NUL) in *lencopied.  If the
 * string is too long, return ENAMETOOLONG; else return 0.
 */
ENTRY(copystr)
#ifdef DDB
	pushl	%ebp
	movl	%esp,%ebp
#endif
	pushl	%esi
	pushl	%edi

	movl	12+FPADD(%esp),%esi		# esi = from
	movl	16+FPADD(%esp),%edi		# edi = to
	movl	20+FPADD(%esp),%edx		# edx = maxlen
	incl	%edx

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
	movl	20+FPADD(%esp),%ecx
	subl	%edx,%ecx
	movl	24+FPADD(%esp),%edx
	testl	%edx,%edx
	jz	7f
	movl	%ecx,(%edx)

7:	popl	%edi
	popl	%esi
#ifdef DDB
	leave
#endif
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
	movw	%ax,%ds
	movw	%ax,%es
	movw	%ax,%ss
	movl	$GSEL(GCPU_SEL, SEL_KPL),%eax
	movw	%ax,%fs
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
 * cpu_switchto(struct proc *old, struct proc *new)
 * Switch from the "old" proc to the "new" proc. If "old" is NULL, we
 * don't need to bother saving old context.
 */
ENTRY(cpu_switchto)
	pushl	%ebx
	pushl	%esi
	pushl	%edi

	movl	16(%esp), %esi
	movl	20(%esp), %edi

	/* If old process exited, don't bother. */
	testl	%esi,%esi
	jz	switch_exited

	/* Save old stack pointers. */
	movl	P_ADDR(%esi),%ebx
	movl	%esp,PCB_ESP(%ebx)
	movl	%ebp,PCB_EBP(%ebx)

switch_exited:
	/* Restore saved context. */

	/* No interrupts while loading new state. */
	cli

	/* Record new process. */
	movl	%edi, CPUVAR(CURPROC)
	movb	$SONPROC, P_STAT(%edi)

	/* Restore stack pointers. */
	movl	P_ADDR(%edi),%ebx
	movl	PCB_ESP(%ebx),%esp
	movl	PCB_EBP(%ebx),%ebp

	/* Record new pcb. */
	movl	%ebx, CPUVAR(CURPCB)

	/*
	 * Activate the address space.  The pcb copy of %cr3 will
	 * be refreshed from the pmap, and because we're
	 * curproc they'll both be reloaded into the CPU.
	 */
	pushl	%edi
	pushl	%esi
	call	_C_LABEL(pmap_switch)
	addl	$8,%esp

	/* Load TSS info. */
	movl	CPUVAR(GDT),%eax
	movl	P_MD_TSS_SEL(%edi),%edx

	/* Switch TSS. */
	andl	$~0x0200,4-SEL_KPL(%eax,%edx,1)
	ltr	%dx

	/* Restore cr0 (including FPU state). */
	movl	PCB_CR0(%ebx),%ecx
#ifdef MULTIPROCESSOR
	/*
	 * If our floating point registers are on a different CPU,
	 * clear CR0_TS so we'll trap rather than reuse bogus state.
	 */
	movl	CPUVAR(SELF), %esi
	cmpl	PCB_FPCPU(%ebx), %esi
	jz	1f
	orl	$CR0_TS,%ecx
1:	
#endif	
	movl	%ecx,%cr0

	/* Interrupts are okay again. */
	sti

	popl	%edi
	popl	%esi
	popl	%ebx
	ret

ENTRY(cpu_idle_enter)
	movl	_C_LABEL(cpu_idle_enter_fcn),%eax
	cmpl	$0,%eax
	je	1f
	jmpl	*%eax
1:
	ret

ENTRY(cpu_idle_cycle)
	movl	_C_LABEL(cpu_idle_cycle_fcn),%eax
	cmpl	$0,%eax
	je	1f
	call	*%eax
	ret
1:
	sti
	hlt
	ret

ENTRY(cpu_idle_leave)
	movl	_C_LABEL(cpu_idle_leave_fcn),%eax
	cmpl	$0,%eax
	je	1f
	jmpl	*%eax
1:
	ret

/*
 * savectx(struct pcb *pcb);
 * Update pcb, saving current processor state.
 */
ENTRY(savectx)
	movl	4(%esp),%edx		# edx = p->p_addr

	/* Save stack pointers. */
	movl	%esp,PCB_ESP(%edx)
	movl	%ebp,PCB_EBP(%edx)

	movl	PCB_FLAGS(%edx),%ecx
	orl	$PCB_SAVECTX,%ecx
	movl	%ecx,PCB_FLAGS(%edx)

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
 */
#define	IDTVEC(name)	ALIGN_TEXT; .globl X##name; X##name:

#define	TRAP(a)		pushl $(a) ; jmp _C_LABEL(alltraps)
#define	ZTRAP(a)	pushl $0 ; TRAP(a)


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
	TRAP(T_TRCTRAP)
IDTVEC(nmi)
	ZTRAP(T_NMI)
IDTVEC(bpt)
	ZTRAP(T_BPTFLT)
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
#ifdef MULTIPROCESSOR
	pushl	CPUVAR(SELF)
#else
	pushl	$_C_LABEL(cpu_info_primary)
#endif
	call	*_C_LABEL(npxdna_func)
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
IDTVEC(f00f_redirect)
	pushl	$T_PAGEFLT
	INTRENTRY
	testb	$PGEX_U,TF_ERR(%esp)
	jnz	calltrap
	movl	%cr2,%eax
	subl	_C_LABEL(idt),%eax
	cmpl	$(6*8),%eax
	jne	calltrap
	movb	$T_PRIVINFLT,TF_TRAPNO(%esp)
	jmp	calltrap
IDTVEC(page)
	TRAP(T_PAGEFLT)
IDTVEC(rsvd)
	ZTRAP(T_RESERVED)
IDTVEC(mchk)
	ZTRAP(T_MACHK)
IDTVEC(simd)
	ZTRAP(T_XFTRAP)
IDTVEC(intrspurious)
	/*
	 * The Pentium Pro local APIC may erroneously call this vector for a
	 * default IR7.  Just ignore it.
	 *
	 * (The local APIC does this when CPL is raised while it's on the
	 * way to delivering an interrupt.. presumably enough has been set
	 * up that it's inconvenient to abort delivery completely..)
	 */
	iret
IDTVEC(fpu)
#if NNPX > 0
	/*
	 * Handle like an interrupt so that we can call npxintr to clear the
	 * error.  It would be better to handle npx interrupts as traps but
	 * this is difficult for nested interrupts.
	 */
	subl	$8,%esp			/* space for tf_{err,trapno} */
	INTRENTRY
	pushl	CPL			# if_ppl in intrframe
	pushl	%esp			# push address of intrframe
	incl	_C_LABEL(uvmexp)+V_TRAP
	call	_C_LABEL(npxintr)
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
	pushl	%es
	movl	$GSEL(GDATA_SEL, SEL_KPL),%eax
	movw	%ax,%es
NENTRY(resume_pop_es)
	pushl	%gs
	xorl	%eax,%eax	/* $GSEL(GNULL_SEL, SEL_KPL) == 0 */
	movw	%ax,%gs
NENTRY(resume_pop_gs)
	pushl	%fs
	movl	$GSEL(GCPU_SEL, SEL_KPL),%eax
	movw	%ax,%fs
NENTRY(resume_pop_fs)
	movl	$T_PROTFLT,TF_TRAPNO(%esp)
	sti
	jmp	calltrap

/*
 * All traps go through here. Call the generic trap handler, and
 * check for ASTs afterwards.
 */
NENTRY(alltraps)
	INTRENTRY
	sti
calltrap:
#ifdef DIAGNOSTIC
	movl	CPL,%ebx
#endif /* DIAGNOSTIC */
#if !defined(GPROF) && defined(DDBPROF)
	cmpl	$T_BPTFLT,TF_TRAPNO(%esp)
	jne	.Lreal_trap

	pushl	%esp
	call	_C_LABEL(db_prof_hook)
	addl	$4,%esp
	cmpl	$1,%eax
	jne	.Lreal_trap

	/*
	 * Abuse the error field to indicate that INTRFASTEXIT needs
	 * to emulate the patched instruction.
	 */
	movl	$INTR_FAKE_TRAP, TF_ERR(%esp)
	jz	2f
.Lreal_trap:
#endif /* !defined(GPROF) && defined(DDBPROF) */
	pushl	%esp
	call	_C_LABEL(trap)
	addl	$4,%esp
2:	/* Check for ASTs on exit to user mode. */
	cli
	CHECK_ASTPENDING(%ecx)
	je	1f
	testb	$SEL_RPL,TF_CS(%esp)
#ifdef VM86
	jnz	5f
	testl	$PSL_VM,TF_EFLAGS(%esp)
#endif
	jz	1f
5:	CLEAR_ASTPENDING(%ecx)
	sti
	pushl	%esp
	call	_C_LABEL(ast)
	addl	$4,%esp
	jmp	2b
1:
#if !defined(GPROF) && defined(DDBPROF)
	/*
	 * If we are returning from a probe trap we need to fix the
	 * stack layout and emulate the patched instruction.
	 *
	 * The code below does that by trashing %eax, so it MUST be
	 * restored afterward.
	 */
	cmpl	$INTR_FAKE_TRAP, TF_ERR(%esp)
	je	.Lprobe_fixup
#endif /* !defined(GPROF) && defined(DDBPROF) */
#ifndef DIAGNOSTIC
	INTRFASTEXIT
#else
	cmpl	CPL,%ebx
	jne	3f
	INTRFASTEXIT
3:	sti
	pushl	$spl_lowered
	call	_C_LABEL(printf)
	addl	$4,%esp
#if defined(DDB) && 0
	int	$3
#endif /* DDB */
	movl	%ebx,CPL
	jmp	2b

	.section .rodata
spl_lowered:
	.asciz	"WARNING: SPL NOT LOWERED ON TRAP EXIT\n"
#endif /* DIAGNOSTIC */

	.text
#if !defined(GPROF) && defined(DDBPROF)
.Lprobe_fixup:
	/* Restore all register unwinding the stack. */
	INTR_RESTORE_ALL

	/*
	 * Use the space left by ``err'' and ``trapno'' to emulate
	 * "pushl %ebp".
	 *
	 * Temporarily save %eax.
	 */
	movl	%eax,0(%esp)

	/* Shift hardware-saved registers: eip, cs, eflags */
	movl	8(%esp),%eax
	movl	%eax,4(%esp)
	movl	12(%esp),%eax
	movl	%eax,8(%esp)
	movl	16(%esp),%eax
	movl	%eax,12(%esp)

	/* Store %ebp in the expected location to finish the emulation. */
	movl	%ebp,16(%esp)

	popl	%eax
	iret
#endif /* !defined(GPROF) && defined(DDBPROF) */
/*
 * Trap gate entry for syscall
 */
IDTVEC(syscall)
	subl	$8,%esp			/* space for tf_{err,trapno} */
	INTRENTRY
	pushl	%esp
	call	_C_LABEL(syscall)
	addl	$4,%esp
2:	/* Check for ASTs on exit to user mode. */
	cli
	CHECK_ASTPENDING(%ecx)
	je	1f
	/* Always returning to user mode here. */
	CLEAR_ASTPENDING(%ecx)
	sti
	pushl	%esp
	call	_C_LABEL(ast)
	addl	$4,%esp
	jmp	2b
1:	INTRFASTEXIT

#include <i386/i386/vector.s>
#include <i386/isa/icu.s>

/*
 * bzero (void *b, size_t len)
 *	write len zero bytes to the string b.
 */

ENTRY(bzero)
	pushl	%edi
	movl	8(%esp),%edi
	movl	12(%esp),%edx

	xorl	%eax,%eax		/* set fill data to 0 */

	/*
	 * if the string is too short, it's really not worth the overhead
	 * of aligning to word boundaries, etc.  So we jump to a plain
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

	cmpl	$CPUCLASS_486,_C_LABEL(cpu_class)
	jne	8f

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

#if !defined(SMALL_KERNEL)
ENTRY(sse2_pagezero)
	pushl	%ebx
	movl	8(%esp),%ecx
	movl	%ecx,%eax
	addl	$4096,%eax
	xor	%ebx,%ebx
1:
	movnti	%ebx,(%ecx)
	addl	$4,%ecx
	cmpl	%ecx,%eax
	jne	1b
	sfence
	popl	%ebx
	ret

ENTRY(i686_pagezero)
	pushl	%edi
	pushl	%ebx

	movl	12(%esp), %edi
	movl	$1024, %ecx

	.align  4,0x90
1:
	xorl	%eax, %eax
	repe
	scasl
	jnz	2f

	popl	%ebx
	popl	%edi
	ret

	.align  4,0x90
2:
	incl	%ecx
	subl	$4, %edi

	movl	%ecx, %edx
	cmpl	$16, %ecx

	jge	3f

	movl	%edi, %ebx
	andl	$0x3f, %ebx
	shrl	%ebx
	shrl	%ebx
	movl	$16, %ecx
	subl	%ebx, %ecx

3:
	subl	%ecx, %edx
	rep
	stosl

	movl	%edx, %ecx
	testl	%edx, %edx
	jnz	1b

	popl	%ebx
	popl	%edi
	ret
#endif

/*
 * int cpu_paenable(void *);
 */
ENTRY(cpu_paenable)
	movl	$-1, %eax
	testl	$CPUID_PAE, _C_LABEL(cpu_feature)
	jz	1f

	pushl	%esi
	pushl	%edi
	movl	12(%esp), %esi
	movl	%cr3, %edi
	orl	$0xfe0, %edi    /* PDPT will be in the last four slots! */
	movl	%edi, %cr3
	addl	$KERNBASE, %edi /* and make it back virtual again */
	movl	$8, %ecx
	rep
	movsl

	movl	$MSR_EFER, %ecx
	rdmsr
	orl	$EFER_NXE, %eax
	wrmsr

	movl	%cr4, %eax
	orl	$CR4_PAE, %eax
	movl	%eax, %cr4      /* BANG!!! */

	movl	12(%esp), %eax
	subl	$KERNBASE, %eax
	movl	%eax, %cr3      /* reload real PDPT */
	movl	$4*NBPG, %eax
	movl	%eax, _C_LABEL(PTDsize)

	xorl	%eax, %eax
	popl	%edi
	popl	%esi
1:
	ret

#if NLAPIC > 0
#include <i386/i386/apicvec.s>
#endif

	.section .rodata
	.globl _C_LABEL(_stac)
_C_LABEL(_stac):
	stac

	.globl _C_LABEL(_clac)
_C_LABEL(_clac):
	clac
