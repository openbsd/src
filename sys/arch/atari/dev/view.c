/*	$NetBSD: view.c,v 1.6 1995/08/17 20:32:50 leo Exp $	*/

/*
 * Copyright (c) 1994 Christian E. Hopps
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Christian E. Hopps.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* The view major device is a placeholder device.  It serves
 * simply to map the semantics of a graphics dipslay to 
 * the semantics of a character block device.  In other
 * words the graphics system as currently built does not like to be
 * refered to by open/close/ioctl.  This device serves as
 * a interface to graphics. */

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/queue.h>
#include <machine/cpu.h>
#include <atari/dev/grfabs_reg.h>
#include <atari/dev/viewioctl.h>
#include <atari/dev/viewvar.h>
#include "view.h"

static void view_display __P((struct view_softc *));
static void view_remove __P((struct view_softc *));
static int  view_setsize __P((struct view_softc *, struct view_size *));
static int  view_get_colormap __P((struct view_softc *, colormap_t *));
static int  view_set_colormap __P((struct view_softc *, colormap_t *));

void viewclose __P((dev_t, int));
int viewioctl __P((dev_t, u_long, caddr_t, int, struct proc *));
int viewopen __P((dev_t, int));
int viewmmap __P((dev_t, int, int));

int viewprobe ();

struct view_softc views[NVIEW];
static int view_inited;

int view_default_x;
int view_default_y;
int view_default_width  = 640;
int view_default_height = 400;
int view_default_depth  = 1;

/* 
 *  functions for probeing.
 */
viewattach(cnt)
	int cnt;
{
	viewprobe();
	printf("%d view%s configured\n", NVIEW, NVIEW > 1 ? "s" : "");
}

/* this function is called early to set up a display. */
viewprobe()
{
    	int i;
	
	if(view_inited)
		return(1);

	view_inited = 1;

	for(i=0; i<NVIEW; i++) {
		views[i].view = NULL;
		views[i].flags = 0;
	}
	return(1);
}


/*
 *  Internal functions.
 */

static void
view_display (vu)
	struct view_softc *vu;
{
	int s, i;

	if (vu == NULL)
		return;
	
	s = spltty ();

	/*
	 * mark views that share this monitor as not displaying 
	 */
	for (i=0; i<NVIEW; i++) {
		if(views[i].flags & VUF_DISPLAY)
			views[i].flags &= ~VUF_DISPLAY;
	}

	vu->flags |= VUF_ADDED;
	if (vu->view) {
		vu->view->display.x = vu->size.x;
		vu->view->display.y = vu->size.y;

		grf_display_view(vu->view);

		vu->size.x = vu->view->display.x;
		vu->size.y = vu->view->display.y;
		vu->flags |= VUF_DISPLAY;
	}
	splx(s);
}

/* 
 * remove a view from our added list if it is marked as displaying
 * switch to a new display.
 */
static void
view_remove(vu)
	struct view_softc *vu;
{
	int i;

	if ((vu->flags & VUF_ADDED) == 0)
		return;

	vu->flags &= ~VUF_ADDED;
	if (vu->flags & VUF_DISPLAY) {
		for (i = 0; i < NVIEW; i++) {
			if((views[i].flags & VUF_ADDED) && &views[i] != vu) {
				view_display(&views[i]);
				break;
			}
		}
	}
	vu->flags &= ~VUF_DISPLAY;
	grf_remove_view(vu->view);
}

static int
view_setsize(vu, vs)
	struct view_softc *vu;
	struct view_size *vs;
{
	view_t	*new, *old;
	dmode_t	*dmode;
	dimen_t ns;
	int	co, cs;
   
	co = 0;
	cs = 0;
	if (vs->x != vu->size.x || vs->y != vu->size.y)
		co = 1;

	if (vs->width != vu->size.width || vs->height != vu->size.height ||
	    vs->depth != vu->size.depth)
		cs = 1;

	if (cs == 0 && co == 0)
		return(0);
    
	ns.width  = vs->width;
	ns.height = vs->height;

	if((dmode = grf_get_best_mode(&ns, vs->depth)) != NULL) {
		/*
		 * If we can't do better, leave it
		 */
		if(dmode == vu->view->mode)
			return(0);
	}
	new = grf_alloc_view(dmode, &ns, vs->depth);
	if (new == NULL)
		return(ENOMEM);
	
	old = vu->view;
	vu->view = new;
	vu->size.x = new->display.x;
	vu->size.y = new->display.y;
	vu->size.width = new->display.width;
	vu->size.height = new->display.height;
	vu->size.depth = new->bitmap->depth;

	/* 
	 * we need a custom remove here to avoid letting 
	 * another view display mark as not added or displayed 
	 */
	if (vu->flags & VUF_DISPLAY) {
		vu->flags &= ~(VUF_ADDED|VUF_DISPLAY);
		view_display(vu);
	}
	grf_free_view(old);
	return(0);
}

static int
view_get_colormap (vu, ucm)
struct view_softc	*vu;
colormap_t		*ucm;
{
	int	error;
	long	*cme;
	long	*uep;

	if(ucm->size > MAX_CENTRIES)
		return(EINVAL);
		
	/* add one incase of zero, ick. */
	cme = malloc(sizeof(ucm->entry[0])*(ucm->size+1), M_IOCTLOPS,M_WAITOK);
	if (cme == NULL)
		return(ENOMEM);

	error      = 0;	
	uep        = ucm->entry;
	ucm->entry = cme;	  /* set entry to out alloc. */
	if(vu->view == NULL || grf_get_colormap(vu->view, ucm))
		error = EINVAL;
	else error = copyout(cme, uep, sizeof(ucm->entry[0]) * ucm->size);
	ucm->entry = uep;	  /* set entry back to users. */
	free(cme, M_IOCTLOPS);
	return(error);
}

static int
view_set_colormap(vu, ucm)
struct view_softc	*vu;
colormap_t		*ucm;
{
	colormap_t	*cm;
	int		error = 0;

	if(ucm->size > MAX_CENTRIES)
		return(EINVAL);
		
	cm = malloc(sizeof(ucm->entry[0])*ucm->size + sizeof(*cm), M_IOCTLOPS,
								M_WAITOK);
	if(cm == NULL)
		return(ENOMEM);

	bcopy(ucm, cm, sizeof(colormap_t));
	cm->entry = (long *)&cm[1];		 /* table directly after. */
	if (((error = 
	    copyin(ucm->entry,cm->entry,sizeof(ucm->entry[0])*ucm->size)) == 0)
	    && (vu->view == NULL || grf_use_colormap(vu->view, cm)))
		error = EINVAL;
	free(cm, M_IOCTLOPS);
	return(error);
}

/*
 *  functions made available by conf.c
 */

/*ARGSUSED*/
int viewopen(dev, flags)
dev_t	dev;
int	flags;
{
	dimen_t			size;
	struct view_softc	*vu;

	vu = &views[minor(dev)];

	if(minor(dev) >= NVIEW)
		return(EXDEV);
	if(vu->flags & VUF_OPEN)
		return(EBUSY);

	vu->size.x = view_default_x;
	vu->size.y = view_default_y;
	size.width = vu->size.width = view_default_width;
	size.height = vu->size.height = view_default_height;
	vu->size.depth = view_default_depth;
	vu->view = grf_alloc_view(NULL, &size, vu->size.depth);
	if (vu->view == NULL)
		return(ENOMEM);

	vu->size.x = vu->view->display.x;
	vu->size.y = vu->view->display.y;
	vu->size.width = vu->view->display.width;
	vu->size.height = vu->view->display.height;
	vu->size.depth = vu->view->bitmap->depth;
       	vu->flags |= VUF_OPEN;
       	return(0);
}

/*ARGSUSED*/
void
viewclose (dev, flags)
	dev_t dev;
	int flags;
{
	struct view_softc *vu;

	vu = &views[minor(dev)];

	if ((vu->flags & VUF_OPEN) == 0)
		return;
	view_remove (vu);
	grf_free_view (vu->view);
	vu->flags = 0;
	vu->view = NULL;
}


/*ARGSUSED*/
int
viewioctl (dev, cmd, data, flag, p)
dev_t		dev;
u_long		cmd;
caddr_t		data;
int		flag;
struct proc	*p;
{
	struct view_softc	*vu;
	bmap_t			*bm;
	int			error;

	vu = &views[minor(dev)];
	error = 0;

	switch (cmd) {
	case VIOCDISPLAY:
		view_display(vu);
		break;
	case VIOCREMOVE:
		view_remove(vu);
		break;
	case VIOCGSIZE:
		bcopy(&vu->size, data, sizeof (struct view_size)); 
		break;
	case VIOCSSIZE:
		error = view_setsize(vu, (struct view_size *)data);
		break;
	case VIOCGBMAP:
		bm = (bmap_t *)data;
		bcopy(vu->view->bitmap, bm, sizeof(bmap_t));
		if ((int)p != -1) {
			bm->plane      = NULL;
			bm->hw_address = NULL;
		}
		break;
	case VIOCGCMAP:
		error = view_get_colormap(vu, (colormap_t *)data);
		break;
	case VIOCSCMAP:
		error = view_set_colormap(vu, (colormap_t *)data);
		break;
	default:
		error = EINVAL;
		break;
	}
	return(error);
}

/*ARGSUSED*/
int
viewmmap(dev, off, prot)
dev_t	dev;
int	off, prot;
{
	struct view_softc	*vu;
	bmap_t			*bm;
	u_char			*bmd_start;
	u_long			bmd_size; 

	vu = &views[minor(dev)];
	bm = vu->view->bitmap;
	bmd_start = bm->hw_address; 
	bmd_size = bm->bytes_per_row*bm->rows*bm->depth;

	if (off >= 0 && off < bmd_size)
		return(((u_int)bmd_start + off) >> PGSHIFT);

	return(-1);
}

/*ARGSUSED*/
int
viewselect(dev, rw)
dev_t	dev;
int	rw;
{
	if(rw == FREAD)
		return(0);
	return(1);
}

view_t	*
viewview(dev)
dev_t	dev;
{
	return(views[minor(dev)].view);
}
