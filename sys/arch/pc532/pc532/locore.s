/*	$NetBSD: locore.s,v 1.30 1995/11/30 00:59:00 jtc Exp $	*/

/*
 * Copyright (c) 1993 Philip A. Nelson.
 * All rights reserved.
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
 *	This product includes software developed by Philip A. Nelson.
 * 4. The name of Philip A. Nelson may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PHILIP NELSON ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL PHILIP NELSON BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	locore.s
 *
 *	locore.s,v 1.2 1993/09/13 07:26:47 phil Exp
 */

/*
 * locore.s  - -  assembler routines needed for BSD stuff.  (ns32532/pc532)
 *
 * Phil Nelson, Dec 6, 1992
 *
 */

/* This is locore.s! */
#define LOCORE

/* Get the defines... */
#include <machine/asm.h>
#include <machine/icu.h>
#include "assym.h"

/* define some labels */
#define PSR_U 0x100
#define PSR_S 0x200
#define PSR_P 0x400
#define PSR_I 0x800

#define CFG_IVEC 0x1
#define CFG_FPU	 0x2
#define CFG_MEM  0x4
#define CFG_DE	 0x100
#define CFG_DATA 0x200
#define CFG_DLCK 0x400
#define CFG_INS  0x800
#define CFG_ILCK 0x1000

/* Initial Kernel stack page and the Idle processes' stack. */
#define KERN_INT_SP    0xFFC00FFC	

/* Global Data */

.data
.globl _cold, __save_sp, __save_fp, __old_intbase
_cold:		.long 1
__save_sp: 	.long 0
__save_fp: 	.long 0
__old_intbase:	.long 0
__have_fpu:	.long 0

.text
.globl start
start:
	br here_we_go

	.align 4	/* So the trap table is double aligned. */
int_base_tab:		/* Here is the fixed jump table for traps! */
	.long __int
	.long __trap_nmi
	.long __trap_abt
	.long __trap_slave
	.long __trap_ill
	.long __trap_svc
	.long __trap_dvz
	.long __trap_flg
	.long __trap_bpt
	.long __trap_trc
	.long __trap_und
	.long __trap_rbe
	.long __trap_nbe
	.long __trap_ovf
	.long __trap_dbg
	.long __trap_reserved

here_we_go:	/* This is the actual start of the locore code! */

	bicpsrw	PSR_I			/* make sure interrupts are off. */
	bicpsrw	PSR_S			/* make sure we are using sp0. */
	lprd    sb, 0			/* gcc expects this. */
	sprd	sp, __save_sp(pc)  	/* save monitor's sp. */
	sprd	fp, __save_fp(pc)  	/* save monitor's fp. */
	sprd	intbase, __old_intbase(pc)  /* save monitor's intbase. */

.globl	_bootdev
.globl	_boothowto
	/* Save the registers loaded by the boot program ... if the kernel
		was loaded by the boot program. */
	cmpd	0xc1e86394, r3
	bne	zero_bss
	movd	r7, _boothowto(pc)
	movd	r6, _bootdev(pc)

zero_bss:	
	/* Zero the bss segment. */
	addr	_end(pc),r0	# setup to zero the bss segment.
	addr	_edata(pc),r1
	subd	r1,r0		# compute _end - _edata
	movd	r0,tos		# push length
	addr	_edata(pc),tos	# push address
	bsr	_bzero		# zero the bss segment

	bsr __low_level_init	/* Do the low level setup. */

	lprd	sp, KERN_INT_SP # use the idle/interrupt stack.
	lprd	fp, KERN_INT_SP # use the idle/interrupt stack.

	/* Load cfg register is bF6 (IC,DC,DE,M,F) or bF4 */
	sprd	cfg, r0
	tbitb	1, r0		/* Test the F bit! */
	bfc	cfg_no_fpu
	movqd	1, __have_fpu(pc)
	lprd	cfg, 0xbf6
	br	jmphi
	
cfg_no_fpu:
	lprd	cfg, 0xbf4

/* Now jump to high addresses after starting mapping! */

jmphi:	
	addr here(pc), r0
	ord  KERNBASE, r0
	jump 0(r0)

here:
	lprd	intbase, int_base_tab  /* set up the intbase.  */

	/* stack and frame pointer are pointing at high memory. */

	bsr 	_init532	/* Set thing up to call main()! */

	/* Get the proc0 kernel stack and pcb set up. */
	movd	KERN_STK_START, r1 	/* Standard sp start! */
	lprd	sp, r1		/* Load it! */
	lprd	fp, USRSTACK	/* fp for the user. */
	lprd	usp, USRSTACK	/* starting stack for the user. */

	/* Build the "trap" frame to return to address 0 in user space! */
	movw	PSR_I|PSR_S|PSR_U, tos	/* psr - user/user stack/interrupts */
	movw	0, tos			/* mod - 0! */
	movd	0, tos			/* pc  - 0 after module table */
	enter	[],8		/* Extra space is for USP */
	movqd	0, tos		/* Zero the registers in the pcb. */
	movqd	0, tos
	movqd	0, tos
	movqd	0, tos
	movqd	0, tos
	movqd	0, tos
	movqd	0, tos
	movqd	0, tos
	movqd	0, REGS_SB(sp)

	/* Now things should be ready to start _main! */

	addr	0(sp), tos
	bsr 	_main		/* Start the kernel! */
	movd	tos, r0		/* Pop addr */

	/* We should only get here in proc 1. */
	movd	_curproc(pc), r1
	cmpqd	0, r1
	beq	main_panic
	movd	P_PID(r1),r0
	cmpqd	1, r0
	bne	main_panic
	lprd	usp, REGS_USP(sp)
	lprd	sb, REGS_SB(sp)

	exit	[r0,r1,r2,r3,r4,r5,r6,r7]
	rett	0	

main_panic:
	addr	main_panic_str(pc), tos
	bsr	_panic

main_panic_str:
	.asciz	"After main -- no curproc or not proc 1."

/* Signal support */
.align 2
.globl _sigcode
.globl _esigcode
_sigcode:
	jsr	0(12(sp))
	movd	103, r0
	svc
.align 2
_esigcode:

/* To get the ptb0 register set correctly. */

ENTRY(_load_ptb0)
	movd	S_ARG0, r0
	andd	~KERNBASE, r0
	lmr 	ptb0, r0
	ret 0

ENTRY(_load_ptb1)
	movd	S_ARG0, r0
	andd	~KERNBASE, r0
	lmr 	ptb1, r0
	ret 0

ENTRY (_get_ptb0)
	smr ptb0, r0
	ret 0

ENTRY (tlbflush)
	smr ptb0, r0
	lmr ptb0, r0
	ret 0

ENTRY (_get_sp_adr)		/* for use in testing.... */
	addr 4(sp), r0
	ret 0

ENTRY (_get_ret_adr)
	movd 0(sp), r0
	ret 0

ENTRY (_get_fp_ret)
	movd 4(fp), r0
	ret 0

ENTRY (_get_2fp_ret)
	movd 4(0(fp)), r0
	ret 0

ENTRY (_get_fp)
	addr 0(fp), r0
	ret 0

/* reboot the machine :)  if possible */

ENTRY(low_level_reboot)

	movd	-1,tos
	bsr	_splx
	cmpqd	0,tos
	ints_off			/* Stop things! */
	addr	xxxlow(pc), r0		/* jump to low memory */
	andd	~KERNBASE, r0
	movd	r0, tos
	ret	0
xxxlow:
	lmr	mcr, 0 			/* Turn off mapping. */
	lprd	sp, __save_sp(pc)  	/* get monitor's sp. */
	jump	0x10000032		/* Jump to the ROM! */


/* To get back to the rom monitor .... */
ENTRY(bpt_to_monitor)

/* Switch to monitor's stack. */
	ints_off
	bicpsrw	PSR_S			/* make sure we are using sp0. */
	sprd	psr, tos		/* Push the current psl. */
	save	[r1,r2,r3,r4]
	sprd	sp, r1  		/* save kernel's sp */
	sprd	fp, r2  		/* save kernel's fp */
	sprd	intbase, r3		/* Save current intbase. */
	smr	ptb0, r4		/* Save current ptd! */	

/* Change to low addresses */
	lmr	ptb0, _IdlePTD(pc)	/* Load the idle ptd */
	addr	low(pc), r0
	andd	~KERNBASE, r0
	movd	r0, tos
	ret	0

low:
/* Turn off mapping. */
	smr	mcr, r0
	lmr	mcr, 0
	lprd	sp, __save_sp(pc)	/* restore monitors sp */
	lprd	fp, __save_fp(pc)	/* restore monitors fp */
	lprd	intbase, __old_intbase(pc)	/* restore monitors intbase */
	bpt

/* Reload kernel stack AND return. */
	lprd	intbase, r3		/* restore kernel's intbase */
	lprd	fp, r2			/* restore kernel's fp */
	lprd	sp, r1			/* restore kernel's sp */
	lmr	mcr, r0
	addr	highagain(pc), r0
	ord  	KERNBASE, r0
	jump 	0(r0)
highagain:
	lmr	ptb0, r4		/* Get the last ptd! */
	restore	[r1,r2,r3,r4]
	lprd	psr, tos		/* restore psl */
	ints_on
	ret 0


/*===========================================================================*
 *				ram_size				     *
 *===========================================================================*

 char *
 ram_size (start)
 char *start;

 Determines RAM size.

 First attempt: write-and-read-back (WRB) each page from start
 until WRB fails or get a parity error.  This didn't work because
 address decoding wraps around.

 New algorithm:

	ret = round-up-page (start);
  loop:
	if (!WRB or parity or wrap) return ret;
	ret += pagesz;  (* check end of RAM at powers of two *)
	goto loop;

 Several things make this tricky.  First, the value read from
 an address will be the same value written to the address if
 the cache is on -- regardless of whether RAM is located at
 the address.  Hence the cache must be disabled.  Second,
 reading an unpopulated RAM address is likely to produce a
 parity error.  Third, a value written to an unpopulated address
 can be held by capacitance on the bus and can be correctly
 read back if there is no intervening bus cycle.  Hence,
 read and write two patterns.

*/

cfg_dc		= 0x200
pagesz		= 0x1000
pattern0	= 0xa5a5a5a5
pattern1	= 0x5a5a5a5a
nmi_vec		= 0x44
parity_clr	= 0x28000050

/*
 r0	current page, return value
 r1	old config register
 r2	temp config register
 r3	pattern0	
 r4	pattern1
 r5	old nmi vector
 r6	save word at @0
 r7	save word at @4
*/
.globl _ram_size
_ram_size:
	enter	[r1,r2,r3,r4,r5,r6,r7],0
	# initialize things
	movd	@0,r6		#save 8 bytes of first page
	movd	@4,r7
	movd	0,@0		#zero 8 bytes of first page
	movd	0,@4
	sprw	cfg,r1		#turn off data cache
	movw	r1,r2		#r1 = old config
	andw	~cfg_dc,r2	# was: com cfg_dc,r2
	lprw	cfg,r2
	movd	@nmi_vec,r5	#save old NMI vector
	addr	tmp_nmi(pc),@nmi_vec	#tmp NMI vector
	movd	8(fp),r0	#r0 = start
	addr	pagesz-1(r0),r0	#round up to page
	andd	~(pagesz-1),r0	# was: com (pagesz-1),r0
	movd	pattern0,r3
	movd	pattern1,r4
rz_loop:
	movd	r3,0(r0)	#write 8 bytes
	movd	r4,4(r0)
	lprw	cfg,r2		#flush write buffer
	cmpd	r3,0(r0)	#read back and compare
	bne	rz_exit
	cmpd	r4,4(r0)
	bne	rz_exit
	cmpqd	0,@0		#check for address wrap
	bne	rz_exit
	cmpqd	0,@4		#check for address wrap
	bne	rz_exit
	addr	pagesz(r0),r0	#next page
	br	rz_loop
rz_exit:
	movd	r6,@0		#restore 8 bytes of first page
	movd	r7,@4
	lprd	cfg,r1		#turn data cache back on
	movd	r5,@nmi_vec	#restore NMI vector
	movd	parity_clr,r2
	movb	0(r2),r2	#clear parity status
	exit	[r1,r2,r3,r4,r5,r6,r7]
	ret	0

tmp_nmi:				#come here if parity error
	addr	rz_exit(pc),0(sp)	#modify return addr to exit
	rett	0

/* Low level kernel support routines. */

/* External symbols that are needed. */
/* .globl EX(cnt) */
.globl EX(curproc)
.globl EX(curpcb)
.globl EX(qs)
.globl EX(whichqs)
.globl EX(want_resched)
.globl EX(Cur_pl)

/*
   User/Kernel copy routines ... {fu,su}{word,byte} and copyin/coyinstr

   These are "Fetch User" or "Save user" word or byte.  They return -1 if
   a page fault occurs on access. 
*/

ENTRY(fuword)
ENTRY(fuiword)
	enter	[r2],0
	movd	_curpcb(pc), r2
	addr	fusufault(pc), PCB_ONFAULT(r2)
	movd	0(B_ARG0), r0
	br	fusu_ret

ENTRY(fubyte)
ENTRY(fuibyte)
	enter	[r2],0
	movd	_curpcb(pc), r2
	addr	fusufault(pc), PCB_ONFAULT(r2)
	movzbd	0(B_ARG0), r0
	br	fusu_ret

ENTRY(suword)
ENTRY(suiword)
	enter	[r2],0
	movqd	4, tos
	movd	B_ARG0, tos
	bsr	_check_user_write
	adjspd	-8
	cmpqd	0, r0
	bne	fusufault
	movd	_curpcb(pc), r2
	addr	fusufault(pc), PCB_ONFAULT(r2)
	movqd	0, r0
	movd	B_ARG1,0(B_ARG0)
	br	fusu_ret

ENTRY(subyte)
ENTRY(suibyte)
	enter	[r2],0
	movqd	1, tos
	movd	B_ARG0, tos
	bsr	_check_user_write
	adjspd	-8
	cmpqd	0, r0
	bne	fusufault
	movd	_curpcb(pc), r2
	addr	fusufault(pc), PCB_ONFAULT(r2)
	movqd	0, r0
	movb	B_ARG1, 0(B_ARG0)
	br	fusu_ret

fusufault:
	movqd	-1, r0
fusu_ret:
	movqd	0, PCB_ONFAULT(r2)
	exit	[r2]
	ret	0

/* Two more fu/su routines .... for now ... just return -1. */
ENTRY(fuswintr)
ENTRY(suswintr)
	movqd -1, r0
	ret	0

/* C prototype:  copyin ( int *usrc, int *kdst, u_int i)  
   C prototype:  copyout ( int *ksrc, int *udst, u_int i) 

   i is the number of Bytes! to copy! 

   Similar code.... 
 */

ENTRY(copyout)
	enter	[r2,r3],0
# Check for copying priviledges!  i.e. copy on write!
	movd	B_ARG2, tos	/* Length */
	movd	B_ARG1, tos	/* adr */
	bsr	_check_user_write
	adjspd	-8
	cmpqd	0, r0
	bne	cifault
	br	docopy

ENTRY(copyin)
	enter	[r2,r3],0
docopy:
	movd	_curpcb(pc), r3
	addr	cifault(pc), PCB_ONFAULT(r3)
	movd	B_ARG2, r0	/* Length! */
	movd	B_ARG0, r1	/* Src adr */
	movd	B_ARG1, r2	/* Dst adr */
	movsb			/* Move it! */
	movqd	0, r0
	movqd	0, PCB_ONFAULT(r3)
	exit	[r2,r3]
	ret	0

cifault:
	movd	EFAULT, r0
	movd	_curpcb(pc), r3
	movqd	0, PCB_ONFAULT(r3)
	exit	[r2,r3]
	ret	0


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
	.globl	_whichqs,_qs,_cnt,_panic

/*
 * setrunqueue(struct proc *p);
 * Insert a process on the appropriate queue.  Should be called at splclock().
 */
ENTRY(setrunqueue)
	movd	S_ARG0, r0
	movd	r2, tos

	cmpqd	0, P_BACK(r0)		/* should not be on q already */
	bne	1f
	cmpqd	0, P_WCHAN(r0)
	bne	1f
	cmpb	SRUN, P_STAT(r0)
	bne	1f

	movzbd	P_PRIORITY(r0),r1
	lshd	-2,r1
	sbitd	r1,_whichqs(pc)		/* set queue full bit */
	addr	_qs(pc)[r1:q], r1	/* locate q hdr */
	movd	P_BACK(r1),r2		/* locate q tail */
	movd	r1, P_FORW(r0)		/* set p->p_forw */
	movd	r0, P_BACK(r1)		/* update q's p_back */
	movd	r0, P_FORW(r2)		/* update tail's p_forw */
	movd    r2, P_BACK(r0)		/* set p->p_back */
	movd	tos, r2
	ret	0

1:	addr	2f(pc),tos		/* Was on the list! */
	bsr	_panic	
2:	.asciz "setrunqueue problem!"

/*
 * remrq(struct proc *p);
 * Remove a process from its queue.  Should be called at splclock().
 */
ENTRY(remrq)
	movd	S_ARG0, r1
	movd	r2, tos
	movzbd	P_PRIORITY(r1), r0

	lshd	-2, r0
	tbitd	r0, _whichqs(pc)
	bfc	1f

	movd	P_BACK(r1), r2		/* Address of prev. item */
	movqd	0, P_BACK(r1)		/* Clear reverse link */
	movd	P_FORW(r1), r1		/* Addr of next item. */
	movd	r1, P_FORW(r2)		/* Unlink item. */
	movd	r2, P_BACK(r1)
	cmpd	r1, r2			/* r1 = r2 => empty queue */
	bne	2f

	cbitd	r0, _whichqs(pc)	/* mark q as empty */

2:	movd	tos, r2
	ret	0

1:	addr	2f(pc),tos		/* No queue entry! */
	bsr	_panic	
2:	.asciz "remrq problem!"

/* Switch to another process from kernel code...  */

ENTRY(cpu_switch)
	ints_off	/* to make sure cpu_switch runs to completion. */
	enter	[r0,r1,r2,r3,r4,r5,r6,r7],0
/*	addqd	1, _cnt+V_SWTCH(pc) */

	movd	_curproc(pc), r0
	cmpqd	0, r0
	beq	sw1

	/* Save "kernel context" - - user context is saved at trap/svc.
	   Kernel registers are saved at entry to swtch. */

	movd	P_ADDR(r0), r0
	sprd	sp, PCB_KSP(r0)
	sprd	fp,  PCB_KFP(r0)
	smr	ptb0,  PCB_PTB(r0)

	/*  Save the Cur_pl.  */
	movd	_Cur_pl(pc), PCB_PL(r0)

	movqd	0, _curproc(pc)		/* no current proc! */

sw1:	/* Get something from a Queue! */
	ints_off	/* Just in case we came from Idle. */
	movqd	0, r0
	ffsd	_whichqs(pc), r0
	bfs	Idle

	/* Get the process and unlink it from the queue. */
	addr	_qs(pc)[r0:q], r1	/* address of qs entry! */
	movd	0(r1), r2		/* get process pointer! */
	movd	P_FORW(r2), r3		/* get address of next entry. */

  /* Test code */
  cmpqd	0, r3
  bne notzero
  bsr _dump_qs
notzero:

	/* unlink the entry. */
	movd	r3, 0(r1)		/* New head pointer. */
	movd	r1, P_BACK(r3) 		/* New reverse pointer. */
	cmpd	r1, r3			/* Empty? */
	bne	restart

	/* queue is empty, turn off whichqs. */
	cbitd	r0, _whichqs(pc)

restart:	/* r2 has pointer to new proc.. */

	/* Reload the new kernel context ... r2 points to proc entry. */
	movqd	0, P_BACK(r2)		/* NULL p_forw */
	movqd	0, _want_resched(pc)	/* We did a resched! */
	movd	P_ADDR(r2), r3		/* get new pcb pointer */


	/* Do we need to reload floating point here? */

	lmr	ptb0, PCB_PTB(r3)
	lprd	sp, PCB_KSP(r3)
	lprd	fp, PCB_KFP(r3)
	movw	PCB_FLAGS(r3), r4	/* Get the flags. */

	movd	r2, _curproc(pc)
	movd	r3, _curpcb(pc)

	/* Restore the previous processor level. */
	movd	PCB_PL(r3), tos
	bsr	_splx
	cmpqd	0,tos
	/* Return to the caller of swtch! */
	exit	[r0,r1,r2,r3,r4,r5,r6,r7]
	ret	0			

/*
 * The idle process!
 */
Idle:
	lprd	sp, KERN_INT_SP	/* Set up the "interrupt" stack. */
	movqd	0, r0
	ffsd	_whichqs(pc), r0
	bfc	sw1
	movd	_imask(pc),tos
	bsr	_splx
	cmpqd	0,tos
	wait			/* Wait for interrupt. */
	br	sw1

/* As part of the fork operation, we need to prepare a user are for 
   execution, to be resumed by swtch()...  

   C proto is low_level_fork (struct user *up)

   up is a pointer the the "user" struct in the child.
   We copy the kernel stack and  update the pcb of the child to
   return from low_level_fork twice.

   The first return should return a 0.  The "second" return should
   be because of a swtch() and should return a 1.

*/

ENTRY(low_level_fork)
	enter	[r0,r1,r2,r3,r4,r5,r6,r7],0

	/* Save "kernel context" - - user context is saved at trap/svc.
	   Kernel registers are saved at entry to swtch. */

	movd	B_ARG0, r2		/* Gets the paddr field of child. */
	sprd	sp, PCB_KSP(r2)
	sprd	fp,  PCB_KFP(r2)
	/* Don't save ptb0 because child has a different ptb0! */
	movd	_Cur_pl(pc), PCB_PL(r2)

	/* Copy the kernel stack from this process to new stack. */
	addr	0(sp), r1	/* Source address */
	movd	r1, r3		/* Calculate the destination address */
	subd	USRSTACK, r3	/* Get the offset */
	addd	r3, r2		/* r2 had B_ARG0 in it.  now the dest addr */
	movd	r2, r5		/* Save the destination address */
	movd	KSTK_SIZE, r0	/* Calculate the length of the kernel stack. */
	subd	r3, r0

	movd	r0, r4		/* Check for a double alligned stack. */
	andd	3, r4
	cmpqd	0, r4
	beq	kcopy
	addr	m_ll_fork(pc),tos  /* panic if not double alligned. */
	bsr	_panic

kcopy:
	lshd	-2,r0		/* Divide by 4 to get # of doubles. */
	movsd			/* Copy the stack! */

	/* Set parent to return 0. */
	movqd	0,28(sp)

	/* Set child to return 1. */
	movqd	1,28(r5)

	exit	[r0,r1,r2,r3,r4,r5,r6,r7]
	ret	0

m_ll_fork: .asciz "_low_level_fork: kstack not double alligned."

/*
 * savectx(struct pcb *pcb, int altreturn);
 * Update pcb, saving current processor state and arranging for alternate
 * return in cpu_switch() if altreturn is true.
 */
ENTRY(savectx)
	enter	[r0,r1,r2,r3,r4,r5,r6,r7],0
	movd	B_ARG0, r2
	sprd	sp,PCB_KSP(r2)
	sprd	fp,PCB_KFP(r2)
	movd	_Cur_pl(pc),PCB_PL(r2)
	exit	[r0,r1,r2,r3,r4,r5,r6,r7]
	ret 0

ENTRY(_trap_nmi)
	enter	[r0,r1,r2,r3,r4,r5,r6,r7],8
	sprd	usp, REGS_USP(sp)
	sprd	sb, REGS_SB(sp)
	movqd	1, tos
	br	all_trap

ENTRY(_trap_abt)
	enter	[r0,r1,r2,r3,r4,r5,r6,r7],8
	sprd	usp, REGS_USP(sp)
	sprd	sb, REGS_SB(sp)
	movqd	2, tos
	smr 	tear, tos
	smr	msr, tos
	br	abt_trap

ENTRY(_trap_slave)
	enter	[r0,r1,r2,r3,r4,r5,r6,r7],8
	sprd	usp, REGS_USP(sp)
	sprd	sb, REGS_SB(sp)
	movqd	3, tos
	br	all_trap

ENTRY(_trap_ill)
	enter	[r0,r1,r2,r3,r4,r5,r6,r7],8
	sprd	usp, REGS_USP(sp)
	sprd	sb, REGS_SB(sp)
	movqd	4, tos
	br	all_trap

ENTRY(_trap_svc)
	enter	[r0,r1,r2,r3,r4,r5,r6,r7],8
	sprd	usp, REGS_USP(sp)
	sprd	sb, REGS_SB(sp)
	lprd    sb, 0			/* for the kernel */

	/* Have an fpu? */
	cmpqd	0, __have_fpu(pc)
	beq	svc_no_fpu

	/* Save the FPU registers. */
	movd	_curpcb(pc), r3
	sfsr	PCB_FSR(r3)
	movl	f0,PCB_F0(r3)
	movl	f1,PCB_F1(r3)
	movl	f2,PCB_F2(r3)
	movl	f3,PCB_F3(r3)
	movl	f4,PCB_F4(r3)
	movl	f5,PCB_F5(r3)
	movl	f6,PCB_F6(r3)
	movl	f7,PCB_F7(r3)
	
	/* Call the system. */
	bsr	_syscall

	/* Restore the FPU registers. */
	movd	_curpcb(pc), r3
	lfsr	PCB_FSR(r3)
	movl	PCB_F0(r3),f0
	movl	PCB_F1(r3),f1
	movl	PCB_F2(r3),f2
	movl	PCB_F3(r3),f3
	movl	PCB_F4(r3),f4
	movl	PCB_F5(r3),f5
	movl	PCB_F6(r3),f6
	movl	PCB_F7(r3),f7

	/* Restore the usp and sb. */
	lprd	usp, REGS_USP(sp)
	lprd	sb, REGS_SB(sp)

	exit	[r0,r1,r2,r3,r4,r5,r6,r7]
	rett	0

svc_no_fpu:
	/* Call the system. */
	bsr	_syscall

	/* Restore the usp and sb. */
	lprd	usp, REGS_USP(sp)
	lprd	sb, REGS_SB(sp)

	exit	[r0,r1,r2,r3,r4,r5,r6,r7]
	rett	0

ENTRY(_trap_dvz)
	enter	[r0,r1,r2,r3,r4,r5,r6,r7],8
	sprd	usp, REGS_USP(sp)
	sprd	sb, REGS_SB(sp)
	movqd	6, tos
	br	all_trap

ENTRY(_trap_flg)
	cinv	ia, r0
	addqd	1, tos		/* Increment return address */
	rett	0

ENTRY(_trap_bpt)
	enter	[r0,r1,r2,r3,r4,r5,r6,r7],8
	sprd	usp, REGS_USP(sp)
	sprd	sb, REGS_SB(sp)
	movd	8, tos
	br	all_trap

ENTRY(_trap_trc)
	enter	[r0,r1,r2,r3,r4,r5,r6,r7],8
	sprd	usp, REGS_USP(sp)
	sprd	sb, REGS_SB(sp)
	movd	9, tos
	br	all_trap

ENTRY(_trap_und)
	enter	[r0,r1,r2,r3,r4,r5,r6,r7],8
	sprd	usp, REGS_USP(sp)
	sprd	sb, REGS_SB(sp)
	movd	10, tos
	br	all_trap

ENTRY(_trap_rbe)
	enter	[r0,r1,r2,r3,r4,r5,r6,r7],8
	sprd	usp, REGS_USP(sp)
	sprd	sb, REGS_SB(sp)
	movd	11, tos
	br	all_trap

ENTRY(_trap_nbe)
	enter	[r0,r1,r2,r3,r4,r5,r6,r7],8
	sprd	usp, REGS_USP(sp)
	sprd	sb, REGS_SB(sp)
	movd	12, tos
	br	all_trap

ENTRY(_trap_ovf)
	enter	[r0,r1,r2,r3,r4,r5,r6,r7],8
	sprd	usp, REGS_USP(sp)
	sprd	sb, REGS_SB(sp)
	movd	13, tos
	br	all_trap

ENTRY(_trap_dbg)
	enter	[r0,r1,r2,r3,r4,r5,r6,r7],8
	sprd	usp, REGS_USP(sp)
	sprd	sb, REGS_SB(sp)
	movd	14, tos
	br	all_trap

ENTRY(_trap_reserved)
	enter	[r0,r1,r2,r3,r4,r5,r6,r7],8
	sprd	usp, REGS_USP(sp)
	sprd	sb, REGS_SB(sp)
	movd	15, tos
all_trap:
	movqd	0,tos	/* Add 2 zeros for msr,tear in frame. */
	movqd	0,tos

abt_trap:
	lprd    sb, 0			/* for the kernel */

	/* Was this a real process? */
	cmpqd	0, _curproc(pc)
	beq	trap_no_fpu

	/* Have an fpu? */
	cmpqd	0, __have_fpu(pc)
	beq	trap_no_fpu

	/* Save the FPU registers. */
	movd	_curpcb(pc), r3		/* R3 is saved by gcc. */
	sfsr	PCB_FSR(r3)
	movl	f0,PCB_F0(r3)
	movl	f1,PCB_F1(r3)
	movl	f2,PCB_F2(r3)
	movl	f3,PCB_F3(r3)
	movl	f4,PCB_F4(r3)
	movl	f5,PCB_F5(r3)
	movl	f6,PCB_F6(r3)
	movl	f7,PCB_F7(r3)
	
	bsr _trap
	adjspd	-12	/* Pop off software part of trap frame. */

	/* Restore the FPU registers. */
	lfsr	PCB_FSR(r3)
	movl	PCB_F0(r3),f0
	movl	PCB_F1(r3),f1
	movl	PCB_F2(r3),f2
	movl	PCB_F3(r3),f3
	movl	PCB_F4(r3),f4
	movl	PCB_F5(r3),f5
	movl	PCB_F6(r3),f6
	movl	PCB_F7(r3),f7

	/* Reload the usp and sb just in case anything has changed. */
	lprd	usp, REGS_USP(sp)
	lprd	sb, REGS_SB(sp)

	exit	[r0,r1,r2,r3,r4,r5,r6,r7]
	rett  0

trap_no_fpu:
	bsr _trap
	adjspd	-12	/* Pop off software part of trap frame. */

	/* Reload the usp and sb just in case anything has changed. */
	lprd	usp, REGS_USP(sp)
	lprd	sb, REGS_SB(sp)

	exit	[r0,r1,r2,r3,r4,r5,r6,r7]
	rett  0

/* Interrupt service routines.... */
ENTRY(_int)
	enter	[r0,r1,r2,r3,r4,r5,r6,r7],8
	sprd	usp,REGS_USP(sp)
	sprd	sb,REGS_SB(sp)
	lprd    sb,0			/* for the kernel */
	movd	_Cur_pl(pc), tos
	movb	@ICU_ADR+HVCT,r0	/* fetch vector */
	andd	0x0f,r0
	movd	r0,tos
	movqd	1,r1
	lshd	r0,r1
	orw	r1,_Cur_pl(pc)		/* or bit to Cur_pl */
	orw	r1,@ICU_ADR+IMSK	/* and to IMSK */
					/* bits set by idisabled in IMSK */
					/* have to be preserved */
	ints_off			/* flush pending writes */
	ints_on				/* and now turn ints on */
	addqd	1,_intrcnt(pc)[r0:d]
	lshd	4,r0
	addqd	1,_cnt+V_INTR(pc)
	addqd	1,_ivt+IV_CNT(r0)	/* increment counters */
	movd	_ivt+IV_ARG(r0),r1	/* get argument */
	cmpqd	0,r1
	bne	1f
	addr	0(sp),r1		/* NULL -> push frame address */
1:	movd	r1,tos
	movd	_ivt+IV_VEC(r0),r0	/* call the handler */
	jsr	0(r0)

	adjspd	-8			/* Remove arg and vec from stack */
	bsr	_splx_di		/* Restore Cur_pl */
	cmpqd	0,tos

	tbitw	8, REGS_PSR(sp)		/* In system mode? */
	bfs	do_user_intr		/* branch if yes! */

	lprd	usp, REGS_USP(sp)
	lprd	sb, REGS_SB(sp)
	exit	[r0,r1,r2,r3,r4,r5,r6,r7]
	rett	0

do_user_intr:
	/* Do "user" mode interrupt processing, including preemption. */
	ints_off
	movd	_curproc(pc), r2
	cmpqd	0,r2
	beq	intr_panic

	/* Have an fpu? */
	cmpqd	0, __have_fpu(pc)
	beq	intr_no_fpu

	/* Save the FPU registers. */
	movd	_curpcb(pc), r3		/* R3 is saved by gcc. */
	sfsr	PCB_FSR(r3)
	movl	f0,PCB_F0(r3)
	movl	f1,PCB_F1(r3)
	movl	f2,PCB_F2(r3)
	movl	f3,PCB_F3(r3)
	movl	f4,PCB_F4(r3)
	movl	f5,PCB_F5(r3)
	movl	f6,PCB_F6(r3)
	movl	f7,PCB_F7(r3)

intr_no_fpu:
	/* turn on interrupts! */
	ints_on

	cmpqd	0, _want_resched(pc)
	beq	do_usr_ret
	movd	18, tos
	movqd	0,tos
	movqd	0,tos
	bsr _trap
	adjspd	-12	/* Pop off software part of trap frame. */

do_usr_ret:

	/* Have an fpu? */
	cmpqd	0, __have_fpu(pc)
	beq	intr_ret_no_fpu

	/* Restore the FPU registers.  r3 should be as set before. */
	lfsr	PCB_FSR(r3)
	movl	PCB_F0(r3),f0
	movl	PCB_F1(r3),f1
	movl	PCB_F2(r3),f2
	movl	PCB_F3(r3),f3
	movl	PCB_F4(r3),f4
	movl	PCB_F5(r3),f5
	movl	PCB_F6(r3),f6
	movl	PCB_F7(r3),f7

intr_ret_no_fpu:
	lprd	usp, REGS_USP(sp)
	lprd	sb, REGS_SB(sp)
	exit	[r0,r1,r2,r3,r4,r5,r6,r7]
	rett	0

intr_panic:
	addr	intr_panic_msg(pc),tos  /* panic if not double alligned. */
	bsr	_panic

intr_panic_msg:
	.asciz "user mode interrupt with no current process!"

/* Include all other .s files. */
#include "bcopy.s"
#include "bzero.s"


/* pmap support??? ..... */

/*
 * Note: This version greatly munged to avoid various assembler errors
 * that may be fixed in newer versions of gas. Perhaps newer versions
 * will have more pleasant appearance.
 */

	.set	IDXSHIFT,10
	.set	SYSTEM,0xFE000000	# virtual address of system start
	/*note: gas copys sign bit (e.g. arithmetic >>), can't do SYSTEM>>22! */
	.set	SYSPDROFF,0x3F8		# Page dir index of System Base

/*
 * PTmap is recursive pagemap at top of virtual address space.
 * Within PTmap, the page directory can be found (third indirection).
 */
#define PDRPDROFF	0x03F7	/* page dir index of page dir */
	.globl	_PTmap, _PTD, _PTDpde, _Sysmap
	.set	_PTmap,0xFDC00000
	.set	_PTD,0xFDFF7000
	.set	_Sysmap,0xFDFF8000
	.set	_PTDpde,0xFDFF7000+4*PDRPDROFF

/*
 * APTmap, APTD is the alternate recursive pagemap.
 * It's used when modifying another process's page tables.
 */
#define APDRPDROFF	0x03FE	/* page dir index of page dir */
	.globl	_APTmap, _APTD, _APTDpde
	.set	_APTmap,0xFF800000
	.set	_APTD,0xFFBFE000
	.set	_APTDpde,0xFDFF7000+4*APDRPDROFF

/*
 * Access to each processes kernel stack is via a region of
 * per-process address space (at the beginning), immediatly above
 * the user process stack.
 */
#if 0
	.set	_kstack, USRSTACK
	.globl	_kstack
#endif
	.set	PPDROFF,0x3F6
/* #	.set	PPTEOFF,0x400-UPAGES	# 0x3FE */
	.set	PPTEOFF,0x3FE

.data
.globl _PDRPDROFF
_PDRPDROFF:
	.long PDRPDROFF

/* vmstat -i uses the following labels and __int even increments the
 * counters. This information is also availiable from ivt[n].iv_use 
 * and ivt[n].iv_cnt in much better form.
 */
	.globl	_intrnames, _eintrnames, _intrcnt, _eintrcnt
_intrnames:
	.asciz "int  0"
	.asciz "int  1"
	.asciz "int  2"
	.asciz "int  3"
	.asciz "int  4"
	.asciz "int  5"
	.asciz "int  6"
	.asciz "int  7"
	.asciz "int  8"
	.asciz "int  9"
	.asciz "int 10"
	.asciz "int 11"
	.asciz "int 12"
	.asciz "int 13"
	.asciz "int 14"
	.asciz "int 15"
_eintrnames:
_intrcnt:
	.long	0
	.long	0
	.long	0
	.long	0
	.long	0
	.long	0
	.long	0
	.long	0
	.long	0
	.long	0
	.long	0
	.long	0
	.long	0
	.long	0
	.long	0
	.long	0
_eintrcnt:
