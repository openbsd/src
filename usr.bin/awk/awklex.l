%Start A str sc reg comment

%{
/****************************************************************
Copyright (C) AT&T and Lucent Technologies 1996
All Rights Reserved

Permission to use, copy, modify, and distribute this software and
its documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appear in all
copies and that both that the copyright notice and this
permission notice and warranty disclaimer appear in supporting
documentation, and that the names of AT&T or Lucent Technologies
or any of their entities not be used in advertising or publicity
pertaining to distribution of the software without specific,
written prior permission.

AT&T AND LUCENT DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
FITNESS. IN NO EVENT SHALL AT&T OR LUCENT OR ANY OF THEIR
ENTITIES BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL
DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE
USE OR PERFORMANCE OF THIS SOFTWARE.
****************************************************************/

/* some of this depends on behavior of lex that
   may not be preserved in other implementations of lex.
*/

#ifndef FLEX_SCANNER
#undef	input	/* defeat lex */
#undef	unput
#endif /* !FLEX_SCANNER */

#include <stdlib.h>
#include <string.h>
#include "awk.h"
#include "awkgram.h"

extern YYSTYPE	yylval;
extern int	infunc;

int	lineno	= 1;
int	bracecnt = 0;
int	brackcnt  = 0;
int	parencnt = 0;

#define DEBUG
#ifdef	DEBUG
#	define	RET(x)	{if(dbg)printf("lex %s [%s]\n", tokname(x), yytext); return(x); }
#else
#	define	RET(x)	return(x)
#endif

#define	CADD	if (cadd(gs, yytext[0]) == 0) { \
			ERROR "string/reg expr %.30s... too long", gs->cbuf SYNTAX; \
			BEGIN A; \
		}

char	*s;
Gstring	*gs = 0;	/* initialized in main() */
int	cflag;

#ifdef FLEX_SCANNER
static	int	my_input( YY_CHAR *buf, int max_size );

#undef YY_INPUT
#define YY_INPUT(buf,result,max_size) result = my_input(buf, max_size);

#undef YY_USER_INIT
#define YY_USER_INIT init_input_source();

#define	FIRST	((yy_start - 1) / 2)
#else /* FLEX_SCANNER */
#define	FIRST	(yybgin - yysvec - 1)
#endif /* FLEX_SCANNER */
%}

A	[a-zA-Z_]
B	[a-zA-Z0-9_]
D	[0-9]
O	[0-7]
H	[0-9a-fA-F]
WS	[ \t]

%%
	switch (FIRST) {	/* witchcraft */
	case 0:
		BEGIN A;
		break;
	case sc:
		BEGIN A;
		RET('}');
	}

<A>\n		{ lineno++; RET(NL); }
<A>#.*		{ ; }	/* strip comments */
<A>{WS}+	{ ; }
<A>;		{ RET(';'); }

<A>"\\"\n	{ lineno++; }
<A>BEGIN	{ RET(XBEGIN); }
<A>END		{ RET(XEND); }
<A>func(tion)?	{ if (infunc) ERROR "illegal nested function" SYNTAX; RET(FUNC); }
<A>return	{ if (!infunc) ERROR "return not in function" SYNTAX; RET(RETURN); }
<A>"&&"		{ RET(AND); }
<A>"||"		{ RET(BOR); }
<A>"!"		{ RET(NOT); }
<A>"!="		{ yylval.i = NE; RET(NE); }
<A>"~"		{ yylval.i = MATCH; RET(MATCHOP); }
<A>"!~"		{ yylval.i = NOTMATCH; RET(MATCHOP); }
<A>"<"		{ yylval.i = LT; RET(LT); }
<A>"<="		{ yylval.i = LE; RET(LE); }
<A>"=="		{ yylval.i = EQ; RET(EQ); }
<A>">="		{ yylval.i = GE; RET(GE); }
<A>">"		{ yylval.i = GT; RET(GT); }
<A>">>"		{ yylval.i = APPEND; RET(APPEND); }
<A>"++"		{ yylval.i = INCR; RET(INCR); }
<A>"--"		{ yylval.i = DECR; RET(DECR); }
<A>"+="		{ yylval.i = ADDEQ; RET(ASGNOP); }
<A>"-="		{ yylval.i = SUBEQ; RET(ASGNOP); }
<A>"*="		{ yylval.i = MULTEQ; RET(ASGNOP); }
<A>"/="		{ yylval.i = DIVEQ; RET(ASGNOP); }
<A>"%="		{ yylval.i = MODEQ; RET(ASGNOP); }
<A>"^="		{ yylval.i = POWEQ; RET(ASGNOP); }
<A>"**="	{ yylval.i = POWEQ; RET(ASGNOP); }
<A>"="		{ yylval.i = ASSIGN; RET(ASGNOP); }
<A>"**"		{ RET(POWER); }
<A>"^"		{ RET(POWER); }

<A>"$"{D}+	{ yylval.cp = fieldadr(atoi(yytext+1)); RET(FIELD); }
<A>"$NF"	{ unputstr("(NF)"); return(INDIRECT); }
<A>"$"{A}{B}*	{
		  int c;
		  char *yytext_copy = strdup(yytext);
		  c = input(); unput(c);	/* look for '(' or '[' */
		  if (c == '(' || c == '[' ||
		      infunc && isarg(yytext_copy+1) >= 0) {
			  unputstr(yytext_copy+1);
			  free(yytext_copy);
			  return(INDIRECT);
		  } else {
			  yylval.cp =
				setsymtab(yytext_copy+1,"",0.0,STR|NUM,symtab);
			  free(yytext_copy);
			  RET(IVAR);
		  }
		}
<A>"$"		{ RET(INDIRECT); }
<A>NF		{ yylval.cp = setsymtab(yytext, "", 0.0, NUM, symtab); RET(VARNF); }

<A>({D}+("."?){D}*|"."{D}+)((e|E)("+"|-)?{D}+)?	{
		  yylval.cp = setsymtab(yytext, tostring(yytext), atof(yytext), CON|NUM, symtab);
		/* should this also have STR set? */
		  RET(NUMBER); }

<A>while	{ RET(WHILE); }
<A>for		{ RET(FOR); }
<A>do		{ RET(DO); }
<A>if		{ RET(IF); }
<A>else		{ RET(ELSE); }
<A>next		{ RET(NEXT); }
<A>nextfile	{ RET(NEXTFILE); }
<A>exit		{ RET(EXIT); }
<A>break	{ RET(BREAK); }
<A>continue	{ RET(CONTINUE); }
<A>print	{ yylval.i = PRINT; RET(PRINT); }
<A>printf	{ yylval.i = PRINTF; RET(PRINTF); }
<A>sprintf	{ yylval.i = SPRINTF; RET(SPRINTF); }
<A>split	{ yylval.i = SPLIT; RET(SPLIT); }
<A>substr	{ RET(SUBSTR); }
<A>sub		{ yylval.i = SUB; RET(SUB); }
<A>gsub		{ yylval.i = GSUB; RET(GSUB); }
<A>index	{ RET(INDEX); }
<A>match	{ RET(MATCHFCN); }
<A>in		{ RET(IN); }
<A>getline	{ RET(GETLINE); }
<A>close	{ RET(CLOSE); }
<A>delete	{ RET(DELETE); }
<A>length	{ yylval.i = FLENGTH; RET(BLTIN); }
<A>log		{ yylval.i = FLOG; RET(BLTIN); }
<A>int		{ yylval.i = FINT; RET(BLTIN); }
<A>exp		{ yylval.i = FEXP; RET(BLTIN); }
<A>sqrt		{ yylval.i = FSQRT; RET(BLTIN); }
<A>sin		{ yylval.i = FSIN; RET(BLTIN); }
<A>cos		{ yylval.i = FCOS; RET(BLTIN); }
<A>atan2	{ yylval.i = FATAN; RET(BLTIN); }
<A>system	{ yylval.i = FSYSTEM; RET(BLTIN); }
<A>rand		{ yylval.i = FRAND; RET(BLTIN); }
<A>srand	{ yylval.i = FSRAND; RET(BLTIN); }
<A>toupper	{ yylval.i = FTOUPPER; RET(BLTIN); }
<A>tolower	{ yylval.i = FTOLOWER; RET(BLTIN); }
<A>fflush	{ yylval.i = FFLUSH; RET(BLTIN); }

<A>{A}{B}*	{ int n, c;
		  char *yytext_copy = strdup(yytext);
		  c = input(); unput(c);	/* look for '(' */
		  if (c != '(' && infunc && (n=isarg(yytext_copy)) >= 0) {
			yylval.i = n;
			free(yytext_copy);
			RET(ARG);
		  } else {
			yylval.cp = setsymtab(yytext_copy, "", 0.0, STR|NUM, symtab);
			free(yytext_copy);
			if (c == '(') {
				RET(CALL);
			} else {
				RET(VAR);
			}
		  }
		}
<A>\"		{ BEGIN str; caddreset(gs); }

<A>"}"		{ if (--bracecnt < 0) ERROR "extra }" SYNTAX; BEGIN sc; RET(';'); }
<A>"]"		{ if (--brackcnt < 0) ERROR "extra ]" SYNTAX; RET(']'); }
<A>")"		{ if (--parencnt < 0) ERROR "extra )" SYNTAX; RET(')'); }

<A>.		{ if (yytext[0] == '{') bracecnt++;
		  else if (yytext[0] == '[') brackcnt++;
		  else if (yytext[0] == '(') parencnt++;
		  RET(yylval.i = yytext[0]); /* everything else */ }

<reg>\\.	{ cadd(gs, '\\'); cadd(gs, yytext[1]); }
<reg>\n		{ ERROR "newline in regular expression %.10s...", gs->cbuf SYNTAX; lineno++; BEGIN A; }
<reg>"/"	{ BEGIN A;
		  cadd(gs, 0);
		  yylval.s = tostring(gs->cbuf);
		  unput('/');
		  RET(REGEXPR); }
<reg>.		{ CADD; }

<str>\"		{ BEGIN A;
		  cadd(gs, 0); s = tostring(gs->cbuf);
		  cunadd(gs);
		  cadd(gs, ' '); cadd(gs, 0);
		  yylval.cp = setsymtab(gs->cbuf, s, 0.0, CON|STR, symtab);
		  RET(STRING); }
<str>\n		{ ERROR "newline in string %.10s...", gs->cbuf SYNTAX; lineno++; BEGIN A; }
<str>"\\\""	{ cadd(gs, '"'); }
<str>"\\"n	{ cadd(gs, '\n'); }
<str>"\\"t	{ cadd(gs, '\t'); }
<str>"\\"f	{ cadd(gs, '\f'); }
<str>"\\"r	{ cadd(gs, '\r'); }
<str>"\\"b	{ cadd(gs, '\b'); }
<str>"\\"v	{ cadd(gs, '\v'); }	/* these ANSIisms may not be known by */
<str>"\\"a	{ cadd(gs, '\007'); }	/* your compiler. hence 007 for bell */
<str>"\\\\"	{ cadd(gs, '\\'); }
<str>"\\"({O}{O}{O}|{O}{O}|{O}) { int n;
		  sscanf(yytext+1, "%o", &n); cadd(gs, n); }
<str>"\\"x({H}+) { int n;	/* ANSI permits any number! */
		  sscanf(yytext+2, "%x", &n); cadd(gs, n); }
<str>"\\".	{ cadd(gs, yytext[1]); }
<str>.		{ CADD; }

%%

void startreg(void)	/* start parsing a regular expression */
{
	BEGIN reg;
	caddreset(gs);
}

#ifdef FLEX_SCANNER
static int my_input( YY_CHAR *buf, int max_size )
{
	extern uschar *lexprog;

	if ( lexprog ) {		/* awk '...' */
		int num_chars = strlen( lexprog );
		if ( num_chars > max_size )
			{
			num_chars = max_size;
			strncpy( buf, lexprog, num_chars );
			}
		else
			strcpy( buf, lexprog );
		lexprog += num_chars;
		return num_chars;

	} else {			/* awk -f ... */
		int c = pgetc();
		if (c == EOF)
			return 0;
		buf[0] = c;
		return 1;
	}
}
#else /* FLEX_SCANNER */
/* input() and unput() are transcriptions of the standard lex
   macros for input and output with additions for error message
   printing.  God help us all if someone changes how lex works.
*/

char	ebuf[300];
char	*ep = ebuf;

int input(void)	/* get next lexical input character */
{
	int c;
	extern char *lexprog;

	if (yysptr > yysbuf)
		c = U(*--yysptr);
	else if (lexprog != NULL) {	/* awk '...' */
		if ((c = *lexprog) != 0)
			lexprog++;
	} else				/* awk -f ... */
		c = pgetc();
	if (c == '\n')
		yylineno++;
	else if (c == EOF)
		c = 0;
	if (ep >= ebuf + sizeof ebuf)
		ep = ebuf;
	return *ep++ = c;
}

void unput(int c)	/* put lexical character back on input */
{
	yytchar = c;
	if (yytchar == '\n')
		yylineno--;
	*yysptr++ = yytchar;
	if (--ep < ebuf)
		ep = ebuf + sizeof(ebuf) - 1;
}
#endif /* FLEX_SCANNER */

void unputstr(char *s)	/* put a string back on input */
{
	int i;

	for (i = strlen(s)-1; i >= 0; i--)
		unput(s[i]);
}

int lex_input()
{
	return input();
}

/* growing-string code */

const int CBUFLEN = 400;

Gstring *newGstring()
{
	Gstring *gs = (Gstring *) malloc(sizeof(Gstring));
	char *cp = (char *) malloc(CBUFLEN);

	if (gs == 0 || cp == 0)
		ERROR "Out of space for strings" FATAL;
	gs->cbuf = cp;
	gs->cmax = CBUFLEN;
	gs->clen = 0;
	return gs;
}

char *cadd(Gstring *gs, int c)	/* add one char to gs->cbuf, grow as needed */
{
	if (gs->clen >= gs->cmax) {	/* need to grow */
		gs->cmax *= 4;
		gs->cbuf = (char *) realloc((void *) gs->cbuf, gs->cmax);

	}
	if (gs->cbuf != 0)
		gs->cbuf[gs->clen++] = c;
	return gs->cbuf;
}

void caddreset(Gstring *gs)
{
	gs->clen = 0;
}

void cunadd(Gstring *gs)
{
	if (gs->clen > 0)
		gs->clen--;
}

void delGstring(Gstring *gs)
{
	free((void *) gs->cbuf);
	free((void *) gs);
}

#ifdef FLEX_SCANNER
void init_input_source(void)
{
	extern int curpfile;
	extern char *pfile[];

	if (yyin == NULL) {
		if (pfile[curpfile] == 0)
			return;
		if (strcmp((char *) pfile[curpfile], "-") == 0)
			yyin = stdin;
		else if ((yyin = fopen((char *) pfile[curpfile], "r")) == NULL)
			ERROR "can't open file %s", pfile[curpfile] FATAL;
	}
}
#endif
