/*
 * Lexical analyser for GSP assembler
 *
 * Copyright (c) 1993 Paul Mackerras.
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
 *      This product includes software developed by Paul Mackerras.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
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
#include <stdio.h>
#include "gsp_ass.h"
#include "y.tab.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

char *lineptr;

char idents[MAXLINE];
char *idptr;

extern YYSTYPE yylval;

void ucasify(char *);
int str_match(char *, char **);

char *regnames[] = {
	"A0",	"A1",	"A2",	"A3",	"A4",	"A5",	"A6",	"A7",
	"A8",	"A9",	"A10",	"A11",	"A12",	"A13",	"A14",	"SP",
	"B0",	"B1",	"B2",	"B3",	"B4",	"B5",	"B6",	"B7",
	"B8",	"B9",	"B10",	"B11",	"B12",	"B13",	"B14",	"B15",
	NULL
};

short regnumbers[] = {
	A0+0,	A0+1,	A0+2,	A0+3,	A0+4,	A0+5,	A0+6,	A0+7,
	A0+8,	A0+9,	A0+10,	A0+11,	A0+12,	A0+13,	A0+14,	SP,
	B0+0,	B0+1,	B0+2,	B0+3,	B0+4,	B0+5,	B0+6,	B0+7,
	B0+8,	B0+9,	B0+10,	B0+11,	B0+12,	B0+13,	B0+14,	B0+15,
};

void
lex_init(char *line)
{
	lineptr = line;
	idptr = idents;
}

int
yylex()
{
	register int c, tok, x, len;
	register char *lp, *ip;
	char *end;

	lp = lineptr;
	c = *lp;
	while( c == ' ' || c == '\t' )
		c = *++lp;
	if( isalpha(c) || c == '_' || c == '.' ){
		/* an identifier or register name */
		ip = lp;
		do {
			c = *++lp;
		} while( isalnum(c) || c == '_' );
		len = lp - ip;
		if( len == 1 && *ip == '.' )
			tok = '.';
		else {
			strncpy(idptr, ip, len);
			idptr[len] = 0;
			ucasify(idptr);
			x = str_match(idptr, regnames);
			if( x == -1 ){
				strncpy(idptr, ip, len);
				yylval.y_id = idptr;
				idptr += len + 1;
				tok = ID;
			} else {
				yylval.y_int = regnumbers[x];
				tok = REGISTER;
			}
		}
	} else if( c == '$' ){
		/* a hex number */
		++lp;
		yylval.y_int = strtoul(lp, &end, 16);
		if( end == lp )
			perr("Bad number syntax");
		else
			lp = end;
		tok = NUMBER;
	} else if( isdigit(c) ){
		yylval.y_int = strtoul(lp, &end, 0);
		ip = lp;
		lp = end;
		c = *lp;
		if( (c == 'f' || c == 'F' || c == 'b' || c == 'B')
		   && !(isalnum(lp[1]) || lp[1] == '_') ){
			/* reference to numeric label */
			c = toupper(c);
			sprintf(idptr, "%d%c", yylval.y_int, c);
			yylval.y_id = idptr;
			idptr += strlen(idptr) + 1;
			++lp;
			tok = ID;
		} else
			tok = NUMBER;
	} else if( c == '\n' ){
		tok = 0;	/* eof */
	} else if( c == ';' ){
		/* comment - skip to end of line */
		while( *++lp != 0 )
			;
		tok = 0;
	} else if( c == '"' ){
		/* string */
		yylval.y_id = idptr;
		while( (c = *++lp) != '"' && c != '\n' && c != 0 )
			*idptr++ = c;
		*idptr++ = 0;
		if( c != '"' )
			perr("Unterminated string");
		else
			++lp;
		tok = STRING;
	} else if( c == '<' && lp[1] == '<' ){
		lp += 2;
		tok = LEFT_SHIFT;
	} else if( c == '>' && lp[1] == '>' ){
		lp += 2;
		tok = RIGHT_SHIFT;
	} else {
		if( c != 0 )
			++lp;
		tok = c;
	}
	lineptr = lp;
	return tok;
}

void
ucasify(register char *p)
{
	register int c;

	for( ; (c = *p) != 0; p++ )
		if( islower(c) )
			*p = toupper(c);
}

int
str_match(register char *id, char **names)
{
	register char **np;

	for( np = names; *np != NULL; ++np )
		if( strcmp(id, *np) == 0 )
			return np - names;
	return -1;
}
