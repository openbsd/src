/*	$OpenBSD: grf.c,v 1.13 2001/11/05 20:12:31 art Exp $	*/
/*	$NetBSD: grf.c,v 1.32 1996/12/23 09:10:01 veego Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
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
 * from: Utah $Hdr: grf.c 1.31 91/01/21$
 *
 *	@(#)grf.c	7.8 (Berkeley) 5/7/91
 */

/*
 * Graphics display driver for the Amiga
 * This is the hardware-independent portion of the driver.
 * Hardware access is through the grf_softc->g_mode routine.
 */

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/ioctl.h>
#include <sys/device.h>
#include <sys/file.h>
#include <sys/malloc.h>
#include <sys/systm.h>
#include <sys/vnode.h>
#include <sys/mman.h>
#include <vm/vm.h>
#include <machine/cpu.h>
#include <machine/fbio.h>
#include <amiga/amiga/color.h>	/* DEBUG */
#include <amiga/amiga/device.h>
#include <amiga/dev/grfioctl.h>
#include <amiga/dev/grfvar.h>
#include <amiga/dev/itevar.h>
#include <amiga/dev/viewioctl.h>

#include <sys/conf.h>
#include <machine/conf.h>

#include "view.h"
#include "grf.h"

#if NGRF > 0
#include "ite.h"
#if NITE == 0
#define	ite_on(u,f)
#define	ite_off(u,f)
#define ite_reinit(d)
#endif

int grfon __P((dev_t));
int grfoff __P((dev_t));
int grfsinfo __P((dev_t, struct grfdyninfo *));
#ifdef BANKEDDEVPAGER
int grfbanked_get __P((dev_t, off_t, int));
int grfbanked_cur __P((dev_t));
int grfbanked_set __P((dev_t, int));
#endif

void grfattach __P((struct device *, struct device *, void *));
int grfmatch __P((struct device *, void *, void *));
int grfprint __P((void *, const char *));
/*
 * pointers to grf drivers device structs 
 */
struct grf_softc *grfsp[NGRF];

struct cfattach grf_ca = {
	sizeof(struct device), grfmatch, grfattach
};

struct cfdriver grf_cd = {
	NULL, "grf", DV_DULL, NULL, 0
};

/*
 * only used in console init.
 */
static struct cfdata *grf_cfdata;

/*
 * match if the unit of grf matches its perspective 
 * low level board driver.
 */
int
grfmatch(pdp, match, auxp)
	struct device *pdp;
	void *match, *auxp;
{
	struct cfdata *cfp = match;

	if (cfp->cf_unit != ((struct grf_softc *)pdp)->g_unit)
		return(0);
	grf_cfdata = cfp;
	return(1);
}

/*
 * attach.. plug pointer in and print some info.
 * then try and attach an ite to us. note: dp is NULL
 * during console init.
 */
void
grfattach(pdp, dp, auxp)
	struct device *pdp, *dp;
	void *auxp;
{
	struct grf_softc *gp;
	int maj;

	gp = (struct grf_softc *)pdp;
	grfsp[gp->g_unit] = (struct grf_softc *)pdp;

	/*
	 * find our major device number 
	 */
	for(maj = 0; maj < nchrdev; maj++)
		if (cdevsw[maj].d_open == grfopen)
			break;

	gp->g_grfdev = makedev(maj, gp->g_unit);
	if (dp != NULL) {
		printf(": width %d height %d", gp->g_display.gd_dwidth,
		    gp->g_display.gd_dheight);
		if (gp->g_display.gd_colors == 2)
			printf(" monochrome\n");
		else
			printf(" colors %d\n", gp->g_display.gd_colors);
	}
	
	/*
	 * try and attach an ite
	 */
	amiga_config_found(grf_cfdata, dp, gp, grfprint);
}

int
grfprint(auxp, pnp)
	void *auxp;
	const char *pnp;
{
	if (pnp)
		printf("ite at %s", pnp);
	return(UNCONF);
}

/*ARGSUSED*/
int
grfopen(dev, flags, devtype, p)
	dev_t dev;
	int flags, devtype;
	struct proc *p;
{
	struct grf_softc *gp;

	if (GRFUNIT(dev) >= NGRF || (gp = grfsp[GRFUNIT(dev)]) == NULL)
		return(ENXIO);

	if ((gp->g_flags & GF_ALIVE) == 0)
		return(ENXIO);

	if ((gp->g_flags & (GF_OPEN|GF_EXCLUDE)) == (GF_OPEN|GF_EXCLUDE))
		return(EBUSY);

	return(0);
}

/*ARGSUSED*/
int
grfclose(dev, flags, mode, p)
	dev_t dev;
	int flags;
	int mode;
	struct proc *p;
{
	struct grf_softc *gp;

	gp = grfsp[GRFUNIT(dev)];
	(void)grfoff(dev);
	gp->g_flags &= GF_ALIVE;
	return(0);
}

/*ARGSUSED*/
int
grfioctl(dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	struct grf_softc *gp;
	int error;

	gp = grfsp[GRFUNIT(dev)];
	error = 0;

	switch (cmd) {
	case OGRFIOCGINFO:
	        /* argl.. no bank-member.. */
	  	bcopy((caddr_t)&gp->g_display, data, sizeof(struct grfinfo)-4);
		break;
	case GRFIOCGINFO:
		bcopy((caddr_t)&gp->g_display, data, sizeof(struct grfinfo));
		break;
	case GRFIOCON:
		error = grfon(dev);
		break;
	case GRFIOCOFF:
		error = grfoff(dev);
		break;
	case GRFIOCSINFO:
		error = grfsinfo(dev, (struct grfdyninfo *) data);
		break;
	case GRFGETVMODE:
		return(gp->g_mode(gp, GM_GRFGETVMODE, data, 0, 0));
	case GRFSETVMODE:
		error = gp->g_mode(gp, GM_GRFSETVMODE, data, 0, 0);
		if (error == 0 && gp->g_itedev && !(gp->g_flags & GF_GRFON))
			ite_reinit(gp->g_itedev);
		break;
	case GRFGETNUMVM:
		return(gp->g_mode(gp, GM_GRFGETNUMVM, data, 0, 0));
	/*
	 * these are all hardware dependant, and have to be resolved
	 * in the respective driver.
	 */
	case GRFIOCPUTCMAP:
	case GRFIOCGETCMAP:
	case GRFIOCSSPRITEPOS:
	case GRFIOCGSPRITEPOS:
	case GRFIOCSSPRITEINF:
	case GRFIOCGSPRITEINF:
	case GRFIOCGSPRITEMAX:
	case GRFIOCBITBLT:
    	case GRFIOCSETMON:
	case GRFIOCBLANK:	/* blank ioctl, IOCON/OFF will turn ite on */
	case GRFTOGGLE: /* Toggles between Cirrus boards and native ECS on
                     Amiga. 15/11/94 ill */
		/*
		 * We need the minor dev number to get the overlay/image
		 * information for grf_ul.
		 */
		return(gp->g_mode(gp, GM_GRFIOCTL, data, cmd, dev));

	case FBIOSVIDEO:
		return(gp->g_mode(gp, GM_GRFIOCTL, data, GRFIOCBLANK, dev));

	default:
#if NVIEW > 0
		/*
		 * check to see whether it's a command recognized by the
		 * view code if the unit is 0
		 * XXX 
		 */
		if (GRFUNIT(dev) == 0)
			return(viewioctl(dev, cmd, data, flag, p));
#endif
		error = EINVAL;
		break;

	}
	return(error);
}

/*ARGSUSED*/
int
grfselect(dev, rw, p)
	dev_t dev;
	int rw;
	struct proc *p;
{
	if (rw == FREAD)
		return(0);
	return(1);
}

/*
 * map the contents of a graphics display card into process' 
 * memory space.
 */
paddr_t
grfmmap(dev, off, prot)
	dev_t dev;
	off_t off;
	int prot;
{
	struct grf_softc *gp;
	struct grfinfo *gi;
	
	gp = grfsp[GRFUNIT(dev)];
	gi = &gp->g_display;

	/* 
	 * control registers
	 */
	if (off >= 0 && off < gi->gd_regsize)
		return(((u_int)gi->gd_regaddr + off) >> PGSHIFT);

	/*
	 * frame buffer
	 */
	if (off >= gi->gd_regsize && off < gi->gd_regsize+gi->gd_fbsize) {
		off -= gi->gd_regsize;
#ifdef BANKEDDEVPAGER
		if (gi->gd_bank_size)
			off %= gi->gd_bank_size;
#endif
		return(((u_int)gi->gd_fbaddr + off) >> PGSHIFT);
	}
	/* bogus */
	return(-1);
}

int
grfon(dev)
	dev_t dev;
{
	struct grf_softc *gp;

	gp = grfsp[GRFUNIT(dev)];

	if (gp->g_flags & GF_GRFON)
		return(0);

	gp->g_flags |= GF_GRFON;
	if (gp->g_itedev != NODEV)
		ite_off(gp->g_itedev, 3);

	return(gp->g_mode(gp, (dev & GRFOVDEV) ? GM_GRFOVON : GM_GRFON,
							NULL, 0, 0));
}

int
grfoff(dev)
	dev_t dev;
{
	struct grf_softc *gp;
	int error;

	gp = grfsp[GRFUNIT(dev)];

	if ((gp->g_flags & GF_GRFON) == 0)
		return(0);

	gp->g_flags &= ~GF_GRFON;
	error = gp->g_mode(gp, (dev & GRFOVDEV) ? GM_GRFOVOFF : GM_GRFOFF,
							NULL, 0, 0);

	/*
	 * Closely tied together no X's
	 */
	if (gp->g_itedev != NODEV)
		ite_on(gp->g_itedev, 2);

	return(error);
}

int
grfsinfo(dev, dyninfo)
	dev_t dev;
	struct grfdyninfo *dyninfo;
{
	struct grf_softc *gp;
	int error;

	gp = grfsp[GRFUNIT(dev)];
	error = gp->g_mode(gp, GM_GRFCONFIG, dyninfo, 0, 0);

	/*
	 * Closely tied together no X's
	 */
	if (gp->g_itedev != NODEV)
		ite_reinit(gp->g_itedev);
	return(error);
}

#ifdef BANKEDDEVPAGER

int
grfbanked_get (dev, off, prot)
     dev_t dev;
     off_t off;
     int   prot;
{
	struct grf_softc *gp;
	struct grfinfo *gi;
	int error, bank;

	gp = grfsp[GRFUNIT(dev)];
	gi = &gp->g_display;

	off -= gi->gd_regsize;
	if (off < 0 || off >= gi->gd_fbsize)
		return -1;

	error = gp->g_mode(gp, GM_GRFGETBANK, &bank, off, prot);
	return error ? -1 : bank;
}

int
grfbanked_cur (dev)
	dev_t dev;
{
	struct grf_softc *gp;
	int error, bank;

	gp = grfsp[GRFUNIT(dev)];

	error = gp->g_mode(gp, GM_GRFGETCURBANK, &bank, 0, 0);
	return(error ? -1 : bank);
}

int
grfbanked_set (dev, bank)
	dev_t dev;
	int bank;
{
	struct grf_softc *gp;

	gp = grfsp[GRFUNIT(dev)];
	return(gp->g_mode(gp, GM_GRFSETBANK, &bank, 0, 0) ? -1 : 0);
}

#endif /* BANKEDDEVPAGER */
#endif	/* NGRF > 0 */
