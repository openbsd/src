/*	$OpenBSD: fb.c,v 1.14 1998/03/20 05:18:27 todd Exp $	*/
/*	$NetBSD: fb.c,v 1.23 1997/07/07 23:30:22 pk Exp $ */

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
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/conf.h>

#include <machine/autoconf.h>
#include <machine/fbio.h>
#include <machine/kbd.h>
#include <machine/fbvar.h>
#include <machine/conf.h>
#if defined(SUN4)
#include <machine/eeprom.h>
#include <sparc/dev/pfourreg.h>
#endif

static struct fbdevice *devfb;


void
fb_unblank()
{

	if (devfb)
		(*devfb->fb_driver->fbd_unblank)(devfb->fb_device);
}

void
fb_attach(fb, isconsole)
	struct fbdevice *fb;
	int isconsole;
{
	static int no_replace, seen_force;

	/*
	 * We've already had a framebuffer forced into /dev/fb.  Don't
	 * allow any more, even if this is the console.
	 */
	if (seen_force) {
		if (devfb) {	/* sanity */
			printf("%s: /dev/fb already full\n",
				fb->fb_device->dv_xname);
			return;
		} else
			seen_force = 0;
	}

	/*
	 * Check to see if we're being forced into /dev/fb.
	 */
	if (fb->fb_flags & FB_FORCE) {
		if (devfb)
			printf("%s: forcefully replacing %s\n",
				fb->fb_device->dv_xname,
				devfb->fb_device->dv_xname);
		devfb = fb;
		seen_force = no_replace = 1;
		goto attached;
	}

	/*
	 * Check to see if we're the console.  If we are, then replace
	 * any currently existing framebuffer.
	 */
	if (isconsole) {
		if (devfb)
			printf("%s: replacing %s\n", fb->fb_device->dv_xname,
				devfb->fb_device->dv_xname);
		devfb = fb;
		no_replace = 1;
		goto attached;
	}

	/*
	 * For the final case, we check to see if we can replace an
	 * existing framebuffer, if not, say so and return.
	 */
	if (no_replace) {
		if (devfb) {	/* sanity */
			printf("%s: /dev/fb already full\n",
				fb->fb_device->dv_xname);
			return;
		} else
			no_replace = 0;
	}

	if (devfb)
		printf("%s: replacing %s\n", fb->fb_device->dv_xname,
			devfb->fb_device->dv_xname);
	devfb = fb;

 attached:
	printf("%s: attached to /dev/fb\n", devfb->fb_device->dv_xname);
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
		if (CPU_ISSUN4M) {   /* 4m has framebuffer on obio */
			fb->fb_type.fb_width = getpropint(node, "width",
							  def_width);
			fb->fb_type.fb_height = getpropint(node, "height",
							   def_height);
			fb->fb_linebytes = getpropint(node, "linebytes",
			    (fb->fb_type.fb_width * depth) / 8);
			break;
		}
		/* Set up some defaults. */
		fb->fb_type.fb_width = def_width;
		fb->fb_type.fb_height = def_height;

		/*
		 * This is not particularly useful on Sun 4 VME framebuffers.
		 * The EEPROM only contains info about the built-in.
		 */
		if (CPU_ISSUN4 && (bustype == BUS_VME16 ||
		    bustype == BUS_VME32))
			goto donesize;

#if defined(SUN4)
		if (CPU_ISSUN4) {
			struct eeprom *eep = (struct eeprom *)eeprom_va;

			if (fb->fb_flags & FB_PFOUR) {
				volatile u_int32_t pfour;

				/*
				 * Some pfour framebuffers, e.g. the
				 * cgsix, don't encode resolution the
				 * same, so the driver handles that.
				 * The driver can let us know that it
				 * needs to do this by not mapping in
				 * the pfour register by the time this
				 * routine is called.
				 */
				if (fb->fb_pfour == NULL)
					goto donesize;

				pfour = *fb->fb_pfour;

				/*
				 * Use the pfour register to determine
				 * the size.  Note that the cgsix and
				 * cgeight don't use this size encoding.
				 * In this case, we have to settle
				 * for the defaults we were provided
				 * with.
				 */
				if ((PFOUR_ID(pfour) == PFOUR_ID_COLOR24) ||
				    (PFOUR_ID(pfour) == PFOUR_ID_FASTCOLOR))
					goto donesize;

				switch (PFOUR_SIZE(pfour)) {
				case PFOUR_SIZE_1152X900:
					fb->fb_type.fb_width = 1152;
					fb->fb_type.fb_height = 900;
					break;

				case PFOUR_SIZE_1024X1024:
					fb->fb_type.fb_width = 1024;
					fb->fb_type.fb_height = 1024;
					break;

				case PFOUR_SIZE_1280X1024:
					fb->fb_type.fb_width = 1280;
					fb->fb_type.fb_height = 1024;
					break;

				case PFOUR_SIZE_1600X1280:
					fb->fb_type.fb_width = 1600;
					fb->fb_type.fb_height = 1280;
					break;

				case PFOUR_SIZE_1440X1440:
					fb->fb_type.fb_width = 1440;
					fb->fb_type.fb_height = 1440;
					break;

				case PFOUR_SIZE_640X480:
					fb->fb_type.fb_width = 640;
					fb->fb_type.fb_height = 480;
					break;

				default:
					/*
					 * XXX: Do nothing, I guess.
					 * Should we print a warning about
					 * an unknown value? --thorpej
					 */
					break;
				}
			} else if (eep != NULL) {
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
		if (CPU_ISSUN4M) {
			/* XXX: need code to find 4/600 vme screen size */
		}
#endif /* SUN4M */

 donesize:
		fb->fb_linebytes = (fb->fb_type.fb_width * depth) / 8;
		break;

	case BUS_SBUS:
		fb->fb_type.fb_width = getpropint(node, "width", def_width);
		fb->fb_type.fb_height = getpropint(node, "height", def_height);
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

static void fb_bell __P((int));

#if !(defined(RASTERCONS_FULLSCREEN) || defined(RASTERCONS_SMALLFONT))
static int a2int __P((char *, int));

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
#endif

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
	/* Setup the static font, use a small font if display is < 800x600 */
	if(rc->rc_height * rc->rc_width < 800*600)
		rc->rc_font = &console_font_fixed;
	else
		rc->rc_font = &console_font;

	rc->rc_maxcol = rc->rc_width / rc->rc_font->width;
	rc->rc_maxrow = rc->rc_height / rc->rc_font->height;
#if !defined(RASTERCONS_FULLSCREEN) && !defined(RASTERCONS_SMALLFONT)
#if defined(SUN4)
	if (CPU_ISSUN4) {
		struct eeprom *eep = (struct eeprom *)eeprom_va;

		rc->rc_maxcol = min(rc->rc_maxcol,
		    (eep && eep->eeTtyCols) ? eep->eeTtyCols : 80);
		rc->rc_maxrow = min(rc->rc_maxrow,
		    (eep && eep->eeTtyRows) ? eep->eeTtyRows : 34);
	}
#endif /* SUN4 */

	if (!CPU_ISSUN4) {
		rc->rc_maxcol = min(rc->rc_maxcol,
		    a2int(getpropstring(optionsnode, "screen-#columns"), 80));
		rc->rc_maxrow = min(rc->rc_maxrow,
		    a2int(getpropstring(optionsnode, "screen-#rows"), 34));
	}
#endif /* !RASTERCONS_FULLSCREEN && !RASTERCONS_SMALLFONT */

#if !(defined(RASTERCONS_FULLSCREEN) || defined(RASTERCONS_SMALLFONT))
	/* Determine addresses of prom emulator row and column */
	if (CPU_ISSUN4 ||
	    romgetcursoraddr(&rc->rc_row, &rc->rc_col))
#endif
		rc->rc_row = rc->rc_col = NULL;

	rc->rc_bell = fb_bell;
	rcons_init(rc);
	/* Hook up virtual console */
	v_putc = rcons_cnputc;
}

int
fbrcons_rows()
{
	return (devfb ? devfb->fb_rcons.rc_maxrow : 0);
}

int
fbrcons_cols()
{
	return (devfb ? devfb->fb_rcons.rc_maxcol : 0);
}
#endif /* RASTERCONSOLE */

#if defined(SUN4)
/*
 * Support routines for pfour framebuffers.
 */

/*
 * Probe for a pfour framebuffer.  Return values:
 *
 *	PFOUR_NOTPFOUR		framebuffer is not a pfour
 *				framebuffer
 *
 *	otherwise returns pfour ID
 */
int
fb_pfour_id(va)
	void *va;
{
	volatile u_int32_t val, save, *pfour = va;

	/* Read the pfour register. */
	save = *pfour;

	/*
	 * Try to modify the type code.  If it changes, put the
	 * original value back, and notify the caller that it's
	 * not a pfour framebuffer.
	 */
	val = save & ~PFOUR_REG_RESET;
	*pfour = (val ^ PFOUR_FBTYPE_MASK);
	if ((*pfour ^ val) & PFOUR_FBTYPE_MASK) {
		*pfour = save;
		return (PFOUR_NOTPFOUR);
	}

	return (PFOUR_ID(val));
}

/*
 * Return the status of the video enable.
 */
int
fb_pfour_get_video(fb)
	struct fbdevice *fb;
{

	return ((*fb->fb_pfour & PFOUR_REG_VIDEO) != 0);
}

/*
 * Enable or disable the framebuffer.
 */
void
fb_pfour_set_video(fb, enable)
	struct fbdevice *fb;
	int enable;
{
	volatile u_int32_t pfour;

	pfour = *fb->fb_pfour & ~(PFOUR_REG_INTCLR|PFOUR_REG_VIDEO);
	*fb->fb_pfour = pfour | (enable ? PFOUR_REG_VIDEO : 0);
}
#endif /* SUN4 */
