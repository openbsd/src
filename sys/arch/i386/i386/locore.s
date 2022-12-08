/*	$OpenBSD: locore.s,v 1.198 2022/12/08 01:25:44 guenther Exp $	*/
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

#define CPL lapic_tpr

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
#define INTR_COPY_FROM_TRAMP_STACK	\
	movl	TRF_SS(%ebp),%eax	; \
	movl	%eax,IRF_SS(%esp)	; \
	movl	TRF_ESP(%ebp),%eax	; \
	movl	%eax,IRF_ESP(%esp)	; \
	movl	TRF_EFLAGS(%ebp),%eax	; \
	movl	%eax,IRF_EFLAGS(%esp)	; \
	movl	TRF_CS(%ebp),%eax	; \
	movl	%eax,IRF_CS(%esp)	; \
	movl	TRF_EIP(%ebp),%eax	; \
	movl	%eax,IRF_EIP(%esp)	; \
	movl	TRF_ERR(%ebp),%eax	; \
	movl	%eax,IRF_ERR(%esp)	; \
	movl	TRF_TRAPNO(%ebp),%eax	; \
	movl	%eax,IRF_TRAPNO(%esp)

#define INTR_ENABLE_U_PLUS_K	\
	movl	$GSEL(GCPU_SEL, SEL_KPL),%eax	; \
	movw	%ax,%fs			; \
	movl	CPUVAR(KERN_CR3),%eax	; \
	testl	%eax,%eax		; \
	jz	100f			; \
	movl	%eax,%cr3		; \
	100:

#define INTRENTRY_LABEL(label)	X##label##_untramp
#define	INTRENTRY(label) \
	/* we have an iretframe */	; \
	testb	$SEL_RPL,IRF_CS(%esp)	; \
	/* from kernel, stay on kernel stack, use iretframe */	; \
	je	INTRENTRY_LABEL(label)	; \
	/* entering from user space, map kernel */	; \
	pushl	%ebp			; \
	pushl	%eax			; \
	pushl	%fs			; \
	INTR_ENABLE_U_PLUS_K		; \
	jmp	99f			; \
	.text				; \
	.global INTRENTRY_LABEL(label) ; \
INTRENTRY_LABEL(label):	/* from kernel */	; \
	jmp	98f			; \
	/* from user space, build trampframe */	; \
99:	movl	CPUVAR(KERN_ESP),%eax	; \
	pushl	%eax			; \
	pushl	$0xdeadbeef		; \
	movl	%esp,%ebp		; \
	movl	%eax,%esp		; \
	subl	$SIZEOF_IRETFRAME,%esp	; \
	/* we have a trampframe, copy to iretframe on kernel stack */	; \
	INTR_COPY_FROM_TRAMP_STACK	; \
	movl	TRF_FS(%ebp),%eax	; \
	movw	%ax,%fs			; \
	movl	TRF_EAX(%ebp),%eax	; \
	movl	TRF_EBP(%ebp),%ebp	; \
98:	INTR_SAVE_ALL

#define INTR_SAVE_ALL \
	cld				; \
	SMAP_CLAC			; \
	/* we have an iretframe, build trapframe */	; \
	subl	$44,%esp		; \
	movl	%eax,TF_EAX(%esp)	; \
	/* the hardware puts err next to %eip, we move it elsewhere and */ ; \
	/* later put %ebp in this slot to make it look like a call frame */ ; \
	movl	(TF_EIP - 4)(%esp),%eax ; \
	movl	%eax,TF_ERR(%esp)	; \
	movl	%ecx,TF_ECX(%esp)	; \
	movl	%edx,TF_EDX(%esp)	; \
	movl	%ebx,TF_EBX(%esp)	; \
	movl	%ebp,TF_EBP(%esp)	; \
	leal	TF_EBP(%esp),%ebp	; \
	movl	%esi,TF_ESI(%esp)	; \
	movl	%edi,TF_EDI(%esp)	; \
	movw	%ds,TF_DS(%esp)		; \
	movw	%es,TF_ES(%esp)		; \
	movw	%gs,TF_GS(%esp)		; \
	movl	$GSEL(GDATA_SEL, SEL_KPL),%eax	; \
	movw	%ax,%ds			; \
	movw	%ax,%es			; \
	xorl	%eax,%eax	; /* $GSEL(GNULL_SEL, SEL_KPL) == 0 */ \
	movw	%ax,%gs			; \
	movw	%fs,TF_FS(%esp)		; \
	movl	$GSEL(GCPU_SEL, SEL_KPL),%eax	; \
	movw	%ax,%fs

#define	INTR_RESTORE_ALL \
	popl	%fs		; \
	popl	%gs		; \
	popl	%es		; \
	popl	%ds		; \
	popl	%edi		; \
	popl	%esi		; \
	addl	$4,%esp	/*err*/ ; \
	popl	%ebx		; \
	popl	%edx		; \
	popl	%ecx		; \
	popl	%eax		; \
	movl	4(%esp),%ebp

#define	INTRFASTEXIT \
	jmp	intr_fast_exit

#define	INTR_FAKE_TRAP_PUSH_RPB	0xbadabada
#define	INTR_FAKE_TRAP_POP_RBP	0xbcbcbcbc

/*
 * PTmap is recursive pagemap at top of virtual address space.
 * Within PTmap, the page directory can be found (third indirection).
 */
	.globl	PTmap, PTD
	.set	PTmap, (PDSLOT_PTE << PDSHIFT)
	.set	PTD, (PTmap + PDSLOT_PTE * NBPG)

/*
 * Initialization
 */
	.data

	.globl	cpu_id, cpu_vendor
	.globl	cpu_brandstr
	.globl	cpuid_level
	.globl	cpu_miscinfo
	.globl	cpu_feature, cpu_ecxfeature
	.globl	ecpu_feature, ecpu_eaxfeature
	.globl	ecpu_ecxfeature
	.globl	cpu_cache_eax, cpu_cache_ebx
	.globl	cpu_cache_ecx, cpu_cache_edx
	.globl	cpu_perf_eax
	.globl	cpu_perf_ebx
	.globl	cpu_perf_edx
	.globl	cpu_apmi_edx
	.globl	cold, cnvmem, extmem
	.globl	cpu_pae
	.globl	esym
	.globl	ssym
	.globl	nkptp_max
	.globl	boothowto, bootdev, atdevbase
	.globl	proc0paddr, PTDpaddr, PTDsize
	.globl	gdt
	.globl	bootapiver, bootargc, bootargv
	.globl	lapic_tpr
	.globl	pg_g_kern
	.globl	cpu_meltdown

#if NLAPIC > 0
	.align NBPG
	.globl local_apic, lapic_id
local_apic:
	.space	LAPIC_ID
lapic_id:
	.long	0x00000000
	.space	LAPIC_TPRI-(LAPIC_ID+4)
lapic_tpr:
	.space	LAPIC_PPRI-LAPIC_TPRI
lapic_ppr:
	.space	LAPIC_ISR-LAPIC_PPRI
lapic_isr:
	.space	NBPG-LAPIC_ISR
#else
lapic_tpr:
	.long	0
#endif

cpu_id:			.long	0	# saved from 'cpuid' instruction
cpu_pae:		.long	0	# are we using PAE paging mode?
cpu_miscinfo:		.long	0	# misc info (apic/brand id) from 'cpuid'
cpu_feature:		.long	0	# feature flags from 'cpuid' instruction
ecpu_feature:		.long	0	# extended feature flags from 'cpuid'
cpu_ecxfeature:		.long	0	# ecx feature flags from 'cpuid'
ecpu_eaxfeature:	.long	0	# extended eax feature flags
ecpu_ecxfeature:	.long	0	# extended ecx feature flags
cpuid_level:		.long	-1	# max. lvl accepted by 'cpuid' insn
cpu_cache_eax:		.long	0
cpu_cache_ebx:		.long	0
cpu_cache_ecx:		.long	0
cpu_cache_edx:		.long	0
cpu_perf_eax:		.long	0	# arch. perf. mon. flags from 'cpuid'
cpu_perf_ebx:		.long	0	# arch. perf. mon. flags from 'cpuid'
cpu_perf_edx:		.long	0	# arch. perf. mon. flags from 'cpuid'
cpu_apmi_edx:		.long	0	# adv. power management info. 'cpuid'
cpu_vendor:		.space	16	# vendor string returned by 'cpuid' instruction
cpu_brandstr:		.space	48	# brand string returned by 'cpuid'
cold:			.long	1	# cold till we are not
ssym:			.long	0	# ptr to start of syms
esym:			.long	0	# ptr to end of syms
cnvmem:			.long	0	# conventional memory size
extmem:			.long	0	# extended memory size
atdevbase:		.long	0	# location of start of iomem in virtual
bootapiver:		.long	0	# /boot API version
bootargc:		.long	0	# /boot argc
bootargv:		.long	0	# /boot argv
bootdev:		.long	0	# device we booted from
proc0paddr:		.long	0
PTDpaddr:		.long	0	# paddr of PTD, for libkvm
PTDsize:		.long	NBPG	# size of PTD, for libkvm
pg_g_kern:		.long	0	# 0x100 if global pages should be used
					# in kernel mappings, 0 otherwise (for
					# insecure CPUs)
cpu_meltdown: .long		0	# 1 if this CPU has Meltdown

	.text

NENTRY(proc_trampoline)
#ifdef MULTIPROCESSOR
	call	proc_trampoline_mp
#endif
	movl	$IPL_NONE,CPL
	pushl	%ebx
	call	*%esi
	addl	$4,%esp
#ifdef DIAGNOSTIC
	movl	$0xfe,%esi
#endif
	jmp	.Lsyscall_check_asts

	/* This must come before any use of the CODEPATCH macros */
       .section .codepatch,"a"
       .align  8
       .globl codepatch_begin
codepatch_begin:
       .previous

       .section .codepatchend,"a"
       .globl codepatch_end
codepatch_end:
       .previous

/*****************************************************************************/

/*
 * Signal trampoline; copied to top of user stack.
 */
	.section .rodata
	.globl	sigcode
sigcode:
	call	*SIGF_HANDLER(%esp)
	leal	SIGF_SC(%esp),%eax	# scp (the call may have clobbered the
					# copy at SIGF_SCP(%esp))
	pushl	%eax
	pushl	%eax			# junk to fake return address
	movl	$SYS_sigreturn,%eax
	int	$0x80			# enter kernel with args on stack
	.globl	sigcoderet
sigcoderet:
	movl	$SYS_exit,%eax
	int	$0x80			# exit if sigreturn fails
	.globl	esigcode
esigcode:

	.globl	sigfill
sigfill:
	int3
esigfill:

	.data
	.globl	sigfillsiz
sigfillsiz:
	.long	esigfill - sigfill

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
	movl	$copy_fault, PCB_ONFAULT(%eax)

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
	jc	copy_fault
	cmpl	$VM_MAXUSER_ADDRESS,%edx
	ja	copy_fault

	GET_CURPCB(%edx)
	movl	$copy_fault,PCB_ONFAULT(%edx)
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
	movl	$copy_fault,PCB_ONFAULT(%eax)
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
	jc	copy_fault
	cmpl	$VM_MAXUSER_ADDRESS,%edx
	ja	copy_fault

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
	movl	$copystr_fault,PCB_ONFAULT(%eax)
	SMAP_STAC
	/*
	 * Get min(%edx, VM_MAXUSER_ADDRESS-%edi).
	 */
	movl	$VM_MAXUSER_ADDRESS,%eax
	subl	%edi,%eax
	jbe	copystr_fault			# die if CF == 1 || ZF == 1
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
	jae	copystr_fault
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
	movl	$copystr_fault,PCB_ONFAULT(%ecx)
	SMAP_STAC

	movl	12+FPADD(%esp),%esi		# %esi = from
	movl	16+FPADD(%esp),%edi		# %edi = to
	movl	20+FPADD(%esp),%edx		# %edx = maxlen

	/*
	 * Get min(%edx, VM_MAXUSER_ADDRESS-%esi).
	 */
	movl	$VM_MAXUSER_ADDRESS,%eax
	subl	%esi,%eax
	jbe	copystr_fault			# Error if CF == 1 || ZF == 1
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
	jae	copystr_fault
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

#ifdef DDB
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
#endif /* DDB */

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

	/* record the bits needed for future U-->K transition */
	movl	PCB_KSTACK(%ebx),%eax
	subl	$FRAMESIZE,%eax
	movl	%eax,CPUVAR(KERN_ESP)

	/*
	 * Activate the address space.  The pcb copy of %cr3 will
	 * be refreshed from the pmap, and because we're
	 * curproc they'll both be reloaded into the CPU.
	 */
	pushl	%edi
	pushl	%esi
	call	pmap_switch
	addl	$8,%esp

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
	movl	cpu_idle_enter_fcn,%eax
	cmpl	$0,%eax
	je	1f
	jmpl	*%eax
1:
	ret

ENTRY(cpu_idle_cycle)
	movl	cpu_idle_cycle_fcn,%eax
	cmpl	$0,%eax
	je	1f
	call	*%eax
	ret
1:
	sti
	hlt
	ret

ENTRY(cpu_idle_leave)
	movl	cpu_idle_leave_fcn,%eax
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

#define	TRAP(a)		pushl $(a) ; jmp alltraps
#define	ZTRAP(a)	pushl $0 ; TRAP(a)

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
	/*
	 * we came through a task gate; now U+K of the idle thread is
	 * enabled; NMIs are blocked until next iret; IRQs are disabled;
	 * all segment descriptors are useable
	 *
	 * first of all, switch back to the U+K we were actually running
	 * on before
	 */
	movl	CPUVAR(CURPMAP),%eax
	movl	PM_PDIRPA(%eax),%eax
	movl	%eax,%cr3

	/*
	 * when we came from within the kernel, iret will not
	 * switch back to the stack we came from but will keep
	 * running on the NMI stack. in that case we switch
	 * manually back to the stack we were running on and
	 * build the iretframe there.
	 */

	/* was there a ring transition? */
	movl	CPUVAR(TSS),%eax
	testb	$SEL_RPL,TSS_CS(%eax)
	jne	1f

	/*
	 * no ring transition, switch back to original stack, build
	 * frame from state saved in TSS.
	 */
	movl	TSS_ESP(%eax),%esp
	subl	$12,%esp
	movl	TSS_EFLAGS(%eax),%ebx
	movl	%ebx,8(%esp)
	movl	TSS_CS(%eax),%ebx
	movl	%ebx,4(%esp)
	movl	TSS_EIP(%eax),%ebx
	movl	%ebx,0(%esp)
	pushl	$0
	pushl	$T_NMI
	jmp	2f

	/*
	 * ring transition, stay on stack, build frame from state
	 * saved in TSS.
	 */
1:	subl	$20,%esp
	pushl	$0
	pushl	$T_NMI
	movl	TSS_SS(%eax),%ebx
	movl	%ebx,IRF_SS(%esp)
	movl	TSS_ESP(%eax),%ebx
	movl	%ebx,IRF_ESP(%esp)
	movl	TSS_EFLAGS(%eax),%ebx
	movl	%ebx,IRF_EFLAGS(%esp)
	movl	TSS_CS(%eax),%ebx
	movl	%ebx,IRF_CS(%esp)
	movl	TSS_EIP(%eax),%ebx
	movl	%ebx,IRF_EIP(%esp)

	/* clear PSL_NT */
2:	pushfl
	popl	%eax
	andl	$~PSL_NT,%eax
	pushl	%eax
	popfl

	/* clear CR0_TS XXX hshoexer: needed? */
	movl	%cr0,%eax
	andl	$~CR0_TS,%eax
	movl	%eax,%cr0

	/* unbusy descriptors and reload common TSS */
	movl	CPUVAR(GDT),%eax
	movl	$GSEL(GNMITSS_SEL, SEL_KPL),%ebx
	andl	$~0x200,4-SEL_KPL(%eax,%ebx,1)
	movl	$GSEL(GTSS_SEL, SEL_KPL),%ebx
	andl	$~0x200,4-SEL_KPL(%eax,%ebx,1)
	ltr	%bx

	/* load GPRs and segment registers with saved values from common TSS */
	movl	CPUVAR(TSS),%eax
	movl	TSS_ECX(%eax),%ecx
	movl	TSS_EDX(%eax),%edx
	movl	TSS_ESI(%eax),%esi
	movl	TSS_EDI(%eax),%edi
	movl	TSS_EBP(%eax),%ebp
	movw	TSS_FS(%eax),%fs
	movw	TSS_GS(%eax),%gs
	movw	TSS_ES(%eax),%es
	/* saved %ds might be invalid, thus push now and pop later */
	movl	TSS_DS(%eax),%ebx
	pushl	%ebx
	movl	TSS_EBX(%eax),%ebx
	movl	TSS_EAX(%eax),%eax
	popl	%ds

	/*
	 * we can now proceed and save everything on the stack as
	 * if no task switch had happened.
	 */
	jmp alltraps
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
	INTRENTRY(dna)
	sti
	pushl	CPUVAR(SELF)
	call	*npxdna_func
	addl	$4,%esp
	testl	%eax,%eax
	jz	calltrap
#ifdef DIAGNOSTIC
	movl	$0xfd,%esi
#endif
	cli
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
	pushl	$T_PROTFLT
	/* If iret faults, we'll get a trap at doreti_iret+3 with CPL == 0. */
	pushl	%eax
	leal	doreti_iret+3,%eax
	cmpl	%eax,12(%esp)	/* over %eax, trapno and err to %eip */
	popl	%eax
	jne	97f
	pushl	%ebp
	pushl	%eax
	pushl	%fs
	INTR_ENABLE_U_PLUS_K
	/*
	 * we have an iretframe on trampoline stack, above it the
	 * remainder of the original iretframe iret faulted on.
	 */
	movl	CPUVAR(KERN_ESP),%eax
	pushl	%eax
	pushl	$0xdeadbeef
	/*
	 * now we have a trampframe on trampoline stack, above it the
	 * remainder of the original iretframe iret faulted on.
	 */
	movl	%esp,%ebp
	movl	%eax,%esp
	subl	$SIZEOF_IRETFRAME+(5*4),%esp
	/* copy to iretframe on kernel stack */
	movl	TRF_EFLAGS(%ebp),%eax
	movl	%eax,IRF_EFLAGS(%esp)
	movl	TRF_CS(%ebp),%eax
	movl	%eax,IRF_CS(%esp)
	movl	TRF_EIP(%ebp),%eax
	movl	%eax,IRF_EIP(%esp)
	movl	TRF_ERR(%ebp),%eax
	movl	%eax,IRF_ERR(%esp)
	movl	TRF_TRAPNO(%ebp),%eax
	movl	%eax,IRF_TRAPNO(%esp)
	/* copy remainder of faulted iretframe */
	movl	40(%ebp),%eax		/* eip */
	movl	%eax,20(%esp)
	movl	44(%ebp),%eax		/* cs */
	movl	%eax,24(%esp)
	movl	48(%ebp),%eax		/* eflags */
	movl	%eax,28(%esp)
	movl	52(%ebp),%eax		/* esp */
	movl	%eax,32(%esp)
	movl	56(%ebp),%eax		/* ss */
	movl	%eax,36(%esp)
	movl	TRF_FS(%ebp),%eax
	movw	%ax,%fs
	movl	TRF_EAX(%ebp),%eax
	movl	TRF_EBP(%ebp),%ebp
	/*
	 * we have an iretframe on kernel stack, above it the
	 * remainder of the original iretframe iret faulted on.
	 * for INTRENTRY(prot) it looks like the fault happened
	 * on the kernel stack
	 */
97:	INTRENTRY(prot)
	sti
	jmp	calltrap
IDTVEC(f00f_redirect)
	pushl	$T_PAGEFLT
	INTRENTRY(f00f_redirect)
	sti
	testb	$PGEX_U,TF_ERR(%esp)
	jnz	calltrap
	movl	%cr2,%eax
	subl	idt,%eax
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
	INTRENTRY(fpu)
	sti
	pushl	CPL			# if_ppl in intrframe
	pushl	%esp			# push address of intrframe
	incl	uvmexp+V_TRAP
	call	npxintr
	addl	$8,%esp			# pop address and if_ppl
#ifdef DIAGNOSTIC
	movl	$0xfc,%esi
#endif
	cli
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
KUENTRY(resume_iret)
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
KUENTRY(alltraps)
	INTRENTRY(alltraps)
	sti
calltrap:
#ifdef DIAGNOSTIC
	movl	CPL,%ebx
#endif /* DIAGNOSTIC */
#if !defined(GPROF) && defined(DDBPROF)
	cmpl	$T_BPTFLT,TF_TRAPNO(%esp)
	jne	.Lreal_trap

	pushl	%esp
	subl	$4, %esp
	pushl	%eax
	leal	dt_prov_kprobe, %eax
	movl	%eax, 4(%esp)
	popl	%eax
	call	dt_prov_kprobe_hook
	addl	$8, %esp
	cmpl	$0, %eax
	je	.Lreal_trap

	/*
	 * Abuse the error field to indicate that INTRFASTEXIT needs
	 * to emulate the patched instruction.
	 */
	cmpl	$1, %eax
	je	.Lset_emulate_push_rbp

	cmpl	$2, %eax
	je	.Lset_emulate_ret

.Lset_emulate_push_rbp:
	movl	$INTR_FAKE_TRAP_PUSH_RPB, TF_ERR(%esp)
	jmp	.Lalltraps_check_asts
.Lset_emulate_ret:
	movl	$INTR_FAKE_TRAP_POP_RBP, TF_ERR(%esp)
	jmp	.Lalltraps_check_asts
.Lreal_trap:
#endif /* !defined(GPROF) && defined(DDBPROF) */
	pushl	%esp
	call	trap
	addl	$4,%esp

.Lalltraps_check_asts:
	/* Check for ASTs on exit to user mode. */
	cli
	CHECK_ASTPENDING(%ecx)
	je	1f
	testb	$SEL_RPL,TF_CS(%esp)
	jz	1f
5:	CLEAR_ASTPENDING(%ecx)
	sti
	pushl	%esp
	call	ast
	addl	$4,%esp
	jmp	.Lalltraps_check_asts
1:
#if !defined(GPROF) && defined(DDBPROF)
	/*
	 * If we are returning from a probe trap we need to fix the
	 * stack layout and emulate the patched instruction.
	 *
	 * The code below does that by trashing %eax, so it MUST be
	 * restored afterward.
	 */
	cmpl	$INTR_FAKE_TRAP_PUSH_RPB, TF_ERR(%esp)
	je	.Lprobe_fixup_push_rbp
	cmpl	$INTR_FAKE_TRAP_POP_RBP, TF_ERR(%esp)
	je	.Lprobe_fixup_pop_rbp
#endif /* !defined(GPROF) && defined(DDBPROF) */
#ifndef DIAGNOSTIC
	INTRFASTEXIT
#else
	cmpl	CPL,%ebx
	jne	3f
#ifdef DIAGNOSTIC
	movl	$0xfb,%esi
#endif
	INTRFASTEXIT
3:	sti
	pushl	$spl_lowered
	call	printf
	addl	$4,%esp
#if defined(DDB) && 0
	int	$3
#endif /* DDB */
	movl	%ebx,CPL
	jmp	.Lalltraps_check_asts

	.section .rodata
spl_lowered:
	.asciz	"WARNING: SPL NOT LOWERED ON TRAP EXIT\n"
#endif /* DIAGNOSTIC */

	.text
#if !defined(GPROF) && defined(DDBPROF)
.Lprobe_fixup_push_rbp:
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
.Lprobe_fixup_pop_rbp:
	/* Restore all register unwinding the stack. */
	INTR_RESTORE_ALL

	movl	%eax, 0(%esp)

	/* pop %ebp */
	movl	20(%esp), %ebp
	/* Shift hardware-saved registers: eflags, cs, eip */
	movl	16(%esp), %eax
	movl	%eax, 20(%esp)
	movl	12(%esp), %eax
	movl	%eax, 16(%esp)
	movl	8(%esp), %eax
	movl	%eax, 12(%esp)

	/* Pop eax and restore the stack pointer */
	popl	%eax
	addl	$8, %esp
	iret
#endif /* !defined(GPROF) && defined(DDBPROF) */

	.text
#ifdef DIAGNOSTIC
.Lintr_exit_not_blocked:
	movl	warn_once,%eax
	testl	%eax,%eax
	jnz	1f
	incl	%eax
	movl	%eax,warn_once
	pushl	%esi		/* marker indicating where we came from */
	pushl	%edx		/* EFLAGS are in %edx */
	pushl	$.Lnot_blocked
	call	printf
	addl	$12,%esp
#ifdef DDB
	int	$3
#endif	/* DDB */
1:	cli
	jmp	intr_fast_exit

	.data
	.global warn_once
warn_once:
	.long	0
	.section .rodata
.Lnot_blocked:
	.asciz	"WARNING: INTERRUPTS NOT BLOCKED ON INTERRUPT RETURN 0x%x 0x%x\n"
	.text
#endif

/*
 * Trap gate entry for syscall
 */
IDTVEC(syscall)
	subl	$8,%esp			/* space for tf_{err,trapno} */
	INTRENTRY(syscall)
	sti
	pushl	%esp
	call	syscall
	addl	$4,%esp

.Lsyscall_check_asts:
	/* Check for ASTs on exit to user mode. */
	cli
	CHECK_ASTPENDING(%ecx)
	je	1f
	/* Always returning to user mode here. */
	CLEAR_ASTPENDING(%ecx)
	sti
	pushl	%esp
	call	ast
	addl	$4,%esp
	jmp	.Lsyscall_check_asts
1:
#ifdef DIAGNOSTIC
	movl	$0xff,%esi
#endif
	jmp intr_fast_exit

NENTRY(intr_fast_exit)
#ifdef DIAGNOSTIC
	pushfl
	popl	%edx
	testl	$PSL_I,%edx
	jnz	.Lintr_exit_not_blocked
#endif
	/* we have a full trapframe */
	INTR_RESTORE_ALL
	/* now we have an iretframe */
	testb	$SEL_RPL,IRF_CS(%esp)
	/* recursing into kernel: stay on kernel stack using iretframe */
	je	doreti_iret

	/* leaving kernel: build trampframe on cpu stack */
	pushl	%ebp
	pushl	%eax
	pushl	%fs
        movl	$GSEL(GCPU_SEL, SEL_KPL),%eax
	movw	%ax,%fs
	movl	CPUVAR(INTR_ESP),%eax
	pushl	%eax
	pushl	$0xcafecafe
	/* now we have an trampframe, copy frame to cpu stack */
	movl	%eax,%ebp
	movl	TRF_EIP(%esp),%eax
	movl	%eax,TRF_EIP(%ebp)
	movl	TRF_CS(%esp),%eax
	movl	%eax,TRF_CS(%ebp)
	movl	TRF_EFLAGS(%esp),%eax
	movl	%eax,TRF_EFLAGS(%ebp)
	movl	TRF_ESP(%esp),%eax
	movl	%eax,TRF_ESP(%ebp)
	movl	TRF_SS(%esp),%eax
	movl	%eax,TRF_SS(%ebp)
	movl	TRF__DEADBEEF(%esp),%eax
	movl	%eax,TRF__DEADBEEF(%ebp)
	movl	TRF__KERN_ESP(%esp),%eax
	movl	%eax,TRF__KERN_ESP(%ebp)
	movl	TRF_FS(%esp),%eax
	movl	%eax,TRF_FS(%ebp)
	movl	TRF_EAX(%esp),%eax
	movl	%eax,TRF_EAX(%ebp)
	movl	TRF_EBP(%esp),%eax
	movl	%eax,TRF_EBP(%ebp)
	/* switch to cpu stack, where we copied the trampframe */
	movl	%ebp,%esp
	movl	CPUVAR(USER_CR3),%eax
	testl	%eax,%eax
	jz	1f
	jmp	iret_tramp

KUENTRY(iret_tramp)
	movl	%eax,%cr3
	/* we have a trampframe; restore registers and adjust to iretframe */
1:	popl	%eax
	popl	%eax
	popl	%fs
	popl	%eax
	popl	%ebp
	.globl	doreti_iret
doreti_iret:
	/* we have an iretframe */
	addl	$IRF_EIP,%esp
	iret

#include <i386/i386/vector.s>
#include <i386/isa/icu.s>

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
	testl	$CPUID_PAE, cpu_feature
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
	movl	%eax, PTDsize

	xorl	%eax, %eax
	popl	%edi
	popl	%esi
1:
	ret

#if NLAPIC > 0
#include <i386/i386/apicvec.s>
#endif

	.section .rodata
	.globl _stac
_stac:
	stac

	.globl _clac
_clac:
	clac
