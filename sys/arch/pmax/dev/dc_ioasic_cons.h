/*	$NetBSD: dc_ioasic_cons.h,v 1.1 1996/09/25 20:48:56 jonathan Exp $	*/

#ifdef _KERNEL
#ifndef _DC_IOASIC_CONS_H
#define _DC_IOASIC_CONS_H

/*
 * Following declaratios for console code.
 * XXX should be redesigned to expose less driver internals.
 */
int dc_ioasic_consinit __P((dev_t dev));

#endif	/* _DCVAR_H */
#endif	/* _KERNEL */
