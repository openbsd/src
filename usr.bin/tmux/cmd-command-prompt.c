/* $OpenBSD: cmd-command-prompt.c,v 1.46 2019/05/23 11:13:30 nicm Exp $ */

/*
 * Copyright (c) 2008 Nicholas Marriott <nicholas.marriott@gmail.com>
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

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "tmux.h"

/*
 * Prompt for command in client.
 */

static enum cmd_retval	cmd_command_prompt_exec(struct cmd *,
			    struct cmdq_item *);

static int	cmd_command_prompt_callback(struct client *, void *,
		    const char *, int);
static void	cmd_command_prompt_free(void *);

const struct cmd_entry cmd_command_prompt_entry = {
	.name = "command-prompt",
	.alias = NULL,

	.args = { "1iI:Np:t:", 0, 1 },
	.usage = "[-1Ni] [-I inputs] [-p prompts] " CMD_TARGET_CLIENT_USAGE " "
		 "[template]",

	.flags = 0,
	.exec = cmd_command_prompt_exec
};

struct cmd_command_prompt_cdata {
	int	 flags;

	char	*inputs;
	char	*next_input;

	char	*prompts;
	char	*next_prompt;

	char	*template;
	int	 idx;
};

static enum cmd_retval
cmd_command_prompt_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args			*args = self->args;
	const char			*inputs, *prompts;
	struct cmd_command_prompt_cdata	*cdata;
	struct client			*c;
	char				*prompt, *ptr, *input = NULL;
	size_t				 n;

	if ((c = cmd_find_client(item, args_get(args, 't'), 0)) == NULL)
		return (CMD_RETURN_ERROR);

	if (c->prompt_string != NULL)
		return (CMD_RETURN_NORMAL);

	cdata = xcalloc(1, sizeof *cdata);

	cdata->inputs = NULL;
	cdata->next_input = NULL;

	cdata->prompts = NULL;
	cdata->next_prompt = NULL;

	cdata->template = NULL;
	cdata->idx = 1;

	if (args->argc != 0)
		cdata->template = xstrdup(args->argv[0]);
	else
		cdata->template = xstrdup("%1");

	if ((prompts = args_get(args, 'p')) != NULL)
		cdata->prompts = xstrdup(prompts);
	else if (args->argc != 0) {
		n = strcspn(cdata->template, " ,");
		xasprintf(&cdata->prompts, "(%.*s) ", (int) n, cdata->template);
	} else
		cdata->prompts = xstrdup(":");

	/* Get first prompt. */
	cdata->next_prompt = cdata->prompts;
	ptr = strsep(&cdata->next_prompt, ",");
	if (prompts == NULL)
		prompt = xstrdup(ptr);
	else
		xasprintf(&prompt, "%s ", ptr);

	/* Get initial prompt input. */
	if ((inputs = args_get(args, 'I')) != NULL) {
		cdata->inputs = xstrdup(inputs);
		cdata->next_input = cdata->inputs;
		input = strsep(&cdata->next_input, ",");
	}

	if (args_has(args, '1'))
		cdata->flags |= PROMPT_SINGLE;
	else if (args_has(args, 'N'))
		cdata->flags |= PROMPT_NUMERIC;
	else if (args_has(args, 'i'))
		cdata->flags |= PROMPT_INCREMENTAL;
	status_prompt_set(c, prompt, input, cmd_command_prompt_callback,
	    cmd_command_prompt_free, cdata, cdata->flags);
	free(prompt);

	return (CMD_RETURN_NORMAL);
}

static int
cmd_command_prompt_callback(struct client *c, void *data, const char *s,
    int done)
{
	struct cmd_command_prompt_cdata	*cdata = data;
	struct cmdq_item		*new_item;
	char				*new_template, *prompt, *ptr;
	char				*input = NULL;
	struct cmd_parse_result		*pr;

	if (s == NULL)
		return (0);
	if (done && (cdata->flags & PROMPT_INCREMENTAL))
		return (0);

	new_template = cmd_template_replace(cdata->template, s, cdata->idx);
	if (done) {
		free(cdata->template);
		cdata->template = new_template;
	}

	/*
	 * Check if there are more prompts; if so, get its respective input
	 * and update the prompt data.
	 */
	if (done && (ptr = strsep(&cdata->next_prompt, ",")) != NULL) {
		xasprintf(&prompt, "%s ", ptr);
		input = strsep(&cdata->next_input, ",");
		status_prompt_update(c, prompt, input);

		free(prompt);
		cdata->idx++;
		return (1);
	}

	pr = cmd_parse_from_string(new_template, NULL);
	switch (pr->status) {
	case CMD_PARSE_EMPTY:
		new_item = NULL;
		break;
	case CMD_PARSE_ERROR:
		new_item = cmdq_get_error(pr->error);
		free(pr->error);
		cmdq_append(c, new_item);
		break;
	case CMD_PARSE_SUCCESS:
		new_item = cmdq_get_command(pr->cmdlist, NULL, NULL, 0);
		cmd_list_free(pr->cmdlist);
		cmdq_append(c, new_item);
		break;
	}

	if (!done)
		free(new_template);
	if (c->prompt_inputcb != cmd_command_prompt_callback)
		return (1);
	return (0);
}

static void
cmd_command_prompt_free(void *data)
{
	struct cmd_command_prompt_cdata	*cdata = data;

	free(cdata->inputs);
	free(cdata->prompts);
	free(cdata->template);
	free(cdata);
}
