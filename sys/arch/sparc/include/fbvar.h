/*	$NetBSD: fbvar.h,v 1.7 1996/02/27 22:09:39 thorpej Exp $ */

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)fbvar.h	8.1 (Berkeley) 6/11/93
 */

/*
 * Frame buffer variables.  All frame buffer drivers must provide the
 * following in order to participate.
 */

#ifdef RASTERCONSOLE
#include <dev/rcons/rcons.h>
#endif

struct fbdriver {
	/* device unblank function (force kernel output to display) */
	void	(*fbd_unblank) __P((struct device *));
	int	(*fbd_open) __P((dev_t, int, int, struct proc *));
	int	(*fbd_close) __P((dev_t, int, int, struct proc *));
	int	(*fbd_ioctl) __P((dev_t, u_long, caddr_t, int, struct proc *));
	int	(*fbd_mmap) __P((dev_t, int, int));
#ifdef notyet
	void	(*fbd_wrrop)();		/* `write region' rasterop */
	void	(*fbd_cprop)();		/* `copy region' rasterop */
	void	(*fbd_clrop)();		/* `clear region' rasterop */
#endif
};

struct fbdevice {
	int	fb_major;		/* XXX */
	struct	fbtype fb_type;		/* what it says */
	caddr_t	fb_pixels;		/* display RAM */
	int	fb_linebytes;		/* bytes per display line */

	struct	fbdriver *fb_driver;	/* pointer to driver */
	struct	device *fb_device;	/* parameter for fbd_unblank */

	int	fb_flags;		/* misc. flags */
#define	FB_FORCE	0x00000001	/* force device into /dev/fb */
#define	FB_PFOUR	0x00010000	/* indicates fb is a pfour fb */
#define FB_USERMASK	(FB_FORCE)	/* flags that the user can set */

	volatile u_int32_t *fb_pfour;	/* pointer to pfour register */

#ifdef RASTERCONSOLE
	/* Raster console emulator state */
	struct	rconsole fb_rcons;
#endif
};

void	fb_attach __P((struct fbdevice *, int));
void	fb_setsize __P((struct fbdevice *, int, int, int, int, int));
#ifdef RASTERCONSOLE
void	fbrcons_init __P((struct fbdevice *));
#endif
#if defined(SUN4)
int	fb_pfour_id __P((void *));
int	fb_pfour_get_video __P((struct fbdevice *));
void	fb_pfour_set_video __P((struct fbdevice *, int));
#endif
