|	$Id: SRT0.S,v 1.4 1995/11/17 22:05:43 deraadt Exp $

| Copyright (c) 1995 Theo de Raadt
| 
| Redistribution and use in source and binary forms, with or without
| modification, are permitted provided that the following conditions
| are met:
| 1. Redistributions of source code must retain the above copyright
|    notice, this list of conditions and the following disclaimer.
| 2. Redistributions in binary form must reproduce the above copyright
|    notice, this list of conditions and the following disclaimer in the
|    documentation and/or other materials provided with the distribution.
| 3. All advertising materials mentioning features or use of this software
|    must display the following acknowledgement:
|	This product includes software developed under OpenBSD by
|	Theo de Raadt for Willowglen Singapore.
| 4. The name of the author may not be used to endorse or promote products
|    derived from this software without specific prior written permission.
|
| THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
| OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
| WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
| ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
| DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
| DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
| OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
| HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
| LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
| OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
| SUCH DAMAGE.
|
| Copyright (c) 1995 Gordon W. Ross
| All rights reserved.
|
| Redistribution and use in source and binary forms, with or without
| modification, are permitted provided that the following conditions
| are met:
| 1. Redistributions of source code must retain the above copyright
|    notice, this list of conditions and the following disclaimer.
| 2. Redistributions in binary form must reproduce the above copyright
|    notice, this list of conditions and the following disclaimer in the
|    documentation and/or other materials provided with the distribution.
| 3. The name of the author may not be used to endorse or promote products
|    derived from this software without specific prior written permission.
| 4. All advertising materials mentioning features or use of this software
|    must display the following acknowledgement:
|      This product includes software developed by Gordon Ross
|
| THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
| IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
| OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
| IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
| INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
| NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
| DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
| THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
| (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
| THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

|	SRT0.S - Stand-alone Run-Time startup code, part 0
		.file	"SRT0.S"
		.text
		.globl	__estack
__estack:
		.globl	start
start:
| Check to see if the code is located correctly.
| This SHOULD do a PC-relative load into a0, but...
|		lea	start, a0	| current location (0x4000)
| XXX - GAS version 1.93 gets the above lea wrong!
		.word	0x41fa
		.word	0xfffe
| Now force a long (not PC-relative) load to a1 and compare.
		lea	start:l, a1	| desired location (LINKADDR)
		cmpl	a0, a1
		beqs	restart

| Relocate the code and data to where they belong.
		movl	#_edata,d2	| Desired end of program
		subl	a1,d2		| Calculate length, round up.
		lsrl	#2,d2
Lcp:		movl	a0@+, a1@+
		dbra	d2, Lcp

| Force a long jump to the relocated code (not pc-relative)
		lea	restart:l, a0
		jmp	a0@

restart:
| now in the relocated code

| Set up stack (just before relocated text)
		lea	__estack:l, a0
		movl	a0, sp
		subl	a6, a6

		movl	d0, _devlun
		movl	d1, _ctrlun
		movl	a3, _oparg
		movl	a4, _opargend

| Call the run-time startup C code, which will:
|   initialize, call main, call exit
		jsr	__start:l

| If _start returns, fall into abort.
		.globl	_abort
_abort:
		trap	#0

| If abort returns, fall into reset.
		.globl	_reset
_reset:
		reset
		jmp	_reset

| function to get the vector base register
		.globl	_getvbr
_getvbr:
		movc	vbr, d0
		rts

| The end.

		.data
		.globl	_devlun, _ctrlun, _oparg, _opargend
_devlun:	.long	0
_ctrlun:	.long	0
_oparg:		.long	0
_opargend:	.long	0
