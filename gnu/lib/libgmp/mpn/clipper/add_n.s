; Clipper __mpn_add_n -- Add two limb vectors of the same length > 0 and store
; sum in a third limb vector.

; Copyright (C) 1995 Free Software Foundation, Inc.

; This file is part of the GNU MP Library.

; The GNU MP Library is free software; you can redistribute it and/or modify
; it under the terms of the GNU Library General Public License as published by
; the Free Software Foundation; either version 2 of the License, or (at your
; option) any later version.

; The GNU MP Library is distributed in the hope that it will be useful, but
; WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
; or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Library General Public
; License for more details.

; You should have received a copy of the GNU Library General Public License
; along with the GNU MP Library; see the file COPYING.LIB.  If not, write to
; the Free Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
; MA 02111-1307, USA.

.text
	.align 16
.globl ___mpn_add_n
___mpn_add_n:
	subq	$8,sp
	storw	r6,(sp)
	loadw	12(sp),r2
	loadw	16(sp),r3
	loadq	$0,r6		; clear carry-save register

.Loop:	loadw	(r1),r4
	loadw	(r2),r5
	addwc	r6,r6		; restore carry from r6
	addwc	r5,r4
	storw	r4,(r0)
	subwc	r6,r6		; save carry in r6
	addq	$4,r0
	addq	$4,r1
	addq	$4,r2
	subq	$1,r3
	brne	.Loop

	negw	r6,r0
	loadw	(sp),r6
	addq	$8,sp
	ret	sp
