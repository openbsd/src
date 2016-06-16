/* $OpenBSD: env.c,v 1.1 2016/06/16 17:40:30 tedu Exp $ */
/*
 * Copyright (c) 2016 Ted Unangst <tedu@openbsd.org>
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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <err.h>
#include <unistd.h>
#include <errno.h>

#include "doas.h"

int
envcmp(struct envnode *a, struct envnode *b)
{
	return strcmp(a->key, b->key);
}
RB_GENERATE(envtree, envnode, node, envcmp)

struct env *
createenv(char **envp)
{
	struct env *env;
	u_int i;

	env = malloc(sizeof(*env));
	if (!env)
		err(1, NULL);
	RB_INIT(&env->root);
	env->count = 0;

	for (i = 0; envp[i] != NULL; i++) {
		struct envnode *node;
		const char *e, *eq;

		e = envp[i];

		if ((eq = strchr(e, '=')) == NULL || eq == e)
			continue;
		node = malloc(sizeof(*node));
		if (!node)
			err(1, NULL);
		node->key = strndup(envp[i], eq - e);
		node->value = strdup(eq + 1);
		if (!node->key || !node->value)
			err(1, NULL);
		if (RB_FIND(envtree, &env->root, node)) {
			free((char *)node->key);
			free((char *)node->value);
			free(node);
		} else {
			RB_INSERT(envtree, &env->root, node);
			env->count++;
		}
	}
	return env;
}

char **
flattenenv(struct env *env)
{
	char **envp;
	struct envnode *node;
	u_int i;

	envp = reallocarray(NULL, env->count + 1, sizeof(char *));
	if (!envp)
		err(1, NULL);
	i = 0;
	RB_FOREACH(node, envtree, &env->root) {
		if (asprintf(&envp[i], "%s=%s", node->key, node->value) == -1)
			err(1, NULL);
		i++;
	}
	envp[i] = NULL;
	return envp;
}

static void
copyenv(struct env *orig, struct env *copy, const char **envlist)
{
	struct envnode *node, key;
	u_int i;

	for (i = 0; envlist[i]; i++) {
		key.key = envlist[i];
		if ((node = RB_FIND(envtree, &orig->root, &key))) {
			RB_REMOVE(envtree, &orig->root, node);
			orig->count--;
			RB_INSERT(envtree, &copy->root, node);
			copy->count++;
		}
	}
}

struct env *
filterenv(struct env *orig, struct rule *rule)
{
	const char *safeset[] = {
		"DISPLAY", "HOME", "LOGNAME", "MAIL",
		"PATH", "TERM", "USER", "USERNAME",
		NULL
	};
	const char *badset[] = {
		"ENV",
		NULL
	};
	struct env *copy;
	struct envnode *node, key;
	u_int i;

	if ((rule->options & KEEPENV) && !rule->envlist) {
		for (i = 0; badset[i]; i++) {
			key.key = badset[i];
			if ((node = RB_FIND(envtree, &orig->root, &key))) {
				RB_REMOVE(envtree, &orig->root, node);
				free((char *)node->key);
				free((char *)node->value);
				free(node);
				orig->count--;
			}
		}
		return orig;
	}

	copy = malloc(sizeof(*copy));
	if (!copy)
		err(1, NULL);
	RB_INIT(&copy->root);
	copy->count = 0;

	if (rule->envlist)
		copyenv(orig, copy, rule->envlist);
	copyenv(orig, copy, safeset);

	return copy;
}
