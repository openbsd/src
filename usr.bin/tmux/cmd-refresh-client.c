/* $OpenBSD: cmd-refresh-client.c,v 1.26 2017/05/28 19:00:52 nicm Exp $ */

/*
 * Copyright (c) 2007 Nicholas Marriott <nicholas.marriott@gmail.com>
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
 * Refresh client.
 */

static enum cmd_retval	cmd_refresh_client_exec(struct cmd *,
			    struct cmdq_item *);

const struct cmd_entry cmd_refresh_client_entry = {
	.name = "refresh-client",
	.alias = "refresh",

	.args = { "C:St:", 0, 0 },
	.usage = "[-S] [-C size] " CMD_TARGET_CLIENT_USAGE,

	.flags = CMD_AFTERHOOK,
	.exec = cmd_refresh_client_exec
};

static enum cmd_retval
cmd_refresh_client_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args	*args = self->args;
	struct client	*c;
	const char	*size;
	u_int		 w, h;

	if ((c = cmd_find_client(item, args_get(args, 't'), 0)) == NULL)
		return (CMD_RETURN_ERROR);

	if (args_has(args, 'C')) {
		if ((size = args_get(args, 'C')) == NULL) {
			cmdq_error(item, "missing size");
			return (CMD_RETURN_ERROR);
		}
		if (sscanf(size, "%u,%u", &w, &h) != 2) {
			cmdq_error(item, "bad size argument");
			return (CMD_RETURN_ERROR);
		}
		if (w < PANE_MINIMUM || w > 5000 ||
		    h < PANE_MINIMUM || h > 5000) {
			cmdq_error(item, "size too small or too big");
			return (CMD_RETURN_ERROR);
		}
		if (!(c->flags & CLIENT_CONTROL)) {
			cmdq_error(item, "not a control client");
			return (CMD_RETURN_ERROR);
		}
		tty_set_size(&c->tty, w, h);
		c->flags |= CLIENT_SIZECHANGED;
		recalculate_sizes();
	} else if (args_has(args, 'S')) {
		c->flags |= CLIENT_STATUSFORCE;
		server_status_client(c);
	} else {
		c->flags |= CLIENT_STATUSFORCE;
		server_redraw_client(c);
	}

	return (CMD_RETURN_NORMAL);
}
