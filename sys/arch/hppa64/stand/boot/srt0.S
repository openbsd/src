/*	$OpenBSD: srt0.S,v 1.1 2005/04/01 10:40:48 mickey Exp $	*/

/*
 * Copyright (c) 2005 Michael Shalayeff
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
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

#define	_LOCORE
#include <machine/iomod.h>
#include <machine/asm.h>
#include <machine/frame.h>

/*
 * This is the ending of the begin
 */
ENTRY(begin,0)

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
	sub	%r3,%r1,%r3		; Subtract to get # of bytes to copy
copyloop				; do
	ldwm	4(%r5),%r1		;   *r4++ = *r5++;
	addib,>= -4,%r3,copyloop	; while (--r3 >= 0);
	stwm	%r1,4(%r4)

	; here we zero the .bss
	ldil	L%__bss_start, %r4
	ldo	R%__bss_start(%r4), %r4
	ldil	L%__bss_end, %r3
	ldo	R%__bss_end(%r3), %r3
zeroloop
	combf,<,n %r3,%r4, zeroloop	; while (r4 < r3);
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
	mtsp	%r0,%sr0		; Set sr0 to kernel space.
	ldo	-1(%r21),%r21
	fdc	%r21(0,%r22)
loop	addib,>,n -16,%r21,loop		; Decrement by cache line size (16).
	fdc	%r21(%sr0,%r22)
	fdc	0(%sr0,%r22)		; Flush first word at addr to handle
	sync				;   arbitrary cache line boundary.
	nop				; Prevent prefetching.
	nop
	nop
	nop
	nop
	nop
	nop
	bv	0(%rp)
	nop
EXIT(begin)				/* jump to relocated code */

start
	ldil	L%HEAP_LIMIT, %sp
	ldo	R%HEAP_LIMIT(%sp), %sp

	.import	bootprompt, data
	ldil	L%bootprompt, %r1
	stw	%arg0, R%bootprompt(%r1)
	b	boot			; Call boot(),
	copy	%r0, %arg0		; use default boot device
	nop

/*
 * rtt - restart the box
 */
LEAF_ENTRY(_rtt)
	ldil    L%LBCAST_ADDR, %arg1
	ldi     CMD_RESET, %arg0
	stw     %arg0, R%iomod_command(%arg1)
forever                                 ; Loop until bus reset takes effect.
	b,n     forever

	bv	0(%rp)
	ldo     -48(%sp),%sp
EXIT(_rtt)

ENTRY(pdc_call,96)
	copy	%r3, %r1
	std	%rp, HPPA_FRAME_RP(%sp)
	copy	%sp, %r3
	std,ma	%r1, HPPA_FRAME_SIZE+8*4(%sp)

	copy	%arg0, %r1
	copy	%arg1, %arg0
	copy	%arg2, %arg1
	copy	%arg3, %arg2
	copy	arg4, %arg3
	stw	arg5, -(32 + 4*(4 + 1))(%sp)
	stw	arg6, -(32 + 4*(5 + 1))(%sp)
	stw	arg7, -(32 + 4*(6 + 1))(%sp)
	ldw	0(ap), arg5
	ldw	8(ap), arg6
	ldw	16(ap), arg7
	stw	arg5, -(32 + 4*(7 + 1))(%sp)
	stw	arg6, -(32 + 4*(8 + 1))(%sp)
	stw	arg7, -(32 + 4*(9 + 1))(%sp)

	.call
	blr	%r0, %rp
	bv,n	%r0(%r1)
	nop

	ldd	HPPA_FRAME_RP(%r3), %rp
	bv	%r0(%rp)
	ldd,mb	-(HPPA_FRAME_SIZE+8*4)(%sp), %r3
EXIT(pdc_call)

	.end
