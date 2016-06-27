/* $OpenBSD: env.c,v 1.3 2016/06/27 15:41:17 tedu Exp $ */
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
#include <sys/tree.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <err.h>
#include <unistd.h>
#include <errno.h>

#include "doas.h"

struct envnode {
	RB_ENTRY(envnode) node;
	const char *key;
	const char *value;
};

struct env {
	RB_HEAD(envtree, envnode) root;
	u_int count;
};

static int
envcmp(struct envnode *a, struct envnode *b)
{
	return strcmp(a->key, b->key);
}
RB_GENERATE_STATIC(envtree, envnode, node, envcmp)

static struct envnode *
createnode(const char *key, const char *value)
{
	struct envnode *node;

	node = malloc(sizeof(*node));
	if (!node)
		err(1, NULL);
	node->key = strdup(key);
	node->value = strdup(value);
	if (!node->key || !node->value)
		err(1, NULL);
	return node;
}

static void
freenode(struct envnode *node)
{
	free((char *)node->key);
	free((char *)node->value);
	free(node);
}

static struct env *
createenv(struct rule *rule)
{
	struct env *env;
	u_int i;

	env = malloc(sizeof(*env));
	if (!env)
		err(1, NULL);
	RB_INIT(&env->root);
	env->count = 0;

	if (rule->options & KEEPENV) {
		extern const char **environ;

		for (i = 0; environ[i] != NULL; i++) {
			struct envnode *node;
			const char *e, *eq;
			size_t len;
			char keybuf[1024];

			e = environ[i];

			/* ignore invalid or overlong names */
			if ((eq = strchr(e, '=')) == NULL || eq == e)
				continue;
			len = eq - e;
			if (len > sizeof(keybuf) - 1)
				continue;
			memcpy(keybuf, e, len);
			keybuf[len] = '\0';

			node = createnode(keybuf, eq + 1);
			if (RB_INSERT(envtree, &env->root, node)) {
				/* ignore any later duplicates */
				freenode(node);
			} else {
				env->count++;
			}
		}
	}

	return env;
}

static char **
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
fillenv(struct env *env, const char **envlist)
{
	struct envnode *node, key;
	const char *e, *eq;
	const char *val;
	char name[1024];
	u_int i;
	size_t len;

	for (i = 0; envlist[i]; i++) {
		e = envlist[i];

		/* parse out env name */
		if ((eq = strchr(e, '=')) == NULL)
			len = strlen(e);
		else
			len = eq - e;
		if (len > sizeof(name) - 1)
			continue;
		memcpy(name, e, len);
		name[len] = '\0';

		/* delete previous copies */
		key.key = name;
		if (*name == '-')
			key.key = name + 1;
		if ((node = RB_FIND(envtree, &env->root, &key))) {
			RB_REMOVE(envtree, &env->root, node);
			freenode(node);
			env->count--;
		}
		if (*name == '-')
			continue;

		/* assign value or inherit from environ */
		if (eq) {
			val = eq + 1;
			if (*val == '$')
				val = getenv(val + 1);
		} else {
			val = getenv(name);
		}
		/* at last, we have something to insert */
		if (val) {
			node = createnode(name, val);
			RB_INSERT(envtree, &env->root, node);
			env->count++;
		}
	}
}

char **
prepenv(struct rule *rule)
{
	static const char *safeset[] = {
		"DISPLAY", "HOME", "LOGNAME", "MAIL",
		"PATH", "TERM", "USER", "USERNAME",
		NULL
	};
	struct env *env;
	
	env = createenv(rule);

	/* if we started with blank, fill some defaults then apply rules */
	if (!(rule->options & KEEPENV))
		fillenv(env, safeset);
	if (rule->envlist)
		fillenv(env, rule->envlist);

	return flattenenv(env);
}
