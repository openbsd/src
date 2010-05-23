/*	$Id: mdoc_strings.c,v 1.18 2010/05/23 22:45:00 schwarze Exp $ */
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "mandoc.h"
#include "libmdoc.h"

static	const char * const secnames[SEC__MAX] = {
	NULL,
	"NAME",
	"LIBRARY",
	"SYNOPSIS",
	"DESCRIPTION",
	"IMPLEMENTATION NOTES",
	"RETURN VALUES",
	"ENVIRONMENT",
	"FILES",
	"EXIT STATUS",
	"EXAMPLES",
	"DIAGNOSTICS",
	"COMPATIBILITY",
	"ERRORS",
	"SEE ALSO",
	"STANDARDS",
	"HISTORY",
	"AUTHORS",
	"CAVEATS",
	"BUGS",
	"SECURITY CONSIDERATIONS",
	NULL
};

/* 
 * FIXME: this is repeated in print_text() (html.c) and term_word()
 * (term.c).
 */
enum mdelim
mdoc_iscdelim(char p)
{

	switch (p) {
	case('('):
		/* FALLTHROUGH */
	case('['):
		return(DELIM_OPEN);
	case('|'):
		return(DELIM_MIDDLE);
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
	case(')'):
		/* FALLTHROUGH */
	case(']'):
		return(DELIM_CLOSE);
	default:
		break;
	}

	return(DELIM_NONE);
}


enum mdelim
mdoc_isdelim(const char *p)
{

	if ('\0' == p[0])
		return(DELIM_NONE);
	if ('\0' == p[1])
		return(mdoc_iscdelim(p[0]));

	/*
	 * XXX; account for groff bubu where the \*(Ba reserved string
	 * is treated in exactly the same way as the vertical bar.  This
	 * is the only function that checks for this.
	 */
	return(strcmp(p, "\\*(Ba") ? DELIM_NONE : DELIM_MIDDLE);
}


enum mdoc_sec 
mdoc_str2sec(const char *p)
{
	int		 i;

	for (i = 0; i < (int)SEC__MAX; i++) 
		if (secnames[i] && 0 == strcmp(p, secnames[i]))
			return((enum mdoc_sec)i);

	return(SEC_CUSTOM);
}


/* FIXME: move this into an editable .in file. */
size_t
mdoc_macro2len(enum mdoct macro)
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
