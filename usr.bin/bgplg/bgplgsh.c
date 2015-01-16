/*	$OpenBSD: bgplgsh.c,v 1.7 2015/01/16 06:40:06 deraadt Exp $	*/

/*
 * Copyright (c) 2005, 2006 Reyk Floeter <reyk@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <sys/stat.h>

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>

#include <readline/readline.h>
#include <readline/history.h>

#include "bgplg.h"

#define BGPDSOCK	"/var/www/run/bgpd.rsock"
#define BGPCTL		"/usr/sbin/bgpctl", "-s", BGPDSOCK
#define PING		"/sbin/ping"
#define TRACEROUTE	"/usr/sbin/traceroute"
#define PING6		"/sbin/ping6"
#define TRACEROUTE6	"/usr/sbin/traceroute6"

static volatile int quit;

static struct cmd cmds[] = CMDS;

char		**lg_arg2argv(char *, int *);
char		**lg_argextra(char **, int, int, struct cmd *);
int		  lg_checkarg(char *);
int		  lg_checkcmd(int, char **, int *, struct cmd *);
char		 *lg_completion(const char *, int);

int
lg_checkarg(char *arg)
{
	size_t len;
	u_int i;

	if (!(len = strlen(arg)))
		return (0);

#define allowed_in_string(_x)                                           \
	((isalnum((unsigned char)_x) || isprint((unsigned char)_x)) &&	\
	(_x != '%' && _x != '\\' && _x != ';' && _x != '&' && _x != '|'))

	for (i = 0; i < len; i++) {
		if (!allowed_in_string(arg[i])) {
			fprintf(stderr, "invalid character in input\n");
			return (EPERM);
		}
	}
#undef allowed_in_string
	return (0);
}

char **
lg_arg2argv(char *arg, int *argc)
{
	char **argv, *ptr = arg;
	size_t len;
	u_int i, c = 1;

	if (lg_checkarg(arg) != 0)
		return (NULL);
	if (!(len = strlen(arg)))
		return (NULL);

	/* Count elements */
	for (i = 0; i < len; i++) {
		if (isspace((unsigned char)arg[i])) {
			/* filter out additional options */
			if (arg[i + 1] == '-') {
				printf("invalid input\n");
				return (NULL);
			}
			arg[i] = '\0';
			c++;
		}
	}
	if (arg[0] == '\0')
		return (NULL);

	/* Generate array */
	if ((argv = calloc(c + 1, sizeof(char *))) == NULL) {
		printf("fatal error: %s\n", strerror(errno));
		return (NULL);
	}

	argv[c] = NULL;
	*argc = c;

	/* Fill array */
	for (i = c = 0; i < len; i++) {
		if (arg[i] == '\0' || i == 0) {
			if (i != 0)
				ptr = &arg[i + 1];
			argv[c++] = ptr;
		}
	}

	return (argv);
}

char **
lg_argextra(char **argv, int argc, int off, struct cmd *cmdp)
{
	char **new_argv;
	int i, c = 0, n;

	if ((n = argc - off) < 0)
		return (NULL);

	/* Count elements */
	for (i = 0; cmdp->earg[i] != NULL; i++)
		c++;

	/* Generate array */
	if ((new_argv = calloc(c + n + 1, sizeof(char *))) == NULL) {
		printf("fatal error: %s\n", strerror(errno));
		return (NULL);
	}

	/* Fill array */
	for (i = c = 0; cmdp->earg[i] != NULL; i++)
		new_argv[c++] = cmdp->earg[i];

	/* Append old array */
	for (i = n; i < argc; i++)
		new_argv[c++] = argv[i];

	new_argv[c] = NULL;

	if (argv != NULL)
		free(argv);

	return (new_argv);
}

int
lg_checkcmd(int argc, char **argv, int *off, struct cmd *cmd)
{
	char **cmdp = NULL, *cmdstr = NULL;
	int i, ncmd, v, ret = -1;

	if ((cmdstr = strdup(cmd->name)) == NULL)
		goto done;
	if ((cmdp = lg_arg2argv(cmdstr, &ncmd)) == NULL)
		goto done;
	if (ncmd > argc || argc > (ncmd + cmd->maxargs))
		goto done;

	for (i = 0; i < ncmd; i++)
		if (strcmp(argv[i], cmdp[i]) != 0)
			goto done;

	if ((v = argc - ncmd) < 0 ||
	    (*off != -1 && *off < v))
		goto done;
	if (cmd->minargs && v < cmd->minargs) {
		ret = EINVAL;
		goto done;
	}
	*off = v;
	ret = 0;

 done:
	if (cmdp != NULL)
		free(cmdp);
	if (cmdstr != NULL)
		free(cmdstr);
	return (ret);
}

char *
lg_completion(const char *str, int state)
{
	static int lg_complidx, len;
	const char *name;

	if (state == 0) {
		len = strlen(str);
		lg_complidx = 0;
	}
	while ((name = cmds[lg_complidx].name) != NULL) {
		lg_complidx++;
		if (strncmp(name, str, len) == 0)
			return (strdup(name));
	}

	return (NULL);
}

int
main(void)
{
	struct cmd *cmd = NULL;
	char prompt[HOST_NAME_MAX+1], *line, **argp = NULL;
	int ncmd, ret, v = -1;
	u_int i;

	rl_readline_name = NAME;
	rl_completion_entry_function = lg_completion;

	/* Ignore the whitespace character */
	rl_basic_word_break_characters = "\t\n\"\\'`@$><=;|&{(";

	while (!quit) {
		v = -1;
		gethostname(prompt, sizeof(prompt) - 2);
		strlcat(prompt, "> ", sizeof(prompt));

		if ((line = readline(prompt)) == NULL) {
			printf("\n");
			lg_help(cmds, NULL);
			continue;
		}
		if (!lg_strip(line))
			goto next;
		if (strcmp(line, "exit") == 0) {
			quit = 1;
			goto next;
		}

		add_history(line);

		if ((argp = lg_arg2argv(line, &ncmd)) == NULL)
			goto next;

		for (i = 0; cmds[i].name != NULL; i++) {
			ret = lg_checkcmd(ncmd, argp, &v, &cmds[i]);
			if (ret == 0)
				cmd = &cmds[i];
			else if (ret == EINVAL) {
				printf("invalid number of arguments\n");
				goto next;
			}
		}

		if (cmd == NULL) {
			printf("invalid command\n");
		} else if (cmd->func != NULL) {
			cmd->func(cmds, argp);
		} else {
			if ((argp = lg_argextra(argp, ncmd, v, cmd)) == NULL)
				goto next;
			lg_exec(cmd->earg[0], argp);
		}

 next:
		if (argp != NULL) {
			free(argp);
			argp = NULL;
		}
		if (line != NULL) {
			free(line);
			line = NULL;
		}
		cmd = NULL;
	}

	return (0);
}
