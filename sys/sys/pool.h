/*	$OpenBSD: pool.h,v 1.1 1999/02/26 03:13:29 art Exp $	*/
/*	$NetBSD: pool.h,v 1.12 1998/12/27 21:13:43 thorpej Exp $	*/

/*-
 * Copyright (c) 1997, 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Paul Kranenburg.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _SYS_POOL_H_
#define _SYS_POOL_H_

#include <sys/lock.h>
#include <sys/queue.h>

#define PR_HASHTABSIZE		8

typedef struct pool {
	TAILQ_ENTRY(pool)
			pr_poollist;
	TAILQ_HEAD(,pool_item_header)
			pr_pagelist;	/* Allocated pages */
	struct pool_item_header	*pr_curpage;
	unsigned int	pr_size;	/* Size of item */
	unsigned int	pr_align;	/* Requested alignment, must be 2^n */
	unsigned int	pr_itemoffset;	/* Align this offset in item */
	unsigned int	pr_minitems;	/* minimum # of items to keep */
	unsigned int	pr_minpages;	/* same in page units */
	unsigned int	pr_maxpages;	/* maximum # of pages to keep */
	unsigned int	pr_npages;	/* # of pages allocated */
	unsigned int	pr_pagesz;	/* page size, must be 2^n */
	unsigned long	pr_pagemask;	/* abbrev. of above */
	unsigned int	pr_pageshift;	/* shift corr. to above */
	unsigned int	pr_itemsperpage;/* # items that fit in a page */
	unsigned int	pr_slack;	/* unused space in a page */
	void		*(*pr_alloc) __P((unsigned long, int, int));
	void		(*pr_free) __P((void *, unsigned long, int));
	int		pr_mtype;	/* memory allocator tag */
	char		*pr_wchan;	/* tsleep(9) identifier */
	unsigned int	pr_flags;
#define PR_MALLOCOK	1
#define	PR_NOWAIT	0		/* for symmetry */
#define PR_WAITOK	2
#define PR_WANTED	4
#define PR_STATIC	8
#define PR_FREEHEADER	16
#define PR_URGENT	32
#define PR_PHINPAGE	64
#define PR_LOGGING	128

	/*
	 * `pr_lock' protects the pool's data structures when removing
	 * items from or returning items to the pool.
	 * `pr_resourcelock' is used to serialize access to the pool's
	 * back-end page allocator. At the same time it also protects
	 * the `pr_maxpages', `pr_minpages' and `pr_minitems' fields.
	 */
	struct simplelock	pr_lock;
	struct lock		pr_resourcelock;

	LIST_HEAD(,pool_item_header)		/* Off-page page headers */
			pr_hashtab[PR_HASHTABSIZE];

	int		pr_maxcolor;	/* Cache colouring */
	int		pr_curcolor;
	int		pr_phoffset;	/* Offset in page of page header */

	/*
	 * Instrumentation
	 */
	unsigned long	pr_nget;	/* # of successful requests */
	unsigned long	pr_nfail;	/* # of unsuccessful requests */
	unsigned long	pr_nput;	/* # of releases */
	unsigned long	pr_npagealloc;	/* # of pages allocated */
	unsigned long	pr_npagefree;	/* # of pages released */
	unsigned int	pr_hiwat;	/* max # of pages in pool */
	unsigned long	pr_nidle;	/* # of idle pages */

#ifdef POOL_DIAGNOSTIC
	struct pool_log	*pr_log;
	int		pr_curlogentry;
	int		pr_logsize;
#endif
} *pool_handle_t;

pool_handle_t	pool_create __P((size_t, u_int, u_int,
				 int, char *, size_t,
				 void *(*)__P((unsigned long, int, int)),
				 void  (*)__P((void *, unsigned long, int)),
				 int));
void		pool_init __P((struct pool *, size_t, u_int, u_int,
				 int, char *, size_t,
				 void *(*)__P((unsigned long, int, int)),
				 void  (*)__P((void *, unsigned long, int)),
				 int));
void		pool_destroy __P((pool_handle_t));
#ifdef POOL_DIAGNOSTIC
void		*_pool_get __P((pool_handle_t, int, const char *, long));
void		_pool_put __P((pool_handle_t, void *, const char *, long));
#define		pool_get(h, f)	_pool_get((h), (f), __FILE__, __LINE__)
#define		pool_put(h, v)	_pool_put((h), (v), __FILE__, __LINE__)
#else
void		*pool_get __P((pool_handle_t, int));
void		pool_put __P((pool_handle_t, void *));
#endif
int		pool_prime __P((pool_handle_t, int, caddr_t));
void		pool_setlowat __P((pool_handle_t, int));
void		pool_sethiwat __P((pool_handle_t, int));
void		pool_print __P((pool_handle_t, char *));
void		pool_reclaim __P((pool_handle_t));
void		pool_drain __P((void *));
#if defined(POOL_DIAGNOSTIC) || defined(DEBUG)
void		pool_print __P((struct pool *, char *));
int		pool_chk __P((struct pool *, char *));
#endif

/*
 * Alternate pool page allocator, provided for pools that know they
 * will never be accessed in interrupt context.
 */
void		*pool_page_alloc_nointr __P((unsigned long, int, int));
void		pool_page_free_nointr __P((void *, unsigned long, int));

#endif /* _SYS_POOL_H_ */
