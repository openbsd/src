/* $OpenBSD: cmd-resize-pane.c,v 1.37 2019/05/03 18:42:40 nicm Exp $ */

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

#include <stdlib.h>

#include "tmux.h"

/*
 * Increase or decrease pane size.
 */

static enum cmd_retval	cmd_resize_pane_exec(struct cmd *, struct cmdq_item *);

static void	cmd_resize_pane_mouse_update(struct client *,
		    struct mouse_event *);

const struct cmd_entry cmd_resize_pane_entry = {
	.name = "resize-pane",
	.alias = "resizep",

	.args = { "DLMRt:Ux:y:Z", 0, 1 },
	.usage = "[-DLMRUZ] [-x width] [-y height] " CMD_TARGET_PANE_USAGE " "
		 "[adjustment]",

	.target = { 't', CMD_FIND_PANE, 0 },

	.flags = CMD_AFTERHOOK,
	.exec = cmd_resize_pane_exec
};

static enum cmd_retval
cmd_resize_pane_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = self->args;
	struct cmdq_shared	*shared = item->shared;
	struct window_pane	*wp = item->target.wp;
	struct winlink		*wl = item->target.wl;
	struct window		*w = wl->window;
	struct client		*c = item->client;
	struct session		*s = item->target.s;
	const char	       	*errstr;
	char			*cause;
	u_int			 adjust;
	int			 x, y;

	if (args_has(args, 'M')) {
		if (cmd_mouse_window(&shared->mouse, &s) == NULL)
			return (CMD_RETURN_NORMAL);
		if (c == NULL || c->session != s)
			return (CMD_RETURN_NORMAL);
		c->tty.mouse_drag_update = cmd_resize_pane_mouse_update;
		cmd_resize_pane_mouse_update(c, &shared->mouse);
		return (CMD_RETURN_NORMAL);
	}

	if (args_has(args, 'Z')) {
		if (w->flags & WINDOW_ZOOMED)
			window_unzoom(w);
		else
			window_zoom(wp);
		server_redraw_window(w);
		server_status_window(w);
		return (CMD_RETURN_NORMAL);
	}
	server_unzoom_window(w);

	if (args->argc == 0)
		adjust = 1;
	else {
		adjust = strtonum(args->argv[0], 1, INT_MAX, &errstr);
		if (errstr != NULL) {
			cmdq_error(item, "adjustment %s", errstr);
			return (CMD_RETURN_ERROR);
		}
	}

	if (args_has(args, 'x')) {
		x = args_strtonum(args, 'x', PANE_MINIMUM, INT_MAX, &cause);
		if (cause != NULL) {
			cmdq_error(item, "width %s", cause);
			free(cause);
			return (CMD_RETURN_ERROR);
		}
		layout_resize_pane_to(wp, LAYOUT_LEFTRIGHT, x);
	}
	if (args_has(args, 'y')) {
		y = args_strtonum(args, 'y', PANE_MINIMUM, INT_MAX, &cause);
		if (cause != NULL) {
			cmdq_error(item, "height %s", cause);
			free(cause);
			return (CMD_RETURN_ERROR);
		}
		layout_resize_pane_to(wp, LAYOUT_TOPBOTTOM, y);
	}

	if (args_has(args, 'L'))
		layout_resize_pane(wp, LAYOUT_LEFTRIGHT, -adjust, 1);
	else if (args_has(args, 'R'))
		layout_resize_pane(wp, LAYOUT_LEFTRIGHT, adjust, 1);
	else if (args_has(args, 'U'))
		layout_resize_pane(wp, LAYOUT_TOPBOTTOM, -adjust, 1);
	else if (args_has(args, 'D'))
		layout_resize_pane(wp, LAYOUT_TOPBOTTOM, adjust, 1);
	server_redraw_window(wl->window);

	return (CMD_RETURN_NORMAL);
}

static void
cmd_resize_pane_mouse_update(struct client *c, struct mouse_event *m)
{
	struct winlink		*wl;
	struct window		*w;
	u_int			 y, ly, x, lx;
	static const int         offsets[][2] = {
	    { 0, 0 }, { 0, 1 }, { 1, 0 }, { 0, -1 }, { -1, 0 },
	};
	struct layout_cell	*cells[nitems(offsets)], *lc;
	u_int			 ncells = 0, i, j, resizes = 0;
	enum layout_type	 type;

	wl = cmd_mouse_window(m, NULL);
	if (wl == NULL) {
		c->tty.mouse_drag_update = NULL;
		return;
	}
	w = wl->window;

	y = m->y + m->oy; x = m->x + m->ox;
	if (m->statusat == 0 && y > 0)
		y--;
	else if (m->statusat > 0 && y >= (u_int)m->statusat)
		y = m->statusat - 1;
	ly = m->ly + m->oy; lx = m->lx + m->ox;
	if (m->statusat == 0 && ly > 0)
		ly--;
	else if (m->statusat > 0 && ly >= (u_int)m->statusat)
		ly = m->statusat - 1;

	for (i = 0; i < nitems(cells); i++) {
		lc = layout_search_by_border(w->layout_root, lx + offsets[i][0],
		    ly + offsets[i][1]);
		if (lc == NULL)
			continue;

		for (j = 0; j < ncells; j++) {
			if (cells[j] == lc) {
				lc = NULL;
				break;
			}
		}
		if (lc == NULL)
			continue;

		cells[ncells] = lc;
		ncells++;
	}
	if (ncells == 0)
		return;

	for (i = 0; i < ncells; i++) {
		type = cells[i]->parent->type;
		if (y != ly && type == LAYOUT_TOPBOTTOM) {
			layout_resize_layout(w, cells[i], type, y - ly, 0);
			resizes++;
		} else if (x != lx && type == LAYOUT_LEFTRIGHT) {
			layout_resize_layout(w, cells[i], type, x - lx, 0);
			resizes++;
		}
	}
	if (resizes != 0)
		server_redraw_window(w);
}
