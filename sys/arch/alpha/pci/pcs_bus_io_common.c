/*	$NetBSD: pcs_bus_io_common.c,v 1.2.4.2 1996/06/13 18:16:59 cgd Exp $	*/

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
 *	CHIP_IO_BASE	Sparse I/O space base to use.
 */

#define	__C(A,B)	__CONCAT(A,B)
#define	__S(S)		__STRING(S)

int		__C(CHIP,_io_map) __P((void *, bus_io_addr_t, bus_io_size_t,
		    bus_io_handle_t *));
void		__C(CHIP,_io_unmap) __P((void *, bus_io_handle_t,
		    bus_io_size_t));
int		__C(CHIP,_io_subregion) __P((void *, bus_io_handle_t,
		    bus_io_size_t, bus_io_size_t, bus_io_handle_t *));
u_int8_t	__C(CHIP,_io_read_1) __P((void *, bus_io_handle_t,
		    bus_io_size_t));
u_int16_t	__C(CHIP,_io_read_2) __P((void *, bus_io_handle_t,
		    bus_io_size_t));
u_int32_t	__C(CHIP,_io_read_4) __P((void *, bus_io_handle_t,
		    bus_io_size_t));
u_int64_t	__C(CHIP,_io_read_8) __P((void *, bus_io_handle_t,
		    bus_io_size_t));
void		__C(CHIP,_io_read_multi_1) __P((void *, bus_io_handle_t,
		    bus_io_size_t, u_int8_t *, bus_io_size_t));
void		__C(CHIP,_io_read_multi_2) __P((void *, bus_io_handle_t,
		    bus_io_size_t, u_int16_t *, bus_io_size_t));
void		__C(CHIP,_io_read_multi_4) __P((void *, bus_io_handle_t,
		    bus_io_size_t, u_int32_t *, bus_io_size_t));
void		__C(CHIP,_io_read_multi_8) __P((void *, bus_io_handle_t,
		    bus_io_size_t, u_int64_t *, bus_io_size_t));
void		__C(CHIP,_io_write_1) __P((void *, bus_io_handle_t,
		    bus_io_size_t, u_int8_t));
void		__C(CHIP,_io_write_2) __P((void *, bus_io_handle_t,
		    bus_io_size_t, u_int16_t));
void		__C(CHIP,_io_write_4) __P((void *, bus_io_handle_t,
		    bus_io_size_t, u_int32_t));
void		__C(CHIP,_io_write_8) __P((void *, bus_io_handle_t,
		    bus_io_size_t, u_int64_t));
void		__C(CHIP,_io_write_multi_1) __P((void *, bus_io_handle_t,
		    bus_io_size_t, const u_int8_t *, bus_io_size_t));
void		__C(CHIP,_io_write_multi_2) __P((void *, bus_io_handle_t,
		    bus_io_size_t, const u_int16_t *, bus_io_size_t));
void		__C(CHIP,_io_write_multi_4) __P((void *, bus_io_handle_t,
		    bus_io_size_t, const u_int32_t *, bus_io_size_t));
void		__C(CHIP,_io_write_multi_8) __P((void *, bus_io_handle_t,
		    bus_io_size_t, const u_int64_t *, bus_io_size_t));

void
__C(CHIP,_bus_io_init)(bc, iov)
	bus_chipset_tag_t bc;
	void *iov;
{

	bc->bc_i_v = iov;

	bc->bc_i_map = __C(CHIP,_io_map);
	bc->bc_i_unmap = __C(CHIP,_io_unmap);
	bc->bc_i_subregion = __C(CHIP,_io_subregion);

	bc->bc_ir1 = __C(CHIP,_io_read_1);
	bc->bc_ir2 = __C(CHIP,_io_read_2);
	bc->bc_ir4 = __C(CHIP,_io_read_4);
	bc->bc_ir8 = __C(CHIP,_io_read_8);

	bc->bc_irm1 = __C(CHIP,_io_read_multi_1);
	bc->bc_irm2 = __C(CHIP,_io_read_multi_2);
	bc->bc_irm4 = __C(CHIP,_io_read_multi_4);
	bc->bc_irm8 = __C(CHIP,_io_read_multi_8);

	bc->bc_iw1 = __C(CHIP,_io_write_1);
	bc->bc_iw2 = __C(CHIP,_io_write_2);
	bc->bc_iw4 = __C(CHIP,_io_write_4);
	bc->bc_iw8 = __C(CHIP,_io_write_8);

	bc->bc_iwm1 = __C(CHIP,_io_write_multi_1);
	bc->bc_iwm2 = __C(CHIP,_io_write_multi_2);
	bc->bc_iwm4 = __C(CHIP,_io_write_multi_4);
	bc->bc_iwm8 = __C(CHIP,_io_write_multi_8);
}

int
__C(CHIP,_io_map)(v, ioaddr, iosize, iohp)
	void *v;
	bus_io_addr_t ioaddr;
	bus_io_size_t iosize;
	bus_io_handle_t *iohp;
{

#ifdef CHIP_IO_W1_START
	if (ioaddr >= CHIP_IO_W1_START(v) &&
	    ioaddr <= CHIP_IO_W1_END(v)) {
		*iohp = (phystok0seg(CHIP_IO_W1_BASE(v)) >> 5) +
		    (ioaddr & CHIP_IO_W1_MASK(v));
	} else
#endif
#ifdef CHIP_IO_W2_START
	if (ioaddr >= CHIP_IO_W2_START(v) &&
	    ioaddr <= CHIP_IO_W2_END(v)) {
		*iohp = (phystok0seg(CHIP_IO_W2_BASE(v)) >> 5) +
		    (ioaddr & CHIP_IO_W2_MASK(v));
	} else
#endif
	{
		printf("\n");
#ifdef CHIP_IO_W1_START
		printf("%s: window[1]=0x%lx-0x%lx\n",
		    __S(__C(CHIP,_io_map)), CHIP_IO_W1_START(v),
		    CHIP_IO_W1_END(v)-1);
#endif
#ifdef CHIP_IO_W2_START
		printf("%s: window[2]=0x%lx-0x%lx\n",
		    __S(__C(CHIP,_io_map)), CHIP_IO_W2_START(v),
		    CHIP_IO_W2_END(v)-1);
#endif
		panic("%s: don't know how to map %lx non-cacheable\n",
		    __S(__C(CHIP,_io_map)), ioaddr);
	}

	return (0);
}

void
__C(CHIP,_io_unmap)(v, ioh, iosize)
	void *v;
	bus_io_handle_t ioh;
	bus_io_size_t iosize;
{

	/* XXX nothing to do. */
}

int
__C(CHIP,_io_subregion)(v, ioh, offset, size, nioh)
	void *v;
	bus_io_handle_t ioh, *nioh;
	bus_io_size_t offset, size;
{

	*nioh = ioh + offset;
	return (0);
}

u_int8_t
__C(CHIP,_io_read_1)(v, ioh, off)
	void *v;
	bus_io_handle_t ioh;
	bus_io_size_t off;
{
	register bus_io_handle_t tmpioh;
	register u_int32_t *port, val;
	register u_int8_t rval;
	register int offset;

	wbflush();

	tmpioh = ioh + off;
	offset = tmpioh & 3;
	port = (u_int32_t *)((tmpioh << 5) | (0 << 3));
	val = *port;
	rval = ((val) >> (8 * offset)) & 0xff;

	return rval;
}

u_int16_t
__C(CHIP,_io_read_2)(v, ioh, off)
	void *v;
	bus_io_handle_t ioh;
	bus_io_size_t off;
{
	register bus_io_handle_t tmpioh;
	register u_int32_t *port, val;
	register u_int16_t rval;
	register int offset;

	wbflush();

	tmpioh = ioh + off;
	offset = tmpioh & 3;
	port = (u_int32_t *)((tmpioh << 5) | (1 << 3));
	val = *port;
	rval = ((val) >> (8 * offset)) & 0xffff;

	return rval;
}

u_int32_t
__C(CHIP,_io_read_4)(v, ioh, off)
	void *v;
	bus_io_handle_t ioh;
	bus_io_size_t off;
{
	register bus_io_handle_t tmpioh;
	register u_int32_t *port, val;
	register u_int32_t rval;
	register int offset;

	wbflush();

	tmpioh = ioh + off;
	offset = tmpioh & 3;
	port = (u_int32_t *)((tmpioh << 5) | (3 << 3));
	val = *port;
#if 0
	rval = ((val) >> (8 * offset)) & 0xffffffff;
#else
	rval = val;
#endif

	return rval;
}

u_int64_t
__C(CHIP,_io_read_8)(v, ioh, off)
	void *v;
	bus_io_handle_t ioh;
	bus_io_size_t off;
{

	/* XXX XXX XXX */
	panic("%s not implemented\n", __S(__C(CHIP,_io_read_8)));
}

void
__C(CHIP,_io_read_multi_1)(v, ioh, off, addr, count)
	void *v;
	bus_io_handle_t ioh;
	bus_io_size_t off, count;
	u_int8_t *addr;
{
	register bus_io_handle_t tmpioh;
	register u_int32_t *port, val;
	register int offset;

	wbflush();

	while (count--) {
		tmpioh = ioh + off;
		offset = tmpioh & 3;
		port = (u_int32_t *)((tmpioh << 5) | (0 << 3));
		val = *port;
		*addr++ = ((val) >> (8 * offset)) & 0xff;
		off++;
	}
}

void
__C(CHIP,_io_read_multi_2)(v, ioh, off, addr, count)
	void *v;
	bus_io_handle_t ioh;
	bus_io_size_t off, count;
	u_int16_t *addr;
{
	register bus_io_handle_t tmpioh;
	register u_int32_t *port, val;
	register int offset;

	wbflush();

	while (count--) {
		tmpioh = ioh + off;
		offset = tmpioh & 3;
		port = (u_int32_t *)((tmpioh << 5) | (1 << 3));
		val = *port;
		*addr++ = ((val) >> (8 * offset)) & 0xffff;
		off++;
	}
}

void
__C(CHIP,_io_read_multi_4)(v, ioh, off, addr, count)
	void *v;
	bus_io_handle_t ioh;
	bus_io_size_t off, count;
	u_int32_t *addr;
{
	register bus_io_handle_t tmpioh;
	register u_int32_t *port, val;
	register int offset;

	wbflush();

	while (count--) {
		tmpioh = ioh + off;
		offset = tmpioh & 3;
		port = (u_int32_t *)((tmpioh << 5) | (3 << 3));
		val = *port;
#if 0
		*addr++ = ((val) >> (8 * offset)) & 0xffffffff;
#else
		*addr++ = val;
#endif
		off++;
	}
}

void
__C(CHIP,_io_read_multi_8)(v, ioh, off, addr, count)
	void *v;
	bus_io_handle_t ioh;
	bus_io_size_t off, count;
	u_int64_t *addr;
{

	/* XXX XXX XXX */
	panic("%s not implemented\n", __S(__C(CHIP,_io_read_multi_8)));
}

void
__C(CHIP,_io_write_1)(v, ioh, off, val)
	void *v;
	bus_io_handle_t ioh;
	bus_io_size_t off;
	u_int8_t val;
{
	register bus_io_handle_t tmpioh;
	register u_int32_t *port, nval;
	register int offset;

	tmpioh = ioh + off;
	offset = tmpioh & 3;
        nval = val << (8 * offset);
        port = (u_int32_t *)((tmpioh << 5) | (0 << 3));
        *port = nval;
        wbflush();
}

void
__C(CHIP,_io_write_2)(v, ioh, off, val)
	void *v;
	bus_io_handle_t ioh;
	bus_io_size_t off;
	u_int16_t val;
{
	register bus_io_handle_t tmpioh;
	register u_int32_t *port, nval;
	register int offset;

	tmpioh = ioh + off;
	offset = tmpioh & 3;
        nval = val << (8 * offset);
        port = (u_int32_t *)((tmpioh << 5) | (1 << 3));
        *port = nval;
        wbflush();
}

void
__C(CHIP,_io_write_4)(v, ioh, off, val)
	void *v;
	bus_io_handle_t ioh;
	bus_io_size_t off;
	u_int32_t val;
{
	register bus_io_handle_t tmpioh;
	register u_int32_t *port, nval;
	register int offset;

	tmpioh = ioh + off;
	offset = tmpioh & 3;
        nval = val /*<< (8 * offset)*/;
        port = (u_int32_t *)((tmpioh << 5) | (3 << 3));
        *port = nval;
        wbflush();
}

void
__C(CHIP,_io_write_8)(v, ioh, off, val)
	void *v;
	bus_io_handle_t ioh;
	bus_io_size_t off;
	u_int64_t val;
{

	/* XXX XXX XXX */
	panic("%s not implemented\n", __S(__C(CHIP,_io_write_8)));
	wbflush();
}

void
__C(CHIP,_io_write_multi_1)(v, ioh, off, addr, count)
	void *v;
	bus_io_handle_t ioh;
	bus_io_size_t off, count;
	const u_int8_t *addr;
{
	register bus_io_handle_t tmpioh;
	register u_int32_t *port, nval;
	register int offset;

	while (count--) {
		tmpioh = ioh + off;
		offset = tmpioh & 3;
        	nval = (*addr++) << (8 * offset);
		port = (u_int32_t *)((tmpioh << 5) | (0 << 3));
		*port = nval;
		off++;
	}
	wbflush();
}

void
__C(CHIP,_io_write_multi_2)(v, ioh, off, addr, count)
	void *v;
	bus_io_handle_t ioh;
	bus_io_size_t off, count;
	const u_int16_t *addr;
{
	register bus_io_handle_t tmpioh;
	register u_int32_t *port, nval;
	register int offset;

	while (count--) {
		tmpioh = ioh + off;
		offset = tmpioh & 3;
        	nval = (*addr++) << (8 * offset);
		port = (u_int32_t *)((tmpioh << 5) | (1 << 3));
		*port = nval;
		off++;
	}
	wbflush();
}

void
__C(CHIP,_io_write_multi_4)(v, ioh, off, addr, count)
	void *v;
	bus_io_handle_t ioh;
	bus_io_size_t off, count;
	const u_int32_t *addr;
{
	register bus_io_handle_t tmpioh;
	register u_int32_t *port, nval;
	register int offset;

	while (count--) {
		tmpioh = ioh + off;
		offset = tmpioh & 3;
        	nval = (*addr++) /*<< (8 * offset)*/;
		port = (u_int32_t *)((tmpioh << 5) | (3 << 3));
		*port = nval;
		off++;
	}
	wbflush();
}

void
__C(CHIP,_io_write_multi_8)(v, ioh, off, addr, count)
	void *v;
	bus_io_handle_t ioh;
	bus_io_size_t off, count;
	const u_int64_t *addr;
{

	/* XXX XXX XXX */
	panic("%s not implemented\n", __S(__C(CHIP,_io_write_multi_8)));
}
