/*	$NetBSD: dcvar.h,v 1.2 1996/01/29 22:52:18 jonathan Exp $	*/

/*
 * External declarations from DECstation dc serial driver.
 */

extern int dcGetc __P ((dev_t dev));
extern int dcparam __P((register struct tty *tp, register struct termios *t));
extern void dcPutc __P((dev_t dev, int c));

