/*	$OpenBSD: zs_cons.h,v 1.2 2002/03/14 01:26:46 millert Exp $	*/

extern void *zs_conschan;

extern void nullcnprobe(struct consdev *);

extern int  zs_getc(void *arg);
extern void zs_putc(void *arg, int c);

