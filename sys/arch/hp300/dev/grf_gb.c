/*	$NetBSD: grf_gb.c,v 1.7 1996/03/03 16:48:58 thorpej Exp $	*/

/*
 * Copyright (c) 1996 Jason R. Thorpe.  All rights reserved.
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 * from: Utah $Hdr: grf_gb.c 1.18 93/08/13$
 *
 *	@(#)grf_gb.c	8.4 (Berkeley) 1/12/94
 */

#include "grf.h"
#if NGRF > 0

/*
 * Graphics routines for the Gatorbox.
 *
 * Note: In the context of this system, "gator" and "gatorbox" both refer to
 *       HP 987x0 graphics systems.  "Gator" is not used for high res mono.
 *       (as in 9837 Gator systems)
 */
#include <sys/param.h>
#include <sys/conf.h>
#include <sys/errno.h>
#include <sys/proc.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/systm.h>

#include <machine/autoconf.h>
#include <machine/cpu.h>
 
#include <dev/cons.h>

#include <hp300/dev/grfioctl.h>
#include <hp300/dev/grfvar.h>
#include <hp300/dev/grfreg.h>
#include <hp300/dev/grf_gbreg.h>

#include <hp300/dev/itevar.h>
#include <hp300/dev/itereg.h>
 
#include "ite.h"

#define CRTC_DATA_LENGTH  0x0e
u_char crtc_init_data[CRTC_DATA_LENGTH] = {
    0x29, 0x20, 0x23, 0x04, 0x30, 0x0b, 0x30,
    0x30, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x00
};

int	gb_init __P((struct grf_data *gp, int, caddr_t));
int	gb_mode __P((struct grf_data *gp, int, caddr_t));
void	gb_microcode __P((struct gboxfb *));

/* Gatorbox grf switch */
struct grfsw gbox_grfsw = {
	GID_GATORBOX, GRFGATOR, "gatorbox", gb_init, gb_mode
};

#if NITE > 0
void	gbox_init __P((struct ite_data *));
void	gbox_deinit __P((struct ite_data *));
void	gbox_putc __P((struct ite_data *, int, int, int, int));
void	gbox_cursor __P((struct ite_data *, int));
void	gbox_clear __P((struct ite_data *, int, int, int, int));
void	gbox_scroll __P((struct ite_data *, int, int, int, int));
void	gbox_windowmove __P((struct ite_data *, int, int, int, int,
		int, int, int));

/* Gatorbox ite switch */
struct itesw gbox_itesw = {
	gbox_init, gbox_deinit, gbox_clear, gbox_putc,
	gbox_cursor, gbox_scroll, ite_readbyte, ite_writeglyph
};
#endif /* NITE > 0 */

/*
 * Initialize hardware.
 * Must point g_display at a grfinfo structure describing the hardware.
 * Returns 0 if hardware not present, non-zero ow.
 */
int
gb_init(gp, scode, addr)
	struct grf_data *gp;
	int scode;
	caddr_t addr;
{
	register struct gboxfb *gbp;
	struct grfinfo *gi = &gp->g_display;
	u_char *fbp, save;
	int fboff;
	extern caddr_t sctopa(), iomap();

	/*
	 * If the console has been initialized, and it was us, there's
	 * no need to repeat this.
	 */
	if (consinit_active || (scode != conscode)) {
		gbp = (struct gboxfb *) addr;
		if (ISIIOVA(addr))
			gi->gd_regaddr = (caddr_t) IIOP(addr);
		else
			gi->gd_regaddr = sctopa(scode);
		gi->gd_regsize = 0x10000;
		gi->gd_fbwidth = 1024;		/* XXX */
		gi->gd_fbheight = 1024;		/* XXX */
		gi->gd_fbsize = gi->gd_fbwidth * gi->gd_fbheight;
		fboff = (gbp->fbomsb << 8) | gbp->fbolsb;
		gi->gd_fbaddr = (caddr_t) (*((u_char *)addr + fboff) << 16);
		gp->g_regkva = addr;
		gp->g_fbkva = iomap(gi->gd_fbaddr, gi->gd_fbsize);
		gi->gd_dwidth = 1024;		/* XXX */
		gi->gd_dheight = 768;		/* XXX */
		gi->gd_planes = 0;		/* how do we do this? */
		/*
		 * The minimal register info here is from the Gatorbox X driver.
		 */
		fbp = (u_char *) gp->g_fbkva;
		gbp->write_protect = 0;
		gbp->interrupt = 4;		/** fb_enable ? **/
		gbp->rep_rule = 3;		/* GXcopy */
		gbp->blink1 = 0xff;
		gbp->blink2 = 0xff;

		gb_microcode(gbp);

		/*
		 * Find out how many colors are available by determining
		 * which planes are installed.  That is, write all ones to
		 * a frame buffer location, see how many ones are read back.
		 */
		save = *fbp;
		*fbp = 0xFF;
		gi->gd_colors = *fbp + 1;
		*fbp = save;
	}
	return(1);
}

/*
 * Program the 6845.
 */
void
gb_microcode(gbp)
	struct gboxfb *gbp;
{
	register int i;
	
	for (i = 0; i < CRTC_DATA_LENGTH; i++) {
		gbp->crtc_address = i;
		gbp->crtc_data = crtc_init_data[i];
	}
}

/*
 * Change the mode of the display.
 * Right now all we can do is grfon/grfoff.
 * Return a UNIX error number or 0 for success.
 */
int
gb_mode(gp, cmd, data)
	register struct grf_data *gp;
	int cmd;
	caddr_t data;
{
	struct gboxfb *gbp;
	int error = 0;

	gbp = (struct gboxfb *)gp->g_regkva;
	switch (cmd) {
	case GM_GRFON:
		gbp->sec_interrupt = 1;
		break;

	case GM_GRFOFF:
		break;

	/*
	 * Remember UVA of mapping for GCDESCRIBE.
	 * XXX this should be per-process.
	 */
	case GM_MAP:
		gp->g_data = data;
		break;

	case GM_UNMAP:
		gp->g_data = 0;
		break;

#ifdef COMPAT_HPUX
	case GM_DESCRIBE:
	{
		struct grf_fbinfo *fi = (struct grf_fbinfo *)data;
		struct grfinfo *gi = &gp->g_display;
		int i;

		/* feed it what HP-UX expects */
		fi->id = gi->gd_id;
		fi->mapsize = gi->gd_fbsize;
		fi->dwidth = gi->gd_dwidth;
		fi->dlength = gi->gd_dheight;
		fi->width = gi->gd_fbwidth;
		fi->length = gi->gd_fbheight;
		fi->bpp = NBBY;
		fi->xlen = (fi->width * fi->bpp) / NBBY;
		fi->npl = gi->gd_planes;
		fi->bppu = fi->npl;
		fi->nplbytes = fi->xlen * ((fi->length * fi->bpp) / NBBY);
		bcopy("HP98700", fi->name, 8);
		fi->attr = 2;	/* HW block mover */
		/*
		 * If mapped, return the UVA where mapped.
		 */
		if (gp->g_data) {
			fi->regbase = gp->g_data;
			fi->fbbase = fi->regbase + gp->g_display.gd_regsize;
		} else {
			fi->fbbase = 0;
			fi->regbase = 0;
		}
		for (i = 0; i < 6; i++)
			fi->regions[i] = 0;
		break;
	}
#endif

	default:
		error = EINVAL;
		break;
	}
	return(error);
}

#if NITE > 0

/*
 * Gatorbox ite routines
 */

#define REGBASE     	((struct gboxfb *)(ip->regbase))
#define WINDOWMOVER 	gbox_windowmove

void
gbox_init(ip)
	register struct ite_data *ip;
{
	/* XXX */
	if (ip->regbase == 0) {
		struct grf_data *gp = ip->grf;

		ip->regbase = gp->g_regkva;
		ip->fbbase = gp->g_fbkva;
		ip->fbwidth = gp->g_display.gd_fbwidth;
		ip->fbheight = gp->g_display.gd_fbheight;
		ip->dwidth = gp->g_display.gd_dwidth;
		ip->dheight = gp->g_display.gd_dheight;
	}

	REGBASE->write_protect = 0x0;
	REGBASE->interrupt = 0x4;
	REGBASE->rep_rule = RR_COPY;
	REGBASE->blink1 = 0xff;
	REGBASE->blink2 = 0xff;
	gb_microcode((struct gboxfb *)ip->regbase);
	REGBASE->sec_interrupt = 0x01;

	/*
	 * Set up the color map entries. We use three entries in the
	 * color map. The first, is for black, the second is for
	 * white, and the very last entry is for the inverted cursor.
	 */
	REGBASE->creg_select = 0x00;
	REGBASE->cmap_red    = 0x00;
	REGBASE->cmap_grn    = 0x00;
	REGBASE->cmap_blu    = 0x00;
	REGBASE->cmap_write  = 0x00;
	gbcm_waitbusy(ip->regbase);
	
	REGBASE->creg_select = 0x01;
	REGBASE->cmap_red    = 0xFF;
	REGBASE->cmap_grn    = 0xFF;
	REGBASE->cmap_blu    = 0xFF;
	REGBASE->cmap_write  = 0x01;
	gbcm_waitbusy(ip->regbase);

	REGBASE->creg_select = 0xFF;
	REGBASE->cmap_red    = 0xFF;
	REGBASE->cmap_grn    = 0xFF;
	REGBASE->cmap_blu    = 0xFF;
	REGBASE->cmap_write  = 0x01;
	gbcm_waitbusy(ip->regbase);

	ite_fontinfo(ip);
	ite_fontinit(ip);

	/*
	 * Clear the display. This used to be before the font unpacking
	 * but it crashes. Figure it out later.
	 */
	gbox_windowmove(ip, 0, 0, 0, 0, ip->dheight, ip->dwidth, RR_CLEAR);
	tile_mover_waitbusy(ip->regbase);

	/*
	 * Stash the inverted cursor.
	 */
	gbox_windowmove(ip, charY(ip, ' '), charX(ip, ' '),
			ip->cblanky, ip->cblankx, ip->ftheight,
			ip->ftwidth, RR_COPYINVERTED);
}

void
gbox_deinit(ip)
	struct ite_data *ip;
{
	gbox_windowmove(ip, 0, 0, 0, 0, ip->dheight, ip->dwidth, RR_CLEAR);
	tile_mover_waitbusy(ip->regbase);

   	ip->flags &= ~ITE_INITED;
}

void
gbox_putc(ip, c, dy, dx, mode)
	struct ite_data *ip;
        int dy, dx;
	int c, mode;
{
        register int wrr = ((mode == ATTR_INV) ? RR_COPYINVERTED : RR_COPY);

	gbox_windowmove(ip, charY(ip, c), charX(ip, c),
			    dy * ip->ftheight, dx * ip->ftwidth,
			    ip->ftheight, ip->ftwidth, wrr);
}

void
gbox_cursor(ip, flag)
	struct ite_data *ip;
        int flag;
{
	if (flag == DRAW_CURSOR)
		draw_cursor(ip)
	else if (flag == MOVE_CURSOR) {
		erase_cursor(ip)
		draw_cursor(ip)
	}
	else
		erase_cursor(ip)
}

void
gbox_clear(ip, sy, sx, h, w)
	struct ite_data *ip;
	int sy, sx, h, w;
{
	gbox_windowmove(ip, sy * ip->ftheight, sx * ip->ftwidth,
			sy * ip->ftheight, sx * ip->ftwidth, 
			h  * ip->ftheight, w  * ip->ftwidth,
			RR_CLEAR);
}
#define	gbox_blockmove(ip, sy, sx, dy, dx, h, w) \
	gbox_windowmove((ip), \
			(sy) * ip->ftheight, \
			(sx) * ip->ftwidth, \
			(dy) * ip->ftheight, \
			(dx) * ip->ftwidth, \
			(h)  * ip->ftheight, \
			(w)  * ip->ftwidth, \
			RR_COPY)

void
gbox_scroll(ip, sy, sx, count, dir)
        struct ite_data *ip;
        int sy, dir, sx, count;
{
	register int height, dy, i;
	
	tile_mover_waitbusy(ip->regbase);
	REGBASE->write_protect = 0x0;
	
	if (dir == SCROLL_UP) {
		dy = sy - count;
		height = ip->rows - sy;
		for (i = 0; i < height; i++)
			gbox_blockmove(ip, sy + i, sx, dy + i, 0, 1, ip->cols);
	}
	else if (dir == SCROLL_DOWN) {
		dy = sy + count;
		height = ip->rows - dy;
		for (i = (height - 1); i >= 0; i--)
			gbox_blockmove(ip, sy + i, sx, dy + i, 0, 1, ip->cols);
	}
	else if (dir == SCROLL_RIGHT) {
		gbox_blockmove(ip, sy, sx, sy, sx + count,
			       1, ip->cols - (sx + count));
	}
	else {
		gbox_blockmove(ip, sy, sx, sy, sx - count,
			       1, ip->cols - sx);
	}		
}

void
gbox_windowmove(ip, sy, sx, dy, dx, h, w, mask)
     struct ite_data *ip;
     int sy, sx, dy, dx, mask, h, w;
{
	register int src, dest;

	src  = (sy * 1024) + sx;	/* upper left corner in pixels */
	dest = (dy * 1024) + dx;

	tile_mover_waitbusy(ip->regbase);
	REGBASE->width = -(w / 4);
	REGBASE->height = -(h / 4);
	if (src < dest)
		REGBASE->rep_rule = MOVE_DOWN_RIGHT|mask;
	else {
		REGBASE->rep_rule = MOVE_UP_LEFT|mask;
		/*
		 * Adjust to top of lower right tile of the block.
		 */
		src = src + ((h - 4) * 1024) + (w - 4);
		dest= dest + ((h - 4) * 1024) + (w - 4);
	}
	FBBASE[dest] = FBBASE[src];
}

/*
 * Gatorbox console support
 */

int
gbox_console_scan(scode, va, arg)
	int scode;
	caddr_t va;
	void *arg;
{
	struct grfreg *grf = (struct grfreg *)va;
	struct consdev *cp = arg;
	u_char *dioiidev;
	int force = 0, pri;

	if ((grf->gr_id == GRFHWID) && (grf->gr_id2 == GID_GATORBOX)) {
		pri = CN_NORMAL;

#ifdef CONSCODE
		/*
		 * Raise our priority, if appropriate.
		 */
		if (scode == CONSCODE) {
			pri = CN_REMOTE;
			force = conforced = 1;
		}
#endif

		/* Only raise priority. */
		if (pri > cp->cn_pri)
			cp->cn_pri = pri;

		/*
		 * If our priority is higher than the currently-remembered
		 * console, stash our priority.
		 */
		if (((cn_tab == NULL) || (cp->cn_pri > cn_tab->cn_pri))
		    || force) {
			cn_tab = cp;
			if (scode >= 132) {
				dioiidev = (u_char *)va;
				return ((dioiidev[0x101] + 1) * 0x100000);
			}
			return (DIOCSIZE);
		}
	}
	return (0);
}

void
gboxcnprobe(cp)
	struct consdev *cp;
{
	int maj;
	caddr_t va;
	struct grfreg *grf;
	int force = 0;

	maj = ite_major();

	/* initialize required fields */
	cp->cn_dev = makedev(maj, 0);		/* XXX */
	cp->cn_pri = CN_DEAD;

	/* Abort early if console already forced. */
	if (conforced)
		return;

	/* Look for "internal" framebuffer. */
	va = (caddr_t)IIOV(GRFIADDR);
	grf = (struct grfreg *)va;
	if (!badaddr(va) &&
	    ((grf->gr_id == GRFHWID) && (grf->gr_id2 == GID_GATORBOX))) {
		cp->cn_pri = CN_INTERNAL;

#ifdef CONSCODE
		/*
		 * Raise our priority and save some work, if appropriate.
		 */
		if (CONSCODE == -1) {
			cp->cn_pri = CN_REMOTE;
			force = conforced = 1;
		}
#endif

		/*
		 * If our priority is higher than the currently
		 * remembered console, stash our priority, and
		 * unmap whichever device might be currently mapped.
		 * Since we're internal, we set the saved size to 0
		 * so they don't attempt to unmap our fixed VA later.
		 */
		if (((cn_tab == NULL) || (cp->cn_pri > cn_tab->cn_pri))
		    || force) {
			cn_tab = cp;
			if (convasize)
				iounmap(conaddr, convasize);
			conscode = -1;
			conaddr = va;
			convasize = 0;
		}
	}

	console_scan(gbox_console_scan, cp);
}

void
gboxcninit(cp)
	struct consdev *cp;
{
	struct ite_data *ip = &ite_cn;
	struct grf_data *gp = &grf_cn;

	/*
	 * Initialize the framebuffer hardware.
	 */
	(void)gb_init(gp, conscode, conaddr);

	/*
	 * Set up required grf data.
	 */
	gp->g_sw = &gbox_grfsw;
	gp->g_display.gd_id = gp->g_sw->gd_swid;
	gp->g_flags = GF_ALIVE;

	/*
	 * Set up required ite data and initialize ite.
	 */
	ip->isw = &gbox_itesw;
	ip->grf = gp;
	ip->flags = ITE_ALIVE|ITE_CONSOLE|ITE_ACTIVE|ITE_ISCONS;
	ip->attrbuf = console_attributes;
	iteinit(ip);

	kbd_ite = ip;		/* XXX */
}

#endif /* NITE > 0 */
#endif /* NGRF > 0 */
