/*	$OpenBSD: dz_ds_cons.h,v 1.1 2000/08/19 18:36:19 maja Exp $	*/
/*	$NetBSD: dc_ds_cons.h,v 1.1 1996/09/25 20:48:55 jonathan Exp $	*/

#ifdef _KERNEL
#ifndef _DZ_DS_CONS_H
#define _DZ_DS_CONS_H

/*
 * Following declaratios for console code.
 * XXX should be redesigned to expose less driver internals.
 */
int dz_ds_consinit __P((dev_t dev));

#endif	/* _DZ_DS_CONS_H */
#endif	/* _KERNEL */
