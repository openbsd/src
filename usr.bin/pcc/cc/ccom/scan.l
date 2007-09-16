%{
/*	$OpenBSD: scan.l,v 1.3 2007/09/16 18:52:52 otto Exp $	*/

/*
 * Copyright (c) 2002 Anders Magnusson. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
%}


D			[0-9]
L			[a-zA-Z_]
H			[a-fA-F0-9]
E			[Ee][+-]?{D}+
P			[Pp][+-]?{D}+
FS			(f|F|l|L)
IS			(u|U|l|L)*

%{
#include <errno.h>  
#include <string.h>
#include <stdarg.h>

#include "pass1.h"
#include "cgram.h"

static NODE *cvtdig(int radix);
static NODE *charcon(void);
static void control(int);
static NODE *floatcon(void);
static NODE *fhexcon(void);
int notype, parbal;

#define	CPP_PRAGMA	1
#define	CPP_IDENT 	2
#define	CPP_LINE 	3
#define	CPP_HASH	4

#ifdef STABS
#define	STABS_LINE(x) if (gflag && blevel) stabs_line(x)
#else
#define STABS_LINE(x)
#endif
#if defined(FLEX_SCANNER) && YY_FLEX_SUBMINOR_VERSION >= 31
/* Hack to avoid unneccessary warnings */
FILE *yyget_in  (void);
FILE *yyget_out  (void);
int yyget_leng  (void);
char *yyget_text  (void);
void yyset_in (FILE *  in_str );
void yyset_out (FILE *  out_str );
int yyget_debug  (void);
void yyset_debug (int  bdebug );
int yylex_destroy  (void);
#endif

%}

%%

"__func__"		{
				if (cftnsp == NULL)
					uerror("__func__ outside function");
				yylval.strp = cftnsp->sname; /* XXX - not C99 */
				return(C_STRING);
			}
"asm"			{ return(C_ASM); }
"auto"			{ yylval.intval = AUTO; return(C_CLASS); }
"_Bool"			{ yylval.nodep = mkty((TWORD)BOOL, 0, MKSUE(BOOL));
				return(C_TYPE); }
"break"			{ return(C_BREAK); }
"case"			{ return(C_CASE); }
"char"			{ yylval.nodep = mkty((TWORD)CHAR, 0, MKSUE(CHAR));
			  notype=1; return(C_TYPE); }
"const"			{ yylval.nodep =
				block(QUALIFIER, NIL, NIL, CON, 0, 0);
			  return(C_QUALIFIER); }
"continue"		{ return(C_CONTINUE); }
"default"		{ return(C_DEFAULT); }
"do"			{ return(C_DO); }
"double"		{ yylval.nodep = mkty((TWORD)DOUBLE, 0, MKSUE(DOUBLE));
			  notype=1; return(C_TYPE); }
"else"			{ return(C_ELSE); }
"enum"			{ notype=1; return(C_ENUM); }
"extern"		{ yylval.intval = EXTERN; return(C_CLASS); }
"float"			{ yylval.nodep = mkty((TWORD)FLOAT, 0, MKSUE(FLOAT));
			  notype=1; return(C_TYPE); }
"for"			{ return(C_FOR); }
"goto"			{ return(C_GOTO); }
"if"			{ return(C_IF); }
"inline"		{ return(C_FUNSPEC); }
"int"			{ yylval.nodep = mkty((TWORD)INT, 0, MKSUE(INT));
			  notype=1; return(C_TYPE); }
"long"			{ yylval.nodep = mkty((TWORD)LONG, 0, MKSUE(LONG));
			  notype=1; return(C_TYPE); }
"register"		{ yylval.intval = REGISTER; return(C_CLASS); }
"restrict"		{ ; /* just ignore */ }
"return"		{ return(C_RETURN); }
"short"			{ yylval.nodep = mkty((TWORD)SHORT, 0, MKSUE(SHORT));
			  notype=1; return(C_TYPE); }
"signed"		{ yylval.nodep = mkty((TWORD)SIGNED, 0, MKSUE(SIGNED));
			  notype=1; return(C_TYPE); }
"sizeof"		{ return(C_SIZEOF); }
"static"		{ yylval.intval = STATIC; return(C_CLASS); }
"struct"		{ yylval.intval = INSTRUCT; notype=1; return(C_STRUCT); }
"switch"		{ return(C_SWITCH); }
"typedef"		{ yylval.intval = TYPEDEF; return(C_CLASS); }
"union"			{ yylval.intval = INUNION; notype=1; return(C_STRUCT); }
"unsigned"		{ yylval.nodep = mkty((TWORD)UNSIGNED, 0, MKSUE(UNSIGNED));
			  notype=1; return(C_TYPE); }
"void"			{ yylval.nodep = mkty((TWORD)VOID, 0, MKSUE(VOID));
			  notype=1; return(C_TYPE); }
"volatile"		{ yylval.nodep =
				block(QUALIFIER, NIL, NIL, VOL, 0, 0);
			  return(C_QUALIFIER); }
"while"			{ return(C_WHILE); }

{L}({L}|{D})*		{ 	struct symtab *s;
				int i;

				yylval.strp = addname(yytext);
				if ((i = gcc_keyword(yylval.strp,
				    &yylval.nodep)) != 0)
					return i;
// printf("str: %s notype %d parbal %d\n", yytext, notype, parbal);
				if (!notype) {
					s = lookup(yylval.strp, SNOCREAT);
					if (s && s->sclass == TYPEDEF)
						return notype=1, C_TYPENAME;
				}
				return(C_NAME);
			}

0[xX]{H}+{IS}?		{ yylval.nodep = cvtdig(16); return(C_ICON); }
0{D}+{IS}?		{ yylval.nodep = cvtdig(8); return(C_ICON); }
{D}+{IS}?		{ yylval.nodep = cvtdig(10); return(C_ICON); }
L?'(\\.|[^\\'])+'	{ yylval.nodep = charcon(); return(C_ICON); }

{D}+{E}{FS}?		{ yylval.nodep = floatcon(); return(C_FCON); }
{D}*"."{D}+({E})?{FS}?	{ yylval.nodep = floatcon(); return(C_FCON); }
{D}+"."{D}*({E})?{FS}?	{ yylval.nodep = floatcon(); return(C_FCON); }
0[xX]{H}*"."{H}+{P}{FS}? { yylval.nodep = fhexcon(); return(C_FCON); }
0[xX]{H}+"."{P}{FS}?	{ yylval.nodep = fhexcon(); return(C_FCON); }
0[xX]{H}+{P}{FS}?	{ yylval.nodep = fhexcon(); return(C_FCON); }

L?\"(\\.|[^\\"])*\"	{
				char *c = yytext;
				int i = yyleng-2, rv;

				if (*c++ == 'L') {
					c++, i--;
					rv = C_WSTRING;
				} else
					rv = C_STRING;
				c[i] = 0; /* last " */
				yylval.strp = c;
				return rv;
			}
"..."			{ return(C_ELLIPSIS); }
">>="			{ yylval.intval = RSEQ; return(C_ASOP); }
"<<="			{ yylval.intval = LSEQ; return(C_ASOP); }
"+="			{ yylval.intval = PLUSEQ; return(C_ASOP); }
"-="			{ yylval.intval = MINUSEQ; return(C_ASOP); }
"*="			{ yylval.intval = MULEQ; return(C_ASOP); }
"/="			{ yylval.intval = DIVEQ; return(C_ASOP); }
"%="			{ yylval.intval = MODEQ; return(C_ASOP); }
"&="			{ yylval.intval = ANDEQ; return(C_ASOP); }
"^="			{ yylval.intval = EREQ; return(C_ASOP); }
"|="			{ yylval.intval = OREQ; return(C_ASOP); }
">>"			{ yylval.intval = RS; return(C_SHIFTOP); }
"<<"			{ yylval.intval = LS; return(C_SHIFTOP); }
"++"			{ yylval.intval = INCR; return(C_INCOP); }
"--"			{ yylval.intval = DECR; return(C_INCOP); }
"->"			{ yylval.intval = STREF; return(C_STROP); }
"&&"			{ yylval.intval = ANDAND; return(C_ANDAND); }
"||"			{ yylval.intval = OROR; return(C_OROR); }
"<="			{ yylval.intval = LE; return(C_RELOP); }
">="			{ yylval.intval = GE; return(C_RELOP); }
"=="			{ yylval.intval = EQ; return(C_EQUOP); }
"!="			{ yylval.intval = NE; return(C_EQUOP); }
";"			{ notype = 0; return(';'); }
("{"|"<%")		{ notype = 0; return('{'); }
("}"|"%>")		{ return('}'); }
","			{ if (parbal) notype = 0; return(','); }
":"			{ return(':'); }
"="			{ return('='); }
"("			{ parbal++; notype = 0; return('('); }
")"			{ parbal--; if (parbal==0) { notype = 0; } return(')'); }
("["|"<:")		{ return('['); }
("]"|":>")		{ return(']'); }
"."			{ yylval.intval = DOT; return(C_STROP); }
"&"			{ return('&'); }
"!"			{ yylval.intval = NOT; return(C_UNOP); }
"~"			{ yylval.intval = COMPL; return(C_UNOP); }
"-"			{ return('-'); }
"+"			{ return('+'); }
"*"			{ if (parbal && notype == 0) notype = 1; return('*'); }
"/"			{ yylval.intval = DIV; return(C_DIVOP); }
"%"			{ yylval.intval = MOD; return(C_DIVOP); }
"<"			{ yylval.intval = LT; return(C_RELOP); }
">"			{ yylval.intval = GT; return(C_RELOP); }
"^"			{ return('^'); }
"|"			{ return('|'); }
"?"			{ return('?'); }
^#pragma[ \t].*		{ control(CPP_PRAGMA); }
^#ident[ \t].*		{ control(CPP_IDENT); }
^#line[ \t].*		{ control(CPP_LINE); }
^#.*			{ control(CPP_HASH); }

[ \t\v\f]		{ }
"\n"			{ ++lineno; STABS_LINE(lineno); }
.			{ /* ignore bad characters */ }

%%

int lineno;
char *ftitle = "<stdin>";

int
yywrap(void)
{
	if (0) unput(0); /* quiet gcc */
	return(1);
}

/*
 * XXX floatcon() and fhexcon() should be in support libraries for
 * the target floating point.
 */
static NODE *
f2(char *str)
{
	TWORD tw;
	NODE *p;
	double dc;
	char *eptr;

	dc = strtod(str, &eptr); /* XXX - avoid strtod() */
	tw = (*eptr == 'f' || *eptr == 'F' ? FLOAT : DOUBLE);
	p = block(FCON, NIL, NIL, tw, 0, MKSUE(tw));
	p->n_dcon = dc;
	return p;
}

NODE *
floatcon(void)
{
	return f2(yytext);
}

static int
h2n(int ch)
{
	if (ch >= '0' && ch <= '9')
		return ch - '0';
	if (ch >= 'a' && ch <= 'f')
		return ch - 'a' + 10;
	return ch - 'A' + 10;
	
}

NODE *
fhexcon(void)
{
	char buf[500];
	char *c = yytext;
	unsigned long long num1, num2;

	/* XXX - convert it to a decimal float number and use strtod */
	c+= 2; /* skip 0x */
	for (num1 = 0; *c != '.' && *c != 'p' && *c != 'P'; c++)
		num1 = (num1 << 4) | h2n(*c);
	if (*c != '.' && *c != 'p' && *c != 'P')
		cerror("fhexcon");
	num2 = 0;
	if (*c == '.') {
		c++;
		for (; *c != 'p' && *c != 'P'; c++)
			num2 = (num2 << 4) | h2n(*c);
	}
	if (*c != 'P' && *c != 'p')
		cerror("fhexcon2");
	c++;
	snprintf(buf, sizeof(buf), "%llu.%lluE%s", num1, num2, c);
	return f2(buf);
}

unsigned int
esccon(char **sptr)
{
	char *wr = *sptr;
	unsigned int val;

	switch (*wr++) {
	case 'a': val = '\a'; break;
	case 'b': val = '\b'; break;
	case 'f': val = '\f'; break;
	case 'n': val = '\n'; break;
	case 'r': val = '\r'; break;
	case 't': val = '\t'; break;
	case 'v': val = '\v'; break;
	case '\"': val = '\"'; break;
	case 'x': val = strtoul(wr, &wr, 16); break;
	case '0': case '1': case '2': case '3': case '4': 
	case '5': case '6': case '7': case '8': case '9': 
		wr--;
		val = strtoul(wr, &wr, 8);
		break;
	default: val = wr[-1];
	}
	*sptr = wr;
	return val;
}

NODE *
cvtdig(int radix)
{
	NODE *p;
	TWORD ntype;
	unsigned long long v;
	char *ch = yytext;
	int n, numl, numu;

	if (radix == 16)
		ch += 2; /* Skip 0x */
	
	v = 0;
	while ((*ch >= '0' && *ch <= '9') || (*ch >= 'a' && *ch <= 'f') ||
	    (*ch >= 'A' && *ch <= 'F')) {
		v *= radix;
		n = *ch;
		n = (n <= '9' ? n - '0' : (n > 'F' ? n - 'a' : n - 'A') + 10);
		ch++;
		v += n;
	}
	/* Parse trailing chars */
	ntype = INT;
	numl = numu = 0;
	for (n = 0; n < 3; n++) {
		if (*ch == 0)
			break;
		if ((*ch == 'l' || *ch == 'L') && numl < 2)
			ntype+=2, numl++;
		else if ((*ch == 'u' || *ch == 'U') && numu < 1)
			ntype = ENUNSIGN(ntype), numu++;
		else
			break;
		ch++;
	}
	if (*ch)
		uerror("constant has too many '%c'", *ch);

	if (ntype == INT) {
		/* v contains a number. Get type correct */
		if (v > MAX_LONGLONG && radix != 10)
			ntype = ULONGLONG;
		else if (v > MAX_ULONG)
			ntype = LONGLONG;
		else if (v > MAX_LONG && radix != 10)
			ntype = ULONG;
		else if (v > MAX_UNSIGNED)
			ntype = LONG;
		else if (v > MAX_INT && radix != 10)
			ntype = UNSIGNED;
	}
	ntype = ctype(ntype);
	p = block(ICON, NIL, NIL, ntype, 0, MKSUE(ntype));
	p->n_lval = v;
	ASGLVAL(p->n_slval, v);

	return p;
}

/*
 * Convert a character constant to an integer.
 */
NODE *
charcon(void)
{
	int lastcon = 0;
	int val, i = 0;
	char *pp = yytext;

	if (*pp == 'L')
		pp++;
	pp++;
	while (*pp != '\'') {
		if (*pp++ == '\\') {
			val = esccon(&pp);
		} else
			val = pp[-1];
		makecc(val, i);
		i++;
	}

	if (i == 0)
		uerror("empty character constant");
	if (i > (SZINT/SZCHAR) || (i>1))
		werror("too many characters in character constant");
	return bcon(lastcon);
}

void
control(int t)
{
	char *wr = yytext;
	char *eptr;
	int val;

	wr++;	/* Skip initial '#' */
	switch (t) {
	case CPP_PRAGMA:
	case CPP_IDENT:
		return;	/* Just skip these for now. */

	case CPP_LINE:
		wr += 4;
		/* FALLTHROUGH */
	case CPP_HASH:
		val = strtol(wr, &eptr, 10);
		if (wr == eptr)	/* Illegal string */
			goto bad;
		wr = eptr;
		lineno = val - 1;
		while (*wr && *wr != '\"')
			wr++;
		if (*wr++ != '\"')
			goto bad;
		eptr = wr;
		while (*wr && *wr != '\"')
			wr++;
		if (*wr != '\"')
			goto bad;
		*wr = 0;
		ftitle = addstring(eptr);
#ifdef STABS
		if (gflag)
			stabs_file(ftitle);
#endif
	}
	return;
bad:
	werror("%s: illegal control", yytext);
}
