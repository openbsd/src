/*	$NetBSD: tc_conf.h,v 1.1 1995/12/20 00:43:32 cgd Exp $	*/

/*
 * Copyright (c) 1995 Carnegie-Mellon University.
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
 * Machine-specific TurboChannel configuration definitions.
 */

#ifdef DEC_3000_500
extern void	tc_3000_500_intr_setup __P((void));
extern void	tc_3000_500_iointr __P((void *, int));

extern void	tc_3000_500_intr_establish __P((struct device *, void *,
		    tc_intrlevel_t, int (*)(void *), void *));
extern void	tc_3000_500_intr_disestablish __P((struct device *, void *));

extern int	tc_3000_500_nslots;
extern struct tc_slotdesc tc_3000_500_slots[];
extern int	tc_3000_500_nbuiltins;
extern struct tc_builtin tc_3000_500_builtins[];
#endif /* DEC_3000_500 */

#ifdef DEC_3000_300
extern void	tc_3000_300_intr_setup __P((void));
extern void	tc_3000_300_iointr __P((void *, int));

extern void	tc_3000_300_intr_establish __P((struct device *, void *,
		    tc_intrlevel_t, int (*)(void *), void *));
extern void	tc_3000_300_intr_disestablish __P((struct device *, void *));

extern int	tc_3000_300_nslots;
extern struct tc_slotdesc tc_3000_300_slots[];
extern int	tc_3000_300_nbuiltins;
extern struct tc_builtin tc_3000_300_builtins[];
#endif /* DEC_3000_300 */
