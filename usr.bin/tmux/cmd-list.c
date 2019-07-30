/* $OpenBSD: cmd-list.c,v 1.16 2016/01/19 15:59:12 nicm Exp $ */

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

struct cmd_list *
cmd_list_parse(int argc, char **argv, const char *file, u_int line,
    char **cause)
{
	struct cmd_list	*cmdlist;
	struct cmd	*cmd;
	int		 i, lastsplit;
	size_t		 arglen, new_argc;
	char	       **copy_argv, **new_argv;

	copy_argv = cmd_copy_argv(argc, argv);

	cmdlist = xcalloc(1, sizeof *cmdlist);
	cmdlist->references = 1;
	TAILQ_INIT(&cmdlist->list);

	lastsplit = 0;
	for (i = 0; i < argc; i++) {
		arglen = strlen(copy_argv[i]);
		if (arglen == 0 || copy_argv[i][arglen - 1] != ';')
			continue;
		copy_argv[i][arglen - 1] = '\0';

		if (arglen > 1 && copy_argv[i][arglen - 2] == '\\') {
			copy_argv[i][arglen - 2] = ';';
			continue;
		}

		new_argc = i - lastsplit;
		new_argv = copy_argv + lastsplit;
		if (arglen != 1)
			new_argc++;

		cmd = cmd_parse(new_argc, new_argv, file, line, cause);
		if (cmd == NULL)
			goto bad;
		TAILQ_INSERT_TAIL(&cmdlist->list, cmd, qentry);

		lastsplit = i + 1;
	}

	if (lastsplit != argc) {
		cmd = cmd_parse(argc - lastsplit, copy_argv + lastsplit,
		    file, line, cause);
		if (cmd == NULL)
			goto bad;
		TAILQ_INSERT_TAIL(&cmdlist->list, cmd, qentry);
	}

	cmd_free_argv(argc, copy_argv);
	return (cmdlist);

bad:
	cmd_list_free(cmdlist);
	cmd_free_argv(argc, copy_argv);
	return (NULL);
}

void
cmd_list_free(struct cmd_list *cmdlist)
{
	struct cmd	*cmd, *cmd1;

	if (--cmdlist->references != 0)
		return;

	TAILQ_FOREACH_SAFE(cmd, &cmdlist->list, qentry, cmd1) {
		TAILQ_REMOVE(&cmdlist->list, cmd, qentry);
		args_free(cmd->args);
		free(cmd->file);
		free(cmd);
	}

	free(cmdlist);
}

char *
cmd_list_print(struct cmd_list *cmdlist)
{
	struct cmd	*cmd;
	char		*buf, *this;
	size_t		 len;

	len = 1;
	buf = xcalloc(1, len);

	TAILQ_FOREACH(cmd, &cmdlist->list, qentry) {
		this = cmd_print(cmd);

		len += strlen(this) + 3;
		buf = xrealloc(buf, len);

		strlcat(buf, this, len);
		if (TAILQ_NEXT(cmd, qentry) != NULL)
			strlcat(buf, " ; ", len);

		free(this);
	}

	return (buf);
}
