/*	$NetBSD: tc_machdep.h,v 1.6 1997/05/25 05:58:36 jonathan Exp $	*/

/*
 * Copyright (c) 1994, 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Jonathan Stone, Chris G. Demetriou
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
 * Machine-specific definitions for TurboChannel support.
 *
 * This file must typedef the following types:
 *
 *	tc_addr_t	TurboChannel bus address
 *	tc_offset_t	TurboChannel bus address difference (offset)
 *
 * This file must prototype or define the following functions
 * or macros (one or more of which may be no-ops):
 *
 *	tc_mb()		read/write memory barrier (any CPU<->memory
 *			reads/writes before must complete before any
 *			CPU<->memory reads/writes after).
 *	tc_wmb()	write memory barrier (any CPU<->memory writes
 *			before must complete before any CPU<->memory
 *			writes after).
 *	tc_syncbus()	sync TC bus; make sure CPU writes are
 *			propagated across the TurboChannel bus.
 *	tc_badaddr()	return non-zero if the given address is invalid.
 *	TC_DENSE_TO_SPARSE()
 *			convert the given physical address in
 *			TurboChannel dense space to the corresponding
 *			address in TurboChannel sparse space.
 *	TC_PHYS_TO_UNCACHED()
 *			convert the given system memory physical address
 *			to the physical address of the corresponding
 *			region that is not cached.
 */

#ifndef __MACHINE_TC_MACHDEP_H__
#define __MACHINE_TC_MACHDEP_H__

#include <machine/cpuregs.h>		/* defines MIPS_PHYS_TO_KSEG1 */

typedef int32_t		tc_addr_t;
typedef int32_t		tc_offset_t;

#define	tc_mb()		wbflush()
#define	tc_wmb()	wbflush()

/*
 * XXX how to do this on a DECstation ?
 */
#define	tc_syncbus()	wbflush();

#define	tc_badaddr(tcaddr)						\
    badaddr((void *)(tcaddr), sizeof (u_int32_t))

#define	TC_DENSE_TO_SPARSE(addr)  (addr)
		
#define	TC_PHYS_TO_UNCACHED(addr) MIPS_PHYS_TO_KSEG1(addr)


/*
 * Use the following macros to compare device names on a pmax, as
 * the autoconfig structs are in a state of flux.
 */
struct confargs;

#define TC_BUS_MATCHNAME(ta, name) \
     (strncmp( (ta)->ta_modname, (name), TC_ROM_LLEN+1) == 0)

/*
 * Port-specific declarations:
 * Declarations "private" sys/dev/tc/tc.c MI functions used to search
 * for potential TC-option console devices (framebuffers),
 */
int tc_checkslot __P((tc_addr_t slotbase, char *namep));

/*
 * And declarations for the MD function used to search for and configure
 * a TC framebuffer  as system console, and to configure the TC bus
 * (the last is a hack).
 */

extern int tc_findconsole __P((int preferred_slot));
extern void config_tcbus __P((struct device *parent, int cputype,
			      int printfn __P((void*, const char*)) ));
extern int badaddr	 __P((void *, u_int));

#endif /* __MACHINE_TC_MACHDEP_H__*/
