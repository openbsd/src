/*	$NetBSD: grfabs.c,v 1.7 1995/09/23 20:29:16 leo Exp $	*/

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

/*
 *  atari abstract graphics driver.
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/queue.h>
#include <sys/malloc.h>

#include <machine/cpu.h>
#include <machine/iomap.h>
#include <machine/video.h>
#include <machine/mfp.h>
#include <atari/dev/grfabs_reg.h>

/*
 * Function decls
 */
static dmode_t    *get_best_display_mode __P((dimen_t *, int, dmode_t *));

/*
 * List of available graphic modes
 */
static MODES modes;

/*
 * Ugh.. Stuff needed to allocate console structures before the VM-system
 * is running. There is no malloc() available at that time.
 * Decision to use these: atari_realconfig == 0
 */
view_t		gra_con_view;
colormap_t	gra_con_cmap;
long		gra_con_colors[MAX_CENTRIES];

/*
 * Default colors.....
 * Currently the TT-low (256 colors) just uses 16 times the 16-color default.
 * If you have a sensible 256 scale, feel free to add.....
 * The first 2 colors in all maps are {black,white}, so ite (text) displays
 * are initially readable. Also, this enables me to supply only 1 map. The
 * 4 color mode uses the first four entries of the 16 color mode thus giving
 * a gray scale display. (Maybe we can add an intensity bit to the ite...)
 */
u_long gra_def_color16[16] = {
	0x00000000,	/* black		*/
	0x00ffffff,	/* white		*/
	0x000c0c0c,	/* light gray		*/
	0x00808008,	/* gray			*/
	0x0000000c,	/* blue			*/
	0x00000c00,	/* green		*/
	0x00000c0c,	/* cyan			*/
	0x00c00000,	/* red			*/
	0x00c0000c,	/* magenta		*/
	0x00c00c00,	/* brown		*/
	0x000000ff,	/* light blue		*/
	0x0000ff00,	/* light green		*/
	0x0000ffff,	/* light cyan		*/
	0x00ff0000,	/* light red		*/
	0x00ff00ff,	/* light magenta	*/
	0x00ffff00	/* light brown		*/
};

/*
 * XXX: called from ite console init routine.
 * Initialize list of posible video modes.
 */
int
grfabs_probe()
{
	static int	inited = 0;

	if (inited)
		return (1);	/* Has to be done only once */
	inited++;

	LIST_INIT(&modes);

#ifdef FALCON_VIDEO
	if (machineid & ATARI_FALCON)
		falcon_probe_video(&modes);
#endif /* FALCON_VIDEO */
#ifdef TT_VIDEO
	if (machineid & ATARI_TT)
		tt_probe_video(&modes);
#endif /* TT_VIDEO */
	return ((modes.lh_first == NULL) ? 0 : 1);
}

view_t *
grf_alloc_view(d, dim, depth)
dmode_t	*d;
dimen_t	*dim;
u_char	depth;
{
	if (!d)
		d = get_best_display_mode(dim, depth, NULL);
	if (d)
		return ((d->grfabs_funcs->alloc_view)(d, dim, depth));
	return(NULL);
}

dmode_t	*
grf_get_best_mode(dim, depth)
dimen_t	*dim;
u_char	depth;
{
	return (get_best_display_mode(dim, depth, NULL));
}

void grf_display_view(v)
view_t *v;
{
	(v->mode->grfabs_funcs->display_view)(v);
}

void grf_remove_view(v)
view_t *v;
{
	(v->mode->grfabs_funcs->remove_view)(v);
}

void grf_free_view(v)
view_t *v;
{
	(v->mode->grfabs_funcs->free_view)(v);
}

int
grf_get_colormap(v, cm)
view_t		*v;
colormap_t	*cm;
{
	colormap_t	*gcm;
	int		i, n;
	u_long		*sv_entry;

	gcm = v->colormap;
	n   = cm->size < gcm->size ? cm->size : gcm->size;

	/*
	 * Copy struct from view but be carefull to keep 'entry'
	 */
	sv_entry = cm->entry;
	*cm = *gcm;
	cm->entry = sv_entry;

	/*
	 * Copy the colors
	 */
	bzero(cm->entry, cm->size * sizeof(long));
	for (i = 0; i < n; i++)
		cm->entry[i] = gcm->entry[i];
	return (0);
}

int
grf_use_colormap(v, cm)
view_t		*v;
colormap_t	*cm;
{
	return (v->mode->grfabs_funcs->use_colormap)(v, cm);
}

static dmode_t *
get_best_display_mode(dim, depth, curr_mode)
int	depth;
dimen_t	*dim;
dmode_t	*curr_mode;
{
	dmode_t		*save;
	dmode_t		*dm;
	long   		dt, dx, dy, ct;

	save = NULL;
	dm = modes.lh_first;
	while (dm != NULL) {
		dx = abs(dm->size.width  - dim->width);
		dy = abs(dm->size.height - dim->height);
		ct = dx + dy;

		if (ct < dt || save == NULL) {
			save = dm;
			dt = ct;
		}
		dm = dm->link.le_next;
	}
	/*
	 * Did we do better than the current mode?
	 */
	if ((save != NULL) && (curr_mode != NULL)) {
		dx = abs(curr_mode->size.width  - dim->width);
		dy = abs(curr_mode->size.height - dim->height);
		ct = dx + dy;
		if (ct <= dt)
			return (NULL);
	}
	return (save);
}
