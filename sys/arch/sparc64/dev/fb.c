/*	$OpenBSD: fb.c,v 1.6 2004/11/29 22:07:40 miod Exp $	*/
/*	$NetBSD: fb.c,v 1.23 1997/07/07 23:30:22 pk Exp $ */

/*
 * Copyright (c) 2002, 2004  Miodrag Vallat.
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

#include <dev/wscons/wsdisplayvar.h>
#include <dev/wscons/wscons_raster.h>
#include <dev/rasops/rasops.h>
#include <machine/fbvar.h>

#include "wsdisplay.h"

static int a2int(char *, int);
static void fb_initwsd(struct sunfb *);

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

void
fb_setsize(struct sunfb *sf, int def_depth, int def_width, int def_height,
    int node, int unused)
{
	int def_linebytes;

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
	if (sf->sf_linebytes < (sf->sf_width * sf->sf_depth) / 8) {
		sf->sf_linebytes = def_linebytes;
	}

	sf->sf_fbsize = sf->sf_height * sf->sf_linebytes;
}

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

/* setup the embedded wsscreen_descr structure from rasops settings */
static void
fb_initwsd(struct sunfb *sf)
{
	strlcpy(sf->sf_wsd.name, "std", sizeof(sf->sf_wsd.name));
	sf->sf_wsd.capabilities = sf->sf_ro.ri_caps;
	sf->sf_wsd.nrows = sf->sf_ro.ri_rows;
	sf->sf_wsd.ncols = sf->sf_ro.ri_cols;
	sf->sf_wsd.textops = &sf->sf_ro.ri_ops;
}

void
fbwscons_init(struct sunfb *sf, int flags)
{
	int cols, rows;

	/* ri_hw and ri_bits must have already been setup by caller */
	sf->sf_ro.ri_flg = RI_CENTER | RI_FULLCLEAR | flags;
	sf->sf_ro.ri_depth = sf->sf_depth;
	sf->sf_ro.ri_stride = sf->sf_linebytes;
	sf->sf_ro.ri_width = sf->sf_width;
	sf->sf_ro.ri_height = sf->sf_height;

	rows = a2int(getpropstring(optionsnode, "screen-#rows"), 34);
	cols = a2int(getpropstring(optionsnode, "screen-#columns"), 80);

	rasops_init(&sf->sf_ro, rows, cols);
}

void
fbwscons_console_init(struct sunfb *sf, int row,
    void (*burner)(void *, u_int, u_int))
{
	long defattr;

	if (romgetcursoraddr(&sf->sf_crowp, &sf->sf_ccolp))
		sf->sf_ccolp = sf->sf_crowp = NULL;
	if (sf->sf_ccolp != NULL)
		sf->sf_ro.ri_ccol = *sf->sf_ccolp;

	if (row < 0) {
		if (sf->sf_crowp != NULL)
			sf->sf_ro.ri_crow = *sf->sf_crowp;
		else
			/* assume last row */
			sf->sf_ro.ri_crow = sf->sf_ro.ri_rows - 1;
	} else {
		/*
		 * If we force the display row, this is because the screen
		 * has been cleared or the font has been changed.
		 * In this case, choose not to keep pointers to the PROM
		 * cursor position, as the values are likely to be inaccurate
		 * upon shutdown...
		 */
		sf->sf_crowp = sf->sf_ccolp = NULL;
		sf->sf_ro.ri_crow = row;
	}

	/*
	 * Scale back rows and columns if the font would not otherwise
	 * fit on this display. Without this we would panic later.
	 */
	if (sf->sf_ro.ri_crow >= sf->sf_ro.ri_rows)
		sf->sf_ro.ri_crow = sf->sf_ro.ri_rows - 1;
	if (sf->sf_ro.ri_ccol >= sf->sf_ro.ri_cols)
		sf->sf_ro.ri_ccol = sf->sf_ro.ri_cols - 1;

	/*
	 * Select appropriate color settings to mimic a
	 * black on white Sun console.
	 */
	if (sf->sf_depth > 8) {
		wscol_white = 0;
		wscol_black = 255;
		wskernel_bg = 0;
		wskernel_fg = 255;
	}

	if (ISSET(sf->sf_ro.ri_caps, WSSCREEN_WSCOLORS) &&
	    sf->sf_depth == 8) {
		sf->sf_ro.ri_ops.alloc_attr(&sf->sf_ro,
		    WSCOL_BLACK, WSCOL_WHITE, WSATTR_WSCOLORS, &defattr);
	} else {
		sf->sf_ro.ri_ops.alloc_attr(&sf->sf_ro, 0, 0, 0, &defattr);
	}

	fb_initwsd(sf);
	wsdisplay_cnattach(&sf->sf_wsd, &sf->sf_ro,
	    sf->sf_ro.ri_ccol, sf->sf_ro.ri_crow, defattr);

	/* remember screen burner routine */
	fb_burner = burner;
	fb_cookie = sf;
}

void
fbwscons_setcolormap(struct sunfb *sf,
    void (*setcolor)(void *, u_int, u_int8_t, u_int8_t, u_int8_t))
{
	int i;
	u_char *color;

	if (sf->sf_depth <= 8 && setcolor != NULL) {
		for (i = 0; i < 16; i++) {
			color = (u_char *)&rasops_cmap[i * 3];
			setcolor(sf, i, color[0], color[1], color[2]);
		}
		for (i = 240; i < 256; i++) {
			color = (u_char *)&rasops_cmap[i * 3];
			setcolor(sf, i, color[0], color[1], color[2]);
		}
		/* compensate for BoW palette */
		setcolor(sf, WSCOL_BLACK, 0, 0, 0);
		setcolor(sf, 0xff ^ WSCOL_BLACK, 255, 255, 255);
		setcolor(sf, WSCOL_WHITE, 255, 255, 255);
		setcolor(sf, 0xff ^ WSCOL_WHITE, 0, 0, 0);
	}
}

void
fbwscons_attach(struct sunfb *sf, struct wsdisplay_accessops *op, int isconsole)
{
	struct wsemuldisplaydev_attach_args waa;
	struct wsscreen_descr *scrlist[1];
	struct wsscreen_list screenlist;

	if (isconsole == 0) {
		/* done in wsdisplay_cnattach() earlier if console */
		fb_initwsd(sf);
	}

	scrlist[0] = &sf->sf_wsd;
	screenlist.nscreens = 1;
	screenlist.screens = (const struct wsscreen_descr **)scrlist;

	waa.console = isconsole;
	waa.scrdata = &screenlist;
	waa.accessops = op;
	waa.accesscookie = sf;
	config_found(&sf->sf_dev, &waa, wsemuldisplaydevprint);
}

#endif	/* NWSDISPLAY */
