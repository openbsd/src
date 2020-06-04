/* $OpenBSD: cmd-find-window.c,v 1.50 2020/05/29 13:42:13 nicm Exp $ */

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
 * Find window containing text.
 */

static enum cmd_retval	cmd_find_window_exec(struct cmd *, struct cmdq_item *);

const struct cmd_entry cmd_find_window_entry = {
	.name = "find-window",
	.alias = "findw",

	.args = { "CiNrt:TZ", 1, 1 },
	.usage = "[-CiNrTZ] " CMD_TARGET_PANE_USAGE " match-string",

	.target = { 't', CMD_FIND_PANE, 0 },

	.flags = 0,
	.exec = cmd_find_window_exec
};

static enum cmd_retval
cmd_find_window_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = cmd_get_args(self), *new_args;
	struct cmd_find_state	*target = cmdq_get_target(item);
	struct window_pane	*wp = target->wp;
	const char		*s = args->argv[0], *suffix = "";
	char			*filter, *argv = { NULL };
	int			 C, N, T;

	C = args_has(args, 'C');
	N = args_has(args, 'N');
	T = args_has(args, 'T');

	if (args_has(args, 'r') && args_has(args, 'i'))
		suffix = "/ri";
	else if (args_has(args, 'r'))
		suffix = "/r";
	else if (args_has(args, 'i'))
		suffix = "/i";

	if (!C && !N && !T)
		C = N = T = 1;

	if (C && N && T) {
		xasprintf(&filter,
		    "#{||:"
		    "#{C%s:%s},#{||:#{m%s:*%s*,#{window_name}},"
		    "#{m%s:*%s*,#{pane_title}}}}",
		    suffix, s, suffix, s, suffix, s);
	} else if (C && N) {
		xasprintf(&filter,
		    "#{||:#{C%s:%s},#{m%s:*%s*,#{window_name}}}",
		    suffix, s, suffix, s);
	} else if (C && T) {
		xasprintf(&filter,
		    "#{||:#{C%s:%s},#{m%s:*%s*,#{pane_title}}}",
		    suffix, s, suffix, s);
	} else if (N && T) {
		xasprintf(&filter,
		    "#{||:#{m%s:*%s*,#{window_name}},"
		    "#{m%s:*%s*,#{pane_title}}}",
		    suffix, s, suffix, s);
	} else if (C)
		xasprintf(&filter, "#{C%s:%s}", suffix, s);
	else if (N)
		xasprintf(&filter, "#{m%s:*%s*,#{window_name}}", suffix, s);
	else
		xasprintf(&filter, "#{m%s:*%s*,#{pane_title}}", suffix, s);

	new_args = args_parse("", 1, &argv);
	if (args_has(args, 'Z'))
		args_set(new_args, 'Z', NULL);
	args_set(new_args, 'f', filter);

	window_pane_set_mode(wp, NULL, &window_tree_mode, target, new_args);

	args_free(new_args);
	free(filter);

	return (CMD_RETURN_NORMAL);
}
