/*	$OpenBSD: tc_bus_mem.c,v 1.2 1996/07/29 23:02:27 niklas Exp $	*/
/*	$NetBSD: tc_bus_mem.c,v 1.2.4.2 1996/06/13 17:42:51 cgd Exp $	*/

/*
 * Copyright (c) 1996 Carnegie-Mellon University.
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
 * Common TurboChannel Chipset "bus memory" functions.
 */

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <vm/vm.h>

#include <machine/bus.h>
#include <dev/tc/tcvar.h>

int		tc_mem_map __P((void *, bus_mem_addr_t, bus_mem_size_t,
		    int, bus_mem_handle_t *));
void		tc_mem_unmap __P((void *, bus_mem_handle_t,
		    bus_mem_size_t));
int		tc_mem_subregion __P((void *, bus_mem_handle_t, bus_mem_size_t,
		    bus_mem_size_t, bus_mem_handle_t *));
u_int8_t	tc_mem_read_1 __P((void *, bus_mem_handle_t,
		    bus_mem_size_t));
u_int16_t	tc_mem_read_2 __P((void *, bus_mem_handle_t,
		    bus_mem_size_t));
u_int32_t	tc_mem_read_4 __P((void *, bus_mem_handle_t,
		    bus_mem_size_t));
u_int64_t	tc_mem_read_8 __P((void *, bus_mem_handle_t,
		    bus_mem_size_t));
void		tc_mem_write_1 __P((void *, bus_mem_handle_t,
		    bus_mem_size_t, u_int8_t));
void		tc_mem_write_2 __P((void *, bus_mem_handle_t,
		    bus_mem_size_t, u_int16_t));
void		tc_mem_write_4 __P((void *, bus_mem_handle_t,
		    bus_mem_size_t, u_int32_t));
void		tc_mem_write_8 __P((void *, bus_mem_handle_t,
		    bus_mem_size_t, u_int64_t));

/* XXX DOES NOT BELONG */
vm_offset_t	tc_XXX_dmamap __P((void *));

void
tc_bus_mem_init(bc, memv)
	bus_chipset_tag_t bc;
	void *memv;
{

	bc->bc_m_v = memv;

	bc->bc_m_map = tc_mem_map;
	bc->bc_m_unmap = tc_mem_unmap;
	bc->bc_m_subregion = tc_mem_subregion;

	bc->bc_mr1 = tc_mem_read_1;
	bc->bc_mr2 = tc_mem_read_2;
	bc->bc_mr4 = tc_mem_read_4;
	bc->bc_mr8 = tc_mem_read_8;

	bc->bc_mw1 = tc_mem_write_1;
	bc->bc_mw2 = tc_mem_write_2;
	bc->bc_mw4 = tc_mem_write_4;
	bc->bc_mw8 = tc_mem_write_8;

	/* XXX DOES NOT BELONG */
	bc->bc_XXX_dmamap = tc_XXX_dmamap;
}

int
tc_mem_map(v, memaddr, memsize, cacheable, memhp)
	void *v;
	bus_mem_addr_t memaddr;
	bus_mem_size_t memsize;
	int cacheable;
	bus_mem_handle_t *memhp;
{

	if (memaddr & 0x7)
		panic("tc_mem_map needs 8 byte alignment");
	if (cacheable)
		*memhp = phystok0seg(memaddr);
	else
		*memhp = phystok0seg(TC_DENSE_TO_SPARSE(memaddr));
	return (0);
}

void
tc_mem_unmap(v, memh, memsize)
	void *v;
	bus_mem_handle_t memh;
	bus_mem_size_t memsize;
{

	/* XXX nothing to do. */
}

int
tc_mem_subregion(v, memh, offset, size, nmemh)
	void *v;
	bus_mem_handle_t memh, *nmemh;
	bus_mem_size_t offset, size;
{

	/* Disallow subregioning that would make the handle unaligned. */
	if ((offset & 0x7) != 0)
		return (1);

	if ((memh & TC_SPACE_SPARSE) != 0)
		*nmemh = memh + (offset << 1);
	else
		*nmemh = memh + offset;

	return (0);
}

u_int8_t
tc_mem_read_1(v, memh, off)
	void *v;
	bus_mem_handle_t memh;
	bus_mem_size_t off;
{
	volatile u_int8_t *p;

	wbflush();

	if ((memh & TC_SPACE_SPARSE) != 0)
		panic("tc_mem_read_1 not implemented for sparse space");

	p = (u_int8_t *)(memh + off);
	return (*p);
}

u_int16_t
tc_mem_read_2(v, memh, off)
	void *v;
	bus_mem_handle_t memh;
	bus_mem_size_t off;
{
	volatile u_int16_t *p;

	wbflush();

	if ((memh & TC_SPACE_SPARSE) != 0)
		panic("tc_mem_read_2 not implemented for sparse space");

	p = (u_int16_t *)(memh + off);
	return (*p);
}

u_int32_t
tc_mem_read_4(v, memh, off)
	void *v;
	bus_mem_handle_t memh;
	bus_mem_size_t off;
{
	volatile u_int32_t *p;

	wbflush();

	if ((memh & TC_SPACE_SPARSE) != 0)
		/* Nothing special to do for 4-byte sparse space accesses */
		p = (u_int32_t *)(memh + (off << 1));
	else
		p = (u_int32_t *)(memh + off);
	return (*p);
}

u_int64_t
tc_mem_read_8(v, memh, off)
	void *v;
	bus_mem_handle_t memh;
	bus_mem_size_t off;
{
	volatile u_int64_t *p;

	wbflush();

	if ((memh & TC_SPACE_SPARSE) != 0)
		panic("tc_mem_read_8 not implemented for sparse space");

	p = (u_int64_t *)(memh + off);
	return (*p);
}

void
tc_mem_write_1(v, memh, off, val)
	void *v;
	bus_mem_handle_t memh;
	bus_mem_size_t off;
	u_int8_t val;
{

	if ((memh & TC_SPACE_SPARSE) != 0) {
		volatile u_int64_t *p, v;
		u_int64_t shift, msk;

		shift = off & 0x3;
		off &= 0x3;

		p = (u_int64_t *)(memh + (off << 1));

		msk = ~(0x1 << shift) & 0xf;
		v = (msk << 32) | (((u_int64_t)val) << (shift * 8));

		*p = val;
	} else {
		volatile u_int8_t *p;

		p = (u_int8_t *)(memh + off);
		*p = val;
	}
        wbflush();
}

void
tc_mem_write_2(v, memh, off, val)
	void *v;
	bus_mem_handle_t memh;
	bus_mem_size_t off;
	u_int16_t val;
{

	if ((memh & TC_SPACE_SPARSE) != 0) {
		volatile u_int64_t *p, v;
		u_int64_t shift, msk;

		shift = off & 0x2;
		off &= 0x3;

		p = (u_int64_t *)(memh + (off << 1));

		msk = ~(0x3 << shift) & 0xf;
		v = (msk << 32) | (((u_int64_t)val) << (shift * 8));

		*p = val;
	} else {
		volatile u_int16_t *p;

		p = (u_int16_t *)(memh + off);
		*p = val;
	}
        wbflush();
}

void
tc_mem_write_4(v, memh, off, val)
	void *v;
	bus_mem_handle_t memh;
	bus_mem_size_t off;
	u_int32_t val;
{
	volatile u_int32_t *p;

	if ((memh & TC_SPACE_SPARSE) != 0)
		/* Nothing special to do for 4-byte sparse space accesses */
		p = (u_int32_t *)(memh + (off << 1));
	else
		p = (u_int32_t *)(memh + off);
	*p = val;
        wbflush();
}

void
tc_mem_write_8(v, memh, off, val)
	void *v;
	bus_mem_handle_t memh;
	bus_mem_size_t off;
	u_int64_t val;
{
	volatile u_int64_t *p;

	if ((memh & TC_SPACE_SPARSE) != 0)
		panic("tc_mem_read_8 not implemented for sparse space");

	p = (u_int64_t *)(memh + off);
	*p = val;
        wbflush();
}

/* XXX DOES NOT BELONG */
vm_offset_t
tc_XXX_dmamap(addr)
	void *addr;
{

	return (vtophys(addr));
}
