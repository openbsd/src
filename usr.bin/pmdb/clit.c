/*	$OpenBSD: clit.c,v 1.4 2002/08/09 02:23:48 aaron Exp $	*/
/*
 * Copyright (c) 2002 Artur Grabowski <art@openbsd.org>
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 *
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL  DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include <stdlib.h>
#include <stdio.h>
#include <histedit.h>
#include <err.h>
#include <string.h>

#include "clit.h"

extern char *__progname;

char *prompt_add;

static char *
prompt(EditLine *el)
{
	static char p[64];

	snprintf(p, sizeof(p), "%s%s> ", __progname,
	    prompt_add ? prompt_add : "");

	return p;
}

/*
 * Returns number of commands that (at least partially) match "name".
 */
static int
name_to_cmd(const char *name, struct clit *cmds, int ncmds, struct clit **res)
{
	int i, len, ret;

	len = strlen(name);
	ret = 0;

	for (i = 0; i < ncmds; i++) {
		if (strncmp(cmds[i].cmd, name, len) == 0) {
			*res = &cmds[i];
			ret++;
		}
	}

	return ret;	
}

struct clitenv {
	struct clit *cmds;
	int ncmds;
	EditLine *el;
	History *hist;
};

int
cmd_help(int argc, char **argv, void *arg)
{
	struct clitenv *env = arg;
	struct clit *cmds = env->cmds, *cmdp;
	int ncmds = env->ncmds;
	int i, res;

	if (argc > 1) {
		res = name_to_cmd(argv[1], cmds, ncmds, &cmdp);
		if (res == 1) {
			printf("%s\t%s\n", cmdp->cmd, cmdp->help);
		} else {
			fprintf(stderr, "%s command: %s\n",
			    res == 0 ? "unknown" : "ambiguous", argv[1]);
		}

		return 0;
	}
	for (i = 0; i < ncmds; i++) {
		cmdp = &cmds[i];

		printf("%s\t%s\n", cmdp->cmd, cmdp->help);
	}

	return 0;
}

/*
 * XXX - there is no way to push external args into this function.
 */
unsigned char
complt(EditLine *el, int ch)
{
	const LineInfo *line;
	char str[1024];
	int len, ret;

	line = el_line(el);
	if (line->cursor != line->lastchar)
		return CC_ERROR;

	len = line->lastchar - line->buffer;

	if (len >= 1023)
		return CC_ERROR;

	memcpy(str, line->buffer, len);
	str[len] = '\0';

	ret = cmd_complt(str, sizeof(str));

	el_push(el, &str[len]);

	return ret ? CC_ERROR : CC_REDISPLAY;
}

void *
cmdinit(struct clit *cmds, int ncmds)
{
	struct clitenv *env;
#ifdef __NetBSD__
	HistEvent ev;
#endif

	if ((env = malloc(sizeof(*env))) == NULL)
		err(1, "Can't init cmd interpreter.");

	env->cmds = cmds;
	env->ncmds = ncmds;

	env->hist = history_init();
#ifdef __NetBSD__
	history(env->hist, &ev, H_SETSIZE, 100);
#else
	history(env->hist, H_EVENT, 100);
#endif

#ifdef __NetBSD__
	env->el = el_init(__progname, stdin, stdout, stderr);
#else
	env->el = el_init(__progname, stdin, stdout);
#endif
	el_set(env->el, EL_EDITOR, "emacs");
	el_set(env->el, EL_PROMPT, prompt);
	el_set(env->el, EL_HIST, history, env->hist);
	el_set(env->el, EL_ADDFN, "complt", "complete", complt);
	el_set(env->el, EL_BIND, "\t", "complt");
	el_source(env->el, NULL);

	/* XXX - EL_SIGNAL ? */

	return env;
}

void
cmdend(void *arg)
{
	struct clitenv *env = arg;

	el_end(env->el);
	history_end(env->hist);

	free(env);
}

int
cmdloop(void *arg)
{
	struct clitenv *env = arg;
	EditLine *el = env->el;
	History *hist = env->hist;
	const char *elline;
	int cnt;
	char **argv;
	int maxargs = 16;	/* XXX */
	int stop;

	stop = 0;

	if ((argv = malloc(sizeof(char *) * maxargs)) == NULL)
		err(1, "malloc");

	while (!stop && (elline = el_gets(el, &cnt)) != NULL) {
		char *line, *orgline;
		struct clit *cmdp;
		char **ap;
		int argc, res;
#ifdef __NetBSD__
		HistEvent ev;
#endif

		memset(argv, 0, sizeof(char *) * maxargs);

#ifdef __NetBSD__
		history(hist, &ev, H_ENTER, elline);
#else
		history(hist, H_ENTER, elline);
#endif

		orgline = line = strdup(elline);
		if (line == NULL)
			err(1, "strdup");

		argc = 0;
		for (ap = argv; (*ap = strsep(&line, " \t\n")) != NULL;) {
			if (**ap != '\0') {
				++ap;
				if (++argc == maxargs)
					break;
			}
		}
		if (argc == maxargs) {
			fprintf(stderr, "Too many arguments\n");
			goto cmdout;
		}
		if (!argc)
			goto cmdout;

		/*
		 * Editline commands.
		 */
		if (el_parse(el, argc, argv) != -1)
			goto cmdout;

		if ((res = name_to_cmd(argv[0], env->cmds, env->ncmds,
		    &cmdp)) == 1) {
			if (argc - 1 > cmdp->maxargc)
				fprintf(stderr, "Too many arguments\n");
			else if (argc - 1 < cmdp->minargc)
				fprintf(stderr, "Too few arguments\n");
			else
				stop = (*cmdp->handler)(argc, argv,
				    cmdp->arg ? cmdp->arg : env);
		} else {
			fprintf(stderr, "%s command: %s\n",
			    res == 0 ? "unknown" : "ambiguous", argv[0]);
		}
cmdout:
		free(orgline);
	}
	free(argv);

	return stop;
}

