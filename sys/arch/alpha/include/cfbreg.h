/*	$OpenBSD: cfbreg.h,v 1.4 2003/10/18 20:14:41 jmc Exp $	*/
/*	$NetBSD: cfbreg.h,v 1.1 1996/05/01 23:25:00 cgd Exp $	*/

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

/*
 * Color Frame Buffer definitions, from:
 * ``PMAG-BA TURBOchannel Color Frame Buffer Functional Specification
 * (Revision 1.2)'', available via anonymous FTP from gatekeeper.dec.com.
 *
 * All definitions are in "dense" TurboChannel space.
 */

/*
 * Size of the CFB address space.
 */
#define	CFB_SIZE		0x400000

/*
 * Offsets into slot space of each functional unit.
 */
#define	CFB_FB_OFFSET		0x000000	/* Frame buffer */
#define	CFB_FB_SIZE		0x100000
#define	CFB_RAMDAC_OFFSET	0x200000	/* Bt495 RAMDAC Registers */
#define	CFB_RAMDAC_SIZE		0x100000
#define	CFB_IREQCTRL_OFFSET	0x300000	/* IReq Control region */
#define	CFB_IREQCTRL_SIZE	0x080000

/*
 * Bt459 RAMDAC registers (offsets from CFB_RAMDAC_OFFSET)
 */
#define	CFB_RAMDAC_ADDRLOW	0x0000	/* Address register low byte */
#define	CFB_RAMDAC_ADDRHIGH	0x0004	/* Address register high byte */
#define	CFB_RAMDAC_REGDATA	0x0008	/* Register addressed by addr reg */
#define	CFB_RAMDAC_CMAPDATA	0x000c	/* Colormap loc addressed by addr reg */
