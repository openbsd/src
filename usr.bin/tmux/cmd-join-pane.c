/* $OpenBSD: cmd-join-pane.c,v 1.34 2019/04/17 14:37:48 nicm Exp $ */

/*
 * Copyright (c) 2011 George Nachman <tmux@georgester.com>
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

#include <paths.h>
#include <stdlib.h>
#include <unistd.h>

#include "tmux.h"

/*
 * Join or move a pane into another (like split/swap/kill).
 */

static enum cmd_retval	cmd_join_pane_exec(struct cmd *, struct cmdq_item *);

const struct cmd_entry cmd_join_pane_entry = {
	.name = "join-pane",
	.alias = "joinp",

	.args = { "bdhvp:l:s:t:", 0, 0 },
	.usage = "[-bdhv] [-p percentage|-l size] " CMD_SRCDST_PANE_USAGE,

	.source = { 's', CMD_FIND_PANE, CMD_FIND_DEFAULT_MARKED },
	.target = { 't', CMD_FIND_PANE, 0 },

	.flags = 0,
	.exec = cmd_join_pane_exec
};

const struct cmd_entry cmd_move_pane_entry = {
	.name = "move-pane",
	.alias = "movep",

	.args = { "bdhvp:l:s:t:", 0, 0 },
	.usage = "[-bdhv] [-p percentage|-l size] " CMD_SRCDST_PANE_USAGE,

	.source = { 's', CMD_FIND_PANE, 0 },
	.target = { 't', CMD_FIND_PANE, 0 },

	.flags = 0,
	.exec = cmd_join_pane_exec
};

static enum cmd_retval
cmd_join_pane_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = self->args;
	struct cmd_find_state	*current = &item->shared->current;
	struct session		*dst_s;
	struct winlink		*src_wl, *dst_wl;
	struct window		*src_w, *dst_w;
	struct window_pane	*src_wp, *dst_wp;
	char			*cause;
	int			 size, percentage, dst_idx;
	enum layout_type	 type;
	struct layout_cell	*lc;
	int			 not_same_window, flags;

	if (self->entry == &cmd_join_pane_entry)
		not_same_window = 1;
	else
		not_same_window = 0;

	dst_s = item->target.s;
	dst_wl = item->target.wl;
	dst_wp = item->target.wp;
	dst_w = dst_wl->window;
	dst_idx = dst_wl->idx;
	server_unzoom_window(dst_w);

	src_wl = item->source.wl;
	src_wp = item->source.wp;
	src_w = src_wl->window;
	server_unzoom_window(src_w);

	if (not_same_window && src_w == dst_w) {
		cmdq_error(item, "can't join a pane to its own window");
		return (CMD_RETURN_ERROR);
	}
	if (!not_same_window && src_wp == dst_wp) {
		cmdq_error(item, "source and target panes must be different");
		return (CMD_RETURN_ERROR);
	}

	type = LAYOUT_TOPBOTTOM;
	if (args_has(args, 'h'))
		type = LAYOUT_LEFTRIGHT;

	size = -1;
	if (args_has(args, 'l')) {
		size = args_strtonum(args, 'l', 0, INT_MAX, &cause);
		if (cause != NULL) {
			cmdq_error(item, "size %s", cause);
			free(cause);
			return (CMD_RETURN_ERROR);
		}
	} else if (args_has(args, 'p')) {
		percentage = args_strtonum(args, 'p', 0, 100, &cause);
		if (cause != NULL) {
			cmdq_error(item, "percentage %s", cause);
			free(cause);
			return (CMD_RETURN_ERROR);
		}
		if (type == LAYOUT_TOPBOTTOM)
			size = (dst_wp->sy * percentage) / 100;
		else
			size = (dst_wp->sx * percentage) / 100;
	}
	if (args_has(args, 'b'))
		flags = SPAWN_BEFORE;
	else
		flags = 0;
	lc = layout_split_pane(dst_wp, type, size, flags);
	if (lc == NULL) {
		cmdq_error(item, "create pane failed: pane too small");
		return (CMD_RETURN_ERROR);
	}

	layout_close_pane(src_wp);

	window_lost_pane(src_w, src_wp);
	TAILQ_REMOVE(&src_w->panes, src_wp, entry);

	src_wp->window = dst_w;
	TAILQ_INSERT_AFTER(&dst_w->panes, dst_wp, src_wp, entry);
	layout_assign_pane(lc, src_wp);

	recalculate_sizes();

	server_redraw_window(src_w);
	server_redraw_window(dst_w);

	if (!args_has(args, 'd')) {
		window_set_active_pane(dst_w, src_wp, 1);
		session_select(dst_s, dst_idx);
		cmd_find_from_session(current, dst_s, 0);
		server_redraw_session(dst_s);
	} else
		server_status_session(dst_s);

	if (window_count_panes(src_w) == 0)
		server_kill_window(src_w);
	else
		notify_window("window-layout-changed", src_w);
	notify_window("window-layout-changed", dst_w);

	return (CMD_RETURN_NORMAL);
}
