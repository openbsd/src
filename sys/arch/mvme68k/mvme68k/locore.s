/*	$OpenBSD: locore.s,v 1.40 2004/04/24 21:09:37 miod Exp $ */

/*
 * Copyright (c) 1995 Theo de Raadt
 * Copyright (c) 1999 Steve Murphree, Jr. (68060 support)
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1980, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 * from: Utah $Hdr: locore.s 1.66 92/12/22$
 *
 *	@(#)locore.s	8.6 (Berkeley) 5/27/94
 */

#include "assym.h"
#include "ksyms.h"
#include <machine/asm.h>
#include <machine/prom.h>
#include <machine/trap.h>

/*
 * Macro to relocate a symbol, used before MMU is enabled.
 */
#define	_RELOC(var, ar) \
	lea	var,ar

#define	RELOC(var, ar)		_RELOC(_C_LABEL(var), ar)
#define	ASRELOC(var, ar)	_RELOC(_ASM_LABEL(var), ar)

/*
 * Macro to invoke a BUG routine.
 */
#define BUGCALL(id) \
	trap	#15;	\
	.short	id

/*
 * Temporary stack for a variety of purposes.
 * Try and make this the first thing is the data segment so it
 * is page aligned.  Note that if we overflow here, we run into
 * our text segment.
 */
	.data
	.space	NBPG
ASLOCAL(tmpstk)

/*
 * Initialization
 *
 * The bootstrap loader loads us in starting at 0, and VBR is non-zero.
 * On entry, args on stack are boot device, boot filename, console unit,
 * boot flags (howto), boot device name, filesystem type name.
 */
BSS(lowram, 4)
BSS(esym, 4)
BSS(emini, 4)
BSS(smini, 4)
BSS(needprom, 4)
BSS(promvbr, 4)
BSS(promcall, 4)

	.text
/*
GLOBAL(edata)
GLOBAL(etext)
GLOBAL(end)
*/
GLOBAL(kernel_text)

ASENTRY_NOPROFILE(start)
	movw	#PSL_HIGHIPL,sr		| no interrupts
	movl	#0,a5			| RAM starts at 0
	movl	sp@(4), d7		| get boothowto
	movl	sp@(8), d6		| get bootaddr
	movl	sp@(12),d5		| get bootctrllun
	movl	sp@(16),d4		| get bootdevlun
	movl	sp@(20),d3		| get bootpart
	movl	sp@(24),d2		| get esyms
	/* note: d2-d7 in use */

	ASRELOC(tmpstk, a0)
	movl	a0,sp			| give ourselves a temporary stack

	RELOC(edata, a0)		| clear out BSS
	movl	#_C_LABEL(end)-4,d0	| (must be <= 256 kB)
	subl	#_C_LABEL(edata),d0
	lsrl	#2,d0
1:	clrl	a0@+
	dbra	d0,1b

	movc	vbr,d0			| save prom's trap #15 vector
	RELOC(promvbr, a0)
	movl	d0, a0@
	RELOC(esym, a0)
	movl	d2,a0@			| store end of symbol table
	/* note: d2 now free, d3-d7 still in use */
	
        RELOC(lowram, a0)
	movl	a5,a0@			| store start of physical memory

	clrl	sp@-
	BUGCALL(MVMEPROM_GETBRDID)
	movl	sp@+, a1

	movl	#SIZEOF_MVMEPROM_BRDID, d0	| copy to local variables
	RELOC(brdid, a0)
1:	movb	a1@+, a0@+
	subql	#1, d0
	bne	1b

	clrl	d0
	RELOC(brdid, a1)
	movw	a1@(MVMEPROM_BRDID_MODEL), d0
	RELOC(cputyp, a0)
	movl	d0, a0@			| init _cputyp

#ifdef MVME147
	cmpw	#CPU_147, d0
	beq	is147
#endif

#ifdef MVME162
	cmpw	#CPU_162, d0
	beq	is162
#endif

#ifdef MVME167
	cmpw	#CPU_166, d0
	beq	is167
	cmpw	#CPU_167, d0
	beq	is167
#endif

#ifdef MVME177
#ifdef notyet	
        cmpw	#CPU_176, d0
	beq	is177
#endif        
        cmpw	#CPU_177, d0
	beq	is177
#endif
	
#ifdef MVME172
	cmpw	#CPU_172, d0
	beq	is172
#endif
	
	.data
notsup:	.asciz	"kernel does not support this model."
notsupend:
	.even
	.text

	| first we bitch, then we die.
	movl	#notsupend, sp@-
	movl	#notsup, sp@-
	BUGCALL(MVMEPROM_OUTSTRCRLF)
	addql	#8,sp

	BUGCALL(MVMEPROM_EXIT)		| return to m68kbug
	/*NOTREACHED*/

#ifdef MVME147
is147:
	RELOC(mmutype, a0)		| no, we have 68030
	movl	#MMU_68030,a0@		| set to reflect 68030 PMMU

	RELOC(cputype, a0)		| no, we have 68030
	movl	#CPU_68030,a0@		| set to reflect 68030 CPU

	movl	#CACHE_OFF,d0
	movc	d0,cacr			| clear and disable on-chip cache(s)

	movb	#0, 0xfffe1026		| XXX serial interrupt off
	movb	#0, 0xfffe1018		| XXX timer 1 off
	movb	#0, 0xfffe1028		| XXX ethernet off

	movl	#0xfffe0000, a0		| mvme147 nvram base
	| move nvram component of etheraddr (only last 3 bytes)
	RELOC(myea, a1)
	movw	a0@(NVRAM_147_ETHER+0), a1@(3+0)
	movb	a0@(NVRAM_147_ETHER+2), a1@(3+2)
	movl	a0@(NVRAM_147_EMEM), d1	| pass memory size

	RELOC(iiomapsize, a1)
	movl	#INTIOSIZE_147, a1@
	RELOC(iiomapbase, a1)
	movl	#INTIOBASE_147, a1@
	bra	Lstart1
#endif

#ifdef MVME162
is162:
#if 0
	| the following 3 things are "just in case". they won't make
	| the kernel work properly, but they will at least let it get
	| far enough that you can figure out that something had an
	| interrupt pending. which the bootrom shouldn't allow, i don't
	| think..
	clrb	0xfff42002 		| XXX MCchip irq off
	clrl	0xfff42018		| XXX MCchip timers irq off
	clrb	0xfff4201d		| XXX MCchip scc irq off
#endif
	RELOC(memsize162, a1)		| how much memory?
	jbsr	a1@
	movl	d0, d2

	RELOC(mmutype, a0)
	movl	#MMU_68040,a0@		| with a 68040 MMU

	RELOC(cputype, a0)		| no, we have 68040
	movl	#CPU_68040,a0@		| set to reflect 68040 CPU

	RELOC(fputype, a0)
	movl	#FPU_68040,a0@		| and a 68040 FPU

	RELOC(vectab, a1)
	movl	#_C_LABEL(buserr40),a1@(8)
	movl	#_C_LABEL(addrerr4060),a1@(12)

	bra	is16x
#endif

#ifdef MVME167
is167:
|	RELOC(needprom,a0)		| this machine needs the prom mapped!
|	movl	#1,a0@

	RELOC(memsize1x7, a1)		| how much memory?
	jbsr	a1@

	RELOC(mmutype, a0)
	movl	#MMU_68040,a0@		| with a 68040 MMU

	RELOC(cputype, a0)		| no, we have 68040
	movl	#CPU_68040,a0@		| set to reflect 68040 CPU

	RELOC(fputype, a0)
	movl	#FPU_68040,a0@		| and a 68040 FPU

	RELOC(vectab, a1)
	movl	#_C_LABEL(buserr40),a1@(8)
	movl	#_C_LABEL(addrerr4060),a1@(12)

	bra	is16x
#endif

#ifdef MVME172
is172:
	
	RELOC(memsize162, a1)		| how much memory?
	jbsr	a1@
	movl	d0, d2
        
        /* enable Super Scalar Dispatch */        
	.word	0x4e7a,0x0808		| movc	pcr,d0
	bset   #0,d0                    | turn on bit 0.
	.word	0x4e7b,0x0808		| movc	d0,pcr  Bang!
        
	RELOC(mmutype, a0)
	movl	#MMU_68060,a0@		| with a 68060 MMU

	RELOC(cputype, a0)		| no, we have 68060
	movl	#CPU_68060,a0@		| set to reflect 68060 CPU

	RELOC(fputype, a0)
	movl	#FPU_68060,a0@		| and a 68060 FPU

	RELOC(vectab, a1)
	movl	#_C_LABEL(buserr60),a1@(8)
	movl	#_C_LABEL(addrerr4060),a1@(12)

	bra	is16x
#endif

#ifdef MVME177
is177:

|	RELOC(needprom,a0)		| this machine needs the prom mapped!
|	movl	#1,a0@
	
	RELOC(memsize1x7, a1)		| how much memory?
	jbsr	a1@
        
        /* enable Super Scalar Dispatch */        
	.word	0x4e7a,0x0808		| movc	pcr,d0
	bset   #0,d0                    | turn on bit 0.
	.word	0x4e7b,0x0808		| movc	d0,pcr  Bang!  We are smokin' !

	RELOC(mmutype, a0)
	movl	#MMU_68060,a0@		| with a 68060 MMU

	RELOC(cputype, a0)		| no, we have 68060
	movl	#CPU_68060,a0@		| set to reflect 68060 CPU
	
	RELOC(fputype, a0)
	movl	#FPU_68060,a0@		| and a 68060 FPU

	RELOC(vectab, a1)
	movl	#_C_LABEL(buserr60),a1@(8)
	movl	#_C_LABEL(addrerr4060),a1@(12)

	bra	is16x
#endif

#if defined(MVME162) || defined(MVME167) || defined(MVME177) || defined(MVME172)
#define	ROMPKT_LEN	200
BSS(rompkt, ROMPKT_LEN)
	.even
	.text
is16x:
	RELOC(iiomapsize, a1)
	movl	#INTIOSIZE_162, a1@
	RELOC(iiomapbase, a1)
	movl	#INTIOBASE_162, a1@

	/* get ethernet address */
	RELOC(rompkt, a0)		| build a .NETCTRL packet
	movb	#0, a0@(NETCTRL_DEV)	| onboard ethernet
	movb	#0, a0@(NETCTRL_CTRL)	| onboard ethernet
	movl	#NETCTRLCMD_GETETHER, a0@(NETCTRL_CMD)
	RELOC(myea, a1)
	movl	a1, a0@(NETCTRL_ADDR)	| where to put it
	movl	#6, a0@(NETCTRL_LEN)	| it is 6 bytes long

	movl	a0, sp@-
	BUGCALL(MVMEPROM_NETCTRL)	| ask the rom
	addl	#4, sp

	| if memory size is unknown, print a diagnostic and make an
	| assumption
	movl	d2, d1
	cmpl	#0, d1
	bne	Lstart1

	movl	#unkmemend, sp@-
	movl	#unkmem, sp@-
	BUGCALL(MVMEPROM_OUTSTRCRLF)
	addql	#8,sp

	movl	#4*1024*1024, d1	| XXX assume 4M of ram
	bra	Lstart1

	.data
unkmem:	.asciz	"could not figure out how much memory; assuming 4M."
unkmemend:
	.even
	.text

#endif

Lstart1:
/* initialize source/destination control registers for movs */
	moveq	#FC_USERD,d0		| user space
	movc	d0,sfc			|   as source
	movc	d0,dfc			|   and destination of transfers
        moveq	#PGSHIFT,d2
	lsrl	d2,d1			| convert to page (click) number
	RELOC(maxmem, a0)
	movl	d1,a0@			| save as maxmem
	movl	a5,d0			| lowram value from ROM via boot
	lsrl	d2,d0			| convert to page number
	subl	d0,d1			| compute amount of RAM present
	RELOC(physmem, a0)
	movl	d1,a0@			| and physmem

/* configure kernel and proc0 VA space so we can get going */
#if defined(DDB) || NKSYMS > 0
	RELOC(esym,a0)			| end of static kernel text/data/syms
	movl	a0@,d2
	jne	Lstart2
#endif
	movl	#_C_LABEL(end),d2	| end of static kernel text/data
Lstart2:
	addl	#NBPG-1,d2
	andl	#PG_FRAME,d2		| round to a page
	movl	d2,a4
	addl	a5,a4			| convert to PA
#if 0
	| XXX clear from end-of-kernel to 1M, as a workaround for an
	| insane pmap_bootstrap bug I cannot find (68040-specific)
	movl	a4,a0
	movl	#1024*1024,d0
	cmpl	a0,d0			| end of kernel is beyond 1M?
	jlt	2f
	subl	a0,d0
1:	clrb	a0@+
	subql	#1,d0
	bne	1b
2:
#endif

/* do pmap_bootstrap stuff */	
	RELOC(mmutype, a0)
        pea	a5@			| firstpa
	pea	a4@			| nextpa
	RELOC(pmap_bootstrap,a0)
	jbsr	a0@			| pmap_bootstrap(firstpa, nextpa)
	addql	#8,sp

/*
 * Enable the MMU.
 * Since the kernel is mapped logical == physical, we just turn it on.
 */
Lmmu_enable:
	RELOC(Sysseg, a0)		| system segment table addr
	movl	a0@,d1			| read value (a KVA)
	addl	a5,d1			| convert to PA
	RELOC(mmutype, a0)
	cmpl	#MMU_68040,a0@		| 68040 or 68060?
	jgt	Lmotommu1 		| no, skip
	.long	0x4e7b1807		| movc d1,srp
	.long	0x4e7b1806		| movc d1,urp
	jra	Lstploaddone
Lmotommu1:
	RELOC(protorp, a0)
	movl	#0x80000202,a0@		| nolimit + share global + 4 byte PTEs
	movl	d1,a0@(4)		| + segtable address
	pmove	a0@,srp			| load the supervisor root pointer
	movl	#0x80000002,a0@		| reinit upper half for CRP loads
Lstploaddone:
	RELOC(mmutype, a0)
	cmpl	#MMU_68040,a0@		| 68040 or 68060?
	jgt	Lmotommu2		| no, skip

	RELOC(needprom,a0)
	cmpl	#0,a0@
	beq	1f
	/*
	 * this machine needs the prom mapped. we use the translation
	 * registers to map it in.. and the ram it needs.
	 */
	movel	#0xff00a044,d0		| map top 16meg 1/1 for bug eprom exe
	.long	0x4e7b0004		| movc d0,itt0
	moveq	#0,d0			| ensure itt1 is disabled
	.long	0x4e7b0005		| movc d0,itt1
	movel	#0xff00a040,d0		| map top 16meg 1/1 for bug io access
	.long	0x4e7b0006		| movc d0,dtt0
	moveq	#0,d0			| ensure dtt1 is disabled
	.long	0x4e7b0007		| movc d0,dtt1
	bra	2f
1:
	moveq	#0,d0			| ensure TT regs are disabled
	.long	0x4e7b0004		| movc d0,itt0
	.long	0x4e7b0005		| movc d0,itt1
	.long	0x4e7b0006		| movc d0,dtt0
	.long	0x4e7b0007		| movc d0,dtt1
2:

	.word	0xf4d8			| cinva bc
	.word	0xf518			| pflusha
        movl	#0x8000,d0
	.long	0x4e7b0003		| movc d0,tc
	/* Enable 68060 extensions here */
	RELOC(mmutype, a0)
	cmpl	#MMU_68060,a0@		| 68060?
        jne     Lchache040
        movl	#CACHE60_ON,d0          | branch cache, etc...
	movc	d0,cacr			| turn on both caches
	jmp	Lenab1
Lchache040:
        movl	#CACHE40_ON,d0
	movc	d0,cacr			| turn on both caches
	jmp	Lenab1
Lmotommu2:
	movl	#0x82c0aa00,a2@		| value to load TC with
	pmove	a2@,tc			| load it
Lenab1:

/*
 * Should be running mapped from this point on
 */
/* select the software page size now */
	lea	_ASM_LABEL(tmpstk),sp	| temporary stack
	jbsr	_C_LABEL(uvm_setpagesize) | select software page size
/* set kernel stack, user SP, and initial pcb */
	movl	_C_LABEL(proc0paddr),a1	| get proc0 pcb addr
	lea	a1@(USPACE-4),sp	| set kernel stack to end of area
	lea	_C_LABEL(proc0), a2	| initialize proc0.p_addr so that
	movl	a1,a2@(P_ADDR)		|  we don't deref NULL in trap()
	movl	#USRSTACK-4,a2
	movl	a2,usp			| init user SP
	movl	a1,_C_LABEL(curpcb)	| proc0 is running

	tstl	_C_LABEL(fputype)	| Have an FPU?
	jeq	Lenab2			| No, skip.
	clrl	a1@(PCB_FPCTX)		| ensure null FP context
	movl	a1,sp@-
	jbsr	_C_LABEL(m68881_restore) | restore it (does not kill a1)
	addql	#4,sp
Lenab2:
/* flush TLB and turn on caches */
	jbsr	_C_LABEL(TBIA)		| invalidate TLB
	cmpl	#MMU_68040,_C_LABEL(mmutype) | 68040 or 68060?
	jle	Lnocache0		| yes, cache already on
	movl	#CACHE_ON,d0
	movc	d0,cacr			| clear cache(s)
Lnocache0:
/* final setup for C code */
#if 1
	movl	#_vectab,d2		| set VBR
	movc	d2,vbr
#endif	
	movw	#PSL_LOWIPL,sr		| lower SPL
	movl	d3, _C_LABEL(bootpart)	| save bootpart
	movl	d4, _C_LABEL(bootdevlun) | save bootdevlun
	movl	d5, _C_LABEL(bootctrllun) | save bootctrllun
	movl	d6, _C_LABEL(bootaddr)	| save bootaddr
	movl	d7, _C_LABEL(boothowto)	| save boothowto
	/* d3-d7 now free */

/* Final setup for call to main(). */
	jbsr	_C_LABEL(mvme68k_init)
 
/*
 * Create a fake exception frame so that cpu_fork() can copy it.
 * main() never returns; we exit to user mode from a forked process
 * later on.
 */
	clrw	sp@-			| vector offset/frame type
	clrl	sp@-			| PC - filled in by "execve"
	movw	#PSL_USER,sp@-		| in user mode
	clrl	sp@-			| stack adjust count and padding
	lea	sp@(-64),sp		| construct space for D0-D7/A0-A7
	lea	_C_LABEL(proc0),a0	| save pointer to frame
	movl	sp,a0@(P_MD_REGS)	|   in proc0.p_md.md_regs

	jra	_C_LABEL(main)		| main()
	PANIC("main() returned")
	/* NOTREACHED */

#if defined(MVME162) || defined(MVME167) || defined(MVME177) || defined(MVME172)
/*
 * Figure out the size of onboard DRAM by querying the memory controller(s).
 * This has to be done in locore as badaddr() can not yet be used at this
 * point.
 */
GLOBAL(memsize1x7)
	movl	#0xfff43008,a0		| MEMC040/MEMECC Controller #1
	jbsr	memc040read
	movl	d0,d2

	movl	#0xfff43108,a0		| MEMC040/MEMECC Controller #2
	jbsr	memc040read
	addl	d0,d2

	rts

/*
 * Probe for a memory controller ASIC (MEMC040 or MEMECC) at the
 * address in a0. If found, return the size in bytes of any RAM
 * controller by the ASIC in d0. Otherwise return zero.
 */
ASLOCAL(memc040read)
	moveml	d1-d2/a1-a2,sp@-	| save scratch regs
	movc	vbr,d2			| Save vbr
	RELOC(vectab,a2)		| Install our own vectab, temporarily
	movc	a2,vbr
	ASRELOC(Lmemc040berr,a1)	| get address of bus error handler
	movl	a2@(8),sp@-		| Save current bus error handler addr
	movl	a1,a2@(8)		| Install our own handler
	movl	sp,d0			| Save current stack pointer value
	movql	#0x07,d1
	andb	a0@,d1			| Access MEMC040/MEMECC
	movl	#0x400000,d0
	lsll	d1,d0			| Convert to memory size, in bytes
Lmemc040ret:
	movc	d2,vbr			| Restore original vbr
	movl	sp@+,a2@(8)		| Restore original bus error handler
	moveml	sp@+,d1-d2/a1-a2
	rts
/*
 * If the memory controller doesn't exist, we get a bus error trying
 * to access a0@ above. Control passes here, where we flag 'no bytes',
 * ditch the exception frame and return as normal.
 */
Lmemc040berr:
	movl	d0,sp			| Get rid of the exception frame
	clrl	d0			| No ASIC at this location, then!
	jbra	Lmemc040ret		| Done
#endif

/*
 * proc_trampoline: call function in register a2 with a3 as an arg
 * and then rei.
 */
GLOBAL(proc_trampoline)
	movl	a3,sp@-			| push function arg
	jbsr	a2@			| call function
	addql	#4,sp			| pop arg
	movl	sp@(FR_SP),a0		| grab and load
	movl	a0,usp			|   user SP
	moveml	sp@+,#0x7FFF		| restore most user regs
	addql	#8,sp			| toss SP and stack adjust
	jra	_ASM_LABEL(rei)		| and return


/*
 * Trap/interrupt vector routines - new for 060
 */
#include <m68k/m68k/trap_subr.s>

#if defined(M68040) || defined(M68060)
ENTRY_NOPROFILE(addrerr4060)
	clrl	sp@-			| stack adjust count
	moveml	#0xFFFF,sp@-		| save user registers
	movl	usp,a0			| save the user SP
	movl	a0,sp@(FR_SP)		|   in the savearea
	movl	sp@(FR_HW+8),sp@-
	clrl	sp@-			| dummy code
	movl	#T_ADDRERR,sp@-		| mark address error
	jra	_ASM_LABEL(faultstkadj)	| and deal with it
#endif

#if defined(M68060)
ENTRY_NOPROFILE(buserr60)
	clrl	sp@-			| stack adjust count
	moveml	#0xFFFF,sp@-		| save user registers
	movl	usp,a0			| save the user SP
	movl	a0,sp@(FR_SP)		|   in the savearea
	movel	sp@(FR_HW+12),d0	| FSLW
	btst	#2,d0			| branch prediction error?
	jeq	Lnobpe			
	movc	cacr,d2
	orl	#IC60_CABC,d2		| clear all branch cache entries
	movc	d2,cacr
	movl	d0,d1
	addql	#1,L60bpe
	andl	#0x7ffd,d1
	jeq	_ASM_LABEL(faultstkadjnotrap2)
Lnobpe:
| we need to adjust for misaligned addresses
	movl	sp@(FR_HW+8),d1		| grab VA
	btst	#27,d0			| check for mis-aligned access
	jeq	Lberr3			| no, skip
	addl	#28,d1			| yes, get into next page
            				| operand case: 3,
                                        | instruction case: 4+12+12
	andl	#PG_FRAME,d1            | and truncate
Lberr3:
	movl	d1,sp@-
	movl	d0,sp@-			| code is FSLW now.
	andw	#0x1f80,d0 
	jeq	Lbuserr60		| no, handle as usual
	movl	#T_MMUFLT,sp@-		| show that we are an MMU fault
	jra	_ASM_LABEL(faultstkadj)	| and deal with it
Lbuserr60:
	tstl	_C_LABEL(nofault)	| device probe?
	jeq	Lisberr			| Bus Error?
	movl	_C_LABEL(nofault),sp@-	| yes,
	jbsr	_C_LABEL(longjmp)	|  longjmp(nofault)
#endif
#if defined(M68040)
ENTRY_NOPROFILE(buserr40)
        clrl	sp@-			| stack adjust count
	moveml	#0xFFFF,sp@-		| save user registers
	movl	usp,a0			| save the user SP
	movl	a0,sp@(FR_SP)		|   in the savearea
	movl	sp@(FR_HW+20),d1	| get fault address
	moveq	#0,d0
	movw	sp@(FR_HW+12),d0	| get SSW
	btst	#11,d0			| check for mis-aligned
	jeq	Lbe1stpg		| no skip
	addl	#3,d1			| get into next page
	andl	#PG_FRAME,d1		| and truncate
Lbe1stpg:
	movl	d1,sp@-			| pass fault address.
	movl	d0,sp@-			| pass SSW as code
	btst	#10,d0			| test ATC
	jeq	Lbuserr40		| no, handle as usual
	movl	#T_MMUFLT,sp@-		| show that we are an MMU fault
	jra	_ASM_LABEL(faultstkadj)	| and deal with it
Lbuserr40:        
	tstl	_C_LABEL(nofault)	| device probe?
	jeq	Lisberr			| it is a bus error
	movl	_C_LABEL(nofault),sp@-	| yes,
	jbsr	_C_LABEL(longjmp)	|  longjmp(nofault)
	/* NOTREACHED */
#endif

ENTRY_NOPROFILE(busaddrerr2030)
	clrl	sp@-			| stack adjust count
	moveml	#0xFFFF,sp@-		| save user registers
	movl	usp,a0			| save the user SP
	movl	a0,sp@(FR_SP)		|   in the savearea
	lea	sp@(FR_HW),a1		| grab base of HW berr frame
	moveq	#0,d0
	movw	a1@(10),d0		| grab SSW for fault processing
	btst	#12,d0			| RB set?
	jeq	LbeX0			| no, test RC
	bset	#14,d0			| yes, must set FB
	movw	d0,a1@(10)		| for hardware too
LbeX0:
	btst	#13,d0			| RC set?
	jeq	LbeX1			| no, skip
	bset	#15,d0			| yes, must set FC
	movw	d0,a1@(10)		| for hardware too
LbeX1:
	btst	#8,d0			| data fault?
	jeq	Lbe0			| no, check for hard cases
	movl	a1@(16),d1		| fault address is as given in frame
	jra	Lbe10			| thats it
Lbe0:
	btst	#4,a1@(6)		| long (type B) stack frame?
	jne	Lbe4			| yes, go handle
	movl	a1@(2),d1		| no, can use save PC
	btst	#14,d0			| FB set?
	jeq	Lbe3			| no, try FC
	addql	#4,d1			| yes, adjust address
	jra	Lbe10			| done
Lbe3:
	btst	#15,d0			| FC set?
	jeq	Lbe10			| no, done
	addql	#2,d1			| yes, adjust address
	jra	Lbe10			| done
Lbe4:
	movl	a1@(36),d1		| long format, use stage B address
	btst	#15,d0			| FC set?
	jeq	Lbe10			| no, all done
	subql	#2,d1			| yes, adjust address
Lbe10:
	movl	d1,sp@-			| push fault VA
	movl	d0,sp@-			| and padded SSW
	movw	a1@(6),d0		| get frame format/vector offset
	andw	#0x0FFF,d0		| clear out frame format
	cmpw	#12,d0			| address error vector?
	jeq	Lisaerr			| yes, go to it
	movl	d1,a0			| fault address
	movl	sp@,d0			| function code from ssw
	btst	#8,d0			| data fault?
	jne	Lbe10a
	movql	#1,d0			| user program access FC
					| (we dont separate data/program)
	btst	#5,a1@			| supervisor mode?
	jeq	Lbe10a			| if no, done
	movql	#5,d0			| else supervisor program access
Lbe10a:
	ptestr	d0,a0@,#7		| do a table search
	pmove	psr,sp@			| save result
	movb	sp@,d1
	btst	#2,d1			| invalid (incl. limit viol. and berr)?
	jeq	Lmightnotbemerr		| no -> wp check
	btst	#7,d1			| is it MMU table berr?
	jeq	Lismerr			| no, must be fast
	jra	Lisberr1		| real bus err needs not be fast.
Lmightnotbemerr:
	btst	#3,d1			| write protect bit set?
	jeq	Lisberr1		| no: must be bus error
	movl	sp@,d0			| ssw into low word of d0
	andw	#0xc0,d0		| Write protect is set on page:
	cmpw	#0x40,d0		| was it read cycle?
	jeq	Lisberr1		| yes, was not WPE, must be bus err
Lismerr:
	movl	#T_MMUFLT,sp@-		| show that we are an MMU fault
	jra	_ASM_LABEL(faultstkadj)	| and deal with it
Lisaerr:
	movl	#T_ADDRERR,sp@-		| mark address error
	jra	_ASM_LABEL(faultstkadj)	| and deal with it
Lisberr1:
	clrw	sp@			| re-clear pad word
Lisberr:
	movl	#T_BUSERR,sp@-		| mark bus error
	jra	_ASM_LABEL(faultstkadj)	| and deal with it

/*
 * FP exceptions.
 */
ENTRY_NOPROFILE(fpfline)
#if defined(M68040) || defined(M68060)
	cmpl	#FPU_68040,_C_LABEL(fputype) | 68040 or 68060 FPU?
	jlt	Lfp_unimp		| no, skip FPSP
	cmpw	#0x202c,sp@(6)		| format type 2?
	jne	_C_LABEL(illinst)	| no, not an FP emulation
Ldofp_unimp:
#ifdef FPSP
	jmp	_ASM_LABEL(fpsp_unimp)	| yes, go handle it
#endif
Lfp_unimp:
#endif/* M68040 || M68060 */
#ifdef FPU_EMULATE
	clrl	sp@-			| stack adjust count
	moveml	#0xFFFF,sp@-		| save registers
	moveq	#T_FPEMULI,d0		| denote as FP emulation trap
	jra	_ASM_LABEL(fault)	| do it
#else
	jra	_C_LABEL(illinst)
#endif

ENTRY_NOPROFILE(fpunsupp)
#if defined(M68040) || defined(M68060)
	cmpl	#FPU_68040,_C_LABEL(fputype) | 68040 or 68060 FPU?
	jlt	_C_LABEL(illinst)	| no, treat as illinst
#ifdef FPSP
	jmp	_ASM_LABEL(fpsp_unsupp)	| yes, go handle it
#endif
Lfp_unsupp:
#endif /* M68040 */
#ifdef FPU_EMULATE
	clrl	sp@-			| stack adjust count
	moveml	#0xFFFF,sp@-		| save registers
	moveq	#T_FPEMULD,d0		| denote as FP emulation trap
	jra	_ASM_LABEL(fault)		| do it
#else
	jra	_C_LABEL(illinst)
#endif

/*
 * Handles all other FP coprocessor exceptions.
 * Note that since some FP exceptions generate mid-instruction frames
 * and may cause signal delivery, we need to test for stack adjustment
 * after the trap call.
 */
ENTRY_NOPROFILE(fpfault)
	clrl	sp@-		| stack adjust count
	moveml	#0xFFFF,sp@-	| save user registers
	movl	usp,a0		| and save
	movl	a0,sp@(FR_SP)	|   the user stack pointer
	clrl	sp@-		| no VA arg
	movl	_C_LABEL(curpcb),a0 | current pcb
	lea	a0@(PCB_FPCTX),a0 | address of FP savearea
	fsave	a0@		| save state
#if defined(M68040) || defined(M68060)
	/* always null state frame on 68040, 68060 */
	cmpl	#CPU_68040,_C_LABEL(cputype)
	jge	Lfptnull
#endif
	tstb	a0@		| null state frame?
	jeq	Lfptnull	| yes, safe
	clrw	d0		| no, need to tweak BIU
	movb	a0@(1),d0	| get frame size
	bset	#3,a0@(0,d0:w)	| set exc_pend bit of BIU
Lfptnull:
	fmovem	fpsr,sp@-	| push fpsr as code argument
	frestore a0@		| restore state
	movl	#T_FPERR,sp@-	| push type arg
	jra	_ASM_LABEL(faultstkadj)	| call trap and deal with stack cleanup

/*
 * Other exceptions only cause four and six word stack frame and require
 * no post-trap stack adjustment.
 */
ENTRY_NOPROFILE(hardtrap)
	moveml	#0xC0C0,sp@-		| save scratch regs
	lea	sp@(16),a1		| get pointer to frame
	movl	a1,sp@-
	movw	sp@(26),d0
	movl	d0,sp@-			| push exception vector info
	movl	sp@(26),sp@-		| and PC
	jbsr	_C_LABEL(hardintr)	| doit
	lea	sp@(12),sp		| pop args
	moveml	sp@+,#0x0303		| restore regs
	jra	_ASM_LABEL(rei)		| all done

ENTRY_NOPROFILE(badtrap)
	moveml	#0xC0C0,sp@-		| save scratch regs
	movw	sp@(22),sp@-		| push exception vector info
	clrw	sp@-
	movl	sp@(22),sp@-		| and PC
	jbsr	_C_LABEL(straytrap)	| report
	addql	#8,sp			| pop args
	moveml	sp@+,#0x0303		| restore regs
	jra	_ASM_LABEL(rei)		| all done

ENTRY_NOPROFILE(trap0)
	clrl	sp@-			| stack adjust count
	moveml	#0xFFFF,sp@-		| save user registers
	movl	usp,a0			| save the user SP
	movl	a0,sp@(FR_SP)		|   in the savearea
	movl	d0,sp@-			| push syscall number
	jbsr	_C_LABEL(syscall)	| handle it
	addql	#4,sp			| pop syscall arg
	tstl	_C_LABEL(astpending)
	jne	Lrei2
	tstb	_C_LABEL(ssir)
	jeq	Ltrap1
	movw	#SPL1,sr
	tstb	_C_LABEL(ssir)
	jne	Lsir1
Ltrap1:
	movl	sp@(FR_SP),a0		| grab and restore
	movl	a0,usp			|   user SP
	moveml	sp@+,#0x7FFF		| restore most registers
	addql	#8,sp			| pop SP and stack adjust
	rte

/*
 * Trap 1 - sigreturn
 */
ENTRY_NOPROFILE(trap1)
	jra	_ASM_LABEL(sigreturn)

/*
 * Trap 2 - trace trap
 */
ENTRY_NOPROFILE(trap2)
	jra	_C_LABEL(trace)

/*
 * Trap 12 is the entry point for the cachectl "syscall" (both HPUX & BSD)
 *	cachectl(command, addr, length)
 * command in d0, addr in a1, length in d1
 */
ENTRY_NOPROFILE(trap12)
	movl	d1,sp@-			| push length
	movl	a1,sp@-			| push addr
	movl	d0,sp@-			| push command
	jbsr	_C_LABEL(cachectl)	| do it
	lea	sp@(12),sp		| pop args
	jra	_ASM_LABEL(rei)		| all done

/*
 * Trace (single-step) trap (trap 1 or 2) instruction. Kernel-mode is
 * special. User mode traps are simply passed on to trap().
 */
ENTRY_NOPROFILE(trace)
	clrl	sp@-
	moveml	#0xFFFF,sp@-
	moveq	#T_TRACE,d0

	| Check PSW and see what happened.
	|   T=0 S=0	(should not happen)
	|   T=1 S=0	trace trap from user mode
	|   T=0 S=1	trace trap on a trap instruction
	|   T=0 S=0	trace trap from system mode (kernel breakpoint)

	movw	sp@(FR_HW),d1		| get SSW
	notw	d1			| XXX no support for T0 on 680[234]0
	andw	#PSL_S,d1		| from system mode (T=1, S=1)?
	jeq	Lkbrkpt			| yes, kernel breakpoint
	jra	_ASM_LABEL(fault)	| no, user-mode fault

/*
 * Trap 15 is used for:
 *	- GDB breakpoints (in user programs)
 *	- KGDB breakpoints (in the kernel)
 *	- trace traps for SUN binaries (not fully supported yet)
 *	- calling the prom, but only from the kernel
 * We just pass it on and let trap() sort it all out
 */
ENTRY_NOPROFILE(trap15)
	clrl	sp@-			| stack adjust count
	moveml	#0xFFFF,sp@-

	tstl	_C_LABEL(promcall)
	jeq	L_notpromcall
	moveml	sp@+,#0xFFFF
	addql	#4, sp
	| unwind stack to put to known value
	| this routine is from the 147 BUG manual
	| currently save and restore are excessive.
	subql	#4,sp
	link	a6,#0
	moveml	#0xFFFE,sp@-
	movl	_C_LABEL(promvbr),a0
	movw	a6@(14),d0
	andl	#0xfff,d0
	movl	a0@(d0:w),a6@(4)
	moveml	sp@+,#0x7FFF
	unlk	a6
	rts
	| really jumps to the bug trap handler
L_notpromcall:
	moveq	#T_TRAP15,d0
	movw	sp@(FR_HW),d1		| get PSW
	andw	#PSL_S,d1		| from system mode?
	jne	Lkbrkpt			| yes, kernel breakpoint
	jra	_ASM_LABEL(fault)	| no, user-mode fault

Lkbrkpt: | Kernel-mode breakpoint or trace trap. (d0=trap_type)
	| Save the system sp rather than the user sp.
	movw	#PSL_HIGHIPL,sr		| lock out interrupts
	lea	sp@(FR_SIZE),a6		| Save stack pointer
	movl	a6,sp@(FR_SP)		|   from before trap

	| If we are not on tmpstk switch to it.
	| (so debugger can change the stack pointer)
	movl	a6,d1
	cmpl	#_ASM_LABEL(tmpstk),d1
	jls	Lbrkpt2			| already on tmpstk
	| Copy frame to the temporary stack
	movl	sp,a0			| a0=src
	lea	_ASM_LABEL(tmpstk)-96,a1 | a1=dst
	movl	a1,sp			| sp=new frame
	moveq	#FR_SIZE,d1
Lbrkpt1:
	movl	a0@+,a1@+
	subql	#4,d1
	bgt	Lbrkpt1

Lbrkpt2:
	| Call the trap handler for the kernel debugger.
	| Do not call trap() to do it, so that we can
	| set breakpoints in trap() if we want.  We know
	| the trap type is either T_TRACE or T_BREAKPOINT.
	| If we have both DDB and KGDB, let KGDB see it first,
	| because KGDB will just return 0 if not connected.
	| Save args in d2, a2
	movl	d0,d2			| trap type
	movl	sp,a2			| frame ptr
#ifdef KGDB
	| Let KGDB handle it (if connected)
	movl	a2,sp@-			| push frame ptr
	movl	d2,sp@-			| push trap type
	jbsr	_C_LABEL(kgdb_trap)	| handle the trap
	addql	#8,sp			| pop args
	cmpl	#0,d0			| did kgdb handle it?
	jne	Lbrkpt3			| yes, done
#endif
#ifdef DDB
	| Let DDB handle it
	movl	a2,sp@-			| push frame ptr
	movl	d2,sp@-			| push trap type
	jbsr	_C_LABEL(kdb_trap)	| handle the trap
	addql	#8,sp			| pop args
	cmpl	#0,d0			| did ddb handle it?
	jne	Lbrkpt3			| yes, done
#endif
	| Drop into the prom
	BUGCALL(MVMEPROM_EXIT)
Lbrkpt3:
	| The stack pointer may have been modified, or
	| data below it modified (by kgdb push call),
	| so push the hardware frame at the current sp
	| before restoring registers and returning.

	movl	sp@(FR_SP),a0		| modified sp
	lea	sp@(FR_SIZE),a1		| end of our frame
	movl	a1@-,a0@-		| copy 2 longs with
	movl	a1@-,a0@-		| ... predecrement
	movl	a0,sp@(FR_SP)		| sp = h/w frame
	moveml	sp@+,#0x7FFF		| restore all but sp
	movl	sp@,sp			| ... and sp
	rte				| all done

/* Use common m68k sigreturn */
#include <m68k/m68k/sigreturn.s>

/*
 * Interrupt handlers.
 * No device interrupts are auto-vectored.
 */

ENTRY_NOPROFILE(spurintr)
	addql	#1,_C_LABEL(intrcnt)+0
	addql	#1,_C_LABEL(uvmexp)+UVMEXP_INTRS
	jra	_ASM_LABEL(rei)		| all done

/*
 * Emulation of VAX REI instruction.
 *
 * This code deals with checking for and servicing ASTs
 * (profiling, scheduling) and software interrupts (network, softclock).
 * We check for ASTs first, just like the VAX.  To avoid excess overhead
 * the T_ASTFLT handling code will also check for software interrupts so we
 * do not have to do it here.  After identifing that we need an AST we
 * drop the IPL to allow device interrupts.
 *
 * This code is complicated by the fact that sendsig may have been called
 * necessitating a stack cleanup.
 */

BSS(ssir,1)

ASENTRY_NOPROFILE(rei)
	tstl	_C_LABEL(astpending)	| AST pending?
	jeq	Lchksir			| no, go check for SIR
Lrei1:
	btst	#5,sp@			| yes, are we returning to user mode?
	jne	Lchksir			| no, go check for SIR
	movw	#PSL_LOWIPL,sr		| lower SPL
	clrl	sp@-			| stack adjust
	moveml	#0xFFFF,sp@-		| save all registers
	movl	usp,a1			| including
	movl	a1,sp@(FR_SP)		|    the users SP
Lrei2:
	clrl	sp@-			| VA == none
	clrl	sp@-			| code == none
	movl	#T_ASTFLT,sp@-		| type == async system trap
	jbsr	_C_LABEL(trap)		| go handle it
	lea	sp@(12),sp		| pop value args
	movl	sp@(FR_SP),a0		| restore user SP
	movl	a0,usp			|   from save area
	movw	sp@(FR_ADJ),d0		| need to adjust stack?
	jne	Laststkadj		| yes, go to it
	moveml	sp@+,#0x7FFF		| no, restore most user regs
	addql	#8,sp			| toss SP and stack adjust
	rte				| and do real RTE
Laststkadj:
	lea	sp@(FR_HW),a1		| pointer to HW frame
	addql	#8,a1			| source pointer
	movl	a1,a0			| source
	addw	d0,a0			|  + hole size = dest pointer
	movl	a1@-,a0@-		| copy
	movl	a1@-,a0@-		|  8 bytes
	movl	a0,sp@(FR_SP)		| new SSP
	moveml	sp@+,#0x7FFF		| restore user registers
	movl	sp@,sp			| and our SP
	rte				| and do real RTE
Lchksir:
	tstb	_C_LABEL(ssir)		| SIR pending?
	jeq	Ldorte			| no, all done
	movl	d0,sp@-			| need a scratch register
	movw	sp@(4),d0		| get SR
	andw	#PSL_IPL7,d0		| mask all but IPL
	jne	Lnosir			| came from interrupt, no can do
	movl	sp@+,d0			| restore scratch register
Lgotsir:
	movw	#SPL1,sr		| prevent others from servicing int
	tstb	_C_LABEL(ssir)		| too late?
	jeq	Ldorte			| yes, oh well...
	clrl	sp@-			| stack adjust
	moveml	#0xFFFF,sp@-		| save all registers
	movl	usp,a1			| including
	movl	a1,sp@(FR_SP)		|    the users SP
Lsir1:
	clrl	sp@-			| VA == none
	clrl	sp@-			| code == none
	movl	#T_SSIR,sp@-		| type == software interrupt
	jbsr	_C_LABEL(trap)		| go handle it
	lea	sp@(12),sp		| pop value args
	movl	sp@(FR_SP),a0		| restore
	movl	a0,usp			|   user SP
	moveml	sp@+,#0x7FFF		| and all remaining registers
	addql	#8,sp			| pop SP and stack adjust
	rte
Lnosir:
	movl	sp@+,d0			| restore scratch register
Ldorte:
	rte				| real return

/*
 * Use common m68k signal trampoline.
 */
#include <m68k/m68k/sigcode.s>

/*
 * Use common m68k support routines.
 */
#include <m68k/m68k/support.s>

/*
 * Use common m68k process manipulation routines.
 */
#include <m68k/m68k/proc_subr.s>

	.data
GLOBAL(curpcb)
GLOBAL(masterpaddr)		| XXX compatibility (debuggers)
	.long	0

ASLOCAL(mdpflag)
	.byte	0		| copy of proc md_flags low byte
	.align	2

ASBSS(nullpcb,SIZEOF_PCB)

/*
 * At exit of a process, do a switch for the last time.
 * Switch to a safe stack and PCB, and deallocate the process's resources.
 */
ENTRY(switch_exit)
	movl	sp@(4),a0
	| save state into garbage pcb
	movl	#_ASM_LABEL(nullpcb),_C_LABEL(curpcb)
	lea	_ASM_LABEL(tmpstk),sp	| goto a tmp stack

        /* Schedule the vmspace and stack to be freed. */
	movl    a0,sp@-                 | exit2(p)
	jbsr    _C_LABEL(exit2)
	lea     sp@(4),sp               | pop args

	jra	_C_LABEL(cpu_switch)

/*
 * When no processes are on the runq, Swtch branches to Idle
 * to wait for something to come ready.
 */
ASENTRY_NOPROFILE(Idle)
	stop	#PSL_LOWIPL
	movw	#PSL_HIGHIPL,sr
	movl	_C_LABEL(whichqs),d0
	jeq	_ASM_LABEL(Idle)
	jra	Lsw1

Lbadsw:
	PANIC("switch")
	/*NOTREACHED*/

/*
 * cpu_switch()
 *
 * NOTE: On the mc68851 we attempt to avoid flushing the
 * entire ATC.  The effort involved in selective flushing may not be
 * worth it, maybe we should just flush the whole thing?
 *
 * NOTE 2: With the new VM layout we now no longer know if an inactive
 * user's PTEs have been changed (formerly denoted by the SPTECHG p_flag
 * bit).  For now, we just always flush the full ATC.
 */
ENTRY(cpu_switch)
	movl	_C_LABEL(curpcb),a0	| current pcb
	movw	sr,a0@(PCB_PS)		| save sr before changing ipl
#ifdef notyet
	movl	_C_LABEL(curproc),sp@-	| remember last proc running
#endif
	clrl	_C_LABEL(curproc)

	/*
	 * Find the highest-priority queue that isn't empty,
	 * then take the first proc from that queue.
	 */
	movw	#PSL_HIGHIPL,sr		| lock out interrupts
	movl	_C_LABEL(whichqs),d0
	jeq	_ASM_LABEL(Idle)
Lsw1:
	movl	d0,d1
	negl	d0
	andl	d1,d0
	bfffo	d0{#0:#32},d1
	eorib	#31,d1

	movl	d1,d0
	lslb	#3,d1			| convert queue number to index
	addl	#_C_LABEL(qs),d1	| locate queue (q)
	movl	d1,a1
	movl	a1@(P_FORW),a0		| p = q->p_forw
	cmpal	d1,a0			| anyone on queue?
	jeq	Lbadsw			| no, panic
	movl	a0@(P_FORW),a1@(P_FORW)	| q->p_forw = p->p_forw
	movl	a0@(P_FORW),a1		| n = p->p_forw
	movl	d1,a1@(P_BACK)		| n->p_back = q
	cmpal	d1,a1			| anyone left on queue?
	jne	Lsw2			| yes, skip
	movl	_C_LABEL(whichqs),d1
	bclr	d0,d1			| no, clear bit
	movl	d1,_C_LABEL(whichqs)
Lsw2:
	movl	a0,_C_LABEL(curproc)
	clrl	_C_LABEL(want_resched)
#ifdef notyet
	movl	sp@+,a1
	cmpl	a0,a1			| switching to same proc?
	jeq	Lswdone			| yes, skip save and restore
#endif
	/*
	 * Save state of previous process in its pcb.
	 */
        movl	_C_LABEL(curpcb),a1
	moveml	#0xFCFC,a1@(PCB_REGS)	| save non-scratch registers
	movl	usp,a2			| grab USP (a2 has been saved)
	movl	a2,a1@(PCB_USP)		| and save it
#ifdef FPU_EMULATE
	tstl	_C_LABEL(fputype)	| do we have any FPU?
	jeq	Lswnofpsave		| no, dont save
#endif
	lea	a1@(PCB_FPCTX),a2	| pointer to FP save area
	fsave	a2@			| save FP state
#ifdef M68060
	cmpl	#FPU_68060,_C_LABEL(fputype) | is 68060?
	jeq	Lsavfp60                | yes, goto Lsavfp60
#endif  /* M68060 */
	tstb	a2@			| null state frame?
	jeq	Lswnofpsave		| yes, all done
	fmovem	fp0-fp7,a2@(FPF_REGS)	| save FP general registers
	fmovem	fpcr/fpsr/fpi,a2@(FPF_FPCR)	| save FP control registers
#ifdef M68060
	jra	Lswnofpsave
Lsavfp60:
	tstb	a2@(2)			| null state frame?
	jeq	Lswnofpsave		| yes, all done
	fmovem	fp0-fp7,a2@(FPF_REGS)	| save FP general registers
	fmovem	fpcr,a2@(FPF_FPCR)	| save FP control registers
	fmovem	fpsr,a2@(FPF_FPSR)
	fmovem	fpi,a2@(FPF_FPI)
#endif /* M68060 */
Lswnofpsave:
#ifdef DIAGNOSTIC
	tstl	a0@(P_WCHAN)
	jne	Lbadsw
	cmpb	#SRUN,a0@(P_STAT)
	jne	Lbadsw
#endif
	clrl	a0@(P_BACK)		| clear back link
	| low byte of p_md.md_flags
	movb	a0@(P_MD_FLAGS+3),_ASM_LABEL(mdpflag)
	movl	a0@(P_ADDR),a1		| get p_addr
	movl	a1,_C_LABEL(curpcb)

	/*
	 * Activate process's address space.
	 * XXX Should remember the last USTP value loaded, and call this
	 * XXX only of it has changed.
	 */
	pea	a0@			| push proc
	jbsr	_C_LABEL(pmap_activate)	| pmap_activate(p)
	addql	#4,sp	
	movl	_C_LABEL(curpcb),a1	| restore p_addr

	lea	_ASM_LABEL(tmpstk),sp	| now goto a tmp stack for NMI

	moveml	a1@(PCB_REGS),#0xFCFC	| and registers
	movl	a1@(PCB_USP),a0
	movl	a0,usp			| and USP

#ifdef FPU_EMULATE
	tstl	_C_LABEL(fputype)	| do we _have_ any fpu?
	jne	Lresnonofpatall
	movw	a1@(PCB_PS),sr		| no, restore PS
	moveq	#1,d0			| return 1 (for alternate returns)
	rts
Lresnonofpatall:
#endif
	lea	a1@(PCB_FPCTX),a0	| pointer to FP save area
#ifdef M68060
	cmpl	#FPU_68060,_C_LABEL(fputype) | is 68060?
	jeq	Lresfp60rest1           | yes, goto Lresfp60rest1
#endif /* M68060 */
	tstb	a0@			| null state frame?
	jeq	Lresfprest2		| yes, easy
	fmovem	a0@(FPF_FPCR),fpcr/fpsr/fpi	| restore FP control registers
	fmovem	a0@(FPF_REGS),fp0-fp7	| restore FP general registers
Lresfprest2:
	frestore a0@			| restore state
	movw	a1@(PCB_PS),sr		| no, restore PS
	moveq	#1,d0			| return 1 (for alternate returns)
	rts

#ifdef M68060
Lresfp60rest1:
	tstb	a0@(2)			| null state frame?
	jeq	Lresfp60rest2		| yes, easy
	fmovem	a0@(FPF_FPCR),fpcr	| restore FP control registers
	fmovem	a0@(FPF_FPSR),fpsr
	fmovem	a0@(FPF_FPI),fpi
	fmovem	a0@(FPF_REGS),fp0-fp7	| restore FP general registers
Lresfp60rest2:
	frestore a0@			| restore state
	movw	a1@(PCB_PS),sr		| no, restore PS
	moveq	#1,d0			| return 1 (for alternate returns)
	rts
#endif /* M68060 */


/*
 * savectx(pcb)
 * Update pcb, saving current processor state.
 */
ENTRY(savectx)
	movl	sp@(4),a1
	movw	sr,a1@(PCB_PS)
	movl	usp,a0			| grab USP
	movl	a0,a1@(PCB_USP)		| and save it
	moveml	#0xFCFC,a1@(PCB_REGS)	| save non-scratch registers
#ifdef FPU_EMULATE
	tstl	_C_LABEL(fputype)
	jeq	Lsavedone
#endif
	lea	a1@(PCB_FPCTX),a0	| pointer to FP save area
	fsave	a0@			| save FP state
#ifdef M68060
	cmpl	#FPU_68060,_C_LABEL(fputype) | is 68060?
	jeq	Lsavctx60               | yes, goto Lsavctx60
#endif
	tstb	a0@			| null state frame?
	jeq	Lsavedone		| yes, all done
	fmovem	fp0-fp7,a0@(FPF_REGS)	| save FP general registers
	fmovem	fpcr/fpsr/fpi,a0@(FPF_FPCR)	| save FP control registers
	moveq	#0,d0
	rts
#ifdef	M68060
Lsavctx60:
	tstb	a0@(2)
	jeq	Lsavedone
	fmovem	fp0-fp7,a0@(FPF_REGS)	| save FP general registers
	fmovem	fpcr,a0@(FPF_FPCR)	| save FP control registers
	fmovem	fpsr,a0@(FPF_FPSR)
	fmovem	fpi,a0@(FPF_FPI)
#endif
Lsavedone:
	moveq	#0,d0			| return 0
	rts

#if defined(M68040) || defined(M68060)
ENTRY(suline)
	movl	sp@(4),a0		| address to write
	movl	_C_LABEL(curpcb),a1	| current pcb
	movl	#Lslerr,a1@(PCB_ONFAULT) | where to return to on a fault
	movl	sp@(8),a1		| address of line
	movl	a1@+,d0			| get lword
	movsl	d0,a0@+			| put lword
	nop				| sync
	movl	a1@+,d0			| get lword
	movsl	d0,a0@+			| put lword
	nop				| sync
	movl	a1@+,d0			| get lword
	movsl	d0,a0@+			| put lword
	nop				| sync
	movl	a1@+,d0			| get lword
	movsl	d0,a0@+			| put lword
	nop				| sync
	moveq	#0,d0			| indicate no fault
	jra	Lsldone
Lslerr:
	moveq	#-1,d0
Lsldone:
	movl	_C_LABEL(curpcb),a1	| current pcb
	clrl	a1@(PCB_ONFAULT)	| clear fault address
	rts
#endif

/*
 * Invalidate entire TLB.
 */
ENTRY(TBIA)
_C_LABEL(_TBIA):
	cmpl	#MMU_68040,_C_LABEL(mmutype) | 68040 or 68060?
	jle	Ltbia040                | yes, goto Ltbia040
	pflusha				| flush entire TLB
	tstl	_C_LABEL(mmutype)
	jpl	Lmc68851a		| 68851 implies no d-cache
	movl	#DC_CLEAR,d0
	movc	d0,cacr			| invalidate on-chip d-cache
Lmc68851a:
	rts
Ltbia040:
	.word	0xf518			| pflusha
#ifdef M68060
	cmpl	#MMU_68060,_C_LABEL(mmutype) | is 68060?
	jne	Ltbiano60               | no, skip
	movc	cacr,d0
	orl	#IC60_CABC,d0		| and clear all branch cache entries
	movc	d0,cacr
#endif
Ltbiano60:
	rts


/*
 * Invalidate any TLB entry for given VA (TB Invalidate Single)
 */
ENTRY(TBIS)
#ifdef DEBUG
	tstl	_ASM_LABEL(fulltflush)	| being conservative?
	jne	_C_LABEL(_TBIA)		| yes, flush entire TLB
#endif
	movl	sp@(4),a0		| get addr to flush
	cmpl	#MMU_68040,_C_LABEL(mmutype) | 68040 or 68060 ?
	jle	Ltbis040                | yes, goto Ltbis040
	tstl	_C_LABEL(mmutype)
	jpl	Lmc68851b		| is 68851?
	pflush	#0,#0,a0@		| flush address from both sides
	movl	#DC_CLEAR,d0
	movc	d0,cacr			| invalidate on-chip data cache
	rts
Lmc68851b:
	pflushs	#0,#0,a0@		| flush address from both sides
	rts
Ltbis040:
	moveq	#5,d0		        | select supervisor
	movc	d0,dfc
	.word	0xf508			| pflush a0@
	moveq	#FC_USERD,d0		| select user
	movc	d0,dfc
	.word	0xf508			| pflush a0@
#ifdef M68060
	cmpl	#MMU_68060,_C_LABEL(mmutype) | is 68060?
	jne	Ltbisno60               | no, skip
	movc	cacr,d0
	orl	#IC60_CABC,d0		| and clear all branch cache entries
	movc	d0,cacr
Ltbisno60:
#endif
	rts

/*
 * Invalidate supervisor side of TLB
 */
ENTRY(TBIAS)
#ifdef DEBUG
	tstl	_ASM_LABEL(fulltflush)	| being conservative?
	jne	_C_LABEL(_TBIA)		| yes, flush everything
#endif
	cmpl	#MMU_68040,_C_LABEL(mmutype) | 68040 or 68060 ?
	jle	Ltbias040               | yes, goto Ltbias040
	tstl	_C_LABEL(mmutype)
	jpl	Lmc68851c		| 68851?
	pflush #4,#4			| flush supervisor TLB entries
	movl	#DC_CLEAR,d0
	movc	d0,cacr			| invalidate on-chip d-cache
	rts
Lmc68851c:
	pflushs #4,#4			| flush supervisor TLB entries
	rts
Ltbias040:
| 68040 cannot specify supervisor/user on pflusha, so we flush all
	.word	0xf518			| pflusha
#ifdef M68060
	cmpl	#MMU_68060,_C_LABEL(mmutype)
	jne	Ltbiasno60
	movc	cacr,d0
	orl	#IC60_CABC,d0		| and clear all branch cache entries
	movc	d0,cacr
Ltbiasno60:
#endif
	rts

/*
 * Invalidate user side of TLB
 */
ENTRY(TBIAU)
#ifdef DEBUG
	tstl	_ASM_LABEL(fulltflush)	| being conservative?
	jne	_C_LABEL(_TBIA)		| yes, flush everything
#endif
	cmpl	#MMU_68040,_C_LABEL(mmutype)
	jle	Ltbiau040
	tstl	_C_LABEL(mmutype)
	jpl	Lmc68851d		| 68851?
	pflush	#0,#4			| flush user TLB entries
	movl	#DC_CLEAR,d0
	movc	d0,cacr			| invalidate on-chip d-cache
	rts
Lmc68851d:
	pflushs	#0,#4			| flush user TLB entries
	rts
Ltbiau040:
| 68040 cannot specify supervisor/user on pflusha, so we flush all
	.word	0xf518			| pflusha
#ifdef M68060
	cmpl	#MMU_68060,_C_LABEL(mmutype)
	jne	Ltbiauno60
	movc	cacr,d0
	orl	#IC60_CUBC,d0		| but only user branch cache entries
	movc	d0,cacr
Ltbiauno60:
#endif
	rts

/*
 * Invalidate instruction cache
 */
ENTRY(ICIA)
#if defined(M68040) || defined(M68060)
ENTRY(ICPA)
	cmpl	#MMU_68040,_C_LABEL(mmutype) | 68040
	jgt	Lmotommu7		| no, skip
	.word	0xf498			| cinva ic
	rts
Lmotommu7:
#endif
	movl	#IC_CLEAR,d0
	movc	d0,cacr			| invalidate i-cache
	rts

/*
 * Invalidate data cache.
 * NOTE: we do not flush 68030 on-chip cache as there are no aliasing
 * problems with DC_WA.  The only cases we have to worry about are context
 * switch and TLB changes, both of which are handled "in-line" in resume
 * and TBI*.
 */
ENTRY(DCIA)
_C_LABEL(_DCIA):
#if defined(M68040) || defined(M68060)
	cmpl	#MMU_68040,_C_LABEL(mmutype) | 68040
	jgt	Lmotommu8		| no, skip
	.word	0xf478			| cpusha dc
	rts
Lmotommu8:
#endif
	rts

ENTRY(DCIS)
_C_LABEL(_DCIS):
#if defined(M68040) || defined(M68060)
	cmpl	#MMU_68040,_C_LABEL(mmutype) | 68040
	jgt	Lmotommu9		| no, skip
	.word	0xf478			| cpusha dc
	rts
Lmotommu9:
#endif
	rts

| Invalid single cache line
ENTRY(DCIAS)
_C_LABEL(_DCIAS):
	cmpl	#MMU_68040,_C_LABEL(mmutype) | 68040 or 68060
	jle	Ldciasx
	movl	sp@(4),a0
	.word	0xf468			| cpushl dc,a0@
Ldciasx:
	rts

ENTRY(DCIU)
_C_LABEL(_DCIU):
#if defined(M68040) || defined(M68060)
	cmpl	#MMU_68040,_C_LABEL(mmutype) | 68040
	jgt	LmotommuA		| no, skip
	.word	0xf478			| cpusha dc
	rts
LmotommuA:
#endif
	rts

#if defined(M68040) || defined(M68060)
ENTRY(ICPL)
	movl	sp@(4),a0		| address
	.word	0xf488			| cinvl ic,a0@
	rts
ENTRY(ICPP)
	movl	sp@(4),a0		| address
	.word	0xf490			| cinvp ic,a0@
	rts
ENTRY(DCPL)
	movl	sp@(4),a0		| address
	.word	0xf448			| cinvl dc,a0@
	rts
ENTRY(DCPP)
	movl	sp@(4),a0		| address
	.word	0xf450			| cinvp dc,a0@
	rts
ENTRY(DCPA)
	.word	0xf458			| cinva dc
	rts
ENTRY(DCFL)
	movl	sp@(4),a0		| address
	.word	0xf468			| cpushl dc,a0@
	rts
ENTRY(DCFP)
	movl	sp@(4),a0		| address
	.word	0xf470			| cpushp dc,a0@
	rts
#endif

ENTRY(PCIA)
#if defined(M68040) || defined(M68060)
ENTRY(DCFA)
	cmpl	#MMU_68040,_C_LABEL(mmutype) | 68040
	jgt	LmotommuB		| no, skip
	.word	0xf478			| cpusha dc
	rts
LmotommuB:
#endif
	movl	#DC_CLEAR,d0
	movc	d0,cacr			| invalidate on-chip d-cache
	rts

ENTRY(getsfc)
	movc	sfc,d0
	rts

ENTRY(getdfc)
	movc	dfc,d0
	rts

/*
 * Load a new user segment table pointer.
 */
ENTRY(loadustp)       /* XXX - smurph */
	movl	sp@(4),d0		| new USTP
	moveq	#PGSHIFT,d1
	lsll	d1,d0			| convert to addr
#ifdef M68060
	cmpl	#MMU_68060,_C_LABEL(mmutype) | 68040 or 68060?
	jeq	Lldustp060		| yes, goto Lldustp060
#endif
	cmpl	#MMU_68040,_C_LABEL(mmutype)
	jeq	Lldustp040
	pflusha				| flush entire TLB
	lea	_C_LABEL(protorp),a0	| CRP prototype
	movl	d0,a0@(4)		| stash USTP
	pmove	a0@,crp			| load root pointer
	movl	#CACHE_CLR,d0
	movc	d0,cacr			| invalidate cache(s)
	rts

#ifdef M68060
Lldustp060:
	movc	cacr,d1
	orl	#IC60_CUBC,d1		| clear user branch cache entries
	movc	d1,cacr
#endif
Lldustp040:
	.word	0xf518			| pflusha
	.long	0x4e7b0806		| movec d0,URP
	rts

ENTRY(ploadw)
	movl	sp@(4),a0		| address to load
	ploadw	#1,a0@			| pre-load translation
	rts

/*
 * Set processor priority level calls.  Most are implemented with
 * inline asm expansions.  However, spl0 requires special handling
 * as we need to check for our emulated software interrupts.
 */

ENTRY(spl0)
	moveq	#0,d0
	movw	sr,d0			| get old SR for return
	movw	#PSL_LOWIPL,sr		| restore new SR
	tstb	_C_LABEL(ssir)		| software interrupt pending?
	jeq	Lspldone		| no, all done
	subql	#4,sp			| make room for RTE frame
	movl	sp@(4),sp@(2)		| position return address
	clrw	sp@(6)			| set frame type 0
	movw	#PSL_LOWIPL,sp@		| and new SR
	jra	Lgotsir			| go handle it
Lspldone:
	rts

/*
 * Save and restore 68881 state.
 * Pretty awful looking since our assembler does not
 * recognize FP mnemonics.
 */
ENTRY(m68881_save)
	movl	sp@(4),a0		| save area pointer
	fsave	a0@			| save state
#ifdef M68060
	cmpl	#FPU_68060,_C_LABEL(fputype) | is 68060?
	jeq	Lm68060fpsave		| yes, goto Lm68060fpsave
#endif
	tstb	a0@			| null state frame?
	jeq	Lm68881sdone		| yes, all done
	fmovem fp0-fp7,a0@(FPF_REGS)	| save FP general registers
	fmovem fpcr/fpsr/fpi,a0@(FPF_FPCR)	| save FP control registers
Lm68881sdone:
	rts

#ifdef M68060
Lm68060fpsave:
	tstb	a0@(2)			| null state frame?
	jeq	Lm68060sdone		| yes, all done
	fmovem fp0-fp7,a0@(FPF_REGS)	| save FP general registers
	fmovem	fpcr,a0@(FPF_FPCR)	| save FP control registers
	fmovem	fpsr,a0@(FPF_FPSR)
	fmovem	fpi,a0@(FPF_FPI)
Lm68060sdone:
	rts
#endif

ENTRY(m68881_restore)
	movl	sp@(4),a0		| save area pointer
#ifdef M68060
	cmpl	#FPU_68060,_C_LABEL(fputype) | is 68060?
	jeq	Lm68060fprestore	| yes, goto Lm68060fprestore
#endif
	tstb	a0@			| null state frame?
	jeq	Lm68881rdone		| yes, easy
	fmovem	a0@(FPF_FPCR),fpcr/fpsr/fpi	| restore FP control registers
	fmovem	a0@(FPF_REGS),fp0-fp7	| restore FP general registers
Lm68881rdone:
	frestore a0@			| restore state
	rts

#ifdef M68060
Lm68060fprestore:
	tstb	a0@(2)			| null state frame?
	jeq	Lm68060fprdone		| yes, easy
	fmovem	a0@(FPF_FPCR),fpcr	| restore FP control registers
	fmovem	a0@(FPF_FPSR),fpsr
	fmovem	a0@(FPF_FPI),fpi
	fmovem	a0@(FPF_REGS),fp0-fp7	| restore FP general registers
Lm68060fprdone:
	frestore a0@			| restore state
	rts
#endif

/*
 * Handle the nitty-gritty of rebooting the machine.
 * Basically we just turn off the MMU and jump to the appropriate ROM routine.
 * XXX add support for rebooting -- that means looking at boothowto and doing
 * the right thing
 */
ENTRY_NOPROFILE(doboot)
	lea	_ASM_LABEL(tmpstk),sp	| physical SP in case of NMI
#if defined(M68040) || defined(M68060)
	cmpl	#MMU_68040,_C_LABEL(mmutype) | 68040?
	jgt	Lbootnot040		| no, skip
	movl	#0,d0
	movc	d0,cacr			| caches off
	.long	0x4e7b0003		| movc d0,tc (turn off MMU)
	bra	1f
Lbootnot040:
#endif
	movl	#CACHE_OFF,d0
	movc	d0,cacr			| disable on-chip cache(s)
	movl	#0,a7@			| value for pmove to TC (turn off MMU)
	pmove	a7@,tc			| disable MMU

1:	movl	#0,d0
	movc	d0,vbr			| ROM VBR

	/*
	 * We're going down. Make various sick attempts to reset the board.
	 */
	RELOC(cputyp, a0)
	movl	a0@,d0
	cmpw	#CPU_147,d0
	bne	not147
	movl	#0xfffe2000,a0		| MVME147: "struct vme1reg *"
	movw	a0@,d0
	movl	d0,d1
	andw	#0x0001,d1		| is VME1_SCON_SWITCH set?
	beq	1f			| not SCON. may not use SRESET.
	orw	#0x0002,d0		| ok, assert VME1_SCON_SRESET
	movw	d0,a0@
1:
	movl	#0xff800000,a0		| if we get here, SRESET did not work.
	movl	a0@(4),a0		| try jumping directly to the ROM.
	jsr	a0@
	| still alive! just return to the prom..
	bra	3f

not147:
	movl	#0xfff40000,a0		| MVME16x: "struct vme2reg *"
	movl	a0@(0x60),d0
	movl	d0,d1
	andl	#0x40000000,d1		| is VME2_TCTL_SCON set?
	beq	1f			| not SCON. may not use SRESET.
	orl	#0x00800000,d0		| ok, assert VME2_TCTL_SRST
	movl	d0,a0@(0x60)
1:
	| lets try the local bus reset
	movl	#0xfff40000,a0		| MVME16x: "struct vme2reg *"
	movl	a0@(0x104),d0
	orw	#0x00000080,d0
	movl	d0,a0@(0x104)
	| lets try jumping off to rom.
	movl	#0xff800000,a0		| if we get here, SRESET did not work.
	movl	a0@(4),a0		| try jumping directly to the ROM.
	jsr	a0@
	| still alive! just return to the prom..

3:	BUGCALL(MVMEPROM_EXIT)		| return to m68kbug
	/*NOTREACHED*/

#ifdef M68060
GLOBAL(intemu60)
	addql	#1,L60iem
	jra	_I_CALL_TOP+128+0x00
GLOBAL(fpiemu60)
	addql	#1,L60fpiem
	jra	_FP_CALL_TOP+128+0x30
GLOBAL(fpdemu60)
	addql	#1,L60fpdem
	jra	_FP_CALL_TOP+128+0x38
GLOBAL(fpeaemu60)
	addql	#1,L60fpeaem
	jra	_FP_CALL_TOP+128+0x40
#endif

	.data
GLOBAL(mmutype)
	.long	MMU_68030	| default to MMU_68030
GLOBAL(cputype)
	.long	CPU_68030	| default to CPU_68030
GLOBAL(fputype)
	.long	FPU_68881	| default to 68881 FPU
GLOBAL(protorp)
	.long	0,0		| prototype root pointer
GLOBAL(cold)
	.long	1		| cold start flag
GLOBAL(want_resched)
	.long	0
GLOBAL(proc0paddr)
	.long	0		| KVA of proc0 u-area
GLOBAL(intiobase)
	.long	0		| KVA of base of internal IO space
GLOBAL(intiolimit)
	.long	0		| KVA of end of internal IO space
GLOBAL(extiobase)
	.long	0		| KVA of base of external IO space

#ifdef DEBUG
ASGLOBAL(fulltflush)
	.long	0
ASGLOBAL(fullcflush)
	.long	0
#endif

/* interrupt counters */
GLOBAL(intrnames)
	.asciz	"spur"
	.asciz	"lev1"
	.asciz	"lev2"
	.asciz	"lev3"
	.asciz	"lev4"
	.asciz	"clock"
	.asciz	"lev6"
	.asciz	"nmi"
	.asciz	"statclock"
#ifdef M68060
	.asciz	"60intemu"
	.asciz	"60fpiemu"
	.asciz	"60fpdemu"
	.asciz	"60fpeaemu"
	.asciz	"60bpe"
#endif
#ifdef FPU_EMULATE
	.asciz	"fpe"
#endif
GLOBAL(eintrnames)
	.even

GLOBAL(intrcnt)
	.long	0,0,0,0,0,0,0,0,0,0
#ifdef M68060
L60iem:		.long	0
L60fpiem:	.long	0
L60fpdem:	.long	0
L60fpeaem:	.long	0
L60bpe:		.long	0
#endif
#ifdef FPU_EMULATE
Lfpecnt:	.long	0
#endif
GLOBAL(eintrcnt)

#include <mvme68k/mvme68k/vectors.s>
