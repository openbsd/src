/*	$NetBSD: pmap.h,v 1.14 1996/02/28 22:50:43 gwr Exp $	*/

/*
 * Copyright (c) 1994 Gordon W. Ross
 * Copyright (c) 1993 Adam Glass
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
 *	This product includes software developed by Adam Glass.
 * 4. The name of the Authors may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef	_MACHINE_PMAP_
#define	_MACHINE_PMAP_

/*
 * Physical map structures exported to the VM code.
 */

struct pmap {
	int	                pm_refcount;	/* pmap reference count */
	simple_lock_data_t      pm_lock;	/* lock on pmap */
	struct pmap_statistics	pm_stats;	/* pmap statistics */
	int                     pm_version;
	int                     pm_ctxnum;
	unsigned char           *pm_segmap;
};

typedef struct pmap *pmap_t;

#ifdef _KERNEL
struct pmap	kernel_pmap_store;

#define	pmap_kernel()			(&kernel_pmap_store)

#define PMAP_ACTIVATE(pmap, pcbp, iscurproc) \
	pmap_activate(pmap, pcbp)
#define PMAP_DEACTIVATE(pmap, pcbp) \
	pmap_deactivate(pmap, pcbp)

extern void pmap_prefer(vm_offset_t, vm_offset_t *);
#define PMAP_PREFER(fo, ap) pmap_prefer((fo), (ap))

/* XXX - Need a (silly) #define get code in kern_sysctl.c */
extern segsz_t pmap_resident_pages(pmap_t);
#define	pmap_resident_count(pmap)	pmap_resident_pages(pmap)

/*
 * Since PTEs also contain type bits, we have to have some way
 * to tell pmap_enter `this is an IO page' or `this is not to
 * be cached'.  Since physical addresses are always aligned, we
 * can do this with the low order bits.
 *
 * The values below must agree with pte.h such that:
 *	(PMAP_OBIO << PG_MOD_SHIFT) == PGT_OBIO
 */
#define	PMAP_OBIO	0x04		/* tells pmap_enter to use PG_OBIO */
#define	PMAP_VME16	0x08		/* etc */
#define	PMAP_VME32	0x0C		/* etc */
#define	PMAP_NC		0x10		/* tells pmap_enter to set PG_NC */
#define	PMAP_SPEC	0x1C		/* mask to get all above. */

#endif	/* _KERNEL */
#endif	/* _MACHINE_PMAP_ */
