/*	$NetBSD: grfabs_cc.c,v 1.10 1995/10/05 12:41:12 chopps Exp $	*/

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

/*
 *  abstract interface for custom chips to the amiga abstract graphics driver.
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/cdefs.h>
#include <sys/queue.h>

#include <amiga/amiga/custom.h>
#include <amiga/amiga/cc.h>

#include <amiga/dev/grfabs_reg.h>
#include <amiga/dev/grfabs_ccreg.h>

monitor_t *m_this;
mdata_t *m_this_data;
char   *monitor_name = "CCMONITOR";
monitor_t monitor;
mdata_t monitor_data;
cop_t  *null_mode_copper_list;

#if defined (GRF_PAL)
#  if defined (GRF_A2024)
dmode_t pal_a2024_mode;
dmdata_t pal_a2024_mode_data;
cop_t  *pal_a2024_frames[F_QD_TOTAL];
u_char *hedley_init;		/* init bitplane. */
dmode_t *p24_this;
dmdata_t *p24_this_data;

dmode_t pal_hires_dlace_mode;
dmdata_t pal_hires_dlace_mode_data;
cop_t  *pal_hires_dlace_frames[F_LACE_TOTAL];
dmode_t *phdl_this;
dmdata_t *phdl_this_data;
#  endif /* GRF_A2024 */

#  if defined (GRF_AGA)
dmode_t paga_mode;
dmdata_t paga_mode_data;
cop_t *paga_frames[F_TOTAL];
dmode_t *paga_this;
dmdata_t *paga_this_data;
#  endif /* GRF_AGA */

dmode_t pal_hires_lace_mode;
dmdata_t pal_hires_lace_mode_data;
cop_t  *pal_hires_lace_frames[F_LACE_TOTAL];
dmode_t *phl_this;
dmdata_t *phl_this_data;

dmode_t pal_hires_mode;
dmdata_t pal_hires_mode_data;
cop_t  *pal_hires_frames[F_TOTAL];
dmode_t *ph_this;
dmdata_t *ph_this_data;
#endif /* PAL */

#if defined (GRF_NTSC)
#  if defined (GRF_A2024)
dmode_t a2024_mode;
dmdata_t a2024_mode_data;
cop_t  *a2024_frames[F_QD_TOTAL];
u_char *hedley_init;		/* init bitplane. */
dmode_t *a24_this;
dmdata_t *a24_this_data;

dmode_t hires_dlace_mode;
dmdata_t hires_dlace_mode_data;
cop_t  *hires_dlace_frames[F_LACE_TOTAL];
dmode_t *hdl_this;
dmdata_t *hdl_this_data;
#  endif /* GRF_A2024 */

#  if defined (GRF_AGA)
dmode_t aga_mode;
dmdata_t aga_mode_data;
cop_t *aga_frames[F_TOTAL];
dmode_t *aga_this;
dmdata_t *aga_this_data;
#  endif /* GRF_AGA */

dmode_t hires_lace_mode;
dmdata_t hires_lace_mode_data;
cop_t  *hires_lace_frames[F_LACE_TOTAL];
dmode_t *hl_this;
dmdata_t *hl_this_data;

void    display_hires_view(view_t * v);
dmode_t hires_mode;
dmdata_t hires_mode_data;
cop_t  *hires_frames[F_TOTAL];
dmode_t *h_this;
dmdata_t *h_this_data;
#endif /* GRF_NTSC */

#ifdef GRF_AGA
#define	AGA_ENABLE	0x0001
#define	AGA_ENABLE2	0x0002
#define AGA_TRACE	0x0004
#define AGA_TRACE2	0x0008
#define AGA_VGAONLY	0x0010
#define AGA_VGA31KHZ	0x0020

int aga_enable = 0;	/* set by start_c(), or can be patched */
#endif

/* monitor functions. */
monitor_t *
cc_init_monitor()
{
	cop_t  *cp;
	
	if (m_this)
		return(m_this);

	cc_monitor = m_this = &monitor;
	/* turn sprite DMA off. we don't support them yet. */
	custom.dmacon = DMAF_SPRITE;

	/* makre sure sprite data registers are clear as well */
	custom.spr[0].data = 0;
	custom.spr[0].datb = 0;

	m_this->name = monitor_name;
	m_this_data = m_this->data = &monitor_data;

	m_this->get_current_mode = get_current_mode;
	m_this->vbl_handler = (vbl_handler_func *) monitor_vbl_handler;
	m_this->get_next_mode = get_next_mode;
	m_this->get_best_mode = get_best_mode;

	m_this->alloc_bitmap = alloc_bitmap;
	m_this->free_bitmap = free_bitmap;

	m_this_data->current_mode = NULL;
	LIST_INIT(&m_this_data->modes);

	cp = null_mode_copper_list = alloc_chipmem(sizeof(cop_t) * 4);
	if (!cp)
		panic("no chipmem for grf.");

	CMOVE(cp, R_COLOR00, 0x0000);	/* background is black */
	CMOVE(cp, R_BPLCON0, 0x0000);	/* no planes to fetch from */
	CWAIT(cp, 255, 255);	/* COPEND */
	CWAIT(cp, 255, 255);	/* COPEND really */

	/* install this list and turn DMA on */
	custom.cop1lc = PREP_DMA_MEM(null_mode_copper_list);
	custom.copjmp1 = 0;
	custom.dmacon = DMAF_SETCLR | DMAF_MASTER | DMAF_RASTER \
	    |DMAF_COPPER;

	cc_init_modes();
	LIST_INSERT_HEAD(monitors, m_this, link);
	return (m_this);
}

void
monitor_vbl_handler(m)
	monitor_t *m;
{
	dmdata_t *dmd;

	if (m_this_data->current_mode == NULL)
		return;

	dmd = DMDATA(m_this_data->current_mode);
	if (dmd)
		dmd->vbl_handler(m_this_data->current_mode);
}

dmode_t *
get_current_mode()
{
	if (m_this_data->current_mode)
		return(m_this_data->current_mode);
	else
		return(NULL);
}

dmode_t *
get_next_mode(d)
	dmode_t *d;
{
	if (d)
		return(d->link.le_next);
	return(m_this_data->modes.lh_first);
}

/* XXX needs to have more control attributes */
dmode_t *
get_best_mode(size, depth)
	dimen_t *size;
	u_char depth;
{
	dmode_t *save;
	dmode_t *dm;
	long    dt, dx, dy, ct;
	dmdata_t *dmd;

	save = NULL;
	dm = m_this_data->modes.lh_first;
	while (dm != NULL) {
		dmd = dm->data;
		if (depth > dmd->max_depth || depth < dmd->min_depth) {
			dm = dm->link.le_next;
			continue;
		} else if (size->width > dmd->max_size.width ||
		    size->height > dmd->max_size.height) {
			dm = dm->link.le_next;
			continue;
		} else if (size->width < dmd->min_size.width ||
		    size->height < dmd->min_size.height) {
			dm = dm->link.le_next;
			continue;
		}
		dx = abs(dm->nominal_size.width - size->width);
		dy = abs(dm->nominal_size.height - size->height);
		ct = dx + dy;

		if (ct < dt || save == NULL) {
			save = dm;
			dt = ct;
		}
		dm = dm->link.le_next;
	}
	return (save);
}
/* bitmap functions */
bmap_t *
alloc_bitmap(width, height, depth, flags)
	u_short width, height, depth, flags;
{
	int     i;
	u_long  total_size;
#ifdef GRF_AGA
	u_short lwpr = (flags & BMF_ALIGN64) ? ((width + 63) / 64) * 2 :
	    (width + 31) / 32;			/* AGA needs 64 bit align */
#else
	u_short lwpr = (width + 31) / 32;
#endif
	u_short wpr = lwpr << 1;
	u_short bpr = wpr << 1;
	u_short array_size = sizeof(u_char *) * depth;
	u_long  plane_size = bpr * height;
	u_short temp_size = bpr + sizeof(u_long);
	bmap_t *bm;

	/* note the next allocation will give everything, also note that all
	 * the stuff we want (including bitmaps) will be long short aligned.
	 * This is a function of the data being allocated and the fact that
	 * alloc_chipmem() returns long short aligned data. note also that
	 * each row of the bitmap is long word aligned and made of exactly n
	 * longwords. -ch */

	/* Sigh, it seems for mapping to work we need the bitplane data to 1:
	 * be aligned on a page boundry. 2: be n pages large.
	 * 
	 * why? becuase the user gets a page aligned address, if this is before
	 * your allocation, too bad.  Also it seems that the mapping routines
	 * do not watch to closely to the allowable length. so if you go over
	 * n pages by less than another page, the user gets to write all over
	 * the entire page.  Since you did not allocate up to a page boundry
	 * (or more) the user writes into someone elses memory. -ch */
	total_size = amiga_round_page(plane_size * depth) +	/* for length */
	    (temp_size) + (array_size) + sizeof(bmap_t) +
	    NBPG;		/* for alignment */
	bm = alloc_chipmem(total_size);
	if (bm) {
		if (flags & BMF_CLEAR) {
			bzero(bm, total_size);
		}
		bm->bytes_per_row = bpr;
		bm->rows = height;
		bm->depth = depth;
		bm->flags = flags;
		bm->plane = (u_char **) & bm[1];
		bm->blit_temp = ((u_char *) bm->plane) + array_size;
		bm->plane[0] = (u_char *) amiga_round_page((u_long) (bm->blit_temp + temp_size));
		if (flags & BMF_INTERLEAVED) {
			bm->row_mod = bm->bytes_per_row * (depth - 1);
			for (i = 1; i < depth; i++) {
				bm->plane[i] = bm->plane[i - 1] + bpr;
			}
		} else {
			bm->row_mod = 0;
			for (i = 1; i < depth; i++) {
				bm->plane[i] = bm->plane[i - 1] + plane_size;
			}
		}
		bm->hardware_address = PREP_DMA_MEM(bm->plane[0]);
		return (bm);
	}
	return (NULL);
}


void
free_bitmap(bm)
	bmap_t *bm;
{
	if (bm) 
		free_chipmem(bm);
}
/* load a new mode into the current display, if NULL shut display off. */
void
cc_load_mode(d)
	dmode_t *d;
{
	if (d) {
		m_this_data->current_mode = d;
		return;
	}
	/* turn off display */
	m_this_data->current_mode = NULL;
	wait_tof();
	wait_tof();
	custom.cop1lc = PREP_DMA_MEM(null_mode_copper_list);
}
/*
 * CC Mode Stuff.
 */

dmode_t *(*mode_init_funcs[]) (void) = {
#if defined (GRF_NTSC)
#if defined (GRF_A2024)
	cc_init_ntsc_a2024,
	cc_init_ntsc_hires_dlace,
#endif /* GRF_A2024 */
	cc_init_ntsc_hires_lace,
	cc_init_ntsc_hires,
#if defined (GRF_AGA)
	cc_init_ntsc_aga,
#endif /* GRF_AGA */
#endif /* GRF_NTSC */
#if defined (GRF_PAL)
#if defined (GRF_A2024)
	cc_init_pal_a2024,
	cc_init_pal_hires_dlace,
#endif /* GRF_A2024 */
	cc_init_pal_hires_lace,
	cc_init_pal_hires,
#if defined (GRF_AGA)
	cc_init_pal_aga,
#endif /* GRF_AGA */
#endif /* GRF_PAL */
	NULL
};

int
cc_init_modes()
{
	int     i = 0;
	int     error = 0;
	while (mode_init_funcs[i]) {
		mode_init_funcs[i] ();
		i++;
	}
	return (error);
}

monitor_t *
cc_get_monitor(d)
	dmode_t *d;
{
	return (DMDATA(d)->monitor);
}

view_t *
cc_get_current_view(d)
	dmode_t *d;
{
	return (DMDATA(d)->current_view);
}


view_t *
cc_alloc_view(mode, dim, depth)
	dmode_t *mode;
	dimen_t *dim;
	u_char   depth;
{
	view_t *v = alloc_chipmem(sizeof(*v) + sizeof(vdata_t));
	if (v) {
		bmap_t *bm = cc_monitor->alloc_bitmap(dim->width, dim->height,
		    depth, BMF_CLEAR | (DMDATA(mode)->max_depth == 8 ? BMF_ALIGN64 : 0));
		if (bm) {
			int     i;
			box_t   box;

			v->data = &v[1];	/* at the end of view */
			VDATA(v)->colormap = DMDATA(mode)->alloc_colormap(depth);
			if (VDATA(v)->colormap) {
				INIT_BOX(&box, 0, 0, dim->width, dim->height);
				cc_init_view(v, bm, mode, &box);
				return (v);
			}
			cc_monitor->free_bitmap(bm);
		}
		free_chipmem(v);
	}
	return (NULL);
}

colormap_t *
cc_alloc_colormap(depth)
	int depth;
{
	u_long  size = 1U << depth, i;
	colormap_t *cm = alloc_chipmem(sizeof(u_long) * size + sizeof(*cm));

	if (cm) {
		cm->type = CM_COLOR;
		cm->red_mask = 0x0F;
		cm->green_mask = 0x0F;
		cm->blue_mask = 0x0F;
		cm->first = 0;
		cm->size = size;
		cm->entry = (u_long *) & cm[1];	/* table directly after. */
		for (i = 0; i < size; i++) {
			cm->entry[i] = CM_WTOL(cc_default_colors[i&31]);
		}
		return (cm);
	}
	return (NULL);
}

#ifdef GRF_AGA
colormap_t *
cc_alloc_aga_colormap(depth)
	int depth;
{
	u_long  size = 1U << depth, i;
	colormap_t *cm = alloc_chipmem(sizeof(u_long) * size + sizeof(*cm));

	if (cm) {
		cm->type = CM_COLOR;
		cm->red_mask = 0x0FF;
		cm->green_mask = 0x0FF;
		cm->blue_mask = 0x0FF;
		cm->first = 0;
		cm->size = size;
		cm->entry = (u_long *) & cm[1];	/* table directly after. */
		for (i = 0; i < size; i++) {
			cm->entry[i] = CM_WTOL(cc_default_colors[i&31]) |
			    (CM_WTOL(cc_default_colors[i&31]) << 4);
		}
		return (cm);
	}
	return (NULL);
}
#endif

int
cc_colormap_checkvals(vcm, cm, use)
	colormap_t *vcm, *cm;
	int         use;
{
	if (use) {
		/* check to see if its the view's colormap, if so just do
		 * update. */
		if (vcm != cm) {
			if (cm->first >= vcm->size || (cm->first + cm->size) > (cm->first + vcm->size) ||
			    cm->type != vcm->type) {
				return (0);
			}
			switch (vcm->type) {
			case CM_COLOR:
				if (cm->red_mask != vcm->red_mask ||
				    cm->green_mask != vcm->green_mask ||
				    cm->blue_mask != vcm->blue_mask) {
					return (0);
				}
				break;
			case CM_GREYSCALE:
				if (cm->grey_mask != vcm->grey_mask) {
					return (0);
				}
				break;
			}
		}
	} else {
		if (cm->first >= vcm->size || (cm->first + cm->size) > (cm->first + vcm->size)) {
			return (0);
		}
	}
	return (1);
}
/* does sanity check on values */
int
cc_get_colormap(v, cm)
	view_t *v;
	colormap_t *cm;
{
	colormap_t *vcm = VDATA(v)->colormap;
	int     i;

	if (!cc_colormap_checkvals(vcm, cm, 0)) {
		return (EINVAL);
	}
	cm->type = vcm->type;

	switch (vcm->type) {
	case CM_COLOR:
		cm->red_mask = vcm->red_mask;
		cm->green_mask = vcm->green_mask;
		cm->blue_mask = vcm->blue_mask;
		break;
	case CM_GREYSCALE:
		cm->grey_mask = vcm->grey_mask;
		break;
	}

	/* copy entries into colormap. */
	for (i = cm->first; i < (cm->first + cm->size); i++) {
		cm->entry[i] = vcm->entry[i];
	}
	return (0);
}

/* does sanity check on values */
int
cc_use_colormap(v, cm)
	view_t *v;
	colormap_t *cm;
{
	colormap_t *vcm = VDATA(v)->colormap;
	int     s, i;

	if (!cc_colormap_checkvals(vcm, cm, 1)) {
		return (EINVAL);
	}
	/* check to see if its the view's colormap, if so just do update. */
	if (vcm != cm) {
		/* copy entries into colormap. */
		for (i = cm->first; i < (cm->first + cm->size); i++) {
			vcm->entry[i] = cm->entry[i];
		}
	}
	s = spltty();

	/* is view currently being displayed? */
	if (VDATA(v)->flags & VF_DISPLAY) {
		/* yes, update the copper lists */
		cop_t  *tmp, *cp;
		int     nframes = 1, j;

		if (DMDATA(VDATA(v)->mode)->flags & DMF_INTERLACE) {
			nframes = 2;
		}
		for (i = 0; i < nframes; i++) {
			cp = DMDATA(VDATA(v)->mode)->frames[i];

			tmp = find_copper_inst(cp, CI_MOVE(R_COLOR07));
			tmp -= 7;

			for (j = 0; j < 32; j++) {
				CMOVE(tmp, R_COLOR00 + (j << 1), CM_LTOW(vcm->entry[j]));
			}
		}
	}
	splx(s);
	return (0);
}

#ifdef GRF_AGA
/* does sanity check on values */
int
cc_use_aga_colormap(v, cm)
	view_t *v;
	colormap_t *cm;
{
	colormap_t *vcm = VDATA(v)->colormap;
	int     s, i;

	if (!cc_colormap_checkvals(vcm, cm, 1)) {
		return (EINVAL);
	}
	/* check to see if its the view's colormap, if so just do update. */
	if (vcm != cm) {
		/* copy entries into colormap. */
		for (i = cm->first; i < (cm->first + cm->size); i++) {
			vcm->entry[i] = cm->entry[i];
		}
	}
	s = spltty();

	/* is view currently being displayed? */
	if (VDATA(v)->flags & VF_DISPLAY) {
		/* yes, update the copper lists */
		cop_t  *tmp, *cp;
		int     nframes = 1, j;

		if (DMDATA(VDATA(v)->mode)->flags & DMF_INTERLACE) {
			nframes = 2;
		}
		for (i = 0; i < nframes; i++) {
			cp = DMDATA(VDATA(v)->mode)->frames[i];

			tmp = find_copper_inst(cp, CI_MOVE(R_COLOR00));
			for (j = 0; j < vcm->size; j += 32) {
				int k;

				for (k = 0; k < 32; k++) {
					int ce = vcm->entry[j + k] >> 4;
					CMOVE(tmp, R_COLOR00 + (k << 1), CM_LTOW(ce));
				}
				tmp++;
				for (k = 0; k < 32; k++) {
					int ce =vcm->entry[j + k];
					CMOVE(tmp, R_COLOR00 + (k << 1), CM_LTOW(ce));
				}
				tmp++;
			}
		}
	}
	splx(s);
	return (0);
}
#endif

#if defined (GRF_A2024)
colormap_t *
cc_a2024_alloc_colormap(depth)
	int depth;
{
	u_long  size = 1U << depth, i;
	colormap_t *cm = alloc_chipmem(sizeof(u_long) * size + sizeof(*cm));

	if (cm) {
		cm->type = CM_GREYSCALE;
		cm->grey_mask = 0x03;
		cm->first = 0;
		cm->size = size;
		cm->entry = (u_long *) & cm[1];	/* table directly after. */
		for (i = 0; i < size; i++) {
			cm->entry[i] = CM_WTOL(cc_a2024_default_colors[i]);
		}
		return (cm);
	}
	return (NULL);
}

int
cc_a2024_get_colormap(v, cm)
	view_t *v;
	colormap_t *cm;
{
	/* there are no differences (yet) in the way the cm's are stored */
	return (cc_get_colormap(v, cm));
}

int
cc_a2024_use_colormap(v, cm)
	view_t *v;
	colormap_t *cm;
{
	colormap_t *vcm = VDATA(v)->colormap;
	int     s, i;

	if (!cc_colormap_checkvals(vcm, cm, 1)) {
		return (EINVAL);
	}
	/* check to see if its the view's colormap, if so just do update. */
	if (vcm != cm) {
		/* copy entries into colormap. */
		for (i = cm->first; i < (cm->first + cm->size); i++) {
			vcm->entry[i] = cm->entry[i];
		}
	}
	s = spltty();

	/* is view currently being displayed? */
	if (VDATA(v)->flags & VF_DISPLAY) {
		/* yes, update the copper lists */
		cop_t  *tmp, *cp;
		int     nframes = 2, nregs = cm->size == 4 ? 16 : 8, j;

		if (DMDATA(VDATA(v)->mode)->flags & DMF_HEDLEY_EXP) {
			nframes = 4;
		}
		for (i = 0; i < nframes; i++) {
			cp = DMDATA(VDATA(v)->mode)->frames[i];

			tmp = find_copper_inst(cp, CI_MOVE(R_COLOR07));
			tmp -= 7;

			for (j = 0; j < nregs; j++) {
				CMOVE(tmp, R_COLOR00 + (j << 1), A2024_CM_TO_CR(vcm, j));
			}
		}
	}
	splx(s);
	return (0);
}
#endif /* GRF_A2024 */


/*
 * CC View stuff.
 */

void
cc_init_view(v, bm, mode, dbox)
	view_t *v;
	bmap_t *bm;
	dmode_t *mode;
	box_t *dbox;
{
	vdata_t *vd = VDATA(v);
	v->bitmap = bm;
	vd->mode = mode;
	bcopy(dbox, &v->display, sizeof(box_t));

	v->display_view = DMDATA(vd->mode)->display_view;
	v->use_colormap = DMDATA(vd->mode)->use_colormap;
	v->get_colormap = DMDATA(vd->mode)->get_colormap;
	v->free_view = cc_free_view;
	v->get_display_mode = cc_get_display_mode;
	v->remove_view = cc_remove_view;
}

void
cc_free_view(v)
	view_t *v;
{
	if (v) {
		vdata_t *vd = VDATA(v);
		dmode_t *md = vd->mode;
		v->remove_view(v);
		free_chipmem(VDATA(v)->colormap);
		cc_monitor->free_bitmap(v->bitmap);
		free_chipmem(v);
	}
}

void
cc_remove_view(v)
	view_t *v;
{
	dmode_t *mode = VDATA(v)->mode;

	if (MDATA(cc_monitor)->current_mode == mode) {
		if (DMDATA(mode)->current_view == v) {
			cc_load_mode(NULL);
		}
	}
	if (DMDATA(mode)->current_view == v) {
		DMDATA(mode)->current_view = NULL;
	}
	VDATA(v)->flags &= ~VF_DISPLAY;
}

dmode_t *
cc_get_display_mode(v)
	view_t *v;
{
	return (VDATA(v)->mode);
}

void
cc_mode_vbl_handler(d)
	dmode_t *d;
{
	u_short vp = ((custom.vposr & 0x0007) << 8) | ((custom.vhposr) >> 8);

	if (vp < 12) {
		custom.cop1lc = PREP_DMA_MEM(DMDATA(d)->frames[F_LONG]);
		custom.copjmp1 = 0;
	}
}

void
cc_lace_mode_vbl_handler(d)
	dmode_t *d;
{
	u_short vp = ((custom.vposr & 0x0007) << 8) | ((custom.vhposr) >> 8);

	if (vp < 12) {
		if (custom.vposr & 0x8000) {
			custom.cop1lc = PREP_DMA_MEM(DMDATA(d)->frames[F_LACE_LONG]);
		} else {
			custom.cop1lc = PREP_DMA_MEM(DMDATA(d)->frames[F_LACE_SHORT]);
		}
		custom.copjmp1 = 0;
	}
}

/*
 * Modes. (ick)
 */

/*
 * NTSC Modes
 */

#if defined (GRF_NTSC)

dmode_t *
cc_init_ntsc_hires()
{
	/* this function should only be called once. */
	if (!h_this) {
		u_short len = std_copper_list_len;
		cop_t  *cp;

		h_this = &hires_mode;
		h_this_data = &hires_mode_data;
		bzero(h_this, sizeof(dmode_t));
		bzero(h_this_data, sizeof(dmdata_t));

		h_this->name = "ntsc: hires";
		h_this->nominal_size.width = 640;
		h_this->nominal_size.height = 200;
		h_this_data->max_size.width = 724;
		h_this_data->max_size.height = 242;
		h_this_data->min_size.width = 320;
		h_this_data->min_size.height = 100;
		h_this_data->min_depth = 1;
		h_this_data->max_depth = 4;
		h_this->data = h_this_data;

		h_this->get_monitor = cc_get_monitor;
		h_this->alloc_view = cc_alloc_view;
		h_this->get_current_view = cc_get_current_view;

		h_this_data->use_colormap = cc_use_colormap;
		h_this_data->get_colormap = cc_get_colormap;
		h_this_data->alloc_colormap = cc_alloc_colormap;
		h_this_data->display_view = display_hires_view;
		h_this_data->monitor = cc_monitor;

		h_this_data->frames = hires_frames;
		h_this_data->frames[F_LONG] = alloc_chipmem(std_copper_list_size * F_TOTAL);
		if (!h_this_data->frames[F_LONG]) {
			panic("couldn't get chipmem for copper list");
		}
		h_this_data->frames[F_STORE_LONG] = &h_this_data->frames[F_LONG][len];

		bcopy(std_copper_list, h_this_data->frames[F_STORE_LONG], std_copper_list_size);
		bcopy(std_copper_list, h_this_data->frames[F_LONG], std_copper_list_size);

		h_this_data->bplcon0 = 0x8200 | USE_CON3;	/* hires, color
								 * composite enable */
		h_this_data->std_start_x = STANDARD_VIEW_X;
		h_this_data->std_start_y = STANDARD_VIEW_Y;
		h_this_data->vbl_handler = (vbl_handler_func *) cc_mode_vbl_handler;
#if defined (GRF_ECS) || defined (GRF_AGA)
		h_this_data->beamcon0 = STANDARD_NTSC_BEAMCON;
#endif

		LIST_INSERT_HEAD(&MDATA(cc_monitor)->modes, h_this, link);
	}
	return (h_this);
}

void
display_hires_view(v)
	view_t *v;
{
	if (h_this_data->current_view != v) {
		vdata_t *vd = VDATA(v);
		monitor_t *monitor = h_this_data->monitor;
		cop_t  *cp = h_this_data->frames[F_STORE_LONG], *tmp;
		int     depth = v->bitmap->depth, i;
		int     hstart, hstop, vstart, vstop, j;
		int     x, y, w = v->display.width, h = v->display.height;
		u_short ddfstart, ddfwidth, con1;

		/* round down to nearest even width */
		/* w &= 0xfffe; */
		/* calculate datafetch width. */

		ddfwidth = ((v->bitmap->bytes_per_row >> 1) - 2) << 2;

		/* This will center the any overscanned display */
		/* and allow user to modify. */
		x = v->display.x + h_this_data->std_start_x - ((w - 640) >> 2);
		y = v->display.y + h_this_data->std_start_y - ((h - 200) >> 1);

		if (y & 1)
			y--;

		if (!(x & 1))
			x--;

		hstart = x;
		hstop = x + (w >> 1);
		vstart = y;
		vstop = y + h;
		ddfstart = (hstart - 9) >> 1;

		/* check for hardware limits, AGA may allow more..? */
		/* anyone got a 4000 I can borrow :^) -ch */
		if ((ddfstart & 0xfffc) + ddfwidth > 0xd8) {
			int     d = 0;

			/* XXX anyone know the equality properties of
			 * intermixed logial AND's */
			/* XXX and arithmetic operators? */
			while (((ddfstart & 0xfffc) + ddfwidth - d) > 0xd8) {
				d++;
			}

			ddfstart -= d;
			hstart -= d << 1;
			hstop -= d << 1;
		}
		/* correct the datafetch to proper limits. */
		/* delay the actual display of the data until we need it. */
		ddfstart &= 0xfffc;
		con1 = ((hstart - 9) - (ddfstart << 1)) | (((hstart - 9) - (ddfstart << 1)) << 4);

		if (h_this_data->current_view) {
			VDATA(h_this_data->current_view)->flags &= ~VF_DISPLAY;	/* mark as no longer */
			/* displayed. */
		}
		h_this_data->current_view = v;

		cp = h_this_data->frames[F_STORE_LONG];
#if defined (GRF_ECS) || defined (GRF_AGA)
		tmp = find_copper_inst(cp, CI_MOVE(R_BPLCON3));
		tmp->cp.inst.operand = 0x0020;
#if defined GRF_AGA
		tmp = find_copper_inst(cp, CI_MOVE(R_FMODE));
		tmp->cp.inst.operand = 0;
#endif
		tmp = find_copper_inst(cp, CI_MOVE(R_BEAMCON0));
		tmp->cp.inst.operand = h_this_data->beamcon0;
		tmp = find_copper_inst(cp, CI_MOVE(R_DIWHIGH));
		tmp->cp.inst.operand = CALC_DIWHIGH(hstart, vstart, hstop, vstop);
#endif /* ECS */
		tmp = find_copper_inst(cp, CI_MOVE(R_BPLCON0));
		tmp->cp.inst.operand = h_this_data->bplcon0 | ((depth & 0x7) << 12);
		tmp = find_copper_inst(cp, CI_MOVE(R_BPLCON1));
		tmp->cp.inst.operand = con1;
		tmp = find_copper_inst(cp, CI_MOVE(R_DIWSTART));
		tmp->cp.inst.operand = ((vstart & 0xff) << 8) | (hstart & 0xff);
		tmp = find_copper_inst(cp, CI_MOVE(R_DIWSTOP));
		tmp->cp.inst.operand = ((vstop & 0xff) << 8) | (hstop & 0xff);
		tmp = find_copper_inst(cp, CI_MOVE(R_DDFSTART));
		tmp->cp.inst.operand = ddfstart;
		tmp = find_copper_inst(cp, CI_MOVE(R_DDFSTOP));
		tmp->cp.inst.operand = ddfstart + ddfwidth;

		tmp = find_copper_inst(cp, CI_MOVE(R_BPL0PTH));
		for (i = 0, j = 0; i < depth; j += 2, i++) {
			/* update the plane pointers */
			tmp[j].cp.inst.operand = HIADDR(PREP_DMA_MEM(v->bitmap->plane[i]));
			tmp[j + 1].cp.inst.operand = LOADDR(PREP_DMA_MEM(v->bitmap->plane[i]));
		}

		/* set mods correctly. */
		tmp = find_copper_inst(cp, CI_MOVE(R_BPL1MOD));
		tmp[0].cp.inst.operand = v->bitmap->row_mod;
		tmp[1].cp.inst.operand = v->bitmap->row_mod;

		/* set next pointers correctly */
		tmp = find_copper_inst(cp, CI_MOVE(R_COP1LCH));
		tmp[0].cp.inst.operand = HIADDR(PREP_DMA_MEM(h_this_data->frames[F_STORE_LONG]));
		tmp[1].cp.inst.operand = LOADDR(PREP_DMA_MEM(h_this_data->frames[F_STORE_LONG]));

		cp = h_this_data->frames[F_LONG];
		h_this_data->frames[F_LONG] = h_this_data->frames[F_STORE_LONG];
		h_this_data->frames[F_STORE_LONG] = cp;

		vd->flags |= VF_DISPLAY;

		cc_use_colormap(v, vd->colormap);
	}
	cc_load_mode(h_this);
}

dmode_t *
cc_init_ntsc_hires_lace()
{
	/* this function should only be called once. */
	if (!hl_this) {
		u_short len = std_copper_list_len;
		cop_t  *cp;

		hl_this = &hires_lace_mode;
		hl_this_data = &hires_lace_mode_data;
		bzero(hl_this, sizeof(dmode_t));
		bzero(hl_this_data, sizeof(dmdata_t));

		hl_this->name = "ntsc: hires interlace";
		hl_this->nominal_size.width = 640;
		hl_this->nominal_size.height = 400;
		hl_this_data->max_size.width = 724;
		hl_this_data->max_size.height = 482;
		hl_this_data->min_size.width = 320;
		hl_this_data->min_size.height = 200;
		hl_this_data->min_depth = 1;
		hl_this_data->max_depth = 4;
		hl_this->data = hl_this_data;

		hl_this->get_monitor = cc_get_monitor;
		hl_this->alloc_view = cc_alloc_view;
		hl_this->get_current_view = cc_get_current_view;

		hl_this_data->use_colormap = cc_use_colormap;
		hl_this_data->get_colormap = cc_get_colormap;
		hl_this_data->alloc_colormap = cc_alloc_colormap;
		hl_this_data->display_view = display_hires_lace_view;
		hl_this_data->monitor = cc_monitor;

		hl_this_data->flags |= DMF_INTERLACE;

		hl_this_data->frames = hires_lace_frames;
		hl_this_data->frames[F_LACE_LONG] = alloc_chipmem(std_copper_list_size * F_LACE_TOTAL);
		if (!hl_this_data->frames[F_LACE_LONG]) {
			panic("couldn't get chipmem for copper list");
		}
		hl_this_data->frames[F_LACE_SHORT] = &hl_this_data->frames[F_LACE_LONG][len];
		hl_this_data->frames[F_LACE_STORE_LONG] = &hl_this_data->frames[F_LACE_SHORT][len];
		hl_this_data->frames[F_LACE_STORE_SHORT] = &hl_this_data->frames[F_LACE_STORE_LONG][len];

		bcopy(std_copper_list, hl_this_data->frames[F_LACE_STORE_LONG], std_copper_list_size);
		bcopy(std_copper_list, hl_this_data->frames[F_LACE_STORE_SHORT], std_copper_list_size);
		bcopy(std_copper_list, hl_this_data->frames[F_LACE_LONG], std_copper_list_size);
		bcopy(std_copper_list, hl_this_data->frames[F_LACE_SHORT], std_copper_list_size);

		hl_this_data->bplcon0 = 0x8204 | USE_CON3;	/* hires, color
								 * composite enable,
								 * lace. */
		hl_this_data->std_start_x = STANDARD_VIEW_X;
		hl_this_data->std_start_y = STANDARD_VIEW_Y;
		hl_this_data->vbl_handler = (vbl_handler_func *) cc_lace_mode_vbl_handler;
#if defined (GRF_ECS) || defined (GRF_AGA)
		hl_this_data->beamcon0 = STANDARD_NTSC_BEAMCON;
#endif

		LIST_INSERT_HEAD(&MDATA(cc_monitor)->modes, hl_this, link);
	}
	return (hl_this);
}

void
display_hires_lace_view(v)
	view_t *v;
{
	if (hl_this_data->current_view != v) {
		vdata_t *vd = VDATA(v);
		monitor_t *monitor = hl_this_data->monitor;
		cop_t  *cp = hl_this_data->frames[F_LACE_STORE_LONG], *tmp;
		int     depth = v->bitmap->depth, i;
		int     hstart, hstop, vstart, vstop, j;
		int     x, y, w = v->display.width, h = v->display.height;
		u_short ddfstart, ddfwidth, con1;

		/* round down to nearest even width */
		/* w &= 0xfffe; */


		/* calculate datafetch width. */

		ddfwidth = ((v->bitmap->bytes_per_row >> 1) - 2) << 2;

		/* This will center the any overscanned display */
		/* and allow user to modify. */
		x = v->display.x + hl_this_data->std_start_x - ((w - 640) >> 2);
		y = v->display.y + hl_this_data->std_start_y - ((h - 400) >> 2);

		if (y & 1)
			y--;

		if (!(x & 1))
			x--;

		hstart = x;
		hstop = x + (w >> 1);
		vstart = y;
		vstop = y + (h >> 1);
		ddfstart = (hstart - 9) >> 1;

		/* check for hardware limits, AGA may allow more..? */
		/* anyone got a 4000 I can borrow :^) -ch */
		if ((ddfstart & 0xfffc) + ddfwidth > 0xd8) {
			int     d = 0;

			/* XXX anyone know the equality properties of
			 * intermixed logial AND's */
			/* XXX and arithmetic operators? */
			while (((ddfstart & 0xfffc) + ddfwidth - d) > 0xd8) {
				d++;
			}

			ddfstart -= d;
			hstart -= d << 1;
			hstop -= d << 1;
		}
		/* correct the datafetch to proper limits. */
		/* delay the actual display of the data until we need it. */
		ddfstart &= 0xfffc;
		con1 = ((hstart - 9) - (ddfstart << 1)) | (((hstart - 9) - (ddfstart << 1)) << 4);

		if (hl_this_data->current_view) {
			VDATA(hl_this_data->current_view)->flags &= ~VF_DISPLAY;	/* mark as no longer */
			/* displayed. */
		}
		hl_this_data->current_view = v;

		cp = hl_this_data->frames[F_LACE_STORE_LONG];
#if defined (GRF_ECS) || defined (GRF_AGA)
		tmp = find_copper_inst(cp, CI_MOVE(R_BPLCON3));
		tmp->cp.inst.operand = 0x0020;
#if defined GRF_AGA
		tmp = find_copper_inst(cp, CI_MOVE(R_FMODE));
		tmp->cp.inst.operand = 0;
#endif
		tmp = find_copper_inst(cp, CI_MOVE(R_BEAMCON0));
		tmp->cp.inst.operand = hl_this_data->beamcon0;
		tmp = find_copper_inst(cp, CI_MOVE(R_DIWHIGH));
		tmp->cp.inst.operand = CALC_DIWHIGH(hstart, vstart, hstop, vstop);
#endif /* ECS */
		tmp = find_copper_inst(cp, CI_MOVE(R_BPLCON0));
		tmp->cp.inst.operand = hl_this_data->bplcon0 | ((depth & 0x7) << 12);
		tmp = find_copper_inst(cp, CI_MOVE(R_BPLCON1));
		tmp->cp.inst.operand = con1;
		tmp = find_copper_inst(cp, CI_MOVE(R_DIWSTART));
		tmp->cp.inst.operand = ((vstart & 0xff) << 8) | (hstart & 0xff);
		tmp = find_copper_inst(cp, CI_MOVE(R_DIWSTOP));
		tmp->cp.inst.operand = ((vstop & 0xff) << 8) | (hstop & 0xff);
		tmp = find_copper_inst(cp, CI_MOVE(R_DDFSTART));
		tmp->cp.inst.operand = ddfstart;
		tmp = find_copper_inst(cp, CI_MOVE(R_DDFSTOP));
		tmp->cp.inst.operand = ddfstart + ddfwidth;

		tmp = find_copper_inst(cp, CI_MOVE(R_BPL0PTH));
		for (i = 0, j = 0; i < depth; j += 2, i++) {
			/* update the plane pointers */
			tmp[j].cp.inst.operand = HIADDR(PREP_DMA_MEM(v->bitmap->plane[i]));
			tmp[j + 1].cp.inst.operand = LOADDR(PREP_DMA_MEM(v->bitmap->plane[i]));
		}

		/* set mods correctly. */
		tmp = find_copper_inst(cp, CI_MOVE(R_BPL1MOD));
		tmp[0].cp.inst.operand = v->bitmap->bytes_per_row + v->bitmap->row_mod;
		tmp[1].cp.inst.operand = v->bitmap->bytes_per_row + v->bitmap->row_mod;

		/* set next pointers correctly */
		tmp = find_copper_inst(cp, CI_MOVE(R_COP1LCH));
		tmp[0].cp.inst.operand = HIADDR(PREP_DMA_MEM(hl_this_data->frames[F_LACE_STORE_SHORT]));
		tmp[1].cp.inst.operand = LOADDR(PREP_DMA_MEM(hl_this_data->frames[F_LACE_STORE_SHORT]));


		bcopy(hl_this_data->frames[F_LACE_STORE_LONG], hl_this_data->frames[F_LACE_STORE_SHORT], std_copper_list_size);

		/* these are the only ones that are different from long frame. */
		cp = hl_this_data->frames[F_LACE_STORE_SHORT];
		tmp = find_copper_inst(cp, CI_MOVE(R_BPL0PTH));
		for (i = 0, j = 0; i < depth; j += 2, i++) {
			u_short mod = v->bitmap->bytes_per_row + v->bitmap->row_mod;
			/* update plane pointers. high and low. */
			tmp[j].cp.inst.operand = HIADDR(PREP_DMA_MEM(&v->bitmap->plane[i][mod]));
			tmp[j + 1].cp.inst.operand = LOADDR(PREP_DMA_MEM(&v->bitmap->plane[i][mod]));
		}

		/* set next pointers correctly */
		tmp = find_copper_inst(cp, CI_MOVE(R_COP1LCH));
		tmp[0].cp.inst.operand = HIADDR(PREP_DMA_MEM(hl_this_data->frames[F_LACE_STORE_LONG]));
		tmp[1].cp.inst.operand = LOADDR(PREP_DMA_MEM(hl_this_data->frames[F_LACE_STORE_LONG]));


		cp = hl_this_data->frames[F_LACE_LONG];
		hl_this_data->frames[F_LACE_LONG] = hl_this_data->frames[F_LACE_STORE_LONG];
		hl_this_data->frames[F_LACE_STORE_LONG] = cp;

		cp = hl_this_data->frames[F_LACE_SHORT];
		hl_this_data->frames[F_LACE_SHORT] = hl_this_data->frames[F_LACE_STORE_SHORT];
		hl_this_data->frames[F_LACE_STORE_SHORT] = cp;

		vd->flags |= VF_DISPLAY;

		cc_use_colormap(v, vd->colormap);
	}
	cc_load_mode(hl_this);
}
#if defined (GRF_A2024)

dmode_t *
cc_init_ntsc_hires_dlace()
{
	/* this function should only be called once. */
	if (!hdl_this) {
		u_short len = std_dlace_copper_list_len;
		cop_t  *cp;

		hdl_this = &hires_dlace_mode;
		hdl_this_data = &hires_dlace_mode_data;
		bzero(hdl_this, sizeof(dmode_t));
		bzero(hdl_this_data, sizeof(dmdata_t));

		hdl_this->name = "ntsc: hires double interlace";
		hdl_this->nominal_size.width = 640;
		hdl_this->nominal_size.height = 800;
		hdl_this_data->max_size.width = 724;
		hdl_this_data->max_size.height = 800;
		hdl_this_data->min_size.width = 320;
		hdl_this_data->min_size.height = 400;
		hdl_this_data->min_depth = 1;
		hdl_this_data->max_depth = 2;
		hdl_this->data = hdl_this_data;

		hdl_this->get_monitor = cc_get_monitor;
		hdl_this->alloc_view = cc_alloc_view;
		hdl_this->get_current_view = cc_get_current_view;

		hdl_this_data->use_colormap = cc_a2024_use_colormap;
		hdl_this_data->get_colormap = cc_a2024_get_colormap;
		hdl_this_data->alloc_colormap = cc_a2024_alloc_colormap;
		hdl_this_data->display_view = display_hires_dlace_view;
		hdl_this_data->monitor = cc_monitor;

		hdl_this_data->flags |= DMF_INTERLACE;

		hdl_this_data->frames = hires_dlace_frames;
		hdl_this_data->frames[F_LACE_LONG] = alloc_chipmem(std_dlace_copper_list_size * F_LACE_TOTAL);
		if (!hdl_this_data->frames[F_LACE_LONG]) {
			panic("couldn't get chipmem for copper list");
		}
		hdl_this_data->frames[F_LACE_SHORT] = &hdl_this_data->frames[F_LACE_LONG][len];
		hdl_this_data->frames[F_LACE_STORE_LONG] = &hdl_this_data->frames[F_LACE_SHORT][len];
		hdl_this_data->frames[F_LACE_STORE_SHORT] = &hdl_this_data->frames[F_LACE_STORE_LONG][len];

		bcopy(std_dlace_copper_list, hdl_this_data->frames[F_LACE_STORE_LONG], std_dlace_copper_list_size);
		bcopy(std_dlace_copper_list, hdl_this_data->frames[F_LACE_STORE_SHORT], std_dlace_copper_list_size);
		bcopy(std_dlace_copper_list, hdl_this_data->frames[F_LACE_LONG], std_dlace_copper_list_size);
		bcopy(std_dlace_copper_list, hdl_this_data->frames[F_LACE_SHORT], std_dlace_copper_list_size);

		hdl_this_data->bplcon0 = 0x8204 | USE_CON3;	/* hires, color
								 * composite enable,
								 * dlace. */
		hdl_this_data->std_start_x = STANDARD_VIEW_X;
		hdl_this_data->std_start_y = STANDARD_VIEW_Y;
		hdl_this_data->vbl_handler = (vbl_handler_func *) cc_lace_mode_vbl_handler;
#if defined (GRF_ECS) || defined (GRF_AGA)
		hdl_this_data->beamcon0 = STANDARD_NTSC_BEAMCON;
#endif
		LIST_INSERT_HEAD(&MDATA(cc_monitor)->modes, hdl_this, link);
	}
	return (hdl_this);
}

void
display_hires_dlace_view(v)
	view_t *v;
{
	if (hdl_this_data->current_view != v) {
		vdata_t *vd = VDATA(v);
		monitor_t *monitor = hdl_this_data->monitor;
		cop_t  *cp = hdl_this_data->frames[F_LACE_STORE_LONG], *tmp;
		int     depth = v->bitmap->depth, i;
		int     hstart, hstop, vstart, vstop, j;
		int     x, y, w = v->display.width, h = v->display.height;
		u_short ddfstart, ddfwidth, con1;
		u_short mod1l, mod2l;

		/* round down to nearest even width */
		/* w &= 0xfffe; */

		/* calculate datafetch width. */

		ddfwidth = ((v->bitmap->bytes_per_row >> 1) - 2) << 2;

		/* This will center the any overscanned display */
		/* and allow user to modify. */
		x = v->display.x + hdl_this_data->std_start_x - ((w - 640) >> 2);
		y = v->display.y + hdl_this_data->std_start_y - ((h - 800) >> 3);

		if (y & 1)
			y--;

		if (!(x & 1))
			x--;

		hstart = x;
		hstop = x + (w >> 1);
		vstart = y;
		vstop = y + (h >> 2);

		ddfstart = (hstart - 9) >> 1;

		/* check for hardware limits, AGA may allow more..? */
		/* anyone got a 4000 I can borrow :^) -ch */
		if ((ddfstart & 0xfffc) + ddfwidth > 0xd8) {
			int     d = 0;

			/* XXX anyone know the equality properties of
			 * intermixed logial AND's */
			/* XXX and arithmetic operators? */
			while (((ddfstart & 0xfffc) + ddfwidth - d) > 0xd8) {
				d++;
			}

			ddfstart -= d;
			hstart -= d << 1;
			hstop -= d << 1;
		}
		/* correct the datafetch to proper limits. */
		/* delay the actual display of the data until we need it. */
		ddfstart &= 0xfffc;
		con1 = ((hstart - 9) - (ddfstart << 1)) | (((hstart - 9) - (ddfstart << 1)) << 4);

		if (hdl_this_data->current_view) {
			VDATA(hdl_this_data->current_view)->flags &= ~VF_DISPLAY;	/* mark as no longer */
			/* displayed. */
		}
		hdl_this_data->current_view = v;

		cp = hdl_this_data->frames[F_LACE_STORE_LONG];
#if defined (GRF_ECS) || defined (GRF_AGA)
		tmp = find_copper_inst(cp, CI_MOVE(R_BPLCON3));
		tmp->cp.inst.operand = 0x0020;
#if defined GRF_AGA
		tmp = find_copper_inst(cp, CI_MOVE(R_FMODE));
		tmp->cp.inst.operand = 0;
#endif
		tmp = find_copper_inst(cp, CI_MOVE(R_BEAMCON0));
		tmp->cp.inst.operand = hdl_this_data->beamcon0;
		tmp = find_copper_inst(cp, CI_MOVE(R_DIWHIGH));
		tmp->cp.inst.operand = CALC_DIWHIGH(hstart, vstart, hstop, vstop);
#endif /* ECS */
		tmp = find_copper_inst(cp, CI_MOVE(R_BPLCON0));
		tmp->cp.inst.operand = hdl_this_data->bplcon0 | ((depth & 0x7) << 13);	/* times two. */
		tmp = find_copper_inst(cp, CI_MOVE(R_BPLCON1));
		tmp->cp.inst.operand = con1;
		tmp = find_copper_inst(cp, CI_MOVE(R_DIWSTART));
		tmp->cp.inst.operand = ((vstart & 0xff) << 8) | (hstart & 0xff);
		tmp = find_copper_inst(cp, CI_MOVE(R_DIWSTOP));
		tmp->cp.inst.operand = ((vstop & 0xff) << 8) | (hstop & 0xff);
		tmp = find_copper_inst(cp, CI_MOVE(R_DDFSTART));
		tmp->cp.inst.operand = ddfstart;
		tmp = find_copper_inst(cp, CI_MOVE(R_DDFSTOP));
		tmp->cp.inst.operand = ddfstart + ddfwidth;

		mod1l = v->bitmap->bytes_per_row + v->bitmap->row_mod;
		mod2l = mod1l << 1;

		/* update plane pointers. */
		tmp = find_copper_inst(cp, CI_MOVE(R_BPL0PTH));
		tmp[0].cp.inst.operand = HIADDR(PREP_DMA_MEM(&v->bitmap->plane[0][0]));
		tmp[1].cp.inst.operand = LOADDR(PREP_DMA_MEM(&v->bitmap->plane[0][0]));
		tmp[2].cp.inst.operand = HIADDR(PREP_DMA_MEM(&v->bitmap->plane[0][mod1l]));
		tmp[3].cp.inst.operand = LOADDR(PREP_DMA_MEM(&v->bitmap->plane[0][mod1l]));
		if (depth == 2) {
			tmp[4].cp.inst.operand = HIADDR(PREP_DMA_MEM(&v->bitmap->plane[1][0]));
			tmp[5].cp.inst.operand = LOADDR(PREP_DMA_MEM(&v->bitmap->plane[1][0]));
			tmp[6].cp.inst.operand = HIADDR(PREP_DMA_MEM(&v->bitmap->plane[1][mod1l]));
			tmp[7].cp.inst.operand = LOADDR(PREP_DMA_MEM(&v->bitmap->plane[1][mod1l]));
		}
		/* set modulos. */
		tmp = find_copper_inst(cp, CI_MOVE(R_BPL1MOD));
		tmp[0].cp.inst.operand = mod2l + mod1l;
		tmp[1].cp.inst.operand = mod2l + mod1l;


		/* set next coper list pointers */
		tmp = find_copper_inst(cp, CI_MOVE(R_COP1LCH));
		tmp[0].cp.inst.operand = HIADDR(PREP_DMA_MEM(hdl_this_data->frames[F_LACE_STORE_SHORT]));
		tmp[1].cp.inst.operand = LOADDR(PREP_DMA_MEM(hdl_this_data->frames[F_LACE_STORE_SHORT]));

		bcopy(hdl_this_data->frames[F_LACE_STORE_LONG], hdl_this_data->frames[F_LACE_STORE_SHORT],
		    std_dlace_copper_list_size);

		/* these are the only ones that are different from long frame. */
		cp = hdl_this_data->frames[F_LACE_STORE_SHORT];
		/* update plane pointers. */
		tmp = find_copper_inst(cp, CI_MOVE(R_BPL0PTH));
		tmp[0].cp.inst.operand = HIADDR(PREP_DMA_MEM(&v->bitmap->plane[0][mod2l]));
		tmp[1].cp.inst.operand = LOADDR(PREP_DMA_MEM(&v->bitmap->plane[0][mod2l]));
		tmp[2].cp.inst.operand = HIADDR(PREP_DMA_MEM(&v->bitmap->plane[0][mod2l + mod1l]));
		tmp[3].cp.inst.operand = LOADDR(PREP_DMA_MEM(&v->bitmap->plane[0][mod2l + mod1l]));
		if (depth == 2) {
			tmp[4].cp.inst.operand = HIADDR(PREP_DMA_MEM(&v->bitmap->plane[1][mod2l]));
			tmp[5].cp.inst.operand = LOADDR(PREP_DMA_MEM(&v->bitmap->plane[1][mod2l]));
			tmp[6].cp.inst.operand = HIADDR(PREP_DMA_MEM(&v->bitmap->plane[1][mod2l + mod1l]));
			tmp[7].cp.inst.operand = LOADDR(PREP_DMA_MEM(&v->bitmap->plane[1][mod2l + mod1l]));
		}
		/* set next copper list pointers */
		tmp = find_copper_inst(cp, CI_MOVE(R_COP1LCH));
		tmp[0].cp.inst.operand = HIADDR(PREP_DMA_MEM(hdl_this_data->frames[F_LACE_STORE_LONG]));
		tmp[1].cp.inst.operand = LOADDR(PREP_DMA_MEM(hdl_this_data->frames[F_LACE_STORE_LONG]));

		cp = hdl_this_data->frames[F_LACE_LONG];
		hdl_this_data->frames[F_LACE_LONG] = hdl_this_data->frames[F_LACE_STORE_LONG];
		hdl_this_data->frames[F_LACE_STORE_LONG] = cp;

		cp = hdl_this_data->frames[F_LACE_SHORT];
		hdl_this_data->frames[F_LACE_SHORT] = hdl_this_data->frames[F_LACE_STORE_SHORT];
		hdl_this_data->frames[F_LACE_STORE_SHORT] = cp;

		vd->flags |= VF_DISPLAY;
		cc_a2024_use_colormap(v, vd->colormap);
	}
	cc_load_mode(hdl_this);
}


dmode_t *
cc_init_ntsc_a2024()
{
	/* this function should only be called once. */
	if (!a24_this) {
		int     i;
		u_short len = std_a2024_copper_list_len;
		cop_t  *cp;

		a24_this = &a2024_mode;
		a24_this_data = &a2024_mode_data;
		bzero(a24_this, sizeof(dmode_t));
		bzero(a24_this_data, sizeof(dmdata_t));

		a24_this->name = "ntsc: A2024 15khz";
		a24_this->nominal_size.width = 1024;
		a24_this->nominal_size.height = 800;
		a24_this_data->max_size.width = 1024;
		a24_this_data->max_size.height = 800;
		a24_this_data->min_size.width = 1024;
		a24_this_data->min_size.height = 800;
		a24_this_data->min_depth = 1;
		a24_this_data->max_depth = 2;
		a24_this->data = a24_this_data;

		a24_this->get_monitor = cc_get_monitor;
		a24_this->alloc_view = cc_alloc_view;
		a24_this->get_current_view = cc_get_current_view;

		a24_this_data->use_colormap = cc_a2024_use_colormap;
		a24_this_data->get_colormap = cc_a2024_get_colormap;
		a24_this_data->display_view = display_a2024_view;
		a24_this_data->alloc_colormap = cc_a2024_alloc_colormap;
		a24_this_data->monitor = cc_monitor;

		a24_this_data->flags |= DMF_HEDLEY_EXP;

		a24_this_data->frames = a2024_frames;
		a24_this_data->frames[F_QD_QUAD0] = alloc_chipmem(std_a2024_copper_list_size * F_QD_TOTAL);
		if (!a24_this_data->frames[F_QD_QUAD0]) {
			panic("couldn't get chipmem for copper list");
		}
		/* setup the hedley init bitplane. */
		hedley_init = alloc_chipmem(128);
		if (!hedley_init) {
			panic("couldn't get chipmem for hedley init bitplane");
		}
		for (i = 1; i < 128; i++)
			hedley_init[i] = 0xff;
		hedley_init[0] = 0x03;

		/* copy image of standard copper list. */
		bcopy(std_a2024_copper_list, a24_this_data->frames[0], std_a2024_copper_list_size);

		/* set the init plane pointer. */
		cp = find_copper_inst(a24_this_data->frames[F_QD_QUAD0], CI_MOVE(R_BPL0PTH));
		cp[0].cp.inst.operand = HIADDR(PREP_DMA_MEM(hedley_init));
		cp[1].cp.inst.operand = LOADDR(PREP_DMA_MEM(hedley_init));

		for (i = 1; i < F_QD_TOTAL; i++) {
			a24_this_data->frames[i] = &a24_this_data->frames[i - 1][len];
			bcopy(a24_this_data->frames[0], a24_this_data->frames[i], std_a2024_copper_list_size);
		}

		a24_this_data->bplcon0 = 0x8200;	/* hires */
		a24_this_data->vbl_handler = (vbl_handler_func *) a2024_mode_vbl_handler;


		LIST_INSERT_HEAD(&MDATA(cc_monitor)->modes, a24_this, link);
	}
	return (a24_this);
}

void
display_a2024_view(v)
	view_t *v;
{
	if (a24_this_data->current_view != v) {
		vdata_t *vd = VDATA(v);
		monitor_t *monitor = a24_this_data->monitor;
		cop_t  *cp, *tmp;
		u_char *inst_plane[2];
		u_char **plane = inst_plane;
		u_long  full_line = v->bitmap->bytes_per_row + v->bitmap->row_mod;
		u_long  half_plane = full_line * v->bitmap->rows / 2;

		int     line_mod = 0xbc;	/* standard 2024 15khz mod. */
		int     depth = v->bitmap->depth, i, j;

		plane[0] = v->bitmap->plane[0];
		if (depth == 2) {
			plane[1] = v->bitmap->plane[1];
		}
		if (a24_this_data->current_view) {
			VDATA(a24_this_data->current_view)->flags &= ~VF_DISPLAY;	/* mark as no longer
											 * displayed. */
		}
		cp = a24_this_data->frames[F_QD_STORE_QUAD0];
		tmp = find_copper_inst(cp, CI_MOVE(R_COLOR1F));
		tmp = find_copper_inst(tmp, CI_MOVE(R_BPLCON0));	/* grab third one. */
		tmp->cp.inst.operand = a24_this_data->bplcon0 | ((depth & 0x7) << 13);	/* times 2 */

		bcopy(a24_this_data->frames[F_QD_STORE_QUAD0], a24_this_data->frames[F_QD_STORE_QUAD1], std_a2024_copper_list_size);
		bcopy(a24_this_data->frames[F_QD_STORE_QUAD0], a24_this_data->frames[F_QD_STORE_QUAD2], std_a2024_copper_list_size);
		bcopy(a24_this_data->frames[F_QD_STORE_QUAD0], a24_this_data->frames[F_QD_STORE_QUAD3], std_a2024_copper_list_size);

		/*
		 * Mark Id's
		 */
		tmp = find_copper_inst(a24_this_data->frames[F_QD_STORE_QUAD1], CI_WAIT(126, 21));
		CBUMP(tmp);
		CMOVE(tmp, R_COLOR01, QUAD1_ID);
		tmp = find_copper_inst(a24_this_data->frames[F_QD_STORE_QUAD2], CI_WAIT(126, 21));
		CBUMP(tmp);
		CMOVE(tmp, R_COLOR01, QUAD2_ID);
		tmp = find_copper_inst(a24_this_data->frames[F_QD_STORE_QUAD3], CI_WAIT(126, 21));
		CBUMP(tmp);
		CMOVE(tmp, R_COLOR01, QUAD3_ID);

		plane[0]--;
		plane[0]--;
		if (depth == 2) {
			plane[1]--;
			plane[1]--;
		}
		/*
		 * Set bitplane pointers.
		 */
		tmp = find_copper_inst(a24_this_data->frames[F_QD_STORE_QUAD0], CI_MOVE(R_BPLMOD2));
		CBUMP(tmp);
		CMOVE(tmp, R_BPL0PTH, HIADDR(PREP_DMA_MEM(&plane[0][0])));
		CMOVE(tmp, R_BPL0PTL, LOADDR(PREP_DMA_MEM(&plane[0][0])));
		CMOVE(tmp, R_BPL1PTH, HIADDR(PREP_DMA_MEM(&plane[0][full_line])));
		CMOVE(tmp, R_BPL1PTL, LOADDR(PREP_DMA_MEM(&plane[0][full_line])));
		if (depth == 2) {
			CMOVE(tmp, R_BPL2PTH, HIADDR(PREP_DMA_MEM(&plane[1][0])));
			CMOVE(tmp, R_BPL2PTL, LOADDR(PREP_DMA_MEM(&plane[1][0])));
			CMOVE(tmp, R_BPL3PTH, HIADDR(PREP_DMA_MEM(&plane[1][full_line])));
			CMOVE(tmp, R_BPL3PTL, LOADDR(PREP_DMA_MEM(&plane[1][full_line])));
		}
#if defined (GRF_ECS) || defined (GRF_AGA)
		CMOVE(tmp, R_DIWHIGH, 0x2000);
#endif
		CMOVE(tmp, R_COP1LCH, HIADDR(PREP_DMA_MEM(a24_this_data->frames[F_QD_STORE_QUAD1])));
		CMOVE(tmp, R_COP1LCL, LOADDR(PREP_DMA_MEM(a24_this_data->frames[F_QD_STORE_QUAD1])));
		CEND(tmp);
		CEND(tmp);

		tmp = find_copper_inst(a24_this_data->frames[F_QD_STORE_QUAD1], CI_MOVE(R_BPLMOD2));
		CBUMP(tmp);
		CMOVE(tmp, R_BPL0PTH, HIADDR(PREP_DMA_MEM(&plane[0][HALF_2024_LINE])));
		CMOVE(tmp, R_BPL0PTL, LOADDR(PREP_DMA_MEM(&plane[0][HALF_2024_LINE])));
		CMOVE(tmp, R_BPL1PTH, HIADDR(PREP_DMA_MEM(&plane[0][full_line + HALF_2024_LINE])));
		CMOVE(tmp, R_BPL1PTL, LOADDR(PREP_DMA_MEM(&plane[0][full_line + HALF_2024_LINE])));
		if (depth == 2) {
			CMOVE(tmp, R_BPL2PTH, HIADDR(PREP_DMA_MEM(&plane[1][HALF_2024_LINE])));
			CMOVE(tmp, R_BPL2PTL, LOADDR(PREP_DMA_MEM(&plane[1][HALF_2024_LINE])));
			CMOVE(tmp, R_BPL3PTH, HIADDR(PREP_DMA_MEM(&plane[1][full_line + HALF_2024_LINE])));
			CMOVE(tmp, R_BPL3PTL, LOADDR(PREP_DMA_MEM(&plane[1][full_line + HALF_2024_LINE])));
		}
#if defined (GRF_ECS) || defined (GRF_AGA)
		CMOVE(tmp, R_DIWHIGH, 0x2000);
#endif
		CMOVE(tmp, R_COP1LCH, HIADDR(PREP_DMA_MEM(a24_this_data->frames[F_QD_STORE_QUAD2])));
		CMOVE(tmp, R_COP1LCL, LOADDR(PREP_DMA_MEM(a24_this_data->frames[F_QD_STORE_QUAD2])));
		CEND(tmp);
		CEND(tmp);

		tmp = find_copper_inst(a24_this_data->frames[F_QD_STORE_QUAD2], CI_MOVE(R_BPLMOD2));
		CBUMP(tmp);
		CMOVE(tmp, R_BPL0PTH, HIADDR(PREP_DMA_MEM(&plane[0][half_plane])));
		CMOVE(tmp, R_BPL0PTL, LOADDR(PREP_DMA_MEM(&plane[0][half_plane])));
		CMOVE(tmp, R_BPL1PTH, HIADDR(PREP_DMA_MEM(&plane[0][half_plane + full_line])));
		CMOVE(tmp, R_BPL1PTL, LOADDR(PREP_DMA_MEM(&plane[0][half_plane + full_line])));
		if (depth == 2) {
			CMOVE(tmp, R_BPL2PTH, HIADDR(PREP_DMA_MEM(&plane[1][half_plane])));
			CMOVE(tmp, R_BPL2PTL, LOADDR(PREP_DMA_MEM(&plane[1][half_plane])));
			CMOVE(tmp, R_BPL3PTH, HIADDR(PREP_DMA_MEM(&plane[1][half_plane + full_line])));
			CMOVE(tmp, R_BPL3PTL, LOADDR(PREP_DMA_MEM(&plane[1][half_plane + full_line])));
		}
#if defined (GRF_ECS) || defined (GRF_AGA)
		CMOVE(tmp, R_DIWHIGH, 0x2000);
#endif
		CMOVE(tmp, R_COP1LCH, HIADDR(PREP_DMA_MEM(a24_this_data->frames[F_QD_STORE_QUAD3])));
		CMOVE(tmp, R_COP1LCL, LOADDR(PREP_DMA_MEM(a24_this_data->frames[F_QD_STORE_QUAD3])));
		CEND(tmp);
		CEND(tmp);

		tmp = find_copper_inst(a24_this_data->frames[F_QD_STORE_QUAD3], CI_MOVE(R_BPLMOD2));
		CBUMP(tmp);
		CMOVE(tmp, R_BPL0PTH, HIADDR(PREP_DMA_MEM(&plane[0][half_plane + HALF_2024_LINE])));
		CMOVE(tmp, R_BPL0PTL, LOADDR(PREP_DMA_MEM(&plane[0][half_plane + HALF_2024_LINE])));
		CMOVE(tmp, R_BPL1PTH, HIADDR(PREP_DMA_MEM(&plane[0][half_plane + full_line + HALF_2024_LINE])));
		CMOVE(tmp, R_BPL1PTL, LOADDR(PREP_DMA_MEM(&plane[0][half_plane + full_line + HALF_2024_LINE])));
		if (depth == 2) {
			CMOVE(tmp, R_BPL2PTH, HIADDR(PREP_DMA_MEM(&plane[1][half_plane + HALF_2024_LINE])));
			CMOVE(tmp, R_BPL2PTL, LOADDR(PREP_DMA_MEM(&plane[1][half_plane + HALF_2024_LINE])));
			CMOVE(tmp, R_BPL3PTH, HIADDR(PREP_DMA_MEM(&plane[1][half_plane + full_line + HALF_2024_LINE])));
			CMOVE(tmp, R_BPL3PTL, LOADDR(PREP_DMA_MEM(&plane[1][half_plane + full_line + HALF_2024_LINE])));
		}
#if defined (GRF_ECS) || defined (GRF_AGA)
		CMOVE(tmp, R_DIWHIGH, 0x2000);
#endif
		CMOVE(tmp, R_COP1LCH, HIADDR(PREP_DMA_MEM(a24_this_data->frames[F_QD_STORE_QUAD0])));
		CMOVE(tmp, R_COP1LCL, LOADDR(PREP_DMA_MEM(a24_this_data->frames[F_QD_STORE_QUAD0])));
		CEND(tmp);
		CEND(tmp);

		/* swap new pointers in. */
		for (i = F_QD_STORE_QUAD0, j = F_QD_QUAD0;
		    i <= F_QD_STORE_QUAD3; i++, j++) {
			cp = a24_this_data->frames[j];
			a24_this_data->frames[j] = a24_this_data->frames[i];
			a24_this_data->frames[i] = cp;
		}

		a24_this_data->current_view = v;
		vd->flags |= VF_DISPLAY;

		cc_a2024_use_colormap(v, vd->colormap);
	}
	cc_load_mode(a24_this);
}

void
a2024_mode_vbl_handler(d)
	dmode_t *d;
{
	u_short vp = ((custom.vposr & 0x0007) << 8) | ((custom.vhposr) >> 8);

	if (vp < 12) {
		custom.cop1lc = PREP_DMA_MEM(a24_this_data->frames[a24_this_data->hedley_current]);
		custom.copjmp1 = 0;
	}
	a24_this_data->hedley_current++;
	a24_this_data->hedley_current &= 0x3;	/* if 4 then 0. */
}
#endif /* GRF_A2024 */

#if defined (GRF_AGA)

dmode_t *
cc_init_ntsc_aga()
{
	/* this function should only be called once. */
	if (!aga_this && (custom.deniseid & 0xff) == 0xf8 &&
	    aga_enable & AGA_ENABLE) {
		u_short len = aga_copper_list_len;
		cop_t  *cp;

		aga_this = &aga_mode;
		aga_this_data = &aga_mode_data;
		bzero(aga_this, sizeof(dmode_t));
		bzero(aga_this_data, sizeof(dmdata_t));

		aga_this->name = "ntsc: AGA dbl";
		aga_this->nominal_size.width = 640;
		aga_this->nominal_size.height = 400;
		aga_this_data->max_size.width = 724;
		aga_this_data->max_size.height = 482;
		aga_this_data->min_size.width = 320;
		aga_this_data->min_size.height = 200;
		aga_this_data->min_depth = 1;
		aga_this_data->max_depth = 8;
		aga_this->data = aga_this_data;

		aga_this->get_monitor = cc_get_monitor;
		aga_this->alloc_view = cc_alloc_view;
		aga_this->get_current_view = cc_get_current_view;

		aga_this_data->use_colormap = cc_use_aga_colormap;
		aga_this_data->get_colormap = cc_get_colormap;
		aga_this_data->alloc_colormap = cc_alloc_aga_colormap;
		aga_this_data->display_view = display_aga_view;
		aga_this_data->monitor = cc_monitor;

		aga_this_data->frames = aga_frames;
		aga_this_data->frames[F_LONG] = alloc_chipmem(aga_copper_list_size * F_TOTAL);
		if (!aga_this_data->frames[F_LONG]) {
			panic("couldn't get chipmem for copper list");
		}
		aga_this_data->frames[F_STORE_LONG] = &aga_this_data->frames[F_LONG][len];

		bcopy(aga_copper_list, aga_this_data->frames[F_STORE_LONG], aga_copper_list_size);
		bcopy(aga_copper_list, aga_this_data->frames[F_LONG], aga_copper_list_size);

		aga_this_data->bplcon0 = 0x0240 | USE_CON3;	/* color composite
								 * enable,
								 * shres. */
		aga_this_data->std_start_x = 0x4f /*STANDARD_VIEW_X*/;
		aga_this_data->std_start_y = 0x2b /*STANDARD_VIEW_Y*/;
		aga_this_data->vbl_handler = (vbl_handler_func *) cc_mode_vbl_handler;
		aga_this_data->beamcon0 = SPECIAL_BEAMCON ^ VSYNCTRUE;

		LIST_INSERT_HEAD(&MDATA(cc_monitor)->modes,
		    aga_this, link);
	}
	return (aga_this);
}

/* static, so I can patch and play */

int	AGA_htotal = 0x79;
int	AGA_hsstrt = 0xe;
int	AGA_hsstop = 0x1c;
int	AGA_hbstrt = 0x8;
int	AGA_hbstop = 0x1e;
int	AGA_vtotal = 0x1ec;
int	AGA_vsstrt = 0x3;
int	AGA_vsstop = 0x6;
int	AGA_vbstrt = 0x0;
int	AGA_vbstop = 0x19;
int	AGA_hcenter = 0x4a;

void
display_aga_view(v)
	view_t *v;
{
	if (aga_this_data->current_view != v) {
		vdata_t *vd = VDATA(v);
		monitor_t *monitor = aga_this_data->monitor;
		cop_t  *cp = aga_this_data->frames[F_STORE_LONG], *tmp;
		int     depth = v->bitmap->depth, i;
		int     hstart, hstop, vstart, vstop, j;
		int     x, y, w = v->display.width, h = v->display.height;
		u_short ddfstart, ddfwidth, con1;

#ifdef DEBUG
		if (aga_enable & AGA_TRACE)
			printf("display_aga_view(%dx%dx%d) %x\n", w, h,
			    depth, v);
#endif
		/* round down to nearest even width */
		/* w &= 0xfffe; */
		/* calculate datafetch width. */

		ddfwidth = ((v->bitmap->bytes_per_row >> 1) - 4) << 1;

		/* this will center the any overscanned display */
		/* and allow user to modify. */
		x = v->display.x + aga_this_data->std_start_x - ((w - 640) >> 3);
		y = v->display.y + aga_this_data->std_start_y - ((h - 400) >> 1);

		if (y & 1)
			y--;

		if (!(x & 1))
			x--;

		hstart = x;
		hstop = x + (w >> 2);
		vstart = y;
		vstop = y + (h >> 0);
		ddfstart = (hstart >> 1) - 8;

#ifdef DEBUG
		if (aga_enable & AGA_TRACE2) {
			printf ("  ddfwidth %04x x %04x y %04x", ddfwidth,
			    x, y);
			printf (" hstart %04x hstop %04x vstart %04x vstop %04x ddfstart %04x\n",
			    hstart, hstop, vstart, vstop, ddfstart);
		}
#endif
		/* check for hardware limits, AGA may allow more..? */
		/* anyone got a 4000 I can borrow :^) -ch */
		if ((ddfstart & 0xfffc) + ddfwidth > 0xd8) {
			int     d = 0;

			/* XXX anyone know the equality properties of
			 * intermixed logial AND's */
			/* XXX and arithmetic operators? */
			while (((ddfstart & 0xfffc) + ddfwidth - d) > 0xd8) {
				d++;
			}

			ddfstart -= d;
			hstart -= d << 1;
			hstop -= d << 1;
		}
		/* correct the datafetch to proper limits. */
		/* delay the actual display of the data until we need it. */
		ddfstart &= 0xfffc;
#ifdef DEBUG
		if (aga_enable & AGA_TRACE2) {
			printf ("  ddfwidth %04x x %04x y %04x", ddfwidth,
			    x, y);
			printf (" hstart %04x hstop %04x vstart %04x vstop %04x ddfstart %04x\n",
			    hstart, hstop, vstart, vstop, ddfstart);
		}
#endif
		con1 = ((hstart - 9) - (ddfstart << 1)) | (((hstart - 9) - (ddfstart << 1)) << 4);

		if (aga_this_data->current_view) {
			VDATA(aga_this_data->current_view)->flags &= ~VF_DISPLAY;	/* mark as no longer */
			/* displayed. */
		}
		aga_this_data->current_view = v;

		cp = aga_this_data->frames[F_STORE_LONG];
		tmp = cp;
		for (i = 0; i < 8; ++i) {
			if (tmp == NULL)
				break;
			tmp = find_copper_inst(tmp + 1, CI_MOVE(R_BPLCON3));
			if (tmp == NULL)
				break;
			tmp->cp.inst.operand = 0x0ca1 | (i << 13);
			tmp = find_copper_inst(tmp + 1, CI_MOVE(R_BPLCON3));
			if (tmp == NULL)
				break;
			tmp->cp.inst.operand = 0x0ea1 | (i << 13);
		}
		if (tmp)
			tmp = find_copper_inst(tmp + 1, CI_MOVE(R_BPLCON3));
		if (tmp)
			tmp->cp.inst.operand = 0x0ca1;
		tmp = find_copper_inst(cp, CI_MOVE(R_FMODE));
		tmp->cp.inst.operand = 0x8003;
		tmp = find_copper_inst(cp, CI_MOVE(R_HTOTAL));
		tmp->cp.inst.operand = AGA_htotal; /* 81/71/73/79? */
		tmp = find_copper_inst(cp, CI_MOVE(R_HBSTRT));
		tmp->cp.inst.operand = AGA_hbstrt; /* 0x0008 */
		tmp = find_copper_inst(cp, CI_MOVE(R_HSSTRT));
		tmp->cp.inst.operand = AGA_hsstrt; /* 0x000e */
		tmp = find_copper_inst(cp, CI_MOVE(R_HSSTOP));
		tmp->cp.inst.operand = AGA_hsstop; /* 0x001c */
		tmp = find_copper_inst(cp, CI_MOVE(R_HBSTOP));
		tmp->cp.inst.operand = AGA_hsstop; /* 0x001e */
		tmp = find_copper_inst(cp, CI_MOVE(R_HCENTER));
		tmp->cp.inst.operand = AGA_hcenter; /*AGA_htotal / 2 + AGA_hsstrt */
		tmp = find_copper_inst(cp, CI_MOVE(R_VBSTRT));
		tmp->cp.inst.operand = AGA_vbstrt; /* 0x0000 */
		tmp = find_copper_inst(cp, CI_MOVE(R_VSSTRT));
		tmp->cp.inst.operand = AGA_vsstrt; /* 0x016b / AGA_htotal */
		tmp = find_copper_inst(cp, CI_MOVE(R_VSSTOP));
		tmp->cp.inst.operand = AGA_vsstop; /* 0x02d6 / AGA_htotal */
		tmp = find_copper_inst(cp, CI_MOVE(R_VBSTOP));
		tmp->cp.inst.operand = AGA_vbstop; /* 0x0bd1 / AGA_htotal */
		tmp = find_copper_inst(cp, CI_MOVE(R_VTOTAL));
		tmp->cp.inst.operand = AGA_vtotal;
		tmp = find_copper_inst(cp, CI_MOVE(R_BEAMCON0));
		tmp->cp.inst.operand = aga_this_data->beamcon0;
#ifdef DEBUG
		if (aga_enable & AGA_TRACE2)
			printf("  beamcon0 %04x", tmp->cp.inst.operand);
#endif
		tmp = find_copper_inst(cp, CI_MOVE(R_DIWHIGH));
		tmp->cp.inst.operand = CALC_DIWHIGH(hstart, vstart, hstop, vstop);
#ifdef DEBUG
		if (aga_enable & AGA_TRACE2)
			printf(" diwhigh %04x>", tmp->cp.inst.operand);
#endif
#if 0
		tmp->cp.inst.operand = (vstop & 0x0700) | ((hstop & 0x0100) << 5);
#endif
#ifdef DEBUG
		if (aga_enable & AGA_TRACE2)
			printf("%04x", tmp->cp.inst.operand);
#endif
		tmp = find_copper_inst(cp, CI_MOVE(R_BPLCON0));
		tmp->cp.inst.operand = aga_this_data->bplcon0 |
		    ((depth & 0x7) << 12) | ((depth & 0x8) << 1);
#ifdef DEBUG
		if (aga_enable & AGA_TRACE2)
			printf(" bplcon0 %04x", tmp->cp.inst.operand);
#endif
		tmp = find_copper_inst(cp, CI_MOVE(R_BPLCON1));
		tmp->cp.inst.operand = con1;
#ifdef DEBUG
		if (aga_enable & AGA_TRACE2)
			printf(" bplcon1 %04x>0000\n", con1);
#endif
		tmp = find_copper_inst(cp, CI_MOVE(R_DIWSTART));
		tmp->cp.inst.operand = ((vstart & 0xff) << 8) | (hstart & 0xff);
#ifdef DEBUG
		if (aga_enable & AGA_TRACE2)
			printf("  diwstart %04x", tmp->cp.inst.operand);
#endif
		tmp = find_copper_inst(cp, CI_MOVE(R_DIWSTOP));
		tmp->cp.inst.operand = ((vstop & 0xff) << 8) | (hstop & 0xff);
#ifdef DEBUG
		if (aga_enable & AGA_TRACE2)
			printf(" diwstop %04x", tmp->cp.inst.operand);
#endif
		tmp = find_copper_inst(cp, CI_MOVE(R_DDFSTART));
		tmp->cp.inst.operand = ddfstart;
#ifdef DEBUG
		if (aga_enable & AGA_TRACE2)
			printf(" ddfstart %04x", tmp->cp.inst.operand);
#endif
		tmp = find_copper_inst(cp, CI_MOVE(R_DDFSTOP));
		tmp->cp.inst.operand = ddfstart + ddfwidth;
#ifdef DEBUG
		if (aga_enable & AGA_TRACE2)
			printf(" ddfstop %04x", tmp->cp.inst.operand);
#endif

		tmp = find_copper_inst(cp, CI_MOVE(R_BPL0PTH));
		for (i = 0, j = 0; i < depth; j += 2, i++) {
			/* update the plane pointers */
			tmp[j].cp.inst.operand = HIADDR(PREP_DMA_MEM(v->bitmap->plane[i]));
			tmp[j + 1].cp.inst.operand = LOADDR(PREP_DMA_MEM(v->bitmap->plane[i]));
#ifdef DEBUG
		if (aga_enable & AGA_TRACE2)
			printf (" bpl%dpth %08x", i, v->bitmap->plane[i]);
#endif
		}

		/* set mods correctly. */
		tmp = find_copper_inst(cp, CI_MOVE(R_BPL1MOD));
		tmp[0].cp.inst.operand = v->bitmap->row_mod;
		tmp[1].cp.inst.operand = v->bitmap->row_mod;
#ifdef DEBUG
		if (aga_enable & AGA_TRACE2)
			printf(" bplxmod %04x\n", v->bitmap->row_mod);
#endif

		/* set next pointers correctly */
		tmp = find_copper_inst(cp, CI_MOVE(R_COP1LCH));
		tmp[0].cp.inst.operand = HIADDR(PREP_DMA_MEM(aga_this_data->frames[F_STORE_LONG]));
		tmp[1].cp.inst.operand = LOADDR(PREP_DMA_MEM(aga_this_data->frames[F_STORE_LONG]));

		cp = aga_this_data->frames[F_LONG];
		aga_this_data->frames[F_LONG] = aga_this_data->frames[F_STORE_LONG];
		aga_this_data->frames[F_STORE_LONG] = cp;

		vd->flags |= VF_DISPLAY;

		cc_use_aga_colormap(v, vd->colormap);
	}
	cc_load_mode(aga_this);
#ifdef DEBUG
	if (aga_enable & AGA_TRACE)
		aga_enable |= AGA_TRACE2;	/* XXXX */
#endif
}

#endif /* GRF_AGA */
#endif /* GRF_NTSC */

/*
 * PAL modes.
 */

#if defined (GRF_PAL)

dmode_t *
cc_init_pal_hires()
{
	/* this function should only be called once. */
	if (!ph_this) {
		u_short len = std_copper_list_len;
		cop_t  *cp;

		ph_this = &pal_hires_mode;
		ph_this_data = &pal_hires_mode_data;
		bzero(ph_this, sizeof(dmode_t));
		bzero(ph_this_data, sizeof(dmdata_t));

		ph_this->name = "pal: hires";
		ph_this->nominal_size.width = 640;
		ph_this->nominal_size.height = 256;
		ph_this_data->max_size.width = 724;
		ph_this_data->max_size.height = 289;
		ph_this_data->min_size.width = 320;
		ph_this_data->min_size.height = 244;
		ph_this_data->min_depth = 1;
		ph_this_data->max_depth = 4;
		ph_this->data = ph_this_data;

		ph_this->get_monitor = cc_get_monitor;
		ph_this->alloc_view = cc_alloc_view;
		ph_this->get_current_view = cc_get_current_view;

		ph_this_data->use_colormap = cc_use_colormap;
		ph_this_data->get_colormap = cc_get_colormap;
		ph_this_data->alloc_colormap = cc_alloc_colormap;
		ph_this_data->display_view = display_pal_hires_view;
		ph_this_data->monitor = cc_monitor;

		ph_this_data->frames = pal_hires_frames;
		ph_this_data->frames[F_LONG] = alloc_chipmem(std_copper_list_size * F_TOTAL);
		if (!ph_this_data->frames[F_LONG]) {
			panic("couldn't get chipmem for copper list");
		}
		ph_this_data->frames[F_STORE_LONG] = &ph_this_data->frames[F_LONG][len];

		bcopy(std_copper_list, ph_this_data->frames[F_STORE_LONG], std_copper_list_size);
		bcopy(std_copper_list, ph_this_data->frames[F_LONG], std_copper_list_size);

		ph_this_data->bplcon0 = 0x8200 | USE_CON3;	/* pal_hires, color
								 * composite enable,
								 * lace. */
		ph_this_data->std_start_x = STANDARD_VIEW_X;
		ph_this_data->std_start_y = STANDARD_VIEW_Y;
		ph_this_data->vbl_handler = (vbl_handler_func *) cc_mode_vbl_handler;
#if defined (GRF_ECS) || defined (GRF_AGA)
		ph_this_data->beamcon0 = STANDARD_PAL_BEAMCON;
#endif

		LIST_INSERT_HEAD(&MDATA(cc_monitor)->modes, ph_this, link);
	}
	return (ph_this);
}

void
display_pal_hires_view(v)
	view_t *v;
{
	if (ph_this_data->current_view != v) {
		vdata_t *vd = VDATA(v);
		monitor_t *monitor = ph_this_data->monitor;
		cop_t  *cp = ph_this_data->frames[F_STORE_LONG], *tmp;
		int     depth = v->bitmap->depth, i;
		int     hstart, hstop, vstart, vstop, j;
		int     x, y, w = v->display.width, h = v->display.height;
		u_short ddfstart, ddfwidth, con1;

		/* round down to nearest even width */
		/* w &= 0xfffe; */

		/* calculate datafetch width. */
		ddfwidth = ((v->bitmap->bytes_per_row >> 1) - 2) << 2;

		/* This will center the any overscanned display */
		/* and allow user to modify. */
		x = v->display.x + ph_this_data->std_start_x - ((w - 640) >> 2);
		y = v->display.y + ph_this_data->std_start_y - ((h - 256) >> 1);

		if (y & 1)
			y--;

		if (!(x & 1))
			x--;

		hstart = x;
		hstop = x + (w >> 1);
		vstart = y;
		vstop = y + h;
		ddfstart = (hstart - 9) >> 1;
		/* check for hardware limits, AGA may allow more..? */
		/* anyone got a 4000 I can borrow :^) -ch */
		if ((ddfstart & 0xfffc) + ddfwidth > 0xd8) {
			int     d = 0;

			/* XXX anyone know the equality properties of
			 * intermixed logial AND's */
			/* XXX and arithmetic operators? */
			while (((ddfstart & 0xfffc) + ddfwidth - d) > 0xd8) {
				d++;
			}

			ddfstart -= d;
			hstart -= d << 1;
			hstop -= d << 1;
		}
		/* correct the datafetch to proper limits. */
		/* delay the actual display of the data until we need it. */
		ddfstart &= 0xfffc;
		con1 = ((hstart - 9) - (ddfstart << 1)) | (((hstart - 9) - (ddfstart << 1)) << 4);

		if (ph_this_data->current_view) {
			VDATA(ph_this_data->current_view)->flags &= ~VF_DISPLAY;	/* mark as no longer */
			/* displayed. */
		}
		ph_this_data->current_view = v;

		cp = ph_this_data->frames[F_STORE_LONG];
#if defined (GRF_ECS) || defined (GRF_AGA)
#if defined GRF_AGA
		tmp = find_copper_inst(cp, CI_MOVE(R_FMODE));
		tmp->cp.inst.operand = 0;
#endif
		tmp = find_copper_inst(cp, CI_MOVE(R_BEAMCON0));
		tmp->cp.inst.operand = ph_this_data->beamcon0;
		tmp = find_copper_inst(cp, CI_MOVE(R_DIWHIGH));
		tmp->cp.inst.operand = CALC_DIWHIGH(hstart, vstart, hstop, vstop);
#endif /* ECS */
		tmp = find_copper_inst(cp, CI_MOVE(R_BPLCON0));
		tmp->cp.inst.operand = ph_this_data->bplcon0 | ((depth & 0x7) << 12);
		tmp = find_copper_inst(cp, CI_MOVE(R_BPLCON1));
		tmp->cp.inst.operand = con1;
		tmp = find_copper_inst(cp, CI_MOVE(R_DIWSTART));
		tmp->cp.inst.operand = ((vstart & 0xff) << 8) | (hstart & 0xff);
		tmp = find_copper_inst(cp, CI_MOVE(R_DIWSTOP));
		tmp->cp.inst.operand = ((vstop & 0xff) << 8) | (hstop & 0xff);
		tmp = find_copper_inst(cp, CI_MOVE(R_DDFSTART));
		tmp->cp.inst.operand = ddfstart;
		tmp = find_copper_inst(cp, CI_MOVE(R_DDFSTOP));
		tmp->cp.inst.operand = ddfstart + ddfwidth;

		tmp = find_copper_inst(cp, CI_MOVE(R_BPL0PTH));
		for (i = 0, j = 0; i < depth; j += 2, i++) {
			/* update the plane pointers */
			tmp[j].cp.inst.operand = HIADDR(PREP_DMA_MEM(v->bitmap->plane[i]));
			tmp[j + 1].cp.inst.operand = LOADDR(PREP_DMA_MEM(v->bitmap->plane[i]));
		}

		/* set mods correctly. */
		tmp = find_copper_inst(cp, CI_MOVE(R_BPL1MOD));
		tmp[0].cp.inst.operand = v->bitmap->row_mod;
		tmp[1].cp.inst.operand = v->bitmap->row_mod;

		/* set next pointers correctly */
		tmp = find_copper_inst(cp, CI_MOVE(R_COP1LCH));
		tmp[0].cp.inst.operand = HIADDR(PREP_DMA_MEM(ph_this_data->frames[F_STORE_LONG]));
		tmp[1].cp.inst.operand = LOADDR(PREP_DMA_MEM(ph_this_data->frames[F_STORE_LONG]));

		cp = ph_this_data->frames[F_LONG];
		ph_this_data->frames[F_LONG] = ph_this_data->frames[F_STORE_LONG];
		ph_this_data->frames[F_STORE_LONG] = cp;

		vd->flags |= VF_DISPLAY;
		cc_use_colormap(v, vd->colormap);
	}
	cc_load_mode(ph_this);
}

dmode_t *
cc_init_pal_hires_lace()
{
	/* this function should only be called once. */
	if (!phl_this) {
		u_short len = std_copper_list_len;
		cop_t  *cp;

		phl_this = &pal_hires_lace_mode;
		phl_this_data = &pal_hires_lace_mode_data;
		bzero(phl_this, sizeof(dmode_t));
		bzero(phl_this_data, sizeof(dmdata_t));

		phl_this->name = "pal: hires interlace";
		phl_this->nominal_size.width = 640;
		phl_this->nominal_size.height = 512;
		phl_this_data->max_size.width = 724;
		phl_this_data->max_size.height = 578;
		phl_this_data->min_size.width = 320;
		phl_this_data->min_size.height = 484;
		phl_this_data->min_depth = 1;
		phl_this_data->max_depth = 4;
		phl_this->data = phl_this_data;

		phl_this->get_monitor = cc_get_monitor;
		phl_this->alloc_view = cc_alloc_view;
		phl_this->get_current_view = cc_get_current_view;

		phl_this_data->use_colormap = cc_use_colormap;
		phl_this_data->get_colormap = cc_get_colormap;
		phl_this_data->alloc_colormap = cc_alloc_colormap;
		phl_this_data->display_view = display_pal_hires_lace_view;
		phl_this_data->monitor = cc_monitor;

		phl_this_data->flags |= DMF_INTERLACE;

		phl_this_data->frames = pal_hires_lace_frames;
		phl_this_data->frames[F_LACE_LONG] = alloc_chipmem(std_copper_list_size * F_LACE_TOTAL);
		if (!phl_this_data->frames[F_LACE_LONG]) {
			panic("couldn't get chipmem for copper list");
		}
		phl_this_data->frames[F_LACE_SHORT] = &phl_this_data->frames[F_LACE_LONG][len];
		phl_this_data->frames[F_LACE_STORE_LONG] = &phl_this_data->frames[F_LACE_SHORT][len];
		phl_this_data->frames[F_LACE_STORE_SHORT] = &phl_this_data->frames[F_LACE_STORE_LONG][len];

		bcopy(std_copper_list, phl_this_data->frames[F_LACE_STORE_LONG], std_copper_list_size);
		bcopy(std_copper_list, phl_this_data->frames[F_LACE_STORE_SHORT], std_copper_list_size);
		bcopy(std_copper_list, phl_this_data->frames[F_LACE_LONG], std_copper_list_size);
		bcopy(std_copper_list, phl_this_data->frames[F_LACE_SHORT], std_copper_list_size);

		phl_this_data->bplcon0 = 0x8204 | USE_CON3;	/* hires, color
								 * composite enable,
								 * lace. */
		phl_this_data->std_start_x = STANDARD_VIEW_X;
		phl_this_data->std_start_y = STANDARD_VIEW_Y;
		phl_this_data->vbl_handler = (vbl_handler_func *) cc_lace_mode_vbl_handler;
#if defined (GRF_ECS) || defined (GRF_AGA)
		phl_this_data->beamcon0 = STANDARD_PAL_BEAMCON;
#endif

		LIST_INSERT_HEAD(&MDATA(cc_monitor)->modes, phl_this, link);
	}
	return (phl_this);
}

void
display_pal_hires_lace_view(v)
	view_t *v;
{
	if (phl_this_data->current_view != v) {
		vdata_t *vd = VDATA(v);
		monitor_t *monitor = phl_this_data->monitor;
		cop_t  *cp = phl_this_data->frames[F_LACE_STORE_LONG], *tmp;
		int     depth = v->bitmap->depth, i;
		int     hstart, hstop, vstart, vstop, j;
		int     x, y, w = v->display.width, h = v->display.height;
		u_short ddfstart, ddfwidth, con1;

		/* round down to nearest even width */
		/* w &= 0xfffe; */

		/* calculate datafetch width. */
		ddfwidth = ((v->bitmap->bytes_per_row >> 1) - 2) << 2;

		/* This will center the any overscanned display */
		/* and allow user to modify. */
		x = v->display.x + phl_this_data->std_start_x - ((w - 640) >> 2);
		y = v->display.y + phl_this_data->std_start_y - ((h - 512) >> 2);

		if (y & 1)
			y--;

		if (!(x & 1))
			x--;

		hstart = x;
		hstop = x + (w >> 1);
		vstart = y;
		vstop = y + (h >> 1);
		ddfstart = (hstart - 9) >> 1;

		/* check for hardware limits, AGA may allow more..? */
		/* anyone got a 4000 I can borrow :^) -ch */
		if ((ddfstart & 0xfffc) + ddfwidth > 0xd8) {
			int     d = 0;

			/* XXX anyone know the equality properties of
			 * intermixed logial AND's */
			/* XXX and arithmetic operators? */
			while (((ddfstart & 0xfffc) + ddfwidth - d) > 0xd8) {
				d++;
			}

			ddfstart -= d;
			hstart -= d << 1;
			hstop -= d << 1;
		}
		/* correct the datafetch to proper limits. */
		/* delay the actual display of the data until we need it. */
		ddfstart &= 0xfffc;
		con1 = ((hstart - 9) - (ddfstart << 1)) | (((hstart - 9) - (ddfstart << 1)) << 4);

		if (phl_this_data->current_view) {
			VDATA(phl_this_data->current_view)->flags &= ~VF_DISPLAY;	/* mark as no longer */
			/* displayed. */
		}
		phl_this_data->current_view = v;

		cp = phl_this_data->frames[F_LACE_STORE_LONG];
#if defined (GRF_ECS) || defined (GRF_AGA)
#if defined GRF_AGA
		tmp = find_copper_inst(cp, CI_MOVE(R_FMODE));
		tmp->cp.inst.operand = 0;
#endif
		tmp = find_copper_inst(cp, CI_MOVE(R_BEAMCON0));
		tmp->cp.inst.operand = phl_this_data->beamcon0;
		tmp = find_copper_inst(cp, CI_MOVE(R_DIWHIGH));
		tmp->cp.inst.operand = CALC_DIWHIGH(hstart, vstart, hstop, vstop);
#endif /* ECS */
		tmp = find_copper_inst(cp, CI_MOVE(R_BPLCON0));
		tmp->cp.inst.operand = phl_this_data->bplcon0 | ((depth & 0x7) << 12);
		tmp = find_copper_inst(cp, CI_MOVE(R_BPLCON1));
		tmp->cp.inst.operand = con1;
		tmp = find_copper_inst(cp, CI_MOVE(R_DIWSTART));
		tmp->cp.inst.operand = ((vstart & 0xff) << 8) | (hstart & 0xff);
		tmp = find_copper_inst(cp, CI_MOVE(R_DIWSTOP));
		tmp->cp.inst.operand = ((vstop & 0xff) << 8) | (hstop & 0xff);
		tmp = find_copper_inst(cp, CI_MOVE(R_DDFSTART));
		tmp->cp.inst.operand = ddfstart;
		tmp = find_copper_inst(cp, CI_MOVE(R_DDFSTOP));
		tmp->cp.inst.operand = ddfstart + ddfwidth;

		tmp = find_copper_inst(cp, CI_MOVE(R_BPL0PTH));
		for (i = 0, j = 0; i < depth; j += 2, i++) {
			/* update the plane pointers */
			tmp[j].cp.inst.operand = HIADDR(PREP_DMA_MEM(v->bitmap->plane[i]));
			tmp[j + 1].cp.inst.operand = LOADDR(PREP_DMA_MEM(v->bitmap->plane[i]));
		}

		/* set mods correctly. */
		tmp = find_copper_inst(cp, CI_MOVE(R_BPL1MOD));
		tmp[0].cp.inst.operand = v->bitmap->bytes_per_row + v->bitmap->row_mod;
		tmp[1].cp.inst.operand = v->bitmap->bytes_per_row + v->bitmap->row_mod;

		/* set next pointers correctly */
		tmp = find_copper_inst(cp, CI_MOVE(R_COP1LCH));
		tmp[0].cp.inst.operand = HIADDR(PREP_DMA_MEM(phl_this_data->frames[F_LACE_STORE_SHORT]));
		tmp[1].cp.inst.operand = LOADDR(PREP_DMA_MEM(phl_this_data->frames[F_LACE_STORE_SHORT]));


		bcopy(phl_this_data->frames[F_LACE_STORE_LONG], phl_this_data->frames[F_LACE_STORE_SHORT], std_copper_list_size);

		/* these are the only ones that are different from long frame. */
		cp = phl_this_data->frames[F_LACE_STORE_SHORT];
		tmp = find_copper_inst(cp, CI_MOVE(R_BPL0PTH));
		for (i = 0, j = 0; i < depth; j += 2, i++) {
			u_short mod = v->bitmap->bytes_per_row + v->bitmap->row_mod;
			/* update plane pointers. high and low. */
			tmp[j].cp.inst.operand = HIADDR(PREP_DMA_MEM(&v->bitmap->plane[i][mod]));
			tmp[j + 1].cp.inst.operand = LOADDR(PREP_DMA_MEM(&v->bitmap->plane[i][mod]));
		}

		/* set next pointers correctly */
		tmp = find_copper_inst(cp, CI_MOVE(R_COP1LCH));
		tmp[0].cp.inst.operand = HIADDR(PREP_DMA_MEM(phl_this_data->frames[F_LACE_STORE_LONG]));
		tmp[1].cp.inst.operand = LOADDR(PREP_DMA_MEM(phl_this_data->frames[F_LACE_STORE_LONG]));


		cp = phl_this_data->frames[F_LACE_LONG];
		phl_this_data->frames[F_LACE_LONG] = phl_this_data->frames[F_LACE_STORE_LONG];
		phl_this_data->frames[F_LACE_STORE_LONG] = cp;

		cp = phl_this_data->frames[F_LACE_SHORT];
		phl_this_data->frames[F_LACE_SHORT] = phl_this_data->frames[F_LACE_STORE_SHORT];
		phl_this_data->frames[F_LACE_STORE_SHORT] = cp;

		vd->flags |= VF_DISPLAY;
		cc_use_colormap(v, vd->colormap);
	}
	cc_load_mode(phl_this);
}
#if defined (GRF_A2024)

dmode_t *
cc_init_pal_hires_dlace()
{
	/* this function should only be called once. */
	if (!phdl_this) {
		u_short len = std_dlace_copper_list_len;
		cop_t  *cp;

		phdl_this = &pal_hires_dlace_mode;
		phdl_this_data = &pal_hires_dlace_mode_data;
		bzero(phdl_this, sizeof(dmode_t));
		bzero(phdl_this_data, sizeof(dmdata_t));

		phdl_this->name = "pal: hires double interlace";
		phdl_this->nominal_size.width = 640;
		phdl_this->nominal_size.height = 1024;
		phdl_this_data->max_size.width = 724;
		phdl_this_data->max_size.height = 1024;
		phdl_this_data->min_size.width = 320;
		phdl_this_data->min_size.height = 512;
		phdl_this_data->min_depth = 1;
		phdl_this_data->max_depth = 2;
		phdl_this->data = phdl_this_data;

		phdl_this->get_monitor = cc_get_monitor;
		phdl_this->alloc_view = cc_alloc_view;
		phdl_this->get_current_view = cc_get_current_view;

		phdl_this_data->use_colormap = cc_a2024_use_colormap;
		phdl_this_data->get_colormap = cc_a2024_get_colormap;
		phdl_this_data->alloc_colormap = cc_a2024_alloc_colormap;
		phdl_this_data->display_view = display_pal_hires_dlace_view;
		phdl_this_data->monitor = cc_monitor;

		phdl_this_data->flags |= DMF_INTERLACE;

		phdl_this_data->frames = pal_hires_dlace_frames;
		phdl_this_data->frames[F_LACE_LONG] = alloc_chipmem(std_dlace_copper_list_size * F_LACE_TOTAL);
		if (!phdl_this_data->frames[F_LACE_LONG]) {
			panic("couldn't get chipmem for copper list");
		}
		phdl_this_data->frames[F_LACE_SHORT] = &phdl_this_data->frames[F_LACE_LONG][len];
		phdl_this_data->frames[F_LACE_STORE_LONG] = &phdl_this_data->frames[F_LACE_SHORT][len];
		phdl_this_data->frames[F_LACE_STORE_SHORT] = &phdl_this_data->frames[F_LACE_STORE_LONG][len];

		bcopy(std_dlace_copper_list, phdl_this_data->frames[F_LACE_STORE_LONG], std_dlace_copper_list_size);
		bcopy(std_dlace_copper_list, phdl_this_data->frames[F_LACE_STORE_SHORT], std_dlace_copper_list_size);
		bcopy(std_dlace_copper_list, phdl_this_data->frames[F_LACE_LONG], std_dlace_copper_list_size);
		bcopy(std_dlace_copper_list, phdl_this_data->frames[F_LACE_SHORT], std_dlace_copper_list_size);

		phdl_this_data->bplcon0 = 0x8204 | USE_CON3;	/* hires, color
								 * composite enable,
								 * dlace. */
		phdl_this_data->std_start_x = STANDARD_VIEW_X;
		phdl_this_data->std_start_y = STANDARD_VIEW_Y;
		phdl_this_data->vbl_handler = (vbl_handler_func *) cc_lace_mode_vbl_handler;
#if defined (GRF_ECS) || defined (GRF_AGA)
		phdl_this_data->beamcon0 = STANDARD_PAL_BEAMCON;
#endif

		LIST_INSERT_HEAD(&MDATA(cc_monitor)->modes, phdl_this, link);
	}
	return (phdl_this);
}

void
display_pal_hires_dlace_view(v)
	view_t *v;
{
	if (phdl_this_data->current_view != v) {
		vdata_t *vd = VDATA(v);
		monitor_t *monitor = phdl_this_data->monitor;
		cop_t  *cp = phdl_this_data->frames[F_LACE_STORE_LONG], *tmp;
		int     depth = v->bitmap->depth, i;
		int     hstart, hstop, vstart, vstop, j;
		int     x, y, w = v->display.width, h = v->display.height;
		u_short ddfstart, ddfwidth, con1;
		u_short mod1l, mod2l;

		/* round down to nearest even width */
		/* w &= 0xfffe; */

		/* calculate datafetch width. */
		ddfwidth = ((v->bitmap->bytes_per_row >> 1) - 2) << 2;

		/* This will center the any overscanned display */
		/* and allow user to modify. */
		x = v->display.x + phdl_this_data->std_start_x - ((w - 640) >> 2);
		y = v->display.y + phdl_this_data->std_start_y - ((h - 1024) >> 3);

		if (y & 1)
			y--;

		if (!(x & 1))
			x--;

		hstart = x;
		hstop = x + (w >> 1);
		vstart = y;
		vstop = y + (h >> 2);
		ddfstart = (hstart - 9) >> 1;

		/* check for hardware limits, AGA may allow more..? */
		/* anyone got a 4000 I can borrow :^) -ch */
		if ((ddfstart & 0xfffc) + ddfwidth > 0xd8) {
			int     d = 0;

			/* XXX anyone know the equality properties of
			 * intermixed logial AND's */
			/* XXX and arithmetic operators? */
			while (((ddfstart & 0xfffc) + ddfwidth - d) > 0xd8) {
				d++;
			}

			ddfstart -= d;
			hstart -= d << 1;
			hstop -= d << 1;
		}
		/* correct the datafetch to proper limits. */
		/* delay the actual display of the data until we need it. */
		ddfstart &= 0xfffc;
		con1 = ((hstart - 9) - (ddfstart << 1)) | (((hstart - 9) - (ddfstart << 1)) << 4);

		if (phdl_this_data->current_view) {
			VDATA(phdl_this_data->current_view)->flags &= ~VF_DISPLAY;	/* mark as no longer */
			/* displayed. */
		}
		phdl_this_data->current_view = v;

		cp = phdl_this_data->frames[F_LACE_STORE_LONG];
#if defined (GRF_ECS) || defined (GRF_AGA)
#if defined GRF_AGA
		tmp = find_copper_inst(cp, CI_MOVE(R_FMODE));
		tmp->cp.inst.operand = 0;
#endif
		tmp = find_copper_inst(cp, CI_MOVE(R_BEAMCON0));
		tmp->cp.inst.operand = phdl_this_data->beamcon0;
		tmp = find_copper_inst(cp, CI_MOVE(R_DIWHIGH));
		tmp->cp.inst.operand = CALC_DIWHIGH(hstart, vstart, hstop, vstop);
#endif /* ECS */
		tmp = find_copper_inst(cp, CI_MOVE(R_BPLCON0));
		tmp->cp.inst.operand = phdl_this_data->bplcon0 | ((depth & 0x7) << 13);	/* times two. */
		tmp = find_copper_inst(cp, CI_MOVE(R_BPLCON1));
		tmp->cp.inst.operand = con1;
		tmp = find_copper_inst(cp, CI_MOVE(R_DIWSTART));
		tmp->cp.inst.operand = ((vstart & 0xff) << 8) | (hstart & 0xff);
		tmp = find_copper_inst(cp, CI_MOVE(R_DIWSTOP));
		tmp->cp.inst.operand = ((vstop & 0xff) << 8) | (hstop & 0xff);
		tmp = find_copper_inst(cp, CI_MOVE(R_DDFSTART));
		tmp->cp.inst.operand = ddfstart;
		tmp = find_copper_inst(cp, CI_MOVE(R_DDFSTOP));
		tmp->cp.inst.operand = ddfstart + ddfwidth;

		mod1l = v->bitmap->bytes_per_row + v->bitmap->row_mod;
		mod2l = mod1l << 1;

		tmp = find_copper_inst(cp, CI_MOVE(R_BPL0PTH));
		tmp[0].cp.inst.operand = HIADDR(PREP_DMA_MEM(&v->bitmap->plane[0][0]));	/* update plane
											 * pointers. */
		tmp[1].cp.inst.operand = LOADDR(PREP_DMA_MEM(&v->bitmap->plane[0][0]));	/* high and low. */
		tmp[2].cp.inst.operand = HIADDR(PREP_DMA_MEM(&v->bitmap->plane[0][mod1l]));	/* update plane
												 * pointers. */
		tmp[3].cp.inst.operand = LOADDR(PREP_DMA_MEM(&v->bitmap->plane[0][mod1l]));	/* high and low. */
		if (depth == 2) {
			tmp[4].cp.inst.operand = HIADDR(PREP_DMA_MEM(&v->bitmap->plane[1][0]));	/* update plane
												 * pointers. */
			tmp[5].cp.inst.operand = LOADDR(PREP_DMA_MEM(&v->bitmap->plane[1][0]));	/* high and low. */
			tmp[6].cp.inst.operand = HIADDR(PREP_DMA_MEM(&v->bitmap->plane[1][mod1l]));	/* update plane
													 * pointers. */
			tmp[7].cp.inst.operand = LOADDR(PREP_DMA_MEM(&v->bitmap->plane[1][mod1l]));	/* high and low. */
		}
		/* set mods correctly. */
		tmp = find_copper_inst(cp, CI_MOVE(R_BPL1MOD));
		tmp[0].cp.inst.operand = mod2l + mod1l;
		tmp[1].cp.inst.operand = mod2l + mod1l;

		/* set next pointers correctly */
		tmp = find_copper_inst(cp, CI_MOVE(R_COP1LCH));
		tmp[0].cp.inst.operand = HIADDR(PREP_DMA_MEM(phdl_this_data->frames[F_LACE_STORE_SHORT]));
		tmp[1].cp.inst.operand = LOADDR(PREP_DMA_MEM(phdl_this_data->frames[F_LACE_STORE_SHORT]));

		bcopy(phdl_this_data->frames[F_LACE_STORE_LONG], phdl_this_data->frames[F_LACE_STORE_SHORT], std_dlace_copper_list_size);

		/* these are the only ones that are different from long frame. */
		cp = phdl_this_data->frames[F_LACE_STORE_SHORT];
		tmp = find_copper_inst(cp, CI_MOVE(R_BPL0PTH));
		tmp[0].cp.inst.operand = HIADDR(PREP_DMA_MEM(&v->bitmap->plane[0][mod2l]));	/* update plane
												 * pointers. */
		tmp[1].cp.inst.operand = LOADDR(PREP_DMA_MEM(&v->bitmap->plane[0][mod2l]));	/* high and low. */
		tmp[2].cp.inst.operand = HIADDR(PREP_DMA_MEM(&v->bitmap->plane[0][mod2l + mod1l]));	/* update plane
													 * pointers. */
		tmp[3].cp.inst.operand = LOADDR(PREP_DMA_MEM(&v->bitmap->plane[0][mod2l + mod1l]));	/* high and low. */
		if (depth == 2) {
			tmp[4].cp.inst.operand = HIADDR(PREP_DMA_MEM(&v->bitmap->plane[1][mod2l]));	/* update plane
													 * pointers. */
			tmp[5].cp.inst.operand = LOADDR(PREP_DMA_MEM(&v->bitmap->plane[1][mod2l]));	/* high and low. */
			tmp[6].cp.inst.operand = HIADDR(PREP_DMA_MEM(&v->bitmap->plane[1][mod2l + mod1l]));	/* update plane
														 * pointers. */
			tmp[7].cp.inst.operand = LOADDR(PREP_DMA_MEM(&v->bitmap->plane[1][mod2l + mod1l]));	/* high and low. */
		}
		/* set next pointers correctly */
		tmp = find_copper_inst(cp, CI_MOVE(R_COP1LCH));
		tmp[0].cp.inst.operand = HIADDR(PREP_DMA_MEM(phdl_this_data->frames[F_LACE_STORE_LONG]));
		tmp[1].cp.inst.operand = LOADDR(PREP_DMA_MEM(phdl_this_data->frames[F_LACE_STORE_LONG]));

		cp = phdl_this_data->frames[F_LACE_LONG];
		phdl_this_data->frames[F_LACE_LONG] = phdl_this_data->frames[F_LACE_STORE_LONG];
		phdl_this_data->frames[F_LACE_STORE_LONG] = cp;

		cp = phdl_this_data->frames[F_LACE_SHORT];
		phdl_this_data->frames[F_LACE_SHORT] = phdl_this_data->frames[F_LACE_STORE_SHORT];
		phdl_this_data->frames[F_LACE_STORE_SHORT] = cp;

		vd->flags |= VF_DISPLAY;

		cc_a2024_use_colormap(v, vd->colormap);
	}
	cc_load_mode(phdl_this);
}

dmode_t *
cc_init_pal_a2024()
{
	/* this function should only be called once. */
	if (!p24_this) {
		int     i;
		u_short len = std_pal_a2024_copper_list_len;
		cop_t  *cp;

		p24_this = &pal_a2024_mode;
		p24_this_data = &pal_a2024_mode_data;
		bzero(p24_this, sizeof(dmode_t));
		bzero(p24_this_data, sizeof(dmdata_t));

		p24_this->name = "pal: A2024 15khz";
		p24_this->nominal_size.width = 1024;
		p24_this->nominal_size.height = 1024;
		p24_this_data->max_size.width = 1024;
		p24_this_data->max_size.height = 1024;
		p24_this_data->min_size.width = 1024;
		p24_this_data->min_size.height = 1024;
		p24_this_data->min_depth = 1;
		p24_this_data->max_depth = 2;
		p24_this->data = p24_this_data;

		p24_this->get_monitor = cc_get_monitor;
		p24_this->alloc_view = cc_alloc_view;
		p24_this->get_current_view = cc_get_current_view;

		p24_this_data->use_colormap = cc_a2024_use_colormap;
		p24_this_data->get_colormap = cc_a2024_get_colormap;
		p24_this_data->display_view = display_pal_a2024_view;
		p24_this_data->alloc_colormap = cc_a2024_alloc_colormap;
		p24_this_data->monitor = cc_monitor;

		p24_this_data->flags |= DMF_HEDLEY_EXP;

		p24_this_data->frames = pal_a2024_frames;
		p24_this_data->frames[F_QD_QUAD0] = alloc_chipmem(std_pal_a2024_copper_list_size * F_QD_TOTAL);
		if (!p24_this_data->frames[F_QD_QUAD0]) {
			panic("couldn't get chipmem for copper list");
		}
		/* setup the hedley init bitplane. */
		hedley_init = alloc_chipmem(128);
		if (!hedley_init) {
			panic("couldn't get chipmem for hedley init bitplane");
		}
		for (i = 1; i < 128; i++)
			hedley_init[i] = 0xff;
		hedley_init[0] = 0x03;

		/* copy image of standard copper list. */
		bcopy(std_pal_a2024_copper_list, p24_this_data->frames[0], std_pal_a2024_copper_list_size);

		/* set the init plane pointer. */
		cp = find_copper_inst(p24_this_data->frames[F_QD_QUAD0], CI_MOVE(R_BPL0PTH));
		cp[0].cp.inst.operand = HIADDR(PREP_DMA_MEM(hedley_init));
		cp[1].cp.inst.operand = LOADDR(PREP_DMA_MEM(hedley_init));

		for (i = 1; i < F_QD_TOTAL; i++) {
			p24_this_data->frames[i] = &p24_this_data->frames[i - 1][len];
			bcopy(p24_this_data->frames[0], p24_this_data->frames[i], std_pal_a2024_copper_list_size);
		}

		p24_this_data->bplcon0 = 0x8200;	/* hires */
		p24_this_data->vbl_handler = (vbl_handler_func *) pal_a2024_mode_vbl_handler;


		LIST_INSERT_HEAD(&MDATA(cc_monitor)->modes, p24_this, link);
	}
	return (p24_this);
}

void
display_pal_a2024_view(v)
	view_t *v;
{
	if (p24_this_data->current_view != v) {
		vdata_t *vd = VDATA(v);
		monitor_t *monitor = p24_this_data->monitor;
		cop_t  *cp, *tmp;
		u_char *inst_plane[2];
		u_char **plane = inst_plane;
		u_long  full_line = v->bitmap->bytes_per_row + v->bitmap->row_mod;
		u_long  half_plane = full_line * v->bitmap->rows / 2;

		int     line_mod = 0xbc;	/* standard 2024 15khz mod. */
		int     depth = v->bitmap->depth, i, j;

		plane[0] = v->bitmap->plane[0];
		if (depth == 2) {
			plane[1] = v->bitmap->plane[1];
		}
		if (p24_this_data->current_view) {
			VDATA(p24_this_data->current_view)->flags &= ~VF_DISPLAY;	/* mark as no longer
											 * displayed. */
		}
		cp = p24_this_data->frames[F_QD_STORE_QUAD0];
		tmp = find_copper_inst(cp, CI_MOVE(R_COLOR1F));
		tmp = find_copper_inst(tmp, CI_MOVE(R_BPLCON0));	/* grab third one. */
		tmp->cp.inst.operand = p24_this_data->bplcon0 | ((depth & 0x7) << 13);	/* times 2 */

		bcopy(p24_this_data->frames[F_QD_STORE_QUAD0], p24_this_data->frames[F_QD_STORE_QUAD1], std_pal_a2024_copper_list_size);
		bcopy(p24_this_data->frames[F_QD_STORE_QUAD0], p24_this_data->frames[F_QD_STORE_QUAD2], std_pal_a2024_copper_list_size);
		bcopy(p24_this_data->frames[F_QD_STORE_QUAD0], p24_this_data->frames[F_QD_STORE_QUAD3], std_pal_a2024_copper_list_size);

		/*
		 * Mark Id's
		 */
		tmp = find_copper_inst(p24_this_data->frames[F_QD_STORE_QUAD1], CI_WAIT(126, 29));
		CBUMP(tmp);
		CMOVE(tmp, R_COLOR01, QUAD1_ID);
		tmp = find_copper_inst(p24_this_data->frames[F_QD_STORE_QUAD2], CI_WAIT(126, 29));
		CBUMP(tmp);
		CMOVE(tmp, R_COLOR01, QUAD2_ID);
		tmp = find_copper_inst(p24_this_data->frames[F_QD_STORE_QUAD3], CI_WAIT(126, 29));
		CBUMP(tmp);
		CMOVE(tmp, R_COLOR01, QUAD3_ID);

		plane[0]--;
		plane[0]--;
		if (depth == 2) {
			plane[1]--;
			plane[1]--;
		}
		/*
		 * Set bitplane pointers.
		 */
		tmp = find_copper_inst(p24_this_data->frames[F_QD_STORE_QUAD0], CI_MOVE(R_BPLMOD2));
		CBUMP(tmp);
		CMOVE(tmp, R_BPL0PTH, HIADDR(PREP_DMA_MEM(&plane[0][0])));
		CMOVE(tmp, R_BPL0PTL, LOADDR(PREP_DMA_MEM(&plane[0][0])));
		CMOVE(tmp, R_BPL1PTH, HIADDR(PREP_DMA_MEM(&plane[0][full_line])));
		CMOVE(tmp, R_BPL1PTL, LOADDR(PREP_DMA_MEM(&plane[0][full_line])));
		if (depth == 2) {
			CMOVE(tmp, R_BPL2PTH, HIADDR(PREP_DMA_MEM(&plane[1][0])));
			CMOVE(tmp, R_BPL2PTL, LOADDR(PREP_DMA_MEM(&plane[1][0])));
			CMOVE(tmp, R_BPL3PTH, HIADDR(PREP_DMA_MEM(&plane[1][full_line])));
			CMOVE(tmp, R_BPL3PTL, LOADDR(PREP_DMA_MEM(&plane[1][full_line])));
		}
#if defined (GRF_ECS) || defined (GRF_AGA)
		CMOVE(tmp, R_DIWHIGH, 0x2100);
#endif
		CMOVE(tmp, R_COP1LCH, HIADDR(PREP_DMA_MEM(p24_this_data->frames[F_QD_STORE_QUAD1])));
		CMOVE(tmp, R_COP1LCL, LOADDR(PREP_DMA_MEM(p24_this_data->frames[F_QD_STORE_QUAD1])));
		CEND(tmp);
		CEND(tmp);

		tmp = find_copper_inst(p24_this_data->frames[F_QD_STORE_QUAD1], CI_MOVE(R_BPLMOD2));
		CBUMP(tmp);
		CMOVE(tmp, R_BPL0PTH, HIADDR(PREP_DMA_MEM(&plane[0][HALF_2024_LINE])));
		CMOVE(tmp, R_BPL0PTL, LOADDR(PREP_DMA_MEM(&plane[0][HALF_2024_LINE])));
		CMOVE(tmp, R_BPL1PTH, HIADDR(PREP_DMA_MEM(&plane[0][full_line + HALF_2024_LINE])));
		CMOVE(tmp, R_BPL1PTL, LOADDR(PREP_DMA_MEM(&plane[0][full_line + HALF_2024_LINE])));
		if (depth == 2) {
			CMOVE(tmp, R_BPL2PTH, HIADDR(PREP_DMA_MEM(&plane[1][HALF_2024_LINE])));
			CMOVE(tmp, R_BPL2PTL, LOADDR(PREP_DMA_MEM(&plane[1][HALF_2024_LINE])));
			CMOVE(tmp, R_BPL3PTH, HIADDR(PREP_DMA_MEM(&plane[1][full_line + HALF_2024_LINE])));
			CMOVE(tmp, R_BPL3PTL, LOADDR(PREP_DMA_MEM(&plane[1][full_line + HALF_2024_LINE])));
		}
#if defined (GRF_ECS) || defined (GRF_AGA)
		CMOVE(tmp, R_DIWHIGH, 0x2100);
#endif
		CMOVE(tmp, R_COP1LCH, HIADDR(PREP_DMA_MEM(p24_this_data->frames[F_QD_STORE_QUAD2])));
		CMOVE(tmp, R_COP1LCL, LOADDR(PREP_DMA_MEM(p24_this_data->frames[F_QD_STORE_QUAD2])));
		CEND(tmp);
		CEND(tmp);

		tmp = find_copper_inst(p24_this_data->frames[F_QD_STORE_QUAD2], CI_MOVE(R_BPLMOD2));
		CBUMP(tmp);
		CMOVE(tmp, R_BPL0PTH, HIADDR(PREP_DMA_MEM(&plane[0][half_plane])));
		CMOVE(tmp, R_BPL0PTL, LOADDR(PREP_DMA_MEM(&plane[0][half_plane])));
		CMOVE(tmp, R_BPL1PTH, HIADDR(PREP_DMA_MEM(&plane[0][half_plane + full_line])));
		CMOVE(tmp, R_BPL1PTL, LOADDR(PREP_DMA_MEM(&plane[0][half_plane + full_line])));
		if (depth == 2) {
			CMOVE(tmp, R_BPL2PTH, HIADDR(PREP_DMA_MEM(&plane[1][half_plane])));
			CMOVE(tmp, R_BPL2PTL, LOADDR(PREP_DMA_MEM(&plane[1][half_plane])));
			CMOVE(tmp, R_BPL3PTH, HIADDR(PREP_DMA_MEM(&plane[1][half_plane + full_line])));
			CMOVE(tmp, R_BPL3PTL, LOADDR(PREP_DMA_MEM(&plane[1][half_plane + full_line])));
		}
#if defined (GRF_ECS) || defined (GRF_AGA)
		CMOVE(tmp, R_DIWHIGH, 0x2100);
#endif
		CMOVE(tmp, R_COP1LCH, HIADDR(PREP_DMA_MEM(p24_this_data->frames[F_QD_STORE_QUAD3])));
		CMOVE(tmp, R_COP1LCL, LOADDR(PREP_DMA_MEM(p24_this_data->frames[F_QD_STORE_QUAD3])));
		CEND(tmp);
		CEND(tmp);

		tmp = find_copper_inst(p24_this_data->frames[F_QD_STORE_QUAD3], CI_MOVE(R_BPLMOD2));
		CBUMP(tmp);
		CMOVE(tmp, R_BPL0PTH, HIADDR(PREP_DMA_MEM(&plane[0][half_plane + HALF_2024_LINE])));
		CMOVE(tmp, R_BPL0PTL, LOADDR(PREP_DMA_MEM(&plane[0][half_plane + HALF_2024_LINE])));
		CMOVE(tmp, R_BPL1PTH, HIADDR(PREP_DMA_MEM(&plane[0][half_plane + full_line + HALF_2024_LINE])));
		CMOVE(tmp, R_BPL1PTL, LOADDR(PREP_DMA_MEM(&plane[0][half_plane + full_line + HALF_2024_LINE])));
		if (depth == 2) {
			CMOVE(tmp, R_BPL2PTH, HIADDR(PREP_DMA_MEM(&plane[1][half_plane + HALF_2024_LINE])));
			CMOVE(tmp, R_BPL2PTL, LOADDR(PREP_DMA_MEM(&plane[1][half_plane + HALF_2024_LINE])));
			CMOVE(tmp, R_BPL3PTH, HIADDR(PREP_DMA_MEM(&plane[1][half_plane + full_line + HALF_2024_LINE])));
			CMOVE(tmp, R_BPL3PTL, LOADDR(PREP_DMA_MEM(&plane[1][half_plane + full_line + HALF_2024_LINE])));
		}
#if defined (GRF_ECS) || defined (GRF_AGA)
		CMOVE(tmp, R_DIWHIGH, 0x2100);
#endif
		CMOVE(tmp, R_COP1LCH, HIADDR(PREP_DMA_MEM(p24_this_data->frames[F_QD_STORE_QUAD0])));
		CMOVE(tmp, R_COP1LCL, LOADDR(PREP_DMA_MEM(p24_this_data->frames[F_QD_STORE_QUAD0])));
		CEND(tmp);
		CEND(tmp);

		/* swap new pointers in. */
		for (i = F_QD_STORE_QUAD0, j = F_QD_QUAD0;
		    i <= F_QD_STORE_QUAD3; i++, j++) {
			cp = p24_this_data->frames[j];
			p24_this_data->frames[j] = p24_this_data->frames[i];
			p24_this_data->frames[i] = cp;
		}

		p24_this_data->current_view = v;
		vd->flags |= VF_DISPLAY;

		cc_a2024_use_colormap(v, vd->colormap);
	}
	cc_load_mode(p24_this);
}

void
pal_a2024_mode_vbl_handler(d)
	dmode_t *d;
{
	u_short vp = ((custom.vposr & 0x0007) << 8) | ((custom.vhposr) >> 8);

	if (vp < 20) {
		custom.cop1lc = PREP_DMA_MEM(p24_this_data->frames[p24_this_data->hedley_current]);
		custom.copjmp1 = 0;
	}
	p24_this_data->hedley_current++;
	p24_this_data->hedley_current &= 0x3;	/* if 4 then 0. */
}
#endif /* GRF_A2024 */

#if defined (GRF_AGA)

dmode_t *
cc_init_pal_aga()
{
	/* this function should only be called once. */
	if (!paga_this && (custom.deniseid & 0xff) == 0xf8 &&
	    aga_enable & AGA_ENABLE) {
		u_short len = aga_copper_list_len;
		cop_t  *cp;

		paga_this = &paga_mode;
		paga_this_data = &paga_mode_data;
		bzero(paga_this, sizeof(dmode_t));
		bzero(paga_this_data, sizeof(dmdata_t));

		paga_this->name = "pal: AGA dbl";
		paga_this->nominal_size.width = 640;
		paga_this->nominal_size.height = 512;
		paga_this_data->max_size.width = 720;
		paga_this_data->max_size.height = 564;
		paga_this_data->min_size.width = 320;
		paga_this_data->min_size.height = 200;
		paga_this_data->min_depth = 1;
		paga_this_data->max_depth = 8;
		paga_this->data = paga_this_data;

		paga_this->get_monitor = cc_get_monitor;
		paga_this->alloc_view = cc_alloc_view;
		paga_this->get_current_view = cc_get_current_view;

		paga_this_data->use_colormap = cc_use_aga_colormap;
		paga_this_data->get_colormap = cc_get_colormap;
		paga_this_data->alloc_colormap = cc_alloc_aga_colormap;
		paga_this_data->display_view = display_pal_aga_view;
		paga_this_data->monitor = cc_monitor;

		paga_this_data->frames = paga_frames;
		paga_this_data->frames[F_LONG] = alloc_chipmem(aga_copper_list_size * F_TOTAL);
		if (!paga_this_data->frames[F_LONG]) {
			panic("couldn't get chipmem for copper list");
		}
		paga_this_data->frames[F_STORE_LONG] = &paga_this_data->frames[F_LONG][len];

		bcopy(aga_copper_list, paga_this_data->frames[F_STORE_LONG], aga_copper_list_size);
		bcopy(aga_copper_list, paga_this_data->frames[F_LONG], aga_copper_list_size);

		paga_this_data->bplcon0 = 0x0240 | USE_CON3;	/* color composite
								 * enable,
								 * shres. */
		paga_this_data->std_start_x = 0x4f /*STANDARD_VIEW_X*/;
		paga_this_data->std_start_y = 0x2b /*STANDARD_VIEW_Y*/;
		paga_this_data->vbl_handler = (vbl_handler_func *) cc_mode_vbl_handler;
		paga_this_data->beamcon0 = STANDARD_PAL_BEAMCON | (SPECIAL_BEAMCON ^ VSYNCTRUE);

		LIST_INSERT_HEAD(&MDATA(cc_monitor)->modes,
		    paga_this, link);
	}
	return (paga_this);
}

/* static, so I can patch and play (VGAOnly is commented-out) */

int	pAGA_htotal  = 0x081;	/* 0x079 */
int	pAGA_hsstrt  = 0x00f;	/* 0x00f */
int	pAGA_hsstop  = 0x019;	/* 0x019 */
int	pAGA_hbstrt  = 0x001;	/* 0x001 */
int	pAGA_hbstop  = 0x021;	/* 0x021 */
int	pAGA_vtotal  = 0x23d;	/* 0x24d */
int	pAGA_vsstrt  = 0x001;	/* 0x001 */
int	pAGA_vsstop  = 0x008;	/* 0x008 */
int	pAGA_vbstrt  = 0x000;	/* 0x000 */
int	pAGA_vbstop  = 0x017;	/* 0x019 */
int	pAGA_hcenter = 0x04f;	/* 0x04b */

void
display_pal_aga_view(v)
	view_t *v;
{
	if (paga_this_data->current_view != v) {
		vdata_t *vd = VDATA(v);
		monitor_t *monitor = paga_this_data->monitor;
		cop_t  *cp = paga_this_data->frames[F_STORE_LONG], *tmp;
		int     depth = v->bitmap->depth, i;
		int     hstart, hstop, vstart, vstop, j;
		int     x, y, w = v->display.width, h = v->display.height;
		u_short ddfstart, ddfwidth, con1;

#ifdef DEBUG
		if (aga_enable & AGA_TRACE)
			printf("display_aga_view(%dx%dx%d) %x\n", w, h,
			    depth, v);
#endif
		/* round down to nearest even width */
		/* w &= 0xfffe; */
		/* calculate datafetch width. */

		ddfwidth = ((v->bitmap->bytes_per_row >> 1) - 4) << 1;

		/* this will center the any overscanned display */
		/* and allow user to modify. */
		x = v->display.x + paga_this_data->std_start_x - ((w - 640) >> 3);
		y = v->display.y + paga_this_data->std_start_y - ((h - 512) >> 1);

		if (y & 1)
			y--;

		if (!(x & 1))
			x--;

		hstart = x;
		hstop = x + (w >> 2);
		vstart = y;
		vstop = y + (h >> 0);
		ddfstart = (hstart >> 1) - 8;

#ifdef DEBUG
		if (aga_enable & AGA_TRACE2) {
			printf ("  ddfwidth %04x x %04x y %04x", ddfwidth,
			    x, y);
			printf (" hstart %04x hstop %04x vstart %04x vstop %04x ddfstart %04x\n",
			    hstart, hstop, vstart, vstop, ddfstart);
		}
#endif
		/* check for hardware limits, AGA may allow more..? */
		/* anyone got a 4000 I can borrow :^) -ch */
		if ((ddfstart & 0xfffc) + ddfwidth > 0xd8) {
			int     d = 0;

			/* XXX anyone know the equality properties of
			 * intermixed logial AND's */
			/* XXX and arithmetic operators? */
			while (((ddfstart & 0xfffc) + ddfwidth - d) > 0xd8) {
				d++;
			}

			ddfstart -= d;
			hstart -= d << 1;
			hstop -= d << 1;
		}
		/* correct the datafetch to proper limits. */
		/* delay the actual display of the data until we need it. */
		ddfstart &= 0xfffc;
#ifdef DEBUG
		if (aga_enable & AGA_TRACE2) {
			printf ("  ddfwidth %04x x %04x y %04x", ddfwidth,
			    x, y);
			printf (" hstart %04x hstop %04x vstart %04x vstop %04x ddfstart %04x\n",
			    hstart, hstop, vstart, vstop, ddfstart);
		}
#endif
		con1 = ((hstart - 9) - (ddfstart << 1)) | (((hstart - 9) - (ddfstart << 1)) << 4);

		if (paga_this_data->current_view) {
			VDATA(paga_this_data->current_view)->flags &= ~VF_DISPLAY;	/* mark as no longer */
			/* displayed. */
		}
		paga_this_data->current_view = v;

		cp = paga_this_data->frames[F_STORE_LONG];
		tmp = cp;
		for (i = 0; i < 8; ++i) {
			if (tmp == NULL)
				break;
			tmp = find_copper_inst(tmp + 1, CI_MOVE(R_BPLCON3));
			if (tmp == NULL)
				break;
			tmp->cp.inst.operand = 0x0ca1 | (i << 13);
			tmp = find_copper_inst(tmp + 1, CI_MOVE(R_BPLCON3));
			if (tmp == NULL)
				break;
			tmp->cp.inst.operand = 0x0ea1 | (i << 13);
		}
		if (tmp)
			tmp = find_copper_inst(tmp + 1, CI_MOVE(R_BPLCON3));
		if (tmp)
			tmp->cp.inst.operand = 0x0ca1;
		tmp = find_copper_inst(cp, CI_MOVE(R_FMODE));
		tmp->cp.inst.operand = 0x8003;
		tmp = find_copper_inst(cp, CI_MOVE(R_HTOTAL));
		tmp->cp.inst.operand = pAGA_htotal; /* 81/71/73/79? */
		tmp = find_copper_inst(cp, CI_MOVE(R_HBSTRT));
		tmp->cp.inst.operand = pAGA_hbstrt; /* 0x0008 */
		tmp = find_copper_inst(cp, CI_MOVE(R_HSSTRT));
		tmp->cp.inst.operand = pAGA_hsstrt; /* 0x000e */
		tmp = find_copper_inst(cp, CI_MOVE(R_HSSTOP));
		tmp->cp.inst.operand = pAGA_hsstop; /* 0x001c */
		tmp = find_copper_inst(cp, CI_MOVE(R_HBSTOP));
		tmp->cp.inst.operand = pAGA_hsstop; /* 0x001e */
		tmp = find_copper_inst(cp, CI_MOVE(R_HCENTER));
		tmp->cp.inst.operand = pAGA_hcenter; /*AGA_htotal / 2 + AGA_hsstrt */
		tmp = find_copper_inst(cp, CI_MOVE(R_VBSTRT));
		tmp->cp.inst.operand = pAGA_vbstrt; /* 0x0000 */
		tmp = find_copper_inst(cp, CI_MOVE(R_VSSTRT));
		tmp->cp.inst.operand = pAGA_vsstrt; /* 0x016b / AGA_htotal */
		tmp = find_copper_inst(cp, CI_MOVE(R_VSSTOP));
		tmp->cp.inst.operand = pAGA_vsstop; /* 0x02d6 / AGA_htotal */
		tmp = find_copper_inst(cp, CI_MOVE(R_VBSTOP));
		tmp->cp.inst.operand = pAGA_vbstop; /* 0x0bd1 / AGA_htotal */
		tmp = find_copper_inst(cp, CI_MOVE(R_VTOTAL));
		tmp->cp.inst.operand = pAGA_vtotal;
		tmp = find_copper_inst(cp, CI_MOVE(R_BEAMCON0));
		tmp->cp.inst.operand = paga_this_data->beamcon0;
#ifdef DEBUG
		if (aga_enable & AGA_TRACE2)
			printf("  beamcon0 %04x", tmp->cp.inst.operand);
#endif
		tmp = find_copper_inst(cp, CI_MOVE(R_DIWHIGH));
		tmp->cp.inst.operand = CALC_DIWHIGH(hstart, vstart, hstop, vstop);
#ifdef DEBUG
		if (aga_enable & AGA_TRACE2)
			printf(" diwhigh %04x>", tmp->cp.inst.operand);
#endif
#if 0
		tmp->cp.inst.operand = (vstop & 0x0700) | ((hstop & 0x0100) << 5);
#endif
#ifdef DEBUG
		if (aga_enable & AGA_TRACE2)
			printf("%04x", tmp->cp.inst.operand);
#endif
		tmp = find_copper_inst(cp, CI_MOVE(R_BPLCON0));
		tmp->cp.inst.operand = paga_this_data->bplcon0 |
		    ((depth & 0x7) << 12) | ((depth & 0x8) << 1);
#ifdef DEBUG
		if (aga_enable & AGA_TRACE2)
			printf(" bplcon0 %04x", tmp->cp.inst.operand);
#endif
		tmp = find_copper_inst(cp, CI_MOVE(R_BPLCON1));
		tmp->cp.inst.operand = con1;
#ifdef DEBUG
		if (aga_enable & AGA_TRACE2)
			printf(" bplcon1 %04x>0000\n", con1);
#endif
		tmp = find_copper_inst(cp, CI_MOVE(R_DIWSTART));
		tmp->cp.inst.operand = ((vstart & 0xff) << 8) | (hstart & 0xff);
#ifdef DEBUG
		if (aga_enable & AGA_TRACE2)
			printf("  diwstart %04x", tmp->cp.inst.operand);
#endif
		tmp = find_copper_inst(cp, CI_MOVE(R_DIWSTOP));
		tmp->cp.inst.operand = ((vstop & 0xff) << 8) | (hstop & 0xff);
#ifdef DEBUG
		if (aga_enable & AGA_TRACE2)
			printf(" diwstop %04x", tmp->cp.inst.operand);
#endif
		tmp = find_copper_inst(cp, CI_MOVE(R_DDFSTART));
		tmp->cp.inst.operand = ddfstart;
#ifdef DEBUG
		if (aga_enable & AGA_TRACE2)
			printf(" ddfstart %04x", tmp->cp.inst.operand);
#endif
		tmp = find_copper_inst(cp, CI_MOVE(R_DDFSTOP));
		tmp->cp.inst.operand = ddfstart + ddfwidth;
#ifdef DEBUG
		if (aga_enable & AGA_TRACE2)
			printf(" ddfstop %04x", tmp->cp.inst.operand);
#endif

		tmp = find_copper_inst(cp, CI_MOVE(R_BPL0PTH));
		for (i = 0, j = 0; i < depth; j += 2, i++) {
			/* update the plane pointers */
			tmp[j].cp.inst.operand = HIADDR(PREP_DMA_MEM(v->bitmap->plane[i]));
			tmp[j + 1].cp.inst.operand = LOADDR(PREP_DMA_MEM(v->bitmap->plane[i]));
#ifdef DEBUG
		if (aga_enable & AGA_TRACE2)
			printf (" bpl%dpth %08x", i, v->bitmap->plane[i]);
#endif
		}

		/* set mods correctly. */
		tmp = find_copper_inst(cp, CI_MOVE(R_BPL1MOD));
		tmp[0].cp.inst.operand = v->bitmap->row_mod;
		tmp[1].cp.inst.operand = v->bitmap->row_mod;
#ifdef DEBUG
		if (aga_enable & AGA_TRACE2)
			printf(" bplxmod %04x\n", v->bitmap->row_mod);
#endif

		/* set next pointers correctly */
		tmp = find_copper_inst(cp, CI_MOVE(R_COP1LCH));
		tmp[0].cp.inst.operand = HIADDR(PREP_DMA_MEM(paga_this_data->frames[F_STORE_LONG]));
		tmp[1].cp.inst.operand = LOADDR(PREP_DMA_MEM(paga_this_data->frames[F_STORE_LONG]));

		cp = paga_this_data->frames[F_LONG];
		paga_this_data->frames[F_LONG] = paga_this_data->frames[F_STORE_LONG];
		paga_this_data->frames[F_STORE_LONG] = cp;

		vd->flags |= VF_DISPLAY;

		cc_use_aga_colormap(v, vd->colormap);
	}
	cc_load_mode(paga_this);
#ifdef DEBUG
	if (aga_enable & AGA_TRACE)
		aga_enable |= AGA_TRACE2;	/* XXXX */
#endif
}

#endif /* GRF_AGA */
#endif /* GRF_PAL */
