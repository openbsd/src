/*	$NetBSD: dc_cons.h,v 1.1 1996/10/13 03:42:17 jonathan Exp $	*/

#ifdef _KERNEL
#ifndef _DC_CONS_H
#define _DC_CONS_H

/*
 * Following declaratios for console code.
 * XXX should be redesigned to expose less driver internals.
 */
void dc_consinit __P((dev_t dev, dcregs *dcaddr));
extern int dcGetc __P ((dev_t dev));
extern int dcparam __P((register struct tty *tp, register struct termios *t));
extern void dcPutc __P((dev_t dev, int c));

#endif	/* _DCVAR_H */
#endif	/* _KERNEL */
