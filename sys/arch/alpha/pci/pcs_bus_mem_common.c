/*	$OpenBSD: pcs_bus_mem_common.c,v 1.2 1996/07/29 23:00:51 niklas Exp $	*/
/*	$NetBSD: pcs_bus_mem_common.c,v 1.1.4.4 1996/06/13 18:17:01 cgd Exp $	*/

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
 *	CHIP_D_MEM_BASE	Dense Mem space base to use.
 *	CHIP_S_MEM_BASE	Sparse Mem space base to use.
 */

#define	__C(A,B)	__CONCAT(A,B)
#define	__S(S)		__STRING(S)

int		__C(CHIP,_mem_map) __P((void *, bus_mem_addr_t, bus_mem_size_t,
		    int, bus_mem_handle_t *));
void		__C(CHIP,_mem_unmap) __P((void *, bus_mem_handle_t,
		    bus_mem_size_t));
int		__C(CHIP,_mem_subregion) __P((void *, bus_mem_handle_t,
		    bus_mem_size_t, bus_mem_size_t, bus_mem_handle_t *));
u_int8_t	__C(CHIP,_mem_read_1) __P((void *, bus_mem_handle_t,
		    bus_mem_size_t));
u_int16_t	__C(CHIP,_mem_read_2) __P((void *, bus_mem_handle_t,
		    bus_mem_size_t));
u_int32_t	__C(CHIP,_mem_read_4) __P((void *, bus_mem_handle_t,
		    bus_mem_size_t));
u_int64_t	__C(CHIP,_mem_read_8) __P((void *, bus_mem_handle_t,
		    bus_mem_size_t));
void		__C(CHIP,_mem_write_1) __P((void *, bus_mem_handle_t,
		    bus_mem_size_t, u_int8_t));
void		__C(CHIP,_mem_write_2) __P((void *, bus_mem_handle_t,
		    bus_mem_size_t, u_int16_t));
void		__C(CHIP,_mem_write_4) __P((void *, bus_mem_handle_t,
		    bus_mem_size_t, u_int32_t));
void		__C(CHIP,_mem_write_8) __P((void *, bus_mem_handle_t,
		    bus_mem_size_t, u_int64_t));

/* XXX DOES NOT BELONG */
vm_offset_t	__C(CHIP,_XXX_dmamap) __P((void *));

void
__C(CHIP,_bus_mem_init)(bc, memv)
	bus_chipset_tag_t bc;
	void *memv;
{

	bc->bc_m_v = memv;

	bc->bc_m_map = __C(CHIP,_mem_map);
	bc->bc_m_unmap = __C(CHIP,_mem_unmap);
	bc->bc_m_subregion = __C(CHIP,_mem_subregion);

	bc->bc_mr1 = __C(CHIP,_mem_read_1);
	bc->bc_mr2 = __C(CHIP,_mem_read_2);
	bc->bc_mr4 = __C(CHIP,_mem_read_4);
	bc->bc_mr8 = __C(CHIP,_mem_read_8);

	bc->bc_mw1 = __C(CHIP,_mem_write_1);
	bc->bc_mw2 = __C(CHIP,_mem_write_2);
	bc->bc_mw4 = __C(CHIP,_mem_write_4);
	bc->bc_mw8 = __C(CHIP,_mem_write_8);

	/* XXX DOES NOT BELONG */
	bc->bc_XXX_dmamap = __C(CHIP,_XXX_dmamap);
}

int
__C(CHIP,_mem_map)(v, memaddr, memsize, cacheable, memhp)
	void *v;
	bus_mem_addr_t memaddr;
	bus_mem_size_t memsize;
	int cacheable;
	bus_mem_handle_t *memhp;
{

	if (cacheable) {
#ifdef CHIP_D_MEM_W1_START
                if (memaddr >= CHIP_D_MEM_W1_START(v) &&
                    memaddr <= CHIP_D_MEM_W1_END(v)) {
                        *memhp = phystok0seg(CHIP_D_MEM_W1_BASE(v)) +
                            (memaddr & CHIP_D_MEM_W1_MASK(v));
                } else
#endif
		{
			printf("\n");
#ifdef CHIP_D_MEM_W1_START
			printf("%s: window[1]=0x%lx-0x%lx\n",
			    __S(__C(CHIP,_mem_map)), CHIP_D_MEM_W1_START(v),
			    CHIP_D_MEM_W1_END(v)-1);
#endif
			panic("%s: don't know how to map %lx cacheable\n",
			    __S(__C(CHIP,_mem_map)), memaddr);
		}
	} else {
#ifdef CHIP_S_MEM_W1_START
		if (memaddr >= CHIP_S_MEM_W1_START(v) &&
		    memaddr <= CHIP_S_MEM_W1_END(v)) {
			*memhp = (phystok0seg(CHIP_S_MEM_W1_BASE(v)) >> 5) +
			    (memaddr & CHIP_S_MEM_W1_MASK(v));
		} else
#endif
#ifdef CHIP_S_MEM_W2_START
		if (memaddr >= CHIP_S_MEM_W2_START(v) &&
		    memaddr <= CHIP_S_MEM_W2_END(v)) {
			*memhp = (phystok0seg(CHIP_S_MEM_W2_BASE(v)) >> 5) +
			    (memaddr & CHIP_S_MEM_W2_MASK(v));
		} else
#endif
#ifdef CHIP_S_MEM_W3_START
		if (memaddr >= CHIP_S_MEM_W3_START(v) &&
		    memaddr <= CHIP_S_MEM_W3_END(v)) {
			*memhp = (phystok0seg(CHIP_S_MEM_W3_BASE(v)) >> 5) +
			    (memaddr & CHIP_S_MEM_W3_MASK(v));
		} else
#endif
		{
			printf("\n");
#ifdef CHIP_S_MEM_W1_START
			printf("%s: window[1]=0x%lx-0x%lx\n",
			    __S(__C(CHIP,_mem_map)), CHIP_S_MEM_W1_START(v),
			    CHIP_S_MEM_W1_END(v)-1);
#endif
#ifdef CHIP_S_MEM_W2_START
			printf("%s: window[2]=0x%lx-0x%lx\n",
			    __S(__C(CHIP,_mem_map)), CHIP_S_MEM_W2_START(v),
			    CHIP_S_MEM_W2_END(v)-1);
#endif
#ifdef CHIP_S_MEM_W3_START
			printf("%s: window[3]=0x%lx-0x%lx\n",
			    __S(__C(CHIP,_mem_map)), CHIP_S_MEM_W3_START(v),
			    CHIP_S_MEM_W3_END(v)-1);
#endif
			panic("%s: don't know how to map %lx non-cacheable\n",
			    __S(__C(CHIP,_mem_map)), memaddr);
		}
	}

	return (0);
}

void
__C(CHIP,_mem_unmap)(v, memh, memsize)
	void *v;
	bus_mem_handle_t memh;
	bus_mem_size_t memsize;
{

	/* XXX nothing to do. */
}

int
__C(CHIP,_mem_subregion)(v, memh, offset, size, nmemh)
	void *v;
	bus_mem_handle_t memh, *nmemh;
	bus_mem_size_t offset, size;
{

	*nmemh = memh + offset;
	return (0);
}

u_int8_t
__C(CHIP,_mem_read_1)(v, memh, off)
	void *v;
	bus_mem_handle_t memh;
	bus_mem_size_t off;
{
	register bus_mem_handle_t tmpmemh;
	register u_int32_t *port, val;
	register u_int8_t rval;
	register int offset;

	wbflush();

	if ((memh >> 63) != 0)
		return (*(u_int8_t *)(memh + off));

	tmpmemh = memh + off;
	offset = tmpmemh & 3;
	port = (u_int32_t *)((tmpmemh << 5) | (0 << 3));
	val = *port;
	rval = ((val) >> (8 * offset)) & 0xff;

	return rval;
}

u_int16_t
__C(CHIP,_mem_read_2)(v, memh, off)
	void *v;
	bus_mem_handle_t memh;
	bus_mem_size_t off;
{
	register bus_mem_handle_t tmpmemh;
	register u_int32_t *port, val;
	register u_int16_t rval;
	register int offset;

	wbflush();

	if ((memh >> 63) != 0)
		return (*(u_int16_t *)(memh + off));

	tmpmemh = memh + off;
	offset = tmpmemh & 3;
	port = (u_int32_t *)((tmpmemh << 5) | (1 << 3));
	val = *port;
	rval = ((val) >> (8 * offset)) & 0xffff;

	return rval;
}

u_int32_t
__C(CHIP,_mem_read_4)(v, memh, off)
	void *v;
	bus_mem_handle_t memh;
	bus_mem_size_t off;
{
	register bus_mem_handle_t tmpmemh;
	register u_int32_t *port, val;
	register u_int32_t rval;
	register int offset;

	wbflush();

	if ((memh >> 63) != 0)
		return (*(u_int32_t *)(memh + off));

	tmpmemh = memh + off;
	offset = tmpmemh & 3;
	port = (u_int32_t *)((tmpmemh << 5) | (3 << 3));
	val = *port;
#if 0
	rval = ((val) >> (8 * offset)) & 0xffffffff;
#else
	rval = val;
#endif

	return rval;
}

u_int64_t
__C(CHIP,_mem_read_8)(v, memh, off)
	void *v;
	bus_mem_handle_t memh;
	bus_mem_size_t off;
{

	wbflush();

        if ((memh >> 63) != 0)
                return (*(u_int64_t *)(memh + off));

	/* XXX XXX XXX */
	panic("%s not implemented\n", __S(__C(CHIP,_mem_read_8)));
}

void
__C(CHIP,_mem_write_1)(v, memh, off, val)
	void *v;
	bus_mem_handle_t memh;
	bus_mem_size_t off;
	u_int8_t val;
{
	register bus_mem_handle_t tmpmemh;
	register u_int32_t *port, nval;
	register int offset;

	if ((memh >> 63) != 0)
		(*(u_int8_t *)(memh + off)) = val;
	else {
		tmpmemh = memh + off;
		offset = tmpmemh & 3;
		nval = val << (8 * offset);
		port = (u_int32_t *)((tmpmemh << 5) | (0 << 3));
		*port = nval;
	}
        wbflush();
}

void
__C(CHIP,_mem_write_2)(v, memh, off, val)
	void *v;
	bus_mem_handle_t memh;
	bus_mem_size_t off;
	u_int16_t val;
{
	register bus_mem_handle_t tmpmemh;
	register u_int32_t *port, nval;
	register int offset;

	if ((memh >> 63) != 0)
		(*(u_int16_t *)(memh + off)) = val;
	else {
		tmpmemh = memh + off;
		offset = tmpmemh & 3;
	        nval = val << (8 * offset);
	        port = (u_int32_t *)((tmpmemh << 5) | (1 << 3));
	        *port = nval;
	}
        wbflush();
}

void
__C(CHIP,_mem_write_4)(v, memh, off, val)
	void *v;
	bus_mem_handle_t memh;
	bus_mem_size_t off;
	u_int32_t val;
{
	register bus_mem_handle_t tmpmemh;
	register u_int32_t *port, nval;
	register int offset;

	if ((memh >> 63) != 0)
		(*(u_int32_t *)(memh + off)) = val;
	else {
		tmpmemh = memh + off;
		offset = tmpmemh & 3;
	        nval = val /*<< (8 * offset)*/;
	        port = (u_int32_t *)((tmpmemh << 5) | (3 << 3));
	        *port = nval;
	}
        wbflush();
}

void
__C(CHIP,_mem_write_8)(v, memh, off, val)
	void *v;
	bus_mem_handle_t memh;
	bus_mem_size_t off;
	u_int64_t val;
{

	if ((memh >> 63) != 0)
		(*(u_int64_t *)(memh + off)) = val;
	else {
		/* XXX XXX XXX */
		panic("%s not implemented\n",
		    __S(__C(CHIP,_mem_write_8)));
	}
	wbflush();
}

vm_offset_t
__C(CHIP,_XXX_dmamap)(addr)
	void *addr;
{

	return (vtophys(addr) | 0x40000000);
}
