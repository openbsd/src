/* $OpenBSD: cmd-select-pane.c,v 1.50 2019/04/30 06:21:30 nicm Exp $ */

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
#include <string.h>

#include "tmux.h"

/*
 * Select pane.
 */

static enum cmd_retval	cmd_select_pane_exec(struct cmd *, struct cmdq_item *);

const struct cmd_entry cmd_select_pane_entry = {
	.name = "select-pane",
	.alias = "selectp",

	.args = { "DdegLlMmP:RT:t:U", 0, 0 },
	.usage = "[-DdegLlMmRU] [-P style] [-T title] " CMD_TARGET_PANE_USAGE,

	.target = { 't', CMD_FIND_PANE, 0 },

	.flags = 0,
	.exec = cmd_select_pane_exec
};

const struct cmd_entry cmd_last_pane_entry = {
	.name = "last-pane",
	.alias = "lastp",

	.args = { "det:", 0, 0 },
	.usage = "[-de] " CMD_TARGET_WINDOW_USAGE,

	.target = { 't', CMD_FIND_WINDOW, 0 },

	.flags = 0,
	.exec = cmd_select_pane_exec
};

static void
cmd_select_pane_redraw(struct window *w)
{
	struct client	*c;

	/*
	 * Redraw entire window if it is bigger than the client (the
	 * offset may change), otherwise just draw borders.
	 */

	TAILQ_FOREACH(c, &clients, entry) {
		if (c->session == NULL || (c->flags & CLIENT_CONTROL))
			continue;
		if (c->session->curw->window == w && tty_window_bigger(&c->tty))
			server_redraw_client(c);
		else {
			if (c->session->curw->window == w)
				c->flags |= CLIENT_REDRAWBORDERS;
			if (session_has(c->session, w))
				c->flags |= CLIENT_REDRAWSTATUS;
		}

	}
}

static enum cmd_retval
cmd_select_pane_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = self->args;
	struct cmd_find_state	*current = &item->shared->current;
	struct client		*c = cmd_find_client(item, NULL, 1);
	struct winlink		*wl = item->target.wl;
	struct window		*w = wl->window;
	struct session		*s = item->target.s;
	struct window_pane	*wp = item->target.wp, *lastwp, *markedwp;
	struct style		*sy = &wp->style;
	char			*pane_title;
	const char		*style;

	if (self->entry == &cmd_last_pane_entry || args_has(args, 'l')) {
		lastwp = w->last;
		if (lastwp == NULL && window_count_panes(w) == 2) {
			lastwp = TAILQ_PREV(w->active, window_panes, entry);
			if (lastwp == NULL)
				lastwp = TAILQ_NEXT(w->active, entry);
		}
		if (lastwp == NULL) {
			cmdq_error(item, "no last pane");
			return (CMD_RETURN_ERROR);
		}
		if (args_has(self->args, 'e'))
			lastwp->flags &= ~PANE_INPUTOFF;
		else if (args_has(self->args, 'd'))
			lastwp->flags |= PANE_INPUTOFF;
		else {
			server_unzoom_window(w);
			window_redraw_active_switch(w, lastwp);
			if (window_set_active_pane(w, lastwp, 1)) {
				cmd_find_from_winlink(current, wl, 0);
				cmd_select_pane_redraw(w);
			}
		}
		return (CMD_RETURN_NORMAL);
	}

	if (args_has(args, 'm') || args_has(args, 'M')) {
		if (args_has(args, 'm') && !window_pane_visible(wp))
			return (CMD_RETURN_NORMAL);
		lastwp = marked_pane.wp;

		if (args_has(args, 'M') || server_is_marked(s, wl, wp))
			server_clear_marked();
		else
			server_set_marked(s, wl, wp);
		markedwp = marked_pane.wp;

		if (lastwp != NULL) {
			server_redraw_window_borders(lastwp->window);
			server_status_window(lastwp->window);
		}
		if (markedwp != NULL) {
			server_redraw_window_borders(markedwp->window);
			server_status_window(markedwp->window);
		}
		return (CMD_RETURN_NORMAL);
	}

	if (args_has(self->args, 'P') || args_has(self->args, 'g')) {
		if ((style = args_get(args, 'P')) != NULL) {
			style_set(sy, &grid_default_cell);
			if (style_parse(sy, &grid_default_cell, style) == -1) {
				cmdq_error(item, "bad style: %s", style);
				return (CMD_RETURN_ERROR);
			}
			wp->flags |= PANE_REDRAW;
		}
		if (args_has(self->args, 'g'))
			cmdq_print(item, "%s", style_tostring(sy));
		return (CMD_RETURN_NORMAL);
	}

	if (args_has(self->args, 'L')) {
		server_unzoom_window(wp->window);
		wp = window_pane_find_left(wp);
	} else if (args_has(self->args, 'R')) {
		server_unzoom_window(wp->window);
		wp = window_pane_find_right(wp);
	} else if (args_has(self->args, 'U')) {
		server_unzoom_window(wp->window);
		wp = window_pane_find_up(wp);
	} else if (args_has(self->args, 'D')) {
		server_unzoom_window(wp->window);
		wp = window_pane_find_down(wp);
	}
	if (wp == NULL)
		return (CMD_RETURN_NORMAL);

	if (args_has(self->args, 'e')) {
		wp->flags &= ~PANE_INPUTOFF;
		return (CMD_RETURN_NORMAL);
	}
	if (args_has(self->args, 'd')) {
		wp->flags |= PANE_INPUTOFF;
		return (CMD_RETURN_NORMAL);
	}

	if (args_has(self->args, 'T')) {
		pane_title = format_single(item, args_get(self->args, 'T'),
		    c, s, wl, wp);
		screen_set_title(&wp->base, pane_title);
		server_status_window(wp->window);
		free(pane_title);
		return (CMD_RETURN_NORMAL);
	}

	if (wp == w->active)
		return (CMD_RETURN_NORMAL);
	server_unzoom_window(wp->window);
	window_redraw_active_switch(w, wp);
	if (window_set_active_pane(w, wp, 1)) {
		cmd_find_from_winlink_pane(current, wl, wp, 0);
		cmdq_insert_hook(s, item, current, "after-select-pane");
		cmd_select_pane_redraw(w);
	}

	return (CMD_RETURN_NORMAL);
}
