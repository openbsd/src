/*	$NetBSD: sid.h,v 1.6 1995/11/12 14:37:18 ragge Exp $	*/

/*
 * Copyright (c) 1994 Ludd, University of Lule}, Sweden.
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
 *     This product includes software developed at Ludd, University of Lule}.
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

 /* All bugs are subject to removal without further notice */
		


#define	VAX_780	1
#define VAX_750	2
#define	VAX_730	3
#define VAX_8600 4
#define VAX_8200 5
#define VAX_8800 6
#define VAX_610 7
#define VAX_78032 8
#define VAX_650 10
#define	VAX_MAX	10

#define MACHID(x)       ((x>>24)&255)

#define V750UCODE(x)    ((x>>8)&255)

/*
 * The MicroVAXII CPU chip (78032) is used on more than one type of system
 * that are differentiated by the low order 8 bits of cpu_type. (Filled in
 * from the System Identification Extension Register.) To test for the cpu
 * chip, compare cpunumber == VAX_78032, but to test for a Qbus MicroVAXII
 * compare cpu_type == VAX_630.
 */
#define VAX_630       0x8000001
#define VAX_410       0x8000002

extern int cpu_type, cpunumber;
