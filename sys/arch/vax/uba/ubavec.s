/*	$NetBSD: ubavec.s,v 1.2 1995/02/23 17:53:22 ragge Exp $ */
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
		

/*
 * Interrupt vectors for Unibus; already at the right place at boot.
 * This allocation should be done in some other way, 8600 has its
 * second SCB at the same place as 750's first UBA vector... foolish.
 */

#define	UBAVEC(lab,off)		.long lab+(off)*8+1;
#define	UBAVEC2(lab,off)	UBAVEC(lab,off);UBAVEC(lab,off+1)
#define	UBAVEC4(lab,off)	UBAVEC2(lab,off);UBAVEC2(lab,off+2)
#define	UBAVEC8(lab,off)	UBAVEC4(lab,off);UBAVEC4(lab,off+4)
#define	UBAVEC16(lab,off)	UBAVEC8(lab,off);UBAVEC8(lab,off+8)

.globl	_UNIvec, _eUNIvec, ubaett

_UNIvec:
	UBAVEC16(ubaett,0);UBAVEC16(ubaett,16);
	UBAVEC16(ubaett,32);UBAVEC16(ubaett,48);
	UBAVEC16(ubaett,64);UBAVEC16(ubaett,80);
	UBAVEC16(ubaett,96);UBAVEC16(ubaett,112);
	UBAVEC16(ubatva,0);UBAVEC16(ubatva,16);
	UBAVEC16(ubatva,32);UBAVEC16(ubatva,48);
	UBAVEC16(ubatva,64);UBAVEC16(ubatva,80);
	UBAVEC16(ubatva,96);UBAVEC16(ubatva,112);
	UBAVEC16(ubatre,0);UBAVEC16(ubatre,16);
	UBAVEC16(ubatre,32);UBAVEC16(ubatre,48);
	UBAVEC16(ubatre,64);UBAVEC16(ubatre,80);
	UBAVEC16(ubatre,96);UBAVEC16(ubatre,112);
	UBAVEC16(ubafyra,0);UBAVEC16(ubafyra,16);
	UBAVEC16(ubafyra,32);UBAVEC16(ubafyra,48);
	UBAVEC16(ubafyra,64);UBAVEC16(ubafyra,80);
	UBAVEC16(ubafyra,96);UBAVEC16(ubafyra,112);
_eUNIvec:

#define	PR(uba)		.align 2;pushr $0x3f;jsb uba ;
#define	UBAJSB4(uba)	PR(uba);PR(uba);PR(uba);PR(uba);
#define	UBAJSB16(uba)	UBAJSB4(uba);UBAJSB4(uba);UBAJSB4(uba);UBAJSB4(uba);
#define	UBAJSB64(uba)	UBAJSB16(uba);UBAJSB16(uba);UBAJSB16(uba);UBAJSB16(uba)

ubaett:
	UBAJSB64(ett);UBAJSB64(ett);UBAJSB64(ett);UBAJSB64(ett);
ubatva:
	UBAJSB64(tva);UBAJSB64(tva);UBAJSB64(tva);UBAJSB64(tva);
ubatre:
	UBAJSB64(tre);UBAJSB64(tre);UBAJSB64(tre);UBAJSB64(tre);
ubafyra:
	UBAJSB64(fyra);UBAJSB64(fyra);UBAJSB64(fyra);UBAJSB64(fyra);


ett:	subl3	$ubaett, (sp), r0
	ashl	$-3, r0, (sp)
	pushl	$0
	brb	1f

tva:	subl3   $ubatva, (sp), r0
	ashl    $-3, r0, (sp)
	pushl   $1
	brb     1f

tre:	subl3   $ubatre, (sp), r0
        ashl    $-3, r0, (sp)
        pushl   $2
        brb     1f

	.globl ett,tva,tre,fyra
fyra:	subl3   $ubafyra, (sp), r0
        ashl    $-3, r0, (sp)
        pushl   $3

1:	mfpr	$PR_IPL, -(sp)
	calls	$3, _ubainterrupt
	popr	$0x3f
	rei






