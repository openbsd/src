/*	$OpenBSD: vm_pager.h,v 1.8 2001/06/27 04:52:40 art Exp $	*/
/*	$NetBSD: vm_pager.h,v 1.10 1995/03/26 20:39:15 jtc Exp $	*/

/*
 * Copyright (c) 1990 University of Utah.
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 *
 *	@(#)vm_pager.h	8.5 (Berkeley) 7/7/94
 */

/*
 * Pager routine interface definition.
 * For BSD we use a cleaner version of the internal pager interface.
 */

#ifndef	_VM_PAGER_
#define	_VM_PAGER_

/*
 * get/put return values
 * OK	   operation was successful
 * BAD	   specified data was out of the accepted range
 * FAIL	   specified data was in range, but doesn't exist
 * PEND	   operations was initiated but not completed
 * ERROR   error while accessing data that is in range and exists
 * AGAIN   temporary resource shortage prevented operation from happening
 * UNLOCK  unlock the map and try again
 * REFAULT [uvm_fault internal use only!] unable to relock data structures,
 *         thus the mapping needs to be reverified before we can procede
 */
#define	VM_PAGER_OK		0
#define	VM_PAGER_BAD		1
#define	VM_PAGER_FAIL		2
#define	VM_PAGER_PEND		3
#define	VM_PAGER_ERROR		4
#define VM_PAGER_AGAIN		5
#define VM_PAGER_UNLOCK		6
#define VM_PAGER_REFAULT	7

#endif	/* _VM_PAGER_ */
