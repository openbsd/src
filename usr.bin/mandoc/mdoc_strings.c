/*	$Id: mdoc_strings.c,v 1.3 2009/06/15 01:36:23 schwarze Exp $ */
/*
 * Copyright (c) 2008 Kristaps Dzonsons <kristaps@kth.se>
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

/*
 * Various string-literal operations:  converting scalars to and from
 * strings, etc.
 */

struct mdoc_secname {
	const char	*name;
	int		 flag;
#define	MSECNAME_META	(1 << 0)
};

/* Section names corresponding to mdoc_sec. */

static	const struct mdoc_secname secnames[] = {
	{ "PROLOGUE", MSECNAME_META },
	{ "BODY", MSECNAME_META },
	{ "NAME", 0 },
	{ "LIBRARY", 0 },
	{ "SYNOPSIS", 0 },
	{ "DESCRIPTION", 0 },
	{ "IMPLEMENTATION NOTES", 0 },
	{ "RETURN VALUES", 0 },
	{ "ENVIRONMENT", 0 },
	{ "FILES", 0 },
	{ "EXAMPLES", 0 },
	{ "DIAGNOSTICS", 0 },
	{ "COMPATIBILITY", 0 },
	{ "ERRORS", 0 },
	{ "SEE ALSO", 0 },
	{ "STANDARDS", 0 },
	{ "HISTORY", 0 },
	{ "AUTHORS", 0 },
	{ "CAVEATS", 0 },
	{ "BUGS", 0 },
	{ NULL, 0 }
};


size_t
mdoc_isescape(const char *p)
{
	size_t		 c;
	
	if ('\\' != *p++)
		return(0);

	switch (*p) {
	case ('\\'):
		/* FALLTHROUGH */
	case ('\''):
		/* FALLTHROUGH */
	case ('`'):
		/* FALLTHROUGH */
	case ('q'):
		/* FALLTHROUGH */
	case ('-'):
		/* FALLTHROUGH */
	case ('~'):
		/* FALLTHROUGH */
	case ('^'):
		/* FALLTHROUGH */
	case ('%'):
		/* FALLTHROUGH */
	case ('0'):
		/* FALLTHROUGH */
	case (' '):
		/* FALLTHROUGH */
	case ('|'):
		/* FALLTHROUGH */
	case ('&'):
		/* FALLTHROUGH */
	case ('.'):
		/* FALLTHROUGH */
	case (':'):
		/* FALLTHROUGH */
	case ('e'):
		return(2);
	case ('*'):
		if (0 == *++p || ! isgraph((u_char)*p))
			return(0);
		switch (*p) {
		case ('('):
			if (0 == *++p || ! isgraph((u_char)*p))
				return(0);
			return(4);
		case ('['):
			for (c = 3, p++; *p && ']' != *p; p++, c++)
				if ( ! isgraph((u_char)*p))
					break;
			return(*p == ']' ? c : 0);
		default:
			break;
		}
		return(3);
	case ('('):
		if (0 == *++p || ! isgraph((u_char)*p))
			return(0);
		if (0 == *++p || ! isgraph((u_char)*p))
			return(0);
		return(4);
	case ('['):
		break;
	default:
		return(0);
	}

	for (c = 3, p++; *p && ']' != *p; p++, c++)
		if ( ! isgraph((u_char)*p))
			break;

	return(*p == ']' ? c : 0);
}


int
mdoc_iscdelim(char p)
{

	switch (p) {
	case('.'):
		/* FALLTHROUGH */
	case(','):
		/* FALLTHROUGH */
	case(';'):
		/* FALLTHROUGH */
	case(':'):
		/* FALLTHROUGH */
	case('?'):
		/* FALLTHROUGH */
	case('!'):
		/* FALLTHROUGH */
	case('('):
		/* FALLTHROUGH */
	case(')'):
		/* FALLTHROUGH */
	case('['):
		/* FALLTHROUGH */
	case(']'):
		/* FALLTHROUGH */
	case('{'):
		/* FALLTHROUGH */
	case('}'):
		return(1);
	default:
		break;
	}

	return(0);
}


int
mdoc_isdelim(const char *p)
{

	if (0 == *p)
		return(0);
	if (0 != *(p + 1))
		return(0);
	return(mdoc_iscdelim(*p));
}


enum mdoc_sec 
mdoc_atosec(const char *p)
{
	const struct mdoc_secname *n;
	int			   i;

	for (i = 0, n = secnames; n->name; n++, i++)
		if ( ! (n->flag & MSECNAME_META))
			if (0 == strcmp(p, n->name))
				return((enum mdoc_sec)i);

	return(SEC_CUSTOM);
}


time_t
mdoc_atotime(const char *p)
{
	struct tm	 tm;
	char		*pp;

	(void)memset(&tm, 0, sizeof(struct tm));

	if (0 == strcmp(p, "$Mdocdate: June 15 2009 $"))
		return(time(NULL));
	if ((pp = strptime(p, "$Mdocdate: June 15 2009 $", &tm)) && 0 == *pp)
		return(mktime(&tm));
	/* XXX - this matches "June 1999", which is wrong. */
	if ((pp = strptime(p, "%b %d %Y", &tm)) && 0 == *pp)
		return(mktime(&tm));
	if ((pp = strptime(p, "%b %d, %Y", &tm)) && 0 == *pp)
		return(mktime(&tm));

	return(0);
}


size_t
mdoc_macro2len(int macro)
{

	switch (macro) {
	case(MDOC_Ad):
		return(12);
	case(MDOC_Ao):
		return(12);
	case(MDOC_An):
		return(12);
	case(MDOC_Aq):
		return(12);
	case(MDOC_Ar):
		return(12);
	case(MDOC_Bo):
		return(12);
	case(MDOC_Bq):
		return(12);
	case(MDOC_Cd):
		return(12);
	case(MDOC_Cm):
		return(10);
	case(MDOC_Do):
		return(10);
	case(MDOC_Dq):
		return(12);
	case(MDOC_Dv):
		return(12);
	case(MDOC_Eo):
		return(12);
	case(MDOC_Em):
		return(10);
	case(MDOC_Er):
		return(12);
	case(MDOC_Ev):
		return(15);
	case(MDOC_Fa):
		return(12);
	case(MDOC_Fl):
		return(10);
	case(MDOC_Fo):
		return(16);
	case(MDOC_Fn):
		return(16);
	case(MDOC_Ic):
		return(10);
	case(MDOC_Li):
		return(16);
	case(MDOC_Ms):
		return(6);
	case(MDOC_Nm):
		return(10);
	case(MDOC_No):
		return(12);
	case(MDOC_Oo):
		return(10);
	case(MDOC_Op):
		return(14);
	case(MDOC_Pa):
		return(32);
	case(MDOC_Pf):
		return(12);
	case(MDOC_Po):
		return(12);
	case(MDOC_Pq):
		return(12);
	case(MDOC_Ql):
		return(16);
	case(MDOC_Qo):
		return(12);
	case(MDOC_So):
		return(12);
	case(MDOC_Sq):
		return(12);
	case(MDOC_Sy):
		return(6);
	case(MDOC_Sx):
		return(16);
	case(MDOC_Tn):
		return(10);
	case(MDOC_Va):
		return(12);
	case(MDOC_Vt):
		return(12);
	case(MDOC_Xr):
		return(10);
	default:
		break;
	};
	return(0);
}
