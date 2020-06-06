#ifndef _MACHINE_PMAP_H_
#define _MACHINE_PMAP_H_

struct pmap {
	struct pmap_statistics	pm_stats;
};

typedef struct pmap *pmap_t;

#define PG_PMAP_MOD	PG_PMAP0
#define PG_PMAP_REF	PG_PMAP1
#define PG_PMAP_EXE	PG_PMAP2
#define PG_PMAP_UC	PG_PMAP3

#define PMAP_CACHE_DEFAULT	0
#define PMAP_CACHE_CI		1	/* cache inhibit */
#define PMAP_CACHE_WB		3	/* write-back cached */

/*
 * MD flags that we use for pmap_enter (in the pa):
 */
#define PMAP_PA_MASK	~((paddr_t)PAGE_MASK) /* to remove the flags */
#define PMAP_NOCACHE	0x1		/* map uncached */

struct vm_page_md {
};

#define VM_MDPAGE_INIT(pg) do { } while (0)

extern struct pmap kernel_pmap_store;

#define pmap_kernel()	(&kernel_pmap_store)
#define pmap_resident_count(pm) 0
#define pmap_unuse_final(p)
#define pmap_remove_holes(vm)
#define pmap_update(pm)

void	pmap_bootstrap(void);

#endif /* _MACHINE_PMAP_H_ */
