/*	$OpenBSD: zs_cons.h,v 1.1.1.1 1997/10/14 07:25:30 gingold Exp $	*/

extern void *zs_conschan;

extern void nullcnprobe __P((struct consdev *));

extern int  zs_getc __P((void *arg));
extern void zs_putc __P((void *arg, int c));

