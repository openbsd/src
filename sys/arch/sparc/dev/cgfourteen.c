/*	$OpenBSD: cgfourteen.c,v 1.10 2002/08/02 16:13:07 millert Exp $	*/
/*	$NetBSD: cgfourteen.c,v 1.7 1997/05/24 20:16:08 pk Exp $ */

/*
 * Copyright (c) 1996 
 *	The President and Fellows of Harvard College. All rights reserved.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by Harvard University.
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
 *	This product includes software developed by Harvard University and
 *	its contributors.
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
 *   Based on:
 *	NetBSD: cgthree.c,v 1.28 1996/05/31 09:59:22 pk Exp
 *	NetBSD: cgsix.c,v 1.25 1996/04/01 17:30:00 christos Exp
 */

/*
 * Driver for Campus-II on-board mbus-based video (cgfourteen).
 * Provides minimum emulation of a Sun cgthree 8-bit framebuffer to
 * allow X to run.
 *
 * Does not handle interrupts, even though they can occur.
 *
 * XXX should defer colormap updates to vertical retrace interrupts
 */

/* 
 * The following is for debugging only; it opens up a security hole
 * enabled by allowing any user to map the control registers for the 
 * cg14 into their space.
 */
#undef CG14_MAP_REGS

/*
 * The following enables 24-bit operation: when opened, the framebuffer
 * will switch to 24-bit mode (actually 32-bit mode), and provide a 
 * simple cg8 emulation.
 *
 * XXX Note that the code enabled by this define is currently untested/broken.
 */
#undef CG14_CG8

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#include <sys/mman.h>
#include <sys/tty.h>
#include <sys/conf.h>

#include <uvm/uvm_extern.h>

#include <machine/fbio.h>
#include <machine/autoconf.h>
#include <machine/pmap.h>
#include <machine/fbvar.h>
#include <machine/cpu.h>
#include <machine/conf.h>

#include <sparc/dev/cgfourteenreg.h>
#include <sparc/dev/cgfourteenvar.h>

/* autoconfiguration driver */
static void	cgfourteenattach(struct device *, struct device *, void *);
static int	cgfourteenmatch(struct device *, void *, void *);
static void	cgfourteenunblank(struct device *);

struct cfattach cgfourteen_ca = {
	sizeof(struct cgfourteen_softc), cgfourteenmatch, cgfourteenattach
};

struct cfdriver cgfourteen_cd = {
	NULL, "cgfourteen", DV_DULL
};

/* frame buffer generic driver */
static struct fbdriver cgfourteenfbdriver = {
	cgfourteenunblank, cgfourteenopen, cgfourteenclose, cgfourteenioctl, 
	cgfourteenmmap
};

extern int fbnode;
extern struct tty *fbconstty;

static void cg14_set_video(struct cgfourteen_softc *, int);
static int  cg14_get_video(struct cgfourteen_softc *);
static int  cg14_get_cmap(struct fbcmap *, union cg14cmap *, int);
static int  cg14_put_cmap(struct fbcmap *, union cg14cmap *, int);
static void cg14_load_hwcmap(struct cgfourteen_softc *, int, int);
static void cg14_init(struct cgfourteen_softc *);
static void cg14_reset(struct cgfourteen_softc *);
static void cg14_loadomap(struct cgfourteen_softc *);/* cursor overlay */
static void cg14_setcursor(struct cgfourteen_softc *);/* set position */
static void cg14_loadcursor(struct cgfourteen_softc *);/* set shape */

/*
 * Match a cgfourteen.
 */
int
cgfourteenmatch(parent, vcf, aux)
	struct device *parent;
	void *vcf, *aux;
{
	struct cfdata *cf = vcf;
	struct confargs *ca = aux;
	struct romaux *ra = &ca->ca_ra;

	/*
	 * Mask out invalid flags from the user.
	 */
	cf->cf_flags &= FB_USERMASK;

	/* Check driver name */
	if (strcmp(cf->cf_driver->cd_name, ra->ra_name))
		return (0);

	/*
	 * The cgfourteen is a local-bus video adaptor, accessed directly
	 * via the processor, and not through device space or an external
	 * bus. Thus we look _only_ at the obio bus.
	 * Additionally, these things exist only on the Sun4m.
	 */
	if (CPU_ISSUN4M && ca->ca_bustype == BUS_OBIO)
		return(1);
	return (0);
}

/*
 * Attach a display.  We need to notice if it is the console, too.
 */
void
cgfourteenattach(parent, self, args)
	struct device *parent, *self;
	void *args;
{
	register struct cgfourteen_softc *sc = (struct cgfourteen_softc *)self;
	register struct confargs *ca = args;
	register int node = 0, ramsize;
	register u_int32_t *lut;
	int i, isconsole;

	sc->sc_fb.fb_driver = &cgfourteenfbdriver;
	sc->sc_fb.fb_device = &sc->sc_dev;
	sc->sc_fb.fb_flags = sc->sc_dev.dv_cfdata->cf_flags;

	/*
	 * We're emulating a cg3/8, so represent ourselves as one
	 */
#ifdef CG14_CG8
	sc->sc_fb.fb_type.fb_type = FBTYPE_MEMCOLOR;
#else
	sc->sc_fb.fb_type.fb_type = FBTYPE_SUN3COLOR;
#endif

	node = ca->ca_ra.ra_node;
#ifdef CG14_CG8
	sc->sc_fb.fb_type.fb_depth = 32;
#else
	sc->sc_fb.fb_type.fb_depth = 8;
#endif
	fb_setsize(&sc->sc_fb, sc->sc_fb.fb_type.fb_depth,
	    1152, 900, node, ca->ca_bustype);

	ramsize = round_page(sc->sc_fb.fb_type.fb_height *
			     sc->sc_fb.fb_linebytes);

	sc->sc_fb.fb_type.fb_cmsize = CG14_CLUT_SIZE;
	sc->sc_fb.fb_type.fb_size = ramsize;

	/*
	 * Now map in the 8 useful pages of registers
	 */
	if (ca->ca_ra.ra_len < 0x10000) {
#ifdef DIAGNOSTIC
		printf("warning: can't find all cgfourteen registers...\n");
#endif
		ca->ca_ra.ra_len = 0x10000;
	}
	sc->sc_ctl = (struct cg14ctl *) mapiodev(ca->ca_ra.ra_reg, 0, 
						 ca->ca_ra.ra_len);

	sc->sc_hwc = (struct cg14curs *) ((u_int)sc->sc_ctl + 
					  CG14_OFFSET_CURS);
	sc->sc_dac = (struct cg14dac *) ((u_int)sc->sc_ctl +
					 CG14_OFFSET_DAC);
	sc->sc_xlut = (struct cg14xlut *) ((u_int)sc->sc_ctl +
					   CG14_OFFSET_XLUT);
	sc->sc_clut1 = (struct cg14clut *) ((u_int)sc->sc_ctl +
					    CG14_OFFSET_CLUT1);
	sc->sc_clut2 = (struct cg14clut *) ((u_int)sc->sc_ctl +
					    CG14_OFFSET_CLUT2);
	sc->sc_clut3 = (struct cg14clut *) ((u_int)sc->sc_ctl +
					    CG14_OFFSET_CLUT3);
	sc->sc_clutincr = (u_int *) ((u_int)sc->sc_ctl +
				     CG14_OFFSET_CLUTINCR);
	
	/*
	 * Stash the physical address of the framebuffer for use by mmap
	 */
	if (ca->ca_ra.ra_nreg < 2)
		panic("cgfourteen with only one register set; can't find"
		      " framebuffer");
	sc->sc_phys = ca->ca_ra.ra_reg[1];

#if defined(DEBUG) && defined(CG14_MAP_REGS)
	/* Store the physical address of the control registers */
	sc->sc_regphys = ca->ca_ra.ra_reg[0];
#endif

	/*
	 * Let the user know that we're here
	 */
#ifdef CG14_CG8
	printf(": cgeight emulated at %dx%dx24bpp",
	    sc->sc_fb.fb_type.fb_width, sc->sc_fb.fb_type.fb_height);
#else
	printf(": cgthree emulated at %dx%dx8bpp",
	       sc->sc_fb.fb_type.fb_width, sc->sc_fb.fb_type.fb_height);
#endif
	/*
	 * Enable the video, but don't change the pixel depth.
	 */
	cg14_set_video(sc, 1);

	/*
	 * Grab the initial colormap
	 */
	lut = (u_int32_t *) sc->sc_clut1->clut_lut;
	for (i = 0; i < CG14_CLUT_SIZE; i++)
		sc->sc_cmap.cm_chip[i] = lut[i];

	/* See if we're the console */
	isconsole = node == fbnode && fbconstty != NULL;

	/*
	 * We don't use the raster console since the cg14 is fast enough
	 * already.
	 */
#ifdef notdef
	/*
	 * When the ROM has mapped in a cgfourteen display, the address
	 * maps only the video RAM, so in any case we have to map the
	 * registers ourselves.  We only need the video RAM if we are
	 * going to print characters via rconsole.
	 */
	if ((sc->sc_fb.fb_pixels = ca->ca_ra.ra_vaddr) == NULL && isconsole) {
		/* this probably cannot happen, but what the heck */
		sc->sc_fb.fb_pixels = mapiodev(ca->ca_ra.ra_reg, CG3REG_MEM,
					       ramsize);
	}
#endif /* notdef */


	if (isconsole) {
		printf(" (console)\n");
#ifdef notdef
#ifdef RASTERCONSOLE
		fbrcons_init(&sc->sc_fb);
#endif
#endif /* notdef */
	} else
		printf("\n");

	/* Attach to /dev/fb */
	if (node == fbnode)
		fb_attach(&sc->sc_fb, isconsole);
}

/*
 * Keep track of the number of opens made. In the 24-bit driver, we need to 
 * switch to 24-bit mode on the first open, and switch back to 8-bit on
 * the last close. This kind of nonsense is needed to give screenblank
 * a fighting chance of working.
 */
static int cg14_opens = 0;

int
cgfourteenopen(dev, flags, mode, p)
	dev_t dev;
	int flags, mode;
	struct proc *p;
{
	register struct cgfourteen_softc *sc = cgfourteen_cd.cd_devs[minor(dev)];
	int unit = minor(dev);
	int s, oldopens;

	if (unit >= cgfourteen_cd.cd_ndevs || 
	    cgfourteen_cd.cd_devs[unit] == NULL)
		return (ENXIO);

	s = splhigh();
	oldopens = cg14_opens++;
	splx(s);

	/* Setup the cg14 as we want it, and save the original PROM state */
	if (oldopens == 0)	/* first open only, to make screenblank work */
		cg14_init(sc);

	return (0);
}

int
cgfourteenclose(dev, flags, mode, p)
	dev_t dev;
	int flags, mode;
	struct proc *p;
{
	register struct cgfourteen_softc *sc = cgfourteen_cd.cd_devs[minor(dev)];
	int s, opens;

	s = splhigh();
	opens = --cg14_opens;
	if (cg14_opens < 0)
		opens = cg14_opens = 0;
	splx(s);

	/*
	 * Restore video state to make the PROM happy, on last close.
	 */
	if (opens == 0)
		cg14_reset(sc);

	return (0);
}

int
cgfourteenioctl(dev, cmd, data, flags, p)
	dev_t dev;
	u_long cmd;
	register caddr_t data;
	int flags;
	struct proc *p;
{
	register struct cgfourteen_softc *sc = cgfourteen_cd.cd_devs[minor(dev)];
	register struct fbgattr *fba;
	union cg14cursor_cmap tcm;
	int v, error;
	u_int count;

	switch (cmd) {

	case FBIOGTYPE:
		*(struct fbtype *)data = sc->sc_fb.fb_type;
		break;

	case FBIOGATTR:
		fba = (struct fbgattr *)data;
		fba->real_type = FBTYPE_MDICOLOR;
		fba->owner = 0;		/* XXX ??? */
		fba->fbtype = sc->sc_fb.fb_type;
		fba->sattr.flags = 0;
		fba->sattr.emu_type = sc->sc_fb.fb_type.fb_type;
		fba->sattr.dev_specific[0] = -1;
		fba->emu_types[0] = sc->sc_fb.fb_type.fb_type;
		fba->emu_types[1] = -1;
		break;

	case FBIOGETCMAP:
		return (cg14_get_cmap((struct fbcmap *)data, &sc->sc_cmap, 
				     CG14_CLUT_SIZE));

	case FBIOPUTCMAP:
		/* copy to software map */
#define p ((struct fbcmap *)data)
#ifdef CG14_CG8
		p->index &= 0xffffff;
#endif
		error = cg14_put_cmap(p, &sc->sc_cmap, CG14_CLUT_SIZE);
		if (error)
			return (error);
		/* now blast them into the chip */
		/* XXX should use retrace interrupt */
		cg14_load_hwcmap(sc, p->index, p->count);
#undef p
		break;

	case FBIOGVIDEO:
		*(int *)data = cg14_get_video(sc);
		break;

	case FBIOSVIDEO:
		cg14_set_video(sc, *(int *)data);
		break;

/* these are for both FBIOSCURSOR and FBIOGCURSOR */
#define p ((struct fbcursor *)data)
#define cc (&sc->sc_cursor)
	case FBIOGCURSOR:
		/* do not quite want everything here... */
		p->set = FB_CUR_SETALL;	/* close enough, anyway */
		p->enable = cc->cc_enable;
		p->pos = cc->cc_pos;
		p->hot = cc->cc_hot;
		p->size = cc->cc_size;

		/* begin ugh ... can we lose some of this crap?? */
		if (p->image != NULL) {
			count = cc->cc_size.y * 32 / NBBY;
			error = copyout((caddr_t)cc->cc_cplane,
			    (caddr_t)p->image, count);
			if (error)
				return (error);
			error = copyout((caddr_t)cc->cc_eplane,
			    (caddr_t)p->mask, count);
			if (error)
				return (error);
		}
		if (p->cmap.red != NULL) {
			error = cg14_get_cmap(&p->cmap,
			    (union cg14cmap *)&cc->cc_color, 2);
			if (error)
				return (error);
		} else {
			p->cmap.index = 0;
			p->cmap.count = 2;
		}
		/* end ugh */
		break;

	case FBIOSCURSOR:
		/*
		 * For setcmap and setshape, verify parameters, so that
		 * we do not get halfway through an update and then crap
		 * out with the software state screwed up.
		 */
		v = p->set;
		if (v & FB_CUR_SETCMAP) {
			/*
			 * This use of a temporary copy of the cursor
			 * colormap is not terribly efficient, but these
			 * copies are small (8 bytes)...
			 */
			tcm = cc->cc_color;
			error = cg14_put_cmap(&p->cmap, (union cg14cmap *)&tcm,
					      2);
			if (error)
				return (error);
		}
		if (v & FB_CUR_SETSHAPE) {
			if ((u_int)p->size.x > 32 || (u_int)p->size.y > 32)
				return (EINVAL);
			count = p->size.y * 32 / NBBY;
			if (!uvm_useracc(p->image, count, B_READ) ||
			    !uvm_useracc(p->mask, count, B_READ))
				return (EFAULT);
		}

		/* parameters are OK; do it */
		if (v & (FB_CUR_SETCUR | FB_CUR_SETPOS | FB_CUR_SETHOT)) {
			if (v & FB_CUR_SETCUR)
				cc->cc_enable = p->enable;
			if (v & FB_CUR_SETPOS)
				cc->cc_pos = p->pos;
			if (v & FB_CUR_SETHOT)
				cc->cc_hot = p->hot;
			cg14_setcursor(sc);
		}
		if (v & FB_CUR_SETCMAP) {
			cc->cc_color = tcm;
			cg14_loadomap(sc); /* XXX defer to vertical retrace */
		}
		if (v & FB_CUR_SETSHAPE) {
			cc->cc_size = p->size;
			count = p->size.y * 32 / NBBY;
			bzero((caddr_t)cc->cc_eplane, sizeof cc->cc_eplane);
			bzero((caddr_t)cc->cc_cplane, sizeof cc->cc_cplane);
			bcopy(p->mask, (caddr_t)cc->cc_eplane, count);
			bcopy(p->image, (caddr_t)cc->cc_cplane, count);
			cg14_loadcursor(sc);
		}
		break;

#undef cc
#undef p
	case FBIOGCURPOS:
		*(struct fbcurpos *)data = sc->sc_cursor.cc_pos;
		break;

	case FBIOSCURPOS:
		sc->sc_cursor.cc_pos = *(struct fbcurpos *)data;
		cg14_setcursor(sc);
		break;

	case FBIOGCURMAX:
		/* max cursor size is 32x32 */
		((struct fbcurpos *)data)->x = 32;
		((struct fbcurpos *)data)->y = 32;
		break;

	default:
		return (ENOTTY);
	}
	return (0);
}

/*
 * Undo the effect of an FBIOSVIDEO that turns the video off.
 */
static void
cgfourteenunblank(dev)
	struct device *dev;
{

	cg14_set_video((struct cgfourteen_softc *)dev, 1);
}

/*
 * Return the address that would map the given device at the given
 * offset, allowing for the given protection, or return -1 for error.
 *
 * The cg14 frame buffer can be mapped in either 8-bit or 32-bit mode
 * starting at the address stored in the PROM. In 8-bit mode, the X
 * channel is not present, and can be ignored. In 32-bit mode, mapping
 * at 0K delivers a 32-bpp buffer where the upper 8 bits select the X
 * channel information. We hardwire the Xlut to all zeroes to insure
 * that, regardless of this value, direct 24-bit color access will be
 * used.
 * 
 * Alternatively, mapping the frame buffer at an offset of 16M seems to 
 * tell the chip to ignore the X channel. XXX where does it get the X value
 * to use?
 */
paddr_t
cgfourteenmmap(dev, off, prot)
	dev_t dev;
	off_t off;
	int prot;
{
	register struct cgfourteen_softc *sc = cgfourteen_cd.cd_devs[minor(dev)];
	
#define CG3START		(128*1024 + 128*1024)
#define CG8START		(256*1024)
#define NOOVERLAY	(0x04000000)

	if (off & PGOFSET)
		panic("cgfourteenmmap");

	if (off < 0)
		return (-1);

#if defined(DEBUG) && defined(CG14_MAP_REGS) /* XXX: security hole */
	/*
	 * Map the control registers into user space. Should only be 
	 * used for debugging!
	 */
	if ((u_int)off >= 0x10000000 && (u_int)off < 0x10000000 + 16*4096) {
		off -= 0x10000000;
		return (REG2PHYS(&sc->sc_regphys, off, 0) | PMAP_NC);
	}
#endif
	
	if ((u_int)off >= NOOVERLAY)
		off -= NOOVERLAY;
#ifdef CG14_CG8
	else if ((u_int)off >= CG8START) {
		off -= CG8START;
	}
#else
	else if ((u_int)off >= CG3START)
		off -= CG3START;
#endif
	else
		off = 0;

	if ((unsigned)off >= sc->sc_fb.fb_type.fb_size *
		sc->sc_fb.fb_type.fb_depth/8) {
#ifdef DEBUG
		printf("\nmmap request out of bounds: request 0x%x, "
		    "bound 0x%x\n", (unsigned) off, 
		    (unsigned)sc->sc_fb.fb_type.fb_size);
#endif
		return (-1);
	}

	/*
	 * Use PMAP_NC to disable the cache, since otherwise refresh is
	 * very confused.
	 */
	return (REG2PHYS(&sc->sc_phys, off) | PMAP_NC);
}

/*
 * Miscellaneous helper functions 
 */

/* Initialize the framebuffer, storing away useful state for later reset */
static void 
cg14_init(sc)
	struct cgfourteen_softc *sc;
{
	register u_int32_t *clut;
	register u_int8_t  *xlut;
	register int i;

	/*
	 * We stash away the following to restore on close:
	 *
	 * 	color look-up table 1 	(sc->sc_saveclut)
	 *	x look-up table		(sc->sc_savexlut)
	 *	control register	(sc->sc_savectl)
	 *	cursor control register (sc->sc_savehwc)
	 */
	sc->sc_savectl = sc->sc_ctl->ctl_mctl;
	sc->sc_savehwc = sc->sc_hwc->curs_ctl;

	clut = (u_int32_t *) sc->sc_clut1->clut_lut;
	xlut = (u_int8_t *) sc->sc_xlut->xlut_lut;
	for (i = 0; i < CG14_CLUT_SIZE; i++) {
		sc->sc_saveclut.cm_chip[i] = clut[i];
		sc->sc_savexlut[i] = xlut[i];
	}

#ifdef CG14_CG8
	/*
	 * Enable the video, and put in 24 bit mode.
	 */
	sc->sc_ctl->ctl_mctl = CG14_MCTL_ENABLEVID | CG14_MCTL_PIXMODE_32 |
		CG14_MCTL_POWERCTL;

	/* 
	 * Zero the xlut to enable direct-color mode
	 */
	bzero(sc->sc_xlut, CG14_CLUT_SIZE);
#else
	/*
	 * Enable the video and put it in 8 bit mode
	 */
	sc->sc_ctl->ctl_mctl = CG14_MCTL_ENABLEVID | CG14_MCTL_PIXMODE_8 |
		CG14_MCTL_POWERCTL;
#endif
}

static void
cg14_reset(sc)	/* Restore the state saved on cg14_init */
	struct cgfourteen_softc *sc;
{
	register u_int32_t *clut;
	register u_int8_t  *xlut;
	register int i;

	/*
	 * We restore the following, saved in cg14_init:
	 *
	 * 	color look-up table 1 	(sc->sc_saveclut)
	 *	x look-up table		(sc->sc_savexlut)
	 *	control register	(sc->sc_savectl)
	 *	cursor control register (sc->sc_savehwc)
	 *
	 * Note that we don't touch the video enable bits in the
	 * control register; otherwise, screenblank wouldn't work.
	 */
	sc->sc_ctl->ctl_mctl = (sc->sc_ctl->ctl_mctl & (CG14_MCTL_ENABLEVID |
							CG14_MCTL_POWERCTL)) |
				(sc->sc_savectl & ~(CG14_MCTL_ENABLEVID |
						    CG14_MCTL_POWERCTL));
	sc->sc_hwc->curs_ctl = sc->sc_savehwc;

	clut = (u_int32_t *) sc->sc_clut1->clut_lut;
	xlut = (u_int8_t *) sc->sc_xlut->xlut_lut;
	for (i = 0; i < CG14_CLUT_SIZE; i++) {
		clut[i] = sc->sc_saveclut.cm_chip[i];
		xlut[i] = sc->sc_savexlut[i];
	}
}

/* Enable/disable video display; power down monitor if DPMS-capable */
static void
cg14_set_video(sc, enable)
	struct cgfourteen_softc *sc;
	int enable;
{
	/* 
	 * We can only use DPMS to power down the display if the chip revision
	 * is greater than 0.
	 */
	if (enable) {
		if ((sc->sc_ctl->ctl_rsr & CG14_RSR_REVMASK) > 0)
			sc->sc_ctl->ctl_mctl |= (CG14_MCTL_ENABLEVID |
						 CG14_MCTL_POWERCTL);
		else
			sc->sc_ctl->ctl_mctl |= CG14_MCTL_ENABLEVID;
	} else {
		if ((sc->sc_ctl->ctl_rsr & CG14_RSR_REVMASK) > 0)
			sc->sc_ctl->ctl_mctl &= ~(CG14_MCTL_ENABLEVID |
						  CG14_MCTL_POWERCTL);
		else
			sc->sc_ctl->ctl_mctl &= ~CG14_MCTL_ENABLEVID;
	}
}

/* Get status of video display */
static int
cg14_get_video(sc)
	struct cgfourteen_softc *sc;
{
	return ((sc->sc_ctl->ctl_mctl & CG14_MCTL_ENABLEVID) != 0);
}

/* Read the software shadow colormap */
static int 
cg14_get_cmap(p, cm, cmsize)
	register struct fbcmap *p;
	union cg14cmap *cm;
	int cmsize;
{
        register u_int i, start, count;
        register u_char *cp;
 
        start = p->index;
        count = p->count;
	if (start >= cmsize || count > cmsize - start)
#ifdef DEBUG
	{
		printf("putcmaperror: start %u cmsize %d count %u\n",
		    start, cmsize, count);
#endif
                return (EINVAL);
#ifdef DEBUG
	}
#endif

        if (!uvm_useracc(p->red, count, B_WRITE) ||
            !uvm_useracc(p->green, count, B_WRITE) ||
            !uvm_useracc(p->blue, count, B_WRITE))
                return (EFAULT);
        for (cp = &cm->cm_map[start][0], i = 0; i < count; cp += 4, i++) {
                p->red[i] = cp[3];
                p->green[i] = cp[2];
                p->blue[i] = cp[1];
        }
        return (0);
}

/* Write the software shadow colormap */
static int
cg14_put_cmap(p, cm, cmsize)
        register struct fbcmap *p;
        union cg14cmap *cm;
        int cmsize;
{
        register u_int i, start, count;
        register u_char *cp;
 
        start = p->index;
        count = p->count;
	if (start >= cmsize || count > cmsize - start)
#ifdef DEBUG
	{
		printf("putcmaperror: start %u cmsize %d count %u\n",
		    start, cmsize, count);
#endif
                return (EINVAL);
#ifdef DEBUG
	}
#endif

        if (!uvm_useracc(p->red, count, B_READ) ||
            !uvm_useracc(p->green, count, B_READ) ||
            !uvm_useracc(p->blue, count, B_READ))
                return (EFAULT);
        for (cp = &cm->cm_map[start][0], i = 0; i < count; cp += 4, i++) {
                cp[3] = p->red[i];
                cp[2] = p->green[i];
                cp[1] = p->blue[i];
		cp[0] = 0;	/* no alpha channel */
        }
        return (0);
}

static void
cg14_load_hwcmap(sc, start, ncolors)
	register struct cgfourteen_softc *sc;
	register int start, ncolors;
{
	/* XXX switch to auto-increment, and on retrace intr */
	
	/* Setup pointers to source and dest */
	register u_int32_t *colp = &sc->sc_cmap.cm_chip[start];
	volatile register u_int32_t *lutp = &sc->sc_clut1->clut_lut[start];

	/* Copy by words */
	while (--ncolors >= 0)
		*lutp++ = *colp++;
}

/*
 * Load the cursor (overlay `foreground' and `background') colors.
 */
static void
cg14_setcursor(sc)
	register struct cgfourteen_softc *sc;
{
	/* we need to subtract the hot-spot value here */
#define COORD(f) (sc->sc_cursor.cc_pos.f - sc->sc_cursor.cc_hot.f)

	sc->sc_hwc->curs_ctl = (sc->sc_cursor.cc_enable ? CG14_CURS_ENABLE : 0);
	sc->sc_hwc->curs_x = COORD(x);
	sc->sc_hwc->curs_y = COORD(y);

#undef COORD
}

static void
cg14_loadcursor(sc)
	register struct cgfourteen_softc *sc;
{
	register volatile struct cg14curs *hwc;
	register u_int edgemask, m;
	register int i;

	/*
	 * Keep the top size.x bits.  Here we *throw out* the top
	 * size.x bits from an all-one-bits word, introducing zeros in
	 * the top size.x bits, then invert all the bits to get what
	 * we really wanted as our mask.  But this fails if size.x is
	 * 32---a sparc uses only the low 5 bits of the shift count---
	 * so we have to special case that.
	 */
	edgemask = ~0;
	if (sc->sc_cursor.cc_size.x < 32)
		edgemask = ~(edgemask >> sc->sc_cursor.cc_size.x);
	hwc = sc->sc_hwc;
	for (i = 0; i < 32; i++) {
		m = sc->sc_cursor.cc_eplane[i] & edgemask;
		hwc->curs_plane0[i] = m;
		hwc->curs_plane1[i] = m & sc->sc_cursor.cc_cplane[i];
	}
}

static void
cg14_loadomap(sc)
	register struct cgfourteen_softc *sc;
{
	/* set background color */
	sc->sc_hwc->curs_color1 = sc->sc_cursor.cc_color.cm_chip[0];
	/* set foreground color */
	sc->sc_hwc->curs_color2 = sc->sc_cursor.cc_color.cm_chip[1];
}
