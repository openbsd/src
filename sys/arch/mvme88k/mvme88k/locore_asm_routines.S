/*
 * Mach Operating System
 * Copyright (c) 1993-1992 Carnegie Mellon University
 * Copyright (c) 1991 OMRON Corporation
 * Copyright (c) 1996 Nivas Madhur
 * Copyright (c) 1998 Steve Murphree, Jr.
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON AND OMRON ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON AND OMRON DISCLAIM ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */
/* locore_asm_routines.c 
 *
 **********************************************************************
 * This file created by Omron Corporation, 1990.
 *
 * HISTORY
 *
 **************************************************************RCS*****/

#ifndef ASSEMBLER
# define ASSEMBLER
#endif

#include "assym.s"
#include <machine/trap.h>
#include <machine/board.h>
#include <machine/asm.h>
#include <sys/errno.h>


/*****************************************************************************
 * DO_LOAD_ADDRESS
 *
 * 	unsigned int do_load_word(address, supervisor_mode)
 *	vm_offset_t address;		\\ in r2
 *	boolean_t supervisor_mode;	\\ in r3
 *
 * Return the word at ADDRESS (from user space if SUPERVISOR_MODE is zero,
 * supervisor space if non-zero).
 *
 */

ENTRY(do_load_word)	/* do_load_word(address, supervisor) */
	bcnd	ne0,r3,1f
#if  ERRATA__XXX_USR
	NOP
	ld.usr	r2,r2,r0
	NOP
	NOP
	NOP
#else
	ld.usr	r2,r2,r0
#endif
	br	2f
1:	ld	r2,r2,r0
2:	jmp	r1

ENTRY(do_load_half)	/* do_load_half(address, supervisor) */
	bcnd	ne0,r3,1f
#if  ERRATA__XXX_USR
	NOP
	ld.h.usr	r2,r2,r0
	NOP
	NOP
	NOP
#else
	ld.h.usr	r2,r2,r0
#endif
	br	2f
1:	ld.h	r2,r2,r0
2:	jmp	r1

ENTRY(do_load_byte)	/* do_load_byte(address, supervisor) */
	bcnd	ne0,r3,1f
#if  ERRATA__XXX_USR
	NOP
	ld.b.usr	r2,r2,r0
	NOP
	NOP
	NOP
#else
	ld.b.usr	r2,r2,r0
#endif
	br	2f
1:	ld.b	r2,r2,r0
2:	jmp	r1

ENTRY(do_store_word)	/* do_store_word(address, data, supervisor) */
	bcnd	ne0,r4,1f
#if  ERRATA__XXX_USR
	NOP
	st.usr	r3,r2,r0
	NOP
	NOP
	NOP
#else
	st.usr	r3,r2,r0
#endif
	br	2f
1:	st	r3,r2,r0
2:	jmp	r1

ENTRY(do_store_half)	/* do_store_half(address, data, supervisor) */
	bcnd	ne0,r4,1f
#if  ERRATA__XXX_USR
	NOP
	st.h.usr	r3,r2,r0
	NOP
	NOP
	NOP
#else
	st.h.usr	r3,r2,r0
#endif
	br	2f
1:	st.h	r3,r2,r0
2:	jmp	r1

ENTRY(do_store_byte)	/* do_store_byte(address, data, supervisor) */
	bcnd	ne0,r4,1f
#if  ERRATA__XXX_USR
	NOP
	st.b.usr	r3,r2,r0
	NOP
	NOP
	NOP
#else
	st.b.usr	r3,r2,r0
#endif
	br	2f
1:	st.b	r3,r2,r0
2:	jmp	r1

ENTRY(do_xmem_word)	/* do_xmem_word(address, data, supervisor) */
	bcnd	ne0,r4,1f
#if  ERRATA__XXX_USR
	NOP
	xmem.usr	r3,r2,r0
	NOP
	NOP
	NOP
#else
	xmem.usr	r3,r2,r0
#endif
	br	2f
1:	xmem	r3,r2,r0
2:	jmp	r1

ENTRY(do_xmem_byte)	/* do_xmem_byte(address, data, supervisor) */
	bcnd	ne0,r4,1f
#if  ERRATA__XXX_USR
	NOP
	xmem.bu.usr	r3,r2,r0
	NOP
	NOP
	NOP
#else
	xmem.bu.usr	r3,r2,r0
#endif
	br	2f
1:	xmem.bu	r3,r2,r0
2:	jmp	r1

/*************************************************************************
 *************************************************************************
 **
 **	void enable_interrupt(void)
 **
 **	Enables processor interrupts (for the executing cpu).
 **/
#undef enable_interrupt
ENTRY(enable_interrupt)
	ldcr	r2, PSR
	clr	r2, r2, 1<PSR_INTERRUPT_DISABLE_BIT>
	stcr	r2, PSR
        FLUSH_PIPELINE
	jmp	r1

#if DDB
/* a version of enable_interrupt for the debugger; should never
   have breakpoints set it in. Keep it consistent with enable
   interrupt above */
ENTRY(db_enable_interrupt)
	ldcr	r2, PSR
	clr	r2, r2, 1<PSR_INTERRUPT_DISABLE_BIT>
	stcr	r2, PSR
        FLUSH_PIPELINE
	jmp	r1
#endif /* DDB */

/*************************************************************************
 *************************************************************************
 **
 ** unsigned long disable_interrupt(void)
 **
 ** Disables processor interrupts (for the executing CPU) and returns
 ** the *previous* PSR.
 **
 **     if ((oldPSR & 0x02) == 0)
 **		interrupts_were_previously_on = 1;
 **/
#undef disable_interrupt
ENTRY(disable_interrupt)
	ldcr	r2, PSR
	set	r3, r2, 1<PSR_INTERRUPT_DISABLE_BIT>	 /* set disable bit*/
	stcr	r3, PSR
        FLUSH_PIPELINE
	jmp	r1

/* a version of disable_interrupt for the kernel debugger. Should never
   have breakpoints set in it. Make sure it stays consistent with 
   disable_interrupt */

#if DDB
ENTRY(db_disable_interrupt)
	ldcr	r2, PSR
	set	r3, r2, 1<PSR_INTERRUPT_DISABLE_BIT>	 /* set disable bit*/
	stcr	r3, PSR
        FLUSH_PIPELINE
	jmp	r1
#endif /* DDB */

/* version for the debugger */

#if DDB

ENTRY(db_are_interrupts_disabled)
     ldcr	r2, PSR                   /* get the processor status word */
     set  	r3, r0, 1<PSR_INTERRUPT_DISABLE_BIT>   /* set mask */
     jmp.n 	r1                                     /* delayed return */
     and 	r2, r2, r3                            /* r2 = r3 & r2 */
#endif /* DDB */

LABEL(_FAULT_ERROR)
	or	r2,r0,1		/* bad copy */
	jmp	r1

/* LABEL(_ALLOW_FAULT_START) */

/*
 * Fetch from user space
 * r2 == address in user space
 */

ENTRY(fuword)
ENTRY(fuiword)
	or.u		r5,   r0,   hi16(_curpcb)	
	ld		r6,   r5,   lo16(_curpcb)
	or.u		r5,   r0,   hi16(fusu_fault)
	or		r5,   r5,   lo16(fusu_fault)
	st		r5,   r6,   PCB_ONFAULT	/* pcb_onfault = fusu_fault */
#if  ERRATA__XXX_USR
	NOP
	ld.usr   	r5,   r0,  r2
	NOP
	NOP
	NOP
#else
	ld.usr   	r5,   r0,  r2
#endif
	or		r2,   r0,  r5
	br		fusu_ret
fusu_fault:
	subu		r2,   r0,  1	
fusu_ret:
	or.u		r5,   r0,   hi16(_curpcb)	
	ld		r6,   r5,   lo16(_curpcb)
	st		r0,   r6,   PCB_ONFAULT	/* pcb_onfault = 0 */

	jmp		r1
		
ENTRY(fusword)
	or.u		r5,   r0,   hi16(_curpcb)	
	ld		r6,   r5,   lo16(_curpcb)
	or.u		r5,   r0,   hi16(fusu_fault)
	or		r5,   r5,   lo16(fusu_fault)
	st		r5,   r6,   PCB_ONFAULT	/* pcb_onfault = fusu_fault */
#if  ERRATA__XXX_USR
	NOP
	ld.h.usr   	r5,   r0,  r2
	NOP
	NOP
	NOP
#else
	ld.h.usr   	r5,   r0,  r2
#endif
	or		r2,   r0,  r5
	br		fusu_ret
	
ENTRY(fubyte)
ENTRY(fuibyte)
	or.u		r5,   r0,   hi16(_curpcb)	
	ld		r6,   r5,   lo16(_curpcb)
	or.u		r5,   r0,   hi16(fusu_fault)
	or		r5,   r5,   lo16(fusu_fault)
	st		r5,   r6,   PCB_ONFAULT	/* pcb_onfault = fusu_fault */
#if  ERRATA__XXX_USR
	NOP
	ld.b.usr   	r5,   r0,  r2
	NOP
	NOP
	NOP
#else
	ld.b.usr   	r5,   r0,  r2
#endif
	or		r2,   r0,  r5
	br		fusu_ret

ENTRY(fuswintr)
	or.u		r5,   r0,   hi16(_curpcb)	
	ld		r6,   r5,   lo16(_curpcb)
	or.u		r5,   r0,   hi16(_fubail)
	or		r5,   r5,   lo16(_fubail)
	st		r5,   r6,   PCB_ONFAULT	/* pcb_onfault = fubail */
#if  ERRATA__XXX_USR
	NOP
	ld.h.usr	r5,   r2, r0
	NOP
	NOP
	NOP
#else
	ld.h.usr	r5,   r2, r0
#endif
	or		r2,   r0, r5
	br		fusu_ret

ENTRY(fubail)
	subu		r2,   r0, 1
	br		fusu_ret

/*
 * store to user space.
 * r2 == address in user space
 * r3 == byte/short/word
 */

ENTRY(suword)
ENTRY(suiword)
	or.u		r5,   r0,   hi16(_curpcb)	
	ld		r6,   r5,   lo16(_curpcb)
	or.u		r5,   r0,   hi16(fusu_fault)
	or		r5,   r5,   lo16(fusu_fault)
	st		r5,   r6,   PCB_ONFAULT	/* pcb_onfault = fusu_fault */
#if  ERRATA__XXX_USR
	NOP
	st.usr		r3,   r2, r0
	NOP
	NOP
	NOP
#else
	st.usr		r3,   r2, r0
#endif
	or		r2,   r0, r0		/* return success */
	br		fusu_ret

ENTRY(susword)
	or.u		r5,   r0,   hi16(_curpcb)	
	ld		r6,   r5,   lo16(_curpcb)
	or.u		r5,   r0,   hi16(fusu_fault)
	or		r5,   r5,   lo16(fusu_fault)
	st		r5,   r6,   PCB_ONFAULT	/* pcb_onfault = fusu_fault */
#if  ERRATA__XXX_USR
	NOP
	st.h.usr	r3,   r2, r0
	NOP
	NOP
	NOP
#else
	st.h.usr	r3,   r2, r0
#endif
	or		r2,   r0, r0		/* return success */
	br		fusu_ret

ENTRY(subyte)
ENTRY(suibyte)
	or.u		r5,   r0,   hi16(_curpcb)	
	ld		r6,   r5,   lo16(_curpcb)
	or.u		r5,   r0,   hi16(fusu_fault)
	or		r5,   r5,   lo16(fusu_fault)
	st		r5,   r6,   PCB_ONFAULT	/* pcb_onfault = fusu_fault */
#if  ERRATA__XXX_USR
	NOP
	st.b.usr	r3,   r2, r0
	NOP
	NOP
	NOP
#else
	st.b.usr	r3,   r2, r0
#endif
	or		r2,   r0, r0		/* return success */
	br		fusu_ret

ENTRY(suswintr)
	or.u		r5,   r0,   hi16(_curpcb)	
	ld		r6,   r5,   lo16(_curpcb)
	or.u		r5,   r0,   hi16(_subail)
	or		r5,   r5,   lo16(_subail)
	st		r5,   r6,   PCB_ONFAULT	/* pcb_onfault = subail */
#if  ERRATA__XXX_USR
	NOP
	st.h.usr	r3,   r2, r0
	NOP
	NOP
	NOP
#else
	st.h.usr	r3,   r2, r0
#endif
	or		r2,   r0, r0		/* return success */
	br		fusu_ret

ENTRY(subail)
	subu		r2,   r0, 1
	br		fusu_ret

#if 0
/*
 * copystr(fromaddr, toaddr, maxlength, &lencopied)
 *
 * Copy a null terminated string from one point to another in
 * the kernel address space.
 */
ENTRY(copystr)
	or	r6,r0,0
	bcnd	lt0,r4,Lcsflt1		/* negative count, error */
	bcnd	eq0,r4,Lcsdone		/* zero count, all done */
Lcsloop:
	ld	r8,r2,r6		/* copy a byte */
	st	r8,r3,r6		
	addu	r6,r6,1			/* bump the index */
	bcnd	eq0,r8,Lcsdone 		/* if null, done */
	subu	r4,r4,1			/* decrement count to copy */
	bcnd	ne0,r4,Lcsloop		/* if more to copy, loop */
	br	Lcsflt2 		/* ran out of room, error */
Lcsdone:
	bcnd	eq0, r5, Lcsret		/* if return len not desired, return */
	st	r6,r5,0			/* stash it */
Lcsret:
	or	r2,r0,0			/* good status */
	jmp	r1
Lcsflt1:
	or	r2,r0,EFAULT		/* return fault */
	br	Lcsdone
Lcsflt2:
	or	r2,r0,ENAMETOOLONG	/* ran out of space */
	br	Lcsdone	
Lcsdone:
	jmp	r1
#endif /* 0 */
/*
 * Copy specified amount of data from user space into the kernel
 * copyin(from, to, len)
 *	r2 == from (user source address)
 *	r3 == to (kernel destination address)
 *	r4 == length
 * (r1=return addr)
 */

#define SRC  	r2
#define DEST  	r3
#define LEN  	r4

ENTRY(copyin)
	/* set up fault handler */
	or.u		r5,   r0,   hi16(_curpcb)	
	ld		r6,   r5,   lo16(_curpcb)
	or.u		r5,   r0,   hi16(.Lciflt)
	or		r5,   r5,   lo16(.Lciflt)
	st		r5,   r6,   PCB_ONFAULT	/* pcb_onfault = .Lciflt */
	
	/*bcnd		ne0,  LEN,  1f ; XXX optimize len = 0 case */
	/*;or		r2,   r0,   0 */
	/*;br		.Lcidone */
    /*;1: ;bcnd		lt0,  LEN,  .Lciflt ; EFAULT if len < 0  */
        
	/* If it's a small length (less than 8), then do byte-by-byte */
	cmp		r9,   LEN,  8
	bb1		lt,   r9,   copyin_byte_only

	/* If they're not aligned similiarly, use byte only... */
	xor		r9,   SRC,  DEST
	mask		r8,   r9,   0x3
	bcnd		ne0,  r8,   copyin_byte_only

	/*
	 * At this point, we don't know if they're word aligned or not,
	 * but we know that what needs to be done to one to align
	 * it is what's needed for the other.
	 */
	bb1		0,    SRC,  copyin_left_align_to_halfword
copyin_left_aligned_to_halfword:
	bb1		1,    SRC,  copyin_left_align_to_word
copyin_left_aligned_to_word:
	bb1		0,    LEN,  copyin_right_align_to_halfword
copyin_right_aligned_to_halfword:
	bb1		1,    LEN,  copyin_right_align_to_word
copyin_right_aligned_to_word:

	/* At this point, both SRC and DEST are aligned to a word */
	/* boundry, and LEN is an even multiple of 4.             */
	bb1.n		2,    LEN,  copyin_right_align_to_doubleword
	or		r7,   r0,   4

copyin_right_aligned_to_doubleword:
#if  ERRATA__XXX_USR
	NOP
	ld.usr 		r5,   SRC,  r0
	NOP
	NOP
	NOP
	ld.usr    	r6,   SRC,  r7
	NOP
	NOP
	NOP
#else
	ld.usr 		r5,   SRC,  r0
	ld.usr    	r6,   SRC,  r7
#endif
	subu		LEN,  LEN,  8
	st		r5,   DEST, r0
	addu		SRC,  SRC,  8
	st		r6,   DEST, r7
	bcnd.n		ne0,  LEN,  copyin_right_aligned_to_doubleword
	addu		DEST, DEST, 8
   	or		r2, r0, r0	/* successful return */
	br		.Lcidone

        /***************************************************/

copyin_left_align_to_halfword:
#if  ERRATA__XXX_USR
	NOP
	ld.b.usr   	r5,   SRC,  r0
	NOP
	NOP
	NOP
#else
	ld.b.usr   	r5,   SRC,  r0
#endif
	subu		LEN,  LEN,  1
	st.b		r5,   DEST, r0
	addu		SRC,  SRC,  1
	br.n		copyin_left_aligned_to_halfword
	addu		DEST, DEST, 1

copyin_left_align_to_word:
#if  ERRATA__XXX_USR
	NOP
	ld.h.usr   	r5,   SRC,  r0
	NOP
	NOP
	NOP
#else
	ld.h.usr   	r5,   SRC,  r0
#endif
	subu      	LEN,  LEN,  2
	st.h		r5,   DEST, r0
	addu		SRC,  SRC,  2
	br.n		copyin_left_aligned_to_word
	addu		DEST, DEST, 2

copyin_right_align_to_halfword:
	subu		LEN,  LEN,  1
#if  ERRATA__XXX_USR
	NOP
	ld.b.usr   	r5,   SRC,  LEN
	NOP
	NOP
	NOP
#else
	ld.b.usr   	r5,   SRC,  LEN
#endif
	br.n		copyin_right_aligned_to_halfword
	st.b		r5,   DEST, LEN

copyin_right_align_to_word:
	subu		LEN,  LEN,  2
#if  ERRATA__XXX_USR
	NOP
	ld.h.usr   	r5,   SRC,  LEN
	NOP
	NOP
	NOP
#else
	ld.h.usr   	r5,   SRC,  LEN
#endif
	br.n		copyin_right_aligned_to_word
	st.h		r5,   DEST, LEN

copyin_right_align_to_doubleword:
	subu		LEN,  LEN,  4
#if  ERRATA__XXX_USR
	NOP
	ld.usr		r5,   SRC,  LEN
	NOP
	NOP
	NOP
#else
	ld.usr		r5,   SRC,  LEN
#endif
	bcnd.n		ne0,  LEN, copyin_right_aligned_to_doubleword
	st		r5,   DEST, LEN
   	or		r2, r0, r0	/* successful return */
	br		.Lcidone

copyin_byte_only:
	bcnd		eq0, LEN, 2f
   1:
	subu		LEN, LEN, 1
#if  ERRATA__XXX_USR
	NOP
	ld.b.usr   	r5, SRC, LEN
	NOP
	NOP
	NOP
#else
	ld.b.usr   	r5, SRC, LEN
#endif
	bcnd.n		ne0, LEN, 1b
	st.b		r5, DEST, LEN
   2:	or		r2, r0, r0	/* successful return */
	br		.Lcidone
.Lcidone:
	or.u		r5,r0,hi16(_curpcb)
	ld		r6,r5,lo16(_curpcb)
	st		r0,r6,PCB_ONFAULT
	jmp		r1
.Lciflt:
	or		r2, r0, EFAULT	/* return fault */
	br		.Lcidone

#undef SRC
#undef DEST
#undef LEN
/*######################################################################*/
/*######################################################################*/

/* 
 * Copy a null terminated string from the user space to the kernel
 * address space.
 *
 * copyinstr(from, to, maxlen, &lencopied)
 * r2 == from
 * r3 == to
 * r4 == maxlen
 * r5 == len actually transferred (includes the terminating NULL!!!)
 * r6 & r7 - used as temporaries
 */
#define SRC	r2
#define DEST	r3
#define CNT	r4
#define LEN	r5

ENTRY(copyinstr)
	/* setup fault handler */
	or.u		r6,   r0,   hi16(_curpcb)	
	ld		r7,   r6,   lo16(_curpcb)
	or.u		r6,   r0,   hi16(.Lcisflt)
	or		r6,   r6,   lo16(.Lcisflt)
	st		r6,   r7,   PCB_ONFAULT
	bcnd		lt0,  CNT,   .Lcisflt
	bcnd		eq0,  CNT,   .Lcisdone
	or		r6,   r0,   0
   1:	
#if  ERRATA__XXX_USR
	NOP
	ld.bu.usr   	r7,   SRC,  r6
	NOP
	NOP
	NOP
#else
	ld.bu.usr	r7,   SRC,  r6
#endif
	st.b		r7,   DEST, r6
	bcnd.n		eq0,  r7, 2f		/* all done */
	addu		r6,   r6, 1
	cmp		r7,   r6, CNT
	bb1		lt,   r7, 1b
	or		r2,   r0, ENAMETOOLONG	/* over flow */
	br		.Lcisdone
   2:   					/* all done */
	or		r2,   r0, 0
	br		.Lcisdone

.Lcisdone:
        bcnd		eq0, LEN, 3f		
	st		r6, r0, LEN
   3:	or.u		r5,r0,hi16(_curpcb)
	ld		r6,r5,lo16(_curpcb)
	st		r0,r6,PCB_ONFAULT	/* clear the handler */
	jmp		r1
.Lcisflt:
	or		r2, r0, EFAULT	/* return fault */
	br		.Lcisdone

#undef SRC
#undef DEST
#undef CNT
#undef LEN

/*
 * Copy specified amount of data from kernel to the user space
 * Copyout(from, to, len)
 *	r2 == from (kernel source address)
 *	r3 == to (user destination address)
 *	r4 == length
 */

#define SRC  	r2
#define DEST  	r3
#define LEN  	r4

ENTRY(copyout)
	/* setup fault handler */
/*	tb0 0, r0, 132     entry trap */
	or.u		r5,   r0,   hi16(_curpcb)	
	ld		r6,   r5,   lo16(_curpcb)
	or.u		r5,   r0,   hi16(.Lcoflt)
	or		r5,   r5,   lo16(.Lcoflt)
	st		r5,   r6,   PCB_ONFAULT	/* pcb_onfault = .Lcoflt */
/*	;bcnd		ne0,  LEN,  1f ; XXX optimize len = 0 case */
/*	;or		r2,   r0,   0 */
/*	;br		.Lcodone */
    /*;1: ;bcnd		lt0,  LEN,  .Lcoflt ; EFAULT if len < 0  */
	/* If it's a small length (less than 8), then do byte-by-byte */
	cmp		r9,   LEN,  8 
	bb1		lt,   r9,   copyout_byte_only 

	/* If they're not aligned similiarly, use byte only... */
	xor		r9,   SRC,  DEST 
	mask		r8,   r9,   0x3 
	bcnd		ne0,  r8,   copyout_byte_only 

	/*
	 * At this point, we don't know if they're word aligned or not,
	 * but we know that what needs to be done to one to align
	 * it is what's needed for the other.
	 */
	bb1		0,    SRC,  copyout_left_align_to_halfword
copyout_left_aligned_to_halfword:
	bb1		1,    SRC,  copyout_left_align_to_word
copyout_left_aligned_to_word:
	bb1		0,    LEN,  copyout_right_align_to_halfword
copyout_right_aligned_to_halfword:
	bb1		1,    LEN,  copyout_right_align_to_word
copyout_right_aligned_to_word:

	/*
	 * At this point, both SRC and DEST are aligned to a word
	 * boundry, and LEN is an even multiple of 4.
	 */
	bb1.n		2,    LEN,  copyout_right_align_to_doubleword
	or		r7,   r0,   4

copyout_right_aligned_to_doubleword:
	ld 		r5,   SRC,  r0
	ld    		r6,   SRC,  r7
	subu		LEN,  LEN,  8
#if  ERRATA__XXX_USR
	NOP
	st.usr		r5,   DEST, r0
	NOP
	NOP
	NOP
#else
	st.usr		r5,   DEST, r0
#endif
	addu		SRC,  SRC,  8
#if  ERRATA__XXX_USR
	NOP
	st.usr		r6,   DEST, r7
	NOP
	NOP
	NOP
#else
	st.usr		r6,   DEST, r7
#endif
	bcnd.n		ne0,  LEN,  copyout_right_aligned_to_doubleword
	addu		DEST, DEST, 8
   	or		r2, r0, r0	/* successful return */
	br		.Lcodone

	/***************************************************/
copyout_left_align_to_halfword:
	ld.b   		r5,   SRC,  r0
	subu		LEN,  LEN,  1
#if  ERRATA__XXX_USR
	NOP
	st.b.usr	r5,   DEST, r0
	NOP
	NOP
	NOP
#else
	st.b.usr	r5,   DEST, r0
#endif
	addu		SRC,  SRC,  1
	br.n		copyout_left_aligned_to_halfword
	addu		DEST, DEST, 1

copyout_left_align_to_word:
	ld.h   		r5,   SRC,  r0
	subu      	LEN,  LEN,  2
#if  ERRATA__XXX_USR
	NOP
	st.h.usr	r5,   DEST, r0
	NOP
	NOP
	NOP
#else
	st.h.usr	r5,   DEST, r0
#endif
	addu		SRC,  SRC,  2
	br.n		copyout_left_aligned_to_word
	addu		DEST, DEST, 2

copyout_right_align_to_halfword:
	subu		LEN,  LEN,  1
	ld.b   		r5,   SRC,  LEN
#if  ERRATA__XXX_USR
	NOP
	st.b.usr	r5,   DEST, LEN
	NOP
	NOP
	NOP
	br		copyout_right_aligned_to_halfword
#else
	br.n		copyout_right_aligned_to_halfword
	st.b.usr	r5,   DEST, LEN
#endif

copyout_right_align_to_word:
	subu		LEN,  LEN,  2
	ld.h   		r5,   SRC,  LEN
#if  ERRATA__XXX_USR
	NOP
	st.h.usr	r5,   DEST, LEN
	NOP
	NOP
	NOP
	br		copyout_right_aligned_to_word
#else
	br.n		copyout_right_aligned_to_word
	st.h.usr	r5,   DEST, LEN
#endif

copyout_right_align_to_doubleword:
	subu		LEN,  LEN,  4
	ld		r5,   SRC,  LEN
#if  ERRATA__XXX_USR
	NOP
	st.usr		r5,   DEST, LEN
	NOP
	NOP
	NOP
	bcnd		ne0,  LEN, copyout_right_aligned_to_doubleword
#else
	bcnd.n		ne0,  LEN, copyout_right_aligned_to_doubleword
	st.usr		r5,   DEST, LEN
#endif
   	or		r2, r0, r0	/* successful return */
	br		.Lcodone

_LABEL(copyout_byte_only)
	bcnd		eq0, LEN, 2f
   1:
	subu		LEN, LEN, 1
	ld.b   		r5, SRC, LEN
#if  ERRATA__XXX_USR
	NOP
	st.b.usr	r5, DEST, LEN
	NOP
	NOP
	NOP
	bcnd		ne0, LEN, 1b
#   else
	bcnd.n		ne0, LEN, 1b
	st.b.usr	r5, DEST, LEN
#   endif

   2:	or		r2, r0, r0	/* successful return */
	br		.Lcodone

.Lcodone:
	or.u		r5,r0,hi16(_curpcb)
	ld		r6,r5,lo16(_curpcb)
	st		r0,r6,PCB_ONFAULT	/* clear the handler */
	jmp		r1
.Lcoflt:
	or		r2, r0, EFAULT	/* return fault */
	br		.Lcodone

#undef SRC
#undef DEST
#undef LEN

/* 
 * Copy a null terminated string from the kernel space to the user
 * address space.
 *
 * copyoutstr(from, to, maxlen, &lencopied)
 * r2 == from
 * r3 == to
 * r4 == maxlen that can be copied
 * r5 == len actually copied (including the terminating NULL!!!)
 */

#define SRC	r2
#define DEST	r3
#define CNT	r4
#define LEN	r5

ENTRY(copyoutstr)
	/* setup fault handler */
	or.u		r6,   r0,   hi16(_curpcb)	
	ld		r7,   r6,   lo16(_curpcb)
	or.u		r6,   r0,   hi16(.Lcosflt)
	or		r6,   r6,   lo16(.Lcosflt)
	st		r6,   r7,   PCB_ONFAULT
	bcnd		lt0,  CNT,   .Lcosflt
	bcnd		eq0,  CNT,   .Lcosdone
	or		r6,   r0,   0
   1:	
	ld.bu		r7,   SRC,  r6
#if  ERRATA__XXX_USR
	NOP
	st.b.usr   	r7,   DEST,  r6
	NOP
	NOP
	NOP
#else
	st.b.usr	r7,   DEST,  r6
#endif
	bcnd.n		eq0,  r7, 2f		/* all done */
	addu		r6,   r6, 1
	cmp		r7,   r6, CNT
	bb1		lt,   r7, 1b
	or		r2,   r0, ENAMETOOLONG	/* over flow */
	br		.Lcosdone
   2:   					/* all done */
	or		r2,   r0, 0
	br		.Lcosdone

.Lcosflt:
	or		r2, r0, EFAULT	/* return fault */
	br		.Lcosdone

.Lcosdone:
        bcnd		eq0, LEN, 3f		
	st		r6, r0, LEN
   3:	or.u		r5,r0,hi16(_curpcb)
	ld		r6,r5,lo16(_curpcb)
	st		r0,r6,PCB_ONFAULT	/* clear the handler */
	jmp		r1

#undef SRC
#undef DEST
#undef CNT
#undef LEN

/*######################################################################*/
/*LABEL(_ALLOW_FAULT_END)*/
/*word 0	*/ /* to separate from routine below */
/*######################################################################*/

/*
 * Gcc 2 generates calls to memcpy for bcopies of unknown size. memcpy
 * can simply be implemented as ovbcopy but the src (r2, r3) and dst args need to
 * be switched. 
 */
/*
 * void memcpy(dest, source, count)
 *
 */
ENTRY(memcpy)
 	or 	r5, r0, r2    /* dst -> tmp */
	or	r2, r0, r3    /* src -> 1st arg */
	br.n	_ovbcopy      /* call ovbcopy */
       	or	r3, r0, r5    /* dst -> 2nd arg */
	
       
/*
 * void bcopy(source, destination, count)
 *
 * copy count bytes of data from source to destination
 * Don Harper (don@omron.co.jp), Omron Corporation.
 *
 */

ENTRY(bcopy)
ENTRY(ovbcopy)
	bcnd	le0,r4,bcopy_out /* nothing to do if count <= 0 */
/*
 *	check position of source and destination data
 */
	cmp 	r9,r2,r3	/* compare source address to destination */
	bb1	eq,r9,bcopy_out	/* nothing to do if addresses are equal */	
	bb1	lo,r9,bcopy_reverse /* copy in reverse if src < destination */
/*
 *	source address is greater than destination address, copy forward 
 */
	cmp 	r9,r4,16	/* see if we have at least 16 bytes */
	bb1	lt,r9,f_byte_copy /* copy bytes for small data length */ 
/*
 *	determine copy strategy based on alignment of source and destination
 */
	mask	r6,r2,3		/* get 2 low order bits of source address */
	mask	r7,r3,3		/* get 2 low order bits of destintation addr */
	mak	r6,r6,0<4>	/* convert source bits to table offset */
	mak	r7,r7,0<2>	/* convert destination bits to table offset */
	or.u	r12,r0,hi16(f_strat) /* forward strategy table address (high) */
	or	r12,r12,lo16(f_strat) /* forward strategy table address (low) */
	addu	r6,r6,r7	/* compute final table offset for strategy */
	ld	r12,r12,r6	/* load the strategy routine */
	jmp	r12		/* branch to strategy routine */


/*
 * Copy three bytes from src to destination then copy words
 */
_LABEL(f_3byte_word_copy)
	ld.bu	r6,r2,0		/* load byte from source */
	ld.bu	r7,r2,1		/* load byte from source */
	ld.bu	r8,r2,2		/* load byte from source */
	st.b	r6,r3,0		/* store byte to destination */
	st.b	r7,r3,1		/* store byte to destination */
	st.b	r8,r3,2		/* store byte to destination */
	addu	r2,r2,3		/* increment source pointer */
	addu	r3,r3,3		/* increment destination pointer */
	br.n	f_word_copy	/* copy full words */
	subu	r4,r4,3		/* decrement length */

/*
 * Copy 1 halfword from src to destination then copy words
 */
_LABEL(f_1half_word_copy)
	ld.hu	r6,r2,0		/* load half-word from source */
	st.h	r6,r3,0		/* store half-word to destination */
	addu	r2,r2,2		/* increment source pointer */
	addu	r3,r3,2		/* increment destination pointer */
	br.n	f_word_copy	/* copy full words */
	subu	r4,r4,2		/* decrement remaining length */

/* 
 * Copy 1 byte from src to destination then copy words
 */
_LABEL(f_1byte_word_copy)
	ld.bu	r6,r2,0		/* load 1 byte from source */
	st.b	r6,r3,0		/* store 1 byte to destination */
	addu	r2,r2,1		/* increment source pointer */
	addu	r3,r3,1		/* increment destination pointer */
	subu	r4,r4,1		/* decrement remaining length */
	/* fall through to word copy */
/*
 * Copy as many full words as possible, 4 words per loop
 */	
_LABEL(f_word_copy)
	cmp	r10,r4,16	/* see if we have 16 bytes remaining */	
	bb1	lo,r10,f_byte_copy 	/* not enough left, copy bytes */
	ld	r6,r2,0		/* load first word */
	ld	r7,r2,4		/* load second word */
	ld	r8,r2,8		/* load third word */
	ld	r9,r2,12	/* load fourth word */
	st	r6,r3,0		/* store first word */
	st	r7,r3,4		/* store second word */
	st 	r8,r3,8		/* store third word */
	st 	r9,r3,12	/* store fourth word */
	addu	r2,r2,16	/* increment source pointer */
	addu	r3,r3,16	/* increment destination pointer */
	br.n	f_word_copy	/* branch to copy another block */
	subu	r4,r4,16	/* decrement remaining length */
	
_LABEL(f_1byte_half_copy)
	ld.bu	r6,r2,0		/* load 1 byte from source */
	st.b	r6,r3,0		/* store 1 byte to destination */
	addu	r2,r2,1		/* increment source pointer */
	addu	r3,r3,1		/* increment destination pointer */
	subu	r4,r4,1		/* decrement remaining length */
	/* fall through to half copy */

_LABEL(f_half_copy)
	cmp	r10,r4,16	/* see if we have 16 bytes remaining */	
	bb1	lo,r10,f_byte_copy /* not enough left, copy bytes */
	ld.hu	r6,r2,0		/* load first half-word */
	ld.hu	r7,r2,2		/* load second half-word */
	ld.hu	r8,r2,4		/* load third half-word */
	ld.hu	r9,r2,6		/* load fourth half-word */
	ld.hu	r10,r2,8	/* load fifth half-word */
	ld.hu	r11,r2,10	/* load sixth half-word */
	ld.hu	r12,r2,12	/* load seventh half-word */
	ld.hu	r13,r2,14	/* load eighth half-word */
	st.h	r6,r3,0		/* store first half-word */
	st.h	r7,r3,2		/* store second half-word */
	st.h 	r8,r3,4		/* store third half-word */
	st.h 	r9,r3,6		/* store fourth half-word */
	st.h	r10,r3,8	/* store fifth half-word */
	st.h	r11,r3,10	/* store sixth half-word */
	st.h 	r12,r3,12	/* store seventh half-word */
	st.h 	r13,r3,14	/* store eighth half-word */
	addu	r2,r2,16	/* increment source pointer */
	addu	r3,r3,16	/* increment destination pointer */
	br.n	f_half_copy	/* branch to copy another block */
	subu	r4,r4,16	/* decrement remaining length */
	
_LABEL(f_byte_copy)
	bcnd	eq0,r4,bcopy_out /* branch if nothing left to copy */ 
	ld.bu	r6,r2,0		/* load byte from source */
	st.b	r6,r3,0		/* store byte in destination */
	addu	r2,r2,1		/* increment source pointer */
	addu	r3,r3,1		/* increment destination pointer */
	br.n	f_byte_copy	/* branch for next byte */ 
	subu	r4,r4,1		/* decrement remaining length */

/*
 *	source address is less than destination address, copy in reverse
 */
_LABEL(bcopy_reverse)
/*
 * start copy pointers at end of data 
 */
	addu	r2,r2,r4	/* start source at end of data */
	addu	r3,r3,r4	/* start destination at end of data */
/*
 * check for short data
 */
	cmp 	r9,r4,16	/* see if we have at least 16 bytes */
	bb1	lt,r9,r_byte_copy /* copy bytes for small data length */ 
/*
 *	determine copy strategy based on alignment of source and destination
 */
	mask	r6,r2,3		/* get 2 low order bits of source address */
	mask	r7,r3,3		/* get 2 low order bits of destintation addr */
	mak	r6,r6,0<4>	/* convert source bits to table offset */
	mak	r7,r7,0<2>	/* convert destination bits to table offset */
	or.u	r12,r0,hi16(r_strat) /* reverse strategy table address (high) */
	or	r12,r12,lo16(r_strat) /* reverse strategy table address (low) */
	addu	r6,r6,r7	/* compute final table offset for strategy */
	ld	r12,r12,r6	/* load the strategy routine */
	jmp	r12		/* branch to strategy routine */

/*
 * Copy three bytes from src to destination then copy words
 */
_LABEL(r_3byte_word_copy)
	subu	r2,r2,3		/* decrement source pointer */
	subu	r3,r3,3		/* decrement destination pointer */
	ld.bu	r6,r2,0		/* load byte from source */
	ld.bu	r7,r2,1		/* load byte from source */
	ld.bu	r8,r2,2		/* load byte from source */
	st.b	r6,r3,0		/* store byte to destination */
	st.b	r7,r3,1		/* store byte to destination */
	st.b	r8,r3,2		/* store byte to destination */
	br.n	r_word_copy	/* copy full words */
	subu	r4,r4,3		/* decrement length */

/*
 * Copy 1 halfword from src to destination then copy words
 */
_LABEL(r_1half_word_copy)
	subu	r2,r2,2		/* decrement source pointer */
	subu	r3,r3,2		/* decrement destination pointer */
	ld.hu	r6,r2,0		/* load half-word from source */
	st.h	r6,r3,0		/* store half-word to destination */
	br.n	r_word_copy	/* copy full words */
	subu	r4,r4,2		/* decrement remaining length */

/* 
 * Copy 1 byte from src to destination then copy words
 */
_LABEL(r_1byte_word_copy)
	subu	r2,r2,1		/* decrement source pointer */
	subu	r3,r3,1		/* decrement destination pointer */
	ld.bu	r6,r2,0		/* load 1 byte from source */
	st.b	r6,r3,0		/* store 1 byte to destination */
	subu	r4,r4,1		/* decrement remaining length */
	/* fall through to word copy */
/*
 * Copy as many full words as possible, 4 words per loop
 */	
_LABEL(r_word_copy)
	cmp	r10,r4,16	/* see if we have 16 bytes remaining */	
	bb1	lo,r10,r_byte_copy 	/* not enough left, copy bytes */
	subu	r2,r2,16	/* decrement source pointer */
	subu	r3,r3,16	/* decrement destination pointer */
	ld	r6,r2,0		/* load first word */
	ld	r7,r2,4		/* load second word */
	ld	r8,r2,8		/* load third word */
	ld	r9,r2,12	/* load fourth word */
	st	r6,r3,0		/* store first word */
	st	r7,r3,4		/* store second word */
	st 	r8,r3,8		/* store third word */
	st 	r9,r3,12	/* store fourth word */
	br.n	r_word_copy	/* branch to copy another block */
	subu	r4,r4,16	/* decrement remaining length */
	
_LABEL(r_1byte_half_copy)
	subu	r2,r2,1		/* decrement source pointer */
	subu	r3,r3,1		/* decrement destination pointer */
	ld.bu	r6,r2,0		/* load 1 byte from source */
	st.b	r6,r3,0		/* store 1 byte to destination */
	subu	r4,r4,1		/* decrement remaining length */
	/* fall through to half copy */

_LABEL(r_half_copy)
	cmp	r10,r4,16	/* see if we have 16 bytes remaining */	
	bb1	lo,r10,r_byte_copy /* not enough left, copy bytes */
	subu	r2,r2,16	/* decrement source pointer */
	subu	r3,r3,16	/* decrement destination pointer */
	ld.hu	r6,r2,0		/* load first half-word */
	ld.hu	r7,r2,2		/* load second half-word */
	ld.hu	r8,r2,4		/* load third half-word */
	ld.hu	r9,r2,6		/* load fourth half-word */
	ld.hu	r10,r2,8	/* load fifth half-word */
	ld.hu	r11,r2,10	/* load sixth half-word */
	ld.hu	r12,r2,12	/* load seventh half-word */
	ld.hu	r13,r2,14	/* load eighth half-word */
	st.h	r6,r3,0		/* store first half-word */
	st.h	r7,r3,2		/* store second half-word */
	st.h 	r8,r3,4		/* store third half-word */
	st.h 	r9,r3,6		/* store fourth half-word */
	st.h	r10,r3,8	/* store fifth half-word */
	st.h	r11,r3,10	/* store sixth half-word */
	st.h 	r12,r3,12	/* store seventh half-word */
	st.h 	r13,r3,14	/* store eighth half-word */
	br.n	r_half_copy	/* branch to copy another block */
	subu	r4,r4,16	/* decrement remaining length */
	
_LABEL(r_byte_copy)
	bcnd	eq0,r4,bcopy_out /* branch if nothing left to copy */ 
	subu	r2,r2,1		/* decrement source pointer */
	subu	r3,r3,1		/* decrement destination pointer */
	ld.bu	r6,r2,0		/* load byte from source */
	st.b	r6,r3,0		/* store byte in destination */
	br.n	r_byte_copy	/* branch for next byte */ 
	subu	r4,r4,1		/* decrement remaining length */

_LABEL(bcopy_out)
	jmp	r1		/* all done, return to caller */
	
	data
	align	4
_LABEL(f_strat)
	word	f_word_copy
	word	f_byte_copy
	word	f_half_copy
	word	f_byte_copy
	word	f_byte_copy
	word	f_3byte_word_copy
	word	f_byte_copy
	word	f_1byte_half_copy
	word	f_half_copy
	word	f_byte_copy
	word	f_1half_word_copy
	word	f_byte_copy
	word	f_byte_copy
	word	f_1byte_half_copy
	word	f_byte_copy
	word	f_1byte_word_copy

_LABEL(r_strat)
	word	r_word_copy
	word	r_byte_copy
	word	r_half_copy
	word	r_byte_copy
	word	r_byte_copy
	word	r_1byte_word_copy
	word	r_byte_copy
	word	r_1byte_half_copy
	word	r_half_copy
	word	r_byte_copy
	word	r_1half_word_copy
	word	r_byte_copy
	word	r_byte_copy
	word	r_1byte_half_copy
	word	r_byte_copy
	word	r_3byte_word_copy

	text

/*######################################################################*/
/*######################################################################*/

/*
 * April 1990, Omron Corporation
 * jfriedl@nff.ncl.omron.co.jp
 *
 * void bzero(destination, length)
 *
 * Clear (set to zero) LENGTH bytes of memory starting at DESTINATION.
 * Note that there is no return value.
 *
 * This is fast. Really fast. Especially for long lengths.
 */
#define R_dest			r2
#define R_len			r3

#define R_bytes			r4
#define R_mark_address		r5
#define R_addr			r6	/* R_addr && R_temp SHARE */
#define R_temp			r6	/* R_addr && R_temp SHARE */


ENTRY(blkclr)
ENTRY(bzero)
	/*
	 * If the destination is not word aligned, we'll word align
	 * it first to make things easier.
	 *
	 * We'll check to see first if bit #0 is set and then bit #1
	 * (of the destination address). If either are set, it's
	 * not word aligned.
	 */
	bb1	0, R_dest, not_initially_word_aligned
	bb1	1, R_dest, not_initially_word_aligned

   now_word_aligned:
	/*
	 * before we get into the main loop, grab the
	 * address of the label "mark" below.
	 */
	or.u	R_mark_address, r0, hi16(mark)
	or	R_mark_address, R_mark_address, lo16(mark)
	
   top_of_main_loop:
#	define MAX_AT_ONE_TIME 128
	/*
	 * Now we find out how many words we can zero-fill in a row.
	 * We do this by doing something like:
 	 *
	 *	bytes &= 0xfffffffc;
	 *	if (bytes > MAX_AT_ONE_TIME)
	 *		bytes = MAX_AT_ONE_TIME;
	 */

	/*
	 * Clear lower two bits of length to give us the number of bytes
	 * ALIGNED TO THE WORD LENGTH remaining to move.
	 */
	clr	R_bytes, R_len, 2<0>

	/* if we're done clearing WORDS, jump out */
	bcnd	eq0, R_bytes, done_doing_words

	/* if the number of bytes > MAX_AT_ONE_TIME, do only the max */
	cmp	R_temp, R_bytes, MAX_AT_ONE_TIME
	bb1	lt, R_temp, 1f

		/*
		 * Since we're doing the max, we know exactly where we're
	 	 * jumping (the first one in the list!), so we can jump
		 * right there.  However, we've still got to adjust
		 * the length, so we'll jump to where we ajust the length
	 	 * which just happens to fall through to the first store zero
		 * in the list.
		 *
	 	 * Note, however, that we're jumping to an instruction that
		 * would be in the delay slot for the jump in front of it,
		 * so if you change things here, WATCH OUT.
		 */
		br.n	do_max
		or	R_bytes, r0, MAX_AT_ONE_TIME
	
     1:

	/*
	 * Now we have the number of bytes to zero during this iteration,
	 * (which, as it happens, is the last iteration if we're here).
	 * We'll calculate the proper place to jump and then jump there,
	 * after adjusting the length.  NOTE that there is a label between
	 * the "jmp.n" and the "subu" below... the "subu" is NOT always
	 * executed in the delay slot of the "jmp.n".
 	 */
	subu	R_addr, R_mark_address, R_bytes

	/* and go there (after adjusting the length via ".n") */
	jmp.n	R_addr
do_max: subu	R_len, R_len, R_bytes	/* NOTE: this is in the delay slot! */

	st	r0, R_dest, 0x7c	/* 128 */
	st	r0, R_dest, 0x78	/* 124 */
	st	r0, R_dest, 0x74	/* 120 */
	st	r0, R_dest, 0x70	/* 116 */
	st	r0, R_dest, 0x6c	/* 112 */
	st	r0, R_dest, 0x68	/* 108 */
	st	r0, R_dest, 0x64	/* 104 */
	st	r0, R_dest, 0x60	/* 100 */
	st	r0, R_dest, 0x5c	/*  96 */
	st	r0, R_dest, 0x58	/*  92 */
	st	r0, R_dest, 0x54	/*  88 */
	st	r0, R_dest, 0x50	/*  84 */
	st	r0, R_dest, 0x4c	/*  80 */
	st	r0, R_dest, 0x48	/*  76 */
	st	r0, R_dest, 0x44	/*  72 */
	st	r0, R_dest, 0x40	/*  68 */
	st	r0, R_dest, 0x3c	/*  64 */
	st	r0, R_dest, 0x38	/*  60 */
	st	r0, R_dest, 0x34	/*  56 */
	st	r0, R_dest, 0x30	/*  52 */
	st	r0, R_dest, 0x2c	/*  44 */
	st	r0, R_dest, 0x28	/*  40 */
	st	r0, R_dest, 0x24	/*  36 */
	st	r0, R_dest, 0x20	/*  32 */
	st	r0, R_dest, 0x1c	/*  28 */
	st	r0, R_dest, 0x18	/*  24 */
	st	r0, R_dest, 0x14	/*  20 */
	st	r0, R_dest, 0x10	/*  16 */
	st	r0, R_dest, 0x0c	/*  12 */
	st	r0, R_dest, 0x08	/*   8 */
	st	r0, R_dest, 0x04	/*   4 */
	st	r0, R_dest, 0x00	/*   0 */

   mark:
	br.n	top_of_main_loop
	addu	R_dest, R_dest, R_bytes	/* bump up the dest address */



   done_doing_words:
	bcnd	ne0, R_len, finish_up_last_bytes
	jmp	r1		/* RETURN */

   finish_up_last_bytes:
	subu	R_len, R_len, 1
	bcnd.n	ne0, R_len, finish_up_last_bytes
	st.b	r0, R_dest, R_len

   leave:
	jmp	r1		/* RETURN */
	
   not_initially_word_aligned:
	/*
	 * Bzero to word-align the address (at least if the length allows it).
	 */
	bcnd	eq0, R_len, leave
	st.b	r0, R_dest, 0
	addu	R_dest, R_dest, 1
	mask	R_temp, R_dest, 0x3
	bcnd.n	eq0, R_temp, now_word_aligned
	subu	R_len, R_len, 1
	br	not_initially_word_aligned

#undef  R_dest
#undef  R_len
#undef  R_bytes
#undef  R_mark_address
#undef  R_addr
#undef  R_temp
#undef  MAX_AT_ONE_TIME

/**********************************************************************/
/**********************************************************************/
/**********************************************************************/

/*
 * non-local goto
 *	int setjmp(label_t *);
 *	void longjmp(label_t*);
 */
	global _setjmp
_setjmp:
	st	r1,r2,0
	st	r14,r2,4
	st	r15,r2,2*4
	st	r16,r2,3*4
	st	r17,r2,4*4
	st	r18,r2,5*4
	st	r19,r2,6*4
	st	r20,r2,7*4
	st	r21,r2,8*4
	st	r22,r2,9*4
	st	r23,r2,10*4
	st	r24,r2,11*4
	st	r25,r2,12*4
	st	r26,r2,13*4
	st	r27,r2,14*4
	st	r28,r2,15*4
	st	r29,r2,16*4
	st	r30,r2,17*4
	st	r31,r2,18*4
	jmp.n	r1
	or	r2,r0,r0

	global	_longjmp
_longjmp:
	ld	r1,r2,0
	ld	r14,r2,4
	ld	r15,r2,2*4
	ld	r16,r2,3*4
	ld	r17,r2,4*4
	ld	r18,r2,5*4
	ld	r19,r2,6*4
	ld	r20,r2,7*4
	ld	r21,r2,8*4
	ld	r22,r2,9*4
	ld	r23,r2,10*4
	ld	r24,r2,11*4
	ld	r25,r2,12*4
	ld	r26,r2,13*4
	ld	r27,r2,14*4
	ld	r28,r2,15*4
	ld	r29,r2,16*4
	ld	r30,r2,17*4
	ld	r31,r2,18*4
	jmp.n	r1
	or	r2,r0,1

ENTRY(longjmp_int_enable)
	ld	r1,r2,0
	ld	r14,r2,4
	ld	r15,r2,2*4
	ld	r16,r2,3*4
	ld	r17,r2,4*4
	ld	r18,r2,5*4
	ld	r19,r2,6*4
	ld	r20,r2,7*4
	ld	r21,r2,8*4
	ld	r22,r2,9*4
	ld	r23,r2,10*4
	ld	r24,r2,11*4
	ld	r25,r2,12*4
	ld	r26,r2,13*4
	ld	r27,r2,14*4
	ld	r28,r2,15*4
	ld	r29,r2,16*4
	ld	r30,r2,17*4
	ld	r31,r2,18*4
	or	r2,r3,r0
	ldcr	r10,PSR
	clr	r10,r10,1<PSR_INTERRUPT_DISABLE_BIT>
	stcr	r10,PSR
	jmp	r1

ENTRY(getsp)
	or	r2, r0, r31
	jmp	r1

/*
 * This has to be cleaned - we should not use INT_MASK_LEVEL. Should
 * use pcc2_int_lvl instead. XXX nivas
 */
/*
 * I don't think we need to explictly enable interrupts. We can always enable
 * interrupts and depend on the int level and mask in PCC2 to block the
 * appropriate interrupts for us. If we don't depend on the PCC2 to block ints
 * for us, we need to explicitly set the IND bit in the PSR for every spln(x)
 * when x > 0. But for now... Rewrite the whole mess. XXX nivas
 * This is bogus - it is blocking interrupts for all but 0 XXX nivas
 */

ENTRY(spln)
	ldcr	r10,PSR
	or	r11,r0,r10
	or	r5,r2,r0
	bb1	PSR_INTERRUPT_DISABLE_BIT,r10,1f
	set	r10,r10,1<PSR_INTERRUPT_DISABLE_BIT>
	stcr	r10,PSR
        FLUSH_PIPELINE
    1:
	or.u	r3,r0,hi16(INT_MASK_LEVEL)
	or	r3,r3,lo16(INT_MASK_LEVEL)
	xmem.bu	r2,r3,r0
	bcnd	ne0, r5, 2f
	clr	r11, r11, 1<PSR_INTERRUPT_DISABLE_BIT>
	stcr	r11,PSR
        FLUSH_PIPELINE
    2:
	jmp	r1

ENTRY(getipl)
ENTRY(spl)
	ldcr	r10,PSR
	or	r11,r0,r10
	bb1	PSR_INTERRUPT_DISABLE_BIT, r10, 1f
	set	r10,r10,1<PSR_INTERRUPT_DISABLE_BIT>
	stcr	r10,PSR
        FLUSH_PIPELINE
      1:
	or.u	r3,r0,hi16(INT_MASK_LEVEL)
	ld.b	r2,r3,lo16(INT_MASK_LEVEL)
	stcr	r11,PSR
        FLUSH_PIPELINE
	jmp	r1

/*
 *	Set the interrupt mask to the value passed in, returning the
 *	the current mask level. This routine does not enable/disable
 *	interrupts explicitly. It is assumed that the callers know what
 *	to do with interrupts.
 */
 
ENTRY(setipl)
	ldcr	r10,PSR
	or	r11,r0,r10
	bb1	PSR_INTERRUPT_DISABLE_BIT, r10, 1f
	set	r10,r10,1<PSR_INTERRUPT_DISABLE_BIT>
	stcr	r10,PSR 			/* disable ints, if needed */
        FLUSH_PIPELINE
     1:
	/* get the current mask value */
	or.u	r3,r0,hi16(INT_MASK_LEVEL)
	or	r3,r3,lo16(INT_MASK_LEVEL)
	xmem.bu	r2,r3,r0			/* xchng the new mask value */
        FLUSH_PIPELINE
	stcr	r11,PSR				/* restore psr */
        FLUSH_PIPELINE
	jmp	r1				/* and return the old value */

/*
 *       invalidate_pte(pte)
 *
 *       This function will invalidate specified pte indivisibly
 *       to avoid the write-back of used-bit and/or modify-bit into
 *       that pte.  It also returns the pte found in the table.
 */
ENTRY(invalidate_pte)
        or      r3,r0,r0
        xmem    r3,r2,r0
	tb1     0,r0,0
        jmp.n   r1
        or      r2,r3,r0

#if DDB

ENTRY(db_spln)
	ldcr	r10,PSR
	or	r11,r0,r10
	or	r5,r2,r0
	bb1	PSR_INTERRUPT_DISABLE_BIT,r10,1f
	set	r10,r10,1<PSR_INTERRUPT_DISABLE_BIT>
	stcr	r10,PSR
        FLUSH_PIPELINE
    1:
	or.u	r3,r0,hi16(INT_MASK_LEVEL)
	or	r3,r3,lo16(INT_MASK_LEVEL)
	xmem.bu	r2,r3,r0
	bcnd	ne0, r5, 2f
	clr	r11, r11, 1<PSR_INTERRUPT_DISABLE_BIT>
	stcr	r11,PSR
        FLUSH_PIPELINE
    2:
	jmp	r1

ENTRY(db_setipl)
	ldcr	r10,PSR
	or	r11,r0,r10
	bb1	PSR_INTERRUPT_DISABLE_BIT, r10, 1f
	set	r10,r10,1<PSR_INTERRUPT_DISABLE_BIT>
	stcr	r10,PSR 			/* disable ints, if needed */
        FLUSH_PIPELINE
     1:
	/* get the current mask value */
	or.u	r3,r0,hi16(INT_MASK_LEVEL)
	or	r3,r3,lo16(INT_MASK_LEVEL)
	xmem.bu	r2,r3,r0			/* xchng the new mask value */
        FLUSH_PIPELINE
	stcr	r11,PSR				/* restore psr */
        FLUSH_PIPELINE
	jmp	r1				/* and return the old value */

ENTRY(db_getipl)
ENTRY(db_spl)
	ldcr	r10,PSR
	or	r11,r0,r10
	bb1	PSR_INTERRUPT_DISABLE_BIT, r10, 1f
	set	r10,r10,1<PSR_INTERRUPT_DISABLE_BIT>
	stcr	r10,PSR
      1:
	or.u	r3,r0,hi16(INT_MASK_LEVEL)
	ld.b	r2,r3,lo16(INT_MASK_LEVEL)
	stcr	r11,PSR
        FLUSH_PIPELINE
	jmp	r1

ENTRY(db_flush_pipeline)
        FLUSH_PIPELINE
	jmp	r1
#endif /* DDB */	

ENTRY(read_processor_identification_register)
     	jmp.n	r1
	ldcr	r2, PID

ENTRY(safe_byte_access)
	ld.b	 r9,r0,r2
	tb1	 0,r0,0
	br	 @L1
	or	r0,r0,r0
@L1:
	st.b	 r9,r0,r3
	jmp	r1

ENTRY(guarded_access)
ENTRY(guarded_access_start)
	cmp	 r9,r3,4
	bb1	 eq,r9,@L145
	cmp	 r9,r3,2
	bb1	 eq,r9,@L144
	cmp	 r9,r3,1
	bb1	 eq,r9,@L143
	br	 _guarded_access_bad
@L143:
	ld.b	 r9,r0,r2
	tb1	0, r0, 0
	st.b	 r9,r0,r4
	br	 @L142
@L144:
	ld.h	 r9,r0,r2
	tb1	0, r0, 0
	st.h	 r9,r0,r4
	br	 @L142
@L145:
	ld	 r9,r0,r2
	tb1	0, r0, 0
	st	 r9,r0,r4
	br	 @L142

ENTRY(guarded_access_bad)
	jmp.n	 r1
	or	 r2,r0,14
@L142:

ENTRY(guarded_access_end)
	jmp.n	 r1
	or	 r2,r0,0
