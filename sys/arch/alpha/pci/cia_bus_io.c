/*	$NetBSD: cia_bus_io.c,v 1.2.4.2 1996/06/13 18:14:59 cgd Exp $	*/

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

#include <alpha/pci/ciareg.h>
#include <alpha/pci/ciavar.h>

#define	CHIP		cia

/* IO region 1 */
#define CHIP_IO_W1_START(v)						\
	    HAE_IO_REG1_START(((struct cia_config *)(v))->cc_hae_io)
#define CHIP_IO_W1_END(v)						\
	    (CHIP_IO_W1_START(v) + HAE_IO_REG1_MASK)
#define CHIP_IO_W1_BASE(v)						\
	    CIA_PCI_SIO1
#define CHIP_IO_W1_MASK(v)						\
	    HAE_IO_REG1_MASK

/* IO region 2 */
#define CHIP_IO_W2_START(v)						\
	    HAE_IO_REG2_START(((struct cia_config *)(v))->cc_hae_io)
#define CHIP_IO_W2_END(v)						\
	    (CHIP_IO_W2_START(v) + HAE_IO_REG2_MASK)
#define CHIP_IO_W2_BASE(v)						\
	    CIA_PCI_SIO2
#define CHIP_IO_W2_MASK(v)						\
	    HAE_IO_REG2_MASK

#include "pcs_bus_io_common.c"
