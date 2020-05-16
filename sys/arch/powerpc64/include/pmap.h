#ifndef _MACHINE_PMAP_H_
#define _MACHINE_PMAP_H_

typedef struct pmap *pmap_t;

struct vm_page_md {
};

#define VM_MDPAGE_INIT(pg) do { } while (0)

#define pmap_kernel()	((struct pmap *)NULL)
#define pmap_resident_count(pm) 0
#define pmap_unuse_final(p)
#define pmap_remove_holes(vm)
#define pmap_update(pm)

#endif /* _MACHINE_PMAP_H_ */
