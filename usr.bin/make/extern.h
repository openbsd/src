#ifndef EXTERN_H
#define EXTERN_H

/*	$OpenBSD: extern.h,v 1.43 2010/07/19 19:46:44 espie Exp $	*/
/*	$NetBSD: nonints.h,v 1.12 1996/11/06 17:59:19 christos Exp $	*/

/*-
 * Copyright (c) 1988, 1989, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1989 by Berkeley Softworks
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Adam de Boor.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 *
 *	from: @(#)nonints.h	8.3 (Berkeley) 3/19/94
 */

extern bool	compatMake;	/* True if we are make compatible */
extern bool	ignoreErrors;	/* True if should ignore all errors */
extern bool	beSilent;	/* True if should print no commands */
extern bool	noExecute;	/* True if should execute nothing */
extern bool	allPrecious;	/* True if every target is precious */
extern bool	keepgoing;	/* True if should continue on unaffected
				 * portions of the graph when have an error
				 * in one portion */
extern bool	touchFlag;	/* true if targets should just be 'touched'
				 * if out of date. Set by the -t flag */
extern bool	queryFlag;	/* true if we aren't supposed to really make
				 * anything, just see if the targets are out-
				 * of-date */

#endif
