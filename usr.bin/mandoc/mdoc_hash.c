/*	$Id: mdoc_hash.c,v 1.5 2009/08/09 18:01:15 schwarze Exp $ */
/*
 * Copyright (c) 2008, 2009 Kristaps Dzonsons <kristaps@kth.se>
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

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "libmdoc.h"

#define	ADJUST_MAJOR(x) 					\
	do if (37 == (x))					\
		(x) = 0; 		/* %   -> 00 */		\
	else if (91 > (x)) 					\
		(x) -= 64; 		/* A-Z -> 01 - 26 */	\
	else 							\
		(x) -= 70;		/* a-z -> 27 - 52 */	\
	while (/*CONSTCOND*/0)

#define ADJUST_MINOR(y)						\
	do if (49 == (y))					\
		(y) = 0;		/* 1   -> 00 */		\
	else if (91 > (y))					\
		(y) -= 65;		/* A-Z -> 00 - 25 */	\
	else 							\
		(y) -= 97;		/* a-z -> 00 - 25 */	\
	while (/*CONSTCOND*/0)

#define INDEX(maj, min) 					\
	((maj) * 26 * 3) + ((min) * 3)

#define	SLOTCMP(slot, val)					\
	(mdoc_macronames[(slot)][0] == (val)[0] && 		\
	 mdoc_macronames[(slot)][1] == (val)[1] && 		\
	 (0 == (val)[2] || 					\
	  mdoc_macronames[(slot)][2] == (val)[2]))


void
mdoc_hash_free(void *htab)
{

	free(htab);
}



void *
mdoc_hash_alloc(void)
{
	int		  i, major, minor, ind;
	const void	**htab;

	htab = calloc(26 * 3 * 52, sizeof(struct mdoc_macro *));
	if (NULL == htab) 
		return(NULL);

	for (i = 0; i < MDOC_MAX; i++) {
		major = mdoc_macronames[i][0];
		assert(isalpha((u_char)major) || 37 == major);

		ADJUST_MAJOR(major);

		minor = mdoc_macronames[i][1];
		assert(isalpha((u_char)minor) || 49 == minor);

		ADJUST_MINOR(minor);

		ind = INDEX(major, minor);

		if (NULL == htab[ind]) {
			htab[ind] = &mdoc_macros[i];
			continue;
		}

		if (NULL == htab[++ind]) {
			htab[ind] = &mdoc_macros[i];
			continue;
		}

		assert(NULL == htab[++ind]);
		htab[ind] = &mdoc_macros[i];
	}

	return((void *)htab);
}


int
mdoc_hash_find(const void *arg, const char *tmp)
{
	int		  major, minor, ind, slot;
	const void	**htab;

	htab = /* LINTED */
		(const void **)arg;

	if (0 == (major = tmp[0]))
		return(MDOC_MAX);
	if (0 == (minor = tmp[1]))
		return(MDOC_MAX);

	if (tmp[2] && tmp[3])
		return(MDOC_MAX);

	if (37 != major && ! isalpha((u_char)major))
		return(MDOC_MAX);
	if (49 != minor && ! isalpha((u_char)minor))
		return(MDOC_MAX);

	ADJUST_MAJOR(major);
	ADJUST_MINOR(minor);

	ind = INDEX(major, minor);

	if (ind < 0 || ind >= 26 * 3 * 52)
		return(MDOC_MAX);

	if (htab[ind]) {
		slot = htab[ind] - /* LINTED */
			(void *)mdoc_macros;
		assert(0 == (size_t)slot % sizeof(struct mdoc_macro));
		slot /= sizeof(struct mdoc_macro);
		if (SLOTCMP(slot, tmp))
			return(slot);
		ind++;
	}

	if (htab[ind]) {
		slot = htab[ind] - /* LINTED */
			(void *)mdoc_macros;
		assert(0 == (size_t)slot % sizeof(struct mdoc_macro));
		slot /= sizeof(struct mdoc_macro);
		if (SLOTCMP(slot, tmp))
			return(slot);
		ind++;
	}

	if (NULL == htab[ind]) 
		return(MDOC_MAX);
	slot = htab[ind] - /* LINTED */
		(void *)mdoc_macros;
	assert(0 == (size_t)slot % sizeof(struct mdoc_macro));
	slot /= sizeof(struct mdoc_macro);
	if (SLOTCMP(slot, tmp))
		return(slot);

	return(MDOC_MAX);
}

