/*	$OpenBSD: mulI.S,v 1.1.1.1 1998/06/23 18:56:54 mickey Exp $	*/

/*
 *  (c) Copyright 1985 HEWLETT-PACKARD COMPANY
 *
 *  To anyone who acknowledges that this file is provided "AS IS"
 *  without any express or implied warranty:
 *      permission to use, copy, modify, and distribute this file
 *  for any purpose is hereby granted without fee, provided that
 *  the above copyright notice and this notice appears in all
 *  copies, and that the name of Hewlett-Packard Company not be
 *  used in advertising or publicity pertaining to distribution
 *  of the software without specific, written prior permission.
 *  Hewlett-Packard Company makes no representations about the
 *  suitability of this software for any purpose.
 */

#include "prefix.h"

/*
 * Author: Dan Zuras
 * Editor: Pratap Subrahmanyam
 * 
 * ROUTINE:	$$mulI
 * 
 * 
 * DESCRIPTION:	
 * 
 * 	$$mulI multiplies two single word integers, giving a single 
 * 	word result.  
 * 
 * 
 * INPUT REGISTERS:
 * 
 * 	arg0 = Operand 1
 * 	arg1 = Operand 2
 * 	r31  == return pc
 * 	sr0  == return space when called externally 
 * 
 * 
 * OUTPUT REGISTERS:
 * 
 * 	arg0 = undefined
 * 	arg1 = undefined
 * 	ret1 = result 
 * 
 * OTHER REGISTERS AFFECTED:
 * 
 * 	r1   = undefined
 * 
 * SIDE EFFECTS:
 * 
 * 	Causes a trap under the following conditions:  NONE
 * 	Changes memory at the following places:  NONE
 * 
 * PERMISSIBLE CONTEXT:
 * 
 * 	Unwindable
 * 	Does not create a stack frame
 * 	Is usable for internal or external microcode
 * 
 * DISCUSSION:
 * 
 * 	Calls other millicode routines via r31:  NONE
 * 	Calls other millicode routines:  NONE
 * 
 */


#define	a0	%arg0
#define	a1	%arg1
#define	rtn	%mrp
#define	t0	%r1
#define	r	%ret1

#define	a0__128a0	zdep	a0,24,25,a0
#define	a0__256a0	zdep	a0,23,24,a0
#define	a1_ne_0_b_l0	comb,<>	a1,0,l0
#define	a1_ne_0_b_l1	comb,<>	a1,0,l1
#define	a1_ne_0_b_l2	comb,<>	a1,0,l2
#define	b_n_ret_t0	b,n	ret_t0
#define	b_e_shift	b	e_shift
#define	b_e_t0ma0	b	e_t0ma0
#define	b_e_t0		b	e_t0
#define	b_e_t0a0	b	e_t0a0
#define	b_e_t02a0	b	e_t02a0
#define	b_e_t04a0	b	e_t04a0
#define	b_e_2t0		b	e_2t0
#define	b_e_2t0a0	b	e_2t0a0
#define	b_e_2t04a0	b	e2t04a0
#define	b_e_3t0		b	e_3t0
#define	b_e_4t0		b	e_4t0
#define	b_e_4t0a0	b	e_4t0a0
#define	b_e_4t08a0	b	e4t08a0
#define	b_e_5t0		b	e_5t0
#define	b_e_8t0		b	e_8t0
#define	b_e_8t0a0	b	e_8t0a0
#define	r__r_a0		add	r,a0,r
#define	r__r_2a0	sh1add	a0,r,r
#define	r__r_4a0	sh2add	a0,r,r
#define	r__r_8a0	sh3add	a0,r,r
#define	r__r_t0		add	r,t0,r
#define	r__r_2t0	sh1add	t0,r,r
#define	r__r_4t0	sh2add	t0,r,r
#define	r__r_8t0	sh3add	t0,r,r
; Use MILLIRET and MILLIRETN instead.
; #define	return		bv	0(rtn)
; #define	return_n	bv,n	0(rtn)
#define	t0__3a0		sh1add	a0,a0,t0
#define	t0__4a0		sh2add	a0,0,t0
#define	t0__5a0		sh2add	a0,a0,t0
#define	t0__8a0		sh3add	a0,0,t0
#define	t0__9a0		sh3add	a0,a0,t0
#define	t0__16a0	zdep	a0,27,28,t0
#define	t0__32a0	zdep	a0,26,27,t0
#define	t0__64a0	zdep	a0,25,26,t0
#define	t0__128a0	zdep	a0,24,25,t0
#define	t0__t0ma0	sub	t0,a0,t0
#define	t0__t0_a0	add	t0,a0,t0
#define	t0__t0_2a0	sh1add	a0,t0,t0
#define	t0__t0_4a0	sh2add	a0,t0,t0
#define	t0__t0_8a0	sh3add	a0,t0,t0
#define	t0__2t0_a0	sh1add	t0,a0,t0
#define	t0__3t0		sh1add	t0,t0,t0
#define	t0__4t0		sh2add	t0,0,t0
#define	t0__4t0_a0	sh2add	t0,a0,t0
#define	t0__5t0		sh2add	t0,t0,t0
#define	t0__8t0		sh3add	t0,0,t0
#define	t0__8t0_a0	sh3add	t0,a0,t0
#define	t0__9t0		sh3add	t0,t0,t0
#define	t0__16t0	zdep	t0,27,28,t0
#define	t0__32t0	zdep	t0,26,27,t0
#define	t0__256a0	zdep	a0,23,24,t0


	.subspa $MILLICODE$
	.align 16
	.proc
	.callinfo millicode
	.export $$mulI, millicode
$$mulI	
	combt,<<=	a1,a0,l4	; swap args if unsigned a1>a0
	copy		0,r		; zero out the result
	xor		a0,a1,a0	; swap a0 & a1 using the
	xor		a0,a1,a1	;  old xor trick
	xor		a0,a1,a0
l4	combt,<=	0,a0,l3		; if a0>=0 then proceed like unsigned
	zdep		a1,30,8,t0	; t0 = (a1&0xff)<<1 *********
	sub,>		0,a1,t0		; otherwise negate both and
	combt,<=,n	a0,t0,l2	;  swap back if |a0|<|a1|
	sub		0,a0,a1
	movb,tr,n	t0,a0,l2	; 10th inst.

l0	r__r_t0				; add in this partial product
l1	a0__256a0			; a0 <<= 8 ******************
l2	zdep		a1,30,8,t0	; t0 = (a1&0xff)<<1 *********
l3	blr		t0,0		; case on these 8 bits ******
	extru		a1,23,24,a1	; a1 >>= 8 ******************

;16 insts before this.
;			  a0 <<= 8 **************************
x0	a1_ne_0_b_l2	! a0__256a0	! MILLIRETN	! nop
x1	a1_ne_0_b_l1	! r__r_a0	! MILLIRETN	! nop
x2	a1_ne_0_b_l1	! r__r_2a0	! MILLIRETN	! nop
x3	a1_ne_0_b_l0	! t0__3a0	! MILLIRET	! r__r_t0
x4	a1_ne_0_b_l1	! r__r_4a0	! MILLIRETN	! nop
x5	a1_ne_0_b_l0	! t0__5a0	! MILLIRET	! r__r_t0
x6	t0__3a0		! a1_ne_0_b_l1	! r__r_2t0	! MILLIRETN
x7	t0__3a0		! a1_ne_0_b_l0	! r__r_4a0	! b_n_ret_t0
x8	a1_ne_0_b_l1	! r__r_8a0	! MILLIRETN	! nop
x9	a1_ne_0_b_l0	! t0__9a0	! MILLIRET	! r__r_t0
x10	t0__5a0		! a1_ne_0_b_l1	! r__r_2t0	! MILLIRETN
x11	t0__3a0		! a1_ne_0_b_l0	! r__r_8a0	! b_n_ret_t0
x12	t0__3a0		! a1_ne_0_b_l1	! r__r_4t0	! MILLIRETN
x13	t0__5a0		! a1_ne_0_b_l0	! r__r_8a0	! b_n_ret_t0
x14	t0__3a0		! t0__2t0_a0	! b_e_shift	! r__r_2t0
x15	t0__5a0		! a1_ne_0_b_l0	! t0__3t0	! b_n_ret_t0
x16	t0__16a0	! a1_ne_0_b_l1	! r__r_t0	! MILLIRETN
x17	t0__9a0		! a1_ne_0_b_l0	! t0__t0_8a0	! b_n_ret_t0
x18	t0__9a0		! a1_ne_0_b_l1	! r__r_2t0	! MILLIRETN
x19	t0__9a0		! a1_ne_0_b_l0	! t0__2t0_a0	! b_n_ret_t0
x20	t0__5a0		! a1_ne_0_b_l1	! r__r_4t0	! MILLIRETN
x21	t0__5a0		! a1_ne_0_b_l0	! t0__4t0_a0	! b_n_ret_t0
x22	t0__5a0		! t0__2t0_a0	! b_e_shift	! r__r_2t0
x23	t0__5a0		! t0__2t0_a0	! b_e_t0	! t0__2t0_a0
x24	t0__3a0		! a1_ne_0_b_l1	! r__r_8t0	! MILLIRETN
x25	t0__5a0		! a1_ne_0_b_l0	! t0__5t0	! b_n_ret_t0
x26	t0__3a0		! t0__4t0_a0	! b_e_shift	! r__r_2t0
x27	t0__3a0		! a1_ne_0_b_l0	! t0__9t0	! b_n_ret_t0
x28	t0__3a0		! t0__2t0_a0	! b_e_shift	! r__r_4t0
x29	t0__3a0		! t0__2t0_a0	! b_e_t0	! t0__4t0_a0
x30	t0__5a0		! t0__3t0	! b_e_shift	! r__r_2t0
x31	t0__32a0	! a1_ne_0_b_l0	! t0__t0ma0	! b_n_ret_t0
x32	t0__32a0	! a1_ne_0_b_l1	! r__r_t0	! MILLIRETN
x33	t0__8a0		! a1_ne_0_b_l0	! t0__4t0_a0	! b_n_ret_t0
x34	t0__16a0	! t0__t0_a0	! b_e_shift	! r__r_2t0
x35	t0__9a0		! t0__3t0	! b_e_t0	! t0__t0_8a0
x36	t0__9a0		! a1_ne_0_b_l1	! r__r_4t0	! MILLIRETN
x37	t0__9a0		! a1_ne_0_b_l0	! t0__4t0_a0	! b_n_ret_t0
x38	t0__9a0		! t0__2t0_a0	! b_e_shift	! r__r_2t0
x39	t0__9a0		! t0__2t0_a0	! b_e_t0	! t0__2t0_a0
x40	t0__5a0		! a1_ne_0_b_l1	! r__r_8t0	! MILLIRETN
x41	t0__5a0		! a1_ne_0_b_l0	! t0__8t0_a0	! b_n_ret_t0
x42	t0__5a0		! t0__4t0_a0	! b_e_shift	! r__r_2t0
x43	t0__5a0		! t0__4t0_a0	! b_e_t0	! t0__2t0_a0
x44	t0__5a0		! t0__2t0_a0	! b_e_shift	! r__r_4t0
x45	t0__9a0		! a1_ne_0_b_l0	! t0__5t0	! b_n_ret_t0
x46	t0__9a0		! t0__5t0	! b_e_t0	! t0__t0_a0
x47	t0__9a0		! t0__5t0	! b_e_t0	! t0__t0_2a0
x48	t0__3a0		! a1_ne_0_b_l0	! t0__16t0	! b_n_ret_t0
x49	t0__9a0		! t0__5t0	! b_e_t0	! t0__t0_4a0
x50	t0__5a0		! t0__5t0	! b_e_shift	! r__r_2t0
x51	t0__9a0		! t0__t0_8a0	! b_e_t0	! t0__3t0
x52	t0__3a0		! t0__4t0_a0	! b_e_shift	! r__r_4t0
x53	t0__3a0		! t0__4t0_a0	! b_e_t0	! t0__4t0_a0
x54	t0__9a0		! t0__3t0	! b_e_shift	! r__r_2t0
x55	t0__9a0		! t0__3t0	! b_e_t0	! t0__2t0_a0
x56	t0__3a0		! t0__2t0_a0	! b_e_shift	! r__r_8t0
x57	t0__9a0		! t0__2t0_a0	! b_e_t0	! t0__3t0
x58	t0__3a0		! t0__2t0_a0	! b_e_2t0	! t0__4t0_a0
x59	t0__9a0		! t0__2t0_a0	! b_e_t02a0	! t0__3t0
x60	t0__5a0		! t0__3t0	! b_e_shift	! r__r_4t0
x61	t0__5a0		! t0__3t0	! b_e_t0	! t0__4t0_a0
x62	t0__32a0	! t0__t0ma0	! b_e_shift	! r__r_2t0
x63	t0__64a0	! a1_ne_0_b_l0	! t0__t0ma0	! b_n_ret_t0
x64	t0__64a0	! a1_ne_0_b_l1	! r__r_t0	! MILLIRETN
x65	t0__8a0		! a1_ne_0_b_l0	! t0__8t0_a0	! b_n_ret_t0
x66	t0__32a0	! t0__t0_a0	! b_e_shift	! r__r_2t0
x67	t0__8a0		! t0__4t0_a0	! b_e_t0	! t0__2t0_a0
x68	t0__8a0		! t0__2t0_a0	! b_e_shift	! r__r_4t0
x69	t0__8a0		! t0__2t0_a0	! b_e_t0	! t0__4t0_a0
x70	t0__64a0	! t0__t0_4a0	! b_e_t0	! t0__t0_2a0
x71	t0__9a0		! t0__8t0	! b_e_t0	! t0__t0ma0
x72	t0__9a0		! a1_ne_0_b_l1	! r__r_8t0	! MILLIRETN
x73	t0__9a0		! t0__8t0_a0	! b_e_shift	! r__r_t0
x74	t0__9a0		! t0__4t0_a0	! b_e_shift	! r__r_2t0
x75	t0__9a0		! t0__4t0_a0	! b_e_t0	! t0__2t0_a0
x76	t0__9a0		! t0__2t0_a0	! b_e_shift	! r__r_4t0
x77	t0__9a0		! t0__2t0_a0	! b_e_t0	! t0__4t0_a0
x78	t0__9a0		! t0__2t0_a0	! b_e_2t0	! t0__2t0_a0
x79	t0__16a0	! t0__5t0	! b_e_t0	! t0__t0ma0
x80	t0__16a0	! t0__5t0	! b_e_shift	! r__r_t0
x81	t0__9a0		! t0__9t0	! b_e_shift	! r__r_t0
x82	t0__5a0		! t0__8t0_a0	! b_e_shift	! r__r_2t0
x83	t0__5a0		! t0__8t0_a0	! b_e_t0	! t0__2t0_a0
x84	t0__5a0		! t0__4t0_a0	! b_e_shift	! r__r_4t0
x85	t0__8a0		! t0__2t0_a0	! b_e_t0	! t0__5t0
x86	t0__5a0		! t0__4t0_a0	! b_e_2t0	! t0__2t0_a0
x87	t0__9a0		! t0__9t0	! b_e_t02a0	! t0__t0_4a0
x88	t0__5a0		! t0__2t0_a0	! b_e_shift	! r__r_8t0
x89	t0__5a0		! t0__2t0_a0	! b_e_t0	! t0__8t0_a0
x90	t0__9a0		! t0__5t0	! b_e_shift	! r__r_2t0
x91	t0__9a0		! t0__5t0	! b_e_t0	! t0__2t0_a0
x92	t0__5a0		! t0__2t0_a0	! b_e_4t0	! t0__2t0_a0
x93	t0__32a0	! t0__t0ma0	! b_e_t0	! t0__3t0
x94	t0__9a0		! t0__5t0	! b_e_2t0	! t0__t0_2a0
x95	t0__9a0		! t0__2t0_a0	! b_e_t0	! t0__5t0
x96	t0__8a0		! t0__3t0	! b_e_shift	! r__r_4t0
x97	t0__8a0		! t0__3t0	! b_e_t0	! t0__4t0_a0
x98	t0__32a0	! t0__3t0	! b_e_t0	! t0__t0_2a0
x99	t0__8a0		! t0__4t0_a0	! b_e_t0	! t0__3t0
x100	t0__5a0		! t0__5t0	! b_e_shift	! r__r_4t0
x101	t0__5a0		! t0__5t0	! b_e_t0	! t0__4t0_a0
x102	t0__32a0	! t0__t0_2a0	! b_e_t0	! t0__3t0
x103	t0__5a0		! t0__5t0	! b_e_t02a0	! t0__4t0_a0
x104	t0__3a0		! t0__4t0_a0	! b_e_shift	! r__r_8t0
x105	t0__5a0		! t0__4t0_a0	! b_e_t0	! t0__5t0
x106	t0__3a0		! t0__4t0_a0	! b_e_2t0	! t0__4t0_a0
x107	t0__9a0		! t0__t0_4a0	! b_e_t02a0	! t0__8t0_a0
x108	t0__9a0		! t0__3t0	! b_e_shift	! r__r_4t0
x109	t0__9a0		! t0__3t0	! b_e_t0	! t0__4t0_a0
x110	t0__9a0		! t0__3t0	! b_e_2t0	! t0__2t0_a0
x111	t0__9a0		! t0__4t0_a0	! b_e_t0	! t0__3t0
x112	t0__3a0		! t0__2t0_a0	! b_e_t0	! t0__16t0
x113	t0__9a0		! t0__4t0_a0	! b_e_t02a0	! t0__3t0
x114	t0__9a0		! t0__2t0_a0	! b_e_2t0	! t0__3t0
x115	t0__9a0		! t0__2t0_a0	! b_e_2t0a0	! t0__3t0
x116	t0__3a0		! t0__2t0_a0	! b_e_4t0	! t0__4t0_a0
x117	t0__3a0		! t0__4t0_a0	! b_e_t0	! t0__9t0
x118	t0__3a0		! t0__4t0_a0	! b_e_t0a0	! t0__9t0
x119	t0__3a0		! t0__4t0_a0	! b_e_t02a0	! t0__9t0
x120	t0__5a0		! t0__3t0	! b_e_shift	! r__r_8t0
x121	t0__5a0		! t0__3t0	! b_e_t0	! t0__8t0_a0
x122	t0__5a0		! t0__3t0	! b_e_2t0	! t0__4t0_a0
x123	t0__5a0		! t0__8t0_a0	! b_e_t0	! t0__3t0
x124	t0__32a0	! t0__t0ma0	! b_e_shift	! r__r_4t0
x125	t0__5a0		! t0__5t0	! b_e_t0	! t0__5t0
x126	t0__64a0	! t0__t0ma0	! b_e_shift	! r__r_2t0
x127	t0__128a0	! a1_ne_0_b_l0	! t0__t0ma0	! b_n_ret_t0
x128	t0__128a0	! a1_ne_0_b_l1	! r__r_t0	! MILLIRETN
x129	t0__128a0	! a1_ne_0_b_l0	! t0__t0_a0	! b_n_ret_t0
x130	t0__64a0	! t0__t0_a0	! b_e_shift	! r__r_2t0
x131	t0__8a0		! t0__8t0_a0	! b_e_t0	! t0__2t0_a0
x132	t0__8a0		! t0__4t0_a0	! b_e_shift	! r__r_4t0
x133	t0__8a0		! t0__4t0_a0	! b_e_t0	! t0__4t0_a0
x134	t0__8a0		! t0__4t0_a0	! b_e_2t0	! t0__2t0_a0
x135	t0__9a0		! t0__5t0	! b_e_t0	! t0__3t0
x136	t0__8a0		! t0__2t0_a0	! b_e_shift	! r__r_8t0
x137	t0__8a0		! t0__2t0_a0	! b_e_t0	! t0__8t0_a0
x138	t0__8a0		! t0__2t0_a0	! b_e_2t0	! t0__4t0_a0
x139	t0__8a0		! t0__2t0_a0	! b_e_2t0a0	! t0__4t0_a0
x140	t0__3a0		! t0__2t0_a0	! b_e_4t0	! t0__5t0
x141	t0__8a0		! t0__2t0_a0	! b_e_4t0a0	! t0__2t0_a0
x142	t0__9a0		! t0__8t0	! b_e_2t0	! t0__t0ma0
x143	t0__16a0	! t0__9t0	! b_e_t0	! t0__t0ma0
x144	t0__9a0		! t0__8t0	! b_e_shift	! r__r_2t0
x145	t0__9a0		! t0__8t0	! b_e_t0	! t0__2t0_a0
x146	t0__9a0		! t0__8t0_a0	! b_e_shift	! r__r_2t0
x147	t0__9a0		! t0__8t0_a0	! b_e_t0	! t0__2t0_a0
x148	t0__9a0		! t0__4t0_a0	! b_e_shift	! r__r_4t0
x149	t0__9a0		! t0__4t0_a0	! b_e_t0	! t0__4t0_a0
x150	t0__9a0		! t0__4t0_a0	! b_e_2t0	! t0__2t0_a0
x151	t0__9a0		! t0__4t0_a0	! b_e_2t0a0	! t0__2t0_a0
x152	t0__9a0		! t0__2t0_a0	! b_e_shift	! r__r_8t0
x153	t0__9a0		! t0__2t0_a0	! b_e_t0	! t0__8t0_a0
x154	t0__9a0		! t0__2t0_a0	! b_e_2t0	! t0__4t0_a0
x155	t0__32a0	! t0__t0ma0	! b_e_t0	! t0__5t0
x156	t0__9a0		! t0__2t0_a0	! b_e_4t0	! t0__2t0_a0
x157	t0__32a0	! t0__t0ma0	! b_e_t02a0	! t0__5t0
x158	t0__16a0	! t0__5t0	! b_e_2t0	! t0__t0ma0
x159	t0__32a0	! t0__5t0	! b_e_t0	! t0__t0ma0
x160	t0__5a0		! t0__4t0	! b_e_shift	! r__r_8t0
x161	t0__8a0		! t0__5t0	! b_e_t0	! t0__4t0_a0
x162	t0__9a0		! t0__9t0	! b_e_shift	! r__r_2t0
x163	t0__9a0		! t0__9t0	! b_e_t0	! t0__2t0_a0
x164	t0__5a0		! t0__8t0_a0	! b_e_shift	! r__r_4t0
x165	t0__8a0		! t0__4t0_a0	! b_e_t0	! t0__5t0
x166	t0__5a0		! t0__8t0_a0	! b_e_2t0	! t0__2t0_a0
x167	t0__5a0		! t0__8t0_a0	! b_e_2t0a0	! t0__2t0_a0
x168	t0__5a0		! t0__4t0_a0	! b_e_shift	! r__r_8t0
x169	t0__5a0		! t0__4t0_a0	! b_e_t0	! t0__8t0_a0
x170	t0__32a0	! t0__t0_2a0	! b_e_t0	! t0__5t0
x171	t0__9a0		! t0__2t0_a0	! b_e_t0	! t0__9t0
x172	t0__5a0		! t0__4t0_a0	! b_e_4t0	! t0__2t0_a0
x173	t0__9a0		! t0__2t0_a0	! b_e_t02a0	! t0__9t0
x174	t0__32a0	! t0__t0_2a0	! b_e_t04a0	! t0__5t0
x175	t0__8a0		! t0__2t0_a0	! b_e_5t0	! t0__2t0_a0
x176	t0__5a0		! t0__4t0_a0	! b_e_8t0	! t0__t0_a0
x177	t0__5a0		! t0__4t0_a0	! b_e_8t0a0	! t0__t0_a0
x178	t0__5a0		! t0__2t0_a0	! b_e_2t0	! t0__8t0_a0
x179	t0__5a0		! t0__2t0_a0	! b_e_2t0a0	! t0__8t0_a0
x180	t0__9a0		! t0__5t0	! b_e_shift	! r__r_4t0
x181	t0__9a0		! t0__5t0	! b_e_t0	! t0__4t0_a0
x182	t0__9a0		! t0__5t0	! b_e_2t0	! t0__2t0_a0
x183	t0__9a0		! t0__5t0	! b_e_2t0a0	! t0__2t0_a0
x184	t0__5a0		! t0__9t0	! b_e_4t0	! t0__t0_a0
x185	t0__9a0		! t0__4t0_a0	! b_e_t0	! t0__5t0
x186	t0__32a0	! t0__t0ma0	! b_e_2t0	! t0__3t0
x187	t0__9a0		! t0__4t0_a0	! b_e_t02a0	! t0__5t0
x188	t0__9a0		! t0__5t0	! b_e_4t0	! t0__t0_2a0
x189	t0__5a0		! t0__4t0_a0	! b_e_t0	! t0__9t0
x190	t0__9a0		! t0__2t0_a0	! b_e_2t0	! t0__5t0
x191	t0__64a0	! t0__3t0	! b_e_t0	! t0__t0ma0
x192	t0__8a0		! t0__3t0	! b_e_shift	! r__r_8t0
x193	t0__8a0		! t0__3t0	! b_e_t0	! t0__8t0_a0
x194	t0__8a0		! t0__3t0	! b_e_2t0	! t0__4t0_a0
x195	t0__8a0		! t0__8t0_a0	! b_e_t0	! t0__3t0
x196	t0__8a0		! t0__3t0	! b_e_4t0	! t0__2t0_a0
x197	t0__8a0		! t0__3t0	! b_e_4t0a0	! t0__2t0_a0
x198	t0__64a0	! t0__t0_2a0	! b_e_t0	! t0__3t0
x199	t0__8a0		! t0__4t0_a0	! b_e_2t0a0	! t0__3t0
x200	t0__5a0		! t0__5t0	! b_e_shift	! r__r_8t0
x201	t0__5a0		! t0__5t0	! b_e_t0	! t0__8t0_a0
x202	t0__5a0		! t0__5t0	! b_e_2t0	! t0__4t0_a0
x203	t0__5a0		! t0__5t0	! b_e_2t0a0	! t0__4t0_a0
x204	t0__8a0		! t0__2t0_a0	! b_e_4t0	! t0__3t0
x205	t0__5a0		! t0__8t0_a0	! b_e_t0	! t0__5t0
x206	t0__64a0	! t0__t0_4a0	! b_e_t02a0	! t0__3t0
x207	t0__8a0		! t0__2t0_a0	! b_e_3t0	! t0__4t0_a0
x208	t0__5a0		! t0__5t0	! b_e_8t0	! t0__t0_a0
x209	t0__5a0		! t0__5t0	! b_e_8t0a0	! t0__t0_a0
x210	t0__5a0		! t0__4t0_a0	! b_e_2t0	! t0__5t0
x211	t0__5a0		! t0__4t0_a0	! b_e_2t0a0	! t0__5t0
x212	t0__3a0		! t0__4t0_a0	! b_e_4t0	! t0__4t0_a0
x213	t0__3a0		! t0__4t0_a0	! b_e_4t0a0	! t0__4t0_a0
x214	t0__9a0		! t0__t0_4a0	! b_e_2t04a0	! t0__8t0_a0
x215	t0__5a0		! t0__4t0_a0	! b_e_5t0	! t0__2t0_a0
x216	t0__9a0		! t0__3t0	! b_e_shift	! r__r_8t0
x217	t0__9a0		! t0__3t0	! b_e_t0	! t0__8t0_a0
x218	t0__9a0		! t0__3t0	! b_e_2t0	! t0__4t0_a0
x219	t0__9a0		! t0__8t0_a0	! b_e_t0	! t0__3t0
x220	t0__3a0		! t0__9t0	! b_e_4t0	! t0__2t0_a0
x221	t0__3a0		! t0__9t0	! b_e_4t0a0	! t0__2t0_a0
x222	t0__9a0		! t0__4t0_a0	! b_e_2t0	! t0__3t0
x223	t0__9a0		! t0__4t0_a0	! b_e_2t0a0	! t0__3t0
x224	t0__9a0		! t0__3t0	! b_e_8t0	! t0__t0_a0
x225	t0__9a0		! t0__5t0	! b_e_t0	! t0__5t0
x226	t0__3a0		! t0__2t0_a0	! b_e_t02a0	! t0__32t0
x227	t0__9a0		! t0__5t0	! b_e_t02a0	! t0__5t0
x228	t0__9a0		! t0__2t0_a0	! b_e_4t0	! t0__3t0
x229	t0__9a0		! t0__2t0_a0	! b_e_4t0a0	! t0__3t0
x230	t0__9a0		! t0__5t0	! b_e_5t0	! t0__t0_a0
x231	t0__9a0		! t0__2t0_a0	! b_e_3t0	! t0__4t0_a0
x232	t0__3a0		! t0__2t0_a0	! b_e_8t0	! t0__4t0_a0
x233	t0__3a0		! t0__2t0_a0	! b_e_8t0a0	! t0__4t0_a0
x234	t0__3a0		! t0__4t0_a0	! b_e_2t0	! t0__9t0
x235	t0__3a0		! t0__4t0_a0	! b_e_2t0a0	! t0__9t0
x236	t0__9a0		! t0__2t0_a0	! b_e_4t08a0	! t0__3t0
x237	t0__16a0	! t0__5t0	! b_e_3t0	! t0__t0ma0
x238	t0__3a0		! t0__4t0_a0	! b_e_2t04a0	! t0__9t0
x239	t0__16a0	! t0__5t0	! b_e_t0ma0	! t0__3t0
x240	t0__9a0		! t0__t0_a0	! b_e_8t0	! t0__3t0
x241	t0__9a0		! t0__t0_a0	! b_e_8t0a0	! t0__3t0
x242	t0__5a0		! t0__3t0	! b_e_2t0	! t0__8t0_a0
x243	t0__9a0		! t0__9t0	! b_e_t0	! t0__3t0
x244	t0__5a0		! t0__3t0	! b_e_4t0	! t0__4t0_a0
x245	t0__8a0		! t0__3t0	! b_e_5t0	! t0__2t0_a0
x246	t0__5a0		! t0__8t0_a0	! b_e_2t0	! t0__3t0
x247	t0__5a0		! t0__8t0_a0	! b_e_2t0a0	! t0__3t0
x248	t0__32a0	! t0__t0ma0	! b_e_shift	! r__r_8t0
x249	t0__32a0	! t0__t0ma0	! b_e_t0	! t0__8t0_a0
x250	t0__5a0		! t0__5t0	! b_e_2t0	! t0__5t0
x251	t0__5a0		! t0__5t0	! b_e_2t0a0	! t0__5t0
x252	t0__64a0	! t0__t0ma0	! b_e_shift	! r__r_4t0
x253	t0__64a0	! t0__t0ma0	! b_e_t0	! t0__4t0_a0
x254	t0__128a0	! t0__t0ma0	! b_e_shift	! r__r_2t0
x255	t0__256a0	! a1_ne_0_b_l0	! t0__t0ma0	! b_n_ret_t0
;1040 insts before this.
ret_t0	MILLIRET
e_t0	r__r_t0
e_shift	a1_ne_0_b_l2
	a0__256a0	; a0 <<= 8 ***********
	MILLIRETN
e_t0ma0	a1_ne_0_b_l0
	t0__t0ma0
	MILLIRET
	r__r_t0
e_t0a0	a1_ne_0_b_l0
	t0__t0_a0
	MILLIRET
	r__r_t0
e_t02a0	a1_ne_0_b_l0
	t0__t0_2a0
	MILLIRET
	r__r_t0
e_t04a0	a1_ne_0_b_l0
	t0__t0_4a0
	MILLIRET
	r__r_t0
e_2t0	a1_ne_0_b_l1
	r__r_2t0
	MILLIRETN
e_2t0a0	a1_ne_0_b_l0
	t0__2t0_a0
	MILLIRET
	r__r_t0
e2t04a0	t0__t0_2a0
	a1_ne_0_b_l1
	r__r_2t0
	MILLIRETN
e_3t0	a1_ne_0_b_l0
	t0__3t0
	MILLIRET
	r__r_t0
e_4t0	a1_ne_0_b_l1
	r__r_4t0
	MILLIRETN
e_4t0a0	a1_ne_0_b_l0
	t0__4t0_a0
	MILLIRET
	r__r_t0
e4t08a0	t0__t0_2a0
	a1_ne_0_b_l1
	r__r_4t0
	MILLIRETN
e_5t0	a1_ne_0_b_l0
	t0__5t0
	MILLIRET
	r__r_t0
e_8t0	a1_ne_0_b_l1
	r__r_8t0
	MILLIRETN
e_8t0a0	a1_ne_0_b_l0
	t0__8t0_a0
	MILLIRET
	r__r_t0

	.procend
	.end

