/*	$NetBSD: grf.c,v 1.6 1995/06/26 19:55:45 leo Exp $	*/

/*
 * Copyright (c) 1995 Leo Weppelman
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
 * Graphics display driver for the Atari
 * This is the hardware-independent portion of the driver.
 * Hardware access is through the grf_softc->g_mode routine.
 */

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/ioctl.h>
#include <sys/device.h>
#include <sys/file.h>
#include <sys/malloc.h>
#include <sys/conf.h>
#include <sys/systm.h>
#include <sys/vnode.h>
#include <sys/mman.h>
#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>
#include <machine/cpu.h>
#include <atari/atari/device.h>
#include <atari/dev/grfioctl.h>
#include <atari/dev/grfabs_reg.h>
#include <atari/dev/grfvar.h>
#include <atari/dev/itevar.h>
#include <atari/dev/viewioctl.h>
#include <atari/dev/viewvar.h>

#include "grf.h"
#if NGRF > 0

#include "ite.h"
#if NITE == 0
#define	ite_on(u,f)
#define	ite_off(u,f)
#define ite_reinit(d)
#endif

int grfopen __P((dev_t, int, int, struct proc *));
int grfclose __P((dev_t, int));
int grfioctl __P((dev_t, u_long, caddr_t, int, struct proc *));
int grfselect __P((dev_t, int));
int grfmmap __P((dev_t, int, int));

int grfon __P((dev_t));
int grfoff __P((dev_t));
int grfsinfo __P((dev_t, struct grfdyninfo *));
#ifdef BANKEDDEVPAGER
int grfbanked_get __P((dev_t, off_t, int));
int grfbanked_cur __P((dev_t));
int grfbanked_set __P((dev_t, int));
#endif
static void grf_viewsync __P((struct grf_softc *));
static int  grf_mode __P((struct grf_softc *, int, void *, int, int));

int grfbusprint __P((void *auxp, const char *));
int grfbusmatch __P((struct device *, struct cfdata *, void *));
void grfbusattach __P((struct device *, struct device *, void *));

void grfattach __P((struct device *, struct device *, void *));
int grfmatch __P((struct device *, struct cfdata *, void *));
int grfprint __P((void *, const char *));
/*
 * pointers to grf drivers device structs 
 */
struct grf_softc *grfsp[NGRF];


struct cfdriver grfbuscd = {
	NULL, "grfbus", (cfmatch_t)grfbusmatch, grfbusattach, DV_DULL,
	sizeof(struct device)
};

struct cfdriver grfcd = {
	NULL, "grf", (cfmatch_t)grfmatch, grfattach, DV_DULL,
	sizeof(struct grf_softc), NULL, 0
};

/*
 * only used in console init.
 */
static struct cfdata *cfdata_gbus  = NULL;
static struct cfdata *cfdata_grf   = NULL;

int
grfbusmatch(pdp, cfp, auxp)
struct device	*pdp;
struct cfdata	*cfp;
void		*auxp;
{
	if(strcmp(auxp, grfbuscd.cd_name))
		return(0);

	if((atari_realconfig == 0) || (cfdata_gbus == NULL)) {
		/*
		 * Probe layers we depend on
		 */
		if(grfabs_probe() == 0)
			return(0);
		viewprobe();

		if(atari_realconfig == 0) {
			/*
			 * XXX: console init opens view 0
			 */
			if(viewopen(0, 0))
				return(0);
			cfdata_gbus = cfp;
		}
	}
	return(1);	/* Always there	*/
}

void
grfbusattach(pdp, dp, auxp)
struct device	*pdp, *dp;
void		*auxp;
{
	static  int	did_cons = 0;
		int	i;

	if(dp == NULL) { /* Console init	*/
		did_cons = 1;
		i = 0;
		atari_config_found(cfdata_gbus, NULL, (void*)&i, grfbusprint);
	}
	else {
		printf("\n");
		for(i = 0; i < NGRF; i++) {
			/*
			 * Skip opening view[0] when we this is the console.
			 */
			if(!did_cons || (i > 0))
			    if(viewopen(i, 0))
				break;
			config_found(dp, (void*)&i, grfbusprint);
		}
	}
}

int
grfbusprint(auxp, name)
     void        *auxp;
     const char  *name;
{
	if(name == NULL)
		return(UNCONF);
	return(QUIET);
}


int
grfmatch(pdp, cfp, auxp)
struct device	*pdp;
struct cfdata	*cfp;
void	*auxp;
{
	int	unit = *(int*)auxp;

	/*
	 * Match only on unit indicated by grfbus attach.
	 */
	if(cfp->cf_unit != unit)
		return(0);

	cfdata_grf = cfp;
	return(1);
}

/*
 * attach: initialize the grf-structure and try to attach an ite to us.
 * note  : dp is NULL during early console init.
 */
void
grfattach(pdp, dp, auxp)
struct device	*pdp, *dp;
void		*auxp;
{
	static struct grf_softc		congrf;
	       struct grf_softc		*gp;
	       int			maj;

	/*
	 * find our major device number 
	 */
	for(maj = 0; maj < nchrdev; maj++)
		if (cdevsw[maj].d_open == grfopen)
			break;

	/*
	 * Handle exeption case: early console init
	 */
	if(dp == NULL) {
		congrf.g_unit    = 0;
		congrf.g_grfdev  = makedev(maj, 0);
		congrf.g_flags   = GF_ALIVE;
		congrf.g_mode    = grf_mode;
		congrf.g_conpri  = grfcc_cnprobe();
		congrf.g_viewdev = congrf.g_unit;
		grfcc_iteinit(&congrf);
		grf_viewsync(&congrf);

		/* Attach console ite */
		atari_config_found(cfdata_grf, NULL, &congrf, grfprint);
		return;
	}

	gp = (struct grf_softc *)dp;
	gp->g_unit = gp->g_device.dv_unit;
	grfsp[gp->g_unit] = gp;

	if((cfdata_grf != NULL) && (gp->g_unit == 0)) {
		/*
		 * We inited earlier just copy the info, take care
		 * not to copy the device struct though.
		 */
		bcopy(&congrf.g_display, &gp->g_display,
			(char *)&gp[1] - (char *)&gp->g_display);
	}
	else {
		gp->g_grfdev  = makedev(maj, gp->g_unit);
		gp->g_flags   = GF_ALIVE;
		gp->g_mode    = grf_mode;
		gp->g_conpri  = 0;
		gp->g_viewdev = gp->g_unit;
		grfcc_iteinit(gp);
		grf_viewsync(gp);
	}

	printf(": width %d height %d", gp->g_display.gd_dwidth,
		    gp->g_display.gd_dheight);
	if(gp->g_display.gd_colors == 2)
		printf(" monochrome\n");
	else printf(" colors %d\n", gp->g_display.gd_colors);
	
	/*
	 * try and attach an ite
	 */
	config_found(dp, gp, grfprint);
}

int
grfprint(auxp, pnp)
     void       *auxp;
     const char *pnp;
{
	if(pnp)
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

	if (GRFUNIT(dev) >= NGRF)
		return(ENXIO);

	gp = grfsp[GRFUNIT(dev)];

	if ((gp->g_flags & GF_ALIVE) == 0)
		return(ENXIO);

	if ((gp->g_flags & (GF_OPEN|GF_EXCLUDE)) == (GF_OPEN|GF_EXCLUDE))
		return(EBUSY);
	grf_viewsync(gp);

	return(0);
}

/*ARGSUSED*/
int
grfclose(dev, flags)
	dev_t dev;
	int flags;
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
dev_t		dev;
u_long		cmd;
int		flag;
caddr_t		data;
struct proc	*p;
{
	struct grf_softc	*gp;
	int			error;

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
		return(gp->g_mode(gp, GM_GRFGETVMODE, data));
	case GRFSETVMODE:
		error = gp->g_mode(gp, GM_GRFSETVMODE, data);
		if (error == 0 && gp->g_itedev)
			ite_reinit(gp->g_itedev);
		break;
	case GRFGETNUMVM:
		return(gp->g_mode(gp, GM_GRFGETNUMVM, data));
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
	default:
		/*
		 * check to see whether it's a command recognized by the
		 * view code.
		 */
		return(viewioctl(gp->g_viewdev, cmd, data, flag, p));
		error = EINVAL;
		break;

	}
	return(error);
}

/*ARGSUSED*/
int
grfselect(dev, rw)
	dev_t dev;
	int rw;
{
	if (rw == FREAD)
		return(0);
	return(1);
}

/*
 * map the contents of a graphics display card into process' 
 * memory space.
 */
int
grfmmap(dev, off, prot)
dev_t	dev;
int	off, prot;
{
	struct grf_softc	*gp;
	struct grfinfo		*gi;
	
	gp = grfsp[GRFUNIT(dev)];
	gi = &gp->g_display;

	/*
	 * frame buffer
	 */
	if ((off >= 0) && (off < gi->gd_fbsize))
		return (((u_int)gi->gd_fbaddr + off) >> PGSHIFT);
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

	return(gp->g_mode(gp, (dev & GRFOVDEV) ? GM_GRFOVON : GM_GRFON));
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
	error = gp->g_mode(gp, (dev & GRFOVDEV) ? GM_GRFOVOFF : GM_GRFOFF);

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
	error = gp->g_mode(gp, GM_GRFCONFIG, dyninfo);

	/*
	 * Closely tied together no X's
	 */
	if (gp->g_itedev != NODEV)
		ite_reinit(gp->g_itedev);
	return(error);
}

/*
 * Get the grf-info in sync with underlying view.
 */
static void
grf_viewsync(gp)
struct grf_softc *gp;
{
	struct view_size	vs;
	bmap_t			bm;
	struct grfinfo		*gi;

	gi = &gp->g_display;

	viewioctl(gp->g_viewdev, VIOCGBMAP, &bm, 0, -1);
  
	gp->g_data = (caddr_t) 0xDeadBeaf; /* not particularly clean.. */
  
	gi->gd_fbaddr  = bm.hw_address;
	gi->gd_fbsize  = bm.depth*bm.bytes_per_row*bm.rows;

	if(viewioctl(gp->g_viewdev, VIOCGSIZE, &vs, 0, -1)) {
		/*
		 * fill in some default values...
		 * XXX: Should _never_ happen
		 */
		vs.width  = 640;
		vs.height = 400;
		vs.depth  = 1;
	}
	gi->gd_colors = 1 << vs.depth;
	gi->gd_planes = vs.depth;
  
	gi->gd_fbwidth         = vs.width;
	gi->gd_fbheight        = vs.height;
	gi->gd_dyn.gdi_fbx     = 0;
	gi->gd_dyn.gdi_fby     = 0;
	gi->gd_dyn.gdi_dwidth  = vs.width;
	gi->gd_dyn.gdi_dheight = vs.height;
	gi->gd_dyn.gdi_dx      = 0;
	gi->gd_dyn.gdi_dy      = 0;
}    

/*
 * Change the mode of the display.
 * Right now all we can do is grfon/grfoff.
 * Return a UNIX error number or 0 for success.
 */
/*ARGSUSED*/
static int
grf_mode(gp, cmd, arg, a2, a3)
struct grf_softc	*gp;
int			cmd, a2, a3;
void			*arg;
{
	switch (cmd) {
		case GM_GRFON:
			/*
			 * Get in sync with view, ite might have changed it.
			 */
			grf_viewsync(gp);
			viewioctl(gp->g_viewdev, VIOCDISPLAY, NULL, 0, -1);
			return(0);
	case GM_GRFOFF:
			viewioctl(gp->g_viewdev, VIOCREMOVE, NULL, 0, -1);
			return(0);
	case GM_GRFCONFIG:
	default:
			break;
	}
	return(EINVAL);
}
#endif	/* NGRF > 0 */
