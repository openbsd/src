/*	$OpenBSD: complete.c,v 1.28 2015/01/16 06:40:08 deraadt Exp $	*/
/*	$NetBSD: complete.c,v 1.10 1997/08/18 10:20:18 lukem Exp $	*/

/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Luke Mewburn.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef SMALL

/*
 * FTP user program - command and file completion routines
 */

#include <ctype.h>
#include <err.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ftp_var.h"

static int	     comparstr(const void *, const void *);
static unsigned char complete_ambiguous(char *, int, StringList *);
static unsigned char complete_command(char *, int);
static unsigned char complete_local(char *, int);
static unsigned char complete_remote(char *, int);
static void          ftpvis(char *, size_t, const char *, size_t);

static int
comparstr(const void *a, const void *b)
{
	return (strcmp(*(char **)a, *(char **)b));
}

/*
 * Determine if complete is ambiguous. If unique, insert.
 * If no choices, error. If unambiguous prefix, insert that.
 * Otherwise, list choices. words is assumed to be filtered
 * to only contain possible choices.
 * Args:
 *	word	word which started the match
 *	list	list by default
 *	words	stringlist containing possible matches
 */
static unsigned char
complete_ambiguous(char *word, int list, StringList *words)
{
	char insertstr[PATH_MAX * 2];
	char *lastmatch;
	int i, j;
	size_t matchlen, wordlen;

	wordlen = strlen(word);
	if (words->sl_cur == 0)
		return (CC_ERROR);	/* no choices available */

	if (words->sl_cur == 1) {	/* only once choice available */
		char *p = words->sl_str[0] + wordlen;
		ftpvis(insertstr, sizeof(insertstr), p, strlen(p));
		if (el_insertstr(el, insertstr) == -1)
			return (CC_ERROR);
		else
			return (CC_REFRESH);
	}

	if (!list) {
		lastmatch = words->sl_str[0];
		matchlen = strlen(lastmatch);
		for (i = 1 ; i < words->sl_cur ; i++) {
			for (j = wordlen ; j < strlen(words->sl_str[i]); j++)
				if (lastmatch[j] != words->sl_str[i][j])
					break;
			if (j < matchlen)
				matchlen = j;
		}
		if (matchlen > wordlen) {
			ftpvis(insertstr, sizeof(insertstr),
			    lastmatch + wordlen, matchlen - wordlen);
			if (el_insertstr(el, insertstr) == -1)
				return (CC_ERROR);
			else	
					/*
					 * XXX: really want CC_REFRESH_BEEP
					 */
				return (CC_REFRESH);
		}
	}

	putc('\n', ttyout);
	qsort(words->sl_str, words->sl_cur, sizeof(char *), comparstr);
	list_vertical(words);
	return (CC_REDISPLAY);
}

/*
 * Complete a command
 */
static unsigned char
complete_command(char *word, int list)
{
	struct cmd *c;
	StringList *words;
	size_t wordlen;
	unsigned char rv;

	words = sl_init();
	wordlen = strlen(word);

	for (c = cmdtab; c->c_name != NULL; c++) {
		if (wordlen > strlen(c->c_name))
			continue;
		if (strncmp(word, c->c_name, wordlen) == 0)
			sl_add(words, c->c_name);
	}

	rv = complete_ambiguous(word, list, words);
	sl_free(words, 0);
	return (rv);
}

/*
 * Complete a local file
 */
static unsigned char
complete_local(char *word, int list)
{
	StringList *words;
	char dir[PATH_MAX];
	char *file;
	DIR *dd;
	struct dirent *dp;
	unsigned char rv;

	if ((file = strrchr(word, '/')) == NULL) {
		dir[0] = '.';
		dir[1] = '\0';
		file = word;
	} else {
		if (file == word) {
			dir[0] = '/';
			dir[1] = '\0';
		} else {
			(void)strlcpy(dir, word, (size_t)(file - word) + 1);
		}
		file++;
	}

	if ((dd = opendir(dir)) == NULL)
		return (CC_ERROR);

	words = sl_init();

	for (dp = readdir(dd); dp != NULL; dp = readdir(dd)) {
		if (!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, ".."))
			continue;
		if (strlen(file) > dp->d_namlen)
			continue;
		if (strncmp(file, dp->d_name, strlen(file)) == 0) {
			char *tcp;

			tcp = strdup(dp->d_name);
			if (tcp == NULL)
				errx(1, "Can't allocate memory for local dir");
			sl_add(words, tcp);
		}
	}
	closedir(dd);

	rv = complete_ambiguous(file, list, words);
	sl_free(words, 1);
	return (rv);
}

/*
 * Complete a remote file
 */
static unsigned char
complete_remote(char *word, int list)
{
	static StringList *dirlist;
	static char	 lastdir[PATH_MAX];
	StringList	*words;
	char		 dir[PATH_MAX];
	char		*file, *cp;
	int		 i;
	unsigned char	 rv;

	char *dummyargv[] = { "complete", dir, NULL };

	if ((file = strrchr(word, '/')) == NULL) {
		dir[0] = '.';
		dir[1] = '\0';
		file = word;
	} else {
		cp = file;
		while (*cp == '/' && cp > word)
			cp--;
		(void)strlcpy(dir, word, (size_t)(cp - word + 2));
		file++;
	}

	if (dirchange || strcmp(dir, lastdir) != 0) {	/* dir not cached */
		char *emesg;

		sl_free(dirlist, 1);
		dirlist = sl_init();

		mflag = 1;
		emesg = NULL;
		if (debug)
			(void)putc('\n', ttyout);
		while ((cp = remglob(dummyargv, 0, &emesg)) != NULL) {
			char *tcp;

			if (!mflag)
				continue;
			if (*cp == '\0') {
				mflag = 0;
				continue;
			}
			tcp = strrchr(cp, '/');
			if (tcp)
				tcp++;
			else
				tcp = cp;
			tcp = strdup(tcp);
			if (tcp == NULL)
				errx(1, "Can't allocate memory for remote dir");
			sl_add(dirlist, tcp);
		}
		if (emesg != NULL) {
			fprintf(ttyout, "\n%s\n", emesg);
			return (CC_REDISPLAY);
		}
		(void)strlcpy(lastdir, dir, sizeof lastdir);
		dirchange = 0;
	}

	words = sl_init();
	for (i = 0; i < dirlist->sl_cur; i++) {
		cp = dirlist->sl_str[i];
		if (strlen(file) > strlen(cp))
			continue;
		if (strncmp(file, cp, strlen(file)) == 0)
			sl_add(words, cp);
	}
	rv = complete_ambiguous(file, list, words);
	sl_free(words, 0);
	return (rv);
}

/*
 * Generic complete routine
 */
unsigned char
complete(EditLine *el, int ch)
{
	static char word[FTPBUFLEN];
	static int lastc_argc, lastc_argo;
	struct cmd *c;
	const LineInfo *lf;
	int celems, dolist;
	size_t len;

	ch = ch;		/* not used */
	lf = el_line(el);
	len = lf->lastchar - lf->buffer;
	if (len >= sizeof(line))
		return (CC_ERROR);
	(void)memcpy(line, lf->buffer, len);
	line[len] = '\0';
	cursor_pos = line + (lf->cursor - lf->buffer);
	lastc_argc = cursor_argc;	/* remember last cursor pos */
	lastc_argo = cursor_argo;
	makeargv();			/* build argc/argv of current line */

	if (cursor_argo >= sizeof(word))
		return (CC_ERROR);

	dolist = 0;
			/* if cursor and word is same, list alternatives */
	if (lastc_argc == cursor_argc && lastc_argo == cursor_argo
	    && strncmp(word, margv[cursor_argc], cursor_argo) == 0)
		dolist = 1;
	else if (cursor_argo)
		memcpy(word, margv[cursor_argc], cursor_argo);
	word[cursor_argo] = '\0';

	if (cursor_argc == 0)
		return (complete_command(word, dolist));

	c = getcmd(margv[0]);
	if (c == (struct cmd *)-1 || c == 0)
		return (CC_ERROR);
	celems = strlen(c->c_complete);

		/* check for 'continuation' completes (which are uppercase) */
	if ((cursor_argc > celems) && (celems > 0)
	    && isupper(c->c_complete[celems-1]))
		cursor_argc = celems;

	if (cursor_argc > celems)
		return (CC_ERROR);

	switch (c->c_complete[cursor_argc - 1]) {
	case 'l':			/* local complete */
	case 'L':
		return (complete_local(word, dolist));
	case 'r':			/* remote complete */
	case 'R':
		if (connected != -1) {
			fputs("\nMust be logged in to complete.\n", ttyout);
			return (CC_REDISPLAY);
		}
		return (complete_remote(word, dolist));
	case 'c':			/* command complete */
	case 'C':
		return (complete_command(word, dolist));
	case 'n':			/* no complete */
		return (CC_ERROR);
	}

	return (CC_ERROR);
}

/*
 * Copy characters from src into dst, \ quoting characters that require it.
 */
static void
ftpvis(char *dst, size_t dstlen, const char *src, size_t srclen)
{
	size_t	di, si;

	di = si = 0;
	while (di + 1 < dstlen && si < srclen && src[si] != '\0') {
		switch (src[si]) {
		case '\\':
		case ' ':
		case '\t':
		case '\r':
		case '\n':
		case '"':
			/* Need room for two characters and NUL, avoiding
			 * incomplete escape sequences at end of dst. */
			if (di + 3 >= dstlen)
				break;
			dst[di++] = '\\';
			/* FALLTHROUGH */
		default:
			dst[di++] = src[si++];
		}
	}
	if (dstlen != 0)
		dst[di] = '\0';
}
#endif /* !SMALL */
