%{
/*
 * Copyright (c) 1997 - 2004 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden). 
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 *
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright 
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the distribution. 
 *
 * 3. Neither the name of the Institute nor the names of its contributors 
 *    may be used to endorse or promote products derived from this software 
 *    without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE 
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS 
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE. 
 */

/* $KTH: lex.l,v 1.24 2004/10/13 17:40:21 lha Exp $ */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#undef ECHO
#include "symbol.h"
#include "parse.h"
#include "lex.h"
#include "gen_locl.h"

static unsigned lineno = 1;

#define YY_NO_UNPUT

#undef ECHO

static void handle_comment(int type);

%}


%%
INTEGER			{ return INTEGER; }
BOOLEAN			{ return BOOLEAN; }
IMPORTS			{ return IMPORTS; }
FROM			{ return FROM; }
SEQUENCE		{ return SEQUENCE; }
CHOICE			{ return CHOICE; }
OF			{ return OF; }
OCTET			{ return OCTET; }
STRING			{ return STRING; }
GeneralizedTime		{ return GeneralizedTime; }
GeneralString		{ return GeneralString; }
UTF8String		{ return UTF8String; }
NULL			{ return NULLTYPE; }
BIT			{ return BIT; }
APPLICATION		{ return APPLICATION; }
OPTIONAL		{ return OPTIONAL; }
BEGIN			{ return TBEGIN; }
END			{ return END; }
DEFAULT			{ return DEFAULT; }
DEFINITIONS		{ return DEFINITIONS; }
ENUMERATED		{ return ENUMERATED; }
EXTERNAL		{ return EXTERNAL; }
OBJECT			{ return OBJECT; }
IDENTIFIER		{ return IDENTIFIER; }
[-,;{}()|\"]		{ return *yytext; }
"["			{ return *yytext; }
"]"			{ return *yytext; }
::=			{ return EEQUAL; }
--			{ handle_comment(0); }
\/\*			{ handle_comment(1); }
0x[0-9A-Fa-f]+|[0-9]+	{ char *e, *y = yytext;
			  yylval.constant = strtol((const char *)yytext,
						   &e, 0);
			  if(e == y) 
			    error_message("malformed constant (%s)", yytext); 
			  else
			    return CONSTANT;
			}
[A-Za-z][-A-Za-z0-9_]*	{
			  yylval.name =  strdup ((const char *)yytext);
			  return IDENT;
			}
[ \t]			;
\n			{ ++lineno; }
\.\.\.			{ return DOTDOTDOT; }
\.\.			{ return DOTDOT; }
.			{ error_message("Ignoring char(%c)\n", *yytext); }
%%

#ifndef yywrap /* XXX */
int
yywrap () 
{
     return 1;
}
#endif

void
error_message (const char *format, ...)
{
     va_list args;

     va_start (args, format);
     fprintf (stderr, "%s:%d: ", filename(), lineno);
     vfprintf (stderr, format, args);
     va_end (args);
}

static void
handle_comment(int type)
{
    int c;
    int start_lineno = lineno;
    if(type == 0) {
	int f = 0;
	while((c = input()) != EOF) {
	    if(f && c == '-')
		return;
	    if(c == '-') {
		f = 1;
		continue;
	    }
	    if(c == '\n') {
		lineno++;
		return;
	    }
	    f = 0;
	}
    } else {
	int level = 1;
	int seen_star = 0;
	int seen_slash = 0;
	while((c = input()) != EOF) {
	    if(c == '/') {
		if(seen_star) {
		    if(--level == 0)
			return;
		    seen_star = 0;
		    continue;
		}
		seen_slash = 1;
		continue;
	    }
	    if(c == '*') {
		if(seen_slash) {
		    level++;
		    seen_star = seen_slash = 0;
		    continue;
		} 
		seen_star = 1;
		continue;
	    }
	    seen_star = seen_slash = 0;
	    if(c == '\n') {
		lineno++;
		continue;
	    }
	}
    }
    if(c == EOF)
	error_message("unterminated comment, possibly started on line %d\n", start_lineno);
}
