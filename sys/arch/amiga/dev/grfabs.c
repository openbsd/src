/*	$NetBSD: grfabs.c,v 1.4 1994/10/26 02:03:21 cgd Exp $	*/

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
 *  amiga abstract graphics driver.
 */
#include <sys/param.h>
#include <sys/queue.h>

#include <amiga/amiga/color.h>
#include <amiga/amiga/cc.h>
#include <amiga/dev/grfabs_reg.h>

/*
 * General and init.
 */

/* add your monitor here. */
monitor_t *cc_init_monitor (void);

/* and here. */
monitor_t *(*init_monitor[])(void) = {
    cc_init_monitor, 
    NULL
};

struct monitor_list instance_monitors, *monitors;

struct vbl_node grf_vbl_node;

#define ABS(type, val) \
    (type) (((int)(val) < 0) ? -(val) : (val))

void
grf_vbl_function(data)
	void *data;
{
	monitor_t *m;

	if (monitors == NULL)
		return;

	for (m = monitors->lh_first; m != NULL; m = m->link.le_next) {
		if (m->vbl_handler) 
			m->vbl_handler(m);
	}
}

/*
 * XXX: called from ite console init routine.
 * Does just what configure will do later but without printing anything.
 */

int
grfcc_probe()
{
	int i = 0;
	
	grf_vbl_node.function = grf_vbl_function;
    
	if (NULL == monitors) {
		LIST_INIT(&instance_monitors);
		monitors = &instance_monitors;
    
		while (init_monitor[i]) {
			init_monitor[i] ();
			i++;
		}
		if (i) {
			add_vbl_function(&grf_vbl_node, 1, 0);
			return(1);
		}
		return(0);
	}
	return(1);
}

dmode_t *
get_best_display_mode(width, height, depth)
	u_long width, height;
	u_char depth;
{
	monitor_t *m;
	dmode_t *d, *save;
	dimen_t dim;
	long dx, dy, ct, dt;
 
	save = NULL;
	for (m = monitors->lh_first; m != NULL; m = m->link.le_next) {
		dim.width = width;
		dim.height = height;
		d = m->get_best_mode(&dim, depth);
		if (d) {
			dx = ABS(long, (d->nominal_size.width - width));
			dy = ABS(long, (d->nominal_size.height - height));
			ct = dx + dy;

			if (ct < dt || save == NULL) {
				save = d;
				dt = ct;
			}
		}	
	}
	return(save);
}


/*
 * Monitor stuff.
 */

dmode_t *
grf_get_next_mode(m, d)
	monitor_t *m;
	dmode_t *d;
{
	return(m->get_next_mode(d));
}

dmode_t *
grf_get_current_mode(m)
	monitor_t *m;
{
	return(m->get_current_mode());
}

dmode_t *
grf_get_best_mode(m, size, depth)
	monitor_t *m;
	dimen_t *size;
	u_char depth;
{
	return(m->get_best_mode(size, depth));
}

bmap_t *
grf_alloc_bitmap(m, w, h, d, f)
	monitor_t *m;
	u_short w, h, d, f;
{
	return(m->alloc_bitmap(w, h, d, f));
}

void
grf_free_bitmap(m, bm)
	monitor_t *m;
	bmap_t *bm;
{
	m->free_bitmap(bm);
}

/*
 * Mode stuff.
 */

view_t *
grf_get_current_view(d)
	dmode_t *d;
{
	return(d->get_current_view(d));
}

monitor_t *
grf_get_monitor(d)
	dmode_t *d;
{
	return(d->get_monitor(d));
}

/*
 * View stuff.
 */

void
grf_display_view(v)
	view_t *v;
{
	v->display_view(v);
}

view_t *
grf_alloc_view(d, dim, depth)
	dmode_t *d;
	dimen_t *dim;
	u_char depth;
{
	if (!d)
		d = get_best_display_mode(dim->width, dim->height, depth);
	if (d) 
		return(d->alloc_view(d, dim, depth));
	return(NULL);
}

void
grf_remove_view(v)
	view_t *v;
{
	v->remove_view(v);
}

void
grf_free_view(v)
	view_t *v;
{
	v->free_view(v);
}

dmode_t *
grf_get_display_mode(v)
	view_t *v;
{
	return(v->get_display_mode(v));
}

int
grf_get_colormap(v, cm)
	view_t *v;
	colormap_t *cm;
{
	return(v->get_colormap(v, cm));
}

int
grf_use_colormap(v, cm)
	view_t *v;
	colormap_t *cm;
{
	return(v->use_colormap(v, cm));
}
