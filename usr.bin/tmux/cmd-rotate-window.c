/* $OpenBSD: cmd-rotate-window.c,v 1.25 2017/08/30 10:33:57 nicm Exp $ */

/*
 * Copyright (c) 2009 Nicholas Marriott <nicholas.marriott@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>

#include "tmux.h"

/*
 * Rotate the panes in a window.
 */

static enum cmd_retval	cmd_rotate_window_exec(struct cmd *,
			    struct cmdq_item *);

const struct cmd_entry cmd_rotate_window_entry = {
	.name = "rotate-window",
	.alias = "rotatew",

	.args = { "Dt:U", 0, 0 },
	.usage = "[-DU] " CMD_TARGET_WINDOW_USAGE,

	.target = { 't', CMD_FIND_WINDOW, 0 },

	.flags = 0,
	.exec = cmd_rotate_window_exec
};

static enum cmd_retval
cmd_rotate_window_exec(struct cmd *self, struct cmdq_item *item)
{
	struct cmd_find_state	*current = &item->shared->current;
	struct winlink		*wl = item->target.wl;
	struct window		*w = wl->window;
	struct window_pane	*wp, *wp2;
	struct layout_cell	*lc;
	u_int			 sx, sy, xoff, yoff;

	server_unzoom_window(w);

	if (args_has(self->args, 'D')) {
		wp = TAILQ_LAST(&w->panes, window_panes);
		TAILQ_REMOVE(&w->panes, wp, entry);
		TAILQ_INSERT_HEAD(&w->panes, wp, entry);

		lc = wp->layout_cell;
		xoff = wp->xoff; yoff = wp->yoff;
		sx = wp->sx; sy = wp->sy;
		TAILQ_FOREACH(wp, &w->panes, entry) {
			if ((wp2 = TAILQ_NEXT(wp, entry)) == NULL)
				break;
			wp->layout_cell = wp2->layout_cell;
			if (wp->layout_cell != NULL)
				wp->layout_cell->wp = wp;
			wp->xoff = wp2->xoff; wp->yoff = wp2->yoff;
			window_pane_resize(wp, wp2->sx, wp2->sy);
		}
		wp->layout_cell = lc;
		if (wp->layout_cell != NULL)
			wp->layout_cell->wp = wp;
		wp->xoff = xoff; wp->yoff = yoff;
		window_pane_resize(wp, sx, sy);

		if ((wp = TAILQ_PREV(w->active, window_panes, entry)) == NULL)
			wp = TAILQ_LAST(&w->panes, window_panes);
		window_set_active_pane(w, wp);
		cmd_find_from_winlink_pane(current, wl, wp, 0);
		server_redraw_window(w);
	} else {
		wp = TAILQ_FIRST(&w->panes);
		TAILQ_REMOVE(&w->panes, wp, entry);
		TAILQ_INSERT_TAIL(&w->panes, wp, entry);

		lc = wp->layout_cell;
		xoff = wp->xoff; yoff = wp->yoff;
		sx = wp->sx; sy = wp->sy;
		TAILQ_FOREACH_REVERSE(wp, &w->panes, window_panes, entry) {
			if ((wp2 = TAILQ_PREV(wp, window_panes, entry)) == NULL)
				break;
			wp->layout_cell = wp2->layout_cell;
			if (wp->layout_cell != NULL)
				wp->layout_cell->wp = wp;
			wp->xoff = wp2->xoff; wp->yoff = wp2->yoff;
			window_pane_resize(wp, wp2->sx, wp2->sy);
		}
		wp->layout_cell = lc;
		if (wp->layout_cell != NULL)
			wp->layout_cell->wp = wp;
		wp->xoff = xoff; wp->yoff = yoff;
		window_pane_resize(wp, sx, sy);

		if ((wp = TAILQ_NEXT(w->active, entry)) == NULL)
			wp = TAILQ_FIRST(&w->panes);
		window_set_active_pane(w, wp);
		cmd_find_from_winlink_pane(current, wl, wp, 0);
		server_redraw_window(w);
	}

	return (CMD_RETURN_NORMAL);
}
