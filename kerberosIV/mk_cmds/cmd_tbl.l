%{
/*	$Id: cmd_tbl.l,v 1.1.1.1 1995/12/14 06:52:48 tholo Exp $	*/

/*-
 * Copyright 1987, 1988 by the Student Information Processing Board
 *	of the Massachusetts Institute of Technology
 *
 * Permission to use, copy, modify, and distribute this software
 * and its documentation for any purpose and without fee is
 * hereby granted, provided that the above copyright notice
 * appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation,
 * and that the names of M.I.T. and the M.I.T. S.I.P.B. not be
 * used in advertising or publicity pertaining to distribution
 * of the software without specific, written prior permission.
 * M.I.T. and the M.I.T. S.I.P.B. make no representations about
 * the suitability of this software for any purpose.  It is
 * provided "as is" without express or implied warranty.
 */
%}

N	[0-9]
PC	[^\"]
AN      [A-Z_a-z0-9]

%{
unsigned lineno = 1;

static	l_command_table(), l_request(), l_unimplemented(), l_end(),
	l_quoted_string(), l_string();
%}

%%

%{
/* emptied */
%}

command_table	return l_command_table();
request		return l_request();
unimplemented	return l_unimplemented();
end		return l_end();

[\t ]		;

\n		++lineno;

\"{PC}*\"	return l_quoted_string();

{AN}*		return l_string();

#.*\n		++lineno;

.		return (*yytext);

%%

/*
 * User-subroutines section.
 *
 * Have to put all this stuff here so that the include file
 * from YACC output can be included, since LEX doesn't allow
 * an include file before the code it generates for the above
 * rules.
 */

#include <string.h>
#include "y.tab.h"
/* #include "copyright.h" */

extern char *last_token, *ds();

static l_command_table()
{
     last_token = "command_table";
     return COMMAND_TABLE;
}

static l_request()
{
     last_token = "request";
     return REQUEST;
}

static l_unimplemented()
{
     last_token = "unimplemented";
     return UNIMPLEMENTED;
}

static l_end()
{
     last_token = "end";
     return END;
}

static l_quoted_string()
{
     register char *p;
     yylval.dynstr = ds(yytext+1);
     if (p=strrchr(yylval.dynstr, '"'))
	  *p='\0';
     last_token = ds(yylval.dynstr);
     return STRING;
}

static l_string()
{
     yylval.dynstr = ds(yytext);
     last_token = ds(yylval.dynstr);
     return STRING;
}
