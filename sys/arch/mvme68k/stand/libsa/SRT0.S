|	$OpenBSD: SRT0.S,v 1.7 2001/07/04 08:06:56 niklas Exp $
|	$NetBSD: SRT0.S,v 1.2 1995/08/12 18:38:55 gwr Exp $

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

|	this is a pre-startup wrapper for bugcrt which relocates the
|	code to the correct place before exec()ing.

|	SRT0.S - Stand-alone Run-Time startup code, part 0
	.file	"SRT0.S"
	.text
	.globl	__estack
__estack:
	.globl	xstart
xstart:
| first, relocate code to correct place without touching critical regs
| (args are in: d0, d1, d4, a0, a1, a2, a3, a4, a5, a6)
| 	[a3 and a4 only when netbooting]
	movl	a3, d3			| SAVE a3 in d3
	movl	a4, d5			| SAVE a4 in d5
	lea	pc@(xstart:w), a3	| a3 = current addr (could be anywhere)
	lea	xstart:l, a4		| a4 = desired location (LINKADDR)
	cmpl	a3, a4			| already there?
	beqs	restart			| short-circuit out

					| Relocate the code and data 
	movl	#_edata,d2		| Desired end of program
	subl	a4,d2			| Calculate length, round up.
	lsrl	#2,d2
Lcp:
	movl	a3@+, a4@+
	dbra	d2, Lcp

| Force a long jump to the relocated code (not pc-relative)
	lea	restart:l, a3
	jmp	a3@

restart:
| now in the relocated code

| Set up stack (just before relocated text)
	lea	__estack:l, a3
	movl	a3, sp

| now that we have relocated, call the bugcrt  (note we skip over the special
|	bug header which has a PC and SP in it)
	movl	d3, a3			| RESTORE a3
	movl	d5, a4			| RESTORE a4
	jmp	_start

| The end.
