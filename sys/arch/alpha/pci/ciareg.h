/*	$NetBSD: ciareg.h,v 1.1.4.3 1996/06/13 18:35:27 cgd Exp $	*/

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
 * 21171 Chipset registers and constants.
 *
 * Taken from XXX
 */

#define	REGVAL(r)	(*(int32_t *)phystok0seg(r))

/*
 * Base addresses
 */
#define	CIA_PCI_SMEM1	0x8000000000L
#define	CIA_PCI_SMEM2	0x8400000000L
#define	CIA_PCI_SMEM3	0x8500000000L
#define	CIA_PCI_SIO1	0x8580000000L
#define	CIA_PCI_SIO2	0x85c0000000L
#define	CIA_PCI_DENSE	0x8600000000L
#define	CIA_PCI_CONF	0x8700000000L
#define	CIA_PCI_IACK	0x8720000000L
#define	CIA_CSRS	0x8740000000L
#define	CIA_PCI_MC_CSRS	0x8750000000L
#define	CIA_PCI_ATRANS	0x8760000000L

/*
 * General CSRs
 */

#define	CIA_CSR_HAE_MEM	(CIA_CSRS + 0x400)

#define		HAE_MEM_REG1_START(x)	(((u_int32_t)(x) & 0xe0000000) << 0)
#define		HAE_MEM_REG1_MASK	0x1fffffff
#define		HAE_MEM_REG2_START(x)	(((u_int32_t)(x) & 0x0000f800) << 16)
#define		HAE_MEM_REG2_MASK	0x07ffffff
#define		HAE_MEM_REG3_START(x)	(((u_int32_t)(x) & 0x000000fc) << 16)
#define		HAE_MEM_REG3_MASK	0x03ffffff

#define	CIA_CSR_HAE_IO	(CIA_CSRS + 0x440)

#define		HAE_IO_REG1_START(x)	0
#define		HAE_IO_REG1_MASK	0x01ffffff
#define		HAE_IO_REG2_START(x)	(((u_int32_t)(x) & 0xfe000000) << 0)
#define		HAE_IO_REG2_MASK	0x01ffffff
