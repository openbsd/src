/*	$NetBSD: dcvar.h,v 1.1 1995/08/04 00:22:12 jonathan Exp $	*/

/*
 * external declarations from DECstation dc serial driver
 */

extern int dcGetc __P ((dev_t dev));
extern int dcparam __P((register struct tty *tp, register struct termios *t));
extern void dcPutc __P ((dev_t dev, int c));


