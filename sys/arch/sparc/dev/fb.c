/*	$NetBSD: fb.c,v 1.11 1995/10/08 01:39:19 pk Exp $ */

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
 *	@(#)fb.c	8.1 (Berkeley) 6/11/93
 */

/*
 * /dev/fb (indirect frame buffer driver).  This is gross; we should
 * just build cdevsw[] dynamically.
 */

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/proc.h>

#include <machine/autoconf.h>
#include <machine/fbio.h>
#include <machine/fbvar.h>
#if defined(SUN4)
#include <machine/eeprom.h>
#endif

static struct fbdevice *devfb;

void
fb_unblank()
{

	if (devfb)
		(*devfb->fb_driver->fbd_unblank)(devfb->fb_device);
}

void
fb_attach(fb)
	struct fbdevice *fb;
{

if (devfb) panic("multiple /dev/fb declarers");
	devfb = fb;
}

int
fbopen(dev, flags, mode, p)
	dev_t dev;
	int flags, mode;
	struct proc *p;
{

	if (devfb == NULL)
		return (ENXIO);
	return (devfb->fb_driver->fbd_open)(dev, flags, mode, p);
}

int
fbclose(dev, flags, mode, p)
	dev_t dev;
	int flags, mode;
	struct proc *p;
{

	return (devfb->fb_driver->fbd_close)(dev, flags, mode, p);
}

int
fbioctl(dev, cmd, data, flags, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flags;
	struct proc *p;
{

	return (devfb->fb_driver->fbd_ioctl)(dev, cmd, data, flags, p);
}

int
fbmmap(dev, off, prot)
	dev_t dev;
	int off, prot;
{
	int (*map)__P((dev_t, int, int)) = devfb->fb_driver->fbd_mmap;

	if (map == NULL)
		return (-1);
	return (map(dev, off, prot));
}

void
fb_setsize(fb, depth, def_width, def_height, node, bustype)
	struct fbdevice *fb;
	int depth, def_width, def_height, node, bustype;
{

	/*
	 * The defaults below match my screen, but are not guaranteed
	 * to be correct as defaults go...
	 */
	switch (bustype) {
	case BUS_VME16:
	case BUS_VME32:
	case BUS_OBIO:
		/* Set up some defaults. */
		fb->fb_type.fb_width = def_width;
		fb->fb_type.fb_height = def_height;

		/*
		 * This is not particularly useful on Sun 4 VME framebuffers.
		 * The EEPROM only contains info about the built-in.
		 */
		if (cputyp == CPU_SUN4 && (bustype == BUS_VME16 ||
		    bustype == BUS_VME32))
			goto donesize;

#if defined(SUN4)
		if (cputyp==CPU_SUN4) {
			struct eeprom *eep = (struct eeprom *)eeprom_va;
			if (eep != NULL) {
				switch (eep->eeScreenSize) {
				case EE_SCR_1152X900:
					fb->fb_type.fb_width = 1152;
					fb->fb_type.fb_height = 900;
					break;

				case EE_SCR_1024X1024:
					fb->fb_type.fb_width = 1024;
					fb->fb_type.fb_height = 1024;
					break;

				case EE_SCR_1600X1280:
					fb->fb_type.fb_width = 1600;
					fb->fb_type.fb_height = 1280;
					break;

				case EE_SCR_1440X1440:
					fb->fb_type.fb_width = 1440;
					fb->fb_type.fb_height = 1440;
					break;

				default:
					/*
					 * XXX: Do nothing, I guess.
					 * Should we print a warning about
					 * an unknown value? --thorpej
					 */
					break;
				}
			}
		}
#endif /* SUN4 */
#if defined(SUN4M)
		if (cputyp==CPU_SUN4M) {
			/* XXX: need code to find 4/600 vme screen size */
		}
#endif /* SUN4M */

 donesize:
		fb->fb_linebytes = (fb->fb_type.fb_width * depth) / 8;
		break;

	case BUS_SBUS:
		fb->fb_type.fb_width = getpropint(node, "width", 1152);
		fb->fb_type.fb_height = getpropint(node, "height", 900);
		fb->fb_linebytes = getpropint(node, "linebytes",
		    (fb->fb_type.fb_width * depth) / 8);
		break;

	default:
		panic("fb_setsize: inappropriate bustype");
		/* NOTREACHED */
	}
}

#ifdef RASTERCONSOLE
#include <machine/kbd.h>

extern int (*v_putc) __P((int));

static int
a2int(cp, deflt)
	register char *cp;
	register int deflt;
{
	register int i = 0;

	if (*cp == '\0')
		return (deflt);
	while (*cp != '\0')
		i = i * 10 + *cp++ - '0';
	return (i);
}

static void
fb_bell(on)
	int on;
{
	(void)kbd_docmd(on?KBD_CMD_BELL:KBD_CMD_NOBELL, 0);
}

#include <sparc/dev/rcons_font.h>

void
fbrcons_init(fb)
	struct fbdevice *fb;
{
	struct rconsole	*rc = &fb->fb_rcons;

	/*
	 * Common glue for rconsole initialization
	 * XXX - mostly duplicates values with fbdevice.
	 */
	rc->rc_linebytes = fb->fb_linebytes;
	rc->rc_pixels = fb->fb_pixels;
	rc->rc_width = fb->fb_type.fb_width;
	rc->rc_height = fb->fb_type.fb_height;
	rc->rc_depth = fb->fb_type.fb_depth;
	/* Setup the static font */
	rc->rc_font = &console_font;

#if defined(RASTERCONS_FULLSCREEN) || defined(RASTERCONS_SMALLFONT)
	rc->rc_maxcol = rc->rc_width / rc->rc_font->width;
	rc->rc_maxrow = rc->rc_height / rc->rc_font->height; 
#else
#if defined(SUN4)
	if (cputyp == CPU_SUN4) {
		struct eeprom *eep = (struct eeprom *)eeprom_va;

		if (eep == NULL) {
			rc->rc_maxcol = 80;
			rc->rc_maxrow = 34;
		} else {
			rc->rc_maxcol = eep->eeTtyCols;
			rc->rc_maxrow = eep->eeTtyRows;
		}
	}
#endif /* SUN4 */
#if defined(SUN4C) || defined(SUN4M)
	if (cputyp != CPU_SUN4) {
		rc->rc_maxcol =
		    a2int(getpropstring(optionsnode, "screen-#columns"), 80);
		rc->rc_maxrow =
		    a2int(getpropstring(optionsnode, "screen-#rows"), 34);
	}
#endif /* SUN4C || SUN4M */
#endif /* RASTERCONS_FULLSCREEN || RASTERCONS_SMALLFONT */

#if !(defined(RASTERCONS_FULLSCREEN) || defined(RASTERCONS_SMALLFONT))
	/* Determine addresses of prom emulator row and column */
	if (cputyp == CPU_SUN4 ||
	    romgetcursoraddr(&rc->rc_row, &rc->rc_col))
#endif
		rc->rc_row = rc->rc_col = NULL;

	rc->rc_bell = fb_bell;
	rcons_init(rc);
	/* Hook up virtual console */
	v_putc = (int (*) __P((int)))rcons_cnputc;
}
#endif
