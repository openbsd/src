/*	$OpenBSD: siovar.h,v 1.6 1998/06/29 05:32:57 downsj Exp $	*/
/*	$NetBSD: siovar.h,v 1.5 1996/10/23 04:12:34 cgd Exp $	*/

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

void	sio_intr_setup __P((bus_space_tag_t));
void	sio_iointr __P((void *framep, unsigned long vec));

const char *sio_intr_string __P((void *, int));
int	sio_intr_check __P((void *, int, int));
void	*sio_intr_establish __P((void *, int, int, int, int (*)(void *),
	    void *, char *));
void	sio_intr_disestablish __P((void *, void *));

#ifdef EVCNT_COUNTERS
extern struct evcnt sio_intr_evcnt;
#endif
