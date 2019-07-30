/* $OpenBSD: cmd-display-message.c,v 1.42 2017/05/01 12:20:55 nicm Exp $ */

/*
 * Copyright (c) 2009 Tiago Cunha <me@tiagocunha.org>
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
#include <time.h>

#include "tmux.h"

/*
 * Displays a message in the status line.
 */

#define DISPLAY_MESSAGE_TEMPLATE			\
	"[#{session_name}] #{window_index}:"		\
	"#{window_name}, current pane #{pane_index} "	\
	"- (%H:%M %d-%b-%y)"

static enum cmd_retval	cmd_display_message_exec(struct cmd *,
			    struct cmdq_item *);

const struct cmd_entry cmd_display_message_entry = {
	.name = "display-message",
	.alias = "display",

	.args = { "c:pt:F:", 0, 1 },
	.usage = "[-p] [-c target-client] [-F format] "
		 CMD_TARGET_PANE_USAGE " [message]",

	.target = { 't', CMD_FIND_PANE, 0 },

	.flags = CMD_AFTERHOOK,
	.exec = cmd_display_message_exec
};

static enum cmd_retval
cmd_display_message_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = self->args;
	struct client		*c;
	struct session		*s = item->target.s;
	struct winlink		*wl = item->target.wl;
	struct window_pane	*wp = item->target.wp;
	const char		*template;
	char			*msg;
	struct format_tree	*ft;

	if (args_has(args, 'F') && args->argc != 0) {
		cmdq_error(item, "only one of -F or argument must be given");
		return (CMD_RETURN_ERROR);
	}
	c = cmd_find_client(item, args_get(args, 'c'), 1);

	template = args_get(args, 'F');
	if (args->argc != 0)
		template = args->argv[0];
	if (template == NULL)
		template = DISPLAY_MESSAGE_TEMPLATE;

	ft = format_create(item->client, item, FORMAT_NONE, 0);
	format_defaults(ft, c, s, wl, wp);

	msg = format_expand_time(ft, template, time(NULL));
	if (args_has(self->args, 'p'))
		cmdq_print(item, "%s", msg);
	else if (c != NULL)
		status_message_set(c, "%s", msg);
	free(msg);

	format_free(ft);

	return (CMD_RETURN_NORMAL);
}
