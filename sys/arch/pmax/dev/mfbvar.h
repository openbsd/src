/*	$NetBSD: mfbvar.h,v 1.2 1997/05/24 09:15:49 jonathan Exp $	*/

/* 
 * Initialize a Turbochannel MFB 1280x1024x1 2-d framebuffer,
 * so it can be used as a bitmapped glass-tty console device.
 */
extern int
mfbinit __P((struct fbinfo *fi, caddr_t base, int unit, int silent));
