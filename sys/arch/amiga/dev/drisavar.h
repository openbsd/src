/*	$OpenBSD: drisavar.h,v 1.1 1997/01/16 09:23:57 niklas Exp $	*/
/*	$NetBSD: drisavar.h,v 1.1 1996/11/30 00:43:05 is Exp $	*/

#ifndef DRISAVAR_H

#ifndef DRCCADDR
#include <amiga/amiga/drcustom.h>
#endif

#define i2drabs(io)	(DRCCADDR + NBPG * DRSUPIOPG + (io << 2) + 1)
#define i2drrel(ioh,io)	(ioh + (io << 2))

#define bus_chipset_tag_t		void *
#define bus_io_handle_t			vm_offset_t

#define bus_io_map(bc, iob, n, iohp)	(*(iohp) = i2drabs(iob), 0)
#define bus_io_unmap(bc, ioh, n)	do {(void)bc; (void)ioh; (void)n;} while (0)

#define bus_io_read_1(bt, ioh, off)	((void)bc, *(u_int8_t *)i2drrel(ioh, off))
#define bus_io_write_1(bt, ioh, off, val) do {\
	(void)bt; *(u_int8_t *)i2drrel(ioh, off) = (val); } while (0)

#endif
