/*	$NetBSD: makexxboot.c,v 1.1 1995/02/13 23:08:47 cgd Exp $	*/

/*
 * Copyright (c) 1994, 1995 Carnegie-Mellon University.
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

#define BBPAD	0x1e0
struct bb {
	char	bb_pad[BBPAD];	/* disklabel lives in here, actually */
	long	bb_secsize;	/* size of secondary boot block */
	long	bb_secstart;	/* start of secondary boot block */
	long	bb_flags;	/* unknown; always zero */
	long	bb_cksum;	/* checksum of the the boot block, as longs. */
};

main()
{
	struct bb bb;
	long *lp, *ep;

	bzero(&bb, sizeof bb);
	bb.bb_secsize = 16;
	bb.bb_secstart = 1;
	bb.bb_flags = 0;
	bb.bb_cksum = 0;

	for (lp = (long *)&bb, ep = &bb.bb_cksum; lp < ep; lp++)
		bb.bb_cksum += *lp;

	if (write(1, &bb, sizeof bb) != sizeof bb)
		exit(1);

	exit(0);
}
