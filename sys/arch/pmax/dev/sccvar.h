/*	$NetBSD: sccvar.h,v 1.1 1995/08/04 00:22:02 jonathan Exp $	*/

/*
 * external declarations from DECstation scc driver
 */

extern int sccGetc __P ((dev_t dev));
extern int sccparam __P((register struct tty *tp, register struct termios *t));
extern void sccPutc __P ((dev_t dev, int c));
