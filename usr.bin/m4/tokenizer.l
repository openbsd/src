%{
/* $OpenBSD: tokenizer.l,v 1.2 2004/05/12 21:28:35 espie Exp $ */
/*
 * Copyright (c) 2004 Marc Espie <espie@cvs.openbsd.org>
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
#include "parser.h"
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <limits.h>

extern int32_t yylval;

int32_t number(void);
%}

delim 	[ \t\n]
ws	{delim}+
hex	0[xX][0-9a-fA-F]+
oct	0[0-7]*
dec	[1-9][0-9]*

%%
{ws}			{/* just skip it */}
{hex}|{oct}|{dec}	{ yylval = number(); return(NUMBER); }
"<="			{ return(LE); }
">="			{ return(GE); }
"<<"			{ return(LSHIFT); }
">>"			{ return(RSHIFT); }
"=="			{ return(EQ); }
"!="			{ return(NE); }
"&&"			{ return(LAND); }
"||"			{ return(LOR); }
.			{ return yytext[0]; }
%%

int32_t
number()
{
	long l;

	errno = 0;
	l = strtol(yytext, NULL, 0);
	if (((l == LONG_MAX || l == LONG_MIN) && errno == ERANGE) ||
	    l > 0x7fffffff || l < (-0x7fffffff - 1)) {
		fprintf(stderr, "m4: numeric overflow in expr: %s\n", yytext);
	}
	return l;

}
