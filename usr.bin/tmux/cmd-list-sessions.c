/* $OpenBSD: cmd-list-sessions.c,v 1.30 2018/10/18 08:38:01 nicm Exp $ */

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

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "tmux.h"

/*
 * List all sessions.
 */

#define LIST_SESSIONS_TEMPLATE				\
	"#{session_name}: #{session_windows} windows "	\
	"(created #{t:session_created})"		\
	"#{?session_grouped, (group ,}"			\
	"#{session_group}#{?session_grouped,),}"	\
	"#{?session_attached, (attached),}"

static enum cmd_retval	cmd_list_sessions_exec(struct cmd *,
			    struct cmdq_item *);

const struct cmd_entry cmd_list_sessions_entry = {
	.name = "list-sessions",
	.alias = "ls",

	.args = { "F:", 0, 0 },
	.usage = "[-F format]",

	.flags = CMD_AFTERHOOK,
	.exec = cmd_list_sessions_exec
};

static enum cmd_retval
cmd_list_sessions_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = self->args;
	struct session		*s;
	u_int		 	 n;
	struct format_tree	*ft;
	const char		*template;
	char			*line;

	if ((template = args_get(args, 'F')) == NULL)
		template = LIST_SESSIONS_TEMPLATE;

	n = 0;
	RB_FOREACH(s, sessions, &sessions) {
		ft = format_create(item->client, item, FORMAT_NONE, 0);
		format_add(ft, "line", "%u", n);
		format_defaults(ft, NULL, s, NULL, NULL);

		line = format_expand(ft, template);
		cmdq_print(item, "%s", line);
		free(line);

		format_free(ft);
		n++;
	}

	return (CMD_RETURN_NORMAL);
}
