/*	$OpenBSD: pmap.c,v 1.75 2015/02/15 21:34:33 miod Exp $ */
/*	$NetBSD: pmap.c,v 1.74 1999/11/13 21:32:25 matt Exp $	   */
/*
 * Copyright (c) 1994, 1998, 1999, 2003 Ludd, University of Lule}, Sweden.
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
 *     This product includes software developed at Ludd, University of Lule}.
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
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/malloc.h>
#include <sys/extent.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/msgbuf.h>
#include <sys/pool.h>

#ifdef PMAPDEBUG
#include <dev/cons.h>
#endif

#include <uvm/uvm.h>

#include <machine/pte.h>
#include <machine/pcb.h>
#include <machine/mtpr.h>
#include <machine/macros.h>
#include <machine/sid.h>
#include <machine/cpu.h>
#include <machine/scb.h>
#include <machine/rpb.h>

#define ISTACK_SIZE (NBPG * 2)
vaddr_t	istack;

struct pmap kernel_pmap_store;

pt_entry_t *Sysmap;		/* System page table */
u_int	sysptsize;

/*
 * Scratch pages usage:
 * Page 1: initial frame pointer during autoconfig. Stack and pcb for
 *	   processes during exit on boot CPU only.
 * Page 2: unused
 * Page 3: unused
 * Page 4: unused
 */
vaddr_t scratch;
#define	SCRATCHPAGES	4

vaddr_t	iospace;

vaddr_t ptemapstart, ptemapend;
struct	extent *ptemap;
#define	PTMAPSZ	EXTENT_FIXED_STORAGE_SIZE(100)
char	ptmapstorage[PTMAPSZ];

struct pool pmap_pmap_pool;
struct pool pmap_ptp_pool;
struct pool pmap_pv_pool;

#define	NPTEPG		0x80	/* # of PTEs per page (logical or physical) */
#define	PPTESZ		sizeof(pt_entry_t)
#define	NPTEPERREG	0x200000

#define	SEGTYPE(x)	(((vaddr_t)(x)) >> 30)
#define	P0SEG		0
#define	P1SEG		1
#define	SYSSEG		2

#define USRPTSIZE	((MAXTSIZ + MAXDSIZ + BRKSIZ + MAXSSIZ) / VAX_NBPG)
#define	NPTEPGS		(USRPTSIZE / (NBPG / (sizeof(pt_entry_t) * LTOHPN)))

/* Mapping macros used when allocating SPT */
#define MAPVIRT(ptr, count)						\
do {									\
	ptr = virtual_avail;						\
	virtual_avail += (count) * VAX_NBPG;				\
} while (0)

#ifdef PMAPDEBUG
volatile int recurse;
#define RECURSESTART							\
do {									\
	if (recurse)							\
		printf("enter at %d, previous %d\n", __LINE__, recurse);\
	recurse = __LINE__;						\
} while (0)
#define RECURSEEND							\
do {									\
	recurse = 0;							\
} while (0)
int	startpmapdebug = 0;
#define PMDEBUG(x) if (startpmapdebug) printf x
#else
#define RECURSESTART
#define RECURSEEND
#define	PMDEBUG(x)
#endif

vsize_t	calc_kvmsize(vsize_t);
u_long	pmap_extwrap(vsize_t);
void	rmpage(struct pmap *, pt_entry_t *);
void	update_pcbs(struct pmap *);
void	rmspace(struct pmap *);
int	pmap_rmproc(struct pmap *);
vaddr_t	pmap_getusrptes(struct pmap *, vsize_t, int);
void	rmptep(pt_entry_t *);
boolean_t grow_p0(struct pmap *, u_long, int);
boolean_t grow_p1(struct pmap *, u_long, int);
pt_entry_t *vaddrtopte(const struct pv_entry *pv);
void	pmap_remove_pcb(struct pmap *, struct pcb *);

/*
 * Map in a virtual page.
 */
static inline void
mapin8(pt_entry_t *ptep, pt_entry_t pte)
{
	ptep[0] = pte;
	ptep[1] = pte + 1;
	ptep[2] = pte + 2;
	ptep[3] = pte + 3;
	ptep[4] = pte + 4;
	ptep[5] = pte + 5;
	ptep[6] = pte + 6;
	ptep[7] = pte + 7;
}

/*
 * Check if page table page is in use.
 */
static inline int
ptpinuse(pt_entry_t *pte)
{
	pt_entry_t *pve = (pt_entry_t *)vax_trunc_page(pte);
	uint i;

	for (i = 0; i < NPTEPG; i += LTOHPN)
		if (pve[i] != PG_NV)
			return 1;
	return 0;
}

vaddr_t   avail_start, avail_end;
vaddr_t   virtual_avail, virtual_end; /* Available virtual memory */

#define	get_pventry()    (struct pv_entry *)pool_get(&pmap_pv_pool, PR_NOWAIT)
#define	free_pventry(pv) pool_put(&pmap_pv_pool, (void *)pv)

static inline
paddr_t
get_ptp(boolean_t waitok)
{
	pt_entry_t *ptp;

	ptp = (pt_entry_t *)pool_get(&pmap_ptp_pool,
	    PR_ZERO | (waitok ? PR_WAITOK : PR_NOWAIT));
	if (ptp == NULL)
		return 0;
	return ((paddr_t)ptp) & ~KERNBASE;
}

#define	free_ptp(pa)	pool_put(&pmap_ptp_pool, (void *)(pa | KERNBASE))

/*
 * Calculation of the System Page Table is somewhat a pain, because it
 * must be in contiguous physical memory and all size calculations must
 * be done before memory management is turned on.
 * Arg is usrptsize in ptes.
 */
vsize_t
calc_kvmsize(vsize_t usrptsize)
{
	vsize_t kvmsize;

	/*
	 * Compute the number of pages kmem_map will have.
	 */
	kmeminit_nkmempages();

	/* All physical memory (reverse mapping struct) */
	kvmsize = avail_end;
	/* User Page table area. This may be large */
	kvmsize += usrptsize * sizeof(pt_entry_t);
	/* Kernel stacks per process */
	kvmsize += USPACE * maxthread;
	/* kernel malloc arena */
	kvmsize += nkmempages * PAGE_SIZE;
	/* IO device register space */
	kvmsize += IOSPSZ * VAX_NBPG;
	/* Pager allocations */
	kvmsize += PAGER_MAP_SIZE;
	/* kernel malloc arena */
	kvmsize += avail_end;

	/* Exec arg space */
	kvmsize += 16 * NCARGS;
#if VAX46 || VAX48 || VAX49 || VAX53 || VAX60
	/* Physmap */
	kvmsize += VM_PHYS_SIZE;
#endif

	return round_page(kvmsize);
}

/*
 * pmap_bootstrap().
 * Called as part of vm bootstrap, allocates internal pmap structures.
 * Assumes that nothing is mapped, and that kernel stack is located
 * immediately after end.
 */
void
pmap_bootstrap()
{
	unsigned int i;
	extern	unsigned int etext, proc0paddr;
	struct pcb *pcb = (struct pcb *)proc0paddr;
	struct pmap *pmap = pmap_kernel();
	vsize_t kvmsize, usrptsize, minusrptsize;

	/* Set logical page size */
	uvmexp.pagesize = NBPG;
	uvm_setpagesize();

	/*
	 * Compute how much page table space a process reaching all its
	 * limits would need. Try to afford four times such this space,
	 * but try and limit ourselves to 5% of the free memory.
	 */
	minusrptsize = (MAXTSIZ + MAXDSIZ + BRKSIZ + MAXSSIZ) / VAX_NBPG;
	usrptsize = 4 * minusrptsize;
	if (vax_btop(usrptsize * PPTESZ) > avail_end / 20)
		usrptsize = (avail_end / (20 * PPTESZ)) * VAX_NBPG;
	if (usrptsize < minusrptsize)
		usrptsize = minusrptsize;

	kvmsize = calc_kvmsize(usrptsize);
	sysptsize = vax_btop(kvmsize);

	/*
	 * Virtual_* and avail_* is used for mapping of system page table.
	 * The need for kernel virtual memory is linear dependent of the
	 * amount of physical memory also, therefore sysptsize is 
	 * a variable here that is changed dependent of the physical
	 * memory size.
	 */
	virtual_avail = avail_end + KERNBASE;
	virtual_end = KERNBASE + sysptsize * VAX_NBPG;
	/* clear SPT before using it */
	memset(Sysmap, 0, sysptsize * sizeof(pt_entry_t));

	/*
	 * The first part of Kernel Virtual memory is the physical
	 * memory mapped in. This makes some mm routines both simpler
	 * and faster, but takes ~0.75% more memory.
	 */
	pmap_map(KERNBASE, 0, avail_end, PROT_READ | PROT_WRITE);
	/*
	 * Kernel code is always readable for user, it must be because
	 * of the emulation code that is somewhere in there.
	 * And it doesn't hurt, the kernel file is also public readable.
	 * There are also a couple of other things that must be in
	 * physical memory and that isn't managed by the vm system.
	 */
	for (i = 0; i < ((unsigned)&etext & ~KERNBASE) >> VAX_PGSHIFT; i++)
		Sysmap[i] = (Sysmap[i] & ~PG_PROT) | PG_URKW;

	/* Map System Page Table and zero it,  Sysmap already set. */
	mtpr((vaddr_t)Sysmap - KERNBASE, PR_SBR);

	/* Map Interrupt stack and set red zone */
	istack = (vaddr_t)Sysmap + round_page(sysptsize * sizeof(pt_entry_t));
	mtpr(istack + ISTACK_SIZE, PR_ISP);
	*kvtopte(istack) &= ~PG_V;

	/* Some scratch pages */
	scratch = istack + ISTACK_SIZE;

	avail_start = (vaddr_t)(scratch + SCRATCHPAGES * VAX_NBPG) - KERNBASE;

	/* Kernel message buffer */
	avail_end -= round_page(MSGBUFSIZE);
	msgbufp = (void *)(avail_end + KERNBASE);
	msgbufp->msg_magic = MSG_MAGIC-1; 	/* ensure that it will be zeroed */

	/* zero all mapped physical memory from Sysmap to here */
	memset((void *)istack, 0, (avail_start + KERNBASE) - istack);

	/* User page table map. This is big. */
	MAPVIRT(ptemapstart, vax_atop(usrptsize * sizeof(pt_entry_t)));
	ptemapend = virtual_avail;

	MAPVIRT(iospace, IOSPSZ); /* Device iospace mapping area */

	/* Init SCB and set up stray vectors. */
	avail_start = scb_init(avail_start);
	bcopy((caddr_t)proc0paddr + REDZONEADDR, 0, sizeof(struct rpb));

	if (dep_call->cpu_init)
		(*dep_call->cpu_init)();

	avail_start = round_page(avail_start);
	virtual_avail = round_page(virtual_avail);
	virtual_end = trunc_page(virtual_end);


#if 0 /* Breaks cninit() on some machines */
	cninit();
	printf("Sysmap %p, istack %p, scratch %p\n", Sysmap, istack, scratch);
	printf("etext %p\n", &etext);
	printf("SYSPTSIZE %x usrptsize %lx\n",
	    sysptsize, usrptsize * sizeof(pt_entry_t));
	printf("ptemapstart %lx ptemapend %lx\n", ptemapstart, ptemapend);
	printf("avail_start %lx, avail_end %lx\n", avail_start, avail_end);
	printf("virtual_avail %lx,virtual_end %lx\n",
	    virtual_avail, virtual_end);
	printf("startpmapdebug %p\n",&startpmapdebug);
#endif

	/* Init kernel pmap */
	pmap->pm_p1br = (pt_entry_t *)KERNBASE;
	pmap->pm_p0br = (pt_entry_t *)KERNBASE;
	pmap->pm_p1lr = NPTEPERREG;
	pmap->pm_p0lr = 0;
	pmap->pm_stats.wired_count = pmap->pm_stats.resident_count = 0;
	    /* btop(virtual_avail - KERNBASE); */

	pmap->pm_count = 1;

	/* Activate the kernel pmap. */
	pcb->P1BR = pmap->pm_p1br;
	pcb->P0BR = pmap->pm_p0br;
	pcb->P1LR = pmap->pm_p1lr;
	pcb->P0LR = pmap->pm_p0lr | AST_PCB;
	pcb->pcb_pm = pmap;
	pcb->pcb_pmnext = pmap->pm_pcbs;
	pmap->pm_pcbs = pcb;
	mtpr((register_t)pcb->P1BR, PR_P1BR);
	mtpr((register_t)pcb->P0BR, PR_P0BR);
	mtpr(pcb->P1LR, PR_P1LR);
	mtpr(pcb->P0LR, PR_P0LR);

	/* Create the pmap, ptp and pv_entry pools. */
	pool_init(&pmap_pmap_pool, sizeof(struct pmap), 0, 0, 0,
	    "pmap_pool", NULL);
	pool_init(&pmap_ptp_pool, VAX_NBPG, 0, 0, 0, "ptp_pool", NULL);
	pool_init(&pmap_pv_pool, sizeof(struct pv_entry), 0, 0, 0,
	    "pv_pool", NULL);

	/*
	 * Now everything should be complete, start virtual memory.
	 */
	uvm_page_physload(atop(avail_start), atop(avail_end),
	    atop(avail_start), atop(avail_end), 0);
	mtpr(sysptsize, PR_SLR);
	rpb.sbr = mfpr(PR_SBR);
	rpb.slr = mfpr(PR_SLR);
	mtpr(1, PR_MAPEN);
}

void
pmap_virtual_space(vaddr_t *vstartp, vaddr_t *vendp)
{
	*vstartp = virtual_avail;
	*vendp = virtual_end;
}

/*
 * Let the VM system do early memory allocation from the direct-mapped
 * physical memory instead.
 */
vaddr_t
pmap_steal_memory(vsize_t size, vaddr_t *vstartp, vaddr_t *vendp)
{
	vaddr_t v;
	int npgs;

	PMDEBUG(("pmap_steal_memory: size 0x%lx start %p end %p\n",
	    size, vstartp, vendp));

	size = round_page(size);
	npgs = atop(size);

#ifdef DIAGNOSTIC
	if (uvm.page_init_done == TRUE)
		panic("pmap_steal_memory: called _after_ bootstrap");
#endif

	/*
	 * A vax only has one segment of memory.
	 */

	v = (vm_physmem[0].avail_start << PAGE_SHIFT) | KERNBASE;
	vm_physmem[0].avail_start += npgs;
	vm_physmem[0].start += npgs;
	if (vstartp)
		*vstartp = virtual_avail;
	if (vendp)
		*vendp = virtual_end;
	bzero((caddr_t)v, size);
	return v;
}

/*
 * pmap_init() is called as part of vm init after memory management
 * is enabled. It is meant to do machine-specific allocations.
 * The extent for the user page tables is initialized here.
 */
void 
pmap_init() 
{
        /*
         * Create the extent map used to manage the page table space.
         */
        ptemap = extent_create("ptemap", ptemapstart, ptemapend,
            M_VMPMAP, ptmapstorage, PTMAPSZ, EX_NOCOALESCE);
        if (ptemap == NULL)
		panic("pmap_init");
}

u_long
pmap_extwrap(vsize_t nsize)
{
	int res;
	u_long rv;

	for (;;) {
		res = extent_alloc(ptemap, nsize, PAGE_SIZE, 0, 0,
		    EX_WAITOK | EX_MALLOCOK, &rv);
		if (res == 0)
			return rv;
		if (res == EAGAIN)
			return 0;
	}
}

/*
 * Do a page removal from the pv list. A page is identified by its
 * virtual address combined with its struct pmap in the page's pv list.
 */
void
rmpage(struct pmap *pm, pt_entry_t *br)
{
	struct pv_entry *pv, *pl, *pf;
	vaddr_t vaddr;
	struct vm_page *pg;
	int s, found = 0;

	/*
	 * Check that we are working on a managed page.
	 */
	pg = PHYS_TO_VM_PAGE((*br & PG_FRAME) << VAX_PGSHIFT);
	if (pg == NULL)
		return;

	if (pm == pmap_kernel())
		vaddr = (br - Sysmap) * VAX_NBPG + 0x80000000;
	else if (br >= pm->pm_p0br && br < pm->pm_p0br + pm->pm_p0lr)
		vaddr = (br - pm->pm_p0br) * VAX_NBPG;
	else
		vaddr = (br - pm->pm_p1br) * VAX_NBPG + 0x40000000;

	s = splvm();
	for (pl = NULL, pv = pg->mdpage.pv_head; pv != NULL; pl = pv, pv = pf) {
		pf = pv->pv_next;
		if (pv->pv_pmap == pm && pv->pv_va == vaddr) {
			if ((pg->mdpage.pv_attr & (PG_V|PG_M)) != (PG_V|PG_M)) {
				switch (br[0] & PG_PROT) {
				case PG_URKW:
				case PG_KW:
				case PG_RW:
					pg->mdpage.pv_attr |=
					    br[0] | br[1] | br[2] | br[3] |
					    br[4] | br[5] | br[6] | br[7];
					break;
				}
			}
			if (pf != NULL) {
				*pv = *pf;
				free_pventry(pf);
			} else {
				if (pl != NULL)
					pl->pv_next = pv->pv_next;
				else
					pg->mdpage.pv_head = NULL;
				free_pventry(pv);
			}
			found++;
			break;
		}
	}
	splx(s);
	if (found == 0)
		panic("rmpage: pg %p br %p", pg, br);
}

/*
 * Update the PCBs using this pmap after a change.
 */
void
update_pcbs(struct pmap *pm)
{
	struct pcb *pcb;

	PMDEBUG(("update_pcbs pm %p\n", pm));

	for (pcb = pm->pm_pcbs; pcb != NULL; pcb = pcb->pcb_pmnext) {
		KASSERT(pcb->pcb_pm == pm);
		pcb->P0BR = pm->pm_p0br;
		pcb->P0LR = pm->pm_p0lr | AST_PCB;
		pcb->P1BR = pm->pm_p1br;
		pcb->P1LR = pm->pm_p1lr;
	}

	/* If curproc uses this pmap update the regs too */ 
	if (pm == curproc->p_vmspace->vm_map.pmap) {
		PMDEBUG(("update_pcbs: %08x %08x %08x %08x\n",
		    pm->pm_p0br, pm->pm_p0lr, pm->pm_p1br, pm->pm_p1lr));
                mtpr((register_t)pm->pm_p0br, PR_P0BR);
                mtpr(pm->pm_p0lr | AST_PCB, PR_P0LR);
                mtpr((register_t)pm->pm_p1br, PR_P1BR);
                mtpr(pm->pm_p1lr, PR_P1LR);
	}
}

/*
 * Remove a full process space. Update all processes pcbs.
 */
void
rmspace(struct pmap *pm)
{
	u_long lr, i, j;
	pt_entry_t *ptpp, *br;

	if (pm->pm_p0lr == 0 && pm->pm_p1lr == NPTEPERREG)
		return; /* Already free */

	lr = pm->pm_p0lr / NPTEPG;
	for (i = 0; i < lr; i++) {
		ptpp = kvtopte((vaddr_t)&pm->pm_p0br[i * NPTEPG]);
		if (*ptpp == PG_NV)
			continue;
		br = &pm->pm_p0br[i * NPTEPG];
		for (j = 0; j < NPTEPG; j += LTOHPN) {
			if (br[j] == 0)
				continue;
			rmpage(pm, &br[j]);
		}
		free_ptp((*ptpp & PG_FRAME) << VAX_PGSHIFT);
		*ptpp = PG_NV;
	}
	lr = pm->pm_p1lr / NPTEPG;
	for (i = lr; i < NPTEPERREG / NPTEPG; i++) {
		ptpp = kvtopte((vaddr_t)&pm->pm_p1br[i * NPTEPG]);
		if (*ptpp == PG_NV)
			continue;
		br = &pm->pm_p1br[i * NPTEPG];
		for (j = 0; j < NPTEPG; j += LTOHPN) {
			if (br[j] == 0)
				continue;
			rmpage(pm, &br[j]);
		}
		free_ptp((*ptpp & PG_FRAME) << VAX_PGSHIFT);
		*ptpp = PG_NV;
	}

	if (pm->pm_p0lr != 0)
		extent_free(ptemap, (u_long)pm->pm_p0br,
		    pm->pm_p0lr * PPTESZ, EX_WAITOK);
	if (pm->pm_p1lr != NPTEPERREG)
		extent_free(ptemap, (u_long)pm->pm_p1ap,
		    (NPTEPERREG - pm->pm_p1lr) * PPTESZ, EX_WAITOK);
	pm->pm_p0br = pm->pm_p1br = (pt_entry_t *)KERNBASE;
	pm->pm_p0lr = 0;
	pm->pm_p1lr = NPTEPERREG;
	pm->pm_p1ap = NULL;
	update_pcbs(pm);
}

/*
 * Find a process to remove the process space for. *sigh*
 * Avoid to remove ourselves. Logic is designed after uvm_swapout_threads().
 */

int
pmap_rmproc(struct pmap *pm)
{
	struct process *pr, *outpr;
	struct pmap *ppm;
	struct proc *p, *slpp;
	int outpri;
	int didswap = 0;
	extern int maxslp;

	outpr = NULL;
	outpri = 0;
	LIST_FOREACH(pr, &allprocess, ps_list) {
		if (pr->ps_flags & (PS_SYSTEM | PS_EXITING))
			continue;
		ppm = pr->ps_vmspace->vm_map.pmap;
		if (ppm == pm)		/* Don't swap ourself */
			continue;
		if (ppm->pm_p0lr == 0 && ppm->pm_p1lr == NPTEPERREG)
			continue;	/* Already swapped */

		/*
		 * slpp: the sleeping or stopped thread in pr with
		 * the smallest p_slptime
		 */
		slpp = NULL;
		TAILQ_FOREACH(p, &pr->ps_threads, p_thr_link) {
			switch (p->p_stat) {
			case SRUN:
			case SONPROC:
				goto next_process;

			case SSLEEP:
			case SSTOP:
				if (slpp == NULL ||
				    slpp->p_slptime < p->p_slptime)
				slpp = p;
				continue;
			}
		}
		if (slpp != NULL) {
			if (slpp->p_slptime >= maxslp) {
				rmspace(ppm);
				didswap++;
			} else if (slpp->p_slptime > outpri) {
				outpr = pr;
				outpri = slpp->p_slptime;
			}
		}
		if (didswap)
			break;
next_process:	;
	}

	if (didswap == 0 && outpr != NULL) {
		rmspace(outpr->ps_vmspace->vm_map.pmap);
		didswap++;
	}
	return didswap;
}

/*
 * Allocate space for user page tables, from ptemap.
 * If the map is full then:
 * 1) Remove processes idle for more than 20 seconds or stopped.
 * 2) Remove processes idle for less than 20 seconds.
 * 
 * Argument is needed space, in bytes.
 * Returns a pointer to the newly allocated space, or zero if space could not
 * be allocated and failure is allowed. Panics otherwise.
 */
vaddr_t
pmap_getusrptes(struct pmap *pm, vsize_t nsize, int canfail)
{
	u_long rv;

#ifdef DEBUG
	if (nsize & PAGE_MASK)
		panic("pmap_getusrptes: bad size %lx", nsize);
#endif
	for (;;) {
		rv = pmap_extwrap(nsize);
		if (rv != 0)
			return rv;
		if (pmap_rmproc(pm) == 0) {
			if (canfail)
				return 0;
			else
				panic("out of space in usrptmap");
		}
	}
}

/*
 * Remove a pte page when all references are gone.
 */
void
rmptep(pt_entry_t *pte)
{
	pt_entry_t *ptpp = kvtopte((vaddr_t)pte);

	PMDEBUG(("rmptep: pte %p -> ptpp %p\n", pte, ptpp));

#ifdef DEBUG
	{
		int i;
		pt_entry_t *ptr = (pt_entry_t *)vax_trunc_page(pte);
		for (i = 0; i < NPTEPG; i++)
			if (ptr[i] != 0)
				panic("rmptep: ptr[%d] != 0", i);
	}
#endif

	free_ptp((*ptpp & PG_FRAME) << VAX_PGSHIFT);
	*ptpp = PG_NV;
}

boolean_t 
grow_p0(struct pmap *pm, u_long reqlen, int canfail)
{
	vaddr_t nptespc;
	pt_entry_t *from, *to;
	size_t srclen, dstlen;
	u_long p0br, p0lr, len;
	int inuse;

	PMDEBUG(("grow_p0: pmap %p reqlen %x\n", pm, reqlen));

	/* Get new pte space */
	p0lr = pm->pm_p0lr;
	inuse = p0lr != 0;
	len = round_page((reqlen + 1) * PPTESZ);
	RECURSEEND;
	nptespc = pmap_getusrptes(pm, len, canfail);
	if (nptespc == 0)
		return FALSE;
	RECURSESTART;

	/*
	 * Copy the old ptes to the new space.
	 * Done by moving on system page table.
	 */
	srclen = vax_btop(p0lr * PPTESZ) * PPTESZ;
	dstlen = vax_atop(len) * PPTESZ;
	from = kvtopte((vaddr_t)pm->pm_p0br);
	to = kvtopte(nptespc);

	PMDEBUG(("grow_p0: from %p to %p src %x dst %x\n",
	    from, to, srclen, dstlen));

	if (inuse)
		memcpy(to, from, srclen);
	bzero((char *)to + srclen, dstlen - srclen);

	p0br = (u_long)pm->pm_p0br;
	pm->pm_p0br = (pt_entry_t *)nptespc;
	pm->pm_p0lr = len / PPTESZ;
	update_pcbs(pm);

	if (inuse)
		extent_free(ptemap, p0br, p0lr * PPTESZ, EX_WAITOK);

	return TRUE;
}

boolean_t
grow_p1(struct pmap *pm, u_long len, int canfail)
{
	vaddr_t nptespc, optespc;
	pt_entry_t *from, *to;
	size_t nlen, olen;

	PMDEBUG(("grow_p1: pm %p len %x\n", pm, len));

	/* Get new pte space */
	nlen = (NPTEPERREG * PPTESZ) - trunc_page(len * PPTESZ);
	RECURSEEND;
	nptespc = pmap_getusrptes(pm, nlen, canfail);
	if (nptespc == 0)
		return FALSE;
	RECURSESTART;
	olen = (NPTEPERREG - pm->pm_p1lr) * PPTESZ;
	optespc = (vaddr_t)pm->pm_p1ap;

	/*
	 * Copy the old ptes to the new space.
	 * Done by moving on system page table.
	 */
	from = kvtopte(optespc);
	to = kvtopte(nptespc);

	PMDEBUG(("grow_p1: from %p to %p src %x dst %x\n",
	    from, to, vax_btop(olen), vax_btop(nlen)));

	bzero(to, vax_btop(nlen - olen) * PPTESZ);
	if (optespc)
		memcpy(kvtopte(nptespc + nlen - olen), from,
		    vax_btop(olen) * PPTESZ);

	pm->pm_p1ap = (pt_entry_t *)nptespc;
	pm->pm_p1br = (pt_entry_t *)(nptespc + nlen - (NPTEPERREG * PPTESZ));
	pm->pm_p1lr = NPTEPERREG - nlen / PPTESZ;
	update_pcbs(pm);

	if (optespc)
		extent_free(ptemap, optespc, olen, EX_WAITOK);

	return TRUE;
}

/*
 * pmap_create() creates a pmap for a new task.
 */
struct pmap * 
pmap_create()
{
	struct pmap *pmap;

	pmap = pool_get(&pmap_pmap_pool, PR_WAITOK | PR_ZERO);

	/*
	 * Do not allocate any pte's here, we don't know the size and
	 * we'll get a page fault anyway when some page is referenced,
	 * so defer until then.
	 */
	pmap->pm_p0br = pmap->pm_p1br = (pt_entry_t *)KERNBASE;
	pmap->pm_p0lr = 0;
	pmap->pm_p1lr = NPTEPERREG;
	pmap->pm_p1ap = NULL;

	PMDEBUG(("pmap_create: pmap %p p0br=%p p0lr=0x%lx p1br=%p p1lr=0x%lx\n",
    	    pmap, pmap->pm_p0br, pmap->pm_p0lr, pmap->pm_p1br, pmap->pm_p1lr));

	pmap->pm_count = 1;
	/* pmap->pm_stats.resident_count = pmap->pm_stats.wired_count = 0; */

	return pmap;
}

void
pmap_remove_holes(struct vmspace *vm)
{
	struct vm_map *map = &vm->vm_map;
	struct pmap *pmap = map->pmap;
	vaddr_t shole, ehole;

	if (pmap == pmap_kernel())	/* can of worms */
		return;

	shole = MAXTSIZ + MAXDSIZ + BRKSIZ;
	ehole = (vaddr_t)vm->vm_maxsaddr;
	shole = max(vm_map_min(map), shole);
	ehole = min(vm_map_max(map), ehole);

	if (ehole <= shole)
		return;

	(void)uvm_map(map, &shole, ehole - shole, NULL, UVM_UNKNOWN_OFFSET, 0,
	    UVM_MAPFLAG(PROT_NONE, PROT_NONE, MAP_INHERIT_SHARE, MADV_RANDOM,
	      UVM_FLAG_NOMERGE | UVM_FLAG_HOLE | UVM_FLAG_FIXED));
}

void
pmap_unwire(struct pmap *pmap, vaddr_t va)
{
	pt_entry_t *pte;
	uint i;

	RECURSESTART;
	if (va & KERNBASE) {
		pte = Sysmap;
		i = vax_btop(va - KERNBASE);
	} else { 
		if (va < 0x40000000)
			pte = pmap->pm_p0br;
		else
			pte = pmap->pm_p1br;
		i = PG_PFNUM(va);
	}

	pte[i] &= ~PG_W;
	RECURSEEND;
	pmap->pm_stats.wired_count--;
}

/*
 * pmap_destroy(pmap): Remove a reference from the pmap. 
 * If this was the last reference, release all its resources.
 */
void
pmap_destroy(struct pmap *pmap)
{
	int count;
#ifdef DEBUG
	vaddr_t saddr, eaddr;
#endif
  
	PMDEBUG(("pmap_destroy: pmap %p\n",pmap));

	count = --pmap->pm_count;
	if (count != 0)
		return;

#ifdef DIAGNOSTIC
	if (pmap->pm_pcbs)
		panic("pmap_destroy used pmap");
#endif

	if (pmap->pm_p0br != 0) {
#ifdef DEBUG
		saddr = (vaddr_t)pmap->pm_p0br;
		eaddr = saddr + pmap->pm_p0lr * PPTESZ;
		for (; saddr < eaddr; saddr += PAGE_SIZE)
			if ((*kvtopte(saddr) & PG_FRAME) != 0)
				panic("pmap_release: P0 page mapped");
		saddr = (vaddr_t)pmap->pm_p1br + pmap->pm_p1lr * PPTESZ;
		eaddr = VM_MAXUSER_ADDRESS;
		for (; saddr < eaddr; saddr += PAGE_SIZE)
			if ((*kvtopte(saddr) & PG_FRAME) != 0)
				panic("pmap_release: P1 page mapped");
#endif
	}

	if (pmap->pm_p0lr != 0)
		extent_free(ptemap, (u_long)pmap->pm_p0br,
		    pmap->pm_p0lr * PPTESZ, EX_WAITOK);
	if (pmap->pm_p1lr != NPTEPERREG)
		extent_free(ptemap, (u_long)pmap->pm_p1ap,
		    (NPTEPERREG - pmap->pm_p1lr) * PPTESZ, EX_WAITOK);

	pool_put(&pmap_pmap_pool, pmap);
}

pt_entry_t *
vaddrtopte(const struct pv_entry *pv)
{
	struct pmap *pm;

	if (pv->pv_va & KERNBASE)
		return &Sysmap[(pv->pv_va & ~KERNBASE) >> VAX_PGSHIFT];
	pm = pv->pv_pmap;
	if (pv->pv_va & 0x40000000)
		return &pm->pm_p1br[vax_btop(pv->pv_va & ~0x40000000)];
	else
		return &pm->pm_p0br[vax_btop(pv->pv_va)];
}

/*
 * New (real nice!) function that allocates memory in kernel space
 * without tracking it in the MD code.
 */
void
pmap_kenter_pa(vaddr_t va, paddr_t pa, vm_prot_t prot)
{
	pt_entry_t *ptp, opte;

	ptp = kvtopte(va);

	PMDEBUG(("pmap_kenter_pa: va: %lx, pa %lx, prot %x ptp %p\n",
	    va, pa, prot, ptp));

	opte = ptp[0];
	if ((opte & PG_FRAME) == 0) {
		pmap_kernel()->pm_stats.resident_count++;
		pmap_kernel()->pm_stats.wired_count++;
	}
	mapin8(ptp, PG_V | ((prot & PROT_WRITE) ? PG_KW : PG_KR) |
	    PG_PFNUM(pa) | PG_W | PG_SREF);
	if (opte & PG_V) {
		mtpr(0, PR_TBIA);
	}
}

void
pmap_kremove(vaddr_t va, vsize_t len)
{
	pt_entry_t *pte;
#ifdef PMAPDEBUG
	int i;
#endif

	PMDEBUG(("pmap_kremove: va: %lx, len %lx, ptp %p\n",
	    va, len, kvtopte(va)));

	pte = kvtopte(va);

#ifdef PMAPDEBUG
	/*
	 * Check if any pages are on the pv list.
	 * This shouldn't happen anymore.
	 */
	len >>= PAGE_SHIFT;
	for (i = 0; i < len; i++) {
		if ((*pte & PG_FRAME) == 0)
			continue;
		pmap_kernel()->pm_stats.resident_count--;
		pmap_kernel()->pm_stats.wired_count--;
		if ((*pte & PG_SREF) == 0)
			panic("pmap_kremove");
		bzero(pte, LTOHPN * sizeof(pt_entry_t));
		pte += LTOHPN;
	}
#else
	len >>= PAGE_SHIFT;
	pmap_kernel()->pm_stats.resident_count -= len;
	pmap_kernel()->pm_stats.wired_count -= len;
	bzero(pte, len * LTOHPN * sizeof(pt_entry_t));
#endif
	mtpr(0, PR_TBIA);
}

/*
 * pmap_enter() is the main routine that puts in mappings for pages, or
 * upgrades mappings to more "rights".
 */
int
pmap_enter(struct pmap *pmap, vaddr_t v, paddr_t p, vm_prot_t prot, int flags)
{
	struct pv_entry *pv;
	struct vm_page *pg;
	pt_entry_t newpte, oldpte;
	pt_entry_t *pteptr;	/* current pte to write mapping info to */
	pt_entry_t *ptpptr;	/* ptr to page table page */
	u_long pteidx;
	int s;

	PMDEBUG(("pmap_enter: pmap %p v %lx p %lx prot %x wired %d flags %x\n",
	    pmap, v, p, prot, (flags & PMAP_WIRED) != 0, flags));

	RECURSESTART;

	/* Find address of correct pte */
	switch (SEGTYPE(v)) {
	case SYSSEG:
		pteptr = Sysmap + vax_btop(v - KERNBASE);
		newpte = prot & PROT_WRITE ? PG_KW : PG_KR;
		break;
	case P0SEG:
		pteidx = vax_btop(v);
		if (pteidx >= pmap->pm_p0lr) {
			if (!grow_p0(pmap, pteidx, flags & PMAP_CANFAIL))
				return ENOMEM;
		}
		pteptr = pmap->pm_p0br + pteidx;
		newpte = prot & PROT_WRITE ? PG_RW : PG_RO;
		break;
	case P1SEG:
		pteidx = vax_btop(v - 0x40000000);
		if (pteidx < pmap->pm_p1lr) {
			if (!grow_p1(pmap, pteidx, flags & PMAP_CANFAIL))
				return ENOMEM;
		}
		pteptr = pmap->pm_p1br + pteidx;
		newpte = prot & PROT_WRITE ? PG_RW : PG_RO;
		break;
	default:
		panic("bad seg");
	}
	newpte |= vax_btop(p);

	if (SEGTYPE(v) != SYSSEG) {
		/*
		 * Check if a pte page must be mapped in.
		 */
		ptpptr = kvtopte((vaddr_t)pteptr);

		if (*ptpptr == PG_NV) {
			paddr_t pa;

			pa = get_ptp((flags & PMAP_CANFAIL) != 0);
			if (pa == 0) {
				RECURSEEND;
				return ENOMEM;
			}
			*ptpptr = PG_V | PG_KW | PG_PFNUM(pa);
		}
	}

	/*
	 * Do not keep track of anything if mapping IO space.
	 */
	pg = PHYS_TO_VM_PAGE(p);
	if (pg == NULL) {
		mapin8(pteptr, newpte);
		RECURSEEND;
		return 0;
	}

	if (flags & PMAP_WIRED)
		newpte |= PG_W;

	oldpte = *pteptr & ~(PG_V | PG_M);

	/* just a wiring change ? */
	if ((newpte ^ oldpte) == PG_W) {
		if (flags & PMAP_WIRED) {
			pmap->pm_stats.wired_count++;
			*pteptr |= PG_W;
		} else {
			pmap->pm_stats.wired_count--;
			*pteptr &= ~PG_W;
		}
		RECURSEEND;
		return 0;
	}

	/* mapping unchanged? just return. */
	if (newpte == oldpte) {
		RECURSEEND;
		return 0;
	}

	/* Changing mapping? */
	if ((newpte & PG_FRAME) == (oldpte & PG_FRAME)) {
		/* protection change. */
#if 0 /* done below */
		mtpr(0, PR_TBIA);
#endif
	} else {
		/*
		 * Mapped before? Remove it then.
		 */
		if (oldpte & PG_FRAME) {
			pmap->pm_stats.resident_count--;
			if (oldpte & PG_W)
				pmap->pm_stats.wired_count--;
			RECURSEEND;
			if ((oldpte & PG_SREF) == 0)
				rmpage(pmap, pteptr);
			else
				panic("pmap_enter on PG_SREF page");
			RECURSESTART;
		}

		s = splvm();
		pv = get_pventry();
		if (pv == NULL) {
			if (flags & PMAP_CANFAIL) {
				splx(s);
				RECURSEEND;
				return ENOMEM;
			}
			panic("pmap_enter: could not allocate pv_entry");
		}
		pv->pv_va = v;
		pv->pv_pmap = pmap;
		pv->pv_next = pg->mdpage.pv_head;
		pg->mdpage.pv_head = pv;
		splx(s);
		pmap->pm_stats.resident_count++;
		if (newpte & PG_W)
			pmap->pm_stats.wired_count++;
	}

	if (flags & PROT_READ) {
		pg->mdpage.pv_attr |= PG_V;
		newpte |= PG_V;
	}
	if (flags & PROT_WRITE)
		pg->mdpage.pv_attr |= PG_M;

	if (flags & PMAP_WIRED)
		newpte |= PG_V; /* Not allowed to be invalid */

	mapin8(pteptr, newpte);
	RECURSEEND;

	mtpr(0, PR_TBIA); /* Always; safety belt */
	return 0;
}

vaddr_t
pmap_map(vaddr_t va, paddr_t pstart, paddr_t pend, int prot)
{
	vaddr_t count;
	pt_entry_t *pentry;

	PMDEBUG(("pmap_map: virt %lx, pstart %lx, pend %lx, Sysmap %p\n",
	    va, pstart, pend, Sysmap));

	pstart &= 0x7fffffffUL;
	pend &= 0x7fffffffUL;
	va &= 0x7fffffffUL;
	pentry = Sysmap + vax_btop(va);
	for (count = pstart; count < pend; count += VAX_NBPG) {
		*pentry++ = vax_btop(count) | PG_V |
		    (prot & PROT_WRITE ? PG_KW : PG_KR);
	}
	return va + (count - pstart) + KERNBASE;
}

boolean_t
pmap_extract(struct pmap *pmap, vaddr_t va, paddr_t *pap)
{
	pt_entry_t *pte;
	ulong sva;

	PMDEBUG(("pmap_extract: pmap %p, va %lx",pmap, va));

	sva = PG_PFNUM(va);
	if (va & KERNBASE) {
		if (sva >= sysptsize || (Sysmap[sva] & PG_V) == 0)
			goto fail;
		*pap = ((Sysmap[sva] & PG_FRAME) << VAX_PGSHIFT) |
		    (va & VAX_PGOFSET);
		PMDEBUG((" -> pa %lx\n", *pap));
		return TRUE;
	}

	if (va < 0x40000000) {
		if (sva >= pmap->pm_p0lr)
			goto fail;
		pte = pmap->pm_p0br;
	} else {
		if (sva < pmap->pm_p1lr)
			goto fail;
		pte = pmap->pm_p1br;
	}
	/*
	 * Since the PTE tables are sparsely allocated, make sure the page
	 * table page actually exists before dereferencing the pte itself.
	 */
	if ((*kvtopte((vaddr_t)&pte[sva]) & PG_V) && (pte[sva] & PG_V)) {
		*pap = ((pte[sva] & PG_FRAME) << VAX_PGSHIFT) |
		    (va & VAX_PGOFSET);
		PMDEBUG((" -> pa %lx\n", *pap));
		return TRUE;
	}
	
fail:
	PMDEBUG((" -> no mapping\n"));
	return FALSE;
}

/*
 * Sets protection for a given region to prot. If prot == none then
 * unmap region. pmap_remove is implemented as pmap_protect with
 * protection none.
 */
void
pmap_protect(struct pmap *pmap, vaddr_t start, vaddr_t end, vm_prot_t prot)
{
	pt_entry_t *pt, *pts, *ptd;
	pt_entry_t pr, lr;

	PMDEBUG(("pmap_protect: pmap %p, start %lx, end %lx, prot %x\n",
	    pmap, start, end,prot));

	RECURSESTART;

	switch (SEGTYPE(start)) {
	case SYSSEG:
		pt = Sysmap;
#ifdef DIAGNOSTIC
		if (PG_PFNUM(end) > mfpr(PR_SLR))
			panic("pmap_protect: outside SLR: %lx", end);
#endif
		start &= ~KERNBASE;
		end &= ~KERNBASE;
		pr = (prot & PROT_WRITE ? PG_KW : PG_KR);
		break;

	case P1SEG:
		if (vax_btop(end - 0x40000000) <= pmap->pm_p1lr) {
			RECURSEEND;
			return;
		}
		if (vax_btop(start - 0x40000000) < pmap->pm_p1lr)
			start = pmap->pm_p1lr * VAX_NBPG;
		pt = pmap->pm_p1br;
		start &= 0x3fffffff;
		end = (end == KERNBASE ? 0x40000000 : end & 0x3fffffff);
		pr = (prot & PROT_WRITE ? PG_RW : PG_RO);
		break;

	case P0SEG:
		lr = pmap->pm_p0lr;

		/* Anything to care about at all? */
		if (vax_btop(start) > lr) {
			RECURSEEND;
			return;
		}
		if (vax_btop(end) > lr)
			end = lr * VAX_NBPG;
		pt = pmap->pm_p0br;
		pr = (prot & PROT_WRITE ? PG_RW : PG_RO);
		break;
	default:
		panic("unsupported segtype: %d", (int)SEGTYPE(start));
	}

	pts = &pt[start >> VAX_PGSHIFT];
	ptd = &pt[end >> VAX_PGSHIFT];
#ifdef DEBUG
	if (((int)pts - (int)pt) & 7)
		panic("pmap_remove: pts not even");
	if (((int)ptd - (int)pt) & 7)
		panic("pmap_remove: ptd not even");
#endif

	while (pts < ptd) {
		if ((*kvtopte((vaddr_t)pts) & PG_FRAME) != 0 && *pts != PG_NV) {
			if (prot == PROT_NONE) {
				pmap->pm_stats.resident_count--;
				if ((*pts & PG_W))
					pmap->pm_stats.wired_count--;
				RECURSEEND;
				if ((*pts & PG_SREF) == 0)
					rmpage(pmap, pts);
				RECURSESTART;
				bzero(pts, sizeof(pt_entry_t) * LTOHPN);
				if (pt != Sysmap) {
					if (ptpinuse(pts) == 0)
						rmptep(pts);
				}
			} else {
				pts[0] = (pts[0] & ~PG_PROT) | pr;
				pts[1] = (pts[1] & ~PG_PROT) | pr;
				pts[2] = (pts[2] & ~PG_PROT) | pr;
				pts[3] = (pts[3] & ~PG_PROT) | pr;
				pts[4] = (pts[4] & ~PG_PROT) | pr;
				pts[5] = (pts[5] & ~PG_PROT) | pr;
				pts[6] = (pts[6] & ~PG_PROT) | pr;
				pts[7] = (pts[7] & ~PG_PROT) | pr;
			}
		}
		pts += LTOHPN;
	}
	RECURSEEND;
	mtpr(0,PR_TBIA);
}

/*
 * Called from interrupt vector routines if we get a page invalid fault.
 * Note: the save mask must be or'ed with 0x3f for this function.
 * Returns 0 if normal call, 1 if CVAX bug detected.
 */
int pmap_simulref(int, vaddr_t);
int
pmap_simulref(int bits, vaddr_t va)
{
	pt_entry_t *pte;
	struct vm_page *pg;
	paddr_t	pa;

	PMDEBUG(("pmap_simulref: bits %x addr %x\n", bits, va));
#ifdef DEBUG
	if (bits & 1)
		panic("pte trans len");
#endif
	/* Set address to logical page boundary */
	va &= ~PGOFSET;

	if (va & KERNBASE) {
		pte = kvtopte(va);
		pa = (paddr_t)pte & ~KERNBASE;
	} else {
		if (va < 0x40000000)
			pte = (pt_entry_t *)mfpr(PR_P0BR);
		else
			pte = (pt_entry_t *)mfpr(PR_P1BR);
		pte += PG_PFNUM(va);
		if (bits & 2) { /* PTE reference */
			pte = kvtopte(vax_trunc_page(pte));
			if (pte[0] == 0) /* Check for CVAX bug */
				return 1;	
			pa = (paddr_t)pte & ~KERNBASE;
		} else
			pa = (Sysmap[PG_PFNUM(pte)] & PG_FRAME) << VAX_PGSHIFT;
	}

	pte[0] |= PG_V;
	pte[1] |= PG_V;
	pte[2] |= PG_V;
	pte[3] |= PG_V;
	pte[4] |= PG_V;
	pte[5] |= PG_V;
	pte[6] |= PG_V;
	pte[7] |= PG_V;

	pa = trunc_page(pa);
	pg = PHYS_TO_VM_PAGE(pa);
	if (pg != NULL) {
		pg->mdpage.pv_attr |= PG_V; /* Referenced */
		if (bits & 4)	/* (will be) modified. XXX page tables  */
			pg->mdpage.pv_attr |= PG_M;
	}
	return 0;
}

/*
 * Checks if page is referenced; returns true or false depending on result.
 */
boolean_t
pmap_is_referenced(struct vm_page *pg)
{
	PMDEBUG(("pmap_is_referenced: pg %p pv_attr %x\n",
	    pg, pg->mdpage.pv_attr));

	if (pg->mdpage.pv_attr & PG_V)
		return 1;

	return 0;
}

/*
 * Clears valid bit in all ptes referenced to this physical page.
 */
boolean_t
pmap_clear_reference(struct vm_page *pg)
{
	struct pv_entry *pv;
	pt_entry_t *pte;
	boolean_t ref = FALSE;
	int s;

	PMDEBUG(("pmap_clear_reference: pg %p\n", pg));

	if (pg->mdpage.pv_attr & PG_V)
		ref = TRUE;

	pg->mdpage.pv_attr &= ~PG_V;

	RECURSESTART;
	s = splvm();
	for (pv = pg->mdpage.pv_head; pv != NULL; pv = pv->pv_next) {
		pte = vaddrtopte(pv);
		pte[0] &= ~PG_V;
		pte[1] &= ~PG_V;
		pte[2] &= ~PG_V;
		pte[3] &= ~PG_V;
		pte[4] &= ~PG_V;
		pte[5] &= ~PG_V;
		pte[6] &= ~PG_V;
		pte[7] &= ~PG_V;
	}
	splx(s);

	RECURSEEND;
	mtpr(0, PR_TBIA);
	return ref;
}

/*
 * Checks if page is modified; returns true or false depending on result.
 */
boolean_t
pmap_is_modified(struct vm_page *pg)
{
	struct pv_entry *pv;
	pt_entry_t *pte;
	boolean_t rv = FALSE;
	int s;

	PMDEBUG(("pmap_is_modified: pg %p pv_attr %x\n",
	    pg, pg->mdpage.pv_attr));

	if (pg->mdpage.pv_attr & PG_M)
		return TRUE;

	s = splvm();
	for (pv = pg->mdpage.pv_head; pv != NULL; pv = pv->pv_next) {
		pte = vaddrtopte(pv);
		if ((pte[0] | pte[1] | pte[2] | pte[3] | pte[4] | pte[5] |
		     pte[6] | pte[7]) & PG_M) {
			rv = TRUE;
			break;
		}
	}
	splx(s);

	return rv;
}

/*
 * Clears modify bit in all ptes referenced to this physical page.
 */
boolean_t
pmap_clear_modify(struct vm_page *pg)
{
	struct pv_entry *pv;
	pt_entry_t *pte;
	boolean_t rv = FALSE;
	int s;

	PMDEBUG(("pmap_clear_modify: pg %p\n", pg));

	if (pg->mdpage.pv_attr & PG_M)
		rv = TRUE;
	pg->mdpage.pv_attr &= ~PG_M;

	s = splvm();
	for (pv = pg->mdpage.pv_head; pv != NULL; pv = pv->pv_next) {
		pte = vaddrtopte(pv);
		if ((pte[0] | pte[1] | pte[2] | pte[3] | pte[4] | pte[5] |
		     pte[6] | pte[7]) & PG_M) {
			rv = TRUE;

			pte[0] &= ~PG_M;
			pte[1] &= ~PG_M;
			pte[2] &= ~PG_M;
			pte[3] &= ~PG_M;
			pte[4] &= ~PG_M;
			pte[5] &= ~PG_M;
			pte[6] &= ~PG_M;
			pte[7] &= ~PG_M;
		}
	}
	splx(s);

	return rv;
}

/*
 * Lower the permission for all mappings to a given page.
 * Lower permission can only mean setting protection to either read-only
 * or none; where none is unmapping of the page.
 */
void
pmap_page_protect(struct vm_page *pg, vm_prot_t prot)
{
	pt_entry_t *pte;
	struct	pv_entry *pv, *npv;
	int	s;

	PMDEBUG(("pmap_page_protect: pg %p, prot %x\n", pg, prot));

	if (pg->mdpage.pv_head == NULL)
		return;

	if (prot == PROT_MASK) /* 'cannot happen' */
		return;

	RECURSESTART;
	s = splvm();
	if (prot == PROT_NONE) {
		npv = pg->mdpage.pv_head;
		pg->mdpage.pv_head = NULL;
		while ((pv = npv) != NULL) {
			npv = pv->pv_next;
			pte = vaddrtopte(pv);
			pv->pv_pmap->pm_stats.resident_count--;
			if (pte[0] & PG_W)
				pv->pv_pmap->pm_stats.wired_count--;
			if ((pg->mdpage.pv_attr & (PG_V|PG_M)) != (PG_V|PG_M))
				pg->mdpage.pv_attr |= 
				    pte[0] | pte[1] | pte[2] | pte[3] |
				    pte[4] | pte[5] | pte[6] | pte[7];
			bzero(pte, sizeof(pt_entry_t) * LTOHPN);
			if (pv->pv_pmap != pmap_kernel()) {
				if (ptpinuse(pte) == 0)
					rmptep(pte);
			}
			free_pventry(pv);
		}
	} else { /* read-only */
		for (pv = pg->mdpage.pv_head; pv != NULL; pv = pv->pv_next) {
			pt_entry_t pr;

			pte = vaddrtopte(pv);
			pr = (vaddr_t)pte < ptemapstart ? 
			    PG_KR : PG_RO;

			pte[0] = (pte[0] & ~PG_PROT) | pr;
			pte[1] = (pte[1] & ~PG_PROT) | pr;
			pte[2] = (pte[2] & ~PG_PROT) | pr;
			pte[3] = (pte[3] & ~PG_PROT) | pr;
			pte[4] = (pte[4] & ~PG_PROT) | pr;
			pte[5] = (pte[5] & ~PG_PROT) | pr;
			pte[6] = (pte[6] & ~PG_PROT) | pr;
			pte[7] = (pte[7] & ~PG_PROT) | pr;
		}
	}
	splx(s);
	RECURSEEND;
	mtpr(0, PR_TBIA);
}

void
pmap_remove_pcb(struct pmap *pm, struct pcb *thispcb)
{
	struct pcb *pcb, **pcbp;

	PMDEBUG(("pmap_remove_pcb pm %p pcb %p\n", pm, thispcb));

	for (pcbp = &pm->pm_pcbs; (pcb = *pcbp) != NULL;
	    pcbp = &pcb->pcb_pmnext) {
#ifdef DIAGNOSTIC
		if (pcb->pcb_pm != pm)
			panic("%s: pcb %p (pm %p) not owned by pmap %p",
			    __func__, pcb, pcb->pcb_pm, pm);
#endif
		if (pcb == thispcb) {
			*pcbp = pcb->pcb_pmnext;
			thispcb->pcb_pm = NULL;
			return;
		}
	}
#ifdef DIAGNOSTIC
	panic("%s: pmap %p: pcb %p not in list", __func__, pm, thispcb);
#endif
}

/*
 * Activate the address space for the specified process.
 * Note that if the process to activate is the current process, then
 * the processor internal registers must also be loaded; otherwise
 * the current process will have wrong pagetables.
 */
void
pmap_activate(struct proc *p)
{
	struct pcb *pcb = &p->p_addr->u_pcb;
	struct pmap *pmap = p->p_vmspace->vm_map.pmap;
	int s;

	PMDEBUG(("pmap_activate: p %p pcb %p pm %p (%08x %08x %08x %08x)\n",
	    p, pcb, pmap, pmap->pm_p0br, pmap->pm_p0lr, pmap->pm_p1br,
	    pmap->pm_p1lr));

	pcb->P0BR = pmap->pm_p0br;
	pcb->P0LR = pmap->pm_p0lr | AST_PCB;
	pcb->P1BR = pmap->pm_p1br;
	pcb->P1LR = pmap->pm_p1lr;

	if (pcb->pcb_pm != pmap) {
		s = splsched();
		if (pcb->pcb_pm != NULL)
			pmap_remove_pcb(pcb->pcb_pm, pcb);
		pcb->pcb_pmnext = pmap->pm_pcbs;
		pmap->pm_pcbs = pcb;
		pcb->pcb_pm = pmap;
		splx(s);
	}

	if (p == curproc) {
		mtpr((register_t)pmap->pm_p0br, PR_P0BR);
		mtpr(pmap->pm_p0lr | AST_PCB, PR_P0LR);
		mtpr((register_t)pmap->pm_p1br, PR_P1BR);
		mtpr(pmap->pm_p1lr, PR_P1LR);
		mtpr(0, PR_TBIA);
	}
}

void
pmap_deactivate(struct proc *p)
{
	struct pcb *pcb = &p->p_addr->u_pcb;
	struct pmap *pmap = p->p_vmspace->vm_map.pmap;
	int s;

	PMDEBUG(("pmap_deactivate: p %p pcb %p\n", p, pcb));

	if (pcb->pcb_pm != NULL) {
		s = splsched();
#ifdef DIAGNOSTIC
		if (pcb->pcb_pm != pmap)
			panic("%s: proc %p pcb %p not owned by pmap %p",
			    __func__, p, pcb, pmap);
#endif
		pmap_remove_pcb(pmap, pcb);
		splx(s);
	}
}
