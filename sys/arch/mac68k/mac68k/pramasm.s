/*	$NetBSD: pramasm.s,v 1.4 1995/09/28 03:15:54 briggs Exp $	*/

/*
 * RTC toolkit version 1.08b, copyright 1995, erik vogan
 *
 * All rights and privledges to this code hereby donated
 * to the ALICE group for use in NetBSD.  see the copyright
 * below for more info...
 */
/*
 * Copyright (c) 1995 Erik Vogan
 * All rights reserved.
 *
 * This code is derived from software contributed to the Alice Group
 * by Erik Vogan.
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
 *	This product includes software developed by the Alice Group.
 * 4. The names of the Alice Group or any of its members may not be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE ALICE GROUP ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE ALICE GROUP BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *  The following are the C interface functions to RTC access functions
 * that are defined later in this file.
 */

	.text

	.even
.globl _readPram
_readPram:
	link	a6,#-4		|  create a little home for ourselves
	.word	0xa03f		|  _InitUtil to read PRam
	moveml	d1/a1,sp@-
	moveq	#0,d0		|  zero out length register
	moveb	a6@(19),d0	|  move the length byte in
	moveq	#0,d1		|  zero out location 
	moveb	a6@(15),d1	|  now get out PRam location
	lea	_SysParam,a1	|  start of PRam data
	movel	a6@(8),a0	|  get our data address
_readPramAgain:
	subql	#1,d0
	bcs	_readPramDone	|  see if we are through
	moveb	a1@(d1),a0@+	|  transfer byte
	addql	#1,d1		|  next byte
	jmp	_readPramAgain	|  do it again 
_readPramDone:
	clrw	d0
	moveml	sp@+,d1/a1
	unlk a6 		|  clean up after ourselves
	rts			|  and return to caller


.globl _writePram
_writePram:
	link	a6,#-4		|  create a little home for ourselves
	.word	0xa03f		|  _InitUtil to read PRam in the case it hasn't been read yet
	moveml	d1/a1,sp@-
	moveq	#0,d0		|  zero out length register
	moveb	a6@(19),d0	|  move the length byte in
	moveq	#0,d1		|  zero out location 
	moveb	a6@(15),d1	|  now get out PRam location
	lea	_SysParam,a1	|  start of PRam data
	movel	a6@(8),a0	|  get our data address
_writePramAgain:
	subql	#1,d0
	bcs	_writePramDone	|  see if we are through
	cmpil	#0x14,d1	|  check for end of _SysParam
	bcc	_writePramDone	|  do not write if beyond end
	moveb	a0@+,a1@(d1)	|  transfer byte
	addql	#1,d1		|  next byte
	jmp	_writePramAgain |  do it again 
_writePramDone:
	.word	0xa038		|  writeParam
	moveml	sp@+,d1/a1
	unlk a6 		|  clean up after ourselves
	rts			|  and return to caller


.globl _readExtPram
_readExtPram:
	link	a6,#-4		|  create a little home for ourselves
	moveq	#0,d0		|  zero out our future command register
	moveb	a6@(19),d0	|  move the length byte in
	swap	d0		|  and make that the MSW
	moveb	a6@(15),d0	|  now get out PRAM location
	movel	a6@(8),a0	|  get our data address
	.word	0xa051		|  and go read the data
	unlk a6 		|  clean up after ourselves
	rts			|  and return to caller

.globl _writeExtPram
_writeExtPram:
	link	a6,#-4		|  create a little home for ourselves
	moveq	#0,d0		|  zero out our future command register
	moveb	a6@(19),d0	|  move the length byte in
	swap	d0		|  and make that the MSW
	moveb	a6@(15),d0	|  now get out PRAM location
	movel	a6@(8),a0	|  get our data address
	.word	0xa052		|  and go write the data
	unlk a6 		|  clean up after ourselves
	rts			|  and return to caller

.globl _getPramTime
_getPramTime:
	link	a6,#-4		|  create a little home for ourselves
	.word	0xa03f		|  call the routine to read the time (_InitUtil)
	movel	_Time,d0
	unlk	a6		|  clean up after ourselves
	rts			|  and return to caller

.globl _setPramTime
_setPramTime:
	link	a6,#-4		|  create a little home for ourselves
	movel	a6@(8),d0	|  get the passed in long (seconds since 1904)
	.word	0xa03a		|  call the routine to write the time
	unlk	a6		|  clean up after ourselves
	rts			|  and return to caller

