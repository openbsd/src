/*	$NetBSD: isa_machdep.h,v 1.2 1996/04/12 05:39:02 cgd Exp $	*/

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
 * Types provided to machine-independent ISA code.
 */
typedef struct alpha_isa_chipset *isa_chipset_tag_t;

struct alpha_isa_chipset {
	void	*ic_v;

	void	(*ic_attach_hook) __P((struct device *, struct device *,
		    struct isabus_attach_args *));
	void	*(*ic_intr_establish) __P((void *, int, int, int,
		    int (*)(void *), void *));
	void	(*ic_intr_disestablish) __P((void *, void *));
};

/*
 * Functions provided to machine-independent ISA code.
 */
#define	isa_attach_hook(p, s, a)					\
    (*(a)->iba_ic->ic_attach_hook)((p), (s), (a))
#define	isa_intr_establish(c, i, t, l, f, a)				\
    (*(c)->ic_intr_establish)((c)->ic_v, (i), (t), (l), (f), (a))
#define	isa_intr_disestablish(c, h)					\
    (*(c)->ic_intr_disestablish)((c)->ic_v, (h))
