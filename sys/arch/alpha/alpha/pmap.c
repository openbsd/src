/*	$OpenBSD: pmap.c,v 1.6 1999/09/03 18:00:11 art Exp $	*/
/*	$NetBSD: pmap.c,v 1.17 1996/10/13 02:59:42 christos Exp $	*/

/*
 * Copyright (c) 1992, 1996 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */

/*
 *	File:	pmap.c
 *
 *	Author list
 *	vax:  Avadis Tevanian, Jr., Michael Wayne Young
 *	i386: Lance Berc, Mike Kupfer, Bob Baron, David Golub, Richard Draves
 *	alpha: Alessandro Forin
 *	{Net,Open}BSD/Alpha: Chris Demetriou
 *
 *	Physical Map management code for DEC Alpha
 *
 *	Manages physical address maps.
 *
 *	This code was derived exclusively from information available in
 *	"Alpha Architecture Reference Manual", Richard L. Sites ed.
 *	Digital Press, Burlington, MA 01803
 *	ISBN 1-55558-098-X, Order no. EY-L520E-DP
 */

/*
 *	In addition to hardware address maps, this
 *	module is called upon to provide software-use-only
 *	maps which may or may not be stored in the same
 *	form as hardware maps.  These pseudo-maps are
 *	used to store intermediate results from copy
 *	operations to and from address spaces.
 *
 *	Since the information managed by this module is
 *	also stored by the logical address mapping module,
 *	this module may throw away valid virtual-to-physical
 *	mappings at almost any time.  However, invalidations
 *	of virtual-to-physical mappings must be done as
 *	requested.
 *
 *	In order to cope with hardware architectures which
 *	make virtual-to-physical map invalidates expensive,
 *	this module may delay invalidate or reduced protection
 *	operations until such time as they are actually
 *	necessary.  This module is given full information as
 *	to which processors are currently using which maps,
 *	and to when physical maps must be made correct.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/user.h>
#include <sys/buf.h>
#ifdef SYSVSHM
#include <sys/shm.h>
#endif

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <vm/vm_pageout.h>

#include <machine/cpu.h>
#include <machine/alpha_cpu.h>


#define	VM_OBJECT_NULL	NULL
#define	VM_PAGE_NULL	NULL
#define	BYTE_SIZE	NBBY
#define	page_size	PAGE_SIZE
#define	ALPHA_PTE_GLOBAL ALPHA_PTE_ASM
#define	MACRO_BEGIN	do {
#define	MACRO_END	} while (0)
#define	K2SEG_BASE	ALPHA_K1SEG_BASE
#define	integer_t	long
#define	spl_t		int
#define	vm_page_fictitious_addr 0
#define	aligned_block_copy(src, dest, size) bcopy((void *)src, (void *)dest, size)
#define	db_printf	printf
#define	tbia		ALPHA_TBIA
#define	alphacache_Iflush alpha_pal_imb
#define cpu_number()	0
#define	check_simple_locks()
#define	K0SEG_TO_PHYS	ALPHA_K0SEG_TO_PHYS
#define	ISA_K0SEG(v)	(v >= ALPHA_K0SEG_BASE && v <= ALPHA_K0SEG_END)
#ifndef assert
#define	assert(x)
#endif

vm_offset_t	avail_start;	/* PA of first available physical page */
vm_offset_t	avail_end;	/* PA of last available physical page */
vm_offset_t	mem_size;	/* memory size in bytes */
vm_offset_t	virtual_avail;	/* VA of first avail page (after kernel bss)*/
vm_offset_t	virtual_end;	/* VA of last avail page (end of kernel AS) */

/* XXX */
struct pv_entry *pmap_alloc_pv __P((void));
void pmap_free_pv __P((struct pv_entry *pv));
vm_page_t vm_page_grab __P((void));

vm_offset_t pmap_resident_extract __P((pmap_t, vm_offset_t));

/* For external use... */
vm_offset_t kvtophys(vm_offset_t virt)
{

	return pmap_resident_extract(kernel_pmap, virt);
}

/* ..but for internal use... */
#define phystokv(a)	ALPHA_PHYS_TO_K0SEG(a)
#define	kvtophys(p)	ALPHA_K0SEG_TO_PHYS((vm_offset_t)p)


/*
 *	Private data structures.
 */
/*
 *	Map from MI protection codes to MD codes.
 *	Assume that there are three MI protection codes, all using low bits.
 */
pt_entry_t	user_protection_codes[8];
pt_entry_t	kernel_protection_codes[8];

alpha_protection_init()
{
	register pt_entry_t	*kp, *up, prot;

	kp = kernel_protection_codes;
	up = user_protection_codes;
	for (prot = 0; prot < 8; prot++) {
		switch (prot) {
		case VM_PROT_NONE | VM_PROT_NONE | VM_PROT_NONE:
			*kp++ = 0;
			*up++ = 0;
			break;
		case VM_PROT_READ | VM_PROT_NONE | VM_PROT_NONE:
		case VM_PROT_READ | VM_PROT_NONE | VM_PROT_EXECUTE:
		case VM_PROT_NONE | VM_PROT_NONE | VM_PROT_EXECUTE:
			*kp++ = ALPHA_PTE_KR;
			*up++ = ALPHA_PTE_UR|ALPHA_PTE_KR;
			break;
		case VM_PROT_NONE | VM_PROT_WRITE | VM_PROT_NONE:
			*kp++ = ALPHA_PTE_KW;
			*up++ = ALPHA_PTE_UW|ALPHA_PTE_KW;
			break;
		case VM_PROT_NONE | VM_PROT_WRITE | VM_PROT_EXECUTE:
		case VM_PROT_READ | VM_PROT_WRITE | VM_PROT_NONE:
		case VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE:
			*kp++ = ALPHA_PTE_KW|ALPHA_PTE_KR;
			*up++ = ALPHA_PTE_UW|ALPHA_PTE_UR|ALPHA_PTE_KW|ALPHA_PTE_KR;
			break;
		}
	}
}

/*
 *	Given a map and a machine independent protection code,
 *	convert to a alpha protection code.
 */

#define	alpha_protection(map, prot) \
	(((map) == kernel_pmap) ? kernel_protection_codes[prot] : \
				  user_protection_codes[prot])

/* Build the typical kernel pte */
#define	pte_ktemplate(t,pa,pr)						\
MACRO_BEGIN								\
	(t) = pa_to_pte(pa) | ALPHA_PTE_VALID | ALPHA_PTE_GLOBAL |	\
	  (alpha_protection(kernel_pmap,pr));				\
MACRO_END

/* build the typical pte */
#define	pte_template(m,t,pa,pr)						\
MACRO_BEGIN								\
	(t) = pa_to_pte(pa) | ALPHA_PTE_VALID |				\
	  (alpha_protection(m,pr));					\
MACRO_END

/*
 *	For each vm_page_t, there is a list of all currently
 *	valid virtual mappings of that page.  An entry is
 *	a pv_entry_t; the list is the pv_table.
 */

typedef struct pv_entry {
	struct pv_entry	*next;		/* next pv_entry */
	pmap_t		pmap;		/* pmap where mapping lies */
	vm_offset_t	va;		/* virtual address for mapping */
} *pv_entry_t;

#define PV_ENTRY_NULL	((pv_entry_t) 0)

pv_entry_t	pv_head_table;		/* array of entries, one per page */

/*
 *	pv_list entries are kept on a list that can only be accessed
 *	with the pmap system locked (at SPLVM, not in the cpus_active set).
 *	The list is refilled from the pv_list_zone if it becomes empty.
 */
pv_entry_t	pv_free_list;		/* free list at SPLVM */
decl_simple_lock_data(, pv_free_list_lock)

#define	PV_ALLOC(pv_e) { \
	simple_lock(&pv_free_list_lock); \
	if ((pv_e = pv_free_list) != 0) { \
	    pv_free_list = pv_e->next; \
	} \
	simple_unlock(&pv_free_list_lock); \
}

#define	PV_FREE(pv_e) { \
	simple_lock(&pv_free_list_lock); \
	pv_e->next = pv_free_list; \
	pv_free_list = pv_e; \
	simple_unlock(&pv_free_list_lock); \
}

#if 0
zone_t		pv_list_zone;		/* zone of pv_entry structures */
#endif

/*
 *	Each entry in the pv_head_table is locked by a bit in the
 *	pv_lock_table.  The lock bits are accessed by the physical
 *	address of the page they lock.
 */

char	*pv_lock_table;		/* pointer to array of bits */
#define pv_lock_table_size(n)	(((n)+BYTE_SIZE-1)/BYTE_SIZE)

/*
 *	First and last physical addresses that we maintain any information
 *	for.  Initialized to zero so that pmap operations done before
 *	pmap_init won't touch any non-existent structures.
 */
vm_offset_t	vm_first_phys = (vm_offset_t) 0;
vm_offset_t	vm_last_phys  = (vm_offset_t) 0;
boolean_t	pmap_initialized = FALSE;/* Has pmap_init completed? */

/*
 *	Index into pv_head table, its lock bits, and the modify/reference
 *	bits starting at vm_first_phys.
 */

#define pa_index(pa)	(atop(pa - vm_first_phys))

#define pai_to_pvh(pai)		(&pv_head_table[pai])
#define lock_pvh_pai(pai)	(bit_lock(pai, pv_lock_table))
#define unlock_pvh_pai(pai)	(bit_unlock(pai, pv_lock_table))

/*
 *	Array of physical page attributes for managed pages.
 *	One byte per physical page.
 */
char	*pmap_phys_attributes;

/*
 *	Physical page attributes.  Copy bits from PTE.
 */
#define	PHYS_MODIFIED	(ALPHA_PTE_MOD>>16)	/* page modified */
#define	PHYS_REFERENCED	(ALPHA_PTE_REF>>16)	/* page referenced */

#define	pte_get_attributes(p)	((*p & (ALPHA_PTE_MOD|ALPHA_PTE_REF)) >> 16)

/*
 *	Amount of virtual memory mapped by one
 *	page-directory entry.
 */
#define	PDE_MAPPED_SIZE		(pdetova(1))
#define	PDE2_MAPPED_SIZE	(pde2tova(1))
#define	PDE3_MAPPED_SIZE	(pde3tova(1))

/*
 *	We allocate page table pages directly from the VM system
 *	through this object.  It maps physical memory.
 */
vm_object_t	pmap_object = VM_OBJECT_NULL;

/*
 *	Locking and TLB invalidation
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

#if	NCPUS > 1
/*
 *	We raise the interrupt level to splvm, to block interprocessor
 *	interrupts during pmap operations.  We must take the CPU out of
 *	the cpus_active set while interrupts are blocked.
 */
#define SPLVM(spl)	{ \
	spl = splvm(); \
	i_bit_clear(cpu_number(), &cpus_active); \
}

#define SPLX(spl)	{ \
	i_bit_set(cpu_number(), &cpus_active); \
	splx(spl); \
}

/*
 *	Lock on pmap system
 */
lock_data_t	pmap_system_lock;

volatile boolean_t	cpu_update_needed[NCPUS];

#define PMAP_READ_LOCK(pmap, spl) { \
	SPLVM(spl); \
	lock_read(&pmap_system_lock); \
	simple_lock(&(pmap)->lock); \
}

#define PMAP_WRITE_LOCK(spl) { \
	SPLVM(spl); \
	lock_write(&pmap_system_lock); \
}

#define PMAP_READ_UNLOCK(pmap, spl) { \
	simple_unlock(&(pmap)->lock); \
	lock_read_done(&pmap_system_lock); \
	SPLX(spl); \
}

#define PMAP_WRITE_UNLOCK(spl) { \
	lock_write_done(&pmap_system_lock); \
	SPLX(spl); \
}

#define PMAP_WRITE_TO_READ_LOCK(pmap) { \
	simple_lock(&(pmap)->lock); \
	lock_write_to_read(&pmap_system_lock); \
}

#define LOCK_PVH(index)		(lock_pvh_pai(index))

#define UNLOCK_PVH(index)	(unlock_pvh_pai(index))

#define PMAP_UPDATE_TLBS(pmap, s, e) \
{ \
	cpu_set	cpu_mask = 1 << cpu_number(); \
	cpu_set	users; \
 \
	/* Since the pmap is locked, other updates are locked */ \
	/* out, and any pmap_activate has finished. */ \
 \
	/* find other cpus using the pmap */ \
	users = (pmap)->cpus_using & ~cpu_mask; \
	if (users) { \
	    /* signal them, and wait for them to finish */ \
	    /* using the pmap */ \
	    signal_cpus(users, (pmap), (s), (e)); \
	    while ((pmap)->cpus_using & cpus_active & ~cpu_mask) \
		continue; \
	} \
 \
	/* invalidate our own TLB if pmap is in use */ \
	if ((pmap)->cpus_using & cpu_mask) { \
	    INVALIDATE_TLB((s), (e)); \
	} \
}

#else	NCPUS > 1

#define SPLVM(spl)
#define SPLX(spl)

#define PMAP_READ_LOCK(pmap, spl)	SPLVM(spl)
#define PMAP_WRITE_LOCK(spl)		SPLVM(spl)
#define PMAP_READ_UNLOCK(pmap, spl)	SPLX(spl)
#define PMAP_WRITE_UNLOCK(spl)		SPLX(spl)
#define PMAP_WRITE_TO_READ_LOCK(pmap)

#define LOCK_PVH(index)
#define UNLOCK_PVH(index)

#if 0 /*fix bug later */
#define PMAP_UPDATE_TLBS(pmap, s, e) { \
	/* invalidate our own TLB if pmap is in use */ \
	if ((pmap)->cpus_using) { \
	    INVALIDATE_TLB((s), (e)); \
	} \
}
#else
#define PMAP_UPDATE_TLBS(pmap, s, e) { \
	    INVALIDATE_TLB((s), (e)); \
}
#endif

#endif	/* NCPUS > 1 */

#if 0
#define INVALIDATE_TLB(s, e) { \
	register vm_offset_t	v = s, ve = e; \
	while (v < ve) { \
	    tbis(v); v += ALPHA_PGBYTES; \
	} \
}
#else
#define INVALIDATE_TLB(s, e) { \
	tbia(); \
}
#endif


#if	NCPUS > 1

void pmap_update_interrupt();

/*
 *	Structures to keep track of pending TLB invalidations
 */

#define UPDATE_LIST_SIZE	4

struct pmap_update_item {
	pmap_t		pmap;		/* pmap to invalidate */
	vm_offset_t	start;		/* start address to invalidate */
	vm_offset_t	end;		/* end address to invalidate */
} ;

typedef	struct pmap_update_item	*pmap_update_item_t;

/*
 *	List of pmap updates.  If the list overflows,
 *	the last entry is changed to invalidate all.
 */
struct pmap_update_list {
	decl_simple_lock_data(,	lock)
	int			count;
	struct pmap_update_item	item[UPDATE_LIST_SIZE];
} ;
typedef	struct pmap_update_list	*pmap_update_list_t;

struct pmap_update_list	cpu_update_list[NCPUS];

#endif	/* NCPUS > 1 */

/*
 *	Other useful macros.
 */
#define current_pmap()		(vm_map_pmap(current_thread()->task->map))
#define pmap_in_use(pmap, cpu)	(((pmap)->cpus_using & (1 << (cpu))) != 0)

struct pmap	kernel_pmap_store;
pmap_t		kernel_pmap;

struct zone	*pmap_zone;		/* zone of pmap structures */

int		pmap_debug = 0;		/* flag for debugging prints */
int		ptes_per_vm_page;	/* number of hardware ptes needed
					   to map one VM page. */
unsigned int	inuse_ptepages_count = 0;	/* debugging */

extern char end;
/*
 * Page directory for kernel.
 */
pt_entry_t	*root_kpdes;

void pmap_remove_range();	/* forward */
#if	NCPUS > 1
void signal_cpus();		/* forward */
#endif	/* NCPUS > 1 */

int	pmap_max_asn;
void	pmap_expand __P((pmap_t, vm_offset_t));

/* XXX */
#define	PDB_BOOTSTRAP		0x00000001
#define	PDB_BOOTSTRAP_ALLOC	0x00000002
#define	PDB_UNMAP_PROM		0x00000004
#define	PDB_ACTIVATE		0x00000008
#define	PDB_DEACTIVATE		0x00000010
#define	PDB_TLBPID_INIT		0x00000020
#define	PDB_TLBPID_ASSIGN	0x00000040
#define	PDB_TLBPID_DESTROY	0x00000080
#define	PDB_ENTER		0x00000100
#define	PDB_CREATE		0x00000200
#define	PDB_PINIT		0x00000400
#define	PDB_EXPAND		0x00000800
#define	PDB_EXTRACT		0x00001000
#define	PDB_PTE			0x00002000
#define	PDB_RELEASE		0x00004000
#define	PDB_DESTROY		0x00008000
#define	PDB_COPY_PAGE		0x00010000
#define	PDB_ZERO_PAGE		0x00020000

#define	PDB_ANOMALOUS		0x20000000
#define	PDB_FOLLOW		0x40000000
#define PDB_VERBOSE		0x80000000

int pmapdebug = PDB_ANOMALOUS  |-1 /* -1 */;

#if defined(DEBUG) || 1
#define	DOPDB(x)	((pmapdebug & (x)) != 0)
#else
#define	DOPDB(x)	0
#endif
#define	DOVPDB(x)	(DOPDB(x) && DOPDB(PDB_VERBOSE))

/*
 *	Given an offset and a map, compute the address of the
 *	pte.  If the address is invalid with respect to the map
 *	then PT_ENTRY_NULL is returned (and the map may need to grow).
 *
 *	This is only used internally.
 */
#define	pmap_pde(pmap, addr) (&(pmap)->dirbase[pdenum(addr)])

pt_entry_t *pmap_pte(pmap, addr)
	register pmap_t		pmap;
	register vm_offset_t	addr;
{
	register pt_entry_t	*ptp, *ptep;
	register pt_entry_t	pte;

	if (DOPDB(PDB_FOLLOW|PDB_PTE))
		printf("pmap_pte(%p, 0x%lx)\n", pmap, addr);

	if (pmap->dirbase == 0) {
		if (DOVPDB(PDB_FOLLOW|PDB_PTE))
			printf("pmap_pte: dirbase == 0\n");
		ptep = PT_ENTRY_NULL;
		goto out;
	}

	/* seg1 */
	pte = *pmap_pde(pmap,addr);
	if ((pte & ALPHA_PTE_VALID) == 0) {
		if (DOVPDB(PDB_FOLLOW|PDB_PTE))
			printf("pmap_pte: l1 not valid\n");
		ptep = PT_ENTRY_NULL;
		goto out;
	}

	/* seg2 */
	ptp = (pt_entry_t *)ptetokv(pte);
	pte = ptp[pte2num(addr)];
	if ((pte & ALPHA_PTE_VALID) == 0) {
		if (DOVPDB(PDB_FOLLOW|PDB_PTE))
			printf("pmap_pte: l2 not valid\n");
		ptep = PT_ENTRY_NULL;
		goto out;
	}

	/* seg3 */
	ptp = (pt_entry_t *)ptetokv(pte);
	ptep = &ptp[pte3num(addr)];

out:
	if (DOPDB(PDB_FOLLOW|PDB_PTE))
		printf("pmap_pte: returns %p\n", ptep);
	return (ptep);
}

#define DEBUG_PTE_PAGE	1

extern	vm_offset_t	virtual_avail, virtual_end;
extern	vm_offset_t	avail_start, avail_end;

/*
 *	Bootstrap the system enough to run with virtual memory.
 *	Map the kernel's code and data, and allocate the system page table.
 *	Called with mapping OFF.  Page_size must already be set.
 *
 *	Parameters:
 *	avail_start	PA of first available physical page
 *	avail_end	PA of last available physical page
 *	virtual_avail	VA of first available page
 *	virtual_end	VA of last available page
 *
 */
vm_size_t	pmap_kernel_vm = 5;	/* each one 8 meg worth */

unsigned int
pmap_free_pages()
{
	return atop(avail_end - avail_start);
}

void
pmap_bootstrap(firstaddr, ptaddr, maxasn)
	vm_offset_t firstaddr, ptaddr;
	int maxasn;
{
	vm_offset_t	pa;
	pt_entry_t	template;
	pt_entry_t	*pde, *pte;
	vm_offset_t start;
        extern int firstusablepage, lastusablepage;
	int i;
	long npages;

        if (DOPDB(PDB_FOLLOW|PDB_BOOTSTRAP))
                printf("pmap_bootstrap(0x%lx, 0x%lx, %d)\n", firstaddr, ptaddr,
		    maxasn);

	/* must be page aligned */
	start = firstaddr = alpha_round_page(firstaddr);

#define valloc(name, type, num)						\
	    (name) = (type *)firstaddr;					\
	    firstaddr = ALIGN((vm_offset_t)((name)+(num)))
#define vallocsz(name, cast, size)					\
	    (name) = (cast)firstaddr;					\
	    firstaddr = ALIGN(firstaddr + size)

	/*
	 *	Initialize protection array.
	 */
	alpha_protection_init();

	/*
	 *	Set ptes_per_vm_page for general use.
	 */
	ptes_per_vm_page = page_size / ALPHA_PGBYTES;

	/*
	 *	The kernel's pmap is statically allocated so we don't
	 *	have to use pmap_create, which is unlikely to work
	 *	correctly at this part of the boot sequence.
	 */

	kernel_pmap = &kernel_pmap_store;

#if	NCPUS > 1
	lock_init(&pmap_system_lock, FALSE);	/* NOT a sleep lock */
#endif	/* NCPUS > 1 */

	simple_lock_init(&kernel_pmap->lock);

	kernel_pmap->ref_count = 1;

	/*
	 *	Allocate the kernel page directory, and put its
	 *	virtual address in root_kpdes.
	 *
	 *	No other physical memory has been allocated.
	 */

	vallocsz(root_kpdes, pt_entry_t *, PAGE_SIZE);
        if (DOVPDB(PDB_BOOTSTRAP))
                printf("pmap_bootstrap: root_kpdes = %p\n", root_kpdes);
	kernel_pmap->dirbase = root_kpdes;
	kernel_pmap->dirpfn = alpha_btop(kvtophys((vm_offset_t)root_kpdes));

        /* First, copy mappings for things below VM_MIN_KERNEL_ADDRESS */
        if (DOVPDB(PDB_BOOTSTRAP))
                printf("pmap_bootstrap: setting up root_kpdes (copy 0x%lx)\n",
		    pdenum(VM_MIN_KERNEL_ADDRESS) * sizeof root_kpdes[0]);
	bzero(root_kpdes, PAGE_SIZE);
        bcopy((caddr_t)ptaddr, root_kpdes,
            pdenum(VM_MIN_KERNEL_ADDRESS) * sizeof root_kpdes[0]);

	/*
	 *	Set up the virtual page table.
	 */
	pte_ktemplate(template, kvtophys(root_kpdes),
	    VM_PROT_READ | VM_PROT_WRITE);
	template &= ~ALPHA_PTE_GLOBAL;
	root_kpdes[pdenum(VPTBASE)] = template;
        if (DOVPDB(PDB_BOOTSTRAP))
                printf("pmap_bootstrap: VPT PTE 0x%lx at 0x%lx)\n",
		    root_kpdes[pdenum(VPTBASE)], &root_kpdes[pdenum(VPTBASE)]);

#if 0
	/*
	 *	Rid of console's default mappings
	 */
	for (pde = pmap_pde(kernel_pmap,0);
	     pde < pmap_pde(kernel_pmap,VM_MIN_KERNEL_ADDRESS);)
		*pde++ = 0;

#endif
	/*
	 *	Allocate the seg2 kernel page table entries from the front
	 *	of available physical memory.  Take enough to cover all of
	 *	the K2SEG range. But of course one page is enough for 8Gb,
	 *	and more in future chips ...
	 */
#define	enough_kseg2()	(PAGE_SIZE)

        if (DOVPDB(PDB_BOOTSTRAP))
                printf("pmap_bootstrap: allocating kvseg segment pages\n");
	vallocsz(pte, pt_entry_t *, enough_kseg2());		/* virtual */
	pa  = kvtophys(pte);					/* physical */
	bzero(pte, enough_kseg2());
        if (DOVPDB(PDB_BOOTSTRAP))
                printf("pmap_bootstrap: kvseg segment pages at %p\n", pte);

#undef	enough_kseg2

	/*
	 *	Make a note of it in the seg1 table
	 */

        if (DOVPDB(PDB_BOOTSTRAP))
                printf("pmap_bootstrap: inserting segment pages into root\n");
	tbia();
	pte_ktemplate(template,pa,VM_PROT_READ|VM_PROT_WRITE);
	pde = pmap_pde(kernel_pmap,K2SEG_BASE);
	i = ptes_per_vm_page;
	do {
	    *pde++ = template;
	    pte_increment_pa(template);
	    i--;
	} while (i > 0);

	/*
	 *	The kernel runs unmapped and cached (k0seg),
	 *	only dynamic data are mapped in k1seg.
	 *	==> No need to map it.
	 */

	/*
	 *	But don't we need some seg2 pagetables to start with ?
	 */
        if (DOVPDB(PDB_BOOTSTRAP))
                printf("pmap_bootstrap: allocating kvseg page table pages\n");
	pde = &pte[pte2num(K2SEG_BASE)];
	for (i = pmap_kernel_vm; i > 0; i--) {
	    register int j;

	    vallocsz(pte, pt_entry_t *, PAGE_SIZE);		/* virtual */
	    pa  = kvtophys(pte);				/* physical */
	    pte_ktemplate(template,pa,VM_PROT_READ|VM_PROT_WRITE);
	    bzero(pte, PAGE_SIZE);
	    j = ptes_per_vm_page;
	    do {
		*pde++ = template;
	        pte_increment_pa(template);
	    } while (--j > 0);
	}

	/*
	 *	Fix up managed physical memory information.
	 */
	avail_start = ALPHA_K0SEG_TO_PHYS(firstaddr);
	avail_end = alpha_ptob(lastusablepage + 1);
	mem_size = avail_end - avail_start;
	if (DOVPDB(PDB_BOOTSTRAP))
		printf("pmap_bootstrap: avail: 0x%lx -> 0x%lx (0x%lx)\n",
		    avail_start, avail_end, mem_size);

	/*
	 *	Allocate memory for the pv_head_table and its
	 *	lock bits, and the reference/modify byte array.
	 */
	if (DOVPDB(PDB_BOOTSTRAP))
		printf("pmap_bootstrap: allocating page management data\n");

	npages = ((BYTE_SIZE * mem_size) /
	          (BYTE_SIZE * (PAGE_SIZE + sizeof (struct pv_entry) + 1) + 1));

	valloc(pv_head_table, struct pv_entry, npages);
	bzero(pv_head_table, sizeof (struct pv_entry) * npages);

	valloc(pv_lock_table, char, pv_lock_table_size(npages));
	bzero(pv_lock_table, pv_lock_table_size(npages));

	valloc(pmap_phys_attributes, char, npages);
	bzero(pmap_phys_attributes, sizeof (char) * npages);

	avail_start = alpha_round_page(ALPHA_K0SEG_TO_PHYS(firstaddr));
	if (npages > pmap_free_pages())
		panic("pmap_bootstrap");
	mem_size = avail_end - avail_start;
	if (DOVPDB(PDB_BOOTSTRAP))
		printf("pmap_bootstrap: avail: 0x%lx -> 0x%lx (0x%lx)\n",
		    avail_start, avail_end, mem_size);

	/*
	 *	Assert kernel limits (because of pmap_expand).
	 */

	virtual_avail = alpha_round_page(K2SEG_BASE);
	virtual_end   = trunc_page(K2SEG_BASE + pde2tova(pmap_kernel_vm));
        if (DOVPDB(PDB_BOOTSTRAP)) {
		printf("pmap_bootstrap: virtual_avail = %p\n", virtual_avail);
		printf("pmap_bootstrap: virtual_end = %p\n", virtual_end);
	}

	/*
	 *	The distinguished tlbpid value of 0 is reserved for
	 *	the kernel pmap. Initialize the tlbpid allocator,
	 *	who knows about this.
	 */
	kernel_pmap->pid = 0;
	pmap_tlbpid_init(maxasn);

        if (DOVPDB(PDB_BOOTSTRAP))
                printf("pmap_bootstrap: leaving\n");
}

pmap_rid_of_console()
{
	pt_entry_t	*pde;
	/*
	 *	Rid of console's default mappings
	 */
	for (pde = pmap_pde(kernel_pmap,0L);
	     pde < pmap_pde(kernel_pmap,VM_MIN_KERNEL_ADDRESS);)
		*pde++ = 0;
}

/*
 * Bootstrap memory allocator. This function allows for early dynamic
 * memory allocation until the virtual memory system has been bootstrapped.
 * After that point, either kmem_alloc or malloc should be used. This
 * function works by stealing pages from the (to be) managed page pool,
 * implicitly mapping them (by using their k0seg addresses),
 * and zeroing them.
 *
 * It should be used from pmap_bootstrap till vm_page_startup, afterwards
 * it cannot be used, and will generate a panic if tried. Note that this
 * memory will never be freed, and in essence it is wired down.
 */

void *
pmap_bootstrap_alloc(size)
	int size;
{
	vm_offset_t val;
	extern boolean_t vm_page_startup_initialized;

	if (DOPDB(PDB_FOLLOW|PDB_BOOTSTRAP_ALLOC))
		printf("pmap_bootstrap_alloc(%lx)\n", size);
	if (vm_page_startup_initialized)
		panic("pmap_bootstrap_alloc: called after startup initialized");

	val = ALPHA_PHYS_TO_K0SEG(avail_start);
	size = alpha_round_page(size);
	avail_start += size;
	if (avail_start > avail_end)			/* sanity */
		panic("pmap_bootstrap_alloc");

	bzero((caddr_t)val, size);

	if (DOVPDB(PDB_BOOTSTRAP_ALLOC))
		printf("pmap_bootstrap_alloc: returns %p\n", val);
	return ((void *)val);
}

/*
 * Unmap the PROM mappings.  PROM mappings are kept around
 * by pmap_bootstrap, so we can still use the prom's printf.
 * Basically, blow away all mappings in the level one PTE
 * table below VM_MIN_KERNEL_ADDRESS.  The Virtual Page Table
 * Is at the end of virtual space, so it's safe.
 */
void
pmap_unmap_prom()
{
	int i;
	extern int prom_mapped;
	extern pt_entry_t *rom_ptep, rom_pte;

	if (DOPDB(PDB_FOLLOW|PDB_UNMAP_PROM))
		printf("pmap_unmap_prom\n");

	/* XXX save old pte so that we can remap prom if necessary */
	rom_ptep = &root_kpdes[0];				/* XXX */
	rom_pte = *rom_ptep & ~ALPHA_PTE_ASM;			/* XXX */

	if (DOVPDB(PDB_UNMAP_PROM))
		printf("pmap_unmap_prom: zero 0x%lx, rom_pte was 0x%lx\n",
		    pdenum(VM_MIN_KERNEL_ADDRESS) * sizeof root_kpdes[0],
		    rom_pte);
	/* Mark all mappings before VM_MIN_KERNEL_ADDRESS as invalid. */
	bzero(root_kpdes, pdenum(VM_MIN_KERNEL_ADDRESS) * sizeof root_kpdes[0]);
	prom_mapped = 0;
	ALPHA_TBIA();
	if (DOVPDB(PDB_UNMAP_PROM))
		printf("pmap_unmap_prom: leaving\n");
}

/*
 *	Initialize the pmap module.
 *	Called by vm_init, to initialize any structures that the pmap
 *	system needs to map virtual memory.
 */
void
pmap_init(phys_start, phys_end)
	vm_offset_t	phys_start, phys_end;
{
	vm_size_t	s;
	int		i;

	/*
	 *	Create the zone of physical maps,
	 *	and of the physical-to-virtual entries.
	 */
#if 0
	s = (vm_size_t) sizeof(struct pmap);
	pmap_zone = zinit(s, 400*s, 4096, FALSE, "pmap"); /* XXX */
	s = (vm_size_t) sizeof(struct pv_entry);
	pv_list_zone = zinit(s, 10000*s, 4096, FALSE, "pv_list"); /* XXX */
#endif

#if	NCPUS > 1
	/*
	 *	Set up the pmap request lists
	 */
	for (i = 0; i < NCPUS; i++) {
	    pmap_update_list_t	up = &cpu_update_list[i];

	    simple_lock_init(&up->lock);
	    up->count = 0;
	}

	alpha_set_scb_entry( SCB_INTERPROC, pmap_update_interrupt);

#endif	/* NCPUS > 1 */

	/*
	 *	Only now, when all of the data structures are allocated,
	 *	can we set vm_first_phys and vm_last_phys.  If we set them
	 *	too soon, the kmem_alloc_wired above will try to use these
	 *	data structures and blow up.
	 */

	vm_first_phys = phys_start;
	vm_last_phys = phys_end;
	pmap_initialized = TRUE;
}

#define pmap_valid_page(x) ((avail_start <= x) && (x < avail_end))
#define valid_page(x) (pmap_initialized && pmap_valid_page(x))

/*
 *	Routine:	pmap_page_table_page_alloc
 *
 *	Allocates a new physical page to be used as a page-table page.
 *
 *	Must be called with the pmap system and the pmap unlocked,
 *	since these must be unlocked to use vm_page_grab.
 */
vm_offset_t
pmap_page_table_page_alloc()
{
	register vm_page_t	m;
	register vm_offset_t	pa;

	check_simple_locks();

	/*
	 *	We cannot allocate the pmap_object in pmap_init,
	 *	because it is called before the zone package is up.
	 *	Allocate it now if it is missing.
	 */
	if (pmap_object == VM_OBJECT_NULL)
	    pmap_object = vm_object_allocate(mem_size);

	/*
	 *	Allocate a VM page
	 */
	while ((m = vm_page_grab()) == VM_PAGE_NULL)
		vm_page_wait();

	/*
	 *	Map the page to its physical address so that it
	 *	can be found later.
	 */
	pa = m->phys_addr;
	vm_object_lock(pmap_object);
	vm_page_insert(m, pmap_object, pa);
	vm_page_lock_queues();
	vm_page_wire(m);
	inuse_ptepages_count++;
	vm_page_unlock_queues();
	vm_object_unlock(pmap_object);

	/*
	 *	Zero the page.
	 */
	bzero((void *)phystokv(pa), PAGE_SIZE);

	return pa;
}

/*
 *	Deallocate a page-table page.
 *	The page-table page must have all mappings removed,
 *	and be removed from its page directory.
 */
void
pmap_page_table_page_dealloc(pa)
	vm_offset_t	pa;
{
	vm_page_t	m;

	vm_object_lock(pmap_object);
	m = vm_page_lookup(pmap_object, pa);
	if (m == VM_PAGE_NULL)
	    panic("pmap_page_table_page_dealloc: page %#X not in object", pa);
	vm_page_lock_queues();
	vm_page_free(m);
	inuse_ptepages_count--;
	vm_page_unlock_queues();
	vm_object_unlock(pmap_object);
}

/*
 *	Create and return a physical map.
 *
 *	If the size specified for the map
 *	is zero, the map is an actual physical
 *	map, and may be referenced by the
 *	hardware.
 *
 *	If the size specified is non-zero,
 *	the map will be used in software only, and
 *	is bounded by that size.
 */
pmap_t
pmap_create(size)
	vm_size_t size;
{
	register pmap_t p;

	if (DOPDB(PDB_FOLLOW|PDB_CREATE))
		printf("pmap_create(%d)\n", size);

	/*
	 *	A software use-only map doesn't even need a map.
	 */

	if (size != 0) {
		p = PMAP_NULL;
		goto out;
	}

	/* XXX: is it ok to wait here? */
	p = (pmap_t) malloc(sizeof *p, M_VMPMAP, M_WAITOK);
	if (p == NULL)
		panic("pmap_create: cannot allocate a pmap");

	bzero(p, sizeof (*p));
	pmap_pinit(p);

out:
	if (DOVPDB(PDB_FOLLOW|PDB_CREATE))
		printf("pmap_create: returning %p\n", p);
	return (p);
}

void
pmap_pinit(p)
	struct pmap *p;
{
	register pmap_statistics_t stats;
	extern struct vmspace vmspace0;

	if (DOPDB(PDB_FOLLOW|PDB_PINIT))
		printf("pmap_init(%p)\n", p);

#if 0
	/* XXX cgd WHY NOT pmap_page_table_page_alloc()? */
	p->dirbase = (void *)kmem_alloc(kernel_map, ALPHA_PGBYTES);
#else
	p->dirbase = (void *)phystokv(pmap_page_table_page_alloc());
#endif
	if (p->dirbase == NULL)
		panic("pmap_create");
	p->dirpfn = alpha_btop(pmap_resident_extract(kernel_pmap,
	    (vm_offset_t)p->dirbase));

	if (DOVPDB(PDB_FOLLOW|PDB_PINIT))
		printf("pmap_init(%p): dirbase = %p, dirpfn = 0x%x\n", p,
		    p->dirbase, p->dirpfn);
	aligned_block_copy(root_kpdes, p->dirbase, ALPHA_PGBYTES);
	p->ref_count = 1;
	p->pid = -1;
	if (DOVPDB(PDB_FOLLOW|PDB_PINIT))
		printf("pmap_init(%p): first pde = 0x%lx\n", p->dirbase[0]);

	{
		pt_entry_t template;

		pte_ktemplate(template, kvtophys(p->dirbase),
		    VM_PROT_READ | VM_PROT_WRITE);
		template &= ~ALPHA_PTE_GLOBAL;
		p->dirbase[pdenum(VPTBASE)] = template;
	}
printf("PMAP_PINIT: FIRST ENT = 0x%lx\n", p->dirbase[0]);

	simple_lock_init(&p->lock);
	p->cpus_using = 0;
	p->hacking = 0;

	/*
	 *	Initialize statistics.
	 */

	stats = &p->stats;
	stats->resident_count = 0;
	stats->wired_count = 0;

out:
	if (DOVPDB(PDB_FOLLOW|PDB_PINIT))
		printf("pmap_init: leaving\n", p);
}

/*
 *	Retire the given physical map from service.
 *	Should only be called if the map contains
 *	no valid mappings.
 */

void pmap_destroy(p)
	register pmap_t	p;
{
	register int		c;
	register spl_t		s;

	if (DOPDB(PDB_FOLLOW|PDB_DESTROY))
		printf("pmap_destroy(%p)\n", p);

	if (p == PMAP_NULL)
		goto out;

	SPLVM(s);
	simple_lock(&p->lock);
	c = --p->ref_count;
	simple_unlock(&p->lock);
	SPLX(s);

	if (c == 0) {
		pmap_release(p);
		free(p, M_VMPMAP);
	}
out:
	if (DOVPDB(PDB_FOLLOW|PDB_DESTROY))
		printf("pmap_destroy: leaving\n");
}

void
pmap_release(p)
	pmap_t p;
{
	register pt_entry_t	*pdep, *ptep, *eptep;
	register vm_offset_t	pa;

	if (DOPDB(PDB_FOLLOW|PDB_RELEASE))
		printf("pmap_release(%p)\n", p);

	if (p->dirbase == NULL) {
		if (DOPDB(PDB_FOLLOW|PDB_ANOMALOUS|PDB_RELEASE))
			printf("pmap_release: already reclaimed\n");
		/* resources already reclaimed */
		goto out;
	}

	/*
	 *	Free the memory maps, then the
	 *	pmap structure.
	 */
	for (pdep = p->dirbase;
	     pdep < pmap_pde(p,VM_MIN_KERNEL_ADDRESS);
	     pdep += ptes_per_vm_page) {
	    if (*pdep & ALPHA_PTE_VALID) {
		pa = pte_to_pa(*pdep);

		ptep = (pt_entry_t *)phystokv(pa);
		eptep = ptep + NPTES;
		for (; ptep < eptep; ptep += ptes_per_vm_page ) {
		    if (*ptep & ALPHA_PTE_VALID)
			pmap_page_table_page_dealloc(pte_to_pa(*ptep));
		}
		pmap_page_table_page_dealloc(pa);
	    }
	}
	pmap_tlbpid_destroy(p->pid, FALSE);

#if 0
	kmem_free(kernel_map, (vm_offset_t)p->dirbase, ALPHA_PGBYTES);
#else
	pmap_page_table_page_dealloc(kvtophys(p->dirbase));
#endif
	p->dirbase = NULL;

out:
	if (DOVPDB(PDB_FOLLOW|PDB_RELEASE))
		printf("pmap_release: leaving\n");
}

/*
 *	Add a reference to the specified pmap.
 */

void pmap_reference(p)
	register pmap_t	p;
{
	spl_t	s;
	if (p != PMAP_NULL) {
		SPLVM(s);
		simple_lock(&p->lock);
		p->ref_count++;
		simple_unlock(&p->lock);
		SPLX(s);
	}
}

/*
 *	Remove a range of hardware page-table entries.
 *	The entries given are the first (inclusive)
 *	and last (exclusive) entries for the VM pages.
 *	The virtual address is the va for the first pte.
 *
 *	The pmap must be locked.
 *	If the pmap is not the kernel pmap, the range must lie
 *	entirely within one pte-page.  This is NOT checked.
 *	Assumes that the pte-page exists.
 */

/* static */
void pmap_remove_range(pmap, va, spte, epte)
	pmap_t			pmap;
	vm_offset_t		va;
	pt_entry_t		*spte;
	pt_entry_t		*epte;
{
	register pt_entry_t	*cpte;
	int			num_removed, num_unwired;
	int			pai;
	vm_offset_t		pa;

	num_removed = 0;
	num_unwired = 0;

	for (cpte = spte; cpte < epte;
	     cpte += ptes_per_vm_page, va += PAGE_SIZE) {

	    if (*cpte == 0)
		continue;
	    pa = pte_to_pa(*cpte);

	    num_removed++;
	    if (*cpte & ALPHA_PTE_WIRED)
		num_unwired++;

	    if (!valid_page(pa)) {

		/*
		 *	Outside range of managed physical memory.
		 *	Just remove the mappings.
		 */
		register int	i = ptes_per_vm_page;
		register pt_entry_t	*lpte = cpte;
		do {
		    *lpte = 0;
		    lpte++;
		} while (--i > 0);
		continue;
	    }

	    pai = pa_index(pa);
	    LOCK_PVH(pai);

	    /*
	     *	Get the modify and reference bits.
	     */
	    {
		register int		i;
		register pt_entry_t	*lpte;

		i = ptes_per_vm_page;
		lpte = cpte;
		do {
		    pmap_phys_attributes[pai] |= pte_get_attributes(lpte);
		    *lpte = 0;
		    lpte++;
		} while (--i > 0);
	    }

	    /*
	     *	Remove the mapping from the pvlist for
	     *	this physical page.
	     */
	    {
		register pv_entry_t	pv_h, prev, cur;

		pv_h = pai_to_pvh(pai);
		if (pv_h->pmap == PMAP_NULL) {
		    panic("pmap_remove: null pv_list!");
		}
		if (pv_h->va == va && pv_h->pmap == pmap) {
		    /*
		     * Header is the pv_entry.  Copy the next one
		     * to header and free the next one (we cannot
		     * free the header)
		     */
		    cur = pv_h->next;
		    if (cur != PV_ENTRY_NULL) {
			*pv_h = *cur;
			PV_FREE(cur);
		    }
		    else {
			pv_h->pmap = PMAP_NULL;
		    }
		}
		else {
		    cur = pv_h;
		    do {
			prev = cur;
			if ((cur = prev->next) == PV_ENTRY_NULL) {
			    panic("pmap-remove: mapping not in pv_list!");
			}
		    } while (cur->va != va || cur->pmap != pmap);
		    prev->next = cur->next;
		    PV_FREE(cur);
		}
		UNLOCK_PVH(pai);
	    }
	}

	/*
	 *	Update the counts
	 */
	pmap->stats.resident_count -= num_removed;
	pmap->stats.wired_count -= num_unwired;
}

/*
 *	One level up, iterate an operation on the
 *	virtual range va..eva, mapped by the 1st
 *	level pte spte.
 */

/* static */
void pmap_iterate_lev2(pmap, s, e, spte, operation)
	pmap_t			pmap;
	vm_offset_t		s, e;
	pt_entry_t		*spte;
	void			(*operation)();
{
	vm_offset_t		l;
	pt_entry_t		*epte;
	pt_entry_t		*cpte;

if (pmap_debug > 1) db_printf("iterate2(%x,%x,%x)", s, e, spte);
	while (s <  e) {
	    /* at most 1 << 23 virtuals per iteration */
	    l = roundup(s+1,PDE2_MAPPED_SIZE);
	    if (l > e)
	    	l = e;
	    if (*spte & ALPHA_PTE_VALID) {
		register int	n;
		cpte = (pt_entry_t *) ptetokv(*spte);
		n = pte3num(l);
		if (n == 0) n = SEG_MASK + 1;/* l == next segment up */
		epte = &cpte[n];
		cpte = &cpte[pte3num(s)];
		assert(epte >= cpte);
if (pmap_debug > 1) db_printf(" [%x %x, %x %x]", s, l, cpte, epte);
		operation(pmap, s, cpte, epte);
	    }
	    s = l;
	    spte++;
	}
if (pmap_debug > 1) db_printf("\n");
}

void
pmap_make_readonly(pmap, va, spte, epte)
	pmap_t			pmap;
	vm_offset_t		va;
	pt_entry_t		*spte;
	pt_entry_t		*epte;
{
	while (spte < epte) {
	    if (*spte & ALPHA_PTE_VALID)
		*spte &= ~ALPHA_PTE_WRITE;
	    spte++;
	}
}

/*
 *	Remove the given range of addresses
 *	from the specified map.
 *
 *	It is assumed that the start and end are properly
 *	rounded to the hardware page size.
 */
vm_offset_t pmap_suspect_vs, pmap_suspect_ve;


void pmap_remove(map, s, e)
	pmap_t		map;
	vm_offset_t	s, e;
{
	spl_t			spl;
	register pt_entry_t	*pde;
	register pt_entry_t	*spte;
	vm_offset_t		l;

	if (map == PMAP_NULL)
		return;

if (pmap_debug || ((s > pmap_suspect_vs) && (s < pmap_suspect_ve))) 
db_printf("[%d]pmap_remove(%x,%x,%x)\n", cpu_number(), map, s, e);
	PMAP_READ_LOCK(map, spl);

	/*
	 *	Invalidate the translation buffer first
	 */
	PMAP_UPDATE_TLBS(map, s, e);

	pde = pmap_pde(map, s);
	while (s < e) {
	    /* at most (1 << 33) virtuals per iteration */
	    l = roundup(s+1, PDE_MAPPED_SIZE);
	    if (l > e)
		l = e;
	    if (*pde & ALPHA_PTE_VALID) {
		spte = (pt_entry_t *)ptetokv(*pde);
		spte = &spte[pte2num(s)];
		pmap_iterate_lev2(map, s, l, spte, pmap_remove_range);
	    }
	    s = l;
	    pde++;
	}

	PMAP_READ_UNLOCK(map, spl);
}

/*
 *	Routine:	pmap_page_protect
 *
 *	Function:
 *		Lower the permission for all mappings to a given
 *		page.
 */
vm_offset_t pmap_suspect_phys;

void pmap_page_protect(phys, prot)
	vm_offset_t	phys;
	vm_prot_t	prot;
{
	pv_entry_t		pv_h, prev;
	register pv_entry_t	pv_e;
	register pt_entry_t	*pte;
	int			pai;
	register pmap_t		pmap;
	spl_t			spl;
	boolean_t		remove;

if (pmap_debug || (phys == pmap_suspect_phys)) db_printf("pmap_page_protect(%x,%x)\n", phys, prot);

	assert(phys != vm_page_fictitious_addr);
	if (!valid_page(phys)) {
	    /*
	     *	Not a managed page.
	     */
	    return;
	}

	/*
	 * Determine the new protection.
	 */
	switch (prot) {
	    case VM_PROT_READ:
	    case VM_PROT_READ|VM_PROT_EXECUTE:
		remove = FALSE;
		break;
	    case VM_PROT_ALL:
		return;	/* nothing to do */
	    default:
		remove = TRUE;
		break;
	}

	/*
	 *	Lock the pmap system first, since we will be changing
	 *	several pmaps.
	 */

	PMAP_WRITE_LOCK(spl);

	pai = pa_index(phys);
	pv_h = pai_to_pvh(pai);

	/*
	 * Walk down PV list, changing or removing all mappings.
	 * We do not have to lock the pv_list because we have
	 * the entire pmap system locked.
	 */
	if (pv_h->pmap != PMAP_NULL) {

	    prev = pv_e = pv_h;
	    do {
		pmap = pv_e->pmap;
		/*
		 * Lock the pmap to block pmap_extract and similar routines.
		 */
		simple_lock(&pmap->lock);

		{
		    register vm_offset_t va;

		    va = pv_e->va;
		    pte = pmap_pte(pmap, va);

		    /*
		     * Consistency checks.
		     */
		    /* assert(*pte & ALPHA_PTE_VALID); XXX */
		    /* assert(pte_to_phys(*pte) == phys); */

		    /*
		     * Invalidate TLBs for all CPUs using this mapping.
		     */
		    PMAP_UPDATE_TLBS(pmap, va, va + PAGE_SIZE);
		}

		/*
		 * Remove the mapping if new protection is NONE
		 * or if write-protecting a kernel mapping.
		 */
		if (remove || pmap == kernel_pmap) {
		    /*
		     * Remove the mapping, collecting any modify bits.
		     */
		    if (*pte & ALPHA_PTE_WIRED)
			panic("pmap_remove_all removing a wired page");

		    {
			register int	i = ptes_per_vm_page;

			do {
			    pmap_phys_attributes[pai] |= pte_get_attributes(pte);
			    *pte++ = 0;
			} while (--i > 0);
		    }

		    pmap->stats.resident_count--;

		    /*
		     * Remove the pv_entry.
		     */
		    if (pv_e == pv_h) {
			/*
			 * Fix up head later.
			 */
			pv_h->pmap = PMAP_NULL;
		    }
		    else {
			/*
			 * Delete this entry.
			 */
			prev->next = pv_e->next;
			PV_FREE(pv_e);
		    }
		}
		else {
		    /*
		     * Write-protect.
		     */
		    register int i = ptes_per_vm_page;

		    do {
			*pte &= ~ALPHA_PTE_WRITE;
			pte++;
		    } while (--i > 0);

		    /*
		     * Advance prev.
		     */
		    prev = pv_e;
		}

		simple_unlock(&pmap->lock);

	    } while ((pv_e = prev->next) != PV_ENTRY_NULL);

	    /*
	     * If pv_head mapping was removed, fix it up.
	     */
	    if (pv_h->pmap == PMAP_NULL) {
		pv_e = pv_h->next;
		if (pv_e != PV_ENTRY_NULL) {
		    *pv_h = *pv_e;
		    PV_FREE(pv_e);
		}
	    }
	}

	PMAP_WRITE_UNLOCK(spl);
}

/*
 *	Set the physical protection on the
 *	specified range of this map as requested.
 *	Will not increase permissions.
 */
void pmap_protect(map, s, e, prot)
	pmap_t		map;
	vm_offset_t	s, e;
	vm_prot_t	prot;
{
	register pt_entry_t	*pde;
	register pt_entry_t	*spte, *epte;
	vm_offset_t		l;
	spl_t			spl;

	if (map == PMAP_NULL)
		return;

if (pmap_debug || ((s > pmap_suspect_vs) && (s < pmap_suspect_ve))) 
db_printf("[%d]pmap_protect(%x,%x,%x,%x)\n", cpu_number(), map, s, e, prot);
	/*
	 * Determine the new protection.
	 */
	switch (prot) {
	    case VM_PROT_READ|VM_PROT_EXECUTE:
		alphacache_Iflush();
	    case VM_PROT_READ:
		break;
	    case VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE:
		alphacache_Iflush();
	    case VM_PROT_READ|VM_PROT_WRITE:
		return;	/* nothing to do */
	    default:
		pmap_remove(map, s, e);
		return;
	}

	SPLVM(spl);
	simple_lock(&map->lock);

	/*
	 *	Invalidate the translation buffer first
	 */
	PMAP_UPDATE_TLBS(map, s, e);

	pde = pmap_pde(map, s);
	while (s < e) {
	    /* at most (1 << 33) virtuals per iteration */
	    l = roundup(s+1, PDE_MAPPED_SIZE);
	    if (l > e)
		l = e;
	    if (*pde & ALPHA_PTE_VALID) {
		spte = (pt_entry_t *)ptetokv(*pde);
		spte = &spte[pte2num(s)];
		pmap_iterate_lev2(map, s, l, spte, pmap_make_readonly);
	    }
	    s = l;
	    pde++;
	}

	simple_unlock(&map->lock);
	SPLX(spl);
}

/*
 *	Insert the given physical page (p) at
 *	the specified virtual address (v) in the
 *	target physical map with the protection requested.
 *
 *	If specified, the page will be wired down, meaning
 *	that the related pte can not be reclaimed.
 *
 *	NB:  This is the only routine which MAY NOT lazy-evaluate
 *	or lose information.  That is, this routine must actually
 *	insert this page into the given map NOW.
 */
void
pmap_enter(pmap, v, pa, prot, wired, access_type)
	register pmap_t		pmap;
	vm_offset_t		v;
	register vm_offset_t	pa;
	vm_prot_t		prot;
	boolean_t		wired;
	vm_prot_t		access_type;
{
	register pt_entry_t	*pte;
	register pv_entry_t	pv_h;
	register int		i, pai;
	pv_entry_t		pv_e;
	pt_entry_t		template;
	spl_t			spl;
	vm_offset_t		old_pa;

	if (DOPDB(PDB_FOLLOW|PDB_ENTER))
		printf("pmap_enter(%p, 0x%lx, 0x%lx, 0x%x, %d)\n",
		    pmap, v, pa, prot, wired);

	assert(pa != vm_page_fictitious_addr);
if (pmap_debug || ((v > pmap_suspect_vs) && (v < pmap_suspect_ve))) 
db_printf("[%d]pmap_enter(%x(%d), %x, %x, %x, %x)\n", cpu_number(), pmap, pmap->pid, v, pa, prot, wired);
	if (pmap == PMAP_NULL)
		goto out;
	assert(!pmap_max_asn || pmap->pid >= 0);

	/*
	 *	Must allocate a new pvlist entry while we're unlocked;
	 *	zalloc may cause pageout (which will lock the pmap system).
	 *	If we determine we need a pvlist entry, we will unlock
	 *	and allocate one.  Then we will retry, throwing away
	 *	the allocated entry later (if we no longer need it).
	 */
	pv_e = PV_ENTRY_NULL;
Retry:
	PMAP_READ_LOCK(pmap, spl);

	/*
	 *	Expand pmap to include this pte.  Assume that
	 *	pmap is always expanded to include enough hardware
	 *	pages to map one VM page.
	 */

	while ((pte = pmap_pte(pmap, v)) == PT_ENTRY_NULL) {
		/*
		 *	Must unlock to expand the pmap.
		 */
		PMAP_READ_UNLOCK(pmap, spl);

		pmap_expand(pmap, v);

		PMAP_READ_LOCK(pmap, spl);
	}

	/*
	 *	Special case if the physical page is already mapped
	 *	at this address.
	 */
	old_pa = pte_to_pa(*pte);
	if (*pte && old_pa == pa) {
	    /*
	     *	May be changing its wired attribute or protection
	     */
		
	    if (DOVPDB(PDB_FOLLOW|PDB_ENTER))
		printf("pmap_enter: same PA already mapped there (0x%lx)\n",
		    *pte);

	    if (wired && !(*pte & ALPHA_PTE_WIRED))
		pmap->stats.wired_count++;
	    else if (!wired && (*pte & ALPHA_PTE_WIRED))
		pmap->stats.wired_count--;

	    pte_template(pmap,template,pa,prot);
	    if (pmap == kernel_pmap)
		template |= ALPHA_PTE_GLOBAL;
	    if (wired)
		template |= ALPHA_PTE_WIRED;
	    PMAP_UPDATE_TLBS(pmap, v, v + PAGE_SIZE);
	    i = ptes_per_vm_page;
	    do {
		template |= (*pte & ALPHA_PTE_MOD);
		*pte = template;
		pte++;
		pte_increment_pa(template);
	    } while (--i > 0);
	}
	else {

	    /*
	     *	Remove old mapping from the PV list if necessary.
	     */
	    if (*pte) {
		if (DOVPDB(PDB_FOLLOW|PDB_ENTER))
			printf("pmap_enter: removing old PTE (0x%lx)\n", *pte);

		/*
		 *	Invalidate the translation buffer,
		 *	then remove the mapping.
		 */
		PMAP_UPDATE_TLBS(pmap, v, v + PAGE_SIZE);

		/*
		 *	Don't free the pte page if removing last
		 *	mapping - we will immediately replace it.
		 */
		pmap_remove_range(pmap, v, pte,
				  pte + ptes_per_vm_page);
	    }

	    if (valid_page(pa)) {
		if (DOVPDB(PDB_FOLLOW|PDB_ENTER))
			printf("pmap_enter: valid page\n");

		/*
		 *	Enter the mapping in the PV list for this
		 *	physical page.
		 */

		pai = pa_index(pa);
		LOCK_PVH(pai);
		pv_h = pai_to_pvh(pai);

		if (pv_h->pmap == PMAP_NULL) {
		    /*
		     *	No mappings yet
		     */
		    if (DOVPDB(PDB_FOLLOW|PDB_ENTER))
			printf("pmap_enter: first mapping\n");
		    pv_h->va = v;
		    pv_h->pmap = pmap;
		    pv_h->next = PV_ENTRY_NULL;
		    if (prot & VM_PROT_EXECUTE)
			alphacache_Iflush();
		}
		else {
		    if (DOVPDB(PDB_FOLLOW|PDB_ENTER))
			printf("pmap_enter: second+ mapping\n");

#if	DEBUG
		    {
			/* check that this mapping is not already there */
			pv_entry_t	e = pv_h;
			while (e != PV_ENTRY_NULL) {
			    if (e->pmap == pmap && e->va == v)
				panic("pmap_enter: already in pv_list");
			    e = e->next;
			}
		    }
#endif	/* DEBUG */
		    
		    /*
		     *	Add new pv_entry after header.
		     */
		    if (pv_e == PV_ENTRY_NULL) {
			pv_e = pmap_alloc_pv();
#if 0
			PV_ALLOC(pv_e);
			if (pv_e == PV_ENTRY_NULL) {
			    UNLOCK_PVH(pai);
			    PMAP_READ_UNLOCK(pmap, spl);

			    /*
			     * Refill from zone.
			     */
			    pv_e = (pv_entry_t) zalloc(pv_list_zone);
			    goto Retry;
			}
#endif
		    }
		    pv_e->va = v;
		    pv_e->pmap = pmap;
		    pv_e->next = pv_h->next;
		    pv_h->next = pv_e;
		    /*
		     *	Remember that we used the pvlist entry.
		     */
		    pv_e = PV_ENTRY_NULL;
		}
		UNLOCK_PVH(pai);
	    }

	    /*
	     *	And count the mapping.
	     */

	    pmap->stats.resident_count++;
	    if (wired)
		pmap->stats.wired_count++;

	    /*
	     *	Build a template to speed up entering -
	     *	only the pfn changes.
	     */
	    pte_template(pmap,template,pa,prot);
	    if (pmap == kernel_pmap)
		template |= ALPHA_PTE_GLOBAL;
	    if (wired)
		template |= ALPHA_PTE_WIRED;
	    i = ptes_per_vm_page;
	    do {
		if (DOVPDB(PDB_FOLLOW|PDB_ENTER))
			printf("pmap_enter: entering PTE 0x%lx at %p\n",
			    template, pte);
		*pte = template;
		pte++;
		pte_increment_pa(template);
	    } while (--i > 0);
	    ALPHA_TBIA();
	}

	if (pv_e != PV_ENTRY_NULL) {
	    PV_FREE(pv_e);
	}

	PMAP_READ_UNLOCK(pmap, spl);
out:
	if (DOVPDB(PDB_FOLLOW|PDB_ENTER))
		printf("pmap_enter: done\n");
}

/*
 *	Routine:	pmap_change_wiring
 *	Function:	Change the wiring attribute for a map/virtual-address
 *			pair.
 *	In/out conditions:
 *			The mapping must already exist in the pmap.
 */
void pmap_change_wiring(map, v, wired)
	register pmap_t	map;
	vm_offset_t	v;
	boolean_t	wired;
{
	register pt_entry_t	*pte;
	register int		i;
	spl_t			spl;

if (pmap_debug) db_printf("pmap_change_wiring(%x,%x,%x)\n", map, v, wired);
	/*
	 *	We must grab the pmap system lock because we may
	 *	change a pte_page queue.
	 */
	PMAP_READ_LOCK(map, spl);

	if ((pte = pmap_pte(map, v)) == PT_ENTRY_NULL)
		panic("pmap_change_wiring: pte missing");

	if (wired && !(*pte & ALPHA_PTE_WIRED)) {
	    /*
	     *	wiring down mapping
	     */
	    map->stats.wired_count++;
	    i = ptes_per_vm_page;
	    do {
		*pte++ |= ALPHA_PTE_WIRED;
	    } while (--i > 0);
	}
	else if (!wired && (*pte & ALPHA_PTE_WIRED)) {
	    /*
	     *	unwiring mapping
	     */
	    map->stats.wired_count--;
	    i = ptes_per_vm_page;
	    do {
		*pte &= ~ALPHA_PTE_WIRED;
	    } while (--i > 0);
	}

	PMAP_READ_UNLOCK(map, spl);
}

/*
 *	Routine:	pmap_extract
 *	Function:
 *		Extract the physical page address associated
 *		with the given map/virtual_address pair.
 */

vm_offset_t
pmap_extract(pmap, va)
	register pmap_t	pmap;
	vm_offset_t	va;
{
	register pt_entry_t	*pte;
	register vm_offset_t	pa;
	spl_t			spl;

	if (DOPDB(PDB_FOLLOW|PDB_EXTRACT))
		printf("pmap_extract(%p, 0x%lx)\n", pmap, va);

	/*
	 *	Special translation for kernel addresses in
	 *	K0 space (directly mapped to physical addresses).
	 */
	if (ISA_K0SEG(va)) {
		pa = K0SEG_TO_PHYS(va);
		if (DOPDB(PDB_FOLLOW|PDB_EXTRACT))
			printf("pmap_extract: returns 0x%lx\n", pa);
		goto out;
	}

	SPLVM(spl);
	simple_lock(&pmap->lock);
	if ((pte = pmap_pte(pmap, va)) == PT_ENTRY_NULL)
	    pa = (vm_offset_t) 0;
	else if (!(*pte & ALPHA_PTE_VALID))
	    pa = (vm_offset_t) 0;
	else
	    pa = pte_to_pa(*pte) + (va & ALPHA_OFFMASK);
	simple_unlock(&pmap->lock);

	/*
	 * Beware: this puts back this thread in the cpus_active set
	 */
	SPLX(spl);

out:
	if (DOPDB(PDB_FOLLOW|PDB_EXTRACT))
		printf("pmap_extract: returns 0x%lx\n", pa);
	return(pa);
}

vm_offset_t
pmap_resident_extract(pmap, va)
	register pmap_t	pmap;
	vm_offset_t	va;
{
	register pt_entry_t	*pte;
	register vm_offset_t	pa;

	/*
	 *	Special translation for kernel addresses in
	 *	K0 space (directly mapped to physical addresses).
	 */
	if (ISA_K0SEG(va)) {
		pa = K0SEG_TO_PHYS(va);
		goto out;
	}

	if ((pte = pmap_pte(pmap, va)) == PT_ENTRY_NULL)
	    pa = (vm_offset_t) 0;
	else if (!(*pte & ALPHA_PTE_VALID))
	    pa = (vm_offset_t) 0;
	else
	    pa = pte_to_pa(*pte) + (va & ALPHA_OFFMASK);

out:
	return(pa);
}

/*
 *	Routine:	pmap_expand
 *
 *	Expands a pmap to be able to map the specified virtual address.
 *
 *	Must be called with the pmap system and the pmap unlocked,
 *	since these must be unlocked to use vm_page_grab.
 *	Thus it must be called in a loop that checks whether the map
 *	has been expanded enough.
 */
void
pmap_expand(map, v)
	register pmap_t		map;
	register vm_offset_t	v;
{
	pt_entry_t		template;
	pt_entry_t		*pdp;
	register vm_page_t	m;
	register vm_offset_t	pa;
	register int		i;
	spl_t			spl;

	if (DOPDB(PDB_FOLLOW|PDB_EXPAND))
		printf("pmap_expand(%p, 0x%lx)\n", map, v);

	/* Would have to go through all maps to add this page */
	if (map == kernel_pmap)
		panic("pmap_expand");

	/*
	 *	Allocate a VM page for the level 2 page table entries,
	 *	if not already there.
	 */
	pdp = pmap_pde(map,v);
	if ((*pdp & ALPHA_PTE_VALID) == 0) {
		pt_entry_t	*pte;

		if (DOVPDB(PDB_FOLLOW|PDB_EXPAND))
			printf("pmap_expand: needs pde\n");

		pa = pmap_page_table_page_alloc();

		/*
		 * Re-lock the pmap and check that another thread has
		 * not already allocated the page-table page.  If it
		 * has, discard the new page-table page (and try
		 * again to make sure).
		 */
		PMAP_READ_LOCK(map, spl);

		if (*pdp & ALPHA_PTE_VALID) {
			/*
			 * Oops...
			 */
			PMAP_READ_UNLOCK(map, spl);
			pmap_page_table_page_dealloc(pa);
			return;
		}

		/*
		 * Map the page.
		 */
		i = ptes_per_vm_page;
		pte = pdp;
		pte_ktemplate(template,pa,VM_PROT_READ|VM_PROT_WRITE);
		if (map != kernel_pmap)
			template &= ~ALPHA_PTE_ASM;
		do {
			*pte = template;
			if (DOVPDB(PDB_FOLLOW|PDB_EXPAND))
				printf("pmap_expand: inserted l1 pte (0x%lx) at %p\n",
				   template, pte);
			pte++;
			pte_increment_pa(template);
		} while (--i > 0);
		PMAP_READ_UNLOCK(map, spl);
	}

	/*
	 *	Allocate a level 3 page table.
	 */

	pa = pmap_page_table_page_alloc();

	/*
	 * Re-lock the pmap and check that another thread has
	 * not already allocated the page-table page.  If it
	 * has, we are done.
	 */
	PMAP_READ_LOCK(map, spl);

	if (pmap_pte(map, v) != PT_ENTRY_NULL) {
		PMAP_READ_UNLOCK(map, spl);
		pmap_page_table_page_dealloc(pa);
		return;
	}

	/*
	 *	Set the page directory entry for this page table.
	 *	If we have allocated more than one hardware page,
	 *	set several page directory entries.
	 */
	i = ptes_per_vm_page;
	pdp = (pt_entry_t *)ptetokv(*pdp);
	pdp = &pdp[pte2num(v)];
	pte_ktemplate(template,pa,VM_PROT_READ|VM_PROT_WRITE);
	if (map != kernel_pmap)
		template &= ~ALPHA_PTE_ASM;
	do {
		*pdp = template;
		if (DOVPDB(PDB_FOLLOW|PDB_EXPAND))
			printf("pmap_expand: inserted l2 pte (0x%lx) at %p\n",
			  template, pdp);
		pdp++;
		pte_increment_pa(template);
	} while (--i > 0);
	PMAP_READ_UNLOCK(map, spl);

out:
	if (DOVPDB(PDB_FOLLOW|PDB_EXPAND))
		printf("pmap_expand: leaving\n");
	return;
}

/*
 *	Copy the range specified by src_addr/len
 *	from the source map to the range dst_addr/len
 *	in the destination map.
 *
 *	This routine is only advisory and need not do anything.
 */
#if	0
void pmap_copy(dst_pmap, src_pmap, dst_addr, len, src_addr)
	pmap_t		dst_pmap;
	pmap_t		src_pmap;
	vm_offset_t	dst_addr;
	vm_size_t	len;
	vm_offset_t	src_addr;
{
#ifdef	lint
	dst_pmap++; src_pmap++; dst_addr++; len++; src_addr++;
#endif	/* lint */
}
#endif

/*
 *	Routine:	pmap_collect
 *	Function:
 *		Garbage collects the physical map system for
 *		pages which are no longer used.
 *		Success need not be guaranteed -- that is, there
 *		may well be pages which are not referenced, but
 *		others may be collected.
 *	Usage:
 *		Called by the pageout daemon when pages are scarce.
 */
void pmap_collect(p)
	pmap_t 		p;
{
#if	notyet

	register pt_entry_t	*pdp, *ptp;
	pt_entry_t		*eptp;
	vm_offset_t		pa;
	spl_t			spl;
	int			wired;

	if (p == PMAP_NULL)
		return;

	if (p == kernel_pmap)
		return;

	/*
	 *	Garbage collect map.
	 */
	PMAP_READ_LOCK(p, spl);
	PMAP_UPDATE_TLBS(p, VM_MIN_ADDRESS, VM_MAX_ADDRESS);
	pmap_tlbpid_destroy(p->pid, FALSE);

	for (pdp = p->dirbase;
	     pdp < pmap_pde(p,VM_MIN_KERNEL_ADDRESS);
	     pdp += ptes_per_vm_page)
	{
	    if (*pdp & ALPHA_PTE_VALID) {

		pa = pte_to_pa(*pdp);
		ptp = (pt_entry_t *)phystokv(pa);
		eptp = ptp + NPTES*ptes_per_vm_page;

		/*
		 * If the pte page has any wired mappings, we cannot
		 * free it.
		 */
		wired = 0;
		{
		    register pt_entry_t *ptep;
		    for (ptep = ptp; ptep < eptp; ptep++) {
			if (*ptep & ALPHA_PTE_WIRED) {
			    wired = 1;
			    break;
			}
		    }
		}
		if (!wired) {
		    /*
		     * Remove the virtual addresses mapped by this pte page.
		     */
.....		    pmap_remove_range_2(p,
				pdetova(pdp - p->dirbase),
				ptp,
				eptp);

		    /*
		     * Invalidate the page directory pointer.
		     */
		    {
			register int i = ptes_per_vm_page;
			register pt_entry_t *pdep = pdp;
			do {
			    *pdep++ = 0;
			} while (--i > 0);
		    }

		    PMAP_READ_UNLOCK(p, spl);

		    /*
		     * And free the pte page itself.
		     */
		    {
			register vm_page_t m;

			vm_object_lock(pmap_object);
			m = vm_page_lookup(pmap_object, pa);
			if (m == VM_PAGE_NULL)
			    panic("pmap_collect: pte page not in object");
			vm_page_lock_queues();
			vm_page_free(m);
			inuse_ptepages_count--;
			vm_page_unlock_queues();
			vm_object_unlock(pmap_object);
		    }

		    PMAP_READ_LOCK(p, spl);
		}
	    }
	}
	PMAP_READ_UNLOCK(p, spl);
	return;
#endif
}

/*
 *	Routine:	pmap_activate
 *	Function:
 *		Binds the given physical map to the given
 *		processor, and returns a hardware map description.
 */
void
pmap_activate(pmap, hwpcb, cpu)
	register pmap_t	pmap;
	struct alpha_pcb *hwpcb;
	int cpu;
{

        if (DOPDB(PDB_FOLLOW|PDB_ACTIVATE))
                printf("pmap_activate(%p, %p, %d)\n", pmap, hwpcb, cpu);

#if 0
	PMAP_ACTIVATE(my_pmap, th, my_cpu);
#else
        if (DOVPDB(PDB_ACTIVATE))
                printf("pmap_activate: old pid = %d\n", pmap->pid);
        if (pmap->pid < 0) pmap_tlbpid_assign(pmap);
	hwpcb->apcb_asn = pmap->pid;
        hwpcb->apcb_ptbr = pmap->dirpfn;
	if (pmap != kernel_pmap)
		pmap->cpus_using = TRUE;
        if (DOVPDB(PDB_ACTIVATE))
                printf("pmap_activate: new pid = %d, new ptbr = 0x%lx\n",
		    pmap->pid, pmap->dirpfn);
#endif
}

/*
 *	Routine:	pmap_deactivate
 *	Function:
 *		Indicates that the given physical map is no longer
 *		in use on the specified processor.  (This is a macro
 *		in pmap.h)
 */
void
pmap_deactivate(pmap, hwpcb, cpu)
	register pmap_t	pmap;
	struct alpha_pcb *hwpcb;
	int cpu;
{
        if (DOPDB(PDB_FOLLOW|PDB_DEACTIVATE))
                printf("pmap_deactivate(%p, %p, %d)\n", pmap, hwpcb, cpu);

#if 0
	PMAP_DEACTIVATE(pmap, th, which_cpu);
#else
        if (DOVPDB(PDB_DEACTIVATE))
                printf("pmap_deactivate: pid = %d, ptbr = 0x%lx\n",
		    pmap->pid, pmap->dirpfn);
	pmap->cpus_using = FALSE;
#endif
}

/*
 *	Routine:	pmap_kernel
 *	Function:
 *		Returns the physical map handle for the kernel.
 */
#if	0
pmap_t pmap_kernel()
{
    	return (kernel_pmap);
}
#endif

/*
 *	pmap_zero_page zeros the specified (machine independent) page.
 *	See machine/phys.c or machine/phys.s for implementation.
 */
#if	1
void
pmap_zero_page(phys)
	register vm_offset_t	phys;
{

	if (DOPDB(PDB_FOLLOW|PDB_ZERO_PAGE))
		printf("pmap_zero_page(0x%lx)\n", phys);

	assert(phys != vm_page_fictitious_addr);

	bzero((void *)phystokv(phys), PAGE_SIZE);

	if (DOVPDB(PDB_FOLLOW|PDB_ZERO_PAGE))
		printf("pmap_zero_page: leaving\n");
}
#endif

/*
 *	pmap_copy_page copies the specified (machine independent) page.
 *	See machine/phys.c or machine/phys.s for implementation.
 */
#if 1	/* fornow */
void
pmap_copy_page(src, dst)
	vm_offset_t	src, dst;
{

	if (DOPDB(PDB_FOLLOW|PDB_COPY_PAGE))
		printf("pmap_copy_page(0x%lx, 0x%lx)\n", src, dst);

	assert(src != vm_page_fictitious_addr);
	assert(dst != vm_page_fictitious_addr);

	aligned_block_copy(phystokv(src), phystokv(dst), PAGE_SIZE);

	if (DOVPDB(PDB_FOLLOW|PDB_COPY_PAGE))
		printf("pmap_copy_page: leaving\n");
}
#endif

/*
 *	Routine:	pmap_pageable
 *	Function:
 *		Make the specified pages (by pmap, offset)
 *		pageable (or not) as requested.
 *
 *		A page which is not pageable may not take
 *		a fault; therefore, its page table entry
 *		must remain valid for the duration.
 *
 *		This routine is merely advisory; pmap_enter
 *		will specify that these pages are to be wired
 *		down (or not) as appropriate.
 */
void
pmap_pageable(pmap, start, end, pageable)
	pmap_t		pmap;
	vm_offset_t	start;
	vm_offset_t	end;
	boolean_t	pageable;
{
#ifdef	lint
	pmap++; start++; end++; pageable++;
#endif
}

/*
 *	Clear specified attribute bits.
 */
void
phys_attribute_clear(phys, bits)
	vm_offset_t	phys;
	int		bits;
{
	pv_entry_t		pv_h;
	register pv_entry_t	pv_e;
	register pt_entry_t	*pte;
	int			pai;
	register pmap_t		pmap;
	spl_t			spl;

	assert(phys != vm_page_fictitious_addr);
	if (!valid_page(phys)) {
	    /*
	     *	Not a managed page.
	     */
	    return;
	}

	/*
	 *	Lock the pmap system first, since we will be changing
	 *	several pmaps.
	 */

	PMAP_WRITE_LOCK(spl);

	pai = pa_index(phys);
	pv_h = pai_to_pvh(pai);

	/*
	 * Walk down PV list, clearing all modify or reference bits.
	 * We do not have to lock the pv_list because we have
	 * the entire pmap system locked.
	 */
	if (pv_h->pmap != PMAP_NULL) {
	    /*
	     * There are some mappings.
	     */
	    for (pv_e = pv_h; pv_e != PV_ENTRY_NULL; pv_e = pv_e->next) {

		pmap = pv_e->pmap;
		/*
		 * Lock the pmap to block pmap_extract and similar routines.
		 */
		simple_lock(&pmap->lock);

		{
		    register vm_offset_t va;

		    va = pv_e->va;
		    pte = pmap_pte(pmap, va);

#if	0
		    /*
		     * Consistency checks.
		     */
		    assert(*pte & ALPHA_PTE_VALID);
		    /* assert(pte_to_phys(*pte) == phys); */
#endif

		    /*
		     * Invalidate TLBs for all CPUs using this mapping.
		     */
		    PMAP_UPDATE_TLBS(pmap, va, va + PAGE_SIZE);
		}

		/*
		 * Clear modify or reference bits.
		 */
		{
		    register int	i = ptes_per_vm_page;
		    do {
			*pte &= ~bits;
		    } while (--i > 0);
		}
		simple_unlock(&pmap->lock);
	    }
	}

	pmap_phys_attributes[pai] &= ~ (bits >> 16);

	PMAP_WRITE_UNLOCK(spl);
}

/*
 *	Check specified attribute bits.
 */
boolean_t
phys_attribute_test(phys, bits)
	vm_offset_t	phys;
	int		bits;
{
	pv_entry_t		pv_h;
	register pv_entry_t	pv_e;
	register pt_entry_t	*pte;
	int			pai;
	register pmap_t		pmap;
	spl_t			spl;

	assert(phys != vm_page_fictitious_addr);
	if (!valid_page(phys)) {
	    /*
	     *	Not a managed page.
	     */
	    return (FALSE);
	}

	/*
	 *	Lock the pmap system first, since we will be checking
	 *	several pmaps.
	 */

	PMAP_WRITE_LOCK(spl);

	pai = pa_index(phys);
	pv_h = pai_to_pvh(pai);

	if (pmap_phys_attributes[pai] & (bits >> 16)) {
	    PMAP_WRITE_UNLOCK(spl);
	    return (TRUE);
	}

	/*
	 * Walk down PV list, checking all mappings.
	 * We do not have to lock the pv_list because we have
	 * the entire pmap system locked.
	 */
	if (pv_h->pmap != PMAP_NULL) {
	    /*
	     * There are some mappings.
	     */
	    for (pv_e = pv_h; pv_e != PV_ENTRY_NULL; pv_e = pv_e->next) {

		pmap = pv_e->pmap;
		/*
		 * Lock the pmap to block pmap_extract and similar routines.
		 */
		simple_lock(&pmap->lock);

		{
		    register vm_offset_t va;

		    va = pv_e->va;
		    pte = pmap_pte(pmap, va);

#if	0
		    /*
		     * Consistency checks.
		     */
		    assert(*pte & ALPHA_PTE_VALID);
		    /* assert(pte_to_phys(*pte) == phys); */
#endif
		}

		/*
		 * Check modify or reference bits.
		 */
		{
		    register int	i = ptes_per_vm_page;

		    do {
			if (*pte & bits) {
			    simple_unlock(&pmap->lock);
			    PMAP_WRITE_UNLOCK(spl);
			    return (TRUE);
			}
		    } while (--i > 0);
		}
		simple_unlock(&pmap->lock);
	    }
	}
	PMAP_WRITE_UNLOCK(spl);
	return (FALSE);
}

/*
 *	Set specified attribute bits.  <ugly>
 */
void
phys_attribute_set(phys, bits)
	vm_offset_t	phys;
	int		bits;
{
	int			pai;
	spl_t			spl;

	assert(phys != vm_page_fictitious_addr);
	if (!valid_page(phys)) {
	    /*
	     *	Not a managed page.
	     */
	    return;
	}

	/*
	 *	Lock the pmap system.
	 */

	PMAP_WRITE_LOCK(spl);

	pai = pa_index(phys);
	pmap_phys_attributes[pai]  |= (bits >> 16);

	PMAP_WRITE_UNLOCK(spl);
}

/*
 *	Clear the modify bits on the specified physical page.
 */

void pmap_clear_modify(phys)
	register vm_offset_t	phys;
{
if (pmap_debug) db_printf("pmap_clear_mod(%x)\n", phys);
	phys_attribute_clear(phys, ALPHA_PTE_MOD);
}

/*
 *	Set the modify bits on the specified physical page.
 */

void pmap_set_modify(phys)
	register vm_offset_t	phys;
{
if (pmap_debug) db_printf("pmap_set_mod(%x)\n", phys);
	phys_attribute_set(phys, ALPHA_PTE_MOD);
}

/*
 *	pmap_is_modified:
 *
 *	Return whether or not the specified physical page is modified
 *	by any physical maps.
 */

boolean_t pmap_is_modified(phys)
	register vm_offset_t	phys;
{
if (pmap_debug) db_printf("pmap_is_mod(%x)\n", phys);
	return (phys_attribute_test(phys, ALPHA_PTE_MOD));
}

/*
 *	pmap_clear_reference:
 *
 *	Clear the reference bit on the specified physical page.
 */

void pmap_clear_reference(phys)
	vm_offset_t	phys;
{
if (pmap_debug) db_printf("pmap_clear_ref(%x)\n", phys);
	phys_attribute_clear(phys, ALPHA_PTE_REF);
}

/*
 *	pmap_is_referenced:
 *
 *	Return whether or not the specified physical page is referenced
 *	by any physical maps.
 */

boolean_t pmap_is_referenced(phys)
	vm_offset_t	phys;
{
if (pmap_debug) db_printf("pmap_is_ref(%x)\n", phys);
	return (phys_attribute_test(phys, ALPHA_PTE_REF));
}

#if	NCPUS > 1
/*
*	    TLB Coherence Code (TLB "shootdown" code)
* 
* Threads that belong to the same task share the same address space and
* hence share a pmap.  However, they  may run on distinct cpus and thus
* have distinct TLBs that cache page table entries. In order to guarantee
* the TLBs are consistent, whenever a pmap is changed, all threads that
* are active in that pmap must have their TLB updated. To keep track of
* this information, the set of cpus that are currently using a pmap is
* maintained within each pmap structure (cpus_using). Pmap_activate() and
* pmap_deactivate add and remove, respectively, a cpu from this set.
* Since the TLBs are not addressable over the bus, each processor must
* flush its own TLB; a processor that needs to invalidate another TLB
* needs to interrupt the processor that owns that TLB to signal the
* update.
* 
* Whenever a pmap is updated, the lock on that pmap is locked, and all
* cpus using the pmap are signaled to invalidate. All threads that need
* to activate a pmap must wait for the lock to clear to await any updates
* in progress before using the pmap. They must ACQUIRE the lock to add
* their cpu to the cpus_using set. An implicit assumption made
* throughout the TLB code is that all kernel code that runs at or higher
* than splvm blocks out update interrupts, and that such code does not
* touch pageable pages.
* 
* A shootdown interrupt serves another function besides signaling a
* processor to invalidate. The interrupt routine (pmap_update_interrupt)
* waits for the both the pmap lock (and the kernel pmap lock) to clear,
* preventing user code from making implicit pmap updates while the
* sending processor is performing its update. (This could happen via a
* user data write reference that turns on the modify bit in the page
* table). It must wait for any kernel updates that may have started
* concurrently with a user pmap update because the IPC code
* changes mappings.
* Spinning on the VALUES of the locks is sufficient (rather than
* having to acquire the locks) because any updates that occur subsequent
* to finding the lock unlocked will be signaled via another interrupt.
* (This assumes the interrupt is cleared before the low level interrupt code 
* calls pmap_update_interrupt()). 
* 
* The signaling processor must wait for any implicit updates in progress
* to terminate before continuing with its update. Thus it must wait for an
* acknowledgement of the interrupt from each processor for which such
* references could be made. For maintaining this information, a set
* cpus_active is used. A cpu is in this set if and only if it can 
* use a pmap. When pmap_update_interrupt() is entered, a cpu is removed from
* this set; when all such cpus are removed, it is safe to update.
* 
* Before attempting to acquire the update lock on a pmap, a cpu (A) must
* be at least at the priority of the interprocessor interrupt
* (splip<=splvm). Otherwise, A could grab a lock and be interrupted by a
* kernel update; it would spin forever in pmap_update_interrupt() trying
* to acquire the user pmap lock it had already acquired. Furthermore A
* must remove itself from cpus_active.  Otherwise, another cpu holding
* the lock (B) could be in the process of sending an update signal to A,
* and thus be waiting for A to remove itself from cpus_active. If A is
* spinning on the lock at priority this will never happen and a deadlock
* will result.
*/

/*
 *	Signal another CPU that it must flush its TLB
 */
void    signal_cpus(use_list, pmap, start, end)
	cpu_set		use_list;
	pmap_t		pmap;
	vm_offset_t	start, end;
{
	register int		which_cpu, j;
	register pmap_update_list_t	update_list_p;

	while ((which_cpu = ffs(use_list)) != 0) {
	    which_cpu -= 1;	/* convert to 0 origin */

	    update_list_p = &cpu_update_list[which_cpu];
	    simple_lock(&update_list_p->lock);

	    j = update_list_p->count;
	    if (j >= UPDATE_LIST_SIZE) {
		/*
		 *	list overflowed.  Change last item to
		 *	indicate overflow.
		 */
		update_list_p->item[UPDATE_LIST_SIZE-1].pmap  = kernel_pmap;
		update_list_p->item[UPDATE_LIST_SIZE-1].start = VM_MIN_ADDRESS;
		update_list_p->item[UPDATE_LIST_SIZE-1].end   = VM_MAX_KERNEL_ADDRESS;
	    }
	    else {
		update_list_p->item[j].pmap  = pmap;
		update_list_p->item[j].start = start;
		update_list_p->item[j].end   = end;
		update_list_p->count = j+1;
	    }
	    cpu_update_needed[which_cpu] = TRUE;
	    simple_unlock(&update_list_p->lock);

	    if ((cpus_idle & (1 << which_cpu)) == 0)
		interrupt_processor(which_cpu);
	    use_list &= ~(1 << which_cpu);
	}
}

void process_pmap_updates(my_pmap)
	register pmap_t		my_pmap;
{
	register int		my_cpu = cpu_number();
	register pmap_update_list_t	update_list_p;
	register int		j;
	register pmap_t		pmap;

	update_list_p = &cpu_update_list[my_cpu];
	simple_lock(&update_list_p->lock);

	for (j = 0; j < update_list_p->count; j++) {
	    pmap = update_list_p->item[j].pmap;
	    if (pmap == my_pmap ||
		pmap == kernel_pmap) {

		INVALIDATE_TLB(update_list_p->item[j].start,
				update_list_p->item[j].end);
	    }
	}
	update_list_p->count = 0;
	cpu_update_needed[my_cpu] = FALSE;
	simple_unlock(&update_list_p->lock);
}

#if	MACH_KDB

static boolean_t db_interp_int[NCPUS];
int db_inside_pmap_update[NCPUS];
int suicide_cpu;

cpu_interrupt_to_db(i)
	int i;
{
	db_interp_int[i] = TRUE;
	interrupt_processor(i);
}
#endif

/*
 *	Interrupt routine for TBIA requested from other processor.
 */
void pmap_update_interrupt()
{
	register int		my_cpu;
	register pmap_t		my_pmap;
	spl_t			s;

	my_cpu = cpu_number();

	db_inside_pmap_update[my_cpu]++;
#if	MACH_KDB
	if (db_interp_int[my_cpu]) {
		db_interp_int[my_cpu] = FALSE;
		remote_db_enter();
		/* In case another processor modified text  */
		alphacache_Iflush();
if (cpu_number() == suicide_cpu) halt();
		goto out;	/* uhmmm, maybe should do updates just in case */
	}
#endif
	/*
	 *	Exit now if we're idle.  We'll pick up the update request
	 *	when we go active, and we must not put ourselves back in
	 *	the active set because we'll never process the interrupt
	 *	while we're idle (thus hanging the system).
	 */
	if (cpus_idle & (1 << my_cpu))
	    goto out;

	if (current_thread() == THREAD_NULL)
	    my_pmap = kernel_pmap;
	else {
	    my_pmap = current_pmap();
	    if (!pmap_in_use(my_pmap, my_cpu))
		my_pmap = kernel_pmap;
	}

	/*
	 *	Raise spl to splvm (above splip) to block out pmap_extract
	 *	from IO code (which would put this cpu back in the active
	 *	set).
	 */
	s = splvm();

	do {

	    /*
	     *	Indicate that we're not using either user or kernel
	     *	pmap.
	     */
	    i_bit_clear(my_cpu, &cpus_active);

	    /*
	     *	Wait for any pmap updates in progress, on either user
	     *	or kernel pmap.
	     */
	    while (*(volatile int *)&my_pmap->lock.lock_data ||
		   *(volatile int *)&kernel_pmap->lock.lock_data)
		continue;

	    process_pmap_updates(my_pmap);

	    i_bit_set(my_cpu, &cpus_active);

	} while (cpu_update_needed[my_cpu]);
	
	splx(s);
out:
	db_inside_pmap_update[my_cpu]--;
}
#else	NCPUS > 1
/*
 *	Dummy routine to satisfy external reference.
 */
void pmap_update_interrupt()
{
	/* should never be called. */
}
#endif	/* NCPUS > 1 */

void
set_ptbr(pmap_t map, pcb_t pcb, boolean_t switchit)
{
	/* optimize later */
	vm_offset_t     pa;

	pa = pmap_resident_extract(kernel_pmap, (vm_offset_t)map->dirbase);
printf("set_ptbr (switch = %d): dirbase = 0x%lx, pa = 0x%lx\n", switchit, map->dirbase, pa);
	if (pa == 0)
		panic("set_ptbr");
#if 0
	pcb->mss.hw_pcb.ptbr = alpha_btop(pa);
	if (switchit) {
		pcb->mss.hw_pcb.asn = map->pid;
		swpctxt(kvtophys((vm_offset_t) pcb), &(pcb)->mss.hw_pcb.ksp);
	}
#else
	pcb->pcb_hw.apcb_ptbr = alpha_btop(pa);
	if (switchit) {
                pcb->pcb_hw.apcb_asn = map->pid;
                swpctxt(kvtophys((vm_offset_t) pcb), &(pcb)->pcb_hw.apcb_ksp);
        }
#endif
}

/***************************************************************************
 *
 *	TLBPID Management
 *
 *	This is basically a unique number generator, with the twist
 *	that numbers are in a given range (dynamically defined).
 *	All things considered, I did it right in the MIPS case.
 */

#if 0
/* above */
int	pmap_max_asn;
#endif

decl_simple_lock_data(static, tlbpid_lock)
static struct pmap **pids_in_use;
static int pmap_next_pid;

pmap_tlbpid_init(maxasn)
	int maxasn;
{
	simple_lock_init(&tlbpid_lock);

        if (DOVPDB(PDB_FOLLOW|PDB_TLBPID_INIT))
                printf("pmap_tlbpid_init: maxasn = %d\n", maxasn);

	pmap_max_asn = maxasn;
	if (maxasn == 0) {
		/* ASNs not implemented...  Is this the right way to check? */
		return;
	}
	
	pids_in_use = (struct pmap **)
		pmap_bootstrap_alloc((maxasn + 1) * sizeof(struct pmap *));
	bzero(pids_in_use, (maxasn + 1) * sizeof(struct pmap *));

	pmap_next_pid = 1;
}

/*
 * Axioms:
 *	- pmap_next_pid always points to a free one, unless the table is full;
 *	  in that case it points to a likely candidate for recycling.
 *	- pmap.pid prevents from making duplicates: if -1 there is no
 *	  pid for it, otherwise there is one and only one entry at that index.
 *
 * pmap_tlbpid_assign	provides a tlbpid for the given pmap, creating
 *			a new one if necessary
 * pmap_tlbpid_destroy	returns a tlbpid to the pool of available ones
 */

pmap_tlbpid_assign(map)
	struct pmap *map;
{
	register int pid, next_pid;

        if (DOVPDB(PDB_FOLLOW|PDB_TLBPID_ASSIGN))
                printf("pmap_tlbpid_assign: pmap %p had %d\n", map, map->pid);

	if (pmap_max_asn && map->pid < 0) {

		simple_lock(&tlbpid_lock);

		next_pid = pmap_next_pid;
		if (pids_in_use[next_pid]) {
			/* are we _really_ sure it's full ? */
			for (pid = 1; pid < pmap_max_asn; pid++)
				if (pids_in_use[pid] == PMAP_NULL) {
					/* aha! */
					next_pid = pid;
					goto got_a_free_one;
				}
			/* Table full */
			while (pids_in_use[next_pid]->cpus_using) {
				if (++next_pid == pmap_max_asn)
					next_pid = 1;
			}
			pmap_tlbpid_destroy(next_pid, TRUE);
		}
got_a_free_one:
		pids_in_use[next_pid] = map;
		map->pid = next_pid;
		if (++next_pid == pmap_max_asn)
			next_pid = 1;
		pmap_next_pid = next_pid;

		simple_unlock(&tlbpid_lock);
	}
        if (DOVPDB(PDB_FOLLOW|PDB_TLBPID_ASSIGN))
                printf("pmap_tlbpid_assign: pmap %p got %d\n", map, map->pid);
}

pmap_tlbpid_destroy(pid, locked)
	int 		pid;
	boolean_t	locked;
{
	struct pmap    *map;

        if (DOVPDB(PDB_FOLLOW|PDB_TLBPID_DESTROY))
                printf("pmap_tlbpid_destroy(%d, %d)\n", pid, locked);

	if (pid < 0)	/* no longer in use */
		return;

	assert(pmap_max_asn);

	if (!locked) simple_lock(&tlbpid_lock);

	/*
	 * Make the pid available, and the map unassigned.
	 */
	map = pids_in_use[pid];
	assert(map != NULL);
	pids_in_use[pid] = PMAP_NULL;
	map->pid = -1;

	if (!locked) simple_unlock(&tlbpid_lock);
}

#if	1 /* DEBUG */

print_pv_list()
{
	pv_entry_t	p;
	vm_offset_t	phys;

	db_printf("phys pages %x < p < %x\n", vm_first_phys, vm_last_phys);
	for (phys = vm_first_phys; phys < vm_last_phys; phys += PAGE_SIZE) {
		p = pai_to_pvh(pa_index(phys));
		if (p->pmap != PMAP_NULL) {
			db_printf("%x: %x %x\n", phys, p->pmap, p->va);
			while (p = p->next)
				db_printf("\t\t%x %x\n", p->pmap, p->va);
		}
	}
}

#endif

vm_offset_t
pmap_phys_address(ppn)
	int ppn;
{
	return(alpha_ptob(ppn));
}

void pmap_copy(dst_pmap, src_pmap, dst_addr, len, src_addr)
        pmap_t          dst_pmap;
        pmap_t          src_pmap;
        vm_offset_t     dst_addr;
        vm_size_t       len;
        vm_offset_t     src_addr;
{
}

void pmap_update()
{
}

vm_page_t
vm_page_grab()
{
        register vm_page_t      mem;
        int             spl;

        spl = splimp();                         /* XXX */
        simple_lock(&vm_page_queue_free_lock);
        if (vm_page_queue_free.tqh_first == NULL) {
                simple_unlock(&vm_page_queue_free_lock);
                splx(spl);
                return (NULL);
        }

        mem = vm_page_queue_free.tqh_first;
        TAILQ_REMOVE(&vm_page_queue_free, mem, pageq);

        cnt.v_free_count--;
        simple_unlock(&vm_page_queue_free_lock);
        splx(spl);

        mem->flags = PG_BUSY | PG_CLEAN | PG_FAKE;
        mem->wire_count = 0;

        /*
         *      Decide if we should poke the pageout daemon.
         *      We do this if the free count is less than the low
         *      water mark, or if the free count is less than the high
         *      water mark (but above the low water mark) and the inactive
         *      count is less than its target.
         *
         *      We don't have the counts locked ... if they change a little,
         *      it doesn't really matter.
         */

        if (cnt.v_free_count < cnt.v_free_min ||
            (cnt.v_free_count < cnt.v_free_target &&
             cnt.v_inactive_count < cnt.v_inactive_target))
                thread_wakeup((void *)&vm_pages_needed);
        return (mem);
}

int
vm_page_wait()
{

	assert_wait(&cnt.v_free_count, 0);
	thread_block();
}

/*
 * Emulate reference and/or modified bit hits.
 */
void
pmap_emulate_reference(p, v, user, write)
        struct proc *p;
        vm_offset_t v;
        int user;
        int write;
{
	/* XXX */
}

struct pv_page;

struct pv_page_info {
        TAILQ_ENTRY(pv_page) pgi_list;
        struct pv_entry *pgi_freelist;
        int pgi_nfree;
};

#define NPVPPG  ((NBPG - sizeof(struct pv_page_info)) / sizeof(struct pv_entry))

struct pv_page {
        struct pv_page_info pvp_pgi;
        struct pv_entry pvp_pv[NPVPPG];
};

TAILQ_HEAD(pv_page_list, pv_page) pv_page_freelist;
int             pv_nfree;

#define pv_next next

struct pv_entry *
pmap_alloc_pv()
{
	struct pv_page *pvp;
	struct pv_entry *pv;
	int i;

	if (pv_nfree == 0) {
		pvp = (struct pv_page *)kmem_alloc(kernel_map, NBPG);
		if (pvp == 0)
			panic("pmap_alloc_pv: kmem_alloc() failed");
		pvp->pvp_pgi.pgi_freelist = pv = &pvp->pvp_pv[1];
		for (i = NPVPPG - 2; i; i--, pv++)
			pv->pv_next = pv + 1;
		pv->pv_next = 0;
		pv_nfree += pvp->pvp_pgi.pgi_nfree = NPVPPG - 1;
		TAILQ_INSERT_HEAD(&pv_page_freelist, pvp, pvp_pgi.pgi_list);
		pv = &pvp->pvp_pv[0];
	} else {
		--pv_nfree;
		pvp = pv_page_freelist.tqh_first;
		if (--pvp->pvp_pgi.pgi_nfree == 0) {
			TAILQ_REMOVE(&pv_page_freelist, pvp, pvp_pgi.pgi_list);
		}
		pv = pvp->pvp_pgi.pgi_freelist;
#ifdef DIAGNOSTIC
		if (pv == 0)
			panic("pmap_alloc_pv: pgi_nfree inconsistent");
#endif
		pvp->pvp_pgi.pgi_freelist = pv->pv_next;
	}
	return pv;
}

void
pmap_free_pv(pv)
	struct pv_entry *pv;
{
	register struct pv_page *pvp;
	register int i;

	pvp = (struct pv_page *) trunc_page(pv);
	switch (++pvp->pvp_pgi.pgi_nfree) {
	case 1:
		TAILQ_INSERT_TAIL(&pv_page_freelist, pvp, pvp_pgi.pgi_list);
	default:
		pv->pv_next = pvp->pvp_pgi.pgi_freelist;
		pvp->pvp_pgi.pgi_freelist = pv;
		++pv_nfree;
		break;
	case NPVPPG:
		pv_nfree -= NPVPPG - 1;
		TAILQ_REMOVE(&pv_page_freelist, pvp, pvp_pgi.pgi_list);
		kmem_free(kernel_map, (vm_offset_t)pvp, NBPG);
		break;
	}
}

#if 0
sanity(pmap, addr)
        register pmap_t         pmap;
        register vm_offset_t    addr;
{
        register pt_entry_t     *ptp;
        register pt_entry_t     pte;
	
	printf("checking dirbase...\n");
	assert(pmap->dirbase != 0);
	printf("checking dirpfn...\n");
	assert(pmap->dirpfn == curproc->p_addr->u_pcb.pcb_hw.apcb_ptbr);
	printf("checking pid...\n");
	assert(pmap->pid == curproc->p_addr->u_pcb.pcb_hw.apcb_asn);

	
        /* seg1 */
        pte = *pmap_pde(pmap,addr);
        if ((pte & ALPHA_PTE_VALID) == 0)
                return(PT_ENTRY_NULL);
        /* seg2 */
        ptp = (pt_entry_t *)ptetokv(pte);
        pte = ptp[pte2num(addr)];
        if ((pte & ALPHA_PTE_VALID) == 0)
                return(PT_ENTRY_NULL);
        /* seg3 */
        ptp = (pt_entry_t *)ptetokv(pte);
        return(&ptp[pte3num(addr)]);

}
#endif
