/*	$OpenBSD: asm_single.h,v 1.2 1998/04/25 07:09:06 d Exp $	*/
/*	$NetBSD: asm_single.h,v 1.1 1996/09/16 06:03:58 leo Exp $	*/

/*
 * Copyright (c) 1996 Leo Weppelman.
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
 *      This product includes software developed by Leo Weppelman.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _M68K_ASM_SINGLE_H
#define _M68K_ASM_SINGLE_H
/*
 * Provide bit manipulation macro's that resolve to a single instruction.
 * These can be considered atomic on single processor architectures when
 * no page faults can occur when acessing <var>.
 * Their primary use is to avoid race conditions when manipulating device
 * registers.
 */

#define single_inst_bset_b(var, bit)	\
	asm volatile ("orb %0,%1" : : "di" ((u_char)bit), "g" (var))
#define single_inst_bclr_b(var, bit)	\
	asm volatile ("andb %0,%1" : : "di" ((u_char)~(bit)), "g" (var));

#define single_inst_bset_w(var, bit)	\
	asm volatile ("orw %0,%1" : : "di" ((u_short)bit), "g" (var))
#define single_inst_bclr_w(var, bit)	\
	asm volatile ("andw %0,%1" : : "di" ((u_short)~(bit)), "g" (var));

#define single_inst_bset_l(var, bit)	\
	asm volatile ("orl %0,%1" : : "di" ((u_long)bit), "g" (var))
#define single_inst_bclr_l(var, bit)	\
	asm volatile ("andl %0,%1" : : "di" ((u_long)~(bit)), "g" (var));

#endif /* _M68K_ASM_SINGLE_H */
