/*	$NetBSD: xcfbvar.h,v 1.2 1996/09/21 03:06:38 jonathan Exp $	*/

/* 
 * Initialize a Personal Decstation baseboard framebuffer,
 * so it can be used as a bitmapped glass-tty console device.
 */
extern int
xcfbinit __P((struct fbinfo *fi, caddr_t base, int unit, int silent));
