/*	$OpenBSD: irr_asset.c,v 1.11 2015/01/16 06:40:15 deraadt Exp $ */

/*
 * Copyright (c) 2007 Henning Brauer <henning@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#include "irrfilter.h"

int		 as_set_compare(struct as_set *, struct as_set *);
struct as_set	*as_set_find(char *);

RB_HEAD(as_set_h, as_set)	as_set_h;
RB_PROTOTYPE(as_set_h, as_set, entry, as_set_compare)
RB_GENERATE(as_set_h, as_set, entry, as_set_compare)

enum obj_type {
	T_UNKNOWN,
	T_ASSET,
	T_AUTNUM
};

struct as_set	*curass;

struct as_set	*asset_get(char *);
enum obj_type	 asset_membertype(char *);
void		 asset_resolve(struct as_set *);
int		 asset_merge(struct as_set *, struct as_set *);
int		 asset_add_as(struct as_set *, char *);
int		 asset_add_asset(struct as_set *, char *);

struct as_set *
asset_expand(char *s)
{
	struct as_set	*ass;
	char		*name;
	size_t		 i;

	if ((name = calloc(1, strlen(s) + 1)) == NULL)
		err(1, "asset_expand calloc");
	for (i = 0; i < strlen(s); i++)
		name[i] = toupper((unsigned char)s[i]);

	ass = asset_get(name);
	asset_resolve(ass);

	free(name);
	return (ass);
}

struct as_set *
asset_get(char *name)
{
	struct as_set	*ass, *mas;
	u_int		 i;

	/*
	 * the caching prevents the endless recursion.
	 * MUST have the RB_INSERT before calling self again.
	 */

	/* cached? then things are easy */
	if ((ass = as_set_find(name)) != NULL)
		return ass;

	if ((ass = calloc(1, sizeof(*ass))) == NULL)
		err(1, "expand_as_set calloc");
	if ((ass->name = strdup(name)) == NULL)
		err(1, "expand_as_set strdup");
	RB_INSERT(as_set_h, &as_set_h, ass);

	switch (asset_membertype(name)) {
	case T_ASSET:
		/* as-set */
		if (irrverbose >= 3) {
			fprintf(stdout, "query AS-SET %s... ", name);
			fflush(stdout);
		}
		curass = ass;
		if (whois(name, QTYPE_ASSET) == -1)
			errx(1, "whois error, asset_get %s", name);
		curass = NULL;
		if (irrverbose >= 3)
			fprintf(stdout, "done\n");
		break;
	case T_AUTNUM:
		/*
		 * make a dummy as-set with the AS both as name
		 * and its only member
		 */
		asset_add_as(ass, name);
		return (ass);
	default:
		fprintf(stderr, "asset_get: %s: unknown object type\n", name);
		break;
	}


	for (i = 0; i < ass->n_members; i++) {
		mas = asset_get(ass->members[i]);
		if (mas->n_members == 0 && mas->n_as == 0)
			fprintf(stderr, "%s: can't resolve member %s\n",
			    name, ass->members[i]);
		else
			asset_add_asset(ass, ass->members[i]);
	}

	return (ass);
}

enum obj_type
asset_membertype(char *name)
{
	char	*s;

	if (!strncmp(name, "AS-", 3))
		return (T_ASSET);

	if ((s = strchr(name, ':')) != NULL) {
		/* this must be an as-set. one component has to start w/ AS- */
		for (s = name; s != NULL; s = strchr(s, ':'))
			if (!strncmp(++s, "AS-", 3))
				return (T_ASSET);
		return (T_UNKNOWN);
	}

	/* neither plain nor hierachical set definition, might be aut-num */
	if (!strncmp(name, "AS", 2) && strlen(name) > 2 &&
	    isdigit((unsigned char)name[2]))
		return (T_AUTNUM);

	return (T_UNKNOWN);
}

void
asset_resolve(struct as_set *ass)
{
	struct as_set	*mas;
	u_int		 i;

	/*
	 * traverse all as_set members and fold their
	 * members as into this as_set.
	 * ass->n_as_set is a moving target, it grows
	 * as member as-sets' member as-sets are beeing
	 * added.
	 * remove processed member as-sets (all!) only
	 * after we are done, they're needed for dupe
	 * detection
	 */

	for (i = 0; i < ass->n_as_set; i++) {
		if ((mas = as_set_find(ass->as_set[i])) == NULL)
			errx(1, "asset_get %s: %s unresolved?!?",
			    ass->name, ass->as_set[i]);
		if (asset_merge(ass, mas) == -1)
			errx(1, "asset_merge failed");
	}

	for (i = 0; i < ass->n_as_set; i++) {
		free(ass->as_set[i]);
		ass->as_set[i] = NULL;
	}
	free(ass->as_set);
	ass->as_set = NULL;
	ass->n_as_set = 0;
}

int
asset_merge(struct as_set *ass, struct as_set *mas)
{
	u_int	i, j;

	/* merge ASes from the member into the parent */
	for (i = 0; i < mas->n_as; i++) {
		for (j = 0; j < ass->n_as && strcmp(ass->as[j],
		    mas->as[i]); j++)
			; /* nothing */
		if (j == ass->n_as)
			if (asset_add_as(ass, mas->as[i]) == -1)
				return (-1);
	}

	/* merge as-set members from the member into the parent */
	for (i = 0; i < mas->n_as_set; i++) {
		if (!strcmp(ass->name, mas->as_set[i]))		/* skip self! */
			continue;
		for (j = 0; j < ass->n_as_set && strcmp(ass->as_set[j],
		    mas->as_set[i]); j++)
			; /* nothing */
		if (j == ass->n_as_set)
			if (asset_add_asset(ass, mas->as_set[i]) == -1)
				return (-1);
	}

	return (0);
}

int
asset_addmember(char *s)
{
	void	*p;
	char	*as;
	size_t	 i;

	/* convert to uppercase on the fly */
	if ((as = calloc(1, strlen(s) + 1)) == NULL)
		err(1, "asset_addmember strdup");
	for (i = 0; i < strlen(s); i++)
		as[i] = toupper((unsigned char)s[i]);

	if ((p = reallocarray(curass->members,
	    curass->n_members + 1, sizeof(char *))) == NULL)
		err(1, "asset_addmember strdup");
	curass->members = p;
	curass->n_members++;
	curass->members[curass->n_members - 1] = as;

	return (0);
}

int
asset_add_as(struct as_set *ass, char *s)
{
	void *p;

	if ((p = reallocarray(ass->as,
	    ass->n_as + 1, sizeof(char *))) == NULL)
		err(1, "asset_add_as strdup");
	ass->as = p;
	ass->n_as++;

	if ((ass->as[ass->n_as - 1] =
	    strdup(s)) == NULL)
		err(1, "asset_add_as strdup");

	return (0);
}

int
asset_add_asset(struct as_set *ass, char *s)
{
	void *p;

	if ((p = reallocarray(ass->as_set,
	    ass->n_as_set + 1, sizeof(char *))) == NULL)
		err(1, "asset_add_asset strdup");
	ass->as_set = p;
	ass->n_as_set++;

	if ((ass->as_set[ass->n_as_set - 1] =
	    strdup(s)) == NULL)
		err(1, "asset_add_asset strdup");

	return (0);
}

/* RB helpers */
int
as_set_compare(struct as_set *a, struct as_set *b)
{
	return (strcmp(a->name, b->name));
}

struct as_set *
as_set_find(char *name)
{
	struct as_set	s;

	s.name = name;
	return (RB_FIND(as_set_h, &as_set_h, &s));
}
