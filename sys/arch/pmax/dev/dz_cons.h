/*	$OpenBSD: dz_cons.h,v 1.1 2000/08/19 18:36:19 maja Exp $	*/
/*	$NetBSD: dc_cons.h,v 1.1 1996/10/13 03:42:17 jonathan Exp $	*/

#ifdef _KERNEL
#ifndef _DZ_CONS_H
#define _DZ_CONS_H

/*
 * Following declaratios for console code.
 * XXX should be redesigned to expose less driver internals.
 */
void dz_consinit __P((dev_t dev, dcregs *dcaddr));
extern int dzGetc __P ((dev_t dev));
extern int dzparam __P((register struct tty *tp, register struct termios *t));
extern void dzPutc __P((dev_t dev, int c));

#endif	/* _DZ_CONS_H */
#endif	/* _KERNEL */
