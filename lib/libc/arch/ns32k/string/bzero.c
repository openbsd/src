/* 
 * Mach Operating System
 * Copyright (c) 1991,1990,1989 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: bzero.c,v 1.2 1996/08/19 08:17:02 tholo Exp $";
#endif /* LIBC_SCCS and not lint */

void
bzero(addr, bcount)
	register unsigned addr;
	register unsigned bcount;
{
	register int    i;

	if (bcount == 0)	/* sanity */
		return;
	switch (addr & 3) {
	    case 1:
		*((char *) addr++) = 0;
		if (--bcount == 0)
			return;
	    case 2:
		*((char *) addr++) = 0;
		if (--bcount == 0)
			return;
	    case 3:
		*((char *) addr++) = 0;
		if (--bcount == 0)
			return;
	    default:
		break;
	}

	for (i = bcount >> 2; i; i--, addr += 4)
		*((int *) addr) = 0;

	switch (bcount & 3) {
	    case 3: *((char*)addr++) = 0;
	    case 2: *((char*)addr++) = 0;
	    case 1: *((char*)addr++) = 0;
	    default:break;
	}
}
