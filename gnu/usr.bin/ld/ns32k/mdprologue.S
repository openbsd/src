/* *	$OpenBSD: mdprologue.S,v 1.2 1998/03/26 19:47:17 niklas Exp $*/
/*
 * Copyright (c) 1994 Matthias Pfaller.
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
 *	This product includes software developed by Matthias Pfaller.
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
 *
 */

/*
 * Run-time link editor entry points for ns32k architecture.
 */

	.text
	.globl	_rtld_entry, _binder_entry, _binder

/*
 * _rtld_entry(int version, struct crt_ldso *crtp)
 */

_rtl_entry:
	enter	[],0
	sprd	sb,tos
	addr	__GLOBAL_OFFSET_TABLE_(pc),r0
	lprd	sb,r0			/* PIC function prologue	*/
	movd	12(fp),r0		/* -> crt_ldso structure	*/
	movd	0(r0),r1 		/* base address 		*/
					/* 1st entry in GOT		*/
					/* + ld.so base address 	*/
	addr	0(0(sb))[r1:b],tos	/* gives address of __DYNAMIC	*/
	movd	r0,tos			/* crt_ldso pointer		*/
	movd	8(fp),tos		/* version			*/
	jsr	0(_rtld(sb))[r1:b]	/* call _rtld function		*/
	lprd	sb,-4(fp)
	exit	[]
	ret	0

/*
 * First call to a procedure generally comes through here for
 * binding.
 */

_binder_entry:
	movd	tos,r1			/* grab return address (within PLT) */
	movd	r0,tos			/* save r0 (for cerror)		*/
	movzwd	0(r1),tos		/* get function index		*/
	addr	-6(r1),tos		/* point to PLT slot		*/
	bsr	_binder			/* relocate function address	*/
	movd	r0,tos			/* prepare for jump		*/
	movd	12(sp),r0		/* restore r0			*/
	ret	12			/* jump into function		*/
