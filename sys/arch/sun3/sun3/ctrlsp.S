/*	$NetBSD: ctrlsp.S,v 1.3 1996/11/20 18:57:26 gwr Exp $	*/

/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Adam Glass.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This is shared with stand/libsa
 * XXX - can not include cpu.h there...
 */

#include <machine/asm.h>
#define FC_CONTROL 3	/* from cpu.h */

/*	
 * unsigned char get_control_byte (char *addr)
 */	
ENTRY(get_control_byte)
	movc sfc, d1			| save sfc
	moveq #FC_CONTROL, d0		| sfc = FC_CONTROL
	movec d0, sfc
	movl sp@(4), a0			| src addr
	moveq #0, d0			| get byte
	movsb a0@, d0
	movc d1, sfc			| restore sfc
	rts

/*
 * unsigned int get_control_word (char *addr)
 */	
ENTRY(get_control_word)
	movc sfc, d1			| save sfc
	moveq #FC_CONTROL, d0		| sfc = FC_CONTROL
	movec d0, sfc
	movl sp@(4), a0			| src addr
	movsl a0@, d0			| get long
	movc d1, sfc			| restore sfc
	rts

/*	
 * void set_control_byte (char *addr, int value)
 */
ENTRY(set_control_byte)
	movc dfc, d1			| save dfc
	moveq #FC_CONTROL, d0		| dfc = FC_CONTROL
	movc d0, dfc
	movl sp@(4), a0			| addr
	movl sp@(8), d0			| value
	movsb d0, a0@			| set byte
	movc d1, dfc			| restore dfc
	rts

/*
 * void set_control_word (char *addr, int value)
 */
ENTRY(set_control_word)
	movc dfc, d1			| save dfc
	moveq #FC_CONTROL, d0		| dfc = FC_CONTROL
	movc d0, dfc
	movl sp@(4), a0			| addr
	movl sp@(8), d0			| value
	movsl d0, a0@			| set long
	movc d1, dfc			| restore dfc
	rts

