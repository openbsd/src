/*	$OpenBSD: XBUG.S,v 1.7 2003/06/04 16:36:14 deraadt Exp $ */

/*
 * Copyright (c) 1989,1990,1995 Theo de Raadt
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Theo says: I wrote a bootrom for the MVME147 *years* ago. To write
 * this ROM I copied a few chunks from the old bootrom, like this piece:
 * 
 * "watch this, the moto bastard struck here, shouldn't have hired people
 * from intel I tried to tell them...
 *       "BOOT"
 *       offset from baseaddr to entry point.
 *       offset from baseaddr to first word after checksum
 *       garbage
 *       checksum made with CS command
 * No need to change any of this unless you try to take our names out
 * of there. Ie. don't touch."
 */

		.text
bootlabel:	.ascii	"BOOT"
		.long	bootstart-0xffa00000	| for rom install
		.long	bootlabelend - bootlabel
		.asciz	"VME147 rboot Copyright (c) 1995 Theo de Raadt"
		.align	2
bootstart:	jmp	bssclr
		.word	0x229c			| XXX bitching cksum!
bootlabelend:
		.word	0

		| clear bss and the kernel location
bssclr:		movl	#_edata,a0
		movl	#_end,d0
		subl	#_edata,d0
1:		clrb	a0@+
		subql	#1,d0
		bpl	1b

		| rip the data segment from ROM into ram..
		movl	#_etext,a2		| start of data
		movl	#0x4000,a1		| shovel address
		movl	#_edata,d0
		subl	#_etext,d0
1:		movb	a2@+,a1@+
		subql	#1,d0
		bpl	1b

		bra	start
