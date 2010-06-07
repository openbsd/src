/*	$OpenBSD: fb.c,v 1.51 2010/06/07 19:43:45 miod Exp $	*/
/*	$NetBSD: fb.c,v 1.23 1997/07/07 23:30:22 pk Exp $ */

/*
 * Copyright (c) 2002, 2004, 2008  Miodrag Vallat.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 *
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
 * 3. Neither the name of the University nor the names of its contributors
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
 * Common wsdisplay framebuffer drivers helpers.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/conf.h>

#include <machine/autoconf.h>
#include <machine/conf.h>
#if defined(SUN4)
#include <machine/eeprom.h>
#include <sparc/dev/pfourreg.h>
#endif
#if defined(SUN4C) || defined(SUN4M)
#include <machine/bsd_openprom.h>
#endif

#include <dev/wscons/wsdisplayvar.h>
#include <dev/rasops/rasops.h>
#include <machine/fbvar.h>

#include "wsdisplay.h"

/*
 * Sun specific color indexes.
 * Black is not really 7, but rather ~0; to fit within the 8 ANSI color
 * palette we are using on console, we pick (~0) & 0x07 instead.
 * This essentially swaps WSCOL_BLACK and WSCOL_WHITE.
 */
#define	WSCOL_SUN_WHITE		0
#define	WSCOL_SUN_BLACK		7

/*
 * emergency unblank code
 * XXX should be somewhat moved to wscons MI code
 */

void (*fb_burner)(void *, u_int, u_int);
void *fb_cookie;

void
fb_unblank()
{
	if (fb_burner != NULL)
		(*fb_burner)(fb_cookie, 1, 0);
}

#if NWSDISPLAY > 0

#if defined(SUN4C) || defined(SUN4M)
static int a2int(char *, int);
#endif
int	fb_get_console_metrics(int *, int *, int *, int *);
void	fb_initwsd(struct sunfb *);
void	fb_updatecursor(struct rasops_info *);

int	fb_alloc_screen(void *, const struct wsscreen_descr *, void **,
	    int *, int *, long *);
void	fb_free_screen(void *, void *);
int	fb_show_screen(void *, void *, int, void (*)(void *, int, int),
	    void *);

void
fb_setsize(struct sunfb *sf, int def_depth, int def_width, int def_height,
    int node, int bustype)
{
	int def_linebytes;

	switch (bustype) {
	case BUS_VME16:
	case BUS_VME32:
	case BUS_OBIO:
#if defined(SUN4M)
		/* 4m may have SBus-like framebuffer on obio */
		if (CPU_ISSUN4M) {
			goto obpsize;
		}
#endif
		/* Set up some defaults. */
		sf->sf_width = def_width;
		sf->sf_height = def_height;
		sf->sf_depth = def_depth;

#if defined(SUN4)
		/*
		 * This is not particularly useful on Sun 4 VME framebuffers.
		 * The EEPROM only contains info about the built-in.
		 */
		if (CPU_ISSUN4 && bustype == BUS_OBIO) {
			struct eeprom *eep = (struct eeprom *)eeprom_va;

			if (ISSET(sf->sf_flags, FB_PFOUR)) {
				volatile u_int32_t pfour;
				u_int size;

				pfour = *sf->sf_pfour;

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
					size = 0x00; /* invalid */
				else
					size = PFOUR_SIZE(pfour);

				switch (size) {
				case PFOUR_SIZE_1152X900:
					sf->sf_width = 1152;
					sf->sf_height = 900;
					break;
				case PFOUR_SIZE_1024X1024:
					sf->sf_width = 1024;
					sf->sf_height = 1024;
					break;
				case PFOUR_SIZE_1280X1024:
					sf->sf_width = 1280;
					sf->sf_height = 1024;
					break;
				case PFOUR_SIZE_1600X1280:
					sf->sf_width = 1600;
					sf->sf_height = 1280;
					break;
				case PFOUR_SIZE_1440X1440:
					sf->sf_width = 1440;
					sf->sf_height = 1440;
					break;
				case PFOUR_SIZE_640X480:
					sf->sf_width = 640;
					sf->sf_height = 480;
					break;
				}
			} else if (eep != NULL) {
				switch (eep->ee_diag.eed_scrsize) {
				case EED_SCR_1152X900:
					sf->sf_width = 1152;
					sf->sf_height = 900;
					break;
				case EED_SCR_1024X1024:
					sf->sf_width = 1024;
					sf->sf_height = 1024;
					break;
				case EED_SCR_1600X1280:
					sf->sf_width = 1600;
					sf->sf_height = 1280;
					break;
				case EED_SCR_1440X1440:
					sf->sf_width = 1440;
					sf->sf_height = 1440;
					break;
				case EED_SCR_640X480:
					sf->sf_width = 640;
					sf->sf_height = 480;
					break;
				case EED_SCR_1280X1024:
					sf->sf_width = 1280;
					sf->sf_height = 1024;
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

		sf->sf_linebytes = (sf->sf_width * sf->sf_depth) / 8;
		break;

	case BUS_SBUS:
#if defined(SUN4M)
obpsize:
#endif
		sf->sf_depth = getpropint(node, "depth", def_depth);
		sf->sf_width = getpropint(node, "width", def_width);
		sf->sf_height = getpropint(node, "height", def_height);

		def_linebytes =
		    roundup(sf->sf_width, sf->sf_depth) * sf->sf_depth / 8;
		sf->sf_linebytes = getpropint(node, "linebytes", def_linebytes);

		/*
		 * XXX If we are configuring a board in a wider depth level
		 * than the mode it is currently operating in, the PROM will
		 * return a linebytes property tied to the current depth value,
		 * which is NOT what we are relying upon!
		 */
		if (sf->sf_linebytes < (sf->sf_width * sf->sf_depth) / 8)
			sf->sf_linebytes = def_linebytes;

		break;
	}

	sf->sf_fbsize = sf->sf_height * sf->sf_linebytes;
}

#if defined(SUN4C) || defined(SUN4M)
static int
a2int(char *cp, int deflt)
{
	int i = 0;

	if (*cp == '\0')
		return (deflt);
	while (*cp != '\0')
		i = i * 10 + *cp++ - '0';
	return (i);
}
#endif

/* setup the embedded wsscreen_descr structure from rasops settings */
void
fb_initwsd(struct sunfb *sf)
{
	strlcpy(sf->sf_wsd.name, "std", sizeof(sf->sf_wsd.name));
	sf->sf_wsd.capabilities = sf->sf_ro.ri_caps;
	sf->sf_wsd.nrows = sf->sf_ro.ri_rows;
	sf->sf_wsd.ncols = sf->sf_ro.ri_cols;
	sf->sf_wsd.textops = &sf->sf_ro.ri_ops;
}

void
fb_updatecursor(struct rasops_info *ri)
{
	struct sunfb *sf = (struct sunfb *)ri->ri_hw;

	if (sf->sf_crowp != NULL)
		*sf->sf_crowp = ri->ri_crow;
	if (sf->sf_ccolp != NULL)
		*sf->sf_ccolp = ri->ri_ccol;
}

void
fbwscons_init(struct sunfb *sf, int isconsole)
{
	struct rasops_info *ri = &sf->sf_ro;
	int cols, rows;
#if defined(SUN4C) || defined(SUN4M)
	int fw, fh, wt, wl;
#endif

	/* ri_hw and ri_bits must have already been setup by caller */
	ri->ri_flg = RI_FULLCLEAR;
	if (!isconsole)
		ri->ri_flg |= RI_CLEAR;
	ri->ri_depth = sf->sf_depth;
	ri->ri_stride = sf->sf_linebytes;
	ri->ri_width = sf->sf_width;
	ri->ri_height = sf->sf_height;

#if defined(SUN4C) || defined(SUN4M)
	if (CPU_ISSUN4COR4M) {
		rows = a2int(getpropstring(optionsnode, "screen-#rows"), 34);
		cols = a2int(getpropstring(optionsnode, "screen-#columns"), 80);
	}
#endif
#if defined(SUN4)
	if (CPU_ISSUN4) {
		struct eeprom *ep = (struct eeprom *)eeprom_va;

		if (ep != NULL) {
			rows = (u_short)ep->ee_diag.eed_rowsize;
			cols = (u_short)ep->ee_diag.eed_colsize;
			/* deal with broken nvram contents... */
			if (rows <= 0)
				rows = 34;
			if (cols <= 0)
				cols = 80;
		} else {
			rows = 34;
			cols = 80;
		}
	}
#endif

	/*
	 * If the framebuffer width is under 960 pixels, rasops will
	 * switch from the 12x22 font to the more adequate 8x16 font
	 * here.
	 * If we are the console device, we need to adjust two things:
	 * - the display row should be overrided from the current PROM
	 *   metrics, since it will not match the PROM reality anymore.
	 * - the screen needs to be cleared.
	 *
	 * However, to accomodate laptops with specific small fonts,
	 * it is necessary to compare the resolution with the actual
	 * font metrics.
	 *
	 * Note that, on sun4 systems, no frame buffer supports display
	 * resolutions requiring a font switch, so it is safe to bypass
	 * this chunk.
	 */

#if defined(SUN4C) || defined(SUN4M)
	if (CPU_ISSUN4COR4M && isconsole) {
		if (fb_get_console_metrics(&fw, &fh, &wt, &wl) != 0) {
			/*
			 * Assume a 12x22 prom font and a centered
			 * 80x34 console window.
			 */
			fw = 12; fh = 22;
			wt = wl = 0;
		} else {
			/*
			 * We have valid font metrics, but the
			 * window-top and window-left values might not
			 * be available.  Also, we have to make sure
			 * their values are consistent with the font
			 * metrics.
			 */
			if (wt <= 0 || wt > sf->sf_height - rows * fh ||
			    wl <= 0 || wl > sf->sf_width - cols * fw)
				wt = wl = 0;
		}
		if (wt == 0 /* || wl == 0 */) {
			ri->ri_flg |= RI_CENTER;

			/*
			 * Since the console window might not be
			 * centered (e.g. on a 1280x1024 vigra
			 * VS-12 frame buffer), have rasops
			 * clear the margins even if the screen is
			 * not cleared.
			 */
			ri->ri_flg |= RI_CLEARMARGINS;
		}

		if (ri->ri_wsfcookie != 0) {
			/* driver handles font issues. do nothing. */
		} else {
			/*
			 * If the PROM uses a different font than the
			 * one we are expecting it to use, or if the
			 * display is shorter than 960 pixels wide,
			 * we'll force a screen clear.
			 */
			if (fw != 12 || sf->sf_width < 12 * 80)
				ri->ri_flg |= RI_CLEAR | RI_CENTER;
		}
	} else {
		ri->ri_flg |= RI_CENTER;
	}
#endif

	rasops_init(ri, rows, cols);

#if defined(SUN4C) || defined(SUN4M)
	/*
	 * If this is the console display and there is no font change,
	 * adjust our terminal window to the position of the PROM
	 * window - in case it is not exactly centered.
	 */
	if ((ri->ri_flg & RI_CENTER) == 0) {
		/* code above made sure wt and wl are initialized */
		ri->ri_bits += wt * ri->ri_stride;
		if (ri->ri_depth >= 8)	/* for 15bpp to compute ok */
			ri->ri_bits += wl * ri->ri_pelbytes;
		else
			ri->ri_bits += (wl * ri->ri_depth) >> 3;

		ri->ri_xorigin = wl;
		ri->ri_yorigin = wt;
	}
#endif

	if (sf->sf_depth == 8) {
		/*
		 * If we are running with an indexed palette, compensate
		 * the swap of black and white through ri_devcmap.
		 */
		ri->ri_devcmap[WSCOL_SUN_BLACK] = 0;
		ri->ri_devcmap[WSCOL_SUN_WHITE] = 0xffffffff;
	} else if (sf->sf_depth > 8) {
		/*
		 * If we are running on a direct color frame buffer,
		 * make the ``normal'' white the same as the hilighted
		 * white.
		 */
		ri->ri_devcmap[WSCOL_WHITE] = ri->ri_devcmap[WSCOL_WHITE + 8];
	}
}

void
fbwscons_console_init(struct sunfb *sf, int row)
{
	struct rasops_info *ri = &sf->sf_ro;
	long defattr;

	if (CPU_ISSUN4 || romgetcursoraddr(&sf->sf_crowp, &sf->sf_ccolp))
		sf->sf_ccolp = sf->sf_crowp = NULL;
	if (sf->sf_ccolp != NULL)
		ri->ri_ccol = *sf->sf_ccolp;

	if (ri->ri_flg & RI_CLEAR) {
		/*
		 * If we have cleared the screen, this is because either
		 * we are not the console display, or the font has been
		 * changed.
		 * In this case, choose not to keep pointers to the PROM
		 * cursor position, as the values are likely to be inaccurate
		 * upon shutdown...
		 */
		sf->sf_crowp = sf->sf_ccolp = NULL;
		row = 0;
	}

	if (row < 0) /* no override */ {
		if (sf->sf_crowp != NULL)
			ri->ri_crow = *sf->sf_crowp;
		else
			/* assume last row */
			ri->ri_crow = ri->ri_rows - 1;
	} else {
		ri->ri_crow = row;
	}

	/*
	 * Scale back rows and columns if the font would not otherwise
	 * fit on this display. Without this we would panic later.
	 */
	if (ri->ri_crow >= ri->ri_rows)
		ri->ri_crow = ri->ri_rows - 1;
	if (ri->ri_ccol >= ri->ri_cols)
		ri->ri_ccol = ri->ri_cols - 1;

	/*
	 * Take care of updating the PROM cursor position as well if we can.
	 */
	if (ri->ri_updatecursor == NULL &&
	    (sf->sf_ccolp != NULL || sf->sf_crowp != NULL))
		ri->ri_updatecursor = fb_updatecursor;

	if (ISSET(ri->ri_caps, WSSCREEN_WSCOLORS))
		ri->ri_ops.alloc_attr(ri,
		    WSCOL_BLACK, WSCOL_WHITE, WSATTR_WSCOLORS, &defattr);
	else
		ri->ri_ops.alloc_attr(ri, 0, 0, 0, &defattr);

	fb_initwsd(sf);
	wsdisplay_cnattach(&sf->sf_wsd, ri,
	    ri->ri_ccol, ri->ri_crow, defattr);
}

void
fbwscons_setcolormap(struct sunfb *sf,
    void (*setcolor)(void *, u_int, u_int8_t, u_int8_t, u_int8_t))
{
	int i;
	const u_char *color;

	if (sf->sf_depth <= 8 && setcolor != NULL) {
		for (i = 0; i < 16; i++) {
			color = &rasops_cmap[i * 3];
			setcolor(sf, i, color[0], color[1], color[2]);
		}
		for (i = 240; i < 256; i++) {
			color = &rasops_cmap[i * 3];
			setcolor(sf, i, color[0], color[1], color[2]);
		}
		/*
		 * Compensate for BoW default hardware palette: existing
		 * output (which we do not want to affect) is black on
		 * white with color index 0 being white and 0xff being
		 * black.
		 */
		setcolor(sf, WSCOL_SUN_WHITE, 0xff, 0xff, 0xff);
		setcolor(sf, 0xff ^ WSCOL_SUN_WHITE, 0, 0, 0);
		setcolor(sf, WSCOL_SUN_BLACK, 0, 0, 0);
		setcolor(sf, 0xff ^ (WSCOL_SUN_BLACK), 0xff, 0xff, 0xff);
	}
}

void
fbwscons_attach(struct sunfb *sf, struct wsdisplay_accessops *op, int isconsole)
{
	struct wsemuldisplaydev_attach_args waa;

	if (isconsole == 0) {
		/* done in wsdisplay_cnattach() earlier if console */
		fb_initwsd(sf);
	} else {
		/* remember screen burner routine */
		fb_burner = op->burn_screen;
		fb_cookie = sf;
	}

	/* plug common wsdisplay_accessops if necessary */
	if (op->alloc_screen == NULL) {
		op->alloc_screen = fb_alloc_screen;
		op->free_screen = fb_free_screen;
		op->show_screen = fb_show_screen;
	}

	sf->sf_scrlist[0] = &sf->sf_wsd;
	sf->sf_wsl.nscreens = 1;
	sf->sf_wsl.screens = (const struct wsscreen_descr **)sf->sf_scrlist;

	waa.console = isconsole;
	waa.scrdata = &sf->sf_wsl;
	waa.accessops = op;
	waa.accesscookie = sf;
	waa.defaultscreens = 0;
	config_found(&sf->sf_dev, &waa, wsemuldisplaydevprint);
}

/*
 * Common wsdisplay_accessops routines.
 */
int
fb_alloc_screen(void *v, const struct wsscreen_descr *type,
    void **cookiep, int *curxp, int *curyp, long *attrp)
{
	struct sunfb *sf = v;
	struct rasops_info *ri = &sf->sf_ro;

	if (sf->sf_nscreens > 0)
		return (ENOMEM);

	*cookiep = ri;
	*curyp = 0;
	*curxp = 0;
	if (ISSET(ri->ri_caps, WSSCREEN_WSCOLORS))
		ri->ri_ops.alloc_attr(ri,
		    WSCOL_BLACK, WSCOL_WHITE, WSATTR_WSCOLORS, attrp);
	else
		ri->ri_ops.alloc_attr(ri, 0, 0, 0, attrp);
	sf->sf_nscreens++;
	return (0);
}

void
fb_free_screen(void *v, void *cookie)
{
	struct sunfb *sf = v;

	sf->sf_nscreens--;
}

int
fb_show_screen(void *v, void *cookie, int waitok, void (*cb)(void *, int, int),
    void *cbarg)
{
	return (0);
}

#if defined(SUN4)

/*
 * Support routines for P4 framebuffers.
 */

/*
 * Probe for a P4 framebuffer.  Return values:
 *	PFOUR_NOTPFOUR		framebuffer is not a P4 framebuffer
 *	otherwise returns P4 ID
 */
int
fb_pfour_id(volatile void *va)
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
 * Screen burner routine for P4
 */
void
fb_pfour_burner(void *v, u_int enable, u_int flags)
{
	struct sunfb *sf = (struct sunfb *)v;
	volatile u_int32_t pfour;

	pfour = *sf->sf_pfour & ~(PFOUR_REG_INTCLR | PFOUR_REG_VIDEO);
	*sf->sf_pfour = pfour | (enable ? PFOUR_REG_VIDEO : 0);
}

#endif /* SUN4 */

#if defined(SUN4C) || defined(SUN4M)
int
fb_get_console_metrics(int *fontwidth, int *fontheight, int *wtop, int *wleft)
{
	int *romwidth, *romheight, *windowtop, *windowleft;
	char buf[200];

	/*
	 * Get the PROM font metrics.
	 */

	/*
	 * char-height and char-width are global in older proms (rom vector
	 * < 2) and in some newer proms.  They are local in version 2.9.  The
	 * correct cutoff point is unknown, as yet; we use 2.9 here.
	 */
	if (promvec->pv_romvec_vers < 2 || promvec->pv_printrev < 0x00020009) {
		if (snprintf(buf, sizeof buf,
		    "' char-height >body >user %lx ! "
		    "' char-width >body >user %lx !",
		    (vaddr_t)&romheight, (vaddr_t)&romwidth) >= sizeof buf)
			return (1);
	} else {
		if (snprintf(buf, sizeof buf, "stdout @ is my-self "
		    "addr char-height %lx ! addr char-width %lx !",
		    (vaddr_t)&romheight, (vaddr_t)&romwidth) >= sizeof buf)
			return (1);
	}

	romheight = romwidth = NULL;
	rominterpret(buf);

	if (romheight == NULL || romwidth == NULL ||
	    *romheight == 0 || *romwidth == 0)
		return (1);

	*fontwidth = *romwidth;
	*fontheight = *romheight;

	/*
	 * Get the PROM console window position, if possible.
	 */

	*wtop = *wleft = 0;	/* if no values available... */
	if (promvec->pv_romvec_vers < 2 || promvec->pv_printrev < 0x00020009)
		return (0);	/* not available */

	if (snprintf(buf, sizeof buf, "stdout @ is my-self "
	    "addr window-top %lx ! addr window-left %lx !",
	    (vaddr_t)&windowtop, (vaddr_t)&windowleft) >= sizeof buf)
		return (0);	/* XXX shouldn't happen */

	windowtop = windowleft = NULL;
	rominterpret(buf);

	if (windowtop != NULL)
		*wtop = *windowtop;
	if (windowleft != NULL)
		*wleft = *windowleft;

	return (0);
}
#endif	/* SUN4C || SUN4M */

#endif	/* NWSDISPLAY */
