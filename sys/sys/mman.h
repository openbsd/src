/*	$OpenBSD: mman.h,v 1.20 2010/05/21 21:17:43 miod Exp $	*/
/*	$NetBSD: mman.h,v 1.11 1995/03/26 20:24:23 jtc Exp $	*/

/*-
 * Copyright (c) 1982, 1986, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)mman.h	8.1 (Berkeley) 6/2/93
 */

#ifndef _KERNEL
#include <sys/cdefs.h>
#endif

/*
 * Protections are chosen from these bits, or-ed together
 */
#define	PROT_NONE	0x00	/* no permissions */
#define	PROT_READ	0x01	/* pages can be read */
#define	PROT_WRITE	0x02	/* pages can be written */
#define	PROT_EXEC	0x04	/* pages can be executed */

/*
 * Flags contain sharing type and options.
 * Sharing types; choose one.
 */
#define	MAP_SHARED	0x0001	/* share changes */
#define	MAP_PRIVATE	0x0002	/* changes are private */
#define	MAP_COPY	0x0004	/* "copy" region at mmap time */

/*
 * Other flags
 */
#define	MAP_FIXED	 0x0010	/* map addr must be exactly as requested */
#define	MAP_RENAME	 0x0020	/* Sun: rename private pages to file */
#define	MAP_NORESERVE	 0x0040	/* Sun: don't reserve needed swap area */
#define	MAP_INHERIT	 0x0080	/* region is retained after exec */
#define	MAP_NOEXTEND	 0x0100	/* for MAP_FILE, don't change file size */
#define	MAP_HASSEMAPHORE 0x0200	/* region may contain semaphores */
#define	MAP_TRYFIXED	 0x0400 /* attempt hint address, even within heap */

/*
 * Error return from mmap()
 */
#define MAP_FAILED	((void *)-1)

/*
 * Mapping type
 */
#define	MAP_FILE	0x0000	/* map from file (default) */
#define	MAP_ANON	0x1000	/* allocated from memory, swap space */
#define	MAP_FLAGMASK	0x17f7

/*
 * POSIX memory advisory values.
 * Note: keep consistent with the original definitions below.
 */
#define	POSIX_MADV_NORMAL	0	/* no further special treatment */
#define	POSIX_MADV_RANDOM	1	/* expect random page references */
#define	POSIX_MADV_SEQUENTIAL	2	/* expect sequential page references */
#define	POSIX_MADV_WILLNEED	3	/* will need these pages */
#define	POSIX_MADV_DONTNEED	4	/* don't need these pages */

#if __BSD_VISIBLE
/*
 * Original advice values, equivalent to POSIX definitions,
 * and few implementation-specific ones.
 */
#define	MADV_NORMAL		POSIX_MADV_NORMAL
#define	MADV_RANDOM		POSIX_MADV_RANDOM
#define	MADV_SEQUENTIAL		POSIX_MADV_SEQUENTIAL
#define	MADV_WILLNEED		POSIX_MADV_WILLNEED
#define	MADV_DONTNEED		POSIX_MADV_DONTNEED
#define	MADV_SPACEAVAIL		5	/* insure that resources are reserved */
#define	MADV_FREE		6	/* pages are empty, free them */
#endif

/*
 * Flags to minherit
 */
#define MAP_INHERIT_SHARE	0	/* share with child */
#define MAP_INHERIT_COPY	1	/* copy into child */
#define MAP_INHERIT_NONE	2	/* absent from child */
#define MAP_INHERIT_DONATE_COPY	3	/* copy and delete -- not
					   implemented in UVM */

/*
 * Flags to msync
 */
#define	MS_ASYNC	0x01	/* perform asynchronous writes */
#define	MS_SYNC		0x02	/* perform synchronous writes */
#define	MS_INVALIDATE	0x04	/* invalidate cached data */

/*
 * Flags to mlockall
 */
#define	MCL_CURRENT	0x01	/* lock all pages currently mapped */
#define	MCL_FUTURE	0x02	/* lock all pages mapped in the future */

#ifndef _KERNEL
#include <sys/_types.h>

#ifndef _SIZE_T_DEFINED_
#define _SIZE_T_DEFINED_
typedef __size_t	size_t;
#endif

#ifndef _OFF_T_DEFINED_
#define _OFF_T_DEFINED_
typedef __off_t		off_t;
#endif

__BEGIN_DECLS
/* Some of these int's should probably be size_t's */
void *	mmap(void *, size_t, int, int, int, off_t);
int	mprotect(void *, size_t, int);
int	munmap(void *, size_t);
int	msync(void *, size_t, int);
int	mlock(const void *, size_t);
int	munlock(const void *, size_t);
int	mlockall(int);
int	munlockall(void);
#if __BSD_VISIBLE
int	madvise(void *, size_t, int);
int	mincore(void *, size_t, char *);
int	minherit(void *, size_t, int);
void *	mquery(void *, size_t, int, int, int, off_t);
#endif
int	posix_madvise(void *, size_t, int);
__END_DECLS

#endif /* !_KERNEL */
