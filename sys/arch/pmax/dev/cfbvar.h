/*	$NetBSD: cfbvar.h,v 1.1 1996/09/21 03:06:37 jonathan Exp $	*/

/* 
 * Initialize a Turbochannel CFB  dumb 2-d framebuffer,
 * so it can be used as a bitmapped glass-tty console device.
 */
extern int
cfbinit __P((struct fbinfo *fi, caddr_t base, int unit, int silent));
