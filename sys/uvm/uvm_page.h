/*	$OpenBSD: uvm_page.h,v 1.2 1999/02/26 05:32:07 art Exp $	*/
/*	$NetBSD: uvm_page.h,v 1.10 1998/08/13 02:11:02 eeh Exp $	*/

/*
 * XXXCDC: "ROUGH DRAFT" QUALITY UVM PRE-RELEASE FILE!
 *         >>>USE AT YOUR OWN RISK, WORK IS NOT FINISHED<<<
 */
/* 
 * Copyright (c) 1997 Charles D. Cranor and Washington University.
 * Copyright (c) 1991, 1993, The Regents of the University of California.  
 *
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * The Mach Operating System project at Carnegie-Mellon University.
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
 *	This product includes software developed by Charles D. Cranor,
 *      Washington University, the University of California, Berkeley and 
 *      its contributors.
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
 *	@(#)vm_page.h   7.3 (Berkeley) 4/21/91
 * from: Id: uvm_page.h,v 1.1.2.6 1998/02/04 02:31:42 chuck Exp
 *
 *
 * Copyright (c) 1987, 1990 Carnegie-Mellon University.
 * All rights reserved.
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#ifndef _UVM_UVM_PAGE_H_
#define _UVM_UVM_PAGE_H_

/*
 * uvm_page.h
 */

/*
 * macros
 */

#define uvm_lock_pageq()	simple_lock(&uvm.pageqlock)
#define uvm_unlock_pageq()	simple_unlock(&uvm.pageqlock)
#define uvm_lock_fpageq()	simple_lock(&uvm.fpageqlock)
#define uvm_unlock_fpageq()	simple_unlock(&uvm.fpageqlock)

#define uvm_pagehash(obj,off) \
	(((unsigned long)obj+(unsigned long)atop(off)) & uvm.page_hashmask)

/*
 * handle inline options
 */

#ifdef UVM_PAGE_INLINE
#define PAGE_INLINE static __inline
#else 
#define PAGE_INLINE /* nothing */
#endif /* UVM_PAGE_INLINE */

/*
 * prototypes: the following prototypes define the interface to pages
 */

void uvm_page_init __P((vaddr_t *, vaddr_t *));
#if defined(UVM_PAGE_TRKOWN)
void uvm_page_own __P((struct vm_page *, char *));
#endif
#if !defined(PMAP_STEAL_MEMORY)
boolean_t uvm_page_physget __P((paddr_t *));
#endif
void uvm_page_rehash __P((void));

PAGE_INLINE void uvm_pageactivate __P((struct vm_page *));
vaddr_t uvm_pageboot_alloc __P((vsize_t));
PAGE_INLINE void uvm_pagecopy __P((struct vm_page *, struct vm_page *));
PAGE_INLINE void uvm_pagedeactivate __P((struct vm_page *));
void uvm_pagefree __P((struct vm_page *));
PAGE_INLINE struct vm_page *uvm_pagelookup 
					__P((struct uvm_object *, vaddr_t));
void uvm_pageremove __P((struct vm_page *));
/* uvm_pagerename: not needed */
PAGE_INLINE void uvm_pageunwire __P((struct vm_page *));
PAGE_INLINE void uvm_pagewait __P((struct vm_page *, int));
PAGE_INLINE void uvm_pagewake __P((struct vm_page *));
PAGE_INLINE void uvm_pagewire __P((struct vm_page *));
PAGE_INLINE void uvm_pagezero __P((struct vm_page *));

PAGE_INLINE int uvm_page_lookup_freelist __P((struct vm_page *));

#endif /* _UVM_UVM_PAGE_H_ */
