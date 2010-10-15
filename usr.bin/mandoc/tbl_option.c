/*	$Id: tbl_option.c,v 1.2 2010/10/15 21:33:47 schwarze Exp $ */
/*
 * Copyright (c) 2009 Kristaps Dzonsons <kristaps@kth.se>
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
#include <sys/queue.h>

#include <stdlib.h>
#include <string.h>

#include "out.h"
#include "term.h"
#include "tbl_extern.h"

struct	tbl_phrase {
	char		*name;
	int		 key;
	int		 ident;
#define	KEY_CENTRE	 0
#define	KEY_DELIM	 1
#define	KEY_EXPAND	 2
#define	KEY_BOX		 3
#define	KEY_DBOX	 4
#define	KEY_ALLBOX	 5
#define	KEY_TAB		 6
#define	KEY_LINESIZE	 7
#define	KEY_NOKEEP	 8
#define	KEY_DPOINT	 9
#define	KEY_NOSPACE	 10
#define	KEY_FRAME	 11
#define	KEY_DFRAME	 12
};

#define	KEY_MAXKEYS	 14

static	const struct tbl_phrase keys[KEY_MAXKEYS] = {
	{ "center",	 TBL_OPT_CENTRE,	KEY_CENTRE},
	{ "centre",	 TBL_OPT_CENTRE,	KEY_CENTRE},
	{ "delim",	 0,	       		KEY_DELIM},
	{ "expand",	 TBL_OPT_EXPAND,	KEY_EXPAND},
	{ "box",	 TBL_OPT_BOX,   	KEY_BOX},
	{ "doublebox",	 TBL_OPT_DBOX,  	KEY_DBOX},
	{ "allbox",	 TBL_OPT_ALLBOX,	KEY_ALLBOX},
	{ "frame",	 TBL_OPT_BOX,		KEY_FRAME},
	{ "doubleframe", TBL_OPT_DBOX,		KEY_DFRAME},
	{ "tab",	 0,			KEY_TAB},
	{ "linesize",	 0,			KEY_LINESIZE},
	{ "nokeep",	 TBL_OPT_NOKEEP,	KEY_NOKEEP},
	{ "decimalpoint", 0,			KEY_DPOINT},
	{ "nospaces",	 TBL_OPT_NOSPACE,	KEY_NOSPACE},
};

static	int		 arg(struct tbl *, const char *, 
				int, const char *, int *, int);
static	int		 opt(struct tbl *, const char *, 
				int, const char *, int *);

static int
arg(struct tbl *tbl, const char *f, int ln,
		const char *p, int *pos, int key)
{
	const char	*buf;
	int		 sv;

again:
	sv = *pos;

	switch (tbl_next(p, pos)) {
	case (TBL_TOK_OPENPAREN):
		break;
	case (TBL_TOK_SPACE):
		/* FALLTHROUGH */
	case (TBL_TOK_TAB):
		goto again;
	default:
		return(tbl_errx(tbl, ERR_SYNTAX, f, ln, sv));
	}

	sv = *pos;

	switch (tbl_next(p, pos)) {
	case (TBL_TOK_WORD):
		break;
	default:
		return(tbl_errx(tbl, ERR_SYNTAX, f, ln, sv));
	}

	buf = tbl_last();

	switch (key) {
	case (KEY_DELIM):
		if (2 != strlen(buf))
			return(tbl_errx(tbl, ERR_SYNTAX, f, ln, sv));
		tbl->delims[0] = buf[0];
		tbl->delims[1] = buf[1];
		break;
	case (KEY_TAB):
		if (1 != strlen(buf))
			return(tbl_errx(tbl, ERR_SYNTAX, f, ln, sv));
		tbl->tab = buf[0];
		break;
	case (KEY_LINESIZE):
		if (-1 == (tbl->linesize = tbl_last_uint()))
			return(tbl_errx(tbl, ERR_SYNTAX, f, ln, sv));
		break;
	case (KEY_DPOINT):
		if (1 != strlen(buf))
			return(tbl_errx(tbl, ERR_SYNTAX, f, ln, sv));
		tbl->decimal = buf[0];
		break;
	default:
		abort();
	}

	sv = *pos;

	switch (tbl_next(p, pos)) {
	case (TBL_TOK_CLOSEPAREN):
		break;
	default:
		return(tbl_errx(tbl, ERR_SYNTAX, f, ln, sv));
	}

	return(1);
}


static int
opt(struct tbl *tbl, const char *f, int ln, const char *p, int *pos)
{
	int		 i, sv;

again:
	sv = *pos;

	/*
	 * EBNF describing this section:
	 *
	 * options	::= option_list [:space:]* [;][\n]
	 * option_list	::= option option_tail
	 * option_tail	::= [:space:]+ option_list |
	 * 		::= epsilon
	 * option	::= [:alpha:]+ args
	 * args		::= [:space:]* [(] [:alpha:]+ [)]
	 */

	switch (tbl_next(p, pos)) {
	case (TBL_TOK_WORD):
		break;
	case (TBL_TOK_SPACE):
		/* FALLTHROUGH */
	case (TBL_TOK_TAB):
		goto again;
	case (TBL_TOK_SEMICOLON):
		tbl->part = TBL_PART_LAYOUT;
		return(1);
	default:
		return(tbl_errx(tbl, ERR_SYNTAX, f, ln, sv));
	}

	for (i = 0; i < KEY_MAXKEYS; i++) {
		if (strcasecmp(tbl_last(), keys[i].name))
			continue;
		if (keys[i].key) 
			tbl->opts |= keys[i].key;
		else if ( ! arg(tbl, f, ln, p, pos, keys[i].ident))
			return(0);

		break;
	}

	if (KEY_MAXKEYS == i)
		return(tbl_errx(tbl, ERR_OPTION, f, ln, sv));

	return(opt(tbl, f, ln, p, pos));
}


int
tbl_option(struct tbl *tbl, const char *f, int ln, const char *p)
{
	int		 pos;

	pos = 0;
	return(opt(tbl, f, ln, p, &pos));
}
