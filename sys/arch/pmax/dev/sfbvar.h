/*	$NetBSD: sfbvar.h,v 1.2 1997/05/24 09:15:46 jonathan Exp $	*/

/* 
 * Initialize a Turbochannel SFB  2-d framebuffer,
 * so it can be used as a bitmapped glass-tty console device.
 */
extern int
sfbinit __P((struct fbinfo *fi, caddr_t base, int unit, int silent));
