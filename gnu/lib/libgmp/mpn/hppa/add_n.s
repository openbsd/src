; HP-PA  __mpn_add_n -- Add two limb vectors of the same length > 0 and store
; sum in a third limb vector.

; Copyright (C) 1992, 1994 Free Software Foundation, Inc.

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


; INPUT PARAMETERS
; res_ptr	gr26
; s1_ptr	gr25
; s2_ptr	gr24
; size		gr23

; One might want to unroll this as for other processors, but it turns
; out that the data cache contention after a store makes such
; unrolling useless.  We can't come under 5 cycles/limb anyway.

	.code
	.export		__mpn_add_n
__mpn_add_n
	.proc
	.callinfo	frame=0,no_calls
	.entry

	ldws,ma		4(0,%r25),%r20
	ldws,ma		4(0,%r24),%r19

	addib,=		-1,%r23,L$end	; check for (SIZE == 1)
	 add		%r20,%r19,%r28	; add first limbs ignoring cy

L$loop	ldws,ma		4(0,%r25),%r20
	ldws,ma		4(0,%r24),%r19
	stws,ma		%r28,4(0,%r26)
	addib,<>	-1,%r23,L$loop
	 addc		%r20,%r19,%r28

L$end	stws		%r28,0(0,%r26)
	bv		0(%r2)
	 addc		%r0,%r0,%r28

	.exit
	.procend
