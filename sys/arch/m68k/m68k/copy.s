/*	$OpenBSD: copy.s,v 1.14 2003/01/09 22:27:09 miod Exp $	*/
/*	$NetBSD: copy.s,v 1.30 1998/03/04 06:39:14 thorpej Exp $	*/

/*-
 * Copyright (c) 1998 Jason R. Thorpe.  All rights reserved.
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

/*
 * This file contains the functions for user-space access:
 * copyin/copyout, etc.
 */

#include <sys/errno.h>
#include <machine/asm.h>

#include "assym.h"

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
Lbadfc:
	PANIC("copy.s: bad sfc or dfc")
	bra	Lbadfc
#define	CHECK_SFC	movec sfc,d0; subql #FC_USERD,d0; bne Lbadfc
#define	CHECK_DFC	movec dfc,d0; subql #FC_USERD,d0; bne Lbadfc
#else	/* DIAGNOSTIC */
#define	CHECK_SFC
#define	CHECK_DFC
#endif	/* DIAGNOSTIC */

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
	cmpl	_C_LABEL(mappedcopysize),d0 | size >= mappedcopysize
	bcc	_C_LABEL(mappedcopyin)	| yes, go do it the new way
#endif
	movl	d2,sp@-			| save scratch register
	movl	_C_LABEL(curpcb),a0	| set fault handler
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
	movl	_C_LABEL(curpcb),a0	| clear fault handler
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
	cmpl	_C_LABEL(mappedcopysize),d0 | size >= mappedcopysize
	bcc	_C_LABEL(mappedcopyout)	| yes, go do it the new way
#endif
	movl	d2,sp@-			| save scratch register
	movl	_C_LABEL(curpcb),a0	| set fault handler
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
	movl	_C_LABEL(curpcb),a0	| clear fault handler
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
	movl	_C_LABEL(curpcb),a0	| set fault handler
	movl	#Lcisfault,a0@(PCB_ONFAULT)
	movl	sp@(4),a0		| a0 = fromaddr
	movl	sp@(8),a1		| a1 = toaddr
	clrl	d0
	movl	sp@(12),d1		| count
	beq	Lcistoolong		| nothing to copy
	subql	#1,d1			| predecrement for dbeq
Lcisloop:
	movsb	a0@+,d0			| copy a byte
	movb	d0,a1@+
	dbeq	d1,Lcisloop		| decrement low word of count
	beq	Lcisdone		| copied null, exit
	subil	#0x10000,d1		| decrement high word of count
	bcc	Lcisloop		| more room, keep going
Lcistoolong:
	moveq	#ENAMETOOLONG,d0	| ran out of space
Lcisnull:
	cmpl	sp@(8),a1		| do not attempt to clear last byte
	beq	Lcisdone		| if we faulted on first write
	subql	#1, a1
	clrb	a1@+			| clear last byte
Lcisdone:
	tstl	sp@(16)			| length desired?
	beq	Lcisexit
	subl	sp@(4),a0		| yes, calculate length copied
	movl	sp@(16),a1		| store at return location
	movl	a0,a1@
Lcisexit:
	movl	_C_LABEL(curpcb),a0	| clear fault handler
	clrl	a0@(PCB_ONFAULT)
	rts
Lcisfault:
	moveq	#EFAULT,d0
	bra	Lcisnull

/*
 * copyoutstr(caddr_t from, caddr_t to, size_t maxlen, size_t *lencopied);
 * Copy a NUL-terminated string, at most maxlen characters long, into the
 * user's address space.  Return the number of characters copied (including
 * the NUL) in *lencopied.  If the string is too long, return ENAMETOOLONG;
 * else return 0 or EFAULT.
 */
ENTRY(copyoutstr)
	CHECK_DFC
	movl	_C_LABEL(curpcb),a0	| set fault handler
	movl	#Lcosfault,a0@(PCB_ONFAULT)
	movl	sp@(4),a0		| a0 = fromaddr
	movl	sp@(8),a1		| a1 = toaddr
	clrl	d0
	movl	sp@(12),d1		| count
	beq	Lcostoolong		| nothing to copy
	subql	#1,d1			| predecrement for dbeq
Lcosloop:
	movb	a0@+,d0			| copy a byte
	movsb	d0,a1@+
	dbeq	d1,Lcosloop		| decrement low word of count
	beq	Lcosdone		| copied null, exit
	subil	#0x10000,d1		| decrement high word of count
	bcc	Lcosloop		| more room, keep going
Lcostoolong:
	moveq	#ENAMETOOLONG,d0	| ran out of space
Lcosdone:
	tstl	sp@(16)			| length desired?
	beq	Lcosexit
	subl	sp@(4),a0		| yes, calculate length copied
	movl	sp@(16),a1		| store at return location
	movl	a0,a1@
Lcosexit:
	movl	_C_LABEL(curpcb),a0	| clear fault handler
	clrl	a0@(PCB_ONFAULT)
	rts
Lcosfault:
	moveq	#EFAULT,d0
	bra	Lcosdone

/*
 * kcopy(const void *src, void *dst, size_t len);
 *
 * Copy len bytes from src to dst, aborting if we encounter a fatal
 * page fault.
 *
 * kcopy() _must_ save and restore the old fault handler since it is
 * called by uiomove(), which may be in the path of servicing a non-fatal
 * page fault.
 */
ENTRY(kcopy)
	link	a6,#-4
	movl	_C_LABEL(curpcb),a0	 | set fault handler
	movl	a0@(PCB_ONFAULT),a6@(-4) | save old handler first
	movl    #Lkcfault,a0@(PCB_ONFAULT)
	movl    a6@(16),sp@-		| push len
	movl    a6@(12),sp@-		| push dst
	movl    a6@(8),sp@-		| push src
	jbsr    _C_LABEL(bcopy)		| copy it
	addl    #12,sp			| pop args
	clrl    d0			| success!
Lkcdone:
	movl	_C_LABEL(curpcb),a0	| restore fault handler
	movl	a6@(-4),a0@(PCB_ONFAULT)
	unlk    a6
	rts
Lkcfault:
	addl    #16,sp			| pop args and return address
	moveq   #EFAULT,d0		| indicate a fault
	bra     Lkcdone
