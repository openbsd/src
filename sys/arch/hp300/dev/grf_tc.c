/*	$OpenBSD: grf_tc.c,v 1.10 2002/12/06 02:17:42 deraadt Exp $	*/
/*	$NetBSD: grf_tc.c,v 1.11 1997/03/31 07:34:18 scottr Exp $	*/

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
 * from: Utah $Hdr: grf_tc.c 1.20 93/08/13$
 *
 *	@(#)grf_tc.c	8.4 (Berkeley) 1/12/94
 */

/*
 * Graphics routines for TOPCAT and CATSEYE frame buffers
 */
#include <sys/param.h>
#include <sys/conf.h>
#include <sys/errno.h>
#include <sys/proc.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/systm.h>
#include <sys/device.h>
 
#include <machine/autoconf.h>
#include <machine/cpu.h>
  
#include <dev/cons.h>

#include <hp300/dev/diovar.h>
#include <hp300/dev/diodevs.h>
#include <hp300/dev/intiovar.h>

#include <hp300/dev/grfioctl.h>
#include <hp300/dev/grfvar.h>
#include <hp300/dev/grfreg.h>
#include <hp300/dev/grf_tcreg.h>

#include <hp300/dev/itevar.h>
#include <hp300/dev/itereg.h>

#include "ite.h"

int	tc_init(struct grf_data *, int, caddr_t);
int	tc_mode(struct grf_data *, int, caddr_t);

void	topcat_common_attach(struct grfdev_softc *, caddr_t, u_int8_t);

int	topcat_intio_match(struct device *, void *, void *);
void	topcat_intio_attach(struct device *, struct device *, void *);

int	topcat_dio_match(struct device *, void *, void *);
void	topcat_dio_attach(struct device *, struct device *, void *);

int	topcat_console_scan(int, caddr_t, void *);
void	topcatcnprobe(struct consdev *cp);
void	topcatcninit(struct consdev *cp);

struct cfattach topcat_intio_ca = {
	sizeof(struct grfdev_softc), topcat_intio_match, topcat_intio_attach
};

struct cfattach topcat_dio_ca = {
	sizeof(struct grfdev_softc), topcat_dio_match, topcat_dio_attach
};

struct cfdriver topcat_cd = {
	NULL, "topcat", DV_DULL
};

/* Topcat (bobcat) grf switch */
struct grfsw topcat_grfsw = {
	GID_TOPCAT, GRFBOBCAT, "topcat", tc_init, tc_mode
};

/* Lo-res catseye grf switch */
struct grfsw lrcatseye_grfsw = {
	GID_LRCATSEYE, GRFCATSEYE, "lo-res catseye", tc_init, tc_mode
};

/* Hi-res catseye grf switch */
struct grfsw hrcatseye_grfsw = {
	GID_HRCCATSEYE, GRFCATSEYE, "hi-res catseye", tc_init, tc_mode
};

/* Hi-res monochrome catseye grf switch */
struct grfsw hrmcatseye_grfsw = {
	GID_HRMCATSEYE, GRFCATSEYE, "hi-res catseye", tc_init, tc_mode
};

#if NITE > 0
void	topcat_init(struct ite_data *);
void	topcat_deinit(struct ite_data *);
void	topcat_putc(struct ite_data *, int, int, int, int);
void	topcat_cursor(struct ite_data *, int);
void	topcat_clear(struct ite_data *, int, int, int, int);
void	topcat_scroll(struct ite_data *, int, int, int, int);
void	topcat_windowmove(struct ite_data *, int, int, int, int,
		int, int, int);

/* Topcat/catseye ite switch */
struct itesw topcat_itesw = {
	topcat_init, topcat_deinit, topcat_clear, topcat_putc,
	topcat_cursor, topcat_scroll, ite_readbyte, ite_writeglyph
};
#endif /* NITE > 0 */

int
topcat_intio_match(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct intio_attach_args *ia = aux;
	struct grfreg *grf;

	grf = (struct grfreg *)IIOV(GRFIADDR);

	if (badaddr((caddr_t)grf))
		return (0);

	if (grf->gr_id == DIO_DEVICE_ID_FRAMEBUFFER) {
		switch (grf->gr_id2) {
		case DIO_DEVICE_SECID_TOPCAT:
		case DIO_DEVICE_SECID_LRCATSEYE:
		case DIO_DEVICE_SECID_HRCCATSEYE:
		case DIO_DEVICE_SECID_HRMCATSEYE:
#if 0
		case DIO_DEVICE_SECID_XXXCATSEYE:
#endif
			ia->ia_addr = (caddr_t)GRFIADDR;
			return (1);
		}
	}

	return (0);
}

void
topcat_intio_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct grfdev_softc *sc = (struct grfdev_softc *)self;
	struct grfreg *grf;

	grf = (struct grfreg *)IIOV(GRFIADDR);
	sc->sc_scode = -1;	/* XXX internal i/o */

	topcat_common_attach(sc, (caddr_t)grf, grf->gr_id2);
}

int
topcat_dio_match(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct dio_attach_args *da = aux;

	if (da->da_id == DIO_DEVICE_ID_FRAMEBUFFER) {
		switch (da->da_secid) {
		case DIO_DEVICE_SECID_TOPCAT:
		case DIO_DEVICE_SECID_LRCATSEYE:
		case DIO_DEVICE_SECID_HRCCATSEYE:
		case DIO_DEVICE_SECID_HRMCATSEYE:
#if 0
		case DIO_DEVICE_SECID_XXXCATSEYE:
#endif
			return (1);
		}
	}

	return (0);
}

void
topcat_dio_attach(parent, self, aux)
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

	topcat_common_attach(sc, grf, da->da_secid);
}

void
topcat_common_attach(sc, grf, secid)
	struct grfdev_softc *sc;
	caddr_t grf;
	u_int8_t secid;
{
	struct grfsw *sw;

	switch (secid) {
	case DIO_DEVICE_SECID_TOPCAT:
		sw = &topcat_grfsw;
		break;

	case DIO_DEVICE_SECID_LRCATSEYE:
		sw = &lrcatseye_grfsw;
		break;

	case DIO_DEVICE_SECID_HRCCATSEYE:
		sw = &hrcatseye_grfsw;
		break;

	case DIO_DEVICE_SECID_HRMCATSEYE:
		sw = &hrmcatseye_grfsw;
		break;
#if 0
	case DIO_DEVICE_SECID_XXXCATSEYE:
		sw = XXX?
		break;
#endif
	default:
		printf("%s: unknown device 0x%x\n",
		    sc->sc_dev.dv_xname, secid);
		panic("topcat_common_attach");
	}

#if NITE > 0
	grfdev_attach(sc, tc_init, grf, sw, &topcat_itesw);
#else
	grfdev_attach(sc, tc_init, grf, sw, NULL);
#endif	/* NITE > 0 */
}

/*
 * Initialize hardware.
 * Must fill in the grfinfo structure in g_softc.
 * Returns 0 if hardware not present, non-zero ow.
 */
int
tc_init(gp, scode, addr)
	struct grf_data *gp;
	int scode;
	caddr_t addr;
{
	struct tcboxfb *tp = (struct tcboxfb *) addr;
	struct grfinfo *gi = &gp->g_display;
	volatile u_char *fbp;
	u_char save;
	int fboff;

	/*
	 * If the console has been initialized, and it was us, there's
	 * no need to repeat this.
	 */
	if (consinit_active || (scode != conscode)) {
		if (ISIIOVA(addr))
			gi->gd_regaddr = (caddr_t) IIOP(addr);
		else
			gi->gd_regaddr = dio_scodetopa(scode);
		gi->gd_regsize = 0x10000;
		gi->gd_fbwidth = (tp->fbwmsb << 8) | tp->fbwlsb;
		gi->gd_fbheight = (tp->fbhmsb << 8) | tp->fbhlsb;
		gi->gd_fbsize = gi->gd_fbwidth * gi->gd_fbheight;
		fboff = (tp->fbomsb << 8) | tp->fbolsb;
		gi->gd_fbaddr = (caddr_t) (*((u_char *)addr + fboff) << 16);
		if (gi->gd_regaddr >= (caddr_t)DIOIIBASE) {
			/*
			 * For DIO II space the fbaddr just computed is the
			 * offset from the select code base (regaddr) of the
			 * framebuffer.  Hence it is also implicitly the
			 * size of the set.
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
		gi->gd_dwidth = (tp->dwmsb << 8) | tp->dwlsb;
		gi->gd_dheight = (tp->dhmsb << 8) | tp->dhlsb;
		gi->gd_planes = tp->num_planes;
		gi->gd_colors = 1 << gi->gd_planes;
		if (gi->gd_colors == 1) {
			fbp = (u_char *) gp->g_fbkva;
			tp->wen = ~0;
			tp->prr = 0x3;
			tp->fben = ~0;
			save = *fbp;
			*fbp = 0xFF;
			gi->gd_colors = *fbp + 1;
			*fbp = save;
		}
	}
	return(1);
}

/*
 * Change the mode of the display.
 * Right now all we can do is grfon/grfoff.
 * Return a UNIX error number or 0 for success.
 * Function may not be needed anymore.
 */
/*ARGSUSED*/
int
tc_mode(gp, cmd, data)
	struct grf_data *gp;
	int cmd;
	caddr_t data;
{
	int error = 0;

	switch (cmd) {
	case GM_GRFON:
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
		/* XXX */
		switch (gp->g_sw->gd_hwid) {
		case GID_HRCCATSEYE:
			bcopy("HP98550", fi->name, 8);
			break;
		case GID_LRCATSEYE:
			bcopy("HP98549", fi->name, 8);
			break;
		case GID_HRMCATSEYE:
			bcopy("HP98548", fi->name, 8);
			break;
		case GID_TOPCAT:
			switch (gi->gd_colors) {
			case 64:
				bcopy("HP98547", fi->name, 8);
				break;
			case 16:
				if (gi->gd_dheight == 400) {
					bcopy("HP98543", fi->name, 8);
				} else {
					bcopy("HP98545", fi->name, 8);
				}
				break;
			case 2:
				bcopy("HP98544", fi->name, 8);
				break;
			}
			break;
		}
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
 * Topcat/catseye ite routines
 */

#define REGBASE	    	((struct tcboxfb *)(ip->regbase))
#define WINDOWMOVER 	topcat_windowmove

void
topcat_init(ip)
	struct ite_data *ip;
{
	/* XXX */
	if (ip->regbase == NULL) {
		struct grf_data *gp = ip->grf;

		ip->regbase = gp->g_regkva;
		ip->fbbase = gp->g_fbkva;
		ip->fbwidth = gp->g_display.gd_fbwidth;
		ip->fbheight = gp->g_display.gd_fbheight;
		ip->dwidth = gp->g_display.gd_dwidth;
		ip->dheight = gp->g_display.gd_dheight;
	}

	/*
	 * Catseye looks a lot like a topcat, but not completely.
	 * So, we set some bits to make it work.
	 */
	if (REGBASE->fbid != GID_TOPCAT) {
		while ((REGBASE->catseye_status & 1))
			;
		REGBASE->catseye_status = 0x0;
		REGBASE->vb_select      = 0x0;
		REGBASE->tcntrl         = 0x0;
		REGBASE->acntrl         = 0x0;
		REGBASE->pncntrl        = 0x0;
		REGBASE->rug_cmdstat    = 0x90;
	}

	/*
	 * Determine the number of planes by writing to the first frame
	 * buffer display location, then reading it back. 
	 */
	REGBASE->wen = ~0;
	REGBASE->fben = ~0;
	REGBASE->prr = RR_COPY;
	*FBBASE = 0xFF;
	ip->planemask = *FBBASE;

	/*
	 * Enable reading/writing of all the planes.
	 */
	REGBASE->fben = ip->planemask;
	REGBASE->wen  = ip->planemask;
	REGBASE->ren  = ip->planemask;
	REGBASE->prr  = RR_COPY;

	ite_fontinfo(ip);

	/*
	 * Clear the framebuffer on all planes.
	 */
	topcat_windowmove(ip, 0, 0, 0, 0, ip->fbheight, ip->fbwidth, RR_CLEAR);
	tc_waitbusy(ip->regbase, ip->planemask);

	ite_fontinit(ip);

	/*
	 * Initialize color map for color displays
	 */
	if (ip->planemask != 1) {
	  	tc_waitbusy(ip->regbase, ip->planemask);
		REGBASE->nblank = 0x01;

		tccm_waitbusy(ip->regbase);
		REGBASE->rdata  = 0x0;
		REGBASE->gdata  = 0x0;
		REGBASE->bdata  = 0x0;
		REGBASE->cindex = 0xFF;
		REGBASE->strobe = 0xFF;

		DELAY(100);
		tccm_waitbusy(ip->regbase);
		REGBASE->rdata  = 0x0;
		REGBASE->gdata  = 0x0;
		REGBASE->bdata  = 0x0;
		REGBASE->cindex = 0x0;

		DELAY(100);
		tccm_waitbusy(ip->regbase);
		REGBASE->rdata  = 0xFF;
		REGBASE->gdata  = 0xFF;
		REGBASE->bdata  = 0xFF;
		REGBASE->cindex = 0xFE;
		REGBASE->strobe = 0xFF;

		DELAY(100);
		tccm_waitbusy(ip->regbase);
		REGBASE->rdata  = 0x0;
		REGBASE->gdata  = 0x0;
		REGBASE->bdata  = 0x0;
		REGBASE->cindex = 0x0;
	}

	/*
	 * Stash the inverted cursor.
	 */
	topcat_windowmove(ip, charY(ip, ' '), charX(ip, ' '),
			  ip->cblanky, ip->cblankx, ip->ftheight,
			  ip->ftwidth, RR_COPYINVERTED);
}

void
topcat_deinit(ip)
	struct ite_data *ip;
{
	topcat_windowmove(ip, 0, 0, 0, 0, ip->fbheight, ip->fbwidth, RR_CLEAR);
	tc_waitbusy(ip->regbase, ip->planemask);

	REGBASE->nblank = ~0;
   	ip->flags &= ~ITE_INITED;
}

void
topcat_putc(ip, c, dy, dx, mode)
	struct ite_data *ip;
	int c, dy, dx, mode;
{
        int wmrr = ((mode == ATTR_INV) ? RR_COPYINVERTED : RR_COPY);
	
	topcat_windowmove(ip, charY(ip, c), charX(ip, c),
			  dy * ip->ftheight, dx * ip->ftwidth,
			  ip->ftheight, ip->ftwidth, wmrr);
}

void
topcat_cursor(ip, flag)
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
topcat_clear(ip, sy, sx, h, w)
	struct ite_data *ip;
	int sy, sx, h, w;
{
	topcat_windowmove(ip, sy * ip->ftheight, sx * ip->ftwidth,
			  sy * ip->ftheight, sx * ip->ftwidth, 
			  h  * ip->ftheight, w  * ip->ftwidth,
			  RR_CLEAR);
}

void
topcat_scroll(ip, sy, sx, count, dir)
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

	topcat_windowmove(ip, sy * ip->ftheight, sx * ip->ftwidth,
			  dy * ip->ftheight, dx * ip->ftwidth,
			  height * ip->ftheight,
			  width  * ip->ftwidth, RR_COPY);
}

void
topcat_windowmove(ip, sy, sx, dy, dx, h, w, func)
	struct ite_data *ip;
	int sy, sx, dy, dx, h, w, func;
{
  	struct tcboxfb *rp = REGBASE;
	
	if (h == 0 || w == 0)
		return;
	tc_waitbusy(ip->regbase, ip->planemask);
	rp->wmrr     = func;
	rp->source_y = sy;
	rp->source_x = sx;
	rp->dest_y   = dy;
	rp->dest_x   = dx;
	rp->wheight  = h;
	rp->wwidth   = w;
	rp->wmove    = ip->planemask;
}

/*
 * Topcat/catseye console support
 */

int
topcat_console_scan(scode, va, arg)
	int scode;
	caddr_t va;
	void *arg;
{
	struct grfreg *grf = (struct grfreg *)va;
	struct consdev *cp = arg;
	u_char *dioiidev; 
	int force = 0, pri; 

	if (grf->gr_id != GRFHWID)
		return (0);

	switch (grf->gr_id2) {
	case GID_TOPCAT:
	case GID_LRCATSEYE:
	case GID_HRCCATSEYE:
	case GID_HRMCATSEYE:
		break;

	default:
		return (0);
	}

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
	if (((cn_tab == NULL) || (cp->cn_pri > cn_tab->cn_pri)) || force) {
		cn_tab = cp;
		if (scode >= 132) {
			dioiidev = (u_char *)va;
			return ((dioiidev[0x101] + 1) * 0x100000);
		}
		return (DIOCSIZE);
	}
	return (0);
}

void
topcatcnprobe(cp)
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

	/* Abort early if the console is already forced. */
	if (conforced)
		return;

	/* Look for "internal" framebuffer. */
	va = (caddr_t)IIOV(GRFIADDR);
	grf = (struct grfreg *)va;
	if (!badaddr(va) && (grf->gr_id == GRFHWID)) {
		switch (grf->gr_id2) {
		case GID_TOPCAT:
		case GID_LRCATSEYE:
		case GID_HRCCATSEYE:
		case GID_HRMCATSEYE:
			cp->cn_pri = CN_INTERNAL;

#ifdef CONSCODE
			/*
			 * Raise our priority and save some work,
			 * if appropriate.
			 */
			if (CONSCODE == -1) {
				cp->cn_pri = CN_REMOTE;
				force = conforced = 1;
			}
#endif

			/*
			 * If our priority is higher than the currently
			 * remembered console, stash our priority, and unmap
			 * whichever device might be currently mapped.
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
	}

	console_scan(topcat_console_scan, cp);
}

void
topcatcninit(cp)
	struct consdev *cp;
{
	struct grf_data *gp = &grf_cn;
	struct grfreg *grf = (struct grfreg *)conaddr;

	/*
	 * Initialize the framebuffer hardware.
	 */
	(void)tc_init(gp, conscode, conaddr);

	/*
	 * Set up required grf data.
	 */
	switch (grf->gr_id2) {
	case GID_TOPCAT:
		gp->g_sw = &topcat_grfsw;
		break;

	case GID_LRCATSEYE:
		gp->g_sw = &lrcatseye_grfsw;
		break;

	case GID_HRCCATSEYE:
		gp->g_sw = &hrcatseye_grfsw;
		break;

	case GID_HRMCATSEYE:
		gp->g_sw = &hrmcatseye_grfsw;
		break;

	default:
		/* THIS SHOULD NEVER HAPPEN! */
		panic("topcat console: impossible!");	/* XXX won't see it */
	}
	gp->g_display.gd_id = gp->g_sw->gd_swid;
	gp->g_flags = GF_ALIVE;

	/*
	 * Initialize the terminal emulator.
	 */
	itecninit(gp, &topcat_itesw);
}

#endif /* NITE > 0 */
