/*	$OpenBSD: dict.c,v 1.2 2013/05/24 17:03:14 eric Exp $	*/

/*
 * Copyright (c) 2012 Gilles Chehade <gilles@poolp.org>
 * Copyright (c) 2012 Eric Faurot <eric@openbsd.org>
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

#include <sys/socket.h>	/* for smtpd.h */
#include <sys/queue.h>	/* for smtpd.h */
#include <stdio.h>	/* for smtpd.h */
#include <imsg.h>	/* for smtpd.h */

#include <err.h>
//#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "smtpd.h"

#define	MAX_DICTKEY_SIZE	64
struct dictentry {
	SPLAY_ENTRY(dictentry)	entry;
	char			key[MAX_DICTKEY_SIZE];
	void		       *data;
};

static int dictentry_cmp(struct dictentry *, struct dictentry *);

SPLAY_PROTOTYPE(_dict, dictentry, entry, dictentry_cmp);

int
dict_check(struct dict *d, const char *k)
{
	struct dictentry	key;

	if (strlcpy(key.key, k, sizeof key.key) >= sizeof key.key)
		errx(1, "dict_check(%p, %s): key too large", d, k);

	return (SPLAY_FIND(_dict, &d->dict, &key) != NULL);
}

void *
dict_set(struct dict *d, const char *k, void *data)
{
	struct dictentry	*entry, key;
	char			*old;

	if (strlcpy(key.key, k, sizeof key.key) >= sizeof key.key)
		errx(1, "dict_set(%p, %s): key too large", d, k);
	if ((entry = SPLAY_FIND(_dict, &d->dict, &key)) == NULL) {
		if ((entry = malloc(sizeof *entry)) == NULL)
			err(1, "dict_set: malloc");
		(void)strlcpy(entry->key, k, sizeof entry->key);
		SPLAY_INSERT(_dict, &d->dict, entry);
		old = NULL;
		d->count += 1;
	} else
		old = entry->data;

	entry->data = data;

	return (old);
}

void
dict_xset(struct dict *d, const char * k, void *data)
{
	struct dictentry	*entry;

	if ((entry = malloc(sizeof *entry)) == NULL)
		err(1, "dict_xset: malloc");
	if (strlcpy(entry->key, k, sizeof entry->key) >= sizeof entry->key)
		errx(1, "dict_xset(%p, %s): key too large", d, k);
	entry->data = data;
	if (SPLAY_INSERT(_dict, &d->dict, entry))
		errx(1, "dict_xset(%p, %s)", d, k);
	d->count += 1;
}

void *
dict_get(struct dict *d, const char *k)
{
	struct dictentry	key, *entry;

	if (strlcpy(key.key, k, sizeof key.key) >= sizeof key.key)
		errx(1, "dict_get(%p, %s): key too large", d, k);
	if ((entry = SPLAY_FIND(_dict, &d->dict, &key)) == NULL)
		return (NULL);

	return (entry->data);
}

void *
dict_xget(struct dict *d, const char *k)
{
	struct dictentry	key, *entry;

	if (strlcpy(key.key, k, sizeof key.key) >= sizeof key.key)
		errx(1, "dict_xget(%p, %s): key too large", d, k);
	if ((entry = SPLAY_FIND(_dict, &d->dict, &key)) == NULL)
		errx(1, "dict_xget(%p, %s)", d, k);

	return (entry->data);
}

void *
dict_pop(struct dict *d, const char *k)
{
	struct dictentry	key, *entry;
	void			*data;

	if (strlcpy(key.key, k, sizeof key.key) >= sizeof key.key)
		errx(1, "dict_pop(%p, %s): key too large", d, k);
	if ((entry = SPLAY_FIND(_dict, &d->dict, &key)) == NULL)
		return (NULL);

	data = entry->data;
	SPLAY_REMOVE(_dict, &d->dict, entry);
	free(entry);
	d->count -= 1;

	return (data);
}

void *
dict_xpop(struct dict *d, const char *k)
{
	struct dictentry	key, *entry;
	void			*data;

	if (strlcpy(key.key, k, sizeof key.key) >= sizeof key.key)
		errx(1, "dict_xpop(%p, %s): key too large", d, k);
	if ((entry = SPLAY_FIND(_dict, &d->dict, &key)) == NULL)
		errx(1, "dict_xpop(%p, %s)", d, k);

	data = entry->data;
	SPLAY_REMOVE(_dict, &d->dict, entry);
	free(entry);
	d->count -= 1;

	return (data);
}

int
dict_poproot(struct dict *d, const char **k, void **data)
{
	struct dictentry	*entry;

	entry = SPLAY_ROOT(&d->dict);
	if (entry == NULL)
		return (0);
	if (k)
		*k = entry->key;
	if (data)
		*data = entry->data;
	SPLAY_REMOVE(_dict, &d->dict, entry);
	free(entry);
	d->count -= 1;

	return (1);
}

int
dict_root(struct dict *d, const char **k, void **data)
{
	struct dictentry	*entry;

	entry = SPLAY_ROOT(&d->dict);
	if (entry == NULL)
		return (0);
	if (k)
		*k = entry->key;
	if (data)
		*data = entry->data;
	return (1);
}

int
dict_iter(struct dict *d, void **hdl, const char **k, void **data)
{
	struct dictentry *curr = *hdl;

	if (curr == NULL)
		curr = SPLAY_MIN(_dict, &d->dict);
	else
		curr = SPLAY_NEXT(_dict, &d->dict, curr);

	if (curr) {
		*hdl = curr;
		if (k)
			*k = curr->key;
		if (data)
			*data = curr->data;
		return (1);
	}

	return (0);
}

int
dict_iterfrom(struct dict *d, void **hdl, const char *kfrom, const char **k,
    void **data)
{
	struct dictentry *curr = *hdl, key;

	if (curr == NULL) {
		if (kfrom == NULL)
			curr = SPLAY_MIN(_dict, &d->dict);
		else {
			if (strlcpy(key.key, kfrom, sizeof key.key)
			    >= sizeof key.key)
				errx(1, "dict_iterfrom(%p, %s): key too large",
				    d, kfrom);
			curr = SPLAY_FIND(_dict, &d->dict, &key);
			if (curr == NULL) {
				SPLAY_INSERT(_dict, &d->dict, &key);
				curr = SPLAY_NEXT(_dict, &d->dict, &key);
				SPLAY_REMOVE(_dict, &d->dict, &key);
			}
		}
	} else
		curr = SPLAY_NEXT(_dict, &d->dict, curr);

	if (curr) {
		*hdl = curr;
		if (k)
			*k = curr->key;
		if (data)
			*data = curr->data;
		return (1);
	}

	return (0);
}

void
dict_merge(struct dict *dst, struct dict *src)
{
	struct dictentry	*entry;

	while (!SPLAY_EMPTY(&src->dict)) {
		entry = SPLAY_ROOT(&src->dict);
		SPLAY_REMOVE(_dict, &src->dict, entry);
		if (SPLAY_INSERT(_dict, &dst->dict, entry))
			errx(1, "dict_merge: duplicate");
	}
	dst->count += src->count;
	src->count = 0;
}

static int
dictentry_cmp(struct dictentry *a, struct dictentry *b)
{
	return strcmp(a->key, b->key);
}

SPLAY_GENERATE(_dict, dictentry, entry, dictentry_cmp);
