/* $OpenBSD: cmd-display-message.c,v 1.50 2019/05/30 20:54:03 nicm Exp $ */

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

	.args = { "ac:Ipt:F:v", 0, 1 },
	.usage = "[-aIpv] [-c target-client] [-F format] "
		 CMD_TARGET_PANE_USAGE " [message]",

	.target = { 't', CMD_FIND_PANE, 0 },

	.flags = CMD_AFTERHOOK,
	.exec = cmd_display_message_exec
};

static void
cmd_display_message_each(const char *key, const char *value, void *arg)
{
	struct cmdq_item	*item = arg;

	cmdq_print(item, "%s=%s", key, value);
}

static enum cmd_retval
cmd_display_message_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = self->args;
	struct client		*c, *target_c;
	struct session		*s = item->target.s;
	struct winlink		*wl = item->target.wl;
	struct window_pane	*wp = item->target.wp;
	const char		*template;
	char			*msg, *cause;
	struct format_tree	*ft;
	int			 flags;

	if (args_has(args, 'I')) {
		if (window_pane_start_input(wp, item, &cause) != 0) {
			cmdq_error(item, "%s", cause);
			free(cause);
			return (CMD_RETURN_ERROR);
		}
		return (CMD_RETURN_WAIT);
	}

	if (args_has(args, 'F') && args->argc != 0) {
		cmdq_error(item, "only one of -F or argument must be given");
		return (CMD_RETURN_ERROR);
	}

	template = args_get(args, 'F');
	if (args->argc != 0)
		template = args->argv[0];
	if (template == NULL)
		template = DISPLAY_MESSAGE_TEMPLATE;

	/*
	 * -c is intended to be the client where the message should be
	 * displayed if -p is not given. But it makes sense to use it for the
	 * formats too, assuming it matches the session. If it doesn't, use the
	 * best client for the session.
	 */
	c = cmd_find_client(item, args_get(args, 'c'), 1);
	if (c != NULL && c->session == s)
		target_c = c;
	else
		target_c = cmd_find_best_client(s);
	if (args_has(self->args, 'v'))
		flags = FORMAT_VERBOSE;
	else
		flags = 0;
	ft = format_create(item->client, item, FORMAT_NONE, flags);
	format_defaults(ft, target_c, s, wl, wp);

	if (args_has(args, 'a')) {
		format_each(ft, cmd_display_message_each, item);
		return (CMD_RETURN_NORMAL);
	}

	msg = format_expand_time(ft, template);
	if (args_has(self->args, 'p'))
		cmdq_print(item, "%s", msg);
	else if (c != NULL)
		status_message_set(c, "%s", msg);
	free(msg);

	format_free(ft);

	return (CMD_RETURN_NORMAL);
}
