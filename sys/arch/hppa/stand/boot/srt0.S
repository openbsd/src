/*	$OpenBSD: srt0.S,v 1.1.1.1 1998/06/23 18:46:41 mickey Exp $	*/

/*
 * Copyright 1996 1995 by Open Software Foundation, Inc.   
 *              All Rights Reserved 
 *  
 * Permission to use, copy, modify, and distribute this software and 
 * its documentation for any purpose and without fee is hereby granted, 
 * provided that the above copyright notice appears in all copies and 
 * that both the copyright notice and this permission notice appear in 
 * supporting documentation. 
 *  
 * OSF DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE 
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS 
 * FOR A PARTICULAR PURPOSE. 
 *  
 * IN NO EVENT SHALL OSF BE LIABLE FOR ANY SPECIAL, INDIRECT, OR 
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM 
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT, 
 * NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION 
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. 
 * 
 */
/*
 * pmk1.1
 */
;
; Copyright (c) 1990 mt Xinu, Inc.  All rights reserved.
; Copyright (c) 1990 University of Utah.  All rights reserved.
;
; This file may be freely distributed in any form as long as
; this copyright notice is included.
; THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
; IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
; WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
;
;	Utah $Hdr: srt0.c 1.3 94/12/13$
;

;
; Startup code for standalone HP700 system.
;

#include <sys/reboot.h>

;
; Define our Stack Unwind spaces/variables.
;
	.SPACE	$TEXT$
;	.SUBSPA	$UNWIND_START$,QUAD=0,ALIGN=8,ACCESS=0x2c,SORT=56
	.EXPORT	$UNWIND_START
$UNWIND_START
;	.SUBSPA	$UNWIND_END$,QUAD=0,ALIGN=8,ACCESS=0x2c,SORT=73
	.EXPORT	$UNWIND_END
$UNWIND_END


	.space  $PRIVATE$
	.subspa $DATA$
	.import howto,data
	.import rstaddr,data
	.import stack_base,data

;
; Execution begins here.
;
; We are called by the PDC as:
;
;	begin(interactive, endaddr)
;
; Where:
;
;	interactive - 0 if not interactive, 1 if interactive.
;
	.SPACE	$TEXT$
;	.SUBSPA	$FIRST$
	.EXPORT	begin,entry
	.IMPORT	boot,code
	.IMPORT	pdc_init,code
	.IMPORT	end,DATA

begin
	blr	%r0,%r5			; Get address of 'boff' into 'r5',
	ldo	begin-boff(%r5),%r5	;   and subtract to get 'begin'.
boff
	ldil	L%RELOC,%r4
	ldo	R%RELOC(%r4),%r4
	ldo	start-begin(%r4),%rp
	ldil	L%edata,%r3
	ldo	R%edata(%r3),%r3	; Get address of edata.
	ldil	L%begin,%r1
	ldo	R%begin(%r1),%r1	; Get address of begin
	sub	%r3,%r1,%r3		; Subtract to get # to bytes to copy
copyloop				; do
	ldwm	4(%r5),%r1		;   *r4++ = *r5++;
	addib,>= -4,%r3,copyloop	; while (--r3 >= 0);
	stwm	%r1,4(%r4)

	; here we zero the .bss
	ldil	L%end, %r3		; the end af all
	ldil	L%__bss_start, %r4	; .bss is here
	sub	%r3,%r4,%r3
zeroloop
	addib,>= -4,%r3,zeroloop	; while (--r3 >= 0);
	stwm	%r0,4(%r4)		;	*r4++ = 0;

	ldil	L%$global$,%dp
	ldo	R%$global$(%dp),%dp
	ldil	L%start,%r1
	ldo	R%start(%r1),%r1
	sub	%dp,%r1,%dp		; Subtract to get difference
	add	%rp,%dp,%dp		;   and relocate it.

;
; We have relocated ourself to RELOC.  If we are running on a machine
; with separate instruction and data caches, we must flush our data
; cache before trying to execute the code starting at rp.
;
	ldil	L%RELOC,%r22		; Set %t1 to start of relocated code.
	ldo	R%RELOC(%r22),%r22
	ldil	L%edata,%r21		; Set r21 to address of edata
	ldo	R%edata(%r21),%r21
	ldil	L%begin,%r1		; set %r1 to address of begin
	ldo	R%begin(%r1),%r1
	sub	%r21,%r1,%r21		; Subtract to get length
	mtsp	%r0,%sr0			; Set sr0 to kernel space.
	ldo	-1(%r21),%r21
	fdc	%r21(0,%r22)
loop	addib,>,n -16,%r21,loop		; Decrement by cache line size (16).
	fdc	%r21(0,%r22)
	fdc	0(0,%r22)		; Flush first word at addr to handle
	sync				;   arbitrary cache line boundary.
	nop				; Prevent prefetching.
	nop
	nop
	nop
	nop
	nop
	nop
	bv	%r0(%rp)		; Jump to relocated start
	stw	%rp,rstaddr-$global$(%dp) ;   saving address for _rtt.

start
	ldil	L%stack_base,%sp
	ldo	R%stack_base(%sp),%sp
	dep	%r0,31,6,%sp		;   and ensure maximum alignment.	

;	bl	pdc_init,%rp		; Initialize PDC and related variables
;	ldo	64(%sp),%sp		;   and push our first stack frame.

	b	boot			; Call boot(),
	ldw	rstaddr-$global$(%dp),%rp ;   a return will go back to start().

;
; rtt - restart boot device selection (after ^C, etc).
;
	.IMPORT	howto,DATA
	.IMPORT	rstaddr,DATA
	.EXPORT	_rtt
_rtt
	ldi	RB_ASKNAME+RB_SINGLE,%r1     ; Restarts get RB_SINGLE|RB_ASKNAME
	stw	%r1,howto-$global$(%dp)		;   and save in 'howto'.
	ldw	rstaddr-$global$(%dp),%rp	; Load restart address into 'rp'
	bv,n	%r0(%rp)			;   and branch to it.
	or	%r0,%r0,%r0
#if 0
	.EXPORT	execute,entry
	.IMPORT	pdc,DATA
	.PROC
	.CALLINFO
	.ENTRY
execute	
	mtsm	%r0			; Disable traps and interrupts.
	mtctl	%r0,%cr17		; Clear two-level IIA Space Queue
	mtctl	%r0,%cr17		;    effectively setting kernel space.
	mtctl	%arg0,%cr18		; Stuff entry point into head of IIA
	ldo	4(%arg0),%arg0		;    Offset Queue, and entry point + 4
	mtctl	%arg0,%cr18		;    into tail of IIA Offset Queue.
	ldi	0x9,%arg0		; Set PSW Q & I bits (collect intrpt
	mtctl	%arg0,%ipsw		;    state, allow external intrpts).
	copy	%arg2,%arg0
	.EXIT
	rfi				; Begin execution of kernel.
	nop
	.PROCEND

	.export getdp
getdp	.proc
	.callinfo

	bv	0(%rp)
	or	%dp,%r0,%ret0

	.procend

	.export getsp
getsp	.proc
	.callinfo

	bv	0(%rp)
	or	%sp,%r0,%ret0

	.procend
#endif
	.SPACE	$PRIVATE$
;	.SUBSPA	$GLOBAL$
	.EXPORT	$global$
$global$
	.WORD	0

	.end
