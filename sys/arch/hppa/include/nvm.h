/*	$OpenBSD: nvm.h,v 1.1 1998/06/23 19:45:24 mickey Exp $	*/

/* 
 * Copyright (c) 1990, 1994 The University of Utah and
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
 * 	Utah $Hdr: nvm.h 1.4 94/12/14$
 *	Author: Jeff Forys, University of Utah CSL
 */

#ifndef _NVM_
#define	_NVM_

/*
 * The PDC provides access to Non-Volatile Memory (NVM).  If this
 * is implemented (it's HVERSION dependent), the first 256 bytes
 * are formatted as follows:
 *
 *	0x000	+----------------------------+
 *		| Implementation information |
 *	0x024	+----------------------------+
 *		|                            |
 *		|      IPL information       |
 *		|                            |
 *	0x080	+----------------------------+
 *		|                            |
 *		|                            |
 *		|    OS Panic information    |
 *		|                            |
 *		|                            |
 *	0x100	+----------------------------+
 *
 * It appears that there are at least 256 bytes of NVM, and only
 * the "OS Panic information" is not architected.  This means that
 * we can use locations 0x80 - 0xFF for saving information across
 * boots (e.g. boot flags and boot device).  I think we should use
 * the higher portions of this space first, to avoid conflicting
 * with possible future HP-PA plans for the NVM.
 *
 * The PDC requires that NVM be read/written to in word multiples.
 */

/*
 * Boot flags and boot device (0xF4 - 0xFF).
 */

#define	NVM_BOOTDATA	0xF4		/* location of bootdata in NVM */
#define	NVM_BOOTMAGIC	0xACCEDE	/* magic used for bootdata cksum */
#define	NVM_BOOTCKSUM(bd) \
	((unsigned int) NVM_BOOTMAGIC + (bd).flags + (bd).device)

struct bootdata {
	unsigned int cksum;		/* NVM_BOOTMAGIC + flags + device */
	unsigned int flags;		/* boot flags */
	unsigned int device;		/* boot device */
};

#endif	/* _NVM_ */
