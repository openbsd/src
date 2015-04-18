/*	$OpenBSD: lex.l,v 1.20 2015/04/18 18:28:37 deraadt Exp $	*/

/*
 * Copyright 2002 Niels Provos <provos@citi.umich.edu>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Niels Provos.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
%x quote

%{
#include <sys/types.h>
#include <sys/tree.h>

#include <sys/time.h>

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <err.h>
#include <stdarg.h>
#include <string.h>

#include "intercept.h"
#include "systrace.h"
#include "y.tab.h"

extern int yyerror(const char *fmt, ...);
int yylex(void);

char *mystring;
int myoff;

char quotestr[8192];
int quoteescape;

#define YY_INPUT(buf,result,max_size) \
{ \
	int len = strlen(mystring + myoff); \
	if (max_size < len) \
		len = max_size; \
	if (len == 0) \
		result = YY_NULL; \
	else { \
		memcpy(buf, mystring + myoff, len); \
		myoff += len; \
		result = len; \
	} \
}
%}

%%
ask		{ return ASK; }
deny		{ return DENY; }
permit		{ return PERMIT; }
and		{ return AND; }
or		{ return OR; }
not		{ return NOT; }
match		{ return MATCH; }
then		{ return THEN; }
eq		{ return EQ; }
neq		{ return NEQ; }
sub		{ return SUB; }
nsub		{ return NSUB; }
inpath		{ return INPATH; }
re		{ return RE; }
log		{ return LOG; }
true		{ return TRUE; }
"->"		{ return THEN; }
\(		{ return LBRACE; }
\)		{ return RBRACE; }
if		{ return IF; }
user		{ return USER; }
group		{ return GROUP; }
as		{ return AS; }
":"		{ return COLON; }
","		{ return COMMA; }
"="		{ return EQUAL; }
"!="		{ return NEQUAL; }
"<"		{ return LESSER; }
">"		{ return GREATER; }
[\_\$A-Za-z][\.\(\)\/A-Za-z_\-0-9]*\$? { yylval.string = strdup(yytext); return STRING; }
[0-9]+		{
		  const char *errstr;
		  yylval.number = strtonum(yytext, 0, INT_MAX, &errstr);
		  if (errstr) {
			yyerror("number %s: %s", yytext, errstr);
		  }
		  return NUMBER;
		}
\"		{ BEGIN(quote);
		  *quotestr = '\0';
		  quoteescape = 0;
		}
<quote>[^\\\"]+	{
		  if (quoteescape) {
			  switch (yytext[0]) {
			  case 'n':
				  yytext[0] = '\n';
				  break;
			  case 'r':
				  yytext[0] = '\r';
				  break;
			  default:
				  break;
			  }
		  }
		  strlcat(quotestr, yytext, sizeof(quotestr));
		  quoteescape = 0;
		}
<quote>\\	{
		  if (!quoteescape)
		     quoteescape = 1;
		  else {
		     strlcat(quotestr, "\\", sizeof(quotestr));
		     quoteescape = 0;
		  }
		}
<quote>\"	{
		  if (!quoteescape) {
			BEGIN(INITIAL);
			yylval.string = strdup(quotestr);
			return CMDSTRING;
		  } else {
			strlcat(quotestr, "\"", sizeof(quotestr));
			quoteescape = 0;
		  }
		}
\[		{ return LSQBRACE; }
\]		{ return RSQBRACE; }
\ 		{ ; }
\n		{ ; }
\t		{ ; }
"#".*\n		{ ; }
.		{ yyerror("illegal token"); }
%%
#ifndef yywrap
int yywrap() { return 1; }
#endif
