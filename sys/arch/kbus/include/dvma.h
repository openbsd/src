/*	$OpenBSD: dvma.h,v 1.1.1.1 1997/10/14 07:25:30 gingold Exp $	*/
/*	$NetBSD: dvma.h,v 1.4 1996/11/20 18:57:08 gwr Exp $	*/

/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Gordon W. Ross.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * dvmamap manages a range of DVMA addresses intended to create double
 * mappings of physical memory. In a way, `dvmamap' is a submap of the
 * VM map `phys_map'. The difference is the use of the `resource map'
 * routines to manage page allocation, allowing DVMA addresses to be
 * allocated and freed from within interrupt routines.
 *
 * Note that `phys_map' can still be used to allocate memory-backed pages
 * in DVMA space.
 */

extern struct map	*dvmamap;

/* Allow up to 1 Mb of DVMA space.  */
#define DVMA_SIZE	(1 << 20)

/*
 * The dvma resource map is defined in page units, which are numbered 1 to N.
 * Use these macros to convert to/from virtual addresses.
 */
#define rctov(n)		ctob(((n)-1))
#define vtorc(v)		((btoc(v))+1)

extern caddr_t	kdvma_mapin __P((caddr_t, int, int));
extern caddr_t	dvma_malloc __P((size_t, void *, int));
extern void	dvma_free __P((caddr_t, size_t, void *));
extern void	dvma_mapout __P((vm_offset_t kva, vm_offset_t va, int len));
extern vm_offset_t dvma_mapin __P((struct vm_map *, vm_offset_t, int, int));

