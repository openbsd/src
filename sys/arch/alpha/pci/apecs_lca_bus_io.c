/*	$OpenBSD: apecs_lca_bus_io.c,v 1.7 2001/11/06 19:53:13 miod Exp $	*/
/*	$NetBSD: apecs_lca_bus_io.c,v 1.4 1996/08/27 16:29:23 cgd Exp $	*/

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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <uvm/uvm_extern.h>

#include <machine/bus.h>

#include <alpha/pci/apecsreg.h>
#include <alpha/pci/lcareg.h>
#include <alpha/pci/lcavar.h>

#if (APECS_PCI_SIO != LCA_PCI_SIO)
#error Sparse I/O addresses do not match up?
#endif

#define	CHIP		apecs_lca

/* IO region 1 */
#define	CHIP_IO_W1_START(v)	0x00000000
#define	CHIP_IO_W1_END(v)	0x0003ffff
#define	CHIP_IO_W1_BASE(v)	APECS_PCI_SIO
#define	CHIP_IO_W1_MASK(v)	0x00ffffff

/* IO region 2 */
#define	CHIP_IO_W2_START(v)	0x00040000              /* XXX from HAXR2 */
#define	CHIP_IO_W2_END(v)	0xfffbffff              /* XXX from HAXR2 */
#define	CHIP_IO_W2_BASE(v)	APECS_PCI_SIO
#define	CHIP_IO_W2_MASK(v)	0x00ffffff

#include "pci_swiz_bus_io_chipdep.c"
