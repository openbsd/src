/*	$OpenBSD: pmap.h,v 1.1 1998/07/07 21:32:44 mickey Exp $	*/

/* 
 * Copyright (c) 1990,1993,1994 The University of Utah and
 * the Computer Systems Laboratory at the University of Utah (CSL).
 * All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software is hereby
 * granted provided that (1) source code retains these copyright, permission,
 * and disclaimer notices, and (2) redistributions including binaries
 * reproduce the notices in supporting documentation, and (3) all advertising
 * materials mentioning features or use of this software display the following
 * acknowledgement: ``This product includes software developed by the
 * Computer Systems Laboratory at the University of Utah.''
 *
 * THE UNIVERSITY OF UTAH AND CSL ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS
 * IS" CONDITION.  THE UNIVERSITY OF UTAH AND CSL DISCLAIM ANY LIABILITY OF
 * ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * CSL requests users of this software to return to csl-dist@cs.utah.edu any
 * improvements that they make and grant CSL redistribution rights.
 *
 * 	Utah $Hdr: pmap.h 1.24 94/12/14$
 *	Author: Mike Hibler, Bob Wheeler, University of Utah CSL, 9/90
 */

/*
 *	Pmap header for hppa.
 */

#ifndef	_HPPA_PMAP_H_
#define	_HPPA_PMAP_H_

#include <sys/queue.h>

#define EQUIV_HACK	/* no multiple mapping of kernel equiv space allowed */

#ifdef hp700
#define BTLB		/* Use block TLBs: PA 1.1 and above */
#define HPT		/* Hashed (Hardware) Page Table */
#define USEALIGNMENT	/* Take advantage of cache alignment for optimization */
#endif

/*
 * Virtual to physical mapping macros/structures.
 * IMPORTANT NOTE: there is one mapping per HW page, not per MACH page.
 */

#define HPPA_HASHSIZE		4096	/* size of hash table */
#define HPPA_HASHSIZE_LOG2	12
#define HPPA_MIN_MPP		2	/* min # of mappings per phys page */

/*
 * This hash function is the one used by the hardware TLB walker on the 7100.
 */
#define pmap_hash(space, offset) \
	(((u_int)(space) << 5 ^ (u_int)(offset) >> PGSHIFT) & (HPPA_HASHSIZE-1))

/*
 * Do not change these structures unless you change the assembly code in
 * locore.s
 */
struct mapping {
	TAILQ_ENTRY(mapping) hash_link;	/* hash table links */
	TAILQ_ENTRY(mapping) phys_link;	/* for mappings of a given PA */
	pa_space_t	map_space;	/* virtual space */
	vm_offset_t	map_offset;	/* virtual page number */
	u_int		map_tlbpage;	/* physical page (for TLB load) */
	u_int		map_tlbprot;	/* prot/access rights (for TLB load) */
	u_int		map_tlbsw;	/* */
};

/* XXX could be in vm_param.h */

#define HPPA_QUADBYTES		0x40000000
#define	hppa_round_quad(x)	((((unsigned)(x)) + HPPA_QUADBYTES-1) & \
					~(HPPA_QUADBYTES-1))
#define hppa_trunc_quad(x)	(((unsigned)(x)) & ~(HPPA_QUADBYTES-1))

struct pmap {
	simple_lock_data_t	lock;	     /* lock on pmap */
	int			ref_count;   /* reference count */
	pa_space_t		space;	     /* space for this pmap */
	int			pid;	     /* protection id for pmap */
	struct pmap		*next;	     /* linked list of free pmaps */
	struct pmap_statistics	stats;	     /* statistics */
	TAILQ_ENTRY(pmap)	pmap_link;   /* hashed list of pmaps */
};

typedef struct pmap *pmap_t;

extern struct pmap	kernel_pmap_store;


struct vtop_entry {
	TAILQ_HEAD(, mapping)	hash_link;	/* head of vtop chain */
};
#define vtop_next	hash_link.tqe_next
#define vtop_prev	hash_link.tqe_prev

struct phys_entry {
	TAILQ_HEAD(, mapping) phys_link; /* head of mappings of a given PA */
	struct mapping	*writer;	/* mapping with R/W access */
	unsigned	tlbprot;	/* TLB format protection */
};


#ifdef 	HPT
/*
 * If HPT is defined, we cache the last miss for each bucket using a
 * structure defined for the 7100 hardware TLB walker. On non-7100s, this
 * acts as a software cache that cuts down on the number of times we have
 * to search the vtop chain. (thereby reducing the number of instructions
 * and cache misses incurred during the TLB miss).
 *
 * The vtop_entry pointer is the address of the associated vtop table entry.
 * This avoids having to reform the address into the new table on a cache
 * miss.
 */
struct hpt_entry {
	unsigned	valid:1,	/* Valid bit */
			vpn:15,		/* Virtual Page Number */
			space:16;	/* Space ID */
	unsigned	tlbprot;	/* prot/access rights (for TLB load) */
	unsigned	tlbpage;	/* physical page (for TLB load) */
	unsigned	vtop_entry;	/* Pointer to associated VTOP entry */
};
#endif

#define HPT_SHIFT	27		/* 16 byte entry (31-4) */
#define VTOP_SHIFT	28		/* 8  byte entry (31-3) */
#define HPT_LEN		HPPA_HASHSIZE_LOG2
#define VTOP_LEN	HPPA_HASHSIZE_LOG2

#define MAX_PID		0xfffa
#define	HPPA_SID_KERNEL  0
#define	HPPA_PID_KERNEL  2

#define KERNEL_ACCESS_ID 1


#define KERNEL_TEXT_PROT (TLB_AR_KRX | (KERNEL_ACCESS_ID << 1))
#define KERNEL_DATA_PROT (TLB_AR_KRW | (KERNEL_ACCESS_ID << 1))

/* Block TLB flags */
#define BLK_ICACHE	0
#define BLK_DCACHE	1
#define BLK_COMBINED	2
#define BLK_LCOMBINED	3

#define pmap_kernel()			(&kernel_pmap_store)
#define	pmap_resident_count(pmap)	((pmap)->stats.resident_count)
#define pmap_remove_attributes(pmap,start,end)
#define pmap_copy(dpmap,spmap,da,len,sa)
#define	pmap_update()

#define pmap_phys_address(x)	((x) << PGSHIFT)
#define pmap_phys_to_frame(x)	((x) >> PGSHIFT)

#define cache_align(x)		(((x) + 64) & ~(64 - 1))

#endif	/* _HPPA_PMAP_H_ */
