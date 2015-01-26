/*	$OpenBSD: tbl_opts.c,v 1.9 2015/01/26 00:54:09 schwarze Exp $ */
/*
 * Copyright (c) 2009, 2010, 2011 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2015 Ingo Schwarze <schwarze@openbsd.org>
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

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mandoc.h"
#include "libmandoc.h"
#include "libroff.h"

enum	tbl_ident {
	KEY_CENTRE = 0,
	KEY_DELIM,
	KEY_EXPAND,
	KEY_BOX,
	KEY_DBOX,
	KEY_ALLBOX,
	KEY_TAB,
	KEY_LINESIZE,
	KEY_NOKEEP,
	KEY_DPOINT,
	KEY_NOSPACE,
	KEY_FRAME,
	KEY_DFRAME,
	KEY_MAX
};

struct	tbl_phrase {
	const char	*name;
	int		 key;
	enum tbl_ident	 ident;
};

/* Handle Commonwealth/American spellings. */
#define	KEY_MAXKEYS	 14

static	const struct tbl_phrase keys[KEY_MAXKEYS] = {
	{ "center",	 TBL_OPT_CENTRE,	KEY_CENTRE},
	{ "centre",	 TBL_OPT_CENTRE,	KEY_CENTRE},
	{ "delim",	 0,			KEY_DELIM},
	{ "expand",	 TBL_OPT_EXPAND,	KEY_EXPAND},
	{ "box",	 TBL_OPT_BOX,		KEY_BOX},
	{ "doublebox",	 TBL_OPT_DBOX,		KEY_DBOX},
	{ "allbox",	 TBL_OPT_ALLBOX,	KEY_ALLBOX},
	{ "frame",	 TBL_OPT_BOX,		KEY_FRAME},
	{ "doubleframe", TBL_OPT_DBOX,		KEY_DFRAME},
	{ "tab",	 0,			KEY_TAB},
	{ "linesize",	 0,			KEY_LINESIZE},
	{ "nokeep",	 TBL_OPT_NOKEEP,	KEY_NOKEEP},
	{ "decimalpoint", 0,			KEY_DPOINT},
	{ "nospaces",	 TBL_OPT_NOSPACE,	KEY_NOSPACE},
};

static	void		 arg(struct tbl_node *, int,
				const char *, int *, enum tbl_ident);


static void
arg(struct tbl_node *tbl, int ln, const char *p, int *pos, enum tbl_ident key)
{
	const char	*optname;
	int		 len, want;

	while (isspace((unsigned char)p[*pos]))
		(*pos)++;

	/* Arguments are enclosed in parentheses. */

	len = 0;
	if (p[*pos] == '(') {
		(*pos)++;
		while (p[*pos + len] != ')')
			len++;
	}

	switch (key) {
	case KEY_DELIM:
		optname = "delim";
		want = 2;
		break;
	case KEY_TAB:
		optname = "tab";
		want = 1;
		if (len == want)
			tbl->opts.tab = p[*pos];
		break;
	case KEY_LINESIZE:
		optname = "linesize";
		want = 0;
		break;
	case KEY_DPOINT:
		optname = "decimalpoint";
		want = 1;
		if (len == want)
			tbl->opts.decimal = p[*pos];
		break;
	default:
		abort();
		/* NOTREACHED */
	}

	if (len == 0)
		mandoc_msg(MANDOCERR_TBLOPT_NOARG,
		    tbl->parse, ln, *pos, optname);
	else if (want && len != want)
		mandoc_vmsg(MANDOCERR_TBLOPT_ARGSZ,
		    tbl->parse, ln, *pos,
		    "%s want %d have %d", optname, want, len);

	*pos += len;
	if (p[*pos] == ')')
		(*pos)++;
}

/*
 * Parse one line of options up to the semicolon.
 * Each option can be preceded by blanks and/or commas,
 * and some options are followed by arguments.
 */
void
tbl_option(struct tbl_node *tbl, int ln, const char *p)
{
	int		 i, pos, len;

	pos = 0;
	for (;;) {
		while (isspace((unsigned char)p[pos]) || p[pos] == ',')
			pos++;

		if (p[pos] == ';')
			return;

		/* Parse one option name. */

		len = 0;
		while (isalpha((unsigned char)p[pos + len]))
			len++;

		if (len == 0) {
			mandoc_vmsg(MANDOCERR_TBLOPT_ALPHA,
			    tbl->parse, ln, pos, "%c", p[pos]);
			pos++;
			continue;
		}

		/* Look up the option name. */

		i = 0;
		while (i < KEY_MAXKEYS &&
		    (strncasecmp(p + pos, keys[i].name, len) ||
		     keys[i].name[len] != '\0'))
			i++;

		if (i == KEY_MAXKEYS) {
			mandoc_vmsg(MANDOCERR_TBLOPT_BAD, tbl->parse,
			    ln, pos, "%.*s", len, p + pos);
			pos += len;
			continue;
		}

		/* Handle the option. */

		pos += len;
		if (keys[i].key)
			tbl->opts.opts |= keys[i].key;
		else
			arg(tbl, ln, p, &pos, keys[i].ident);
	}
}
