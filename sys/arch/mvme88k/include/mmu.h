/*	$OpenBSD: mmu.h,v 1.4 1999/02/09 06:36:27 smurph Exp $ */
/*
 * Ashura Project
 */
/*
 * HISTORY
 *
 * Original SCCS ID in ISEDL 
 * @(#)mmu.h 1.22		 90/09/20 19:13:34
 */

#ifndef	_MACHINE_MMU_
#define	_MACHINE_MMU_

/* for m88k_pgbytes, m8kk_pgshift */
#include <machine/vmparam.h> 


/*
 * Parameters which determine the 'geometry' of the M88K page tables in memory.
 */
#define SDT_BITS	10	/* M88K segment table size bits */
#define PDT_BITS	10	/* M88K page table size bits */
#define PG_BITS		M88K_PGSHIFT	/* M88K hardware page size bits */

/*
 * Shifts and masks for M88K (hardware) page
 */
/* M88K_PGBYTES, PG_SHIFT in vm_param.h */
#define M88K_PGOFSET	(M88K_PGBYTES-1)	/* offset into M88K page */
#define M88K_PGMASK	(~M88K_PGOFSET)		/* page mask */ 

/*
 * Convert byte address to page frame number
 */
#define M88K_BTOP(x)	(((unsigned) (x)) >> M88K_PGSHIFT)
#define M88K_PTOB(x)	(((unsigned) (x)) << M88K_PGSHIFT)

/* 
 * Round off or truncate to the nearest page. These will work for
 * either addresses of counts. (i.e. 1 byte round to 1 page bytes).
 */
#define M88K_TRUNC_PAGE(x)	(((unsigned) (x) & M88K_PGMASK))
#define M88K_ROUND_PAGE(x)	M88K_TRUNC_PAGE((x) + M88K_PGOFSET)

/*
 * M88K area descriptors
 */
typedef struct cmmu_apr {
	unsigned long
			st_base:20,	/* segment table base address */
			rsvA:2,		/* reserved */
			wt:1,		/* writethrough (cache control) */
			rsvB:1,		/* reserved */
			g:1,		/* global (cache control) */
			ci:1,		/* cache inhibit */
			rsvC:5,		/* reserved */
			te:1;		/* transration enable */
} cmmu_apr_t;

typedef union apr_template {
	cmmu_apr_t	field;
	unsigned long	bits;
} apr_template_t;

/*
 * M88K segment descriptors
 */
typedef struct sdt_entry {
	unsigned long
			table_addr:20,	/* page table base address */
			rsvA:2,		/* reserved */
			wt:1,		/* writethrough (cache control) */
			sup:1,		/* supervisor protection */
			g:1,		/* global (cache control) */
			no_cache:1,	/* cache inhibit */
			rsvB:3,		/* reserved */
			prot:1,		/* write protect */
			rsvC:1,		/* reserved */
			dtype:1;	/* valid */
} sdt_entry_t;

typedef union sdt_entry_template {
	sdt_entry_t	sdt_desc;
	unsigned long	bits;
} sdt_entry_template_t;

#define SDT_ENTRY_NULL	((sdt_entry_t *) 0)
	    
/*
 * M88K page descriptors
 */
typedef struct pt_entry {
	unsigned long
			pfn:20,		/* page frame address */
			rsvA:1,		/* reserved */
			wired:1,	/* wired bit <<software>> */
			wt:1,		/* writethrough (cache control) */
			sup:1,		/* supervisor protection */
			g:1,		/* global (cache control) */
			ci:1,		/* cache inhibit */
			rsvB:1,		/* reserved */
			modified:1,	/* modified */
			pg_used:1,	/* used (referenced) */
			prot:1,		/* write protect */
			rsvC:1,		/* reserved */
			dtype:1;	/* valid */
} pt_entry_t;

typedef union pte_template {
	pt_entry_t	pte;
	unsigned long	bits;
} pte_template_t;

#define PT_ENTRY_NULL	((pt_entry_t *) 0)

/*
 * 88200 PATC (TLB)
 */

#define PATC_ENTRIES	56

/*
 * M88K BATC entries
 */
typedef struct {
	unsigned long
			lba:13,		/* logical block address */
			pba:13,		/* physical block address */
			sup:1,		/* supervisor mode bit */
			wt:1,		/* writethrough (cache control) */
			g:1,		/* global (cache control) */
			ci:1,		/* cache inhibit */
			wp:1,		/* write protect */
			v:1;		/* valid */
} batc_entry_t;

typedef union batc_template {
	batc_entry_t	field;
	unsigned long	bits;
} batc_template_t;

/*
 * Parameters and macros for BATC
 */
#define BATC_BLKBYTES	(512*1024)	/* 'block' size of a BATC entry mapping */
#define BATC_BLKSHIFT	19		/* number of bits to BATC shift (log2(BATC_BLKBYTES)) */
#define BATC_BLKMASK	(BATC_BLKBYTES-1)	/* BATC block mask */

#define BATC_MAX	8		/* number of BATC entries */

#define BATC_BLK_ALIGNED(x)	((x & BATC_BLKMASK) == 0)

#define M88K_BTOBLK(x)	(x >> BATC_BLKSHIFT)

/*
 * protection codes (prot field)
 */
#define M88K_RO		1	/* read only */
#define M88K_RW		0	/* read/write */

/*
 * protection codes (sup field)
 */
#define M88K_SUPV	1	/* translation can only be done in supervisor mode */
#define M88K_USER	0	/* translation can be done supv. or user mode */

/*
 * descriptor types
 */
#define DT_INVALID	0
#define DT_VALID	1

/*
 * Number of entries in a page table.
 */
#define	SDT_ENTRIES	(1<<(SDT_BITS))
#define PDT_ENTRIES	(1<<(PDT_BITS))

/*
 * Size in bytes of a single page table.
 */
#define SDT_SIZE	(sizeof(sdt_entry_t) * SDT_ENTRIES)
#define PDT_SIZE	(sizeof(pt_entry_t) * PDT_ENTRIES)

/*
 * Shifts and masks 
 */
#define SDT_SHIFT	(PDT_BITS + PG_BITS)
#define PDT_SHIFT	(PG_BITS)

#define SDT_MASK	(((1<<SDT_BITS)-1) << SDT_SHIFT)
#define PDT_MASK	(((1<<PDT_BITS)-1) << PDT_SHIFT)

#define SDT_NEXT(va)	((va + (1<<SDT_SHIFT)) & SDT_MASK)
#define PDT_NEXT(va)	((va + (1<<PDT_SHIFT)) & (SDT_MASK|PDT_MASK))

#define	SDTIDX(va)	((va & SDT_MASK) >> SDT_SHIFT)
#define	PDTIDX(va)	((va & PDT_MASK) >> PDT_SHIFT)

#define SDTENT(map, va)	((sdt_entry_t *)(map->sdt_vaddr + SDTIDX(va)))

/*
 * Size of a PDT table group.
 */
#define LOG2_PDT_SIZE			(PDT_BITS + 2)
#define LOG2_PDT_TABLE_GROUP_SIZE	(PAGE_SHIFT - LOG2_PDT_SIZE)
#define PDT_TABLE_GROUP_SIZE		(1 << LOG2_PDT_TABLE_GROUP_SIZE)
#define PT_FREE(tbl)		kmem_free(kernel_map, (vm_offset_t)tbl, PAGE_SIZE)

/*
 * Va spaces mapped by tables and PDT table group.
 */
#define PDT_VA_SPACE			(PDT_ENTRIES * M88K_PGBYTES)
#define PDT_TABLE_GROUP_VA_SPACE	(PDT_VA_SPACE * PDT_TABLE_GROUP_SIZE)

/*
 * Number of sdt entries used to map user and kernel space.
 */
#define USER_SDT_ENTRIES	SDTIDX(VM_MIN_KERNEL_ADDRESS)
#define KERNEL_SDT_ENTRIES	(SDT_ENTRIES - USER_SDT_ENTRIES)

/*
 * Macros to check if the descriptor is valid.
 */
#define SDT_VALID(sd_ptr)	((sd_ptr)->dtype == DT_VALID)
#define PDT_VALID(pd_ptr)	((pd_ptr)->dtype == DT_VALID)

/*
 * Alignment checks for pages (must lie on page boundaries).
 */
#define PAGE_ALIGNED(ad)	(((vm_offset_t)(ad) & ~M88K_PGMASK) == 0)
#define	CHECK_PAGE_ALIGN(ad,who)	\
    if (!PAGE_ALIGNED(ad))		\
    	printf("%s: addr  %x not page aligned.\n", who, ad)

/*
 * Validate PTE's for all hardware pages in a VM page.
 * "ptes_per_vm_page" should be set in pmap_bootstrap.
 *
 * PARAMETERS:
 *		pt_entry_t 	*start;
 * 		unsigned long	template;
 */
#define DO_PTES(start, template)			\
{							\
    int i_;						\
    pt_entry_t *p_ = start;				\
							\
    for (i_ = ptes_per_vm_page; i_>0; i_--) {		\
	*(int *)p_++ = (unsigned long)(template);	\
	template += M88K_PGBYTES;			\
	/* (unsigned long)(template) for m88k C compiler\
		'90.7.24	Fuzzy		*/	\
    }							\
}

/*
 * Flags for cmmu_store() <cmmu.s>
 */
#define STORE_CMD	0
#define STORE_UAPR	4
#define STORE_SAPR	8
#define STORE_BATCWP	0x400

#define C_CMMU		0
#define D_CMMU		0x1000

/*
 * Parameters for ATC(TLB) fulsh
 */

#define CMMU_SCR	0x004

#define FLUSH_SUP_ALL	0x37
#define FLUSH_USR_ALL	0x33
#define FLUSH_SUP_SEG	0x36
#define FLUSH_USR_SEG	0x32
#define FLUSH_SUP_PG	0x35
#define FLUSH_USR_PG	0x31

/*
 * Cache coontrol bits for pte
 */
#define CACHE_DFL	0
#define CACHE_INH	0x40
#define CACHE_GLOBAL	0x80
#define CACHE_WT	0x200

#define CACHE_MASK	(~(unsigned)(CACHE_INH | CACHE_GLOBAL | CACHE_WT))

/*
 * Prototype for invalidate_pte found in "motorola/m88k/m88100/misc.s"
 */
unsigned invalidate_pte(pt_entry_t *pointer);

extern vm_offset_t kmapva;

#define kvtopte(va)	\
({ 									\
	sdt_entry_t *sdt; 						\
	sdt = (sdt_entry_t *)kmapva + SDTIDX(va) + SDT_ENTRIES;		\
	(pte_template_t *)(sdt->table_addr << PDT_SHIFT) + PDTIDX(va);	\
})


#define DMA_CACHE_SYNC		0x1
#define DMA_CACHE_SYNC_INVAL	0x2
#define DMA_CACHE_INV		0x3
extern void dma_cachectl(vm_offset_t, int, int);

#endif
/* endif _MACHINE_MMU_ */
