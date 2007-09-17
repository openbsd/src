/*	$OpenBSD: direxpand.c,v 1.3 2007/09/17 09:28:36 espie Exp $ */
/*
 * Copyright (c) 1999,2007 Marc Espie.
 *
 * Extensive code changes for the OpenBSD project.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE OPENBSD PROJECT AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OPENBSD
 * PROJECT OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Copyright (c) 1988, 1989, 1990 The Regents of the University of California.
 * Copyright (c) 1988, 1989 by Adam de Boor
 * Copyright (c) 1989 by Berkeley Softworks
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Adam de Boor.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "defines.h"
#include "lst.h"
#include "dir.h"
#include "direxpand.h"
#include "error.h"
#include "memory.h"
#include "str.h"

/* Handles simple wildcard expansion on a path. */
static void PathMatchFilesi(const char *, const char *, Lst, Lst);
/* Handles wildcards expansion except for curly braces. */
static void DirExpandWildi(const char *, const char *, Lst, Lst);
#define DirExpandWild(s, l1, l2) DirExpandWildi(s, strchr(s, '\0'), l1, l2)
/* Handles wildcard expansion including curly braces. */
static void DirExpandCurlyi(const char *, const char *, Lst, Lst);

/* Debugging: show each word in an expansion list. */
static void DirPrintWord(void *);

/*-
 *-----------------------------------------------------------------------
 * PathMatchFilesi --
 *	Traverse directories in the path, calling Dir_MatchFiles for each.
 *	NOTE: This doesn't handle patterns in directories.
 *-----------------------------------------------------------------------
 */
static void
PathMatchFilesi(const char *word, const char *eword, Lst path, Lst expansions)
{
	LstNode	ln;		/* Current node */

	for (ln = Lst_First(path); ln != NULL; ln = Lst_Adv(ln))
		Dir_MatchFilesi(word, eword, (struct PathEntry *)Lst_Datum(ln), expansions);
}

/*-
 *-----------------------------------------------------------------------
 * DirExpandWildi:
 *	Expand all wild cards in a fully qualified name, except for
 *	curly braces.
 * Side-effect:
 *	Will hash any directory in which a file is found, and add it to
 *	the path, on the assumption that future lookups will find files
 *	there as well.
 *-----------------------------------------------------------------------
 */
static void
DirExpandWildi(const char *word, const char *eword, Lst path, Lst expansions)
{
	const char	*cp;
	const char	*slash; 	/* keep track of first slash before wildcard */

	slash = memchr(word, '/', eword - word);
	if (slash == NULL) {
		/* First the files in dot.  */
		Dir_MatchFilesi(word, eword, dot, expansions);

		/* Then the files in every other directory on the path.  */
		PathMatchFilesi(word, eword, path, expansions);
		return;
	}
	/* The thing has a directory component -- find the first wildcard
	 * in the string.  */
	slash = word;
	for (cp = word; cp != eword; cp++) {
		if (*cp == '/')
			slash = cp;
		if (*cp == '?' || *cp == '[' || *cp == '*') {

			if (slash != word) {
				char	*dirpath;

				/* If the glob isn't in the first component,
				 * try and find all the components up to
				 * the one with a wildcard.  */
				dirpath = Dir_FindFilei(word, slash+1, path);
				/* dirpath is null if we can't find the
				 * leading component
				 * XXX: Dir_FindFile won't find internal
				 * components.  i.e. if the path contains
				 * ../Etc/Object and we're looking for Etc,
				 * it won't be found. */
				if (dirpath != NULL) {
					char *dp;
					LIST temp;

					dp = strchr(dirpath, '\0');
					while (dp > dirpath && dp[-1] == '/')
						dp--;

					Lst_Init(&temp);
					Dir_AddDiri(&temp, dirpath, dp);
					PathMatchFilesi(slash+1, eword, &temp,
					    expansions);
					Lst_Destroy(&temp, NOFREE);
				}
			} else
				/* Start the search from the local directory. */
				PathMatchFilesi(word, eword, path, expansions);
			return;
		}
	}
	/* Return the file -- this should never happen.  */
	PathMatchFilesi(word, eword, path, expansions);
}

/*-
 *-----------------------------------------------------------------------
 * DirExpandCurly --
 *	Expand curly braces like the C shell, and other wildcards as per
 *	Str_Match.
 *	XXX: if curly expansion yields a result with
 *	no wildcards, the result is placed on the list WITHOUT CHECKING
 *	FOR ITS EXISTENCE.
 *-----------------------------------------------------------------------
 */
static void
DirExpandCurlyi(const char *word, const char *eword, Lst path, Lst expansions)
{
	const char *cp2;/* Pointer for checking for wildcards in
			 * expansion before calling Dir_Expand */
	LIST curled; 	/* Queue of words to expand */
	char *toexpand;	/* Current word to expand */
	bool dowild; 	/* Wildcard left after curlies ? */

	/* Determine once and for all if there is something else going on */
	dowild = false;
	for (cp2 = word; cp2 != eword; cp2++)
		if (*cp2 == '*' || *cp2 == '?' || *cp2 == '[') {
			dowild = true;
			break;
		}

	/* Prime queue with copy of initial word */
	Lst_Init(&curled);
	Lst_EnQueue(&curled, Str_dupi(word, eword));
	while ((toexpand = (char *)Lst_DeQueue(&curled)) != NULL) {
		const char *brace;
		const char *start;
				/* Start of current chunk of brace clause */
		const char *end;/* Character after the closing brace */
		int bracelevel; /* Keep track of nested braces. If we hit
				 * the right brace with bracelevel == 0,
				 * this is the end of the clause. */
		size_t endLen;  /* The length of the ending non-curlied
				 * part of the current expansion */

		/* End case: no curly left to expand */
		brace = strchr(toexpand, '{');
		if (brace == NULL) {
			if (dowild) {
				DirExpandWild(toexpand, path, expansions);
				free(toexpand);
			} else
				Lst_AtEnd(expansions, toexpand);
			continue;
		}

		start = brace+1;

		/* Find the end of the brace clause first, being wary of
		 * nested brace clauses.  */
		for (end = start, bracelevel = 0;; end++) {
			if (*end == '{')
				bracelevel++;
			else if (*end == '\0') {
				Error("Unterminated {} clause \"%s\"", start);
				return;
			} else if (*end == '}' && bracelevel-- == 0)
				break;
		}
		end++;
		endLen = strlen(end);

		for (;;) {
			char *file;	/* To hold current expansion */
			const char *cp;	/* Current position in brace clause */

			/* Find the end of the current expansion */
			for (bracelevel = 0, cp = start;
			    bracelevel != 0 || (*cp != '}' && *cp != ',');
			    cp++) {
				if (*cp == '{')
					bracelevel++;
				else if (*cp == '}')
					bracelevel--;
			}

			/* Build the current combination and enqueue it.  */
			file = emalloc((brace - toexpand) + (cp - start) +
			    endLen + 1);
			if (brace != toexpand)
				memcpy(file, toexpand, brace-toexpand);
			if (cp != start)
				memcpy(file+(brace-toexpand), start, cp-start);
			memcpy(file+(brace-toexpand)+(cp-start), end,
			    endLen + 1);
			Lst_EnQueue(&curled, file);
			if (*cp == '}')
				break;
			start = cp+1;
		}
		free(toexpand);
	}
}

/* Side effects:
 * 	Dir_Expandi will hash directories that were not yet visited */
void
Dir_Expandi(const char *word, const char *eword, Lst path, Lst expansions)
{
	const char	*cp;

	if (DEBUG(DIR)) {
		char *s = Str_dupi(word, eword);
		printf("expanding \"%s\"...", s);
		free(s);
	}

	cp = memchr(word, '{', eword - word);
	if (cp)
		DirExpandCurlyi(word, eword, path, expansions);
	else
		DirExpandWildi(word, eword, path, expansions);

	if (DEBUG(DIR)) {
		Lst_Every(expansions, DirPrintWord);
		fputc('\n', stdout);
	}
}

static void
DirPrintWord(void *word)
{
	printf("%s ", (char *)word);
}


/* XXX: This code is not 100% correct ([^]] fails) */
bool
Dir_HasWildcardsi(const char *name, const char *ename)
{
	const char *cp;
	bool wild = false;
	unsigned long brace = 0, bracket = 0;

	for (cp = name; cp != ename; cp++) {
		switch (*cp) {
		case '{':
			brace++;
			wild = true;
			break;
		case '}':
			if (brace == 0)
				return false;
			brace--;
			break;
		case '[':
			bracket++;
			wild = true;
			break;
		case ']':
			if (bracket == 0)
				return false;
			bracket--;
			break;
		case '?':
		case '*':
			wild = true;
			break;
		default:
			break;
		}
	}
	return wild && bracket == 0 && brace == 0;
}


