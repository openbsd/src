/*	$OpenBSD: cgtwo.c,v 1.21 2002/08/02 16:13:07 millert Exp $	*/
/*	$NetBSD: cgtwo.c,v 1.22 1997/05/24 20:16:12 pk Exp $ */

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
 *	from: @(#)cgthree.c	8.2 (Berkeley) 10/30/93
 */

/*
 * color display (cgtwo) driver.
 *
 * Does not handle interrupts, even though they can occur.
 *
 * XXX should defer colormap updates to vertical retrace interrupts
 */

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
#if defined(SUN4)
#include <machine/eeprom.h>
#endif
#include <machine/conf.h>
#include <machine/cgtworeg.h>


/* per-display variables */
struct cgtwo_softc {
	struct	device sc_dev;		/* base device */
	struct	fbdevice sc_fb;		/* frame buffer device */
	struct rom_reg	sc_phys;	/* display RAM (phys addr) */
	int	sc_bustype;		/* type of bus we live on */
	volatile struct cg2statusreg *sc_reg;	/* CG2 control registers */
	volatile u_short *sc_cmap;
#define sc_redmap(sc)	((sc)->sc_cmap)
#define sc_greenmap(sc)	((sc)->sc_cmap + CG2_CMSIZE)
#define sc_bluemap(sc)	((sc)->sc_cmap + 2 * CG2_CMSIZE)
};

/* autoconfiguration driver */
static void	cgtwoattach(struct device *, struct device *, void *);
static int	cgtwomatch(struct device *, void *, void *);
static void	cgtwounblank(struct device *);
int		cgtwogetcmap(struct cgtwo_softc *, struct fbcmap *);
int		cgtwoputcmap(struct cgtwo_softc *, struct fbcmap *);

struct cfattach cgtwo_ca = {
	sizeof(struct cgtwo_softc), cgtwomatch, cgtwoattach
};

struct cfdriver cgtwo_cd = {
	NULL, "cgtwo", DV_DULL
};

/* frame buffer generic driver */
static struct fbdriver cgtwofbdriver = {
	cgtwounblank, cgtwoopen, cgtwoclose, cgtwoioctl, cgtwommap
};

extern int fbnode;
extern struct tty *fbconstty;

/*
 * Match a cgtwo.
 */
int
cgtwomatch(parent, vcf, aux)
	struct device *parent;
	void *vcf, *aux;
{
	struct cfdata *cf = vcf;
	struct confargs *ca = aux;
	struct romaux *ra = &ca->ca_ra;
#if defined(SUN4)
	caddr_t tmp;
#endif

	/*
	 * Mask out invalid flags from the user.
	 */
	cf->cf_flags &= FB_USERMASK;

	if (ca->ca_bustype != BUS_VME16)
		return (0);

	if (strcmp(cf->cf_driver->cd_name, ra->ra_name))
		return (0);

#if defined(SUN4)
	if (!CPU_ISSUN4 || cf->cf_unit != 0)
		return (0);

	/* XXX - Must do our own mapping at CG2_CTLREG_OFF */
	bus_untmp();
	tmp = (caddr_t)mapdev(ra->ra_reg, TMPMAP_VA, CG2_CTLREG_OFF, NBPG);
	if (probeget(tmp, 2) != -1)
		return 1;
#endif
	return 0;
}

/*
 * Attach a display.  We need to notice if it is the console, too.
 */
void
cgtwoattach(parent, self, args)
	struct device *parent, *self;
	void *args;
{
	register struct cgtwo_softc *sc = (struct cgtwo_softc *)self;
	register struct confargs *ca = args;
	register int node = 0;
	int isconsole = 0;
	char *nam = NULL;

	sc->sc_fb.fb_driver = &cgtwofbdriver;
	sc->sc_fb.fb_device = &sc->sc_dev;
	sc->sc_fb.fb_type.fb_type = FBTYPE_SUN2COLOR;
	sc->sc_fb.fb_flags = sc->sc_dev.dv_cfdata->cf_flags;

	switch (ca->ca_bustype) {
	case BUS_VME16:
		node = 0;
		nam = "cgtwo";
		break;

	default:
		panic("cgtwoattach: impossible bustype");
		/* NOTREACHED */
	}

	sc->sc_fb.fb_type.fb_depth = 8;
	fb_setsize(&sc->sc_fb, sc->sc_fb.fb_type.fb_depth,
	    1152, 900, node, ca->ca_bustype);

	sc->sc_fb.fb_type.fb_cmsize = 256;
	sc->sc_fb.fb_type.fb_size = round_page(CG2_MAPPED_SIZE);
	printf(": %s, %d x %d", nam,
	    sc->sc_fb.fb_type.fb_width, sc->sc_fb.fb_type.fb_height);

	/*
	 * When the ROM has mapped in a cgtwo display, the address
	 * maps only the video RAM, so in any case we have to map the
	 * registers ourselves.  We only need the video RAM if we are
	 * going to print characters via rconsole.
	 */
#if defined(SUN4)
	if (CPU_ISSUN4) {
		struct eeprom *eep = (struct eeprom *)eeprom_va;
		/*
		 * Assume this is the console if there's no eeprom info
		 * to be found.
		 */
		if (eep == NULL || eep->eeConsole == EE_CONS_COLOR)
			isconsole = (fbconstty != NULL);
		else
			isconsole = 0;
	}
#endif
	sc->sc_phys = ca->ca_ra.ra_reg[0];
	/* Apparently, the pixels are 32-bit data space */
	sc->sc_phys.rr_iospace = PMAP_VME32;
	sc->sc_bustype = ca->ca_bustype;

	if ((sc->sc_fb.fb_pixels = ca->ca_ra.ra_vaddr) == NULL && isconsole) {
		/* this probably cannot happen, but what the heck */
		sc->sc_fb.fb_pixels = mapiodev(&sc->sc_phys, CG2_PIXMAP_OFF,
					       CG2_PIXMAP_SIZE);
	}

	sc->sc_reg = (volatile struct cg2statusreg *)
	    mapiodev(ca->ca_ra.ra_reg,
		     CG2_ROPMEM_OFF + offsetof(struct cg2fb, status.reg),
		     sizeof(struct cg2statusreg));

	sc->sc_cmap = (volatile u_short *)
	    mapiodev(ca->ca_ra.ra_reg,
		     CG2_ROPMEM_OFF + offsetof(struct cg2fb, redmap[0]),
		     3 * CG2_CMSIZE);

	if (isconsole) {
		printf(" (console)\n");
#ifdef RASTERCONSOLE
		fbrcons_init(&sc->sc_fb);
#endif
	} else
		printf("\n");

	if (node == fbnode || CPU_ISSUN4)
		fb_attach(&sc->sc_fb, isconsole);
}

int
cgtwoopen(dev, flags, mode, p)
	dev_t dev;
	int flags, mode;
	struct proc *p;
{
	int unit = minor(dev);

	if (unit >= cgtwo_cd.cd_ndevs || cgtwo_cd.cd_devs[unit] == NULL)
		return (ENXIO);
	return (0);
}

int
cgtwoclose(dev, flags, mode, p)
	dev_t dev;
	int flags, mode;
	struct proc *p;
{

	return (0);
}

int
cgtwoioctl(dev, cmd, data, flags, p)
	dev_t dev;
	u_long cmd;
	register caddr_t data;
	int flags;
	struct proc *p;
{
	register struct cgtwo_softc *sc = cgtwo_cd.cd_devs[minor(dev)];
	register struct fbgattr *fba;

	switch (cmd) {

	case FBIOGTYPE:
		*(struct fbtype *)data = sc->sc_fb.fb_type;
		break;

	case FBIOGATTR:
		fba = (struct fbgattr *)data;
		fba->real_type = sc->sc_fb.fb_type.fb_type;
		fba->owner = 0;		/* XXX ??? */
		fba->fbtype = sc->sc_fb.fb_type;
		fba->sattr.flags = 0;
		fba->sattr.emu_type = sc->sc_fb.fb_type.fb_type;
		fba->sattr.dev_specific[0] = -1;
		fba->emu_types[0] = sc->sc_fb.fb_type.fb_type;
		fba->emu_types[1] = -1;
		break;

	case FBIOGETCMAP:
		return cgtwogetcmap(sc, (struct fbcmap *) data);

	case FBIOPUTCMAP:
		return cgtwoputcmap(sc, (struct fbcmap *) data);

	case FBIOGVIDEO:
		*(int *)data = sc->sc_reg->video_enab;
		break;

	case FBIOSVIDEO:
		sc->sc_reg->video_enab = (*(int *)data) & 1;
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
cgtwounblank(dev)
	struct device *dev;
{
	struct cgtwo_softc *sc = (struct cgtwo_softc *)dev;
	sc->sc_reg->video_enab = 1;
}

/*
 */
int
cgtwogetcmap(sc, cmap)
	register struct cgtwo_softc *sc;
	register struct fbcmap *cmap;
{
	u_char red[CG2_CMSIZE], green[CG2_CMSIZE], blue[CG2_CMSIZE];
	int error;
	u_int start, count, ecount;
	register u_int i;
	register volatile u_short *p;

	start = cmap->index;
	count = cmap->count;
	ecount = start + count;
	if (start >= CG2_CMSIZE || count > CG2_CMSIZE - start)
		return (EINVAL);

	/* XXX - Wait for retrace? */

	/* Copy hardware to local arrays. */
	p = &sc_redmap(sc)[start];
	for (i = start; i < ecount; i++)
		red[i] = *p++;
	p = &sc_greenmap(sc)[start];
	for (i = start; i < ecount; i++)
		green[i] = *p++;
	p = &sc_bluemap(sc)[start];
	for (i = start; i < ecount; i++)
		blue[i] = *p++;

	/* Copy local arrays to user space. */
	if ((error = copyout(red + start, cmap->red, count)) != 0)
		return (error);
	if ((error = copyout(green + start, cmap->green, count)) != 0)
		return (error);
	if ((error = copyout(blue + start, cmap->blue, count)) != 0)
		return (error);

	return (0);
}

/*
 */
int
cgtwoputcmap(sc, cmap)
	register struct cgtwo_softc *sc;
	register struct fbcmap *cmap;
{
	u_char red[CG2_CMSIZE], green[CG2_CMSIZE], blue[CG2_CMSIZE];
	int error;
	u_int start, count, ecount;
	register u_int i;
	register volatile u_short *p;

	start = cmap->index;
	count = cmap->count;
	ecount = start + count;
	if (start >= CG2_CMSIZE || count > CG2_CMSIZE - start)
		return (EINVAL);

	/* Copy from user space to local arrays. */
	if ((error = copyin(cmap->red, red + start, count)) != 0)
		return (error);
	if ((error = copyin(cmap->green, green + start, count)) != 0)
		return (error);
	if ((error = copyin(cmap->blue, blue + start, count)) != 0)
		return (error);

	/* XXX - Wait for retrace? */

	/* Copy from local arrays to hardware. */
	p = &sc_redmap(sc)[start];
	for (i = start; i < ecount; i++)
		*p++ = red[i];
	p = &sc_greenmap(sc)[start];
	for (i = start; i < ecount; i++)
		*p++ = green[i];
	p = &sc_bluemap(sc)[start];
	for (i = start; i < ecount; i++)
		*p++ = blue[i];

	return (0);
}

/*
 * Return the address that would map the given device at the given
 * offset, allowing for the given protection, or return -1 for error.
 */
paddr_t
cgtwommap(dev, off, prot)
	dev_t dev;
	off_t off;
	int prot;
{
	register struct cgtwo_softc *sc = cgtwo_cd.cd_devs[minor(dev)];

	if (off & PGOFSET)
		panic("cgtwommap");

	if (off < 0)
		return (-1);
	if ((unsigned)off >= sc->sc_fb.fb_type.fb_size)
		return (-1);

	return (REG2PHYS(&sc->sc_phys, off) | PMAP_NC);
}
