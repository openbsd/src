/*	$OpenBSD: grf_rb.c,v 1.11 2005/01/08 22:13:53 miod Exp $	*/
/*	$NetBSD: grf_rb.c,v 1.11 1997/03/31 07:34:17 scottr Exp $	*/

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
 * from: Utah $Hdr: grf_rb.c 1.15 93/08/13$
 *
 *	@(#)grf_rb.c	8.4 (Berkeley) 1/12/94
 */

/*
 * Graphics routines for the Renaissance, HP98720 Graphics system.
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/proc.h>
#include <sys/tty.h>

#include <machine/autoconf.h>
#include <machine/cpu.h>
 
#include <dev/cons.h>

#include <hp300/dev/dioreg.h>
#include <hp300/dev/diovar.h>
#include <hp300/dev/diodevs.h>
#include <hp300/dev/intiovar.h>

#include <hp300/dev/grfioctl.h>
#include <hp300/dev/grfvar.h>
#include <hp300/dev/grfreg.h>
#include <hp300/dev/grf_rbreg.h>

#include <hp300/dev/itevar.h>
#include <hp300/dev/itereg.h>

#include "ite.h"

int	rb_init(struct grf_data *gp, int, caddr_t);
int	rb_mode(struct grf_data *gp, int, caddr_t);

int	rbox_intio_match(struct device *, void *, void *);
void	rbox_intio_attach(struct device *, struct device *, void *);

int	rbox_dio_match(struct device *, void *, void *);
void	rbox_dio_attach(struct device *, struct device *, void *);

int	rbox_console_scan(int, caddr_t, void *);
void	rboxcnprobe(struct consdev *cp);
void	rboxcninit(struct consdev *cp);

struct cfattach rbox_intio_ca = {
	sizeof(struct grfdev_softc), rbox_intio_match, rbox_intio_attach
};

struct cfattach rbox_dio_ca = {
	sizeof(struct grfdev_softc), rbox_dio_match, rbox_dio_attach
};

struct cfdriver rbox_cd = {
	NULL, "rbox", DV_DULL
};

/* Renaissance grf switch */
struct grfsw rbox_grfsw = {
	GID_RENAISSANCE, GRFRBOX, "renaissance", rb_init, rb_mode
};

#if NITE > 0
void	rbox_init(struct ite_data *);
void	rbox_deinit(struct ite_data *);
void	rbox_putc(struct ite_data *, int, int, int, int);
void	rbox_cursor(struct ite_data *, int);
void	rbox_clear(struct ite_data *, int, int, int, int);
void	rbox_scroll(struct ite_data *, int, int, int, int);
void	rbox_windowmove(struct ite_data *, int, int, int, int,
		int, int, int);

/* Renaissance ite switch */
struct itesw rbox_itesw = {
	rbox_init, rbox_deinit, rbox_clear, rbox_putc,
	rbox_cursor, rbox_scroll, ite_readbyte, ite_writeglyph
};
#endif /* NITE > 0 */

int
rbox_intio_match(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct intio_attach_args *ia = aux;
	struct grfreg *grf;

	grf = (struct grfreg *)IIOV(GRFIADDR);
	if (badaddr((caddr_t)grf))
		return (0);

	if (grf->gr_id == DIO_DEVICE_ID_FRAMEBUFFER &&
	    grf->gr_id2 == DIO_DEVICE_SECID_RENAISSANCE) {
		ia->ia_addr = (caddr_t)GRFIADDR;
		return (1);
	}

	return (0);
}

void
rbox_intio_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct grfdev_softc *sc = (struct grfdev_softc *)self;
	caddr_t grf;

	grf = (caddr_t)IIOV(GRFIADDR);
	sc->sc_scode = -1;	/* XXX internal i/o */

#if NITE > 0
	grfdev_attach(sc, rb_init, grf, &rbox_grfsw, &rbox_itesw);
#else
	grfdev_attach(sc, rb_init, grf, &rbox_grfsw, NULL);
#endif	/* NITE > 0 */
}

int
rbox_dio_match(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct dio_attach_args *da = aux;

	if (da->da_id == DIO_DEVICE_ID_FRAMEBUFFER &&
	    da->da_secid == DIO_DEVICE_SECID_RENAISSANCE)
		return (1);

	return (0);
}

void
rbox_dio_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct grfdev_softc *sc = (struct grfdev_softc *)self;
	struct dio_attach_args *da = aux;
	caddr_t grf;

	sc->sc_scode = da->da_scode;
	if (sc->sc_scode == conscode)
		grf = conaddr;
	else {
		grf = iomap(dio_scodetopa(sc->sc_scode), da->da_size);
		if (grf == 0) {
			printf("%s: can't map framebuffer\n",
			    sc->sc_dev.dv_xname);
			return;
		}
	}

#if NITE > 0
	grfdev_attach(sc, rb_init, grf, &rbox_grfsw, &rbox_itesw);
#else
	grfdev_attach(sc, rb_init, grf, &rbox_grfsw, NULL);
#endif	/* NITE > 0 */
}

/*
 * Initialize hardware.
 * Must point g_display at a grfinfo structure describing the hardware.
 * Returns 0 if hardware not present, non-zero ow.
 */
int
rb_init(gp, scode, addr)
	struct grf_data *gp;
	int scode;
	caddr_t addr;
{
	struct rboxfb *rbp;
	struct grfinfo *gi = &gp->g_display;
	int fboff;

	/*
	 * If the console has been initialized, and it was us, there's
	 * no need to repeat this.
	 */
	if (consinit_active || (scode != conscode)) {
		rbp = (struct rboxfb *) addr;
		if (ISIIOVA(addr))
			gi->gd_regaddr = (caddr_t) IIOP(addr);
		else
			gi->gd_regaddr = dio_scodetopa(scode);
		gi->gd_regsize = 0x20000;
		gi->gd_fbwidth = (rbp->fbwmsb << 8) | rbp->fbwlsb;
		gi->gd_fbheight = (rbp->fbhmsb << 8) | rbp->fbhlsb;
		gi->gd_fbsize = gi->gd_fbwidth * gi->gd_fbheight;
		fboff = (rbp->fbomsb << 8) | rbp->fbolsb;
		gi->gd_fbaddr = (caddr_t) (*((u_char *)addr + fboff) << 16);
		if (gi->gd_regaddr >= (caddr_t)DIOII_BASE) {
			/*
			 * For DIO II space the fbaddr just computed is
			 * the offset from the select code base (regaddr)
			 * of the framebuffer.  Hence it is also implicitly
			 * the size of the set.
			 */
			gi->gd_regsize = (int) gi->gd_fbaddr;
			gi->gd_fbaddr += (int) gi->gd_regaddr;
			gp->g_regkva = addr;
			gp->g_fbkva = addr + gi->gd_regsize;
		} else {
			/*
			 * For DIO space we need to map the separate
			 * framebuffer.
			 */
			gp->g_regkva = addr;
			gp->g_fbkva = iomap(gi->gd_fbaddr, gi->gd_fbsize);
		}
		gi->gd_dwidth = (rbp->dwmsb << 8) | rbp->dwlsb;
		gi->gd_dheight = (rbp->dwmsb << 8) | rbp->dwlsb;
		gi->gd_planes = 0;	/* ?? */
		gi->gd_colors = 256;
	}
	return(1);
}

/*
 * Change the mode of the display.
 * Right now all we can do is grfon/grfoff.
 * Return a UNIX error number or 0 for success.
 */
int
rb_mode(gp, cmd, data)
	struct grf_data *gp;
	int cmd;
	caddr_t data;
{
	struct rboxfb *rbp;
	int error = 0;

	rbp = (struct rboxfb *) gp->g_regkva;
	switch (cmd) {
	/*
	 * The minimal info here is from the Renaissance X driver.
	 */
	case GM_GRFON:
	case GM_GRFOFF:
		break;

	case GM_GRFOVON:
		rbp->write_enable = 0;
		rbp->opwen = 0xF;
		rbp->drive = 0x10;
		break;

	case GM_GRFOVOFF:
		rbp->opwen = 0;
		rbp->write_enable = 0xffffffff;
		rbp->drive = 0x01;
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
		bcopy("HP98720", fi->name, 8);
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
 * Renaissance ite routines
 */

#define REGBASE		((struct rboxfb *)(ip->regbase))
#define WINDOWMOVER	rbox_windowmove

void
rbox_init(ip)
	struct ite_data *ip;
{
	int i;

	/* XXX */
	if (ip->regbase == 0) {
		struct grf_data *gp = ip->grf;

		ip->regbase = gp->g_regkva;
		ip->fbbase = gp->g_fbkva;
		ip->fbwidth = gp->g_display.gd_fbwidth;
		ip->fbheight = gp->g_display.gd_fbheight;
		ip->dwidth = gp->g_display.gd_dwidth;
		ip->dheight = gp->g_display.gd_dheight;
		/*
		 * XXX some displays (e.g. the davinci) appear
		 * to return a display height greater than the
		 * returned FB height.  Guess we should go back
		 * to getting the display dimensions from the
		 * fontrom...
		 */
		if (ip->dwidth > ip->fbwidth)
			ip->dwidth = ip->fbwidth;
		if (ip->dheight > ip->fbheight)
			ip->dheight = ip->fbheight;
	}

	rb_waitbusy(ip->regbase);

	REGBASE->reset = 0x39;
	DELAY(1000);

	REGBASE->interrupt = 0x04;
	REGBASE->display_enable = 0x01;
	REGBASE->video_enable = 0x01;
	REGBASE->drive = 0x01;
	REGBASE->vdrive = 0x0;

	ite_fontinfo(ip);
	
	REGBASE->opwen = 0xFF;

	/*
	 * Clear the framebuffer.
	 */
	rbox_windowmove(ip, 0, 0, 0, 0, ip->fbheight, ip->fbwidth, RR_CLEAR);
	rb_waitbusy(ip->regbase);

	for(i = 0; i < 16; i++) {
		*(ip->regbase + 0x63c3 + i*4) = 0x0;
		*(ip->regbase + 0x6403 + i*4) = 0x0;
		*(ip->regbase + 0x6803 + i*4) = 0x0;
		*(ip->regbase + 0x6c03 + i*4) = 0x0;
		*(ip->regbase + 0x73c3 + i*4) = 0x0;
		*(ip->regbase + 0x7403 + i*4) = 0x0;
		*(ip->regbase + 0x7803 + i*4) = 0x0;
		*(ip->regbase + 0x7c03 + i*4) = 0x0;
	}

	REGBASE->rep_rule = 0x33;
	
	/*
	 * I cannot figure out how to make the blink planes stop. So, we
	 * must set both colormaps so that when the planes blink, and
	 * the secondary colormap is active, we still get text.
	 */
	CM1RED[0x00].value = 0x00;
	CM1GRN[0x00].value = 0x00;
	CM1BLU[0x00].value = 0x00;
	CM1RED[0x01].value = 0xFF;
	CM1GRN[0x01].value = 0xFF;
	CM1BLU[0x01].value = 0xFF;

	CM2RED[0x00].value = 0x00;
	CM2GRN[0x00].value = 0x00;
	CM2BLU[0x00].value = 0x00;
	CM2RED[0x01].value = 0xFF;
	CM2GRN[0x01].value = 0xFF;
	CM2BLU[0x01].value = 0xFF;

 	REGBASE->blink = 0x00;
	REGBASE->write_enable = 0x01;
	REGBASE->opwen = 0x00;
	
	ite_fontinit(ip);
	
	/*
	 * Stash the inverted cursor.
	 */
	rbox_windowmove(ip, charY(ip, ' '), charX(ip, ' '),
			    ip->cblanky, ip->cblankx, ip->ftheight,
			    ip->ftwidth, RR_COPYINVERTED);
}

void
rbox_deinit(ip)
	struct ite_data *ip;
{
	rbox_windowmove(ip, 0, 0, 0, 0, ip->fbheight, ip->fbwidth, RR_CLEAR);
	rb_waitbusy(ip->regbase);

   	ip->flags &= ~ITE_INITED;
}

void
rbox_putc(ip, c, dy, dx, mode)
	struct ite_data *ip;
        int dy, dx, c, mode;
{
        int wrr = ((mode == ATTR_INV) ? RR_COPYINVERTED : RR_COPY);
	
	rbox_windowmove(ip, charY(ip, c), charX(ip, c),
			dy * ip->ftheight, dx * ip->ftwidth,
			ip->ftheight, ip->ftwidth, wrr);
}

void
rbox_cursor(ip, flag)
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
rbox_clear(ip, sy, sx, h, w)
	struct ite_data *ip;
	int sy, sx, h, w;
{
	rbox_windowmove(ip, sy * ip->ftheight, sx * ip->ftwidth,
			sy * ip->ftheight, sx * ip->ftwidth, 
			h  * ip->ftheight, w  * ip->ftwidth,
			RR_CLEAR);
}

void
rbox_scroll(ip, sy, sx, count, dir)
        struct ite_data *ip;
        int sy, count, dir, sx;
{
	int dy;
	int dx = sx;
	int height = 1;
	int width = ip->cols;

	if (dir == SCROLL_UP) {
		dy = sy - count;
		height = ip->rows - sy;
	}
	else if (dir == SCROLL_DOWN) {
		dy = sy + count;
		height = ip->rows - dy - 1;
	}
	else if (dir == SCROLL_RIGHT) {
		dy = sy;
		dx = sx + count;
		width = ip->cols - dx;
	}
	else {
		dy = sy;
		dx = sx - count;
		width = ip->cols - sx;
	}		

	rbox_windowmove(ip, sy * ip->ftheight, sx * ip->ftwidth,
			dy * ip->ftheight, dx * ip->ftwidth,
			height * ip->ftheight,
			width  * ip->ftwidth, RR_COPY);
}

void
rbox_windowmove(ip, sy, sx, dy, dx, h, w, func)
	struct ite_data *ip;
	int sy, sx, dy, dx, h, w, func;
{
	struct rboxfb *rp = REGBASE;
	if (h == 0 || w == 0)
		return;
	
	rb_waitbusy(ip->regbase);
	rp->rep_rule = func << 4 | func;
	rp->source_y = sy;
	rp->source_x = sx;
	rp->dest_y = dy;
	rp->dest_x = dx;
	rp->wheight = h;
	rp->wwidth  = w;
	rp->wmove = 1;
}

/*
 * Renaissance console support
 */

int
rbox_console_scan(scode, va, arg)
	int scode;
	caddr_t va;
	void *arg;
{
	struct grfreg *grf = (struct grfreg *)va;
	struct consdev *cp = arg;
	u_char *dioiidev;
	int force = 0, pri;

	if ((grf->gr_id == GRFHWID) && (grf->gr_id2 == GID_RENAISSANCE)) {
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
			return (DIO_DEVSIZE);
		}
	}
	return (0);
}

void
rboxcnprobe(cp)
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

	/* Abort early if console is already forced. */
	if (conforced)
		return;

	/* Look for "internal" framebuffer. */
	va = (caddr_t)IIOV(GRFIADDR);
	grf = (struct grfreg *)va;
	if (!badaddr(va) &&
	    ((grf->gr_id == GRFHWID) && (grf->gr_id2 == GID_RENAISSANCE))) {
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

	console_scan(rbox_console_scan, cp);
}

void
rboxcninit(cp)
	struct consdev *cp;
{
	struct grf_data *gp = &grf_cn;

	/*
	 * Initialize the framebuffer hardware.
	 */
	(void)rb_init(gp, conscode, conaddr);

	/*
	 * Set up required grf data.
	 */
	gp->g_sw = &rbox_grfsw;
	gp->g_display.gd_id = gp->g_sw->gd_swid;
	gp->g_flags = GF_ALIVE;

	/*
	 * Initialize the terminal emulator.
	 */
	itecninit(gp, &rbox_itesw);
}

#endif /* NITE > 0 */
