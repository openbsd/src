/*	$NetBSD: mfbvar.h,v 1.1 1996/09/21 03:06:37 jonathan Exp $	*/

/* 
 * Initialize a Turbochannel MFB 1280x1024x1 2-d framebuffer,
 * so it can be used as a bitmapped glass-tty console device.
 */
extern int
mcfbinit __P((struct fbinfo *fi, caddr_t base, int unit, int silent));
