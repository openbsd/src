/*	$NetBSD: copy.s,v 1.22 1995/12/11 02:37:55 thorpej Exp $	*/

/*-
 * Copyright (c) 1994, 1995 Charles Hannum.
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
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
 */

#include <sys/errno.h>
#include <machine/asm.h>

#include "assym.s"

	.file	"copy.s"
	.text

#ifdef	DIAGNOSTIC
/*
 * The following routines all use the "moves" instruction to access
 * memory with "user" privilege while running in supervisor mode.
 * The "function code" registers actually determine what type of
 * access "moves" does, and the kernel arranges to leave them set
 * for "user data" access when these functions are called.
 *
 * The diagnostics:  CHECK_SFC,  CHECK_DFC
 * will verify that the sfc/dfc register values are correct.
 */
Lbadfc_msg:
	.asciz	"copy.s: bad sfc or dfc"
	.even
badfc:
	pea	Lbadfc_msg
	jsr	_panic
	bra	badfc
#define	CHECK_SFC	movec sfc,d0; subql #FC_USERD,d0; bne badfc
#define	CHECK_DFC	movec dfc,d0; subql #FC_USERD,d0; bne badfc
#else	/* DIAGNOSTIC */
#define	CHECK_SFC
#define	CHECK_DFC
#endif	/* DIAGNOSTIC */

#ifdef	MAPPEDCOPY
	.globl	_mappedcopyin
	.globl	_mappedcopyout
	.globl	_mappedcopysize
#endif

/*
 * copyin(caddr_t from, caddr_t to, size_t len);
 * Copy len bytes from the user's address space.
 *
 * This is probably not the best we can do, but it is still 2-10 times
 * faster than the C version in the portable gen directory.
 *
 * Things that might help:
 *	- unroll the longword copy loop (might not be good for a 68020)
 *	- longword align when possible (only on the 68020)
 */
ENTRY(copyin)
	CHECK_SFC
	movl	sp@(12),d0		| check count
	beq	Lciret			| == 0, don't do anything
#ifdef MAPPEDCOPY
	cmpl	_mappedcopysize,d0	| size >= mappedcopysize
	bcc	_mappedcopyin		| yes, go do it the new way
#endif
	movl	d2,sp@-			| save scratch register
	movl	_curpcb,a0		| set fault handler
	movl	#Lcifault,a0@(PCB_ONFAULT)
	movl	sp@(8),a0		| src address
	movl	sp@(12),a1		| dest address
	movl	a0,d1
	btst	#0,d1			| src address odd?
	beq	Lcieven			| no, skip alignment
	movsb	a0@+,d2			| yes, copy a byte
	movb	d2,a1@+
	subql	#1,d0			| adjust count
	beq	Lcidone			| count 0, all done
Lcieven:
	movl	a1,d1
	btst	#0,d1			| dest address odd?
	bne	Lcibytes		| yes, must copy bytes
	movl	d0,d1			| OK, both even.  Get count
	lsrl	#2,d1			|   and convert to longwords
	beq	Lcibytes		| count 0, skip longword loop
	subql	#1,d1			| predecrement for dbf
Lcilloop:
	movsl	a0@+,d2			| copy a longword
	movl	d2,a1@+
	dbf	d1,Lcilloop		| decrement low word of count
	subil	#0x10000,d1		| decrement high word of count
	bcc	Lcilloop
	andl	#3,d0			| what remains
	beq	Lcidone			| nothing, all done
Lcibytes:
	subql	#1,d0			| predecrement for dbf
Lcibloop:
	movsb	a0@+,d2			| copy a byte
	movb	d2,a1@+
	dbf	d0,Lcibloop		| decrement low word of count
	subil	#0x10000,d0		| decrement high word of count
	bcc	Lcibloop
	clrl	d0			| no error
Lcidone:
	movl	_curpcb,a0		| clear fault handler
	clrl	a0@(PCB_ONFAULT)
	movl	sp@+,d2			| restore scratch register
Lciret:
	rts
Lcifault:
	moveq	#EFAULT,d0		| got a fault
	bra	Lcidone

/*
 * copyout(caddr_t from, caddr_t to, size_t len);
 * Copy len bytes into the user's address space.
 *
 * This is probably not the best we can do, but it is still 2-10 times
 * faster than the C version in the portable gen directory.
 *
 * Things that might help:
 *	- unroll the longword copy loop (might not be good for a 68020)
 *	- longword align when possible (only on the 68020)
 */
ENTRY(copyout)
	CHECK_DFC
	movl	sp@(12),d0		| check count
	beq	Lcoret			| == 0, don't do anything
#ifdef MAPPEDCOPY
	cmpl	_mappedcopysize,d0	| size >= mappedcopysize
	bcc	_mappedcopyout		| yes, go do it the new way
#endif
	movl	d2,sp@-			| save scratch register
	movl	_curpcb,a0		| set fault handler
	movl	#Lcofault,a0@(PCB_ONFAULT)
	movl	sp@(8),a0		| src address
	movl	sp@(12),a1		| dest address
	movl	a0,d1
	btst	#0,d1			| src address odd?
	beq	Lcoeven			| no, skip alignment
	movb	a0@+,d2			| yes, copy a byte
	movsb	d2,a1@+
	subql	#1,d0			| adjust count
	beq	Lcodone			| count 0, all done
Lcoeven:
	movl	a1,d1
	btst	#0,d1			| dest address odd?
	bne	Lcobytes		| yes, must copy bytes
	movl	d0,d1			| OK, both even.  Get count
	lsrl	#2,d1			|   and convert to longwords
	beq	Lcobytes		| count 0, skip longword loop
	subql	#1,d1			| predecrement for dbf
Lcolloop:
	movl	a0@+,d2			| copy a longword
	movsl	d2,a1@+
	dbf	d1,Lcolloop		| decrement low word of count
	subil	#0x10000,d1		| decrement high word of count
	bcc	Lcolloop
	andl	#3,d0			| what remains
	beq	Lcodone			| nothing, all done
Lcobytes:
	subql	#1,d0			| predecrement for dbf
Lcobloop:
	movb	a0@+,d2			| copy a byte
	movsb	d2,a1@+
	dbf	d0,Lcobloop		| decrement low word of count
	subil	#0x10000,d0		| decrement high word of count
	bcc	Lcobloop
	clrl	d0			| no error
Lcodone:
	movl	_curpcb,a0		| clear fault handler
	clrl	a0@(PCB_ONFAULT)
	movl	sp@+,d2			| restore scratch register
Lcoret:
	rts
Lcofault:
	moveq	#EFAULT,d0
	bra	Lcodone

/*
 * copystr(caddr_t from, caddr_t to, size_t maxlen, size_t *lencopied);
 * Copy a NUL-terminated string, at most maxlen characters long.  Return the
 * number of characters copied (including the NUL) in *lencopied.  If the
 * string is too long, return ENAMETOOLONG; else return 0.
 */
ENTRY(copystr)
	movl	sp@(4),a0		| a0 = fromaddr
	movl	sp@(8),a1		| a1 = toaddr
	clrl	d0
	movl	sp@(12),d1		| count
	beq	Lcsdone			| nothing to copy
	subql	#1,d1			| predecrement for dbeq
Lcsloop:
	movb	a0@+,a1@+		| copy a byte
	dbeq	d1,Lcsloop		| decrement low word of count
	beq	Lcsdone			| copied null, exit
	subil	#0x10000,d1		| decrement high word of count
	bcc	Lcsloop			| more room, keep going
	moveq	#ENAMETOOLONG,d0	| ran out of space
Lcsdone:
	tstl	sp@(16)			| length desired?
	beq	Lcsret
	subl	sp@(4),a0		| yes, calculate length copied
	movl	sp@(16),a1		| store at return location
	movl	a0,a1@
Lcsret:
	rts

/*
 * copyinstr(caddr_t from, caddr_t to, size_t maxlen, size_t *lencopied);
 * Copy a NUL-terminated string, at most maxlen characters long, from the
 * user's address space.  Return the number of characters copied (including
 * the NUL) in *lencopied.  If the string is too long, return ENAMETOOLONG;
 * else return 0 or EFAULT.
 */
ENTRY(copyinstr)
	CHECK_SFC
	movl	_curpcb,a0		| set fault handler
	movl	#Lcisfault,a0@(PCB_ONFAULT)
	movl	sp@(4),a0		| a0 = fromaddr
	movl	sp@(8),a1		| a1 = toaddr
	clrl	d0
	movl	sp@(12),d1		| count
	beq	Lcisdone		| nothing to copy
	subql	#1,d1			| predecrement for dbeq
Lcisloop:
	movsb	a0@+,d0			| copy a byte
	movb	d0,a1@+
	dbeq	d1,Lcisloop		| decrement low word of count
	beq	Lcisdone		| copied null, exit
	subil	#0x10000,d1		| decrement high word of count
	bcc	Lcisloop		| more room, keep going
	moveq	#ENAMETOOLONG,d0	| ran out of space
Lcisdone:
	tstl	sp@(16)			| length desired?
	beq	Lcisexit
	subl	sp@(4),a0		| yes, calculate length copied
	movl	sp@(16),a1		| store at return location
	movl	a0,a1@
Lcisexit:
	movl	_curpcb,a0		| clear fault handler
	clrl	a0@(PCB_ONFAULT)
	rts
Lcisfault:
	moveq	#EFAULT,d0
	bra	Lcisdone

/*
 * copyoutstr(caddr_t from, caddr_t to, size_t maxlen, size_t *lencopied);
 * Copy a NUL-terminated string, at most maxlen characters long, into the
 * user's address space.  Return the number of characters copied (including
 * the NUL) in *lencopied.  If the string is too long, return ENAMETOOLONG;
 * else return 0 or EFAULT.
 */
ENTRY(copyoutstr)
	CHECK_DFC
	movl	_curpcb,a0		| set fault handler
	movl	#Lcosfault,a0@(PCB_ONFAULT)
	movl	sp@(4),a0		| a0 = fromaddr
	movl	sp@(8),a1		| a1 = toaddr
	clrl	d0
	movl	sp@(12),d1		| count
	beq	Lcosdone		| nothing to copy
	subql	#1,d1			| predecrement for dbeq
Lcosloop:
	movb	a0@+,d0			| copy a byte
	movsb	d0,a1@+
	dbeq	d1,Lcosloop		| decrement low word of count
	beq	Lcosdone		| copied null, exit
	subil	#0x10000,d1		| decrement high word of count
	bcc	Lcosloop		| more room, keep going
	moveq	#ENAMETOOLONG,d0	| ran out of space
Lcosdone:
	tstl	sp@(16)			| length desired?
	beq	Lcosexit
	subl	sp@(4),a0		| yes, calculate length copied
	movl	sp@(16),a1		| store at return location
	movl	a0,a1@
Lcosexit:
	movl	_curpcb,a0		| clear fault handler
	clrl	a0@(PCB_ONFAULT)
	rts
Lcosfault:
	moveq	#EFAULT,d0
	bra	Lcosdone

/*
 * fuword(caddr_t uaddr);
 * Fetch an int from the user's address space.
 */
ENTRY(fuword)
	CHECK_SFC
	movl	sp@(4),a0		| address to read
	movl	_curpcb,a1		| set fault handler
	movl	#Lferr,a1@(PCB_ONFAULT)
	movsl	a0@,d0			| do read from user space
	bra	Lfdone

/*
 * fusword(caddr_t uaddr);
 * Fetch a short from the user's address space.
 */
ENTRY(fusword)
	CHECK_SFC
	movl	sp@(4),a0		| address to read
	movl	_curpcb,a1		| set fault handler
	movl	#Lferr,a1@(PCB_ONFAULT)
	moveq	#0,d0
	movsw	a0@,d0			| do read from user space
	bra	Lfdone

/*
 * fuswintr(caddr_t uaddr);
 * Fetch a short from the user's address space.
 * Can be called during an interrupt.
 */
ENTRY(fuswintr)
	CHECK_SFC
	movl	sp@(4),a0		| address to read
	movl	_curpcb,a1		| set fault handler
	movl	#_fubail,a1@(PCB_ONFAULT)
	moveq	#0,d0
	movsw	a0@,d0			| do read from user space
	bra	Lfdone

/*
 * fubyte(caddr_t uaddr);
 * Fetch a byte from the user's address space.
 */
ENTRY(fubyte)
	CHECK_SFC
	movl	sp@(4),a0		| address to read
	movl	_curpcb,a1		| set fault handler
	movl	#Lferr,a1@(PCB_ONFAULT)
	moveq	#0,d0
	movsb	a0@,d0			| do read from user space
	bra	Lfdone

/*
 * Error routine for fuswintr.  The fault handler in trap.c
 * checks for pcb_onfault set to this fault handler and
 * "bails out" before calling the VM fault handler.
 * (We can not call VM code from interrupt level.)
 * Same code as Lferr but must have a different address.
 */
ENTRY(fubail)
	nop
Lferr:
	moveq	#-1,d0			| error indicator
Lfdone:
	clrl	a1@(PCB_ONFAULT) 	| clear fault handler
	rts

/*
 * suword(caddr_t uaddr, int x);
 * Store an int in the user's address space.
 */
ENTRY(suword)
	CHECK_DFC
	movl	sp@(4),a0		| address to write
	movl	sp@(8),d0		| value to put there
	movl	_curpcb,a1		| set fault handler
	movl	#Lserr,a1@(PCB_ONFAULT)
	movsl	d0,a0@			| do write to user space
	moveq	#0,d0			| indicate no fault
	bra	Lsdone

/*
 * fusword(caddr_t uaddr);
 * Fetch a short from the user's address space.
 */
ENTRY(susword)
	CHECK_DFC
	movl	sp@(4),a0		| address to write
	movw	sp@(10),d0		| value to put there
	movl	_curpcb,a1		| set fault handler
	movl	#Lserr,a1@(PCB_ONFAULT)
	movsw	d0,a0@			| do write to user space
	moveq	#0,d0			| indicate no fault
	bra	Lsdone

/*
 * suswintr(caddr_t uaddr, short x);
 * Store a short in the user's address space.
 * Can be called during an interrupt.
 */
ENTRY(suswintr)
	CHECK_DFC
	movl	sp@(4),a0		| address to write
	movw	sp@(10),d0		| value to put there
	movl	_curpcb,a1		| set fault handler
	movl	#_subail,a1@(PCB_ONFAULT)
	movsw	d0,a0@			| do write to user space
	moveq	#0,d0			| indicate no fault
	bra	Lsdone

/*
 * subyte(caddr_t uaddr, char x);
 * Store a byte in the user's address space.
 */
ENTRY(subyte)
	CHECK_DFC
	movl	sp@(4),a0		| address to write
	movb	sp@(11),d0		| value to put there
	movl	_curpcb,a1		| set fault handler
	movl	#Lserr,a1@(PCB_ONFAULT)
	movsb	d0,a0@			| do write to user space
	moveq	#0,d0			| indicate no fault
	bra	Lsdone

/*
 * Error routine for suswintr.  The fault handler in trap.c
 * checks for pcb_onfault set to this fault handler and
 * "bails out" before calling the VM fault handler.
 * (We can not call VM code from interrupt level.)
 * Same code as Lserr but must have a different address.
 */
ENTRY(subail)
	nop
Lserr:
	moveq	#-1,d0			| error indicator
Lsdone:
	clrl	a1@(PCB_ONFAULT) 	| clear fault handler
	rts

/*
 * {ov}bcopy(from, to, len)
 * memcpy(to, from, len)
 *
 * Works for counts up to 128K.
 */
ENTRY(memcpy)
	movl	sp@(12),d0		| get count
	jeq	Lbccpyexit		| if zero, return
	movl	sp@(8), a0		| src address
	movl	sp@(4), a1		| dest address
	jra	Lbcdocopy		| jump into bcopy
ALTENTRY(ovbcopy, _bcopy)
ENTRY(bcopy)
	movl	sp@(12),d0		| get count
	jeq	Lbccpyexit		| if zero, return
	movl	sp@(4),a0		| src address
	movl	sp@(8),a1		| dest address
Lbcdocopy:
	cmpl	a1,a0			| src before dest?
	jlt	Lbccpyback		| yes, copy backwards (avoids overlap)
	movl	a0,d1
	btst	#0,d1			| src address odd?
	jeq	Lbccfeven		| no, go check dest
	movb	a0@+,a1@+		| yes, copy a byte
	subql	#1,d0			| update count
	jeq	Lbccpyexit		| exit if done
Lbccfeven:
	movl	a1,d1
	btst	#0,d1			| dest address odd?
	jne	Lbccfbyte		| yes, must copy by bytes
	movl	d0,d1			| no, get count
	lsrl	#2,d1			| convert to longwords
	jeq	Lbccfbyte		| no longwords, copy bytes
	subql	#1,d1			| set up for dbf
Lbccflloop:
	movl	a0@+,a1@+		| copy longwords
	dbf	d1,Lbccflloop		| til done
	andl	#3,d0			| get remaining count
	jeq	Lbccpyexit		| done if none
Lbccfbyte:
	subql	#1,d0			| set up for dbf
Lbccfbloop:
	movb	a0@+,a1@+		| copy bytes
	dbf	d0,Lbccfbloop		| til done
Lbccpyexit:
	rts
Lbccpyback:
	addl	d0,a0			| add count to src
	addl	d0,a1			| add count to dest
	movl	a0,d1
	btst	#0,d1			| src address odd?
	jeq	Lbccbeven		| no, go check dest
	movb	a0@-,a1@-		| yes, copy a byte
	subql	#1,d0			| update count
	jeq	Lbccpyexit		| exit if done
Lbccbeven:
	movl	a1,d1
	btst	#0,d1			| dest address odd?
	jne	Lbccbbyte		| yes, must copy by bytes
	movl	d0,d1			| no, get count
	lsrl	#2,d1			| convert to longwords
	jeq	Lbccbbyte		| no longwords, copy bytes
	subql	#1,d1			| set up for dbf
Lbccblloop:
	movl	a0@-,a1@-		| copy longwords
	dbf	d1,Lbccblloop		| til done
	andl	#3,d0			| get remaining count
	jeq	Lbccpyexit		| done if none
Lbccbbyte:
	subql	#1,d0			| set up for dbf
Lbccbbloop:
	movb	a0@-,a1@-		| copy bytes
	dbf	d0,Lbccbbloop		| til done
	rts
