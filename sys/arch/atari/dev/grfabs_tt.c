/*	$NetBSD: grfabs_tt.c,v 1.1 1995/08/20 18:17:34 leo Exp $	*/

/*
 * Copyright (c) 1995 Leo Weppelman.
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
 *      This product includes software developed by Leo Weppelman.
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

#ifdef TT_VIDEO
/*
 *  atari abstract graphics driver: TT-interface
 */
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/malloc.h>
#include <sys/device.h>

#include <machine/iomap.h>
#include <machine/video.h>
#include <machine/mfp.h>
#include <atari/atari/device.h>
#include <atari/dev/grfabs_reg.h>

/*
 * Function decls
 */
static void       init_view __P((view_t *, bmap_t *, dmode_t *, box_t *));
static bmap_t	  *alloc_bitmap __P((u_long, u_long, u_char));
static colormap_t *alloc_colormap __P((dmode_t *));
static void 	  free_bitmap __P((bmap_t *));
static void	  tt_display_view __P((view_t *));
static view_t	  *tt_alloc_view __P((dmode_t *, dimen_t *, u_char));
static void	  tt_free_view __P((view_t *));
static void	  tt_remove_view __P((view_t *));
static int	  tt_use_colormap __P((view_t *, colormap_t *));

/*
 * Our function switch table
 */
struct grfabs_sw tt_vid_sw = {
	tt_display_view,
	tt_alloc_view,
	tt_free_view,
	tt_remove_view,
	tt_use_colormap
};

static dmode_t vid_modes[] = {
	{ { NULL, NULL }, "sthigh", { 640,  400 },  1, RES_STHIGH, &tt_vid_sw },
	{ { NULL, NULL }, "tthigh", { 1280, 960 },  1, RES_TTHIGH, &tt_vid_sw },
	{ { NULL, NULL }, "stmid",  { 640,  200 },  2, RES_STMID , &tt_vid_sw },
	{ { NULL, NULL }, "stlow",  { 320,  200 },  4, RES_STLOW , &tt_vid_sw },
	{ { NULL, NULL }, "ttmid",  { 640,  480 },  4, RES_TTMID , &tt_vid_sw },
	{ { NULL, NULL }, "ttlow",  { 320,  480 },  8, RES_TTLOW , &tt_vid_sw },
	{ { NULL, NULL }, NULL,  }
};

/*
 * XXX: called from ite console init routine.
 * Initialize list of posible video modes.
 */
void
tt_probe_video(modelp)
MODES	*modelp;
{
	dmode_t	*dm;
	int	i;
	int	has_mono;

	/*
	 * First find out what kind of monitor is attached. Dma-sound
	 * should be off because the 'sound-done' and 'monochrome-detect'
	 * are xor-ed together. I think that shutting it down here is the
	 * wrong place.
	 */
	has_mono = (MFP->mf_gpip & IA_MONO) == 0;

	for (i = 0; (dm = &vid_modes[i])->name != NULL; i++) {
		if (has_mono && (dm->vm_reg != RES_TTHIGH))
			continue;
		if (!has_mono && (dm->vm_reg == RES_TTHIGH))
			continue;
		LIST_INSERT_HEAD(modelp, dm, link);
	}

	for (i=0; i < 16; i++)
		VIDEO->vd_tt_rgb[i] = CM_L2TT(gra_def_color16[i]);
}

static void
tt_display_view(v)
view_t *v;
{
	dmode_t	*dm = v->mode;
	bmap_t	*bm = v->bitmap;

	if (dm->current_view) {
		/*
		 * Mark current view for this mode as no longer displayed
		 */
		dm->current_view->flags &= ~VF_DISPLAY;
	}
	dm->current_view = v;
	v->flags |= VF_DISPLAY;

	tt_use_colormap(v, v->colormap);

	/* XXX: should use vbl for this	*/
	VIDEO->vd_tt_res = dm->vm_reg;
	VIDEO->vd_raml   =  (u_long)bm->hw_address & 0xff;
	VIDEO->vd_ramm   = ((u_long)bm->hw_address >>  8) & 0xff;
	VIDEO->vd_ramh   = ((u_long)bm->hw_address >> 16) & 0xff;
}

void
tt_remove_view(v)
view_t *v;
{
	dmode_t *mode = v->mode;

	if (mode->current_view == v) {
#if 0
		if (v->flags & VF_DISPLAY)
			panic("Cannot shutdown display"); /* XXX */
#endif
		mode->current_view = NULL;
	}
	v->flags &= ~VF_DISPLAY;
}

void
tt_free_view(v)
view_t *v;
{
	if(v) {
		dmode_t *md = v->mode;

		tt_remove_view(v);
		if (v->colormap != &gra_con_cmap)
			free(v->colormap, M_DEVBUF);
		free_bitmap(v->bitmap);
		if (v != &gra_con_view)
			free(v, M_DEVBUF);
	}
}

static int
tt_use_colormap(v, cm)
view_t		*v;
colormap_t	*cm;
{
	dmode_t			*dm;
	volatile u_short	*creg;
	u_long			*src;
	colormap_t		*vcm;
	u_long			*vcreg;
	u_short			ncreg;
	int			i;

	dm  = v->mode;
	vcm = v->colormap;

	/*
	 * I guess it seems reasonable to require the maps to be
	 * of the same type...
	 */
	if (cm->type != vcm->type)
		return(EINVAL);

	/*
	 * First figure out where the actual colormap resides and
	 * howmany colors are in it.
	 */
	switch (dm->vm_reg) {
		case RES_STLOW:
			creg  = &VIDEO->vd_tt_rgb[0];
			ncreg = 16;
			break;
		case RES_STMID:
			creg  = &VIDEO->vd_tt_rgb[0];
			ncreg = 4;
			break;
		case RES_STHIGH:
			creg  = &VIDEO->vd_tt_rgb[254];
			ncreg = 2;
			break;
		case RES_TTLOW:
			creg  = &VIDEO->vd_tt_rgb[0];
			ncreg = 256;
			break;
		case RES_TTMID:
			creg  = &VIDEO->vd_tt_rgb[0];
			ncreg = 16;
			break;
		case RES_TTHIGH:
			return(0);	/* No colors	*/
		default:
			panic("grf_tt:use_colormap: wrong mode!?");
	}

	/* If first entry specified beyond capabilities -> error */
	if (cm->first >= ncreg)
		return(EINVAL);

	/*
	 * A little tricky, the actual colormap pointer will be NULL
	 * when view is not displaying, valid otherwise.
	 */
	if (v->flags & VF_DISPLAY)
		creg = &creg[cm->first];
	else creg = NULL;

	vcreg  = &vcm->entry[cm->first];
	ncreg -= cm->first;
	if (cm->size > ncreg)
		return(EINVAL);
	ncreg = cm->size;

	for (i = 0, src = cm->entry; i < ncreg; i++, vcreg++) {
		*vcreg = *src++;

		/*
		 * If displaying, also update actual color registers.
		 */
		if (creg != NULL)
			*creg++ = CM_L2TT(*vcreg);
	}
	return (0);
}

static view_t *
tt_alloc_view(mode, dim, depth)
dmode_t	*mode;
dimen_t	*dim;
u_char   depth;
{
	view_t *v;
	bmap_t *bm;

	if (!atari_realconfig)
		v = &gra_con_view;
	else v = malloc(sizeof(*v), M_DEVBUF, M_NOWAIT);
	if(v == NULL)
		return (NULL);
	
	bm = alloc_bitmap(mode->size.width, mode->size.height, mode->depth);
	if (bm) {
		box_t   box;

		v->colormap = alloc_colormap(mode);
		if (v->colormap) {
			INIT_BOX(&box,0,0,mode->size.width,mode->size.height);
			init_view(v, bm, mode, &box);
			return (v);
		}
		free_bitmap(bm);
	}
	if (v != &gra_con_view)
		free(v, M_DEVBUF);
	return (NULL);
}

static void
init_view(v, bm, mode, dbox)
view_t	*v;
bmap_t	*bm;
dmode_t	*mode;
box_t	*dbox;
{
	v->bitmap = bm;
	v->mode   = mode;
	v->flags  = 0;
	bcopy(dbox, &v->display, sizeof(box_t));
}

/* bitmap functions */

static bmap_t *
alloc_bitmap(width, height, depth)
u_long	width, height;
u_char	depth;
{
	int     i;
	u_long  total_size, bm_size;
	void	*hw_address;
	bmap_t	*bm;

	/*
	 * Sigh, it seems for mapping to work we need the bitplane data to
	 *  1: be aligned on a page boundry.
	 *  2: be n pages large.
	 * 
	 * why? because the user gets a page aligned address, if this is before
	 * your allocation, too bad.  Also it seems that the mapping routines
	 * do not watch to closely to the allowable length. so if you go over
	 * n pages by less than another page, the user gets to write all over
	 * the entire page. Since you did not allocate up to a page boundry
	 * (or more) the user writes into someone elses memory. -ch
	 */
	bm_size    = atari_round_page((width * height * depth) / NBBY);
	total_size = bm_size + sizeof(bmap_t) + NBPG;

	if ((bm = (bmap_t*)alloc_stmem(total_size, &hw_address)) == NULL)
		return(NULL);

	bm->plane         = (u_char*)bm + sizeof(bmap_t);
	bm->plane         = (u_char*)atari_round_page(bm->plane);
	bm->hw_address    = (u_char*)hw_address + sizeof(bmap_t);
	bm->hw_address    = (u_char*)atari_round_page(bm->hw_address);
	bm->bytes_per_row = (width * depth) / NBBY;
	bm->rows          = height;
	bm->depth         = depth;

	bzero(bm->plane, bm_size);
	return (bm);
}

static void
free_bitmap(bm)
bmap_t *bm;
{
	if (bm)
		free_stmem(bm);
}

static colormap_t *
alloc_colormap(dm)
dmode_t		*dm;
{
	int		nentries, i;
	colormap_t	*cm;
	u_char		type = CM_COLOR;

	switch (dm->vm_reg) {
		case RES_STLOW:
		case RES_TTMID:
			nentries = 16;
			break;
		case RES_STMID:
			nentries = 4;
			break;
		case RES_STHIGH:
			nentries = 2;
			break;
		case RES_TTLOW:
			nentries = 256;
			break;
		case RES_TTHIGH:
			type     = CM_MONO;
			nentries = 0;
			break;
		default:
			panic("grf_tt:alloc_colormap: wrong mode!?");
	}
	if (!atari_realconfig) {
		cm = &gra_con_cmap;
		cm->entry = gra_con_colors;
	}
	else {
		int size;

		size = sizeof(*cm) + (nentries * sizeof(cm->entry[0]));
		cm   = malloc(size, M_DEVBUF, M_NOWAIT);
		if (cm == NULL)
			return (NULL);
		cm->entry = (long *)&cm[1];

	}
	if ((cm->type = type) == CM_COLOR)
		cm->red_mask = cm->green_mask = cm->blue_mask = 0xf;
	cm->first = 0;
	cm->size  = nentries;

	for (i = 0; i < nentries; i++)
		cm->entry[i] = gra_def_color16[i % 16];
	return (cm);
}
#endif /* TT_VIDEO */
