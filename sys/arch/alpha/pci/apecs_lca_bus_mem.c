/*	$OpenBSD: apecs_lca_bus_mem.c,v 1.2 1996/07/29 23:00:06 niklas Exp $	*/
/*	$NetBSD: apecs_lca_bus_mem.c,v 1.2.4.2 1996/06/13 18:14:58 cgd Exp $	*/

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
#include <sys/malloc.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <vm/vm.h>

#include <machine/bus.h>

#include <alpha/pci/apecsreg.h>
#include <alpha/pci/lcareg.h>

#if (APECS_PCI_SPARSE != LCA_PCI_SPARSE) || (APECS_PCI_DENSE != LCA_PCI_DENSE)
#error Memory addresses do not match up?
#endif

#define	CHIP	apecs_lca

/* Dense region 1 */
#define	CHIP_D_MEM_W1_START(v)	0x00000000
#define	CHIP_D_MEM_W1_END(v)	0xffffffff
#define	CHIP_D_MEM_W1_BASE(v)	APECS_PCI_DENSE
#define	CHIP_D_MEM_W1_MASK(v)	0xffffffff

/* Sparse region 1 */
#define	CHIP_S_MEM_W1_START(v)	0x00000000
#define	CHIP_S_MEM_W1_END(v)	0x00ffffff
#define	CHIP_S_MEM_W1_BASE(v)	APECS_PCI_SPARSE
#define	CHIP_S_MEM_W1_MASK(v)	0x07ffffff

/* Sparse region 2 */
#define	CHIP_S_MEM_W2_START(v)	0x01000000		/* XXX from HAXR1 */
#define	CHIP_S_MEM_W2_END(v)	0xfeffffff		/* XXX from HAXR1 */
#define	CHIP_S_MEM_W2_BASE(v)	APECS_PCI_SPARSE
#define	CHIP_S_MEM_W2_MASK(v)	0x07ffffff

#include "pcs_bus_mem_common.c"
