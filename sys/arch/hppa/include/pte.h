/*	$OpenBSD: pte.h,v 1.5 1999/09/02 05:43:38 mickey Exp $	*/

/* 
 * Copyright (c) 1990,1993,1994 The University of Utah and
 * the Computer Systems Laboratory at the University of Utah (CSL).
 * All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software is hereby
 * granted provided that (1) source code retains these copyright, permission,
 * and disclaimer notices, and (2) redistributions including binaries
 * reproduce the notices in supporting documentation, and (3) all advertising
 * materials mentioning features or use of this software display the following
 * acknowledgement: ``This product includes software developed by the
 * Computer Systems Laboratory at the University of Utah.''
 *
 * THE UNIVERSITY OF UTAH AND CSL ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS
 * IS" CONDITION.  THE UNIVERSITY OF UTAH AND CSL DISCLAIM ANY LIABILITY OF
 * ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * CSL requests users of this software to return to csl-dist@cs.utah.edu any
 * improvements that they make and grant CSL redistribution rights.
 *
 * 	Utah $Hdr: pmap.h 1.24 94/12/14$
 *	Author: Mike Hibler, Bob Wheeler, University of Utah CSL, 9/90
 */

#ifndef	_MACHINE_PTE_H_
#define	_MACHINE_PTE_H_

/* TLB access/protection values */
#define TLB_REF		0x80000000	/* software only */
#define	TLB_ALIGNED	0x40000000	/* software only */
#define TLB_TRAP	0x20000000
#define TLB_DIRTY	0x10000000
#define TLB_BREAK	0x08000000
#define TLB_AR_MASK	0x07f00000
#define		TLB_AR_NA	0x07300000
#define		TLB_AR_KR	0x00000000
#define		TLB_AR_KRW	0x01000000
#define		TLB_AR_KRX	0x02000000
#define		TLB_AR_KRWX	0x03000000
#define		TLB_AR_UR	0x00f00000
#define		TLB_AR_URW	0x01f00000
#define		TLB_AR_URX	0x02f00000
#define		TLB_AR_URWX	0x03f00000
#define TLB_ZERO	0x00080000	/* software only */
#define TLB_ICACHE	0x00040000	/* software only */
#define TLB_NOTUSED	0x00020000      /* software only */
#define TLB_DCACHE	0x00010000      /* software only */
#define TLB_PID_MASK	0x0000fffe
#define TLB_WIRED	0x00000001	/* software only */

#define TLB_REF_POS	0
#define TLB_ALIGNED_POS	1
#define TLB_TRAP_POS	2
#define TLB_DIRTY_POS	3
#define TLB_BREAK_POS	4
#define TLB_ITLB_POS    12
#define TLB_ICACHE_POS  13
#define TLB_DTLB_POS    14
#define TLB_DCACHE_POS  15
#define TLB_WIRED_POS	31

/* protection for a gateway page */
#define TLB_GATE_PROT	0x04c00000

/* protection for break page */
#define TLB_BREAK_PROT	0x02c00000

#endif	/* _MACHINE_PTE_H_ */
