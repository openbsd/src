/*	$OpenBSD: trap.h,v 1.1 1998/06/23 19:45:28 mickey Exp $	*/

/* 
 * Copyright (c) 1988-1994, The University of Utah and
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
 * 	Utah $Hdr: trap.h 1.6 94/12/16$
 */

/*
 * Trap type values
 * also known in trap.c for name strings
 */

#define	T_NONEXIST	0
#define	T_HPMACH_CHK	1
#define	T_POW_FAIL	2
#define	T_RECOV_CTR	3
#define	T_EXT_INTP	4
#define	T_LPMACH_CHK	5
#define	T_IPGFT		6
#define	T_IMEM_PROT	7
#define	T_UNIMPL_INST	8
#define	T_BRK_INST	9
#define	T_PRIV_OP	10
#define	T_PRIV_REG	11
#define	T_OVFLO		12
#define	T_COND		13
#define	T_EXCEP		14
#define	T_DPGFT		15
#define	T_IPGFT_NA	16
#define	T_DPGFT_NA	17
#define	T_DMEM_PROT	18
#define	T_DMEM_BREAK	19
#define	T_TLB_DIRTY	20
#define	T_VIO_REF	21
#define	T_EMULAT	22
#define	T_HPRIV_XFR	23
#define	T_LPRIV_XFR	24
#define	T_TAKEN_BR	25

#define	T_DMEM_ACC	26	/* 7100 */
#define	T_DMEM_PID	27	/* 7100 */
#define	T_DMEM_UNALIGN	28	/* 7100 */

#define	T_ICS_OVFL	30	/* SW: interrupt stack overflow */
#define	T_KS_OVFL	31	/* SW: kernel stack overflow */

#define	T_USER		0x20	/* user-mode flag or'ed with type */

/* definitions for <sys/signal.h> */
#define	    ILL_PRIVREG_FAULT	0x4
#define	    ILL_UNIMPL_INST	0x8
#define     ILL_PRIV_OP		0x10
#define     ILL_PRIV_REG	0x11
#define     ILL_OVFLO		0x12
#define     ILL_COND		0x13
#define     ILL_EXCEP		0x14
