/*	$OpenBSD: asm.h,v 1.3 1997/01/13 11:51:09 niklas Exp $	*/
/*	$NetBSD: asm.h,v 1.12 1996/11/30 02:49:00 jtc Exp $	*/

/*
 * Copyright (c) 1994 Allen Briggs
 * All rights reserved.
 *
 * Gleaned from locore.s and sun3 asm.h which had the following copyrights:
 * locore.s:
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1990 The Regents of the University of California.
 * sun3/include/asm.h:
 * Copyright (c) 1993 Adam Glass
 * Copyright (c) 1990 The Regents of the University of California.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _ASM_H_
#define _ASM_H_

#ifdef __STDC__
#define _C_LABEL(name)		_ ## name
#else
#define _C_LABEL(name)		_/**/name
#endif
#define	_ASM_LABEL(name)	name

#ifndef _KERNEL
#define	_ENTRY(name) \
	.text; .even; .globl name; .type name,@function; name:
#else
#define	_ENTRY(name) \
	.text; .even; .globl name; name:
#endif

#ifdef GPROF
#define _PROF_PROLOG	link a6,#0; jbsr mcount; unlk a6
#else
#define _PROF_PROLOG
#endif

#define ENTRY(name)		_ENTRY(_C_LABEL(name)) _PROF_PROLOG
#define	ASENTRY(name)		_ENTRY(_ASM_LABEL(name)) _PROF_PROLOG

/*
 * The m68k ALTENTRY macro is very different than the traditional
 * implementation used by other OpenBSD ports.  Usually ALTENTRY 
 * simply provides an alternate function entry point.  The m68k
 * definition takes a second argument and jumps inside the second
 * function when profiling is enabled.
 *
 * The m68k behavior is similar to the ENTRY2 macro found in
 * solaris' asm_linkage.h.
 *
 * Providing ENTRY2 and changing all the code that uses ALTENTRY
 * to use it would be a desirable change.
 */
#ifdef PROF
#define ALTENTRY(name, rname)	ENTRY(name); jra rname+12
#else
#define ALTENTRY(name, rname)	_ENTRY(_C_LABEL(name))
#endif

#define RCSID(x)		.text; .asciz x

#endif /* _ASM_H_ */
