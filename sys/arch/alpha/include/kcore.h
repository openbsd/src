/*	$OpenBSD: kcore.h,v 1.1 1996/10/30 22:39:11 niklas Exp $	*/
/*	$NetBSD: kcore.h,v 1.1 1996/10/01 18:38:05 cgd Exp $	*/

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

#ifndef _ALPHA_KCORE_H_
#define _ALPHA_KCORE_H_

typedef struct cpu_kcore_hdr {
	u_int64_t	lev1map_pa;		/* PA of Lev1map */
	u_int64_t	page_size;		/* Page size */
        phys_ram_seg_t  core_seg;		/* Core addrs; only one seg */
} cpu_kcore_hdr_t;

#endif /* _ALPHA_KCORE_H_ */
