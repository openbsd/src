/*	$OpenBSD: alias.c,v 1.2 2002/07/19 14:38:57 itojun Exp $	*/
/*
 * Copyright 2002 Niels Provos <provos@citi.umich.edu>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Niels Provos.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/tree.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <grp.h>
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <err.h>

#include "intercept.h"
#include "systrace.h"

static SPLAY_HEAD(alitr, systrace_alias) aliasroot;
static SPLAY_HEAD(revtr, systrace_revalias) revroot;

static int aliascompare(struct systrace_alias *, struct systrace_alias *);
static int revcompare(struct systrace_revalias *, struct systrace_revalias *);

static int
aliascompare(struct systrace_alias *a, struct systrace_alias *b)
{
	int diff;

	diff = strcmp(a->emulation, b->emulation);
	if (diff)
		return (diff);
	return (strcmp(a->name, b->name));
}

static int
revcompare(struct systrace_revalias *a, struct systrace_revalias *b)
{
	int diff;

	diff = strcmp(a->emulation, b->emulation);
	if (diff)
		return (diff);
	return (strcmp(a->name, b->name));
}

SPLAY_PROTOTYPE(alitr, systrace_alias, node, aliascompare);
SPLAY_GENERATE(alitr, systrace_alias, node, aliascompare);

SPLAY_PROTOTYPE(revtr, systrace_revalias, node, revcompare);
SPLAY_GENERATE(revtr, systrace_revalias, node, revcompare);

int
systrace_initalias(void)
{
	SPLAY_INIT(&aliasroot);
	SPLAY_INIT(&revroot);

	return (0);
}

struct systrace_alias *
systrace_find_alias(const char *emulation, const char *name)
{
	struct systrace_alias tmp;

	strlcpy(tmp.emulation, emulation, sizeof(tmp.emulation));
	strlcpy(tmp.name, name, sizeof(tmp.name));

	return (SPLAY_FIND(alitr, &aliasroot, &tmp));
}

struct systrace_revalias *
systrace_find_reverse(const char *emulation, const char *name)
{
	struct systrace_revalias tmp;

	strlcpy(tmp.emulation, emulation, sizeof(tmp.emulation));
	strlcpy(tmp.name, name, sizeof(tmp.name));

	return (SPLAY_FIND(revtr, &revroot, &tmp));
}

struct systrace_revalias *
systrace_reverse(const char *emulation, const char *name)
{
	struct systrace_revalias tmp, *reverse;

	strlcpy(tmp.emulation, emulation, sizeof(tmp.emulation));
	strlcpy(tmp.name, name, sizeof(tmp.name));

	reverse = SPLAY_FIND(revtr, &revroot, &tmp);
	if (reverse != NULL)
		return (reverse);

	reverse = calloc(1, sizeof(struct systrace_alias));
	if (reverse == NULL)
		err(1, "%s: %s-%s: malloc", __func__, emulation, name);

	strlcpy(reverse->emulation, emulation, sizeof(reverse->emulation));
	strlcpy(reverse->name, name, sizeof(reverse->name));

	TAILQ_INIT(&reverse->revl);

	if (SPLAY_INSERT(revtr, &revroot, reverse) != NULL)
		errx(1, "%s: %s-%s: double revalias",
		    __func__, emulation, name);

	return (reverse);
}

struct systrace_alias *
systrace_new_alias(const char *emulation, const char *name,
    char *aemul, char *aname)
{
	struct systrace_alias *alias;
	struct systrace_revalias *reverse;

	alias = malloc(sizeof(struct systrace_alias));
	if (alias == NULL)
		err(1, "%s: %s-%s: malloc", __func__, emulation, name);

	strlcpy(alias->emulation, emulation, sizeof(alias->emulation));
	strlcpy(alias->name, name, sizeof(alias->name));
	strlcpy(alias->aemul, aemul, sizeof(alias->aemul));
	strlcpy(alias->aname, aname, sizeof(alias->aname));
	alias->nargs = 0;

	if (SPLAY_INSERT(alitr, &aliasroot, alias) != NULL)
		errx(1, "%s: %s-%s: double alias", __func__, emulation, name);

	reverse = systrace_reverse(aemul, aname);
	alias->reverse = reverse;
	TAILQ_INSERT_TAIL(&reverse->revl, alias, next);

	return (alias);
}

void
systrace_switch_alias(const char *emulation, const char *name,
    char *aemul, char *aname)
{
	struct systrace_alias *alias;
	struct systrace_revalias *reverse;

	if ((alias = systrace_find_alias(emulation, name)) == NULL)
		errx(1, "%s: unknown alias %s-%s", __func__, emulation, name);

	/* Switch to a different alias */
	reverse = alias->reverse;
	TAILQ_REMOVE(&reverse->revl, alias, next);

	strlcpy(alias->aemul, aemul, sizeof(alias->aemul));
	strlcpy(alias->aname, aname, sizeof(alias->aname));

	reverse = systrace_reverse(aemul, aname);
	alias->reverse = reverse;
	TAILQ_INSERT_TAIL(&reverse->revl, alias, next);
}

/* Add an already translated argument to this alias */

void
systrace_alias_add_trans(struct systrace_alias *alias,
    struct intercept_translate *tl)
{
	if (alias->nargs >= SYSTRACE_MAXALIAS)
		errx(1, "%s: too many arguments", __func__);

	alias->arguments[alias->nargs++] = tl;
}
