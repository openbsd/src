/*	$OpenBSD: tc_bus_io.c,v 1.2 1996/07/29 23:02:26 niklas Exp $	*/
/*	$NetBSD: tc_bus_io.c,v 1.1.4.1 1996/06/13 17:41:51 cgd Exp $	*/

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
 * TurboChannel "bus I/O" functions:
 * These functions make no sense for TC, and just panic.
 */

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <vm/vm.h>

#include <machine/bus.h>

int		tc_io_map __P((void *, bus_io_addr_t, bus_io_size_t,
		    bus_io_handle_t *));
void		tc_io_unmap __P((void *, bus_io_handle_t,
		    bus_io_size_t));
int		tc_io_subregion __P((void *, bus_io_handle_t, bus_io_size_t,
		    bus_io_size_t, bus_io_handle_t *));
u_int8_t	tc_io_read_1 __P((void *, bus_io_handle_t,
		    bus_io_size_t));
u_int16_t	tc_io_read_2 __P((void *, bus_io_handle_t,
		    bus_io_size_t));
u_int32_t	tc_io_read_4 __P((void *, bus_io_handle_t,
		    bus_io_size_t));
u_int64_t	tc_io_read_8 __P((void *, bus_io_handle_t,
		    bus_io_size_t));
void		tc_io_read_multi_1 __P((void *, bus_io_handle_t,
		    bus_io_size_t, u_int8_t *, bus_io_size_t));
void		tc_io_read_multi_2 __P((void *, bus_io_handle_t,
		    bus_io_size_t, u_int16_t *, bus_io_size_t));
void		tc_io_read_multi_4 __P((void *, bus_io_handle_t,
		    bus_io_size_t, u_int32_t *, bus_io_size_t));
void		tc_io_read_multi_8 __P((void *, bus_io_handle_t,
		    bus_io_size_t, u_int64_t *, bus_io_size_t));
void		tc_io_write_1 __P((void *, bus_io_handle_t,
		    bus_io_size_t, u_int8_t));
void		tc_io_write_2 __P((void *, bus_io_handle_t,
		    bus_io_size_t, u_int16_t));
void		tc_io_write_4 __P((void *, bus_io_handle_t,
		    bus_io_size_t, u_int32_t));
void		tc_io_write_8 __P((void *, bus_io_handle_t,
		    bus_io_size_t, u_int64_t));
void		tc_io_write_multi_1 __P((void *, bus_io_handle_t,
		    bus_io_size_t, const u_int8_t *, bus_io_size_t));
void		tc_io_write_multi_2 __P((void *, bus_io_handle_t,
		    bus_io_size_t, const u_int16_t *, bus_io_size_t));
void		tc_io_write_multi_4 __P((void *, bus_io_handle_t,
		    bus_io_size_t, const u_int32_t *, bus_io_size_t));
void		tc_io_write_multi_8 __P((void *, bus_io_handle_t,
		    bus_io_size_t, const u_int64_t *, bus_io_size_t));

void
tc_bus_io_init(bc, iov)
	bus_chipset_tag_t bc;
	void *iov;
{

	bc->bc_i_v = iov;

	bc->bc_i_map = tc_io_map;
	bc->bc_i_unmap = tc_io_unmap;
	bc->bc_i_subregion = tc_io_subregion;

	bc->bc_ir1 = tc_io_read_1;
	bc->bc_ir2 = tc_io_read_2;
	bc->bc_ir4 = tc_io_read_4;
	bc->bc_ir8 = tc_io_read_8;

	bc->bc_irm1 = tc_io_read_multi_1;
	bc->bc_irm2 = tc_io_read_multi_2;
	bc->bc_irm4 = tc_io_read_multi_4;
	bc->bc_irm8 = tc_io_read_multi_8;

	bc->bc_iw1 = tc_io_write_1;
	bc->bc_iw2 = tc_io_write_2;
	bc->bc_iw4 = tc_io_write_4;
	bc->bc_iw8 = tc_io_write_8;

	bc->bc_iwm1 = tc_io_write_multi_1;
	bc->bc_iwm2 = tc_io_write_multi_2;
	bc->bc_iwm4 = tc_io_write_multi_4;
	bc->bc_iwm8 = tc_io_write_multi_8;
}

static const char *tc_bus_io_panicstr = "tc_io_%s nonsensical; unimplemented";

int
tc_io_map(v, ioaddr, iosize, iohp)
	void *v;
	bus_io_addr_t ioaddr;
	bus_io_size_t iosize;
	bus_io_handle_t *iohp;
{

	panic(tc_bus_io_panicstr, "map");
}

void
tc_io_unmap(v, ioh, iosize)
	void *v;
	bus_io_handle_t ioh;
	bus_io_size_t iosize;
{

	panic(tc_bus_io_panicstr, "unmap");
}

int
tc_io_subregion(v, ioh, offset, size, nioh)
	void *v;
	bus_io_handle_t ioh, *nioh;
	bus_io_size_t offset, size;
{

	panic(tc_bus_io_panicstr, "subregion");
}

u_int8_t
tc_io_read_1(v, ioh, off)
	void *v;
	bus_io_handle_t ioh;
	bus_io_size_t off;
{

	panic(tc_bus_io_panicstr, "read_1");
}

u_int16_t
tc_io_read_2(v, ioh, off)
	void *v;
	bus_io_handle_t ioh;
	bus_io_size_t off;
{

	panic(tc_bus_io_panicstr, "read_2");
}

u_int32_t
tc_io_read_4(v, ioh, off)
	void *v;
	bus_io_handle_t ioh;
	bus_io_size_t off;
{

	panic(tc_bus_io_panicstr, "read_4");
}

u_int64_t
tc_io_read_8(v, ioh, off)
	void *v;
	bus_io_handle_t ioh;
	bus_io_size_t off;
{

	panic(tc_bus_io_panicstr, "read_8");
}

void
tc_io_read_multi_1(v, ioh, off, addr, count)
	void *v;
	bus_io_handle_t ioh;
	bus_io_size_t off, count;
	u_int8_t *addr;
{

	panic(tc_bus_io_panicstr, "read_multi_1");
}

void
tc_io_read_multi_2(v, ioh, off, addr, count)
	void *v;
	bus_io_handle_t ioh;
	bus_io_size_t off, count;
	u_int16_t *addr;
{

	panic(tc_bus_io_panicstr, "read_multi_2");
}

void
tc_io_read_multi_4(v, ioh, off, addr, count)
	void *v;
	bus_io_handle_t ioh;
	bus_io_size_t off, count;
	u_int32_t *addr;
{

	panic(tc_bus_io_panicstr, "read_multi_4");
}

void
tc_io_read_multi_8(v, ioh, off, addr, count)
	void *v;
	bus_io_handle_t ioh;
	bus_io_size_t off, count;
	u_int64_t *addr;
{

	panic(tc_bus_io_panicstr, "read_multi_8");
}

void
tc_io_write_1(v, ioh, off, val)
	void *v;
	bus_io_handle_t ioh;
	bus_io_size_t off;
	u_int8_t val;
{

	panic(tc_bus_io_panicstr, "write_1");
}

void
tc_io_write_2(v, ioh, off, val)
	void *v;
	bus_io_handle_t ioh;
	bus_io_size_t off;
	u_int16_t val;
{

	panic(tc_bus_io_panicstr, "write_2");
}

void
tc_io_write_4(v, ioh, off, val)
	void *v;
	bus_io_handle_t ioh;
	bus_io_size_t off;
	u_int32_t val;
{

	panic(tc_bus_io_panicstr, "write_4");
}

void
tc_io_write_8(v, ioh, off, val)
	void *v;
	bus_io_handle_t ioh;
	bus_io_size_t off;
	u_int64_t val;
{

	panic(tc_bus_io_panicstr, "write_8");
}

void
tc_io_write_multi_1(v, ioh, off, addr, count)
	void *v;
	bus_io_handle_t ioh;
	bus_io_size_t off, count;
	const u_int8_t *addr;
{

	panic(tc_bus_io_panicstr, "write_multi_1");
}

void
tc_io_write_multi_2(v, ioh, off, addr, count)
	void *v;
	bus_io_handle_t ioh;
	bus_io_size_t off, count;
	const u_int16_t *addr;
{

	panic(tc_bus_io_panicstr, "write_multi_2");
}

void
tc_io_write_multi_4(v, ioh, off, addr, count)
	void *v;
	bus_io_handle_t ioh;
	bus_io_size_t off, count;
	const u_int32_t *addr;
{

	panic(tc_bus_io_panicstr, "write_multi_4");
}

void
tc_io_write_multi_8(v, ioh, off, addr, count)
	void *v;
	bus_io_handle_t ioh;
	bus_io_size_t off, count;
	const u_int64_t *addr;
{

	panic(tc_bus_io_panicstr, "write_multi_8");
}
