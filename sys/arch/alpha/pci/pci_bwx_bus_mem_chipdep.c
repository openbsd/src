/* $OpenBSD: pci_bwx_bus_mem_chipdep.c,v 1.3 2001/10/26 01:28:06 nate Exp $ */
/* $NetBSD: pcs_bus_mem_common.c,v 1.15 1996/12/02 22:19:36 cgd Exp $ */

/*
 * Copyright (c) 1995, 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/*
 * Common PCI Chipset "bus I/O" functions, for chipsets which have to
 * deal with only a single PCI interface chip in a machine.
 *
 * uses:
 *	CHIP		name of the 'chip' it's being compiled for.
 *	CHIP_MEM_BASE   Mem space base to use.
 *	CHIP_MEM_EX_STORE
 *			If defined, device-provided static storage area
 *			for the memory space extent.  If this is
 *			defined, CHIP_MEM_EX_STORE_SIZE must also be
 *			defined.  If this is not defined, a static area
 *			will be declared.
 *	CHIP_MEM_EX_STORE_SIZE
 *			Size of the device-provided static storage area
 *			for the memory space extent.
 */

#include <sys/extent.h>
#include <machine/bwx.h>

#define	__C(A,B)	__CONCAT(A,B)
#define	__S(S)		__STRING(S)

/* mapping/unmapping */
int		__C(CHIP,_mem_map) __P((void *, bus_addr_t, bus_size_t, int,
		    bus_space_handle_t *));
void		__C(CHIP,_mem_unmap) __P((void *, bus_space_handle_t,
		    bus_size_t));
int		__C(CHIP,_mem_subregion) __P((void *, bus_space_handle_t,
		    bus_size_t, bus_size_t, bus_space_handle_t *));

/* allocation/deallocation */
int		__C(CHIP,_mem_alloc) __P((void *, bus_addr_t, bus_addr_t,
		    bus_size_t, bus_size_t, bus_addr_t, int, bus_addr_t *,
                    bus_space_handle_t *));
void		__C(CHIP,_mem_free) __P((void *, bus_space_handle_t,
		    bus_size_t));

/* barrier */
inline void	__C(CHIP,_mem_barrier) __P((void *, bus_space_handle_t,
		    bus_size_t, bus_size_t, int));

/* read (single) */
inline u_int8_t	__C(CHIP,_mem_read_1) __P((void *, bus_space_handle_t,
		    bus_size_t));
inline u_int16_t __C(CHIP,_mem_read_2) __P((void *, bus_space_handle_t,
		    bus_size_t));
inline u_int32_t __C(CHIP,_mem_read_4) __P((void *, bus_space_handle_t,
		    bus_size_t));
inline u_int64_t __C(CHIP,_mem_read_8) __P((void *, bus_space_handle_t,
		    bus_size_t));

/* read multiple */
void		__C(CHIP,_mem_read_multi_1) __P((void *, bus_space_handle_t,
		    bus_size_t, u_int8_t *, bus_size_t));
void		__C(CHIP,_mem_read_multi_2) __P((void *, bus_space_handle_t,
		    bus_size_t, u_int16_t *, bus_size_t));
void		__C(CHIP,_mem_read_multi_4) __P((void *, bus_space_handle_t,
		    bus_size_t, u_int32_t *, bus_size_t));
void		__C(CHIP,_mem_read_multi_8) __P((void *, bus_space_handle_t,
		    bus_size_t, u_int64_t *, bus_size_t));

/* read region */
void		__C(CHIP,_mem_read_region_1) __P((void *, bus_space_handle_t,
		    bus_size_t, u_int8_t *, bus_size_t));
void		__C(CHIP,_mem_read_region_2) __P((void *, bus_space_handle_t,
		    bus_size_t, u_int16_t *, bus_size_t));
void		__C(CHIP,_mem_read_region_4) __P((void *, bus_space_handle_t,
		    bus_size_t, u_int32_t *, bus_size_t));
void		__C(CHIP,_mem_read_region_8) __P((void *, bus_space_handle_t,
		    bus_size_t, u_int64_t *, bus_size_t));

/* write (single) */
inline void	__C(CHIP,_mem_write_1) __P((void *, bus_space_handle_t,
		    bus_size_t, u_int8_t));
inline void	__C(CHIP,_mem_write_2) __P((void *, bus_space_handle_t,
		    bus_size_t, u_int16_t));
inline void	__C(CHIP,_mem_write_4) __P((void *, bus_space_handle_t,
		    bus_size_t, u_int32_t));
inline void	__C(CHIP,_mem_write_8) __P((void *, bus_space_handle_t,
		    bus_size_t, u_int64_t));

/* write multiple */
void		__C(CHIP,_mem_write_multi_1) __P((void *, bus_space_handle_t,
		    bus_size_t, const u_int8_t *, bus_size_t));
void		__C(CHIP,_mem_write_multi_2) __P((void *, bus_space_handle_t,
		    bus_size_t, const u_int16_t *, bus_size_t));
void		__C(CHIP,_mem_write_multi_4) __P((void *, bus_space_handle_t,
		    bus_size_t, const u_int32_t *, bus_size_t));
void		__C(CHIP,_mem_write_multi_8) __P((void *, bus_space_handle_t,
		    bus_size_t, const u_int64_t *, bus_size_t));

/* write region */
void		__C(CHIP,_mem_write_region_1) __P((void *, bus_space_handle_t,
		    bus_size_t, const u_int8_t *, bus_size_t));
void		__C(CHIP,_mem_write_region_2) __P((void *, bus_space_handle_t,
		    bus_size_t, const u_int16_t *, bus_size_t));
void		__C(CHIP,_mem_write_region_4) __P((void *, bus_space_handle_t,
		    bus_size_t, const u_int32_t *, bus_size_t));
void		__C(CHIP,_mem_write_region_8) __P((void *, bus_space_handle_t,
		    bus_size_t, const u_int64_t *, bus_size_t));

/* set multiple */
void		__C(CHIP,_mem_set_multi_1) __P((void *, bus_space_handle_t,
		    bus_size_t, u_int8_t, bus_size_t));
void		__C(CHIP,_mem_set_multi_2) __P((void *, bus_space_handle_t,
		    bus_size_t, u_int16_t, bus_size_t));
void		__C(CHIP,_mem_set_multi_4) __P((void *, bus_space_handle_t,
		    bus_size_t, u_int32_t, bus_size_t));
void		__C(CHIP,_mem_set_multi_8) __P((void *, bus_space_handle_t,
		    bus_size_t, u_int64_t, bus_size_t));

/* set region */
void		__C(CHIP,_mem_set_region_1) __P((void *, bus_space_handle_t,
		    bus_size_t, u_int8_t, bus_size_t));
void		__C(CHIP,_mem_set_region_2) __P((void *, bus_space_handle_t,
		    bus_size_t, u_int16_t, bus_size_t));
void		__C(CHIP,_mem_set_region_4) __P((void *, bus_space_handle_t,
		    bus_size_t, u_int32_t, bus_size_t));
void		__C(CHIP,_mem_set_region_8) __P((void *, bus_space_handle_t,
		    bus_size_t, u_int64_t, bus_size_t));

/* copy */
void		__C(CHIP,_mem_copy_1) __P((void *, bus_space_handle_t,
		    bus_size_t, bus_space_handle_t, bus_size_t, bus_size_t));
void		__C(CHIP,_mem_copy_2) __P((void *, bus_space_handle_t,
		    bus_size_t, bus_space_handle_t, bus_size_t, bus_size_t));
void		__C(CHIP,_mem_copy_4) __P((void *, bus_space_handle_t,
		    bus_size_t, bus_space_handle_t, bus_size_t, bus_size_t));
void		__C(CHIP,_mem_copy_8) __P((void *, bus_space_handle_t,
		    bus_size_t, bus_space_handle_t, bus_size_t, bus_size_t));

/* read multiple raw */
void		__C(CHIP,_mem_read_raw_multi_2) __P((void *,
		    bus_space_handle_t, bus_size_t, u_int8_t *, bus_size_t));
void		__C(CHIP,_mem_read_raw_multi_4) __P((void *,
		    bus_space_handle_t, bus_size_t, u_int8_t *, bus_size_t));
void		__C(CHIP,_mem_read_raw_multi_8) __P((void *,
		    bus_space_handle_t, bus_size_t, u_int8_t *, bus_size_t));

/* write multiple raw */
void		__C(CHIP,_mem_write_raw_multi_2) __P((void *,
		    bus_space_handle_t, bus_size_t, const u_int8_t *,
		    bus_size_t));
void		__C(CHIP,_mem_write_raw_multi_4) __P((void *,
		    bus_space_handle_t, bus_size_t, const u_int8_t *,
		    bus_size_t));
void		__C(CHIP,_mem_write_raw_multi_8) __P((void *,
		    bus_space_handle_t, bus_size_t, const u_int8_t *,
		    bus_size_t));

static long
    __C(CHIP,_mem_ex_storage)[EXTENT_FIXED_STORAGE_SIZE(8) / sizeof(long)];

static struct alpha_bus_space __C(CHIP,_mem_space) = {
	/* cookie */
	NULL,

	/* mapping/unmapping */
	__C(CHIP,_mem_map),
	__C(CHIP,_mem_unmap),
	__C(CHIP,_mem_subregion),

	/* allocation/deallocation */
	__C(CHIP,_mem_alloc),
	__C(CHIP,_mem_free),

	/* barrier */
	__C(CHIP,_mem_barrier),
	
	/* read (single) */
	__C(CHIP,_mem_read_1),
	__C(CHIP,_mem_read_2),
	__C(CHIP,_mem_read_4),
	__C(CHIP,_mem_read_8),
	
	/* read multiple */
	__C(CHIP,_mem_read_multi_1),
	__C(CHIP,_mem_read_multi_2),
	__C(CHIP,_mem_read_multi_4),
	__C(CHIP,_mem_read_multi_8),
	
	/* read region */
	__C(CHIP,_mem_read_region_1),
	__C(CHIP,_mem_read_region_2),
	__C(CHIP,_mem_read_region_4),
	__C(CHIP,_mem_read_region_8),
	
	/* write (single) */
	__C(CHIP,_mem_write_1),
	__C(CHIP,_mem_write_2),
	__C(CHIP,_mem_write_4),
	__C(CHIP,_mem_write_8),
	
	/* write multiple */
	__C(CHIP,_mem_write_multi_1),
	__C(CHIP,_mem_write_multi_2),
	__C(CHIP,_mem_write_multi_4),
	__C(CHIP,_mem_write_multi_8),
	
	/* write region */
	__C(CHIP,_mem_write_region_1),
	__C(CHIP,_mem_write_region_2),
	__C(CHIP,_mem_write_region_4),
	__C(CHIP,_mem_write_region_8),

	/* set multiple */
	__C(CHIP,_mem_set_multi_1),
	__C(CHIP,_mem_set_multi_2),
	__C(CHIP,_mem_set_multi_4),
	__C(CHIP,_mem_set_multi_8),
	
	/* set region */
	__C(CHIP,_mem_set_region_1),
	__C(CHIP,_mem_set_region_2),
	__C(CHIP,_mem_set_region_4),
	__C(CHIP,_mem_set_region_8),

	/* copy */
	__C(CHIP,_mem_copy_1),
	__C(CHIP,_mem_copy_2),
	__C(CHIP,_mem_copy_4),
	__C(CHIP,_mem_copy_8),

	/* read multiple raw */
	__C(CHIP,_mem_read_raw_multi_2),
	__C(CHIP,_mem_read_raw_multi_4),
	__C(CHIP,_mem_read_raw_multi_8),
	
	/* write multiple raw*/
	__C(CHIP,_mem_write_raw_multi_2),
	__C(CHIP,_mem_write_raw_multi_4),
	__C(CHIP,_mem_write_raw_multi_8),
};

bus_space_tag_t
__C(CHIP,_bus_mem_init)(v)
	void *v;
{
        bus_space_tag_t t = &__C(CHIP,_mem_space);
	struct extent *ex;

	t->abs_cookie = v;

	ex = extent_create(__S(__C(CHIP,_bus_dmem)), 0x0UL,
	    0xffffffffffffffffUL, M_DEVBUF,
	    (caddr_t)__C(CHIP,_mem_ex_storage),
	    sizeof(__C(CHIP,_mem_ex_storage)), EX_NOWAIT|EX_NOCOALESCE);

        CHIP_MEM_EXTENT(v) = ex;

	return (t);
}

int
__C(CHIP,_mem_map)(v, memaddr, memsize, cacheable, memhp)
	void *v;
	bus_addr_t memaddr;
	bus_size_t memsize;
	int cacheable;
	bus_space_handle_t *memhp;
{
	int error;

#ifdef EXTENT_DEBUG
	printf("mem: allocating 0x%lx to 0x%lx\n", memaddr,
	    memaddr + memsize - 1);
#endif  
	error = extent_alloc_region(CHIP_MEM_EXTENT(v), memaddr, memsize,
	    EX_NOWAIT | (CHIP_EX_MALLOC_SAFE(v) ? EX_MALLOCOK : 0));
	if (error) {
#ifdef EXTENT_DEBUG
		printf("mem: allocation failed (%d)\n", error);
		extent_print(CHIP_MEM_EXTENT(v));
#endif
		return (error);
	}

	*memhp = ALPHA_PHYS_TO_K0SEG(CHIP_MEM_SYS_START(v)) + memaddr;

	return (0);
}

void
__C(CHIP,_mem_unmap)(v, memh, memsize)
	void *v;
	bus_space_handle_t memh;
	bus_size_t memsize;
{
	bus_addr_t memaddr;
	int error;

#ifdef EXTENT_DEBUG
	printf("mem: freeing handle 0x%lx for 0x%lx\n", memh, memsize);
#endif
	memaddr = memh - ALPHA_PHYS_TO_K0SEG(CHIP_MEM_SYS_START(v));

#ifdef EXTENT_DEBUG
	"mem: freeing 0x%lx to 0x%lx\n", memaddr, memaddr + memsize - 1);
#endif
	error = extent_free(CHIP_MEM_EXTENT(v), memaddr, memsize,
	    EX_NOWAIT | (CHIP_EX_MALLOC_SAFE(v) ? EX_MALLOCOK : 0));
	if (error) {
		printf("%s: WARNING: could not unmap 0x%lx-0x%lx (error %d)\n",
		    __S(__C(CHIP,_mem_unmap)), memaddr, memaddr + memsize - 1,
		    error);
#ifdef EXTENT_DEBUG
	extent_print(CHIP_MEM_EXTENT(v));
#endif
	}
}

int
__C(CHIP,_mem_subregion)(v, memh, offset, size, nmemh)
	void *v;
	bus_space_handle_t memh, *nmemh;
	bus_size_t offset, size;
{

	*nmemh = memh + offset;
	return (0);
}

int
__C(CHIP,_mem_alloc)(v, rstart, rend, size, align, boundary, cacheable,
    addrp, bshp)
	void *v;
	bus_addr_t rstart, rend, *addrp;
	bus_size_t size, align, boundary;
	int cacheable;
	bus_space_handle_t *bshp;
{
	bus_addr_t memaddr;
	int error;

	/*
	 * Do the requested allocation.
	 */
#ifdef EXTENT_DEBUG
	printf("mem: allocating from 0x%lx to 0x%lx\n", rstart, rend);
#endif
	error = extent_alloc_subregion(CHIP_MEM_EXTENT(v), rstart, rend,
	    size, align, 0, boundary,
	    EX_FAST | EX_NOWAIT | (CHIP_EX_MALLOC_SAFE(v) ? EX_MALLOCOK : 0),
	    &memaddr);
	if (error) {
#ifdef EXTENT_DEBUG
		printf("mem: allocation failed (%d)\n", error);
		extent_print(CHIP_MEM_EXTENT(v));
#endif
	}

#ifdef EXTENT_DEBUG
	printf("mem: allocated 0x%lx to 0x%lx\n", memaddr, memaddr + size - 1);
#endif

	*addrp = memaddr;
	*bshp = ALPHA_PHYS_TO_K0SEG(CHIP_MEM_SYS_START(v)) + memaddr;

	return (0);
}

void
__C(CHIP,_mem_free)(v, bsh, size)
	void *v;
	bus_space_handle_t bsh;
	bus_size_t size;
{

	/* Unmap does all we need to do. */
	__C(CHIP,_mem_unmap)(v, bsh, size);
}

inline void
__C(CHIP,_mem_barrier)(v, h, o, l, f)
	void *v;
	bus_space_handle_t h;
	bus_size_t o, l;
	int f;
{

	if ((f & BUS_BARRIER_READ) != 0)
		alpha_mb();
	else if ((f & BUS_BARRIER_WRITE) != 0)
		alpha_wmb();
}

inline u_int8_t
__C(CHIP,_mem_read_1)(v, memh, off)
	void *v;
	bus_space_handle_t memh;
	bus_size_t off;
{
	bus_addr_t addr;

	addr = memh + off;
	alpha_mb();
	return (alpha_ldbu((u_int8_t *)addr));
}

inline u_int16_t
__C(CHIP,_mem_read_2)(v, memh, off)
	void *v;
	bus_space_handle_t memh;
	bus_size_t off;
{
	bus_addr_t addr;

	addr = memh + off;
#ifdef DIAGNOSTIC
	if (addr & 1)
		panic(__S(__C(CHIP,_mem_read_2)) ": addr 0x%lx not aligned",
		    addr);
#endif
	alpha_mb();
	return (alpha_ldwu((u_int16_t *)addr));
}

inline u_int32_t
__C(CHIP,_mem_read_4)(v, memh, off)
	void *v;
	bus_space_handle_t memh;
	bus_size_t off;
{
	bus_addr_t addr;

	addr = memh + off;
#ifdef DIAGNOSTIC
	if (addr & 3)
		panic(__S(__C(CHIP,_mem_read_4)) ": addr 0x%lx not aligned",
		    addr);
#endif
	alpha_mb();
	return (*(u_int32_t *)addr);
}

inline u_int64_t
__C(CHIP,_mem_read_8)(v, memh, off)
	void *v;
	bus_space_handle_t memh;
	bus_size_t off;
{

	alpha_mb();

	/* XXX XXX XXX */
	panic("%s not implemented", __S(__C(CHIP,_mem_read_8)));
}

#define CHIP_mem_read_multi_N(BYTES,TYPE)				\
void									\
__C(__C(CHIP,_mem_read_multi_),BYTES)(v, h, o, a, c)			\
	void *v;							\
	bus_space_handle_t h;						\
	bus_size_t o, c;						\
	TYPE *a;							\
{									\
									\
	while (c-- > 0) {						\
		__C(CHIP,_mem_barrier)(v, h, o, sizeof *a,		\
		    BUS_BARRIER_READ);					\
		*a++ = __C(__C(CHIP,_mem_read_),BYTES)(v, h, o);	\
	}								\
}
CHIP_mem_read_multi_N(1,u_int8_t)
CHIP_mem_read_multi_N(2,u_int16_t)
CHIP_mem_read_multi_N(4,u_int32_t)
CHIP_mem_read_multi_N(8,u_int64_t)

#define CHIP_mem_read_region_N(BYTES,TYPE)				\
void									\
__C(__C(CHIP,_mem_read_region_),BYTES)(v, h, o, a, c)			\
	void *v;							\
	bus_space_handle_t h;						\
	bus_size_t o, c;						\
	TYPE *a;							\
{									\
									\
	while (c-- > 0) {						\
		*a++ = __C(__C(CHIP,_mem_read_),BYTES)(v, h, o);	\
		o += sizeof *a;						\
	}								\
}
CHIP_mem_read_region_N(1,u_int8_t)
CHIP_mem_read_region_N(2,u_int16_t)
CHIP_mem_read_region_N(4,u_int32_t)
CHIP_mem_read_region_N(8,u_int64_t)

inline void
__C(CHIP,_mem_write_1)(v, memh, off, val)
	void *v;
	bus_space_handle_t memh;
	bus_size_t off;
	u_int8_t val;
{
	bus_addr_t addr;

	addr = memh + off;
	alpha_stb((u_int8_t *)addr, val);
	alpha_mb();
}

inline void
__C(CHIP,_mem_write_2)(v, memh, off, val)
	void *v;
	bus_space_handle_t memh;
	bus_size_t off;
	u_int16_t val;
{
	bus_addr_t addr;

	addr = memh + off;
#ifdef DIAGNOSTIC
	if (addr & 1)
		panic(__S(__C(CHIP,_mem_write_2)) ": addr 0x%lx not aligned",
		   addr);
#endif
	alpha_stw((u_int16_t *)addr, val);
	alpha_mb();
}

inline void
__C(CHIP,_mem_write_4)(v, memh, off, val)
	void *v;
	bus_space_handle_t memh;
	bus_size_t off;
	u_int32_t val;
{
	bus_addr_t addr;

	addr = memh + off;
#ifdef DIAGNOSTIC
	if (addr & 3)
		panic(__S(__C(CHIP,_mem_write_4)) ": addr 0x%lx not aligned",
		    addr);
#endif
	*(u_int32_t *)addr = val;
	alpha_mb();
}

inline void
__C(CHIP,_mem_write_8)(v, memh, off, val)
	void *v;
	bus_space_handle_t memh;
	bus_size_t off;
	u_int64_t val;
{

	/* XXX XXX XXX */
	panic("%s not implemented", __S(__C(CHIP,_mem_write_8)));
	alpha_mb();
}

#define CHIP_mem_write_multi_N(BYTES,TYPE)				\
void									\
__C(__C(CHIP,_mem_write_multi_),BYTES)(v, h, o, a, c)			\
	void *v;							\
	bus_space_handle_t h;						\
	bus_size_t o, c;						\
	const TYPE *a;							\
{									\
									\
	while (c-- > 0) {						\
		__C(__C(CHIP,_mem_write_),BYTES)(v, h, o, *a++);	\
		__C(CHIP,_mem_barrier)(v, h, o, sizeof *a,		\
		    BUS_BARRIER_WRITE);					\
	}								\
}
CHIP_mem_write_multi_N(1,u_int8_t)
CHIP_mem_write_multi_N(2,u_int16_t)
CHIP_mem_write_multi_N(4,u_int32_t)
CHIP_mem_write_multi_N(8,u_int64_t)

#define CHIP_mem_write_region_N(BYTES,TYPE)				\
void									\
__C(__C(CHIP,_mem_write_region_),BYTES)(v, h, o, a, c)			\
	void *v;							\
	bus_space_handle_t h;						\
	bus_size_t o, c;						\
	const TYPE *a;							\
{									\
									\
	while (c-- > 0) {						\
		__C(__C(CHIP,_mem_write_),BYTES)(v, h, o, *a++);	\
		o += sizeof *a;						\
	}								\
}
CHIP_mem_write_region_N(1,u_int8_t)
CHIP_mem_write_region_N(2,u_int16_t)
CHIP_mem_write_region_N(4,u_int32_t)
CHIP_mem_write_region_N(8,u_int64_t)

#define CHIP_mem_set_multi_N(BYTES,TYPE)				\
void									\
__C(__C(CHIP,_mem_set_multi_),BYTES)(v, h, o, val, c)			\
	void *v;							\
	bus_space_handle_t h;						\
	bus_size_t o, c;						\
	TYPE val;							\
{									\
									\
	while (c-- > 0) {						\
		__C(__C(CHIP,_mem_write_),BYTES)(v, h, o, val);		\
		__C(CHIP,_mem_barrier)(v, h, o, sizeof val,		\
		    BUS_BARRIER_WRITE);					\
	}								\
}
CHIP_mem_set_multi_N(1,u_int8_t)
CHIP_mem_set_multi_N(2,u_int16_t)
CHIP_mem_set_multi_N(4,u_int32_t)
CHIP_mem_set_multi_N(8,u_int64_t)

#define CHIP_mem_set_region_N(BYTES,TYPE)				\
void									\
__C(__C(CHIP,_mem_set_region_),BYTES)(v, h, o, val, c)			\
	void *v;							\
	bus_space_handle_t h;						\
	bus_size_t o, c;						\
	TYPE val;							\
{									\
									\
	while (c-- > 0) {						\
		__C(__C(CHIP,_mem_write_),BYTES)(v, h, o, val);		\
		o += sizeof val;					\
	}								\
}
CHIP_mem_set_region_N(1,u_int8_t)
CHIP_mem_set_region_N(2,u_int16_t)
CHIP_mem_set_region_N(4,u_int32_t)
CHIP_mem_set_region_N(8,u_int64_t)

#define	CHIP_mem_copy_N(BYTES)						\
void									\
__C(__C(CHIP,_mem_copy_),BYTES)(v, h1, o1, h2, o2, c)			\
	void *v;							\
	bus_space_handle_t h1, h2;					\
	bus_size_t o1, o2, c;						\
{									\
	bus_size_t i, o;						\
									\
	if ((h1 >> 63) != 0 && (h2 >> 63) != 0) {			\
		bcopy((void *)(h1 + o1), (void *)(h2 + o2), c * BYTES);	\
		return;							\
	}								\
									\
	/* Circumvent a common case of overlapping problems */		\
	if (h1 == h2 && o2 > o1)					\
		for (i = 0, o = (c - 1) * BYTES; i < c; i++, o -= BYTES)\
			__C(__C(CHIP,_mem_write_),BYTES)(v, h2, o2 + o,	\
			    __C(__C(CHIP,_mem_read_),BYTES)(v, h1, o1 + o));\
	else								\
		for (i = 0, o = 0; i < c; i++, o += BYTES)		\
			__C(__C(CHIP,_mem_write_),BYTES)(v, h2, o2 + o,	\
			    __C(__C(CHIP,_mem_read_),BYTES)(v, h1, o1 + o));\
}
CHIP_mem_copy_N(1)
CHIP_mem_copy_N(2)
CHIP_mem_copy_N(4)
CHIP_mem_copy_N(8)

#define CHIP_mem_read_raw_multi_N(BYTES,TYPE)				\
void									\
__C(__C(CHIP,_mem_read_raw_multi_),BYTES)(v, h, o, a, c)			\
	void *v;							\
	bus_space_handle_t h;						\
	bus_size_t o, c;						\
	u_int8_t *a;							\
{									\
	TYPE temp;							\
	int i;								\
									\
	while (c > 0) {							\
		__C(CHIP,_mem_barrier)(v, h, o, BYTES, BUS_BARRIER_READ); \
		temp = __C(__C(CHIP,_mem_read_),BYTES)(v, h, o);	\
		i = MIN(c, BYTES);					\
		c -= i;							\
		while (i--) {						\
			*a++ = temp & 0xff;				\
			temp >>= 8;					\
		}							\
	}								\
}
CHIP_mem_read_raw_multi_N(2,u_int16_t)
CHIP_mem_read_raw_multi_N(4,u_int32_t)
CHIP_mem_read_raw_multi_N(8,u_int64_t)

#define CHIP_mem_write_raw_multi_N(BYTES,TYPE)				\
void									\
__C(__C(CHIP,_mem_write_raw_multi_),BYTES)(v, h, o, a, c)		\
	void *v;							\
	bus_space_handle_t h;						\
	bus_size_t o, c;						\
	const u_int8_t *a;						\
{									\
	TYPE temp;							\
	int i;								\
									\
	while (c > 0) {							\
		temp = 0;						\
		for (i = BYTES - 1; i >= 0; i--) {			\
			temp <<= 8;					\
			if (i < c)					\
				temp |= *(a + i);			\
		}							\
		__C(__C(CHIP,_mem_write_),BYTES)(v, h, o, temp);	\
		__C(CHIP,_mem_barrier)(v, h, o, BYTES, BUS_BARRIER_WRITE); \
		i = MIN(c, BYTES);					\
		c -= i;							\
		a += i;							\
	}								\
}
CHIP_mem_write_raw_multi_N(2,u_int16_t)
CHIP_mem_write_raw_multi_N(4,u_int32_t)
CHIP_mem_write_raw_multi_N(8,u_int64_t)
