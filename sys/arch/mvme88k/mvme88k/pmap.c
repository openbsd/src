/*
 * Copyright (c) 1996 Nivas Madhur
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
 *      This product includes software developed by Nivas Madhur.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
/*
 * Mach Operating System
 * Copyright (c) 1991 Carnegie Mellon University
 * Copyright (c) 1991 OMRON Corporation
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 */

/* don't want to make them general yet. */
#ifdef luna88k
#  define OMRON_PMAP
#endif
#  define OMRON_PMAP

#include <sys/types.h>
#include <machine/board.h>
#include <sys/param.h>
#include <machine/m882xx.h>		/* CMMU stuff */
#include <vm/vm.h>
#include <vm/vm_kern.h>			/* vm/vm_kern.h */

#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/msgbuf.h>
#include <machine/assert.h>

#include <mvme88k/dev/pcctworeg.h>
#include <mvme88k/dev/clreg.h>

 /*
  *  VM externals
  */
extern vm_offset_t      avail_start, avail_next, avail_end;
extern vm_offset_t      virtual_avail, virtual_end;

extern vm_offset_t	pcc2consvaddr;
extern vm_offset_t	clconsvaddr;

char *iiomapbase;
int iiomapsize;

/*
 * Static variables, functions and variables for debugging
 */
#ifdef	DEBUG
#define	STATIC

/*
 * conditional debugging
 */

#define CD_NORM		0x01
#define CD_FULL 	0x02

#define CD_ACTIVATE	0x0000004	/* _pmap_activate */
#define CD_KMAP		0x0000008	/* pmap_expand_kmap */
#define CD_MAP		0x0000010	/* pmap_map */
#define CD_MAPB		0x0000020	/* pmap_map_batc */
#define CD_CACHE	0x0000040	/* pmap_cache_ctrl */
#define CD_BOOT		0x0000080	/* pmap_bootstrap */
#define CD_INIT		0x0000100	/* pmap_init */
#define CD_CREAT	0x0000200	/* pmap_create */
#define CD_FREE		0x0000400	/* pmap_free_tables */
#define CD_DESTR	0x0000800	/* pmap_destroy */
#define CD_RM		0x0001000	/* pmap_remove */
#define CD_RMAL		0x0002000	/* pmap_remove_all */
#define CD_COW		0x0004000	/* pmap_copy_on_write */
#define CD_PROT		0x0008000	/* pmap_protect */
#define CD_EXP		0x0010000	/* pmap_expand */
#define CD_ENT		0x0020000	/* pmap_enter */
#define CD_UPD		0x0040000	/* pmap_update */
#define CD_COL		0x0080000	/* pmap_collect */
#define CD_CMOD		0x0100000	/* pmap_clear_modify */
#define CD_IMOD		0x0200000	/* pmap_is_modified */
#define CD_CREF		0x0400000	/* pmap_clear_reference */
#define CD_PGMV		0x0800000	/* pagemove */
#define CD_CHKPV	0x1000000	/* check_pv_list */
#define CD_CHKPM	0x2000000	/* check_pmap_consistency */
#define CD_CHKM		0x4000000	/* check_map */
#define CD_ALL		0x0FFFFFC

/*int pmap_con_dbg = CD_ALL | CD_FULL | CD_COW | CD_BOOT;*/
int pmap_con_dbg = CD_NORM;
#else

#define	STATIC		static

#endif /* DEBUG */

caddr_t	vmmap;
pt_entry_t	*vmpte, *msgbufmap;

STATIC struct pmap 	kernel_pmap_store;
pmap_t kernel_pmap = &kernel_pmap_store;

typedef struct kpdt_entry *kpdt_entry_t;
struct kpdt_entry {
		kpdt_entry_t	next;
		vm_offset_t	phys;
};
#define	KPDT_ENTRY_NULL		((kpdt_entry_t)0)

STATIC kpdt_entry_t		kpdt_free;

/*
 * MAX_KERNEL_VA_SIZE must fit into the virtual address space between
 * VM_MIN_KERNEL_ADDRESS and VM_MAX_KERNEL_ADDRESS.
 */

#define	MAX_KERNEL_VA_SIZE	(256*1024*1024)	/* 256 Mb */

/*
 * Size of kernel page tables, which is enough to map MAX_KERNEL_VA_SIZE
 */
#define	KERNEL_PDT_SIZE	(M88K_BTOP(MAX_KERNEL_VA_SIZE) * sizeof(pt_entry_t))

/*
 * Size of kernel page tables for mapping onboard IO space.
 */
#define	OBIO_PDT_SIZE	(M88K_BTOP(OBIO_SIZE) * sizeof(pt_entry_t))

#define MAX_KERNEL_PDT_SIZE	(KERNEL_PDT_SIZE + OBIO_PDT_SIZE)

/*
 * Two pages of scratch space.
 * Used in copy_to_phys(), pmap_copy_page() and pmap_zero_page().
 */
vm_offset_t	phys_map_vaddr1, phys_map_vaddr2;

int		ptes_per_vm_page; /* no. of ptes required to map one VM page */

#define PMAP_MAX	512

/*
 *	The Modify List
 *
 * This is an array, one byte per physical page, which keeps track
 * of modified flags for pages which are no longer containd in any
 * pmap. (for mapped pages, the modified flags are in the PTE.)
 */
char	*pmap_modify_list;


/* 	The PV (Physical to virtual) List.
 *
 * For each vm_page_t, pmap keeps a list of all currently valid virtual
 * mappings of that page. An entry is a pv_entry_t; the list is the
 * pv_head_table. This is used by things like pmap_remove, when we must
 * find and remove all mappings for a particular physical page.
 */
typedef	struct pv_entry {
	struct pv_entry	*next;		/* next pv_entry */
	pmap_t		pmap;		/* pmap where mapping lies */
	vm_offset_t	va;		/* virtual address for mapping */
} *pv_entry_t;

#define PV_ENTRY_NULL	((pv_entry_t) 0)

static pv_entry_t	pv_head_table;	/* array of entries, one per page */

/*
 * Index into pv_head table, its lock bits, and the modify bits
 * starting at pmap_phys_start.
 */
#define PFIDX(pa)		(atop(pa - pmap_phys_start))
#define PFIDX_TO_PVH(pfidx)	(&pv_head_table[pfidx])


/*
 *	Locking and TLB invalidation primitives
 */

/*
 *	Locking Protocols:
 *
 *	There are two structures in the pmap module that need locking:
 *	the pmaps themselves, and the per-page pv_lists (which are locked
 *	by locking the pv_lock_table entry that corresponds to the pv_head
 *	for the list in question.)  Most routines want to lock a pmap and
 *	then do operations in it that require pv_list locking -- however
 *	pmap_remove_all and pmap_copy_on_write operate on a physical page
 *	basis and want to do the locking in the reverse order, i.e. lock
 *	a pv_list and then go through all the pmaps referenced by that list.
 *	To protect against deadlock between these two cases, the pmap_lock
 *	is used.  There are three different locking protocols as a result:
 *
 *  1.  pmap operations only (pmap_extract, pmap_access, ...)  Lock only
 *		the pmap.
 *
 *  2.  pmap-based operations (pmap_enter, pmap_remove, ...)  Get a read
 *		lock on the pmap_lock (shared read), then lock the pmap
 *		and finally the pv_lists as needed [i.e. pmap lock before
 *		pv_list lock.]
 *
 *  3.  pv_list-based operations (pmap_remove_all, pmap_copy_on_write, ...)
 *		Get a write lock on the pmap_lock (exclusive write); this
 *		also guaranteees exclusive access to the pv_lists.  Lock the
 *		pmaps as needed.
 *
 *	At no time may any routine hold more than one pmap lock or more than
 *	one pv_list lock.  Because interrupt level routines can allocate
 *	mbufs and cause pmap_enter's, the pmap_lock and the lock on the
 *	kernel_pmap can only be held at splvm.
 */
/* DCR: 12/18/91 - The above explanation is no longer true.  The pmap
 *	system lock has been removed in favor of a backoff strategy to
 *	avoid deadlock.  Now, pv_list-based operations first get the
 *	pv_list lock, then try to get the pmap lock, but if they can't,
 *	they release the pv_list lock and retry the whole operation.
 */

#define	SPLVM(spl)	{ spl = splvm(); }
#define	SPLX(spl)	{ splx(spl); }

#define PMAP_LOCK(pmap, spl)	SPLVM(spl)
#define PMAP_UNLOCK(pmap, spl)	SPLX(spl)

#define PV_LOCK_TABLE_SIZE(n)	0
#define LOCK_PVH(index)
#define UNLOCK_PVH(index)

#define ETHERPAGES 16
void	*etherbuf;
int	etherlen;

/*
 * First and last physical address that we maintain any information
 * for. Initalized to zero so that pmap operations done before
 * pmap_init won't touch any non-existent structures.
 */

static vm_offset_t	pmap_phys_start	= (vm_offset_t) 0;
static vm_offset_t	pmap_phys_end	= (vm_offset_t) 0;

#define PMAP_MANAGED(pa) (pmap_initialized && ((pa) >= pmap_phys_start && (pa) < pmap_phys_end))

/*
 *	This variable extract vax's pmap.c.
 *	pmap_verify_free refer to this.
 *	pmap_init initialize this.
 *	'90.7.17	Fuzzy
 */
boolean_t	pmap_initialized = FALSE;/* Has pmap_init completed? */

/*
 * Consistency checks.
 * These checks are disabled by default; enabled by setting CD_FULL
 * in pmap_con_dbg.
 */

#ifdef	DEBUG

static void check_pv_list __P((vm_offset_t, pv_entry_t, char *));
static void check_pmap_consistency __P((char *));

#define CHECK_PV_LIST(phys,pv_h,who) \
	if (pmap_con_dbg & CD_CHKPV) check_pv_list(phys,pv_h,who)
#define CHECK_PMAP_CONSISTENCY(who) \
	if (pmap_con_dbg & CD_CHKPM) check_pmap_consistency(who)
#else
#define CHECK_PV_LIST(phys,pv_h,who)
#define CHECK_PMAP_CONSISTENCY(who)
#endif /* DEBUG */

/*
 * number of BATC entries used
 */
int	batc_used;

/*
 * keep track BATC mapping
 */
batc_entry_t batc_entry[BATC_MAX];

int 	maxcmmu_pb = 4;	/* max number of CMMUs per processors pbus	*/
int 	n_cmmus_pb = 1;	/* number of CMMUs per processors pbus		*/

#define cpu_number()	0	/* just being lazy, should be taken out -nivas*/

vm_offset_t kmapva = 0;
extern vm_offset_t bugromva;
extern vm_offset_t sramva;
extern vm_offset_t obiova;

STATIC void
flush_atc_entry(unsigned users, vm_offset_t va, int kernel)
{
	cmmu_flush_remote_tlb(cpu_number(), kernel, va, M88K_PGBYTES);
}

/*
 * Routine:	_PMAP_ACTIVATE
 *
 * Author:	N. Sugai
 *
 * Function:
 *	Binds the given physical map to the given processor.
 *
 * Parameters:
 *	pmap	pointer to pmap structure
 *	p	pointer to proc structure
 *	cpu	CPU number
 *
 * If the specified pmap is not kernel_pmap, this routine makes arp
 * template and stores it into UAPR (user area pointer register) in the
 * CMMUs connected to the specified CPU.
 *
 * If kernel_pmap is specified, only flushes the TLBs mapping kernel
 * virtual space, in the CMMUs connected to the specified CPU.
 *
 * NOTE:
 * All of the code of this function extracted from macro PMAP_ACTIVATE
 * to make debugging easy. Accordingly, PMAP_ACTIVATE simlpy call
 * _pmap_activate.
 *
 */
void
_pmap_activate(pmap_t pmap, pcb_t pcb, int my_cpu)
{
    apr_template_t	apr_data;
    int 		n;

#ifdef DEBUG
    if ((pmap_con_dbg & (CD_ACTIVATE | CD_FULL)) == (CD_ACTIVATE | CD_NORM))
	printf("(_pmap_activate :%x) pmap 0x%x\n", curproc, (unsigned)pmap);
#endif
    
    if (pmap != kernel_pmap) {
	/*
	 *	Lock the pmap to put this cpu in its active set.
	 */
	simple_lock(&pmap->lock);

	apr_data.bits = 0;
	apr_data.field.st_base = M88K_BTOP(pmap->sdt_paddr); 
	apr_data.field.wt = 0;
	apr_data.field.g  = 1;
	apr_data.field.ci = 0;
	apr_data.field.te = 1;
#ifdef notyet
#ifdef OMRON_PMAP
	/*
	 * cmmu_pmap_activate will set the uapr and the batc entries, then
	 * flush the *USER* TLB.  IF THE KERNEL WILL EVER CARE ABOUT THE
	 * BATC ENTRIES, THE SUPERVISOR TLBs SHOULB BE FLUSHED AS WELL.
	 */
	cmmu_pmap_activate(my_cpu, apr_data.bits, pmap->i_batc, pmap->d_batc);
        for (n = 0; n < BATC_MAX; n++)
	    *(unsigned*)&batc_entry[n] = pmap->i_batc[n].bits;
#else
	cmmu_set_uapr(apr_data.bits);
	cmmu_flush_tlb(0, 0, -1);
#endif
#endif /* notyet */
	/*
	 * I am forcing it to not program the BATC at all. pmap.c module
	 * needs major, major cleanup. XXX nivas
	 */
	cmmu_set_uapr(apr_data.bits);
	cmmu_flush_tlb(0, 0, -1);

	/*
	 *	Mark that this cpu is using the pmap.
	 */
	simple_unlock(&pmap->lock);

    } else {

	/*
	 * kernel_pmap must be always active.
	 */

#ifdef DEBUG
    if ((pmap_con_dbg & (CD_ACTIVATE | CD_NORM)) == (CD_ACTIVATE | CD_NORM))
	printf("(_pmap_activate :%x) called for kernel_pmap\n", curproc);
#endif

    }
} /* _pmap_activate */

/*
 * Routine:	_PMAP_DEACTIVATE
 *
 * Author:	N. Sugai
 *
 * Function:
 *	Unbinds the given physical map to the given processor.
 *
 * Parameters:
 *	pmap	pointer to pmap structure
 *	th	pointer to thread structure
 *	cpu	CPU number
 *
 * _pmap_deactive simply clears the cpus_using field in given pmap structure.
 *
 * NOTE:
 * All of the code of this function extracted from macro PMAP_DEACTIVATE
 * to make debugging easy. Accordingly, PMAP_DEACTIVATE simlpy call
 * _pmap_deactivate.
 *
 */
void
_pmap_deactivate(pmap_t pmap, pcb_t pcb, int my_cpu)
{
    if (pmap != kernel_pmap) {
	/* Nothing to do */
    }
}

/*
 *  Author: Joe Uemura
 *	Convert machine-independent protection code to M88K protection bits.
 *
 *	History:
 *	'90.8.3	Fuzzy
 *			if defined TEST, 'static' undeclared.
 *	'90.8.30	Fuzzy
 *			delete "if defined TEST, 'static' undeclared."
 *
 */

STATIC unsigned int
m88k_protection(pmap_t map, vm_prot_t prot)
{
	pte_template_t p;

	p.bits = 0;
	p.pte.prot = (prot & VM_PROT_WRITE) ? 0 : 1;

	return(p.bits);

} /* m88k_protection */


/*
 * Routine:	PMAP_PTE
 *
 * Function:
 *	Given a map and a virtual address, compute a (virtual) pointer
 *	to the page table entry (PTE) which maps the address .
 *	If the page table associated with the address does not
 *	exist, PT_ENTRY_NULL is returned (and the map may need to grow).
 *
 * Parameters:
 *	pmap	pointer to pmap structure
 *	virt	virtual address for which page table entry is desired
 *
 *    Otherwise the page table address is extracted from the segment table,
 *    the page table index is added, and the result is returned.
 *
 * Calls:
 *	SDTENT
 *	SDT_VALID
 *	PDT_IDX
 */

pt_entry_t *
pmap_pte(pmap_t map, vm_offset_t virt)
{
	sdt_entry_t	*sdt;

	/*XXX will this change if physical memory is not contiguous? */
	/* take a look at PDTIDX XXXnivas */
	if (map == PMAP_NULL)
		panic("pmap_pte: pmap is NULL");

	sdt = SDTENT(map,virt);

	/*
	 * Check whether page table is exist or not.
	 */
	if (!SDT_VALID(sdt))
		return(PT_ENTRY_NULL);
	else
		return((pt_entry_t *)(((sdt + SDT_ENTRIES)->table_addr)<<PDT_SHIFT) +
				PDTIDX(virt));

} /* pmap_pte */


/*
 * Routine:	PMAP_EXPAND_KMAP (internal)
 *
 * Function:
 *    Allocate a page descriptor table (pte_table) and validate associated
 * segment table entry, returning pointer to page table entry. This is
 * much like 'pmap_expand', except that table space is acquired
 * from an area set up by pmap_bootstrap, instead of through
 * kmem_alloc. (Obviously, because kmem_alloc uses the kernel map
 * for allocation - which we can't do when trying to expand the
 * kernel map!) Note that segment tables for the kernel map were
 * all allocated at pmap_bootstrap time, so we only need to worry
 * about the page table here.
 *
 * Parameters:
 *	virt	VA for which translation tables are needed
 *	prot	protection attributes for segment entries
 *
 * Extern/Global:
 *	kpdt_free	kernel page table free queue
 *
 * Calls:
 *	m88k_protection
 *	SDTENT
 *	SDT_VALID
 *	PDT_IDX
 *
 * This routine simply dequeues a table from the kpdt_free list,
 * initalizes all its entries (invalidates them), and sets the
 * corresponding segment table entry to point to it. If the kpdt_free
 * list is empty - we panic (no other places to get memory, sorry). (Such
 * a panic indicates that pmap_bootstrap is not allocating enough table
 * space for the kernel virtual address space).
 *
 */

STATIC pt_entry_t *
pmap_expand_kmap(vm_offset_t virt, vm_prot_t prot)
{
    int			aprot;
    sdt_entry_t		*sdt;
    kpdt_entry_t	kpdt_ent;
    pmap_t		map = kernel_pmap;

#if	DEBUG
    if ((pmap_con_dbg & (CD_KMAP | CD_FULL)) == (CD_KMAP | CD_FULL))
	printf("(pmap_expand_kmap :%x) v %x\n", curproc,virt);
#endif

    aprot = m88k_protection (map, prot);

    /*  segment table entry derivate from map and virt. */
    sdt = SDTENT(map, virt);
    if (SDT_VALID(sdt))
	panic("pmap_expand_kmap: segment table entry VALID");

    kpdt_ent = kpdt_free;
    if (kpdt_ent == KPDT_ENTRY_NULL) {
	printf("pmap_expand_kmap: Ran out of kernel pte tables\n");
	return(PT_ENTRY_NULL);
    }
    kpdt_free = kpdt_free->next;

    ((sdt_entry_template_t *)sdt)->bits = kpdt_ent->phys | aprot | DT_VALID;
    ((sdt_entry_template_t *)(sdt + SDT_ENTRIES))->bits = (vm_offset_t)kpdt_ent | aprot | DT_VALID;
    (unsigned)(kpdt_ent->phys) = 0;
    (unsigned)(kpdt_ent->next) = 0;

    return((pt_entry_t *)(kpdt_ent) + PDTIDX(virt));
}/* pmap_expand_kmap() */

/*
 * Routine:	PMAP_MAP
 *
 * Function:
 *    Map memory at initalization. The physical addresses being
 * mapped are not managed and are never unmapped.
 *
 * Parameters:
 *	virt	virtual address of range to map    (IN)
 *	start	physical address of range to map   (IN)
 *	end	physical address of end of range   (IN)
 *	prot	protection attributes              (IN)
 *
 * Calls:
 *	pmap_pte
 *	pmap_expand_kmap
 *
 * Special Assumptions
 *	For now, VM is already on, only need to map the specified
 * memory. Used only by pmap_bootstrap() and vm_page_startup().
 *
 * For each page that needs mapping:
 *	pmap_pte is called to obtain the address of the page table
 *	table entry (PTE). If the page table does not exist,
 *	pmap_expand_kmap is called to allocate it. Finally, the page table
 *	entry is set to point to the physical page.
 *
 *
 *	initialize template with paddr, prot, dt
 *	look for number of phys pages in range
 *	{
 *		pmap_pte(virt)	- expand if necessary
 *		stuff pte from template
 *		increment virt one page
 *		increment template paddr one page
 *	}
 *
 */
vm_offset_t
pmap_map(vm_offset_t virt, vm_offset_t start, vm_offset_t end, vm_prot_t prot)
{
    int			aprot;
    unsigned		npages;
    unsigned		num_phys_pages;
    unsigned		cmode;
    pt_entry_t		*pte;
    pte_template_t	 template;

    /*
     * cache mode is passed in the top 16 bits.
     * extract it from there. And clear the top
     * 16 bits from prot.
     */
    cmode = (prot & 0xffff0000) >> 16;
    prot &= 0x0000ffff;

#if	DEBUG
    if ((pmap_con_dbg & (CD_MAP | CD_FULL)) == (CD_MAP | CD_FULL))
	printf ("(pmap_map :%x) phys address from %x to %x mapped at virtual %x, prot %x cmode %x\n",
		 curproc, start, end, virt, prot, cmode);
#endif

    if (start > end)
	panic("pmap_map: start greater than end address");

    aprot = m88k_protection (kernel_pmap, prot);

    template.bits = M88K_TRUNC_PAGE(start) | aprot | DT_VALID | cmode;

    npages = M88K_BTOP(M88K_ROUND_PAGE(end) - M88K_TRUNC_PAGE(start));

    for (num_phys_pages = npages; num_phys_pages > 0; num_phys_pages--) {

	if ((pte = pmap_pte(kernel_pmap, virt)) == PT_ENTRY_NULL)
	    if ((pte = pmap_expand_kmap(virt, VM_PROT_READ|VM_PROT_WRITE)) == PT_ENTRY_NULL)
		panic ("pmap_map: Cannot allocate pte table");

#ifdef	DEBUG
	if (pmap_con_dbg & CD_MAP)
	  if (pte->dtype)
		printf("(pmap_map :%x) pte @ 0x%x already valid\n", curproc, (unsigned)pte);
#endif

	*pte = template.pte;
	virt += M88K_PGBYTES;
	template.bits += M88K_PGBYTES;
    }

    return(virt);

} /* pmap_map() */

/*
 * Routine:	PMAP_MAP_BATC
 *
 * Function:
 *    Map memory using BATC at initalization. The physical addresses being
 * mapped are not managed and are never unmapped.
 *
 * Parameters:
 *	virt	virtual address of range to map    (IN)
 *	start	physical address of range to map   (IN)
 *	end	physical address of end of range   (IN)
 *	prot	protection attributes              (IN)
 *	cmode	cache control attributes	   (IN)
 *
 * External & Global:
 *	batc_used	number of BATC used	(IN/OUT)
 *
 * Calls:
 *	m88k_protection
 *	BATC_BLK_ALIGNED
 *	cmmu_store
 *	pmap_pte
 *	pmap_expand_kmap
 *
 *
 * For each page that needs mapping:
 *	If both virt and phys are on the BATC block boundary, map using BATC.
 *	Else make mapping in the same manner as pmap_map.
 *
 *	initialize BATC and pte template
 *	look for number of phys pages in range
 *	{
 *		if virt and phys are on BATC block boundary
 *		{
 *			map using BATC
 *			increment virt and phys one BATC block
 *			continue outer loop
 *		}
 *		pmap_pte(virt)	- expand if necessary
 *		stuff pte from template
 *		increment virt one page
 *		increment template paddr one page
 *	}
 *
 */
vm_offset_t
pmap_map_batc(vm_offset_t virt, vm_offset_t start, vm_offset_t end,
					vm_prot_t prot, unsigned cmode)
{
    int			aprot;
    unsigned		num_phys_pages;
    vm_offset_t		phys;
    pt_entry_t		*pte;
    pte_template_t	template;
    batc_template_t	batctmp;
    register int	i;

#if	DEBUG
    if ((pmap_con_dbg & (CD_MAPB | CD_FULL)) == (CD_MAPB | CD_FULL))
      printf ("(pmap_map_batc :%x) phys address from %x to %x mapped at virtual %x, prot %x\n", curproc,
	      start, end, virt, prot);
#endif

    if (start > end)
      panic("pmap_map_batc: start greater than end address");

    aprot = m88k_protection (kernel_pmap, prot);
    template.bits = M88K_TRUNC_PAGE(start) | aprot | DT_VALID | cmode;
    phys = start;
    batctmp.bits = 0;
    batctmp.field.sup = 1;			/* supervisor */
    batctmp.field.wt = template.pte.wt;		/* write through */
    batctmp.field.g = template.pte.g;		/* global */
    batctmp.field.ci = template.pte.ci;		/* cache inhibit */
    batctmp.field.wp = template.pte.prot;	/* protection */
    batctmp.field.v = 1;			/* valid */

    num_phys_pages = M88K_BTOP(M88K_ROUND_PAGE(end) - M88K_TRUNC_PAGE(start));

    while (num_phys_pages > 0) {

#ifdef DEBUG
	if ((pmap_con_dbg & (CD_MAPB | CD_FULL)) == (CD_MAPB | CD_FULL))
	  printf("(pmap_map_batc :%x) num_phys_pg=%x, virt=%x, aligne V=%d, phys=%x, aligne P=%d\n", curproc,
	       num_phys_pages, virt, BATC_BLK_ALIGNED(virt), phys, BATC_BLK_ALIGNED(phys));
#endif

	if ( BATC_BLK_ALIGNED(virt) && BATC_BLK_ALIGNED(phys) && 
	    num_phys_pages >= BATC_BLKBYTES/M88K_PGBYTES &&
	    batc_used < BATC_MAX ) {

	    /*
	     * map by BATC
	     */
	    batctmp.field.lba = M88K_BTOBLK(virt);
	    batctmp.field.pba = M88K_BTOBLK(phys);

	    cmmu_set_pair_batc_entry(0, batc_used, batctmp.bits);

	    batc_entry[batc_used] = batctmp.field;

#ifdef DEBUG
	    if ((pmap_con_dbg & (CD_MAPB | CD_NORM)) == (CD_MAPB | CD_NORM)) {
		printf("(pmap_map_batc :%x) BATC used=%d, data=%x\n", curproc, batc_used, batctmp.bits);
	    }
	    if (pmap_con_dbg & CD_MAPB) {

		for (i = 0; i < BATC_BLKBYTES; i += M88K_PGBYTES ) {
		    pte = pmap_pte(kernel_pmap, virt+i);
		    if (pte->dtype)
		      printf("(pmap_map_batc :%x) va %x is already mapped : pte %x\n", curproc, virt+i, ((pte_template_t *)pte)->bits);
		}
	    }
#endif
	    batc_used++;
    	    virt += BATC_BLKBYTES;
	    phys += BATC_BLKBYTES;
	    template.pte.pfn = M88K_BTOP(phys);
	    num_phys_pages -= BATC_BLKBYTES/M88K_PGBYTES;
	    continue;
	}
	if ((pte = pmap_pte(kernel_pmap, virt)) == PT_ENTRY_NULL)
	  if ((pte = pmap_expand_kmap(virt, VM_PROT_READ|VM_PROT_WRITE)) == PT_ENTRY_NULL)
	    panic ("pmap_map_batc: Cannot allocate pte table");

#ifdef	DEBUG
	if (pmap_con_dbg & CD_MAPB)
	  if (pte->dtype)
		printf("(pmap_map_batc :%x) pte @ 0x%x already valid\n", curproc, (unsigned)pte);
#endif

	*pte = template.pte;
	virt += M88K_PGBYTES;
	phys += M88K_PGBYTES;
	template.bits += M88K_PGBYTES;
	num_phys_pages--;
    }

    return(M88K_ROUND_PAGE(virt));

} /* pmap_map_batc() */

/*
 * Routine:	PMAP_CACHE_CONTROL
 *
 * Function:
 *	Set the cache-control bits in the page table entries(PTE) which maps
 *	the specifid virutal address range.
 *
 * 	mode
 *		writethrough	0x200
 *		global		0x80
 *		cache inhibit	0x40
 *
 * Parameters:
 *	pmap_t		map
 *	vm_offset_t	s
 *	vm_offset_t	e
 *	unsigned	mode
 *
 * Calls:
 *	PMAP_LOCK
 *	PMAP_UNLOCK
 *	pmap_pte
 *	invalidate_pte
 *	flush_atc_entry
 *	dcachefall
 *
 *  This routine sequences through the pages of the specified range.
 * For each, it calls pmap_pte to acquire a pointer to the page table
 * entry (PTE). If the PTE is invalid, or non-existant, nothing is done.
 * Otherwise, the cache-control bits in the PTE's are adjusted as specified.
 *
 */
void
pmap_cache_ctrl(pmap_t pmap, vm_offset_t s, vm_offset_t e, unsigned mode)
{
    int		spl, spl_sav;
    pt_entry_t	*pte;
    vm_offset_t	va;
    int				kflush;
    int				cpu;
    register pte_template_t	opte;

#ifdef DEBUG
    if ( mode & CACHE_MASK ) {
	printf("(cache_ctrl) illegal mode %x\n",mode);
	return;
    }
    if ((pmap_con_dbg & (CD_CACHE | CD_NORM)) == (CD_CACHE | CD_NORM)) {
	printf("(pmap_cache_ctrl :%x) pmap %x, va %x, mode %x\n", curproc, pmap, s, mode);
    }
#endif /* DEBUG */

    if ( pmap == PMAP_NULL ) {
	panic("pmap_cache_ctrl: pmap is NULL");
    }

    PMAP_LOCK(pmap, spl);

    if (pmap == kernel_pmap) {
	kflush = 1;
    } else {
	kflush = 0;
    }

    for (va = s; va < e; va += M88K_PGBYTES) {
	if ((pte = pmap_pte(pmap, va)) == PT_ENTRY_NULL)
	    continue;
#ifdef DEBUG
    if ((pmap_con_dbg & (CD_CACHE | CD_NORM)) == (CD_CACHE | CD_NORM)) {
	printf("(cache_ctrl) pte@0x%08x\n",(unsigned)pte);
    }
#endif /* DEBUG */

	/*
	 * Invalidate pte temporarily to avoid being written back
	 * the modified bit and/or the reference bit by other cpu.
	 *  XXX
	 */
	spl_sav = splimp();
	opte.bits = invalidate_pte(pte);
	((pte_template_t *)pte)->bits = (opte.bits & CACHE_MASK) | mode;
	flush_atc_entry(0, va, kflush);
	splx(spl_sav);

	/*
	 * Data cache should be copied back and invalidated.
	 */
        cmmu_flush_remote_cache(0, M88K_PTOB(pte->pfn), M88K_PGBYTES);
    }

    PMAP_UNLOCK(pmap, spl);

} /* pmap_cache_ctrl */


/*
 * Routine:	PMAP_BOOTSTRAP
 *
 * Function:
 *	Bootstarp the system enough to run with virtual memory.
 *	Map the kernel's code and data, allocate the kernel
 *	translation table space, and map control registers
 *	and other IO addresses.
 *
 * Parameters:
 *	load_start	PA where kernel was loaded (IN)
 *	&phys_start	PA of first available physical page (IN/OUT)
 *	&phys_end	PA of last available physical page (IN)
 *	&virtual_avail	VA of first available page (after kernel bss)
 *	&virtual_end	VA of last available page (end of kernel address space)
 *
 * Extern/Global:
 *
 *	PAGE_SIZE	VM (software) page size (IN)
 *	kernelstart	start symbol of kernel text (IN)
 *	etext		end of kernel text (IN)
 *	phys_map_vaddr1 VA of page mapped arbitrarily for debug/IO (OUT)
 *	phys_map_vaddr2 VA of page mapped arbitrarily for debug/IO (OUT)
 *
 * Calls:
 *	simple_lock_init
 *	pmap_map
 *	pmap_map_batc
 *
 *    The physical address 'load_start' is mapped at
 * VM_MIN_KERNEL_ADDRESS, which maps the kernel code and data at the
 * virtual address for which it was (presumably) linked. Immediately
 * following the end of the kernel code/data, sufficent page of
 * physical memory are reserved to hold translation tables for the kernel
 * address space. The 'phys_start' parameter is adjusted upward to
 * reflect this allocation. This space is mapped in virtual memory
 * immediately following the kernel code/data map.
 *
 *    A pair of virtual pages are reserved for debugging and IO
 * purposes. They are arbitrarily mapped when needed. They are used,
 * for example, by pmap_copy_page and pmap_zero_page.
 *
 * For m88k, we have to map BUG memory also. This is a read only 
 * mapping for 0x10000 bytes. We will end up having load_start as
 * 0 and VM_MIN_KERNEL_ADDRESS as 0 - yes sir, we have one-to-one
 * mapping!!!
 */

void
pmap_bootstrap(vm_offset_t	load_start,	/* IN */
	       vm_offset_t	*phys_start,	/* IN/OUT */
	       vm_offset_t	*phys_end,	/* IN */
	       vm_offset_t	*virt_start,	/* OUT */
	       vm_offset_t	*virt_end)	/* OUT */
{
    kpdt_entry_t	kpdt_virt;
    sdt_entry_t		*kmap;
    vm_offset_t		vaddr,
    			virt,
    			kpdt_phys,
    			s_text,
    			e_text,
    			kernel_pmap_size,
    			etherpa;
    apr_template_t	apr_data;
    pt_entry_t		*pte;
    int			i;
    u_long		foo;
    extern char	*kernelstart, *etext;
    extern void cmmu_go_virt(void);

#ifdef DEBUG 
    if ((pmap_con_dbg & (CD_BOOT | CD_NORM)) == (CD_BOOT | CD_NORM)) {
	printf("pmap_bootstrap : \"load_start\" 0x%x\n", load_start);
    }
#endif
    ptes_per_vm_page = PAGE_SIZE >> M88K_PGSHIFT;
    if (ptes_per_vm_page == 0){
	panic("pmap_bootstrap: VM page size < MACHINE page size");
    }
    if (!PAGE_ALIGNED(load_start)) {
	panic("pmap_bootstrap : \"load_start\" not on the m88k page boundary : 0x%x\n", load_start);
    }

    /*
     * Allocate the kernel page table from the front of available
     * physical memory,
     * i.e. just after where the kernel image was loaded.
     */
    /*
     * The calling sequence is 
     *    ...
     *  pmap_bootstrap(&kernelstart,...) 
     *  kernelstart is the first symbol in the load image.
     *  We link the kernel such that &kernelstart == 0x10000 (size of
     *							BUG ROM)
     *  The expression (&kernelstart - load_start) will end up as
     *	0, making *virt_start == *phys_start, giving a 1-to-1 map)
     */

    *phys_start = M88K_ROUND_PAGE(*phys_start);
    *virt_start = *phys_start +
   		 (M88K_TRUNC_PAGE((unsigned)&kernelstart) - load_start);

    /*
     * Initialilze kernel_pmap structure
     */
    kernel_pmap->ref_count = 1;
    kernel_pmap->sdt_paddr = kmap = (sdt_entry_t *)(*phys_start);
    kernel_pmap->sdt_vaddr = (sdt_entry_t *)(*virt_start);
    kmapva = *virt_start;

#ifdef DEBUG
    if ((pmap_con_dbg & (CD_BOOT | CD_FULL)) == (CD_BOOT | CD_FULL)) {
	printf("kernel_pmap->sdt_paddr = %x\n",kernel_pmap->sdt_paddr);
	printf("kernel_pmap->sdt_vaddr = %x\n",kernel_pmap->sdt_vaddr);
    }
    /* init double-linked list of pmap structure */
    kernel_pmap->next = kernel_pmap;
    kernel_pmap->prev = kernel_pmap;
#endif

    /* 
     * Reserve space for segment table entries.
     * One for the regular segment table and one for the shadow table
     * The shadow table keeps track of the virtual address of page
     * tables. This is used in virtual-to-physical address translation
     * functions. Remember, MMU cares only for physical addresses of
     * segment and page table addresses. For kernel page tables, we
     * really don't need this virtual stuff (since the kernel will
     * be mapped 1-to-1) but for user page tables, this is required.
     * Just to be consistent, we will maintain the shadow table for
     * kernel pmap also.
     */

    kernel_pmap_size = 2*SDT_SIZE;

    /* save pointers to where page table entries start in physical memory */
    kpdt_phys = (*phys_start + kernel_pmap_size);
    kpdt_virt = (kpdt_entry_t)(*virt_start + kernel_pmap_size);
    kernel_pmap_size += MAX_KERNEL_PDT_SIZE;
    *phys_start += kernel_pmap_size;
    *virt_start += kernel_pmap_size;

    /* init all segment and page descriptor to zero */
    bzero(kernel_pmap->sdt_vaddr, kernel_pmap_size);

#ifdef DEBUG
    if ((pmap_con_dbg & (CD_BOOT | CD_FULL)) == (CD_BOOT | CD_FULL)) {
	printf("kpdt_phys = %x\n",kpdt_phys);
	printf("kpdt_virt = %x\n",kpdt_virt);
    	printf("end of kpdt at (virt)0x%08x  ; (phys)0x%08x\n",
		*virt_start,*phys_start);
    }
#endif
    /*
     * init the kpdt queue
     */
    kpdt_free = kpdt_virt;
    for (i = MAX_KERNEL_PDT_SIZE/PDT_SIZE; i>0; i--) {
	kpdt_virt->next = (kpdt_entry_t)((vm_offset_t)kpdt_virt + PDT_SIZE);
	kpdt_virt->phys = kpdt_phys;
	kpdt_virt = kpdt_virt->next;
	kpdt_phys += PDT_SIZE;
    }
    kpdt_virt->next = KPDT_ENTRY_NULL; /* terminate the list */

    /*
     * Map the kernel image into virtual space
     */

    s_text = load_start;			/* paddr of text */
    e_text = load_start + ((unsigned)&etext -
		M88K_TRUNC_PAGE((unsigned)&kernelstart));
					/* paddr of end of text section*/
    e_text = M88K_ROUND_PAGE(e_text);

#ifdef OMRON_PMAP
#define PMAPER	pmap_map
#else
#define PMAPER	pmap_map_batc
#endif

    /*  map the first 64k (BUG ROM) read only, cache inhibited (? XXX) */
    vaddr = PMAPER(
		0,
		0,
		0x10000,
		(VM_PROT_WRITE | VM_PROT_READ)|(CACHE_INH <<16));

    assert(vaddr == M88K_TRUNC_PAGE((unsigned)&kernelstart));

    vaddr = PMAPER(
		(vm_offset_t)M88K_TRUNC_PAGE(((unsigned)&kernelstart)),
		s_text,
		e_text,
		VM_PROT_WRITE | VM_PROT_READ|(CACHE_GLOBAL<<16));	/* shouldn't it be RO? XXX*/

    vaddr = PMAPER(
		vaddr,
		e_text,
		(vm_offset_t)kmap,
		(VM_PROT_WRITE|VM_PROT_READ)|(CACHE_GLOBAL << 16));

    /*
     * Map system segment & page tables - should be cache inhibited?
     * 88200 manual says that CI bit is driven on the Mbus while accessing
     * the translation tree. I don't think we need to map it CACHE_INH
     * here...
     */
    if (kmapva != vaddr) {
#ifdef DEBUG
    	if ((pmap_con_dbg & (CD_BOOT | CD_FULL)) == (CD_BOOT | CD_FULL)) {
	    printf("(pmap_bootstrap) correcting vaddr\n");
	}
#endif
	while (vaddr < (*virt_start - kernel_pmap_size))
	  vaddr = M88K_ROUND_PAGE(vaddr + 1);
    }

    vaddr = PMAPER(
		vaddr,
		(vm_offset_t)kmap,
		*phys_start,
		(VM_PROT_WRITE|VM_PROT_READ)|(CACHE_GLOBAL << 16));
	
    etherlen = ETHERPAGES * NBPG;

    /*
     *	Get ethernet buffer - need etherlen bytes physically contiguous.
     *  1 to 1 mapped as well???. There is actually a bug in the macros
     *  used by the 1x7 ethernet driver. Remove this when that is fixed.
     *  XXX -nivas
     */

    if (vaddr != *virt_start) {
#ifdef DEBUG
        if ((pmap_con_dbg & (CD_BOOT | CD_FULL)) == (CD_BOOT | CD_FULL)) {
 	    printf("1:vaddr %x *virt_start %x *phys_start %x\n", vaddr,
		*virt_start, *phys_start);
	}
#endif
	*virt_start = vaddr;
	*phys_start = round_page(*phys_start);
    }

    *phys_start = vaddr;

    etherbuf = (void *)vaddr;

    vaddr = PMAPER(
		vaddr,
		*phys_start,
		*phys_start + etherlen,
		(VM_PROT_WRITE|VM_PROT_READ)|(CACHE_INH << 16));

    *virt_start += etherlen;
    *phys_start += etherlen;

    if (vaddr != *virt_start) {
#ifdef DEBUG
        if ((pmap_con_dbg & (CD_BOOT | CD_FULL)) == (CD_BOOT | CD_FULL)) {
	     printf("2:vaddr %x *virt_start %x *phys_start %x\n", vaddr,
		*virt_start, *phys_start);
	}
#endif
	*virt_start = vaddr;
	*phys_start = round_page(*phys_start);
    }

    *virt_start = round_page(*virt_start);
    *virt_end = VM_MAX_KERNEL_ADDRESS;

    /*
     * Map a few more pages for phys routines and debugger.
     */

    phys_map_vaddr1 = round_page(*virt_start);
    phys_map_vaddr2 = phys_map_vaddr1 + PAGE_SIZE;

    /*
     * To make 1:1 mapping of virt:phys, throw away a few phys pages.
     * XXX what is this? nivas
     */

   
    *phys_start += 2 * PAGE_SIZE;
    *virt_start += 2 * PAGE_SIZE;

    /*
     * Map all IO space 1-to-1. Ideally, I would like to not do this
     * but have va for the given IO address dynamically allocated. But
     * on the 88200, 2 of the BATCs are hardwired to do map the IO space
     * 1-to-1; I decided to map the rest of the IO space 1-to-1.
     * And bug ROM & the SRAM need to be mapped 1-to-1 if we ever want to
     * execute bug system calls after the MMU has been turned on.
     * OBIO should be mapped cache inhibited.
     */

    PMAPER(
            BUGROM_START,
            BUGROM_START,
            BUGROM_START + BUGROM_SIZE,
            VM_PROT_WRITE|VM_PROT_READ|(CACHE_INH << 16));

    PMAPER(
            SRAM_START,
            SRAM_START,
            SRAM_START + SRAM_SIZE,
            VM_PROT_WRITE|VM_PROT_READ|(CACHE_GLOBAL << 16));

    PMAPER(
            OBIO_START,
            OBIO_START,
            OBIO_START + OBIO_SIZE,
            VM_PROT_WRITE|VM_PROT_READ|(CACHE_INH << 16));

	/*
	 * Allocate all the submaps we need. Note that SYSMAP just allocates
	 * kernel virtual address with no physical backing memory. The idea
	 * is physical memory will be mapped at this va before using that va.
	 * This means that if different physcal pages are going to be mapped
	 * at different times, we better do a tlb flush before using it -	         * else we will be referencing the wrong page.
	 */

#define	SYSMAP(c, p, v, n)	\
({ \
	v = (c)virt; \
    	if ((p = pmap_pte(kernel_pmap, virt)) == PT_ENTRY_NULL) \
    		pmap_expand_kmap(virt, VM_PROT_READ|VM_PROT_WRITE|(CACHE_GLOBAL << 16)); \
	virt += ((n)*NBPG); \
})

	virt = *virt_start;

	SYSMAP(caddr_t, vmpte , vmmap, 1);
	SYSMAP(struct msgbuf *,	msgbufmap ,msgbufp, 1);

	vmpte->pfn = -1;
	vmpte->dtype = DT_INVALID;
	
	*virt_start = virt;

    /*
     * Set translation for UPAGES at UADDR. The idea is we want to
     * have translations set up for UADDR. Later on, the ptes for
     * for this address will be set so that kstack will refer
     * to the u area. Make sure pmap knows about this virtual
     * address by doing vm_findspace on kernel_map.
     */

    for (i = 0, virt = UADDR; i < UPAGES; i++, virt += PAGE_SIZE) {
#ifdef DEBUG
    	if ((pmap_con_dbg & (CD_BOOT | CD_FULL)) == (CD_BOOT | CD_FULL)) {
		printf("setting up mapping for Upage %d @ %x\n", i, virt);
    	}
#endif
    	if ((pte = pmap_pte(kernel_pmap, virt)) == PT_ENTRY_NULL)
    		pmap_expand_kmap(virt, VM_PROT_READ|VM_PROT_WRITE|(CACHE_GLOBAL << 16));
    }

    /*
     * Switch to using new page tables
     */

    apr_data.bits = 0;
    apr_data.field.st_base = M88K_BTOP(kernel_pmap->sdt_paddr);
    apr_data.field.wt = 1;
    apr_data.field.g  = 1;
    apr_data.field.ci = 0;
    apr_data.field.te = 1;	/* Translation enable */
#ifdef DEBUG
    if ((pmap_con_dbg & (CD_BOOT | CD_FULL)) == (CD_BOOT | CD_FULL)) {
	void show_apr(unsigned value);
	show_apr(apr_data.bits);
    }
#endif 
    /* Invalidate entire kernel TLB. */
#ifdef DEBUG
    if ((pmap_con_dbg & (CD_BOOT | CD_FULL)) == (CD_BOOT | CD_FULL)) {
	printf("invalidating tlb %x\n", apr_data.bits);
    }
#endif

    cmmu_flush_remote_tlb(0, 1, 0, -1);

#ifdef DEBUG
    if ((pmap_con_dbg & (CD_BOOT | CD_FULL)) == (CD_BOOT | CD_FULL)) {
	printf("done invalidating tlb %x\n", apr_data.bits);
    }
#endif

    /*
     * Set valid bit to DT_INVALID so that the very first pmap_enter()
     * on these won't barf in pmap_remove_range().
     */
    pte = pmap_pte(kernel_pmap, phys_map_vaddr1);
    pte->pfn = -1;
    pte->dtype = DT_INVALID;
    pte = pmap_pte(kernel_pmap, phys_map_vaddr2);
    pte->dtype = DT_INVALID;
    pte->pfn = -1;

    /* still physical */ 
    /* Load supervisor pointer to segment table. */
    cmmu_remote_set_sapr(0, apr_data.bits);
    /* virtual now on */
#ifdef DEBUG
    if ((pmap_con_dbg & (CD_BOOT | CD_FULL)) == (CD_BOOT | CD_FULL)) {
        printf("running virtual - avail_next 0x%x\n", *phys_start);
    }
#endif
    avail_next = *phys_start;

    return;
    
} /* pmap_bootstrap() */

/*
 * Bootstrap memory allocator. This function allows for early dynamic
 * memory allocation until the virtual memory system has been bootstrapped.
 * After that point, either kmem_alloc or malloc should be used. This
 * function works by stealing pages from the (to be) managed page pool,
 * stealing virtual address space, then mapping the pages and zeroing them.
 *
 * It should be used from pmap_bootstrap till vm_page_startup, afterwards
 * it cannot be used, and will generate a panic if tried. Note that this
 * memory will never be freed, and in essence it is wired down.
 */

void *
pmap_bootstrap_alloc(int size)
{
	register void *mem;

	size = round_page(size);
	mem = (void *)virtual_avail;
	virtual_avail = pmap_map(virtual_avail, avail_start,
				avail_start + size,
				VM_PROT_READ|VM_PROT_WRITE|(CACHE_GLOBAL << 16));
	avail_start += size;
#ifdef DEBUG
    	if ((pmap_con_dbg & (CD_BOOT | CD_FULL)) == (CD_BOOT | CD_FULL)) {
		printf("pmap_bootstrap_alloc: size %x virtual_avail %x avail_start %x\n",
			size, virtual_avail, avail_start);
    	}
#endif
	bzero((void *)mem, size);
	return (mem);
}

/*
 * Routine:	PMAP_INIT
 *
 * Function:
 *	Initialize the pmap module. It is called by vm_init, to initialize
 *	any structures that the pmap system needs to map virtual memory.
 *
 * Parameters:
 *	phys_start	physical address of first available page
 *			(was last set by pmap_bootstrap)
 *	phys_end	physical address of last available page
 *
 * Extern/Globals
 *	pv_head_table (OUT)
 *	pv_lock_table (OUT)
 *	pmap_modify_list (OUT)
 *	pmap_phys_start (OUT)
 *	pmap_phys_end (OUT)
 *	pmap_initialized(OUT)
 *
 * Calls:
 *	kmem_alloc
 *	zinit
 *
 *   This routine does not really have much to do. It allocates space
 * for the pv_head_table, pv_lock_table, pmap_modify_list; and sets these
 * pointers. It also initializes zones for pmap structures, pv_entry
 * structures, and segment tables.
 *
 *  Last, it sets the pmap_phys_start and pmap_phys_end global
 * variables. These define the range of pages 'managed' be pmap. These
 * are pages for which pmap must maintain the PV list and the modify
 * list. (All other pages are kernel-specific and are permanently
 * wired.)
 *
 *
 *	kmem_alloc() memory for pv_table
 * 	kmem_alloc() memory for modify_bits
 *	zinit(pmap_zone)
 *	zinit(segment zone)
 *
 */
void
pmap_init(vm_offset_t phys_start, vm_offset_t phys_end)
{
    register long		npages;
    register vm_offset_t    	addr;
    register vm_size_t		s;
    register int		i;
    vm_size_t			pvl_table_size;

#ifdef	DEBUG
    if ((pmap_con_dbg & (CD_INIT | CD_NORM)) == (CD_INIT | CD_NORM))
	printf("(pmap_init) phys_start %x  phys_end %x\n", phys_start, phys_end);
#endif

    /*
     * Allocate memory for the pv_head_table,
     * the modify bit array, and the pte_page table.
     */
    npages = atop(phys_end - phys_start);
    pvl_table_size = PV_LOCK_TABLE_SIZE(npages);
    s = (vm_size_t)(npages * sizeof(struct pv_entry)	/* pv_list */
	+ npages);					/* pmap_modify_list */

#ifdef	DEBUG
    if ((pmap_con_dbg & (CD_INIT | CD_FULL)) == (CD_INIT | CD_FULL)) {
	printf("(pmap_init) nbr of managed pages = %x\n", npages);
	printf("(pmap_init) size of pv_list = %x\n",
	       npages * sizeof(struct pv_entry));
    }
#endif

    s = round_page(s);
    addr = (vm_offset_t)kmem_alloc(kernel_map, s);

    pv_head_table =  (pv_entry_t)addr;
    addr = (vm_offset_t)(pv_head_table + npages);

    pmap_modify_list = (char *)addr;

    /*
     * Only now, when all of the data structures are allocated,
     * can we set pmap_phys_start and pmap_phys_end. If we set them
     * too soon, the kmem_alloc above will blow up when it causes
     * a call to pmap_enter, and pmap_enter tries to manipulate the
     * (not yet existing) pv_list.
     */
    pmap_phys_start = phys_start;
    pmap_phys_end = phys_end;

    pmap_initialized = TRUE;

} /* pmap_init() */


/*
 * Routine:	PMAP_ZERO_PAGE
 *
 * History:
 *	'90.7.13	Fuzzy
 *	'90.9.05	Fuzzy
 *		Bug: template page invalid --> template page valid
 *
 *	template = M88K_TRUNC_PAGE(phys)
 *	   | m88k_protection (kernel_pmap, VM_PROT_READ | VM_PROT_WRITE)
 *	   | DT_VALID;
 *	     ^^^^^^^^ add
 *
 * Function:
 *	Zeros the specified (machine independent) page.
 *
 * Parameters:
 *	phys		PA of page to zero
 *
 * Extern/Global:
 *	phys_map_vaddr1
 *
 * Calls:
 *	M88K_TRUNC_PAGE
 *	m88k_protection
 *	cmmu_sflush_page
 *	DO_PTES
 *	bzero
 *
 * Special Assumptions:
 *	no locking required
 *
 *	This routine maps the physical pages ath the 'phys_map' virtual
 * address set up in pmap_bootstrap. It flushes the TLB to make the new
 * mappings effective, and zeros all the bits.
 */
void
pmap_zero_page(vm_offset_t phys)
{
	vm_offset_t	srcva;
	pte_template_t	template;
	unsigned int i;
	unsigned int spl_sav;

	register int	my_cpu = cpu_number();
	pt_entry_t	*srcpte;

	srcva = (vm_offset_t)(phys_map_vaddr1 + (my_cpu * PAGE_SIZE));
	srcpte = pmap_pte(kernel_pmap, srcva);

	for (i = 0; i < ptes_per_vm_page; i++, phys += M88K_PGBYTES)
	  {
	    template.bits = M88K_TRUNC_PAGE(phys)
	      | m88k_protection (kernel_pmap, VM_PROT_READ | VM_PROT_WRITE)
		| DT_VALID | CACHE_GLOBAL;


	    spl_sav = splimp();
	    cmmu_flush_tlb(1, srcva, M88K_PGBYTES);
	    *srcpte = template.pte;
	    splx(spl_sav);
	    bzero (srcva, M88K_PGBYTES);
	    /* force the data out */
	    cmmu_flush_remote_data_cache(my_cpu,phys, M88K_PGBYTES);
	  }

} /* pmap_zero_page() */


/*
 * Routine:	PMAP_CREATE
 *
 * Author:	Fuzzy
 *
 * History:
 *	'90.7.13	Fuzzy	level 1 --> segment exchange
 *	'90.7.16	Fuzzy	PT_ALIGNED --> PAGE_ALIGNED exchange
 *					l1_utemplate	delete
 *	'90.7.20	Fuzzy	kernel segment entries in segment table
 *				entries for user space address delete.
 *				copying kernel segment entries
 *				to user pmap segment entries delete.
 *				all user segment table entries initialize
 *				to zero (invalid).
 *
 * Function:
 *	Create and return a physical map. If the size specified for the
 *	map is zero, the map is an actual physical map, and may be referenced
 *	by the hardware. If the size specified is non-zero, the map will be
 *	used in software only, and is bounded by that size.
 *
 * Paramerters:
 *	size		size of the map
 *
 *  This routines allocates a pmap structure.
 */
pmap_t
pmap_create(vm_size_t size)
{
    	pmap_t		p;

    /*
     * A software use-only map doesn't even need a map.
     */
    if (size != 0)
	return(PMAP_NULL);

    CHECK_PMAP_CONSISTENCY("pmap_create");

    p = (pmap_t)malloc(sizeof(*p), M_VMPMAP, M_WAITOK);
    if (p == PMAP_NULL) {
	panic("pmap_create: cannot allocate a pmap");
    }

    bzero(p, sizeof(*p));
    pmap_pinit(p);
    return(p);

} /* pmap_create() */

void
pmap_pinit(pmap_t p)
{
    pmap_statistics_t	stats;
    sdt_entry_t		*segdt;
    int                	i;

    /*
     * Allocate memory for *actual* segment table and *shadow* table.
     */
    segdt = (sdt_entry_t *)kmem_alloc(kernel_map, 2 * SDT_SIZE);
    if (segdt == NULL)
	      panic("pmap_create: kmem_alloc failure");

#if 0
    /* maybe, we can use bzero to zero out the segdt. XXX nivas */
    bzero(segdt, 2 * SDT_SIZE); */
#endif /* 0 */
    /* use pmap zero page to zero it out */
    pmap_zero_page(pmap_extract(kernel_pmap,(vm_offset_t)segdt));
    if (PAGE_SIZE == SDT_SIZE)  /* only got half */
      pmap_zero_page(pmap_extract(kernel_pmap,(vm_offset_t)segdt+PAGE_SIZE));
    if (PAGE_SIZE < 2*SDT_SIZE) /* get remainder */
      bzero((vm_offset_t)segdt+PAGE_SIZE, (2*SDT_SIZE)-PAGE_SIZE);

    /*
     * Initialize pointer to segment table both virtual and physical.
     */
    p->sdt_vaddr = segdt;
    p->sdt_paddr = (sdt_entry_t *)pmap_extract(kernel_pmap,(vm_offset_t)segdt);

    if (!PAGE_ALIGNED(p->sdt_paddr)) {
	printf("pmap_create: std table = %x\n",(int)p->sdt_paddr);
	panic("pmap_create: sdt_table not aligned on page boundary");
    }

#ifdef	DEBUG
    if ((pmap_con_dbg & (CD_CREAT | CD_NORM)) == (CD_CREAT | CD_NORM)) {
	printf("(pmap_create :%x) pmap=0x%x, sdt_vaddr=0x%x, sdt_paddr=0x%x\n",
	     curproc, (unsigned)p, p->sdt_vaddr, p->sdt_paddr);
    }
#endif

#if notneeded
    /*
     * memory for page tables should be CACHE DISABLED?
     */
    pmap_cache_ctrl(kernel_pmap,
		   (vm_offset_t)segdt,
		   (vm_offset_t)segdt+SDT_SIZE,
		   CACHE_INH);
#endif
    /*
     * Initalize SDT_ENTRIES.
     */
    /*
     * There is no need to clear segment table, since kmem_alloc would
     * provides us clean pages.
     */

    /*
     * Initialize pmap structure.
     */
    p->ref_count = 1;

#ifdef OMRON_PMAP
     /* initialize block address translation cache */
     for (i = 0; i < BATC_MAX; i++) {
	 p->i_batc[i].bits = 0;
	 p->d_batc[i].bits = 0;
     }
#endif

    /*
     * Initialize statistics.
     */
    stats = &p->stats;
    stats->resident_count = 0;
    stats->wired_count = 0;

#ifdef	DEBUG
    /* link into list of pmaps, just after kernel pmap */
    p->next = kernel_pmap->next;
    p->prev = kernel_pmap;
    kernel_pmap->next = p;
    p->next->prev = p;
#endif

} /* pmap_pinit() */

/*
 * Routine:	PMAP_FREE_TABLES (internal)
 *
 *	Internal procedure used by pmap_destroy() to actualy deallocate
 *	the tables.
 *
 * Parameters:
 *	pmap		pointer to pmap structure
 *
 * Calls:
 *	pmap_pte
 *	kmem_free
 *	PT_FREE
 *
 * Special Assumptions:
 *	No locking is needed, since this is only called which the
 * 	ref_count field of the pmap structure goes to zero.
 *
 * This routine sequences of through the user address space, releasing
 * all translation table space back to the system using PT_FREE.
 * The loops are indexed by the virtual address space
 * ranges represented by the table group sizes(PDT_TABLE_GROUP_VA_SPACE).
 *
 */

STATIC void
pmap_free_tables(pmap_t pmap)
{
    unsigned long	sdt_va;	/*  outer loop index */
    sdt_entry_t	*sdttbl; /*  ptr to first entry in the segment table */
    pt_entry_t  *gdttbl; /*  ptr to first entry in a page table */
    unsigned int i,j;

#if	DEBUG
    if ((pmap_con_dbg & (CD_FREE | CD_NORM)) == (CD_FREE | CD_NORM))
	printf("(pmap_free_tables :%x) pmap %x\n", curproc, pmap);
#endif

    sdttbl = pmap->sdt_vaddr;		/* addr of segment table */

    /* 
      This contortion is here instead of the natural loop 
      because of integer overflow/wraparound if VM_MAX_USER_ADDRESS is near 0xffffffff
    */

    i = VM_MIN_USER_ADDRESS / PDT_TABLE_GROUP_VA_SPACE;
    j = VM_MAX_USER_ADDRESS / PDT_TABLE_GROUP_VA_SPACE;
    if ( j < 1024 ) j++;

    /* Segment table Loop */
    for ( ; i < j; i++)
      {
	sdt_va = PDT_TABLE_GROUP_VA_SPACE*i;
	if ((gdttbl = pmap_pte(pmap, (vm_offset_t)sdt_va)) != PT_ENTRY_NULL) {
#ifdef	DEBUG
	    if ((pmap_con_dbg & (CD_FREE | CD_FULL)) == (CD_FREE | CD_FULL))
		printf("(pmap_free_tables :%x) free page table = 0x%x\n", curproc, gdttbl);
#endif
		PT_FREE(gdttbl);
	}

    } /* Segment Loop */

#ifdef	DEBUG
    if ((pmap_con_dbg & (CD_FREE | CD_FULL)) == (CD_FREE | CD_FULL))
      printf("(pmap_free_tables :%x) free segment table = 0x%x\n", curproc, sdttbl);
#endif
    /*
     * Freeing both *actual* and *shadow* segment tables
     */
    kmem_free(kernel_map, (vm_offset_t)sdttbl, 2*SDT_SIZE);

} /* pmap_free_tables() */


void
pmap_release(register pmap_t p)
{
	pmap_free_tables(p);
#ifdef	DBG
	DEBUG ((pmap_con_dbg & (CD_DESTR | CD_NORM)) == (CD_DESTR | CD_NORM))
	  printf("(pmap_destroy :%x) ref_count = 0\n", curproc);
	/* unlink from list of pmap structs */
	p->prev->next = p->next;
	p->next->prev = p->prev;
#endif

}

/*
 * Routine:	PMAP_DESTROY
 *
 * Function:
 *	Retire the given physical map from service. Should only be called
 *	if the map contains no valid mappings.
 *
 * Parameters:
 *	pmap		pointer to pmap structure
 *
 * Calls:
 *	CHECK_PMAP_CONSISTENCY
 *	PMAP_LOCK, PMAP_UNLOCK
 *	pmap_free_tables
 *	zfree
 *
 * Special Assumptions:
 *	Map contains no valid mappings.
 *
 *  This routine decrements the reference count in the pmap
 * structure. If it goes to zero, pmap_free_tables is called to release
 * the memory space to the system. Then, call kmem_free to free the
 * pmap structure.
 */
void
pmap_destroy(pmap_t p)
{
    register int	c, s;

    if (p == PMAP_NULL) {
#ifdef	DEBUG
	if ((pmap_con_dbg & (CD_DESTR | CD_NORM)) == (CD_DESTR | CD_NORM))
	  printf("(pmap_destroy :%x) pmap is NULL\n", curproc);
#endif
	return;
     }

    if (p == kernel_pmap) {
	panic("pmap_destroy: Attempt to destroy kernel pmap");
    }

    CHECK_PMAP_CONSISTENCY("pmap_destroy");

    PMAP_LOCK(p, s);
    c = --p->ref_count;
    PMAP_UNLOCK(p, s);

    if (c == 0) {
	pmap_release(p);
	free((caddr_t)p,M_VMPMAP);
    }

} /* pmap_destroy() */


/*
 * Routine:	PMAP_REFERENCE
 *
 * Function:
 *	Add a reference to the specified  pmap.
 *
 * Parameters:
 *	pmap		pointer to pmap structure
 *
 * Calls:
 *	PMAP_LOCK, PMAP_UNLOCK
 *
 *   Under a pmap read lock, the ref_count field of the pmap structure
 * is incremented. The function then returns.
 */
void
pmap_reference(pmap_t p)
{
    int		s;

    if (p != PMAP_NULL) {
	PMAP_LOCK(p, s);
	p->ref_count++;
	PMAP_UNLOCK(p, s);
    }

} /* pmap_reference */


/*
 * Routine:	PMAP_REMOVE_RANGE (internal)
 *
 * Function:
 *	Invalidate page table entries associated with the
 *	given virtual address range. The entries given are the first
 *	(inclusive) and last (exclusive) entries for the VM pages.
 *
 * Parameters:
 *	pmap		pointer to pmap structure
 *	s		virtual address of start of range to remove
 *	e		virtual address of start of range to remove
 *
 * External/Global:
 *	pv lists
 *	pmap_modify_list
 *
 * Calls:
 *	CHECK_PAGE_ALIGN
 *	SDTENT
 *	SDT_VALID
 *	SDT_NEXT
 *	pmap_pte
 *	PDT_VALID
 *	M88K_PTOB
 *	PMAP_MANAGED
 *	PFIDX
 *	LOCK_PVH
 *	UNLOCK_PVH
 *	PFIDX_TO_PVH
 *	CHECK_PV_LIST
 *	zfree
 *	invalidate_pte
 *	flush_atc_entry
 *	vm_page_set_modified
 *	PHYS_TO_VM_PAGE
 *
 * Special Assumptions:
 *	The pmap must be locked.
 *
 *   This routine sequences through the pages defined by the given
 * range. For each page, pmap_pte is called to obtain a (virtual)
 * pointer to the page table  entry (PTE) associated with the page's
 * virtual address. If the page table entry does not exist, or is invalid,
 * nothing need be done.
 *
 *  If the PTE is valid, the routine must invalidated the entry. The
 * 'modified' bit, if on, is referenced to the VM through the
 * 'vm_page_set_modified' macro, and into the appropriate entry in the
 * pmap_modify_list. Next, the function must find the PV list entry
 * associated with this pmap/va (if it doesn't exist - the function
 * panics). The PV list entry is unlinked from the list, and returned to
 * its zone.
 */

STATIC void
pmap_remove_range(pmap_t pmap, vm_offset_t s, vm_offset_t e)
{
    int			pfi;
    int			pfn;
    int			num_removed = 0,
    			num_unwired = 0;
    register int	i;
    pt_entry_t		*pte;
    pv_entry_t		prev, cur;
    pv_entry_t		pvl;
    vm_offset_t		pa, va, tva;
    register unsigned		users;
    register pte_template_t	opte;
    int				kflush;

    if (e < s)
      panic("pmap_remove_range: end < start");

    /*
     * Pmap has been locked by pmap_remove.
     */
    if (pmap == kernel_pmap) {
	kflush = 1;
    } else {
	kflush = 0;
    }

    /*
     * Loop through the range in vm_page_size increments.
     * Do not assume that either start or end fail on any
     * kind of page boundary (though this may be true!?).
     */

    CHECK_PAGE_ALIGN(s, "pmap_remove_range - start addr");

    for (va = s; va < e; va += PAGE_SIZE) {

	sdt_entry_t *sdt;

	sdt = SDTENT(pmap,va);

	if (!SDT_VALID(sdt)) {
	    va &= SDT_MASK; /* align to segment */
	    if (va <= e - (1<<SDT_SHIFT))
	      va += (1<<SDT_SHIFT) - PAGE_SIZE; /* no page table, skip to next seg entry */
	    else /* wrap around */
	      break;
	    continue;
	}

	pte = pmap_pte(pmap,va);

	if (!PDT_VALID(pte)) {
	    continue;			/* no page mapping */
	}

	num_removed++;

	if (pte->wired)
	  num_unwired++;

	pfn = pte->pfn;
	pa = M88K_PTOB(pfn);

	if (PMAP_MANAGED(pa)) {
	    pfi = PFIDX(pa);
	    /*
	     * Remove the mapping from the pvlist for
	     * this physical page.
	     */
	    pvl = PFIDX_TO_PVH(pfi);
	    CHECK_PV_LIST(pa, pvl, "pmap_remove_range before");

	    if (pvl->pmap == PMAP_NULL)
	      panic("pmap_remove: null pv_list");

	    if (pvl->va == va && pvl->pmap == pmap) {

		/*
		 * Hander is the pv_entry. Copy the next one
		 * to hander and free the next one (we can't
		 * free the hander)
		 */
		cur = pvl->next;
		if (cur != PV_ENTRY_NULL) {
		    *pvl = *cur;
		    free((caddr_t)cur, M_VMPVENT);
		} else {
		    pvl->pmap =  PMAP_NULL;
		}

	    } else {

		for (prev = pvl; (cur = prev->next) != PV_ENTRY_NULL; prev = cur) {
		    if (cur->va == va && cur->pmap == pmap) {
			break;
		    }
		}
		if (cur == PV_ENTRY_NULL) {
		    printf("pmap_remove_range: looking for VA "
			   "0x%x (pa 0x%x) PV list at 0x%x\n", va, pa, (unsigned)pvl);
		    panic("pmap_remove_range: mapping not in pv_list");
		}

		prev->next = cur->next;
	        free((caddr_t)cur, M_VMPVENT);
	    }

	    CHECK_PV_LIST(pa, pvl, "pmap_remove_range after");

	} /* if PAGE_MANAGED */

	/*
	 * For each pte in vm_page (NOTE: vm_page, not
	 * M88K (machine dependent) page !! ), reflect
	 * modify bits to pager and zero (invalidate,
	 * remove) the pte entry.
	 */
	tva = va;
	for (i = ptes_per_vm_page; i > 0; i--) {

	    /*
	     * Invalidate pte temporarily to avoid being written back
	     * the modified bit and/or the reference bit by other cpu.
	     */
	    opte.bits = invalidate_pte(pte);
	    flush_atc_entry(0, tva, kflush);

	    if (opte.pte.modified) {
		if (IS_VM_PHYSADDR(pa)) {
			vm_page_set_modified(PHYS_TO_VM_PAGE(opte.bits & M88K_PGMASK));
		}
		/* keep track ourselves too */
		if (PMAP_MANAGED(pa))
		  pmap_modify_list[pfi] = 1;
	    }
	    pte++;
	    tva += M88K_PGBYTES;
	}
	
    } /* end for ( va = s; ...) */

    /*
     * Update the counts
     */
    pmap->stats.resident_count -= num_removed;
    pmap->stats.wired_count -= num_unwired;

} /* pmap_remove_range */

/*
 * Routine:	PMAP_REMOVE
 *
 * Function:
 *	Remove the given range of addresses from the specified map.
 *	It is assumed that start is properly rounded to the VM page size.
 *
 * Parameters:
 *	pmap		pointer to pmap structure
 *
 * Special Assumptions:
 *	Assumes not all entries must be valid in specified range.
 *
 * Calls:
 *	CHECK_PAGE_ALIGN
 *	PMAP_LOCK, PMAP_UNLOCK
 *	pmap_remove_range
 *	panic
 *
 *  After taking pmap read lock, pmap_remove_range is called to do the
 * real work.
 */
void
pmap_remove(pmap_t map, vm_offset_t s, vm_offset_t e)
{
    int 		spl;

    if (map == PMAP_NULL) {
	 return;
     }

#if	DEBUG
    if ((pmap_con_dbg & (CD_RM | CD_NORM)) == (CD_RM | CD_NORM))
	printf("(pmap_remove :%x) map %x  s %x  e %x\n", curproc, map, s, e);
#endif

    CHECK_PAGE_ALIGN(s, "pmap_remove start addr");

    if (s>e)
	panic("pmap_remove: start greater than end address");

    pmap_remove_range(map, s, e);
} /* pmap_remove() */


/*
 * Routine:	PMAP_REMOVE_ALL
 *
 * Function:
 *	Removes this physical page from all physical maps in which it
 *	resides. Reflects back modify bits to the pager.
 *
 * Parameters:
 *	phys		physical address of pages which is to
 *			be removed from all maps
 *
 * Extern/Global:
 *	pv_head_array, pv lists
 *	pmap_modify_list
 *
 * Calls:
 *	PMAP_MANAGED
 *	SPLVM, SPLX
 *	PFIDX
 *	PFIDX_TO_PVH
 *	CHECK_PV_LIST
 *	simple_lock
 *	M88K_PTOB
 *	PDT_VALID
 *	pmap_pte
 *	vm_page_set_modified
 *	PHYS_TO_VM_PAGE
 *	zfree
 *
 *  If the page specified by the given address is not a managed page,
 * this routine simply returns. Otherwise, the PV list associated with
 * that page is traversed. For each pmap/va pair pmap_pte is called to
 * obtain a pointer to the page table entry (PTE) associated with the
 * va (the PTE must exist and be valid, otherwise the routine panics).
 * The hardware 'modified' bit in the PTE is examined. If it is on, the
 * pmap_modify_list entry corresponding to the physical page is set to 1.
 * Then, the PTE is invalidated, and the PV list entry is unlinked and
 * freed.
 *
 *  At the end of this function, the PV list for the specified page
 * will be null.
 */
void
pmap_remove_all(vm_offset_t phys)
{
    pv_entry_t			pvl, cur;
    register pt_entry_t		*pte;
    int				pfi;
    register int		i;
    register vm_offset_t	va;
    register pmap_t		pmap;
    int				spl;
    int				dbgcnt = 0;
    register unsigned		users;
    register pte_template_t	opte;
    int				kflush;

    if (!PMAP_MANAGED(phys)) {
	/* not a managed page. */
#ifdef	DEBUG
	if (pmap_con_dbg & CD_RMAL)
	  printf("(pmap_remove_all :%x) phys addr 0x%x not a managed page\n", curproc, phys);
#endif
	return;
    }

    SPLVM(spl);

    /*
     * Walk down PV list, removing all mappings.
     * We have to do the same work as in pmap_remove_pte_page
     * since that routine locks the pv_head. We don't have
     * to lock the pv_head, since we have the entire pmap system.
     */
remove_all_Retry:

    pfi = PFIDX(phys);
    pvl = PFIDX_TO_PVH(pfi);
    CHECK_PV_LIST(phys, pvl, "pmap_remove_all before");

    /*
     * Loop for each entry on the pv list
     */
    while ((pmap = pvl->pmap) != PMAP_NULL) {
	va = pvl->va;
	users = 0;
	if (pmap == kernel_pmap) {
	    kflush = 1;
	} else {
	    kflush = 0;
	}

	pte = pmap_pte(pmap, va);

	/*
	 * Do a few consistency checks to make sure
	 * the PV list and the pmap are in synch.
	 */
	if (pte == PT_ENTRY_NULL) {
	    printf("(pmap_remove_all :%x) phys %x pmap %x va %x dbgcnt %x\n",
		 (unsigned)curproc, phys, (unsigned)pmap, va, dbgcnt);
	    panic("pmap_remove_all: pte NULL");
	}
	if (!PDT_VALID(pte))
	    panic("pmap_remove_all: pte invalid");
	if (M88K_PTOB(pte->pfn) != phys)
	    panic("pmap_remove_all: pte doesn't point to page");
	if (pte->wired)
	    panic("pmap_remove_all: removing  a wired page");

	pmap->stats.resident_count--;

	if ((cur = pvl->next) != PV_ENTRY_NULL) {
	    *pvl  = *cur;
	    free((caddr_t)cur, M_VMPVENT);
	}
	else
	    pvl->pmap = PMAP_NULL;

	/*
	 * Reflect modified pages to pager.
	 */
	for (i = ptes_per_vm_page; i>0; i--) {

	    /*
	     * Invalidate pte temporarily to avoid being written back
	     * the modified bit and/or the reference bit by other cpu.
	     */
	    opte.bits = invalidate_pte(pte);
	    flush_atc_entry(users, va, kflush);

	    if (opte.pte.modified) {
		vm_page_set_modified((vm_page_t)PHYS_TO_VM_PAGE(phys));
		/* keep track ourselves too */
		pmap_modify_list[pfi] = 1;
	    }
	    pte++;
	    va += M88K_PGBYTES;
	}

	/*
	 * Do not free any page tables,
	 * leaves that for when VM calls pmap_collect().
	 */
	dbgcnt++;
    }
    CHECK_PV_LIST(phys, pvl, "pmap_remove_all after");

    SPLX(spl);

} /* pmap_remove_all() */

/*
 * Routine:	PMAP_COPY_ON_WRITE
 *
 * Function:
 *	Remove write privileges from all physical maps for this physical page.
 *
 * Parameters:
 *	phys		physical address of page to be read-protected.
 *
 * Calls:
 *		SPLVM, SPLX
 *		PFIDX_TO_PVH
 *		CHECK_PV_LIST
 *		simple_lock, simple_unlock
 *		panic
 *		PDT_VALID
 *		M88K_PTOB
 *		pmap_pte
 *
 * Special Assumptions:
 *	All mapings of the page are user-space mappings.
 *
 *  This routine walks the PV list. For each pmap/va pair it locates
 * the page table entry (the PTE), and sets the hardware enforced
 * read-only bit. The TLB is appropriately flushed.
 */
STATIC void
pmap_copy_on_write(vm_offset_t phys)
{
    register pv_entry_t		pv_e;
    register pt_entry_t 	*pte;
    register int		i;
    int				spl, spl_sav;
    register unsigned		users;
    register pte_template_t	opte;
    int				kflush;

    if (!PMAP_MANAGED(phys)) {
#ifdef	DEBUG
	if (pmap_con_dbg & CD_CMOD)
	  printf("(pmap_copy_on_write :%x) phys addr 0x%x not managed \n", curproc, phys);
#endif
	return;
    }

    SPLVM(spl);

    pv_e = PFIDX_TO_PVH(PFIDX(phys));
    CHECK_PV_LIST(phys, pv_e, "pmap_copy_on_write before");
    if (pv_e->pmap  == PMAP_NULL) {

#ifdef	DEBUG
	if ((pmap_con_dbg & (CD_COW | CD_NORM)) == (CD_COW | CD_NORM))
	  printf("(pmap_copy_on_write :%x) phys addr 0x%x not mapped\n", curproc, phys);
#endif

	SPLX(spl);

	return;		/* no mappings */
    }

    /*
     * Run down the list of mappings to this physical page,
     * disabling write privileges on each one.
     */

    while (pv_e != PV_ENTRY_NULL) {
	pmap_t		pmap;
	vm_offset_t	va;

	pmap = pv_e->pmap;
	va = pv_e->va;

	users = 0;
	if (pmap == kernel_pmap) {
	    kflush = 1;
	} else {
	    kflush = 0;
	}

	/*
	 * Check for existing and valid pte
	 */
	pte = pmap_pte(pmap, va);
	if (pte == PT_ENTRY_NULL)
	    panic("pmap_copy_on_write: pte from pv_list not in map");
	if (!PDT_VALID(pte))
	    panic("pmap_copy_on_write: invalid pte");
	if (M88K_PTOB(pte->pfn) != phys)
	    panic("pmap_copy_on_write: pte doesn't point to page");

	/*
	 * Flush TLBs of which cpus using pmap.
	 */

	for (i = ptes_per_vm_page; i > 0; i--) {

	    /*
	     * Invalidate pte temporarily to avoid being written back
	     * the modified bit and/or the reference bit by other cpu.
	     */
	    spl_sav = splimp();
	    opte.bits = invalidate_pte(pte);
	    opte.pte.prot = M88K_RO;
	    ((pte_template_t *)pte)->bits = opte.bits;
	    flush_atc_entry(users, va, kflush);
	    splx(spl_sav);
	    pte++;
	    va += M88K_PGBYTES;
	}

	pv_e = pv_e->next;
    }
    CHECK_PV_LIST(phys, PFIDX_TO_PVH(PFIDX(phys)), "pmap_copy_on_write");

    SPLX(spl);

} /* pmap_copy_on_write */

/*
 * Routine:	PMAP_PROTECT
 *
 * Function:
 *	Sets the physical protection on the specified range of this map
 *	as requested.
 *
 * Parameters:
 *	pmap		pointer to pmap structure
 *	s		start address of start of range
 *	e		end address of end of range
 *	prot		desired protection attributes
 *
 *	Calls:
 *		m88k_protection
 *		PMAP_LOCK, PMAP_UNLOCK
 *		CHECK_PAGE_ALIGN
 *		panic
 *		pmap_pte
 *		SDT_NEXT
 *		PDT_VALID
 *
 *  This routine sequences through the pages of the specified range.
 * For each, it calls pmap_pte to acquire a pointer to the page table
 * entry (PTE). If the PTE is invalid, or non-existant, nothing is done.
 * Otherwise, the PTE's protection attributes are adjusted as specified.
 */
void
pmap_protect(pmap_t pmap, vm_offset_t s, vm_offset_t e, vm_prot_t prot)
{
    pte_template_t		maprot;
    unsigned			ap;
    int				spl, spl_sav;
    register int		i;
    pt_entry_t			*pte;
    vm_offset_t			va, tva;
    register unsigned		users;
    register pte_template_t	opte;
    int				kflush;

    if (pmap == PMAP_NULL || prot & VM_PROT_WRITE)
	return;
    if ((prot & VM_PROT_READ) == 0) {
    	pmap_remove(pmap, s, e);
    	return;
    }
    if (s > e)
	panic("pmap_protect: start grater than end address");

    maprot.bits = m88k_protection(pmap, prot);
    ap = maprot.pte.prot;

    PMAP_LOCK(pmap, spl);

    if (pmap == kernel_pmap) {
	kflush = 1;
    } else {
	kflush = 0;
    }

    CHECK_PAGE_ALIGN(s, "pmap_protect");

    /*
     * Loop through the range in vm_page_size increment.
     * Do not assume that either start or end fall on any
     * kind of page boundary (though this may be true ?!).
     */
    for (va = s; va <= e; va += PAGE_SIZE) {

	pte = pmap_pte(pmap, va);

	if (pte == PT_ENTRY_NULL) {

	    va &= SDT_MASK; /* align to segment */
	    if (va <= e - (1<<SDT_SHIFT))
	      va += (1<<SDT_SHIFT) - PAGE_SIZE; /* no page table, skip to next seg entry */
	    else /* wrap around */
	      break;
	    
#ifdef	DEBUG
	    if ((pmap_con_dbg & (CD_PROT | CD_FULL)) == (CD_PROT | CD_FULL))
	      printf("(pmap_protect :%x) no page table :: skip to 0x%x\n", curproc, va + PAGE_SIZE);
#endif
	    continue;
	}

	if (!PDT_VALID(pte)) {
#ifdef	DEBUG
	    if ((pmap_con_dbg & (CD_PROT | CD_FULL)) == (CD_PROT | CD_FULL))
	      printf("(pmap_protect :%x) pte invalid pte @ 0x%x\n", curproc, pte);
#endif
	    continue;			/*  no page mapping */
	}
#if 0
	printf("(pmap_protect :%x) pte good\n", curproc);
#endif

	tva = va;
	for (i = ptes_per_vm_page; i>0; i--) {

	    /*
	     * Invalidate pte temporarily to avoid being written back
	     * the modified bit and/or the reference bit by other cpu.
	     */
	    spl_sav = splimp();
	    opte.bits = invalidate_pte(pte);
	    opte.pte.prot = ap;
	    ((pte_template_t *)pte)->bits = opte.bits;
	    flush_atc_entry(0, tva, kflush);
	    splx(spl_sav);
	    pte++;
	    tva += M88K_PGBYTES;
	}
    }

    PMAP_UNLOCK(pmap, spl);

} /* pmap_protect() */



/*
 * Routine:	PMAP_EXPAND
 *
 * Function:
 *	Expands a pmap to be able to map the specified virtual address.
 *	New kernel virtual memory is allocated for a page table
 *
 *	Must be called with the pmap system and the pmap unlocked, since
 *	these must be unlocked to use vm_allocate or vm_deallocate (via
 *	kmem_alloc, zalloc). Thus it must be called in a unlock/lock loop
 *	that checks whether the map has been expanded enough. ( We won't loop
 *	forever, since page table aren't shrunk.)
 *
 * Parameters:
 *	map	point to map structure
 *	v	VA indicating which tables are needed
 *
 * Extern/Global:
 *	user_pt_map
 *	kernel_pmap
 *
 * Calls:
 *	pmap_pte
 *	kmem_alloc
 *	kmem_free
 *	zalloc
 *	zfree
 *	pmap_extract
 *
 * Special Assumptions
 *	no pmap locks held
 *
 * 1:	This routine immediately allocates space for a page table.
 *
 * 2:   The page table entries (PTEs) are initialized (set invalid), and
 *	the corresponding segment table entry is set to point to the new
 *	page table.
 *
 *
 *	if (kernel_pmap)
 *		pmap_expand_kmap()
 *	ptva = kmem_alloc(user_pt_map)
 *
 */
STATIC void
pmap_expand(pmap_t map, vm_offset_t v)
{
	int		i,
			spl;
	vm_offset_t	pdt_vaddr,
			pdt_paddr;

	sdt_entry_t *sdt;
	pt_entry_t		*pte;
	vm_offset_t pmap_extract();

	if (map == PMAP_NULL) {
	    panic("pmap_expand: pmap is NULL");
	}

#ifdef	DEBUG
	if ((pmap_con_dbg & (CD_EXP | CD_NORM)) == (CD_EXP | CD_NORM))
		printf ("(pmap_expand :%x) map %x v %x\n", curproc, map, v);
#endif

	CHECK_PAGE_ALIGN (v, "pmap_expand");

	/*
	 * Handle kernel pmap in pmap_expand_kmap().
	 */
	if (map == kernel_pmap) {
		PMAP_LOCK(map, spl);
		if (pmap_expand_kmap(v, VM_PROT_READ|VM_PROT_WRITE) == PT_ENTRY_NULL)
			panic ("pmap_expand: Cannot allocate kernel pte table");
		PMAP_UNLOCK(map, spl);
#ifdef	DEBUG
		if ((pmap_con_dbg & (CD_EXP | CD_FULL)) == (CD_EXP | CD_FULL))
			printf("(pmap_expand :%x) kernel_pmap\n", curproc);
#endif
		return;
	}

	/* XXX */
#ifdef MACH_KERNEL
        if (kmem_alloc_wired(kernel_map, &pdt_vaddr, PAGE_SIZE) != KERN_SUCCESS)
          panic("pmap_enter: kmem_alloc failure");
	pmap_zero_page(pmap_extract(kernel_pmap, pdt_vaddr));
#else
	pdt_vaddr = kmem_alloc (kernel_map, PAGE_SIZE);
#endif

	pdt_paddr = pmap_extract(kernel_pmap, pdt_vaddr);

#if notneeded
	/*
	 * the page for page tables should be CACHE DISABLED
	 */
	pmap_cache_ctrl(kernel_pmap, pdt_vaddr, pdt_vaddr+PAGE_SIZE, CACHE_INH);
#endif

	PMAP_LOCK(map, spl);

	if ((pte = pmap_pte(map, v)) != PT_ENTRY_NULL) {
		/*
		 * Someone else caused us to expand
		 * during our vm_allocate.
		 */
		PMAP_UNLOCK(map, spl);
		/* XXX */
		kmem_free (kernel_map, pdt_vaddr, PAGE_SIZE);
#ifdef	DEBUG
		if (pmap_con_dbg & CD_EXP)
			printf("(pmap_expand :%x) table has already allocated\n", curproc);
#endif
		return;
	}

	/*
	 * Apply a mask to V to obtain the vaddr of the beginning of
	 * its containing page 'table group',i.e. the group of
	 * page  tables that fit eithin a single VM page.
	 * Using that, obtain the segment table pointer that references the
	 * first page table in the group, and initilize all the
	 * segment table descriptions for the page 'table group'.
	 */
	v &= ~((1<<(LOG2_PDT_TABLE_GROUP_SIZE+PDT_BITS+PG_BITS))-1);

	sdt = SDTENT(map,v);

	/*
	 * Init each of the segment entries to point the freshly allocated
	 * page tables.
	 */

	for (i = PDT_TABLE_GROUP_SIZE; i>0; i--) {
		((sdt_entry_template_t *)sdt)->bits = pdt_paddr | M88K_RW | DT_VALID;
		((sdt_entry_template_t *)(sdt + SDT_ENTRIES))->bits = pdt_vaddr | M88K_RW | DT_VALID;
		sdt++;
		pdt_paddr += PDT_SIZE;
		pdt_vaddr += PDT_SIZE;
	}

	PMAP_UNLOCK(map, spl);

} /* pmap_expand() */



/*
 * Routine:	PMAP_ENTER
 *
 *
 * Update:
 *	July 13,90 - JUemura
 *		initial porting
 *         *****TO CHECK*****
 *              locks removed since we don't have to allocate
 *		level 2 tables anymore. locks needed?
 *	'90.7.26	Fuzzy	VM_MIN_KERNEL_ADDRESS -> VM_MAX_USER_ADDRESS
 *	'90.8.17	Fuzzy	Debug message added(PV no mapped at VA)
 *	'90.8.31	Sugai	Remove redundant message output
 *
 * Function:
 *	Insert the given physical page (p) at the specified virtual
 *	address (v) in the target phisical map with the protecton requested.
 *	If specified, the page will be wired down, meaning that the
 *	related pte can not be reclaimed.
 *
 * N.B.: This is only routine which MAY NOT lazy-evaluation or lose
 *	information. That is, this routine must actually insert this page
 *	into the given map NOW.
 *
 * Parameters:
 *	pmap	pointer to pmap structure
 *	va	VA of page to be mapped
 *	pa	PA of page to be mapped
 *	prot	protection attributes for page
 *	wired	wired attribute for page
 *
 * Extern/Global:
 *	pv_head_array, pv lists
 *	pmap_modify_list
 *
 * Calls:
 *	m88k_protection
 *	pmap_pte
 *	pmap_expand
 *	pmap_remove_range
 *	zfree
 *
 *	This routine starts off by calling pmap_pte to obtain a (virtual)
 * pointer to the page table entry corresponding to given virtual
 * address. If the page table itself does not exist, pmap_expand is
 * called to allocate it.
 *
 *      If the page table entry (PTE) already maps the given physical page,
 * all that is needed is to set the protection and wired attributes as
 * given. TLB entries are flushed and pmap_enter returns.
 *
 *	If the page table entry (PTE) maps a different physical page than
 * that given, the old mapping is removed by a call to map_remove_range.
 * And execution of pmap_enter continues.
 *
 *	To map the new physical page, the routine first inserts a new
 * entry in the PV list exhibiting the given pmap and virtual address.
 * It then inserts the physical page address, protection attributes, and
 * wired attributes into the page table entry (PTE).
 *
 *
 *	get machine-dependent prot code
 *	get the pte for this page
 *	if necessary pmap expand(pmap,v)
 *	if (changing wired attribute or protection) {
 * 		flush entry from TLB
 *		update template
 *		for (ptes per vm page)
 *			stuff pte
 *	} else if (mapped at wrong addr)
 *		flush entry from TLB
 *		pmap_remove_range
 *	} else {
 *		enter mapping in pv_list
 *		setup template and stuff ptes
 *	}
 *
 */
void
pmap_enter(pmap_t pmap, vm_offset_t va, vm_offset_t pa,
					vm_prot_t prot, boolean_t wired)
{
    int				ap;
    int				spl, spl_sav;
    pv_entry_t			pv_e;
    pt_entry_t			*pte;
    vm_offset_t			old_pa;
    pte_template_t		template;
    register int		i;
    int				pfi;
    pv_entry_t			pvl;
    register unsigned		users;
    register pte_template_t	opte;
    int				kflush;

    if (pmap == PMAP_NULL) {
	panic("pmap_enter: pmap is NULL");
    }

    CHECK_PAGE_ALIGN (va, "pmap_entry - VA");
    CHECK_PAGE_ALIGN (pa, "pmap_entry - PA");

    /*
     *	Range check no longer use, since we use whole address space
     */

#ifdef	DEBUG
    if ((pmap_con_dbg & (CD_ENT | CD_NORM)) == (CD_ENT | CD_NORM)) {
	if (pmap == kernel_pmap)
	  printf ("(pmap_enter :%x) pmap kernel va %x pa %x\n", curproc, va, pa);
	else
	  printf ("(pmap_enter :%x) pmap %x  va %x pa %x\n", curproc, pmap, va, pa);
    }
#endif

    ap = m88k_protection (pmap, prot);

    /*
     *	Must allocate a new pvlist entry while we're unlocked;
     *	zalloc may cause pageout (which will lock the pmap system).
     *	If we determine we need a pvlist entry, we will unlock
     *	and allocate one. Then will retry, throwing away
     *	the allocated entry later (if we no longer need it).
     */
    pv_e = PV_ENTRY_NULL;
  Retry:

    PMAP_LOCK(pmap, spl);

    /*
     * Expand pmap to include this pte. Assume that
     * pmap is always expanded to include enough M88K
     * pages to map one VM page.
     */
    while ((pte = pmap_pte(pmap, va)) == PT_ENTRY_NULL) {
	/*
	 * Must unlock to expand the pmap.
	 */
	PMAP_UNLOCK(pmap, spl);
	pmap_expand(pmap, va);
	PMAP_LOCK(pmap, spl);
    }

    /*
     *	Special case if the physical page is already mapped
     *	at this address.
     */
    old_pa = M88K_PTOB(pte->pfn);
    if (old_pa == pa) {

	users = 0;
	if (pmap == kernel_pmap) {
	    kflush = 1;
	} else {
	    kflush = 0;
	}
	
	/*
	 * May be changing its wired attributes or protection
	 */

	if (wired && !pte->wired)
	  pmap->stats.wired_count++;
	else if (!wired && pte->wired)
	  pmap->stats.wired_count--;

	if ((unsigned long)pa >= MAXPHYSMEM)
	    template.bits = DT_VALID | ap | M88K_TRUNC_PAGE(pa) | CACHE_INH;
	else
	    template.bits = DT_VALID | ap | M88K_TRUNC_PAGE(pa) | CACHE_GLOBAL;
	if (wired)
	  template.pte.wired = 1;

	/*
	 * If there is a same mapping, we have nothing to do.
	 */
	if ( !PDT_VALID(pte) || (pte->wired != template.pte.wired)
	    || (pte->prot != template.pte.prot)) {

	    for (i = ptes_per_vm_page; i>0; i--) {
		
		/*
		 * Invalidate pte temporarily to avoid being written back
		 * the modified bit and/or the reference bit by other cpu.
		 */
		spl_sav = splimp();
		opte.bits = invalidate_pte(pte);
		template.pte.modified = opte.pte.modified;
		*pte++ = template.pte;
		flush_atc_entry(users, va, kflush);
		splx(spl_sav);
		template.bits += M88K_PGBYTES;
		va += M88K_PGBYTES;
	    }
	}
	
    } else { /* if ( pa == old_pa) */

	/*
	 * Remove old mapping from the PV list if necessary.
	 */
	if (old_pa != (vm_offset_t)-1) {
	    /*
	     *	Invalidate the translation buffer,
	     *	then remove the mapping.
	     */
#ifdef	DEBUG
		if ((pmap_con_dbg & (CD_ENT | CD_NORM)) == (CD_ENT | CD_NORM)) {
			if (va == phys_map_vaddr1 || va == phys_map_vaddr2) {
				printf("vaddr1 0x%x vaddr2 0x%x va 0x%x pa 0x%x managed %x\n", 
				phys_map_vaddr1, phys_map_vaddr2, va, old_pa,
					PMAP_MANAGED(pa) ? 1 : 0);
				printf("pte %x pfn %x valid %x\n",
					pte, pte->pfn, pte->dtype);
	    		}
		}
#endif
		if (va == phys_map_vaddr1 || va == phys_map_vaddr2) {
			flush_atc_entry(0, va, 1);
		} else {
			pmap_remove_range(pmap, va, va + PAGE_SIZE);
		}
	}

	if (PMAP_MANAGED(pa)) {
#ifdef	DEBUG
		if ((pmap_con_dbg & (CD_ENT | CD_NORM)) == (CD_ENT | CD_NORM)) {
	    		if (va == phys_map_vaddr1 || va == phys_map_vaddr2) {
				printf("va 0x%x and managed pa 0x%x\n", va, pa);
	    		}
    		}
#endif
	    /*
	     *	Enter the mappimg in the PV list for this
	     *	physical page.
	     */
	    pfi = PFIDX(pa);
	    pvl = PFIDX_TO_PVH(pfi);
	    CHECK_PV_LIST (pa, pvl, "pmap_enter before");

	    if (pvl->pmap == PMAP_NULL) {

		/*
		 *	No mappings yet
		 */
		pvl->va = va;
		pvl->pmap = pmap;
		pvl->next = PV_ENTRY_NULL;

	    } else {
#ifdef	DEBUG
		/*
		 * check that this mapping is not already there
		 */
		{
		    pv_entry_t e = pvl;
		    while (e != PV_ENTRY_NULL) {
			if (e->pmap == pmap && e->va == va)
			  panic ("pmap_enter: already in pv_list");
			e = e->next;
		    }
		}
#endif
		/*
		 *	Add new pv_entry after header.
		 */
		if (pv_e == PV_ENTRY_NULL) {
		    PMAP_UNLOCK(pmap, spl);
		    pv_e = (pv_entry_t) malloc(sizeof *pv_e, M_VMPVENT,
				M_NOWAIT);
		    goto Retry;
		}
		pv_e->va = va;
		pv_e->pmap = pmap;
		pv_e->next = pvl->next;
		pvl->next = pv_e;
		/*
		 *		Remeber that we used the pvlist entry.
		 */
		pv_e = PV_ENTRY_NULL;
	    }
	}

	/*
	 * And count the mapping.
	 */
	pmap->stats.resident_count++;
	if (wired)
	  pmap->stats.wired_count++;

	if ((unsigned long)pa >= MAXPHYSMEM)
	    template.bits = DT_VALID | ap | M88K_TRUNC_PAGE(pa) | CACHE_INH;
	else
	    template.bits = DT_VALID | ap | M88K_TRUNC_PAGE(pa) | CACHE_GLOBAL;

	if (wired)
	  template.pte.wired = 1;

	DO_PTES (pte, template.bits);

    } /* if ( pa == old_pa ) ... else */

    PMAP_UNLOCK(pmap, spl);

    if (pv_e != PV_ENTRY_NULL)
      free((caddr_t) pv_e, M_VMPVENT);

} /* pmap_enter */



/*
 *	Routine:	pmap_change_wiring
 *
 *	Author:		Fuzzy
 *
 *	Function:	Change the wiring attributes for a map/virtual-address
 *			Pair.
 *	Prameterts:
 *		pmap		pointer to pmap structure
 *		v		virtual address of page to be wired/unwired
 *		wired		flag indicating new wired state
 *
 *	Extern/Global:
 *		pte_per_vm_page
 *
 *	Calls:
 *		PMAP_LOCK, PMAP_UNLOCK
 *		pmap_pte
 *		panic
 *
 *	Special Assumptions:
 *		The mapping must already exist in the pmap.
 */
void
pmap_change_wiring(pmap_t map, vm_offset_t v, boolean_t wired)
{
	pt_entry_t	*pte;
	int		i;
	int		spl;

	PMAP_LOCK(map, spl);

	if ((pte = pmap_pte(map, v)) == PT_ENTRY_NULL)
		panic ("pmap_change_wiring: pte missing");

	if (wired && !pte->wired)
		/*
		 *	wiring mapping
		 */
		map->stats.wired_count++;

	else if (!wired && pte->wired)
		/*
		 *	unwired mapping
		 */
		map->stats.wired_count--;

	for (i = ptes_per_vm_page; i>0; i--)
		(pte++)->wired = wired;

	PMAP_UNLOCK(map, spl);

} /* pmap_change_wiring() */



/*
 * Routine:	PMAP_EXTRACT
 *
 * Author:	Fuzzy
 *
 * Function:
 *	Extract the physical page address associoated
 *	with the given map/virtual_address pair.
 *
 * Parameters:
 *	pmap		pointer to pmap structure
 *	va		virtual address
 *
 * Calls:
 *	PMAP_LOCK, PMAP_UNLOCK
 *	pmap_pte
 *
 *
 *	This routine checks BATC mapping first. BATC has been used and 
 * the specified pmap is kernel_pmap, batc_entry is scanned to find out
 * the mapping. 
 * 	Then the routine calls pmap_pte to get a (virtual) pointer to
 * the page table entry (PTE) associated with the given virtual
 * address. If the page table does not exist, or if the PTE is not valid,
 * then 0 address is returned. Otherwise, the physical page address from
 * the PTE is returned.
 */
vm_offset_t
pmap_extract(pmap_t pmap, vm_offset_t va)
{
    register pt_entry_t	 *pte;
    register vm_offset_t pa;
    register int	 i;
    int			 spl;
    
    if (pmap == PMAP_NULL)
	panic("pmap_extract: pmap is NULL");
    
    /*
     * check BATC first
     */
    if (pmap == kernel_pmap && batc_used > 0)
	for (i = batc_used-1; i > 0; i--)
	    if (batc_entry[i].lba == M88K_BTOBLK(va)) {
		pa = (batc_entry[i].pba << BATC_BLKSHIFT) | (va & BATC_BLKMASK );
		return(pa);
	    }

    PMAP_LOCK(pmap, spl);
    
    if ((pte = pmap_pte(pmap, va)) == PT_ENTRY_NULL)
	pa = (vm_offset_t) 0;
    else {
	if (PDT_VALID(pte))
	    pa = M88K_PTOB(pte->pfn);
	else
	    pa = (vm_offset_t) 0;
    }
    
    if (pa)
	pa |= (va & M88K_PGOFSET);	/* offset within page */

    PMAP_UNLOCK(pmap, spl);
    
#if 0
    printf("pmap_extract ret %x\n", pa);
#endif /* 0 */
    return(pa);
    
} /* pamp_extract() */

/*
  a version for the kernel debugger 
*/

vm_offset_t
pmap_extract_unlocked(pmap_t pmap, vm_offset_t va)
{
    pt_entry_t	*pte;
    vm_offset_t	pa;
    int		i;
    
    if (pmap == PMAP_NULL)
	panic("pmap_extract: pmap is NULL");
    
    /*
     * check BATC first
     */
    if (pmap == kernel_pmap && batc_used > 0)
	for (i = batc_used-1; i > 0; i--)
	    if (batc_entry[i].lba == M88K_BTOBLK(va)) {
		pa = (batc_entry[i].pba << BATC_BLKSHIFT) | (va & BATC_BLKMASK );
		return(pa);
	    }

    if ((pte = pmap_pte(pmap, va)) == PT_ENTRY_NULL)
	pa = (vm_offset_t) 0;
    else {
	if (PDT_VALID(pte))
	    pa = M88K_PTOB(pte->pfn);
	else
	    pa = (vm_offset_t) 0;
    }
    
    if (pa)
	pa |= (va & M88K_PGOFSET);	/* offset within page */

    return(pa);
    
} /* pamp_extract_unlocked() */


/*
 *	Routine:	PMAP_COPY
 *
 *	Function:
 *		Copy the range specigfied by src_adr/len from the source map
 *		to the range dst_addr/len in the destination map. This routine
 *		is only advisory and need not do anything.
 *
 *	Parameters:
 *		dst_pmap	pointer to destination  pmap structure
 *		src_pmap	pointer to source pmap structure
 *		dst_addr	VA in destionation map
 *		len		length of address space being copied
 *		src_addr	VA in source map
 *
 *	At this time, the 88200 pmap implementation does nothing in this
 * function. Translation tables in the destination map will be allocated
 * at VM fault time.
 */
void
pmap_copy(pmap_t dst_pmap, pmap_t src_pmap, vm_offset_t dst_addr,
				vm_size_t len, vm_offset_t src_addr)
{
#ifdef        lint
      dst_pmap++; src_pmap++; dst_addr++; len++; src_addr++;
#endif


}/* pmap_copy() */


/*
 * Routine:	PMAP_UPDATE
 *
 * Function:
 *	Require that all active physical maps contain no incorrect entries
 *	NOW. [This update includes forcing updates of any address map
 *	cashing]
 *	Generally used to ensure that thread about to run will see a
 *	semantically correct world.
 *
 * Parameters:
 *	none
 *
 * Call:
 *	cmmuflush
 *
 *	The 88200 pmap implementation does not defer any operations.
 * Therefore, the translation table trees are always consistent while the
 * pmap lock is not held. Therefore, there is really no work to do in
 * this function other than to flush the TLB.
 */
void
pmap_update(void)
{
#ifdef	DBG
    if ((pmap_con_dbg & (CD_UPD | CD_FULL)) == (CD_UPD | CD_FULL))
	printf("(pmap_update :%x) Called \n", curproc);
#endif

}/* pmap_update() */



/*
 * Routine:	PMAP_COLLECT
 *
 * Runction:
 *	Garbage collects the physical map system for pages which are
 *	no longer used. there may well be pages which are not
 *	referenced, but others may be collected as well.
 *	Called by the pageout daemon when pages are scarce.
 *
 * Parameters:
 *	pmap		pointer to pmap structure
 *
 * Calls:
 *	CHECK_PMAP_CONSISTENCY
 *	panic
 *	PMAP_LOCK, PMAP_UNLOCK
 *	PT_FREE
 *	pmap_pte
 *	pmap_remove_range
 *
 *	The intent of this routine is to release memory pages being used
 * by translation tables. They can be release only if they contain no
 * valid mappings, and their parent table entry has been invalidated.
 *
 *	The routine sequences through the entries user address space,
 * inspecting page-sized groups of page tables for wired entries. If
 * a full page of tables has no wired enties, any otherwise valid
 * entries are invalidated (via pmap_remove_range). Then, the segment
 * table entries corresponding to this group of page tables are
 * invalidated. Finally, PT_FREE is called to return the page to the
 * system.
 *
 *	If all entries in a segment table are invalidated, it too can
 * be returned to the system.
 *
 *	[Note: depending upon compilation options, tables may be in zones
 * or allocated through kmem_alloc. In the former case, the
 * module deals with a single table at a time.]
 */
void
pmap_collect(pmap_t pmap)
{

    vm_offset_t		sdt_va;	/* outer loop index */
    vm_offset_t		sdt_vt; /* end of segment */
    sdt_entry_t	*sdttbl;	/* ptr to first entry in the segment table */
    sdt_entry_t	*sdtp;		/* ptr to index into segment table */
    sdt_entry_t	*sdt;		/* ptr to index into segment table */
    pt_entry_t	*gdttbl;	/* ptr to first entry in a page table */
    pt_entry_t	*gdttblend;	/* ptr to byte after last entry in table group */
    pt_entry_t	*gdtp;		/* ptr to index into a page table */
    boolean_t	found_gdt_wired; /* flag indicating a wired page exists in */
    				 /* a page table's address range 	   */
    int		spl;
    unsigned int i,j;



    if (pmap == PMAP_NULL) {
	panic("pmap_collect: pmap is NULL");
    }
    if (pmap == kernel_pmap) {
#ifdef MACH_KERNEL
	return;
#else
	panic("pmap_collect attempted on kernel pmap");
#endif
    }
    
    CHECK_PMAP_CONSISTENCY ("pmap_collect");

#if	DBG
    if ((pmap_con_dbg & (CD_COL | CD_NORM)) == (CD_COL | CD_NORM))
      printf ("(pmap_collect :%x) pmap %x\n", curproc, pmap);
#endif

    PMAP_LOCK(pmap, spl);

    sdttbl = pmap->sdt_vaddr;	/* addr of segment table */
    sdtp = sdttbl;

    /* 
      This contortion is here instead of the natural loop 
      because of integer overflow/wraparound if VM_MAX_USER_ADDRESS is near 0xffffffff
    */

    i = VM_MIN_USER_ADDRESS / PDT_TABLE_GROUP_VA_SPACE;
    j = VM_MAX_USER_ADDRESS / PDT_TABLE_GROUP_VA_SPACE;
    if ( j < 1024 ) j++;

    /* Segment table loop */
    for ( ; i < j; i++, sdtp += PDT_TABLE_GROUP_SIZE)
      {
	sdt_va = VM_MIN_USER_ADDRESS + PDT_TABLE_GROUP_VA_SPACE*i;

	gdttbl = pmap_pte(pmap, (vm_offset_t)sdt_va);

	if (gdttbl == PT_ENTRY_NULL)
	  continue;	/* no maps in this range */

	gdttblend = gdttbl + (PDT_ENTRIES * PDT_TABLE_GROUP_SIZE);

	/* scan page maps for wired pages */
	found_gdt_wired = FALSE;
	for (gdtp=gdttbl; gdtp <gdttblend; gdtp++) {
	    if (gdtp->wired) {
		found_gdt_wired = TRUE;
		break;
	    }
	}

	if (found_gdt_wired)
	  continue;	/* can't free this range */

	/* figure out end of range. Watch for wraparound */

	sdt_vt = sdt_va <= VM_MAX_USER_ADDRESS-PDT_TABLE_GROUP_VA_SPACE ?
	         sdt_va+PDT_TABLE_GROUP_VA_SPACE : 
	         VM_MAX_USER_ADDRESS;

	/* invalidate all maps in this range */
	pmap_remove_range (pmap, (vm_offset_t)sdt_va, (vm_offset_t)sdt_vt);

	/*
	 * we can safely deallocated the page map(s)
	 */
	for (sdt = sdtp; sdt < (sdtp+PDT_TABLE_GROUP_SIZE); sdt++) {
	    ((sdt_entry_template_t *) sdt) -> bits = 0;
	    ((sdt_entry_template_t *) sdt+SDT_ENTRIES) -> bits = 0;
	}

	/*
	 * we have to unlock before freeing the table, since PT_FREE
	 * calls kmem_free or zfree, which will invoke another pmap routine
	 */
	PMAP_UNLOCK(pmap, spl);
	PT_FREE(gdttbl);
	PMAP_LOCK(pmap, spl);

    } /* Segment table Loop */

    PMAP_UNLOCK(pmap, spl);

#if	DBG
    if ((pmap_con_dbg & (CD_COL | CD_NORM)) == (CD_COL | CD_NORM))
      printf  ("(pmap_collect :%x) done \n", curproc);
#endif

    CHECK_PMAP_CONSISTENCY("pmap_collect");
} /* pmap collect() */



/*
 *	Routine:	PMAP_ACTIVATE
 *
 *	Function:
 *		Binds the given physical map to the given
 *		processor, and returns a hardware map description.
 *		In a mono-processor implementation the my_cpu
 *		argument is ignored, and the PMAP_ACTIVATE macro
 *		simply sets the MMU root pointer element of the PCB
 *		to the physical address of the segment descriptor table.
 *
 *	Parameters:
 *		pmap		pointer to pmap structure
 *		pcbp		pointer to current pcb
 *		cpu		CPU number
 */
void
pmap_activate(pmap_t pmap, pcb_t pcb)
{
#ifdef	lint
	my_cpu++;
#endif
	PMAP_ACTIVATE(pmap, pcb, 0);
} /* pmap_activate() */



/*
 *	Routine:	PMAP_DEACTIVATE
 *
 *	Function:
 *		Unbinds the given physical map from the given processor,
 *		i.e. the pmap i no longer is use on the processor.
 *		In a mono-processor the PMAP_DEACTIVATE macro is null.
 *
 *	Parameters:
 *		pmap		pointer to pmap structure
 *		pcb		pointer to pcb
 */
void
pmap_deactivate(pmap_t pmap, pcb_t pcb)
{
#ifdef	lint
	pmap++; th++; which_cpu++;
#endif
	PMAP_DEACTIVATE(pmap, pcb, 0);
} /* pmap_deactivate() */



/*
 *	Routine:	PMAP_KERNEL
 *
 *	Function:
 *		Retruns a pointer to the kernel pmap.
 */
pmap_t
pmap_kernel(void)
{
	return (kernel_pmap);
}/* pmap_kernel() */


/*
 *	Routine:	PMAP_COPY_PAGE
 *
 *	Function:
 *		Copies the specified (machine independent) pages.
 *
 *	Parameters:
 *		src		PA of source page
 *		dst		PA of destination page
 *
 *	Extern/Global:
 *		phys_map_vaddr1
 *		phys_map_vaddr2
 *
 *	Calls:
 *		m88kprotection
 *		M88K_TRUNC_PAGE
 *		cmmu_sflush_page
 *		DO_PTES
 *		bcopy
 *
 *	Special Assumptions:
 *		no locking reauired
 *
 *	This routine maps the phsical pages at the 'phys_map' virtual
 * addresses set up in pmap_bootstrap. It flushes the TLB to make the
 * new mappings effective, and performs the copy.
 */
void
pmap_copy_page(vm_offset_t src, vm_offset_t dst)
{
	vm_offset_t	dstva, srcva;
  	unsigned int spl_sav;
  	int i;
	int		aprot;
	pte_template_t	template;
	pt_entry_t	*dstpte, *srcpte;
	int 		my_cpu = cpu_number();

	/*
	 *	Map source physical address.
	 */
	aprot = m88k_protection (kernel_pmap, VM_PROT_READ | VM_PROT_WRITE);

	srcva = (vm_offset_t)(phys_map_vaddr1 + (cpu_number() * PAGE_SIZE));
	dstva = (vm_offset_t)(phys_map_vaddr2 + (cpu_number() * PAGE_SIZE));

	srcpte = pmap_pte(kernel_pmap, srcva);
	dstpte = pmap_pte(kernel_pmap, dstva);

	for (i=0; i < ptes_per_vm_page; i++, src += M88K_PGBYTES, dst += M88K_PGBYTES)
	  {
	    template.bits = M88K_TRUNC_PAGE(src) | aprot | DT_VALID | CACHE_GLOBAL;

	    /* do we need to write back dirty bits */
	    spl_sav = splimp();
	    cmmu_flush_tlb(1, srcva, M88K_PGBYTES);
	    *srcpte = template.pte;

	    /*
	     *	Map destination physical address.
	     */
	    template.bits = M88K_TRUNC_PAGE(dst) | aprot | CACHE_GLOBAL | DT_VALID;
	    cmmu_flush_tlb(1, dstva, M88K_PGBYTES);
	    *dstpte  = template.pte;
	    splx(spl_sav);

	    bcopy((void*)srcva, (void*)dstva, M88K_PGBYTES);
	    /* flush source, dest out of cache? */
	    cmmu_flush_remote_data_cache(my_cpu, src, M88K_PGBYTES);
	    cmmu_flush_remote_data_cache(my_cpu, dst, M88K_PGBYTES);
	  }

} /* pmap_copy_page() */


/*
 *	copy_to_phys
 *
 *	Copy virtual memory to physical memory by mapping the physical
 *	memory into virtual memory and then doing a virtual to virtual
 *	copy with bcopy.
 *
 *	Parameters:
 *		srcva		VA of source page
 *		dstpa		PA of destination page
 *		bytecount	copy byte size
 *
 *	Extern/Global:
 *		phys_map_vaddr2
 *
 *	Calls:
 *		m88kprotection
 *		M88K_TRUNC_PAGE
 *		cmmu_sflush_page
 *		DO_PTES
 *		bcopy
 *
 */
void
copy_to_phys(vm_offset_t srcva, vm_offset_t dstpa, int bytecount)
{
    vm_offset_t	dstva;
    pt_entry_t	*dstpte;
    int		copy_size,
   		offset,
    		aprot;
    unsigned int i;
    pte_template_t  template;

    dstva = (vm_offset_t)(phys_map_vaddr2 + (cpu_number() * PAGE_SIZE));
    dstpte = pmap_pte(kernel_pmap, dstva);
    copy_size = M88K_PGBYTES;
    offset = dstpa - M88K_TRUNC_PAGE(dstpa);
    dstpa -= offset;

    aprot = m88k_protection(kernel_pmap, VM_PROT_READ | VM_PROT_WRITE);
    while (bytecount > 0){
	copy_size = M88K_PGBYTES - offset;
	if (copy_size > bytecount)
	  copy_size = bytecount;

	/*
	 *      Map distation physical address. 
	 */

	for (i = 0; i < ptes_per_vm_page; i++)
	  {
	    template.bits = M88K_TRUNC_PAGE(dstpa) | aprot | CACHE_WT | DT_VALID;
	    cmmu_flush_tlb(1, dstva, M88K_PGBYTES);
	    *dstpte = template.pte;

	    dstva += offset;
	    bcopy((void*)srcva, (void*)dstva, copy_size);
	    srcva += copy_size;
	    dstva += copy_size;
	    dstpa += M88K_PGBYTES;
	    bytecount -= copy_size;
	    offset = 0;
	  }
      }
}

/*
 *	copy_from_phys
 *
 *	Copy physical memory to virtual memory by mapping the physical
 *	memory into virtual memory and then doing a virtual to virtual
 *	copy with bcopy.
 *
 *	Parameters:
 *		srcpa		PA of source page
 *		dstva		VA of destination page
 *		bytecount	copy byte size
 *
 *	Extern/Global:
 *		phys_map_vaddr2
 *
 *	Calls:
 *		m88kprotection
 *		M88K_TRUNC_PAGE
 *		cmmu_sflush_page
 *		DO_PTES
 *		bcopy
 *
 */
void
copy_from_phys(vm_offset_t srcpa, vm_offset_t dstva, int bytecount)
{
    register vm_offset_t	srcva;
    register pt_entry_t	*srcpte;
    register int		copy_size, offset;
    int             aprot;
    unsigned int i;
    pte_template_t  template;

    srcva = (vm_offset_t)(phys_map_vaddr2 + (cpu_number() * PAGE_SIZE));
    srcpte = pmap_pte(kernel_pmap, srcva);
    copy_size = M88K_PGBYTES;
    offset = srcpa - M88K_TRUNC_PAGE(srcpa);
    srcpa -= offset;

    aprot = m88k_protection(kernel_pmap, VM_PROT_READ | VM_PROT_WRITE);
    while (bytecount > 0){
	copy_size = M88K_PGBYTES - offset;
	if (copy_size > bytecount)
	  copy_size = bytecount;

	/*
	 *      Map destnation physical address.
	 */

	for (i=0; i < ptes_per_vm_page; i++)
	  {
	    template.bits = M88K_TRUNC_PAGE(srcpa) | aprot | CACHE_WT | DT_VALID;
	    cmmu_flush_tlb(1, srcva, M88K_PGBYTES);
	    *srcpte = template.pte;

	    srcva += offset;
	    bcopy((void*)srcva, (void*)dstva, copy_size);
	    srcpa += M88K_PGBYTES;
	    dstva += copy_size;
	    srcva += copy_size;
	    bytecount -= copy_size;
	    offset = 0;
	    /* cache flush source? */
	  }
      }
}

/*
 * Routine:	PMAP_PAGEABLE
 *
 * History:
 *	'90.7.16	Fuzzy
 *
 * Function:
 *	Make the specified pages (by pmap, offset) pageable (or not) as
 *	requested. A page which is not pageable may not take a fault;
 *	therefore, its page table entry must remain valid for the duration.
 *	this routine is merely advisory; pmap_enter will specify that
 *	these pages are to be wired down (or not) as appropriate.
 *
 * Parameters:
 *	pmap		pointer to pmap structure
 *	start		virtual address of start of range
 *	end		virtual address of end of range
 *	pageable	flag indicating whether range is to be pageable.
 *
 *	This routine currently does nothing in the 88100 implemetation.
 */
void
pmap_pageable(pmap_t pmap, vm_offset_t start, vm_offset_t end,
							boolean_t pageable)
{
#ifdef	lint
	pmap++; start++; end++; pageable++;
#endif
} /* pmap_pagealbe() */



/*
 * Routine:	PMAP_REDZONE
 *
 * Function:
 *	Give the kernel read-only access to the specified address. This
 *	is used to detect stack overflows. It is assumed that the address
 *	specified is the last possible kernel stack address. Therefore, we
 *	round up to the nearest machine dependent page.
 *
 * Parameters:
 *	pmap		pointer to pmap structure
 *	addr		virtual address of page to which access should
 *			be restricted to read-only
 *
 * Calls:
 *	M88K_ROUND_PAGE
 *	PMAP_LOCK
 *	pmap_pte
 *	PDT_VALID
 *
 *	This function calls pmap_pte to obtain a pointer to the page
 * table entry associated with the given virtual address. If there is a
 * page entry, and it is valid, its write protect bit will be set.
 */
void
pmap_redzone(pmap_t pmap, vm_offset_t va)
{
    pt_entry_t		*pte;
    int			spl, spl_sav;
    int			i;
    unsigned		users;
    pte_template_t	opte;
    int			kflush;

    va = M88K_ROUND_PAGE(va);
    PMAP_LOCK(pmap, spl);

    users = 0;
    if (pmap == kernel_pmap) {
	kflush = 1;
    } else {
	kflush = 0;
    }

    if ((pte = pmap_pte(pmap, va)) != PT_ENTRY_NULL && PDT_VALID(pte))
      for (i = ptes_per_vm_page; i > 0; i--) {

	  /*
	   * Invalidate pte temporarily to avoid being written back
	   * the modified bit and/or the reference bit by other cpu.
	   */
	  spl_sav = splimp();
	  opte.bits = invalidate_pte(pte);
	  opte.pte.prot = M88K_RO;
	  ((pte_template_t *)pte)->bits = opte.bits;
	  flush_atc_entry(users, va, kflush);
	  splx(spl_sav);
	  pte++;
	  va +=M88K_PGBYTES;
      }

    PMAP_UNLOCK(pmap, spl);

} /* pmap_redzone() */



/*
 *	Routine:	PMAP_CLEAR_MODIFY
 *
 *	Function:
 *		Clear the modify bits on the specified physical page.
 *
 *	Parameters:
 *		phys	physical address of page
 *
 *	Extern/Global:
 *		pv_head_table, pv_lists
 *		pmap_modify_list
 *
 *	Calls:
 *		PMAP_MANAGED
 *		SPLVM, SPLX
 *		PFIDX
 *		PFIDX_TO_PVH
 *		CHECK_PV_LIST
 *		simple_lock, simple_unlock
 *		pmap_pte
 *		panic
 *
 *	For managed pages, the modify_list entry corresponding to the
 * page's frame index will be zeroed. The PV list will be traversed.
 * For each pmap/va the hardware 'modified' bit in the page descripter table
 * entry inspected - and turned off if  necessary. If any of the
 * inspected bits were found on, an TLB flush will be performed.
 */
void
pmap_clear_modify(vm_offset_t phys)
{
    pv_entry_t		pvl;
    int			pfi;
    pv_entry_t		pvep;
    pt_entry_t		*pte;
    pmap_t		pmap;
    int			spl, spl_sav;
    vm_offset_t		va;
    int			i;
    unsigned		users;
    pte_template_t	opte;
    int			kflush;

    if (!PMAP_MANAGED(phys)) {
#ifdef	DBG
	if (pmap_con_dbg & CD_CMOD)
	  printf("(pmap_clear_modify :%x) phys addr 0x%x not managed \n", curproc, phys);
#endif
	return;
    }

    SPLVM(spl);

clear_modify_Retry:
    pfi = PFIDX(phys);
    pvl = PFIDX_TO_PVH(pfi);
    CHECK_PV_LIST (phys, pvl, "pmap_clear_modify");

    /* update correspoinding pmap_modify_list element */
    pmap_modify_list[pfi] = 0;

    if (pvl->pmap == PMAP_NULL) {
#ifdef	DEBUG
	if ((pmap_con_dbg & (CD_CMOD | CD_NORM)) == (CD_CMOD | CD_NORM))
	  printf("(pmap_clear_modify :%x) phys addr 0x%x not mapped\n", curproc, phys);
#endif

	SPLX(spl);
	return;
    }

    /* for each listed pmap, trun off the page modified bit */
    pvep = pvl;
    while (pvep != PV_ENTRY_NULL) {
	pmap = pvep->pmap;
	va = pvep->va;
	if (!simple_lock_try(&pmap->lock)) {
		goto clear_modify_Retry;
	}

	users = 0;
	if (pmap == kernel_pmap) {
	    kflush = 1;
	} else {
	    kflush = 0;
	}

	pte = pmap_pte(pmap, va);
	if (pte == PT_ENTRY_NULL)
	  panic("pmap_clear_modify: bad pv list entry.");

	for (i = ptes_per_vm_page; i > 0; i--) {

	    /*
	     * Invalidate pte temporarily to avoid being written back
	     * the modified bit and/or the reference bit by other cpu.
	     */
	    spl_sav = splimp();
	    opte.bits = invalidate_pte(pte);
	    /* clear modified bit */
	    opte.pte.modified = 0;
	    ((pte_template_t *)pte)->bits = opte.bits;
	    flush_atc_entry(users, va, kflush);
	    splx(spl_sav);
	    pte++;
	    va += M88K_PGBYTES;
	}

	simple_unlock(&pmap->lock);

	pvep = pvep->next;
    }

    SPLX(spl);

} /* pmap_clear_modify() */



/*
 *	Routine:	PMAP_IS_MODIFIED
 *
 *	Function:
 *		Return whether or not the specified physical page is modified
 *		by any physical maps. That is, whether the hardware has
 *		stored data into the page.
 *
 *	Parameters:
 *		phys		physical address og a page
 *
 *	Extern/Global:
 *		pv_head_array, pv lists
 *		pmap_modify_list
 *
 *	Calls:
 *		simple_lock, simple_unlock
 *		SPLVM, SPLX
 *		PMAP_MANAGED
 *		PFIDX
 *		PFIDX_TO_PVH
 *		pmap_pte
 *
 *	If the physical address specified is not a managed page, this
 * routine simply returns TRUE (looks like it is returning FALSE XXX).
 *
 *	If the entry in the modify list, corresponding to the given page,
 * is TRUE, this routine return TRUE. (This means at least one mapping
 * has been invalidated where the MMU had set the modified bit in the
 * page descripter table entry (PTE).
 *
 *	Otherwise, this routine walks the PV list corresponding to the
 * given page. For each pmap/va pair, the page descripter table entry is
 * examined. If a modified bit is found on, the function returns TRUE
 * immediately (doesn't need to walk remainder of list).
 */
boolean_t
pmap_is_modified(vm_offset_t phys)
{
	pv_entry_t	pvl;
	int		pfi;
	pv_entry_t	pvep;
	pt_entry_t	*ptep;
	int		spl;
	int		i;
	boolean_t	modified_flag;

	if (!PMAP_MANAGED(phys)) {
#ifdef	DBG
	    if (pmap_con_dbg & CD_IMOD)
		printf("(pmap_is_modified :%x) phys addr 0x%x not managed\n", curproc, phys);
#endif
		return(FALSE);
	}

	SPLVM(spl);

	pfi = PFIDX(phys);
	pvl = PFIDX_TO_PVH(pfi);
	CHECK_PV_LIST (phys, pvl, "pmap_is_modified");
is_mod_Retry:

	if ((boolean_t) pmap_modify_list[pfi]) {
		/* we've already cached a modify flag for this page,
			no use looking further... */
#ifdef	DBG
	    if ((pmap_con_dbg & (CD_IMOD | CD_NORM)) == (CD_IMOD | CD_NORM))
		printf("(pmap_is_modified :%x) already cached a modify flag for this page\n", curproc);
#endif
		SPLX(spl);
		return(TRUE);
	}

	if (pvl->pmap == PMAP_NULL) {
		/* unmapped page - get info from page_modified array
			maintained by pmap_remove_range/ pmap_remove_all */
		modified_flag = (boolean_t) pmap_modify_list[pfi];
#ifdef	DBG
		if ((pmap_con_dbg & (CD_IMOD | CD_NORM)) == (CD_IMOD | CD_NORM))
		  printf("(pmap_is_modified :%x) phys addr 0x%x not mapped\n", curproc, phys);
#endif
		SPLX(spl);
		return(modified_flag);
	}

	/* for each listed pmap, check modified bit for given page */
	pvep = pvl;
	while (pvep != PV_ENTRY_NULL) {
		if (!simple_lock_try(&pvep->pmap->lock)) {
			UNLOCK_PVH(pfi);
			goto is_mod_Retry;
		}

		ptep = pmap_pte(pvep->pmap, pvep->va);
		if (ptep == PT_ENTRY_NULL) {
			printf("pmap_is_modified: pte from pv_list not in map virt = 0x%x\n", pvep->va);
			panic("pmap_is_modified: bad pv list entry");
		}
		for (i = ptes_per_vm_page; i > 0; i--) {
			if (ptep->modified) {
				simple_unlock(&pvep->pmap->lock);
#ifdef	DBG
				if ((pmap_con_dbg & (CD_IMOD | CD_FULL)) == (CD_IMOD | CD_FULL))
				  printf("(pmap_is_modified :%x) modified page pte@0x%x\n", curproc, (unsigned)ptep);
#endif
				SPLX(spl);
				return(TRUE);
			}
			ptep++;
		}
		simple_unlock(&pvep->pmap->lock);

		pvep = pvep->next;
	}

	SPLX(spl);
	return(FALSE);

} /* pmap_is_modified() */



/*
 *	Routine:	PMAP_CLEAR_REFERECE
 *
 *	History:
 *		'90. 7.16	Fuzzy	unchanged
 *		'90. 7.19	Fuzzy	comment "Calls:' add
 *		'90. 8.21	Fuzzy	Debugging message add
 *		'93. 3. 1	jfriedl Added call to LOCK_PVH
 *
 *	Function:
 *		Clear the reference bits on the specified physical page.
 *
 *	Parameters:
 *		phys		physical address of page
 *
 *	Calls:
 *		PMAP_MANAGED
 *		SPLVM, SPLX
 *		PFIDX
 *		PFIDX_TO_PVH
 *		CHECK_PV_LIST
 *		simple_lock
 *		pmap_pte
 *		panic
 *
 *	Extern/Global:
 *		pv_head_array, pv lists
 *
 *	For managed pages, the coressponding PV list will be traversed.
 * For each pmap/va the hardware 'used' bit in the page table entry
 * inspected - and turned off if necessary. If any of the inspected bits
 * werw found on, an TLB flush will be performed.
 */
void
pmap_clear_reference(vm_offset_t phys)
{
    pv_entry_t		pvl;
    int			pfi;
    pv_entry_t		pvep;
    pt_entry_t		*pte;
    pmap_t		pmap;
    int			spl, spl_sav;
    vm_offset_t		va;
    int			i;
    unsigned		users;
    pte_template_t	opte;
    int			kflush;

    if (!PMAP_MANAGED(phys)) {
#ifdef	DBG
	if (pmap_con_dbg & CD_CREF) {
	    printf("(pmap_clear_reference :%x) phys addr 0x%x not managed\n", curproc,phys);
	}
#endif
	return;
    }

    SPLVM(spl);

clear_reference_Retry:
    pfi = PFIDX(phys);
    pvl = PFIDX_TO_PVH(pfi);
    CHECK_PV_LIST(phys, pvl, "pmap_clear_reference");


    if (pvl->pmap == PMAP_NULL) {
#ifdef	DBG
	if ((pmap_con_dbg & (CD_CREF | CD_NORM)) == (CD_CREF | CD_NORM))
	  printf("(pmap_clear_reference :%x) phys addr 0x%x not mapped\n", curproc,phys);
#endif
	SPLX(spl);
	return;
    }

    /* for each listed pmap, turn off the page refrenced bit */
    pvep = pvl;
    while (pvep != PV_ENTRY_NULL) {
	pmap = pvep->pmap;
	va = pvep->va;
	if (!simple_lock_try(&pmap->lock)) {
		goto clear_reference_Retry;
	}

	users = 0;
	if (pmap == kernel_pmap) {
	    kflush = 1;
	} else {
	    kflush = 0;
	}

	pte = pmap_pte(pmap, va);
	if (pte == PT_ENTRY_NULL)
	  panic("pmap_clear_reference: bad pv list entry.");

	for (i = ptes_per_vm_page; i > 0; i--) {

	    /*
	     * Invalidate pte temporarily to avoid being written back
	     * the modified bit and/or the reference bit by other cpu.
	     */
	    spl_sav = splimp();
	    opte.bits = invalidate_pte(pte);
	    /* clear reference bit */
	    opte.pte.pg_used = 0;
	    ((pte_template_t *)pte)->bits = opte.bits;
	    flush_atc_entry(users, va, kflush);
	    splx(spl_sav);
	    pte++;
	    va += M88K_PGBYTES;
	}

	simple_unlock(&pmap->lock);

	pvep = pvep->next;
    }

    SPLX(spl);

} /* pmap_clear_reference() */



/*
 * Routine:	PMAP_IS_REFERENCED
 *
 * History:
 *	'90. 7.16	Fuzzy
 *	'90. 7.19	Fuzzy	comment	'Calls:' add
 *
 * Function:
 *	Retrun whether or not the specifeid physical page is referenced by
 *	any physical maps. That is, whether the hardware has touched the page.
 *
 * Parameters:
 *	phys		physical address of a page
 *
 * Extern/Global:
 *	pv_head_array, pv lists
 *
 * Calls:
 *	PMAP_MANAGED
 *	SPLVM
 *	PFIDX
 *	PFIDX_TO_PVH
 *	CHECK_PV_LIST
 *	simple_lock
 *	pmap_pte
 *
 *	If the physical address specified is not a managed page, this
 * routine simply returns TRUE.
 *
 *	Otherwise, this routine walks the PV list corresponding to the
 * given page. For each pmap/va/ pair, the page descripter table entry is
 * examined. If a used bit is found on, the function returns TRUE
 * immediately (doesn't need to walk remainder of list).
 */
boolean_t
pmap_is_referenced(vm_offset_t phys)
{
    pv_entry_t	pvl;
    int		pfi;
    pv_entry_t	pvep;
    pt_entry_t	*ptep;
    int		spl;
    int		i;
    
    if (!PMAP_MANAGED(phys))
      return(FALSE);
    
    SPLVM(spl);
    
    pfi = PFIDX(phys);
    pvl = PFIDX_TO_PVH(pfi);
    CHECK_PV_LIST(phys, pvl, "pmap_is_referenced");
    
is_ref_Retry:

    if (pvl->pmap == PMAP_NULL) {
	SPLX(spl);
	return(FALSE);
    }

    /* for each listed pmap, check used bit for given page */
    pvep = pvl;
    while (pvep != PV_ENTRY_NULL) {
	if (!simple_lock_try(&pvep->pmap->lock)) {
		UNLOCK_PVH(pfi);
		goto is_ref_Retry;
	}

	ptep = pmap_pte(pvep->pmap, pvep->va);
	if (ptep == PT_ENTRY_NULL)
	  panic("pmap_is_referenced: bad pv list entry.");
	for (i = ptes_per_vm_page; i > 0; i--) {
	    if (ptep->pg_used) {
		simple_unlock(&pvep->pmap->lock);
		SPLX(spl);
		return(TRUE);
	    }
	    ptep++;
	}
	simple_unlock(&pvep->pmap->lock);

	pvep = pvep->next;
    }
    
    SPLX(spl);
    return(FALSE);
} /* pmap_is referenced() */

/*
 *	Routine:	PMAP_VERIFY_FREE
 *
 *	History:
 *	'90. 7.17	Fuzzy	This routine extract vax's pmap.c.
 *				This do not exit in m68k's pmap.c.
 *				vm_page_alloc calls this.
 *				Variables changed below,
 *					vm_first_phys --> pmap_phys_start
 *					vm_last_phys  --> pmap_phys_end
 *				Macro chnged below,
 *					pa_index   --> PFIDX
 *					pai_to_pvh --> PFI_TO_PVH
 *
 *	Calls:
 *		SPLVM, SPLX
 *		PFIDX
 *		PFI_TO_PVH
 *
 *	Global/Extern:
 *		pmap_initialized
 *		pmap_phys_start
 *		pmap_phys_end
 *		TRUE, FALSE
 *		PMAP_NULL
 *
 *	This routine check physical address if that have pmap modules.
 *	It returns TRUE/FALSE.
 */

boolean_t
pmap_verify_free(vm_offset_t phys)
{
	pv_entry_t	pv_h;
	int		spl;
	boolean_t	result;

	if (!pmap_initialized)
		return(TRUE);

	if (!PMAP_MANAGED(phys))
		return(FALSE);

	SPLVM(spl);

	pv_h = PFIDX_TO_PVH(PFIDX(phys));

	result = (pv_h->pmap == PMAP_NULL);
	SPLX(spl);

	return(result);

} /* pmap_verify_free */


/*
 *	Routine:	PMAP_VALID_PAGE
 *
 *	The physical address space is dense... there are no holes.
 *	All addresses provided to vm_page_startup() are valid.
 */
boolean_t
pmap_valid_page(vm_offset_t p)
{
#ifdef	lint
	p++;
#endif
	return(TRUE);
}	/* pmap_valid_page() */

/*
 * Routine:	PMAP_PAGE_PROTECT
 *
 * Calls:
 *	pmap_copy_on_write
 *	pmap_remove_all
 *
 *	Lower the permission for all mappings to a given page.
 */
void
pmap_page_protect(vm_offset_t phys, vm_prot_t prot)
{
	switch (prot) {
		case VM_PROT_READ:
		case VM_PROT_READ|VM_PROT_EXECUTE:
			pmap_copy_on_write(phys);
			break;
		case VM_PROT_ALL:
			break;
		default:
			pmap_remove_all(phys);
			break;
	}
}

#if FUTURE_MAYBE
/*
 * Routine:	PAGEMOVE
 *
 * Function:
 *	Move pages from one kernel virtual address to another.
 *
 * Parameters:
 *	from	kernel virtual address of source
 *	to	kernel virtual address of distination
 *	size	size in bytes
 *
 * Calls:
 *	PMAP_LOCK
 *	PMAP_UNLOCK
 *	LOCK_PVH
 *	UNLOCK_PVH
 *	CHECK_PV_LIST
 *	pmap_pte
 *	pmap_expand_kmap
 *	cmmu_sflush
 *
 * Special Assumptions:
 *	size must be a multiple of CLBYTES (?)
 */
void
pagemove(vm_offset_t from, vm_offset_t to, int size)
{
    vm_offset_t		pa;
    pt_entry_t		*srcpte, *dstpte;
    int			pfi;
    pv_entry_t		pvl;
    int			spl;
    int			i;
    unsigned		users;
    pte_template_t	opte;

    PMAP_LOCK(kernel_pmap, spl);

    users = 0;

    while (size > 0) {

	/*
	 * check if the source addr is mapped
	 */
	if ((srcpte = pmap_pte(kernel_pmap, (vm_offset_t)from)) == PT_ENTRY_NULL) {
	    printf("pagemove: source vaddr 0x%x\n", from);
	    panic("pagemove: Source addr not mapped");
	}

	/*
	 *
	 */
	if ((dstpte = pmap_pte(kernel_pmap, (vm_offset_t)to)) == PT_ENTRY_NULL)
	  if ((dstpte = pmap_expand_kmap((vm_offset_t)to, VM_PROT_READ | VM_PROT_WRITE))
	      == PT_ENTRY_NULL)
	    panic("pagemove: Cannot allocate distination pte");
	/*
	 *
	 */
	if (dstpte->dtype == DT_VALID) {
	    printf("pagemove: distination vaddr 0x%x, pte = 0x%x\n", to, *((unsigned *)dstpte));
	    panic("pagemove: Distination pte already valid");
	}

#ifdef DEBUG
	if ((pmap_con_dbg & (CD_PGMV | CD_NORM)) == (CD_PGMV | CD_NORM))
	  printf("(pagemove :%x) from 0x%x to 0x%x\n", curproc, from, to);
	if ((pmap_con_dbg & (CD_PGMV | CD_FULL)) == (CD_PGMV | CD_FULL))
	  printf("(pagemove :%x) srcpte @ 0x%x = %x dstpte @ 0x%x = %x\n", curproc, (unsigned)srcpte, *(unsigned *)srcpte, (unsigned)dstpte, *(unsigned *)dstpte);

#endif /* DEBUG */

	/*
	 * Update pv_list
	 */
	pa = M88K_PTOB(srcpte->pfn);
	if (PMAP_MANAGED(pa)) {
	    pfi = PFIDX(pa);
	    pvl = PFIDX_TO_PVH(pfi);
	    CHECK_PV_LIST(pa, pvl, "pagemove");
	    pvl->va = (vm_offset_t)to;
	}

	/*
	 * copy pte
	 */
	for (i = ptes_per_vm_page; i > 0; i--) {
	    /*
	     * Invalidate pte temporarily to avoid being written back
	     * the modified bit and/or the reference bit by other cpu.
	     */
	    opte.bits = invalidate_pte(srcpte);
	    flush_atc_entry(users, from, 1);
	    ((pte_template_t *)dstpte)->bits = opte.bits;
	    from += M88K_PGBYTES;
	    to += M88K_PGBYTES;
	    srcpte++; dstpte++;
	}
	size -= PAGE_SIZE;
    }

    PMAP_UNLOCK(kernel_pmap, spl);

} /* pagemove */

#endif /* FUTURE_MAYBE */
/*
 *	Routine:	icache_flush
 *
 *	Function:
 *		Invalidate instruction cache for all CPUs on specified
 *		physical address.  Called when a page is removed from a
 *		vm_map.  This is done because the Instruction CMMUs are not
 *		snooped, and if a page is subsequently used as a text page,
 *		we want the CMMUs to re-load the cache for the page.
 *
 *	Parameters:
 *		pa	physical address of the (vm) page
 *
 *	Extern/globals:
 *		ptes_per_vm_page
 *
 *	Calls:
 *		cachefall
 *
 *	Called by:
 *		vm_remove_page
 *
 */
void
icache_flush(vm_offset_t pa)
{
    int 	i;
    int 	cpu = 0;

    for (i = ptes_per_vm_page; i > 0; i--, pa += M88K_PGBYTES) {
	 cmmu_flush_remote_inst_cache(cpu, pa, M88K_PGBYTES);
    }

} /* icache_flush */

/*
 *	Routine:	pmap_dcache_flush
 *
 *	Function:
 *		Flush DATA cache on specified virtual address.
 *
 *	Parameters:
 *		pmap	specify pmap
 *		va	virtual address of the (vm) page to be flushed
 *
 *	Extern/globals:
 *		pmap_pte
 *		ptes_per_vm_page
 *
 *	Calls:
 *		dcacheflush
 *
 */
void
pmap_dcache_flush(pmap_t pmap, vm_offset_t va)
{
    vm_offset_t pa;
    int 	i;
    int		spl;

    if (pmap == PMAP_NULL)
	panic("pmap_dcache_flush: pmap is NULL");

    PMAP_LOCK(pmap, spl);

    pa = M88K_PTOB((pmap_pte(pmap, va))->pfn);
    for (i = ptes_per_vm_page; i > 0; i--, pa += M88K_PGBYTES) {
	cmmu_flush_data_cache(pa, M88K_PGBYTES);
    }

    PMAP_UNLOCK(pmap, spl);


} /* pmap_dcache_flush */

STATIC void
cache_flush_loop(int mode, vm_offset_t pa, int size)
{
    int		i;
    int		ncpus;
    void 	(*cfunc)(int cpu, vm_offset_t physaddr, int size);

    switch (mode) {
    default:
	panic("bad cache_flush_loop mode");
	return;

    case FLUSH_CACHE:	/* All caches, all CPUs */
	ncpus = NCPUS;
	cfunc = cmmu_flush_remote_cache;
	break;

    case FLUSH_CODE_CACHE:	/* Instruction caches, all CPUs */
	ncpus = NCPUS;
	cfunc = cmmu_flush_remote_inst_cache;
	break;
    
    case FLUSH_DATA_CACHE:	/* Data caches, all CPUs */
	ncpus = NCPUS;
	cfunc = cmmu_flush_remote_data_cache;
	break;

    case FLUSH_LOCAL_CACHE:		/* Both caches, my CPU */
	ncpus = 1;
	cfunc = cmmu_flush_remote_cache;
	break;
    
    case FLUSH_LOCAL_CODE_CACHE:	/* Instruction cache, my CPU */
	ncpus = 1;
	cfunc = cmmu_flush_remote_inst_cache;
	break;

    case FLUSH_LOCAL_DATA_CACHE:	/* Data cache, my CPU */
	ncpus = 1;
	cfunc = cmmu_flush_remote_data_cache;
	break;
    }

    if (ncpus == 1) {
	(*cfunc)(cpu_number(), pa, size);
    }
    else {
	for (i=0; i<NCPUS; i++) {
		(*cfunc)(i, pa, size);
	}
    }
}

/*
 * pmap_cache_flush
 *	Internal function.
 */
void
pmap_cache_flush(pmap_t pmap, vm_offset_t virt, int bytes, int mode)
{
    vm_offset_t pa;
    vm_offset_t va;
    int 	i;
    int		spl;

    if (pmap == PMAP_NULL)
	panic("pmap_dcache_flush: NULL pmap");

    /*
     * If it is more than a couple of pages, just blow the whole cache
     * because of the number of cycles involved.
     */
    if (bytes > 2*M88K_PGBYTES) {
	cache_flush_loop(mode, 0, -1);
	return;
    }

    PMAP_LOCK(pmap, spl);
    for(va = virt; bytes > 0; bytes -= M88K_PGBYTES,va += M88K_PGBYTES) {
        pa = M88K_PTOB((pmap_pte(pmap, va))->pfn);
        for (i = ptes_per_vm_page; i > 0; i--, pa += M88K_PGBYTES) {
	    cache_flush_loop(mode, pa, M88K_PGBYTES);
        }
    }
    PMAP_UNLOCK(pmap, spl);
} /* pmap_cache_flush */

#ifdef	DEBUG
/*
 *  DEBUGGING ROUTINES - check_pv_list and check_pmp_consistency are used
 *		only for debugging.  They are invoked only
 *		through the macros CHECK_PV_LIST AND CHECK_PMAP_CONSISTENCY
 *		defined early in this sourcefile.
 */

/*
 *	Routine:	CHECK_PV_LIST (internal)
 *
 *	Function:
 *		Debug-mode routine to check consistency of a PV list. First, it
 *		makes sure every map thinks the physical page is the same. This
 *		should be called by all routines which touch a PV list.
 *
 *	Parameters:
 *		phys	physical address of page whose PV list is
 *			to be checked
 *		pv_h	pointer to head to the PV list
 *		who	string containing caller's name to be
 *			printed if a panic arises
 *
 *	Extern/Global:
 *		pv_head_array, pv lists
 *
 *	Calls:
 *		pmap_extract
 *
 *	Special Assumptions:
 *		No locking is required.
 *
 *	This function walks the given PV list. For each pmap/va pair,
 * pmap_extract is called to obtain the physical address of the page from
 * the pmap in question. If the retruned physical address does not match
 * that for the PV list being perused, the function panics.
 */

STATIC void
check_pv_list(vm_offset_t phys, pv_entry_t pv_h, char *who)
{
	pv_entry_t	pv_e;
	pt_entry_t 	*pte;
	vm_offset_t	pa;

	if (pv_h != PFIDX_TO_PVH(PFIDX(phys))) {
		printf("check_pv_list: incorrect pv_h supplied.\n");
		panic(who);
	}

	if (!PAGE_ALIGNED(phys)) {
		printf("check_pv_list: supplied phys addr not page aligned.\n");
		panic(who);
	}

	if (pv_h->pmap == PMAP_NULL) {
		if (pv_h->next != PV_ENTRY_NULL) {
			printf("check_pv_list: first entry has null pmap, but list non-empty.\n");
			panic(who);
		}
		else	return;		/* proper empry lst */
	}

	pv_e = pv_h;
	while (pv_e != PV_ENTRY_NULL) {
		if (!PAGE_ALIGNED(pv_e->va)) {
			printf("check_pv_list: non-aligned VA in entry at 0x%x.\n", pv_e);
			panic(who);
		}
		/*
		 * We can't call pmap_extract since it requires lock.
		 */
		if ((pte = pmap_pte(pv_e->pmap, pv_e->va)) == PT_ENTRY_NULL)
		  pa = (vm_offset_t)0;
		else
		  pa = M88K_PTOB(pte->pfn) | (pv_e->va & M88K_PGOFSET);

		if (pa != phys) {
			printf("check_pv_list: phys addr diff in entry at 0x%x.\n", pv_e);
			panic(who);
		}

		pv_e = pv_e->next;
	}

} /* check_pv_list() */

/*
 *	Routine:	CHECK_MAP (itnernal)
 *
 *	Function:
 *		Debug mode routine to check consistency of map.
 *		Called by check_pmap_consistency only.
 *
 *	Parameters:
 *		map	pointer to pmap structure
 *		s	start of range to be checked
 *		e	end of range to be checked
 *		who	string containing caller's name to be
 *			printed if a panic arises
 *
 *	Extern/Global:
 *		pv_head_array, pv lists
 *
 *	Calls:
 *		pmap_pte
 *
 *	Special Assumptions:
 *		No locking required.
 *
 *	This function sequences through the given range of  addresses. For
 * each page, pmap_pte is called to obtain the page table entry. If
 * its valid, and the physical page it maps is managed, the PV list is
 * searched for the corresponding pmap/va entry. If not found, the
 * function panics. If duplicate PV list entries are found, the function
 * panics.
 */

STATIC void
check_map(pmap_t map, vm_offset_t s, vm_offset_t e, char *who)
{
	vm_offset_t	va,
			old_va,
			phys;
	pv_entry_t	pv_h,
			pv_e,
			saved_pv_e;
	pt_entry_t	*ptep;
	boolean_t	found;
	int		loopcnt;


	/*
	 * for each page in the address space, check to see if there's
	 * a valid mapping. If so makes sure it's listed in the PV_list.
	 */

	if ((pmap_con_dbg & (CD_CHKM | CD_NORM)) == (CD_CHKM | CD_NORM))
		printf("(check_map) checking map at 0x%x\n", map);

	old_va = s;
	for (va = s; va < e; va += PAGE_SIZE) {
		/* check for overflow - happens if e=0xffffffff */
		if (va < old_va)
			break;
		else
			old_va = va;

		if (va == phys_map_vaddr1 || va == phys_map_vaddr2)
			/* don't try anything with these */
			continue;

		ptep = pmap_pte(map, va);

		if (ptep == PT_ENTRY_NULL) {
			/* no page table, skip to next segment entry */
			va = SDT_NEXT(va)-PAGE_SIZE;
			continue;
		}

		if (!PDT_VALID(ptep))
			continue;		/* no page mapping */

		phys = M88K_PTOB(ptep->pfn);	/* pick up phys addr */

		if (!PMAP_MANAGED(phys))
			continue;		/* no PV list */

		/* note: vm_page_startup allocates some memory for itself
			through pmap_map before pmap_init is run. However,
			it doesn't adjust the physical start of memory.
			So, pmap thinks those pages are managed - but they're
			not actually under it's control. So, the following
			conditional is a hack to avoid those addresses
			reserved by vm_page_startup */
		/* pmap_init also allocate some memory for itself. */

		if (map == kernel_pmap &&
		    va < round_page((vm_offset_t)(pmap_modify_list + (pmap_phys_end - pmap_phys_start))))
			continue;

		pv_h = PFIDX_TO_PVH(PFIDX(phys));
		found = FALSE;

		if (pv_h->pmap != PMAP_NULL) {

			loopcnt = 10000;	/* loop limit */
			pv_e = pv_h;
			while(pv_e != PV_ENTRY_NULL) {

				if (loopcnt-- < 0) {
					printf("check_map: loop in PV list at PVH 0x%x (for phys 0x%x)\n", pv_h, phys);
					panic(who);
				}

				if (pv_e->pmap == map && pv_e->va == va) {
					if (found) {
						printf("check_map: Duplicate PV list entries at 0x%x and 0x%x in PV list 0x%x.\n", saved_pv_e, pv_e, pv_h);
						printf("check_map: for pmap 0x%x, VA 0x%x,phys 0x%x.\n", map, va, phys);
						panic(who);
					}
					else {
						found = TRUE;
						saved_pv_e = pv_e;
					}
				}
				pv_e = pv_e->next;
			}
		}

		if (!found) {
				printf("check_map: Mapping for pmap 0x%x VA 0x%x Phys 0x%x does not appear in PV list 0x%x.\n", map, va, phys, pv_h);
		}
	}

	if ((pmap_con_dbg & (CD_CHKM | CD_NORM)) == (CD_CHKM | CD_NORM))
		printf("(check_map) done \n");

} /* check_map() */

/*
 *	Routine:	CHECK_PMAP_CONSISTENCY (internal)
 *
 *	Function:
 *		Debug mode routine which walks all pmap, checking for internal
 *		consistency. We are called UNLOCKED, so we'll take the write
 *		lock.
 *
 *	Parameters:
 *		who		string containing caller's name tobe
 *				printed if a panic arises
 *
 *	Extern/Global:
 *		list of pmap structures
 *
 *	Calls:
 *		check map
 *		check pv_list
 *
 *	This function obtains the pmap write lock. Then, for each pmap
 * structure in the pmap struct queue, it calls check_map to verify the
 * consistency of its translation table hierarchy.
 *
 *	Once all pmaps have been checked, check_pv_list is called to check
 * consistency of the PV lists for each managed page.
 *
 *	NOTE: Added by Sugai 10/29/90
 *	There are some pages do not appaer in PV list. These pages are
 * allocated for pv structures by kmem_alloc called in pmap_init.
 * Though they are in the range of pmap_phys_start to pmap_phys_end,
 * PV maniupulations had not been activated when these pages were alloceted.
 *
 */

STATIC void
check_pmap_consistency(char *who)
{
	pmap_t		p;
	int		i;
	vm_offset_t	phys;
	pv_entry_t	pv_h;
	int		spl;

	if ((pmap_con_dbg & (CD_CHKPM | CD_NORM)) == (CD_CHKPM | CD_NORM))
		printf("check_pmap_consistency (%s :%x) start.\n", who, curproc);

	if (pv_head_table == PV_ENTRY_NULL) {

		printf("check_pmap_consistency (%s) PV head table not initialized.\n", who);
		return;
	}

	SPLVM(spl);

	p = kernel_pmap;
	check_map(p, VM_MIN_KERNEL_ADDRESS, VM_MAX_KERNEL_ADDRESS, who);

	/* run through all pmaps. check consistency of each one... */
	i = PMAP_MAX;
	for (p = kernel_pmap->next;p != kernel_pmap; p = p->next) {
		if (i == 0) { /* can not read pmap list */
			printf("check_pmap_consistency: pmap strcut loop error.\n");
			panic(who);
		}
		check_map(p, VM_MIN_USER_ADDRESS, VM_MAX_USER_ADDRESS, who);
	}

	/* run through all managed paes, check pv_list for each one */
	for (phys = pmap_phys_start; phys < pmap_phys_end; phys += PAGE_SIZE) {
		pv_h = PFIDX_TO_PVH(PFIDX(phys));
		check_pv_list(phys, pv_h, who);
	}

	SPLX(spl);

	if ((pmap_con_dbg & (CD_CHKPM | CD_NORM)) == (CD_CHKPM | CD_NORM))
		printf("check_pmap consistency (%s :%x): done.\n",who, curproc);

} /* check_pmap_consistency() */
#endif /* DEBUG */

/*
 * PMAP PRINT MACROS AND ROUTINES FOR DEBUGGING
 * These routines are called only from the debugger.
 */

#define	PRINT_SDT(p)							\
		printf("%08x : ",					\
			((sdt_entry_template_t *)p)-> bits);		\
		printf("table adress=0x%x, prot=%d, dtype=%d\n",	\
			M88K_PTOB(p->table_addr),			\
			p->prot,					\
			p->dtype);

#define	PRINT_PDT(p)							\
		printf("%08x : ",					\
			((pte_template_t *)p)-> bits);			\
		printf("frame num=0x%x, prot=%d, dtype=%d, wired=%d, modified=%d, pg_used=%d\n",	\
			p->pfn,						\
			p->prot,					\
			p->dtype,					\
			p->wired,					\
			p->modified,				\
			p->pg_used);

/*
 *	Routine:	PMAP_PRINT
 *
 *  	History:
 *
 *	Function:
 *		Print pmap stucture, including segment table.
 *
 *	Parameters:
 *		pmap		pointer to pmap structure
 *
 *	Special Assumptions:
 *		No locking required.
 *
 *	This function prints the fields of the pmap structure, then
 * iterates through the segment translation table, printing each entry.
 */
void
pmap_print(pmap_t pmap)
{
    sdt_entry_t	*sdtp;
    sdt_entry_t	*sdtv;
    int		i;

    printf("Pmap @ 0x%x:\n", (unsigned)pmap);
    sdtp = pmap->sdt_paddr;
    sdtv = pmap->sdt_vaddr;
    printf("	sdt_paddr: 0x%x; sdt_vaddr: 0x%x; ref_count: %d;\n",
	    (unsigned)sdtp, (unsigned)sdtv,
	    pmap->ref_count);

#ifdef	statistics_not_yet_maintained
    printf("	statistics: pagesize %d: free_count %d; "
	   "active_count %d; inactive_count %d; wire_count %d\n",
		pmap->stats.pagesize,
		pmap->stats.free_count,
		pmap->stats.active_count,
		pmap->stats.inactive_count,
		pmap->stats.wire_count);

    printf("	zero_fill_count %d; reactiveations %d; "
	   "pageins %d; pageouts %d; faults %d\n",
		pmap->stats.zero_fill_count,
		pmap->stats.reactivations,
		pmap->stats.pageins,
		pmap->stats.pageouts,
		pmap->stats.fault);

    printf("	cow_faults %d, lookups %d, hits %d\n",
		pmap->stats.cow_faults,
		pmap->stats.loopups,
		pmap->stats.faults);
#endif

    sdtp = (sdt_entry_t *) pmap->sdt_vaddr;	/* addr of physical table */
    sdtv = sdtp + SDT_ENTRIES;		/* shadow table with virt address */
    if (sdtp == (sdt_entry_t *)0)
	printf("Error in pmap - sdt_paddr is null.\n");
    else {
	int	count = 0;
	printf("	Segment table at 0x%x (0x%x):\n",
	    (unsigned)sdtp, (unsigned)sdtv);
	for (i = 0; i < SDT_ENTRIES; i++, sdtp++, sdtv++) {
	    if ((sdtp->table_addr != 0 ) || (sdtv->table_addr != 0)) {
		if (count != 0)
			printf("sdt entry %d skip !!\n", count);
		count = 0;
		printf("   (%x)phys: ", i);
		PRINT_SDT(sdtp);
		printf("   (%x)virt: ", i);
		PRINT_SDT(sdtv);
	    }
	    else
		count++;
	}
	if (count != 0)
	    printf("sdt entry %d skip !!\n", count);
    }

} /* pmap_print() */

/*
 * Routine:	PMAP_PRINT_TRACE
 *
 * Function:
 *	Using virt addr, derive phys addr, printing pmap tables along the way.
 *
 * Parameters:
 *	pmap		pointer to pmap strucuture
 *	va		virtual address whose translation is to be trace
 *	long_format	flag indicating long from output is desired
 *
 * Special Assumptions:
 *	No locking required.
 *
 *	This function chases down through the translation tree as
 * appropriate for the given virtual address. each table entry
 * encoutered is printed. If the long_format is desired, all entries of
 * each table are printed, with special indication of the entries used in
 * the translation.
 */
void
pmap_print_trace (pmap_t pmap, vm_offset_t va, boolean_t long_format)
{
    sdt_entry_t	*sdtp;	/* ptr to sdt table of physical addresses */
    sdt_entry_t	*sdtv;	/* ptr to sdt shadow table of virtual addresses */
    pt_entry_t	*ptep;	/* ptr to pte table of physical page addresses */

    int		i;	/* table loop index */
    unsigned long prev_entry;	/* keep track of value of previous table entry */
    int		n_dup_entries;	/* count contiguous duplicate entries */

    printf("Trace of virtual address 0x%08x. Pmap @ 0x%08x.\n",
	va, (unsigned)pmap);

    /*** SDT TABLES ***/
    /* get addrs of sdt tables */
    sdtp = (sdt_entry_t *)pmap->sdt_vaddr;
    sdtv = sdtp + SDT_ENTRIES;

    if (sdtp == SDT_ENTRY_NULL) {
	printf("    Segment table pointer (pmap.sdt_paddr) null, trace stops.\n");
	return;
    }

    n_dup_entries = 0;
    prev_entry = 0xFFFFFFFF;

    if (long_format) {
	printf("    Segment table at 0x%08x (virt shadow at 0x%08x)\n",
		(unsigned)sdtp, (unsigned)sdtv);
	for (i = 0; i < SDT_ENTRIES; i++, sdtp++, sdtv++) {
	    if (prev_entry == ((sdt_entry_template_t *)sdtp)->bits
		&& SDTIDX(va) != i && i != SDT_ENTRIES-1) {
		    n_dup_entries++;
		    continue;	/* suppress duplicate entry */
	    }
	    if (n_dup_entries != 0) {
		printf("    - %d duplicate entries skipped -\n",n_dup_entries);
		n_dup_entries = 0;
	    }
	    prev_entry = ((pte_template_t *)sdtp)->bits;
	    if (SDTIDX(va) == i) {
		printf("    >> (%x)phys: ", i);
	    } else {
		printf("       (%x)phys: ", i);
	    }
	    PRINT_SDT(sdtp);
	    if (SDTIDX(va) == i) {
		printf("    >> (%x)virt: ", i);
	    } else {
		printf("       (%x)virt: ", i);
	    }
	    PRINT_SDT(sdtv);
	} /* for */
    } else {
	/* index into both tables for given VA */
	sdtp += SDTIDX(va);
	sdtv += SDTIDX(va);
	printf("    SDT entry index 0x%x at 0x%x (virt shadow at 0x%x)\n",
	       SDTIDX(va), (unsigned)sdtp, (unsigned)sdtv);
	printf("    phys:  ");
	PRINT_SDT(sdtp);
	printf("    virt:  ");
	PRINT_SDT(sdtv);
    }

    /*** PTE TABLES ***/
    /* get addrs of page (pte) table (no shadow table) */

    sdtp = ((sdt_entry_t *)pmap->sdt_vaddr) + SDTIDX(va);
    #ifdef DBG
	    printf("*** DEBUG (sdtp) ");
	    PRINT_SDT(sdtp);
    #endif
    sdtv = sdtp + SDT_ENTRIES;
    ptep = (pt_entry_t *)(M88K_PTOB(sdtv->table_addr));
    if (sdtp->dtype != DT_VALID) {
	printf("    segment table entry invlid, trace stops.\n");
	return;
    }

    n_dup_entries = 0;
    prev_entry = 0xFFFFFFFF;
    if (long_format) {
	printf("        page table (ptes) at 0x%x\n", (unsigned)ptep);
	for (i = 0; i < PDT_ENTRIES; i++, ptep++) {
	    if (prev_entry == ((pte_template_t *)ptep)->bits
		&& PDTIDX(va) != i && i != PDT_ENTRIES-1) {
		    n_dup_entries++;
		    continue;	/* suppress suplicate entry */
	    }
	    if (n_dup_entries != 0) {
		printf("    - %d duplicate entries skipped -\n",n_dup_entries);
		n_dup_entries = 0;
	    }
	    prev_entry = ((pte_template_t *)ptep)->bits;
	    if (PDTIDX(va) == i) {
		printf("    >> (%x)pte: ", i);
	    } else	{
		printf("       (%x)pte: ", i);
	    }
	    PRINT_PDT(ptep);
	} /* for */
    } else {
	/* index into page table */
	ptep += PDTIDX(va);
	printf("    pte index 0x%x\n", PDTIDX(va));
	printf("    pte: ");
	PRINT_PDT(ptep);
    }
} /* pmap_print_trace() */

/*
 * Check whether the current transaction being looked at by dodexc()
 *  could have been the one that caused a fault.  Given the virtual
 *  address, map, and transaction type, checks whether the page at that
 *  address is valid, and, for write transactions, whether it has write
 *  permission.
 */
boolean_t
pmap_check_transaction(pmap_t pmap, vm_offset_t va, vm_prot_t type)
{
    pt_entry_t	*pte;
    sdt_entry_t	*sdt;
    int                         spl;

    PMAP_LOCK(pmap, spl);

    if ((pte = pmap_pte(pmap, va)) == PT_ENTRY_NULL) {
	PMAP_UNLOCK(pmap, spl);
	return FALSE;
    }
    
    if (!PDT_VALID(pte)) {
	PMAP_UNLOCK(pmap, spl);
	return FALSE;
    }

    /*
     * Valid pte.  If the transaction was a read, there is no way it
     *  could have been a fault, so return true.  For now, assume
     *  that a write transaction could have caused a fault.  We need
     *  to check pte and sdt entries for write permission to really
     *  tell.
     */

    if (type == VM_PROT_READ) {
	PMAP_UNLOCK(pmap, spl);
	return TRUE;
    } else {
	sdt = SDTENT(pmap,va);
	if (sdt->prot || pte->prot) {
	    PMAP_UNLOCK(pmap, spl);
	    return FALSE;
	} else {
	    PMAP_UNLOCK(pmap, spl);
	    return TRUE;
	}
    }
}

/* New functions to satisfy rpd - contributed by danner */

void
pmap_virtual_space(vm_offset_t *startp, vm_offset_t *endp)
{
    *startp = virtual_avail;
    *endp = virtual_end;
}

unsigned int
pmap_free_pages(void)
{
        return atop(avail_end - avail_next);
}

boolean_t
pmap_next_page(vm_offset_t *addrp)
{
        if (avail_next == avail_end)
                return FALSE;

        *addrp = avail_next;
        avail_next += PAGE_SIZE;
        return TRUE;
}

#if USING_BATC
#ifdef OMRON_PMAP
/*
 *      Set BATC
 */
void
pmap_set_batc(
    pmap_t pmap,
    boolean_t data,
    int i,
    vm_offset_t va,
    vm_offset_t pa,
    boolean_t super,
    boolean_t wt,
    boolean_t global,
    boolean_t ci,
    boolean_t wp,
    boolean_t valid)
{
    register batc_template_t batctmp;

    if (i < 0 || i > (BATC_MAX - 1)) {
        panic("pmap_set_batc: illegal batc number\n");
        /* bad number */
        return;
    }

    batctmp.field.lba = va >> 19;
    batctmp.field.pba = pa >> 19;
    batctmp.field.sup = super;
    batctmp.field.wt = wt;
    batctmp.field.g = global;
    batctmp.field.ci = ci;
    batctmp.field.wp = wp;
    batctmp.field.v = valid;

    if (data) {
        pmap->d_batc[i].bits = batctmp.bits;
    } else {
        pmap->i_batc[i].bits = batctmp.bits;
    }
}

void use_batc(
    task_t task,	
    boolean_t data,         /* for data-cmmu ? */
    int i,                  /* batc number */
    vm_offset_t va,         /* virtual address */
    vm_offset_t pa,         /* physical address */
    boolean_t s,            /* for super-mode ? */
    boolean_t wt,           /* is writethrough */
    boolean_t g,            /* is global ? */
    boolean_t ci,           /* is cache inhibited ? */
    boolean_t wp,           /* is write-protected ? */
    boolean_t v)            /* is valid ? */
{
    pmap_t pmap;
    pmap = vm_map_pmap(task->map);
    pmap_set_batc(pmap, data, i, va, pa, s, wt, g, ci, wp, v);
}

#endif
#endif /* USING_BATC */
#if FUTURE_MAYBE
/*
 *	Machine-level page attributes
 *
 *	The only attribute that may be controlled right now is cacheability.
 *
 *	Obviously these attributes will be used in a sparse
 *	fashion, so we use a simple sorted list of address ranges
 *	which possess the attribute.
 */

/*
 *	Destroy an attribute list.
 */
void
pmap_destroy_ranges(pmap_range_t *ranges)
{
	pmap_range_t this, next;

	this = *ranges;
	while (this != 0) {
		next = this->next;
		pmap_range_free(this);
		this = next;
	      }
	*ranges = 0;
}

/*
 *	Lookup an address in a sorted range list.
 */
boolean_t
pmap_range_lookup(pmap_range_t *ranges, vm_offset_t address)
{
	pmap_range_t range;

	for (range = *ranges; range != 0; range = range->next) {
		if (address < range->start)
			return FALSE;
		if (address < range->end)
			return TRUE;
	}
	return FALSE;
}

/*
 *	Add a range to a list.
 *	The pmap must be locked.
 */
void
pmap_range_add(pmap_range_t *ranges, vm_offset_t start, vm_offset_t end)
{
	pmap_range_t range, *prev;

	/* look for the start address */

	for (prev = ranges; (range = *prev) != 0; prev = &range->next) {
		if (start < range->start)
			break;
		if (start <= range->end)
			goto start_overlaps;
	}

	/* start address is not present */

	if ((range == 0) || (end < range->start)) {
		/* no overlap; allocate a new range */

		range = pmap_range_alloc();
		range->start = start;
		range->end = end;
		range->next = *prev;
		*prev = range;
		return;
	}

	/* extend existing range forward to start */

	range->start = start;

    start_overlaps:
	assert((range->start <= start) && (start <= range->end));

	/* delete redundant ranges */

	while ((range->next != 0) && (range->next->start <= end)) {
		pmap_range_t old;

		old = range->next;
		range->next = old->next;
		range->end = old->end;
		pmap_range_free(old);
	}

	/* extend existing range backward to end */

	if (range->end < end)
		range->end = end;
}

/*
 *	Remove a range from a list.
 *	The pmap must be locked.
 */
void
pmap_range_remove(pmap_range_t *ranges, vm_offset_t start, vm_offset_t end)
{
	pmap_range_t range, *prev;

	/* look for start address */

	for (prev = ranges; (range = *prev) != 0; prev = &range->next) {
		if (start <= range->start)
			break;
		if (start < range->end) {
			if (end < range->end) {
				pmap_range_t new;

				/* split this range */

				new = pmap_range_alloc();
				new->next = range->next;
				new->start = end;
				new->end = range->end;

				range->next = new;
				range->end = start;
				return;
			}

			/* truncate this range */

			range->end = start;
		}
	}

	/* start address is not in the middle of a range */

	while ((range != 0) && (range->end <= end)) {
		*prev = range->next;
		pmap_range_free(range);
		range = *prev;
	}

	if ((range != 0) && (range->start < end))
		range->start = end;
}
#endif /* FUTURE_MAYBE */
