typedef unsigned char Uchar;
# include <stdio.h>
# define U(x) x
# define NLSTATE yyprevious=YYNEWLINE
# define BEGIN yybgin = yysvec + 1 +
# define INITIAL 0
# define YYLERR yysvec
# define YYSTATE (yyestate-yysvec-1)
# define YYOPTIM 1
# define YYLMAX 200
# define output(c) putc(c,yyout)
# define input() (((yytchar=yysptr>yysbuf?U(*--yysptr):getc(yyin))==10?(yylineno++,yytchar):yytchar)==EOF?0:yytchar)
# define unput(c) {yytchar= (c);if(yytchar=='\n')yylineno--;*yysptr++=yytchar;}
# define yymore() (yymorfg=1)
# define ECHO fprintf(yyout, "%s",yytext)
# define REJECT { nstr = yyreject(); goto yyfussy;}
int yyleng; extern char yytext[];
int yymorfg;
extern Uchar *yysptr, yysbuf[];
int yytchar;
FILE *yyin = {stdin}, *yyout = {stdout};
extern int yylineno;
struct yysvf { 
	struct yywork *yystoff;
	struct yysvf *yyother;
	int *yystops;};
struct yysvf *yyestate;
extern struct yysvf yysvec[], *yybgin;
int yylook(void), yywrap(void), yyback(int *, int);
#define A 2
#define str 4
#define sc 6
#define reg 8
#define comment 10
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

#undef	input	/* defeat lex */
#undef	unput

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
#define YYNEWLINE 10
yylex(void){
int nstr; extern int yyprevious;
switch (yybgin-yysvec-1) {	/* witchcraft */
	case 0:
		BEGIN A;
		break;
	case sc:
		BEGIN A;
		RET('}');
	}
while((nstr = yylook()) >= 0)
yyfussy: switch(nstr){
case 0:
if(yywrap()) return(0); break;
case 1:
	{ lineno++; RET(NL); }
break;
case 2:
	{ ; }
break;
case 3:
{ ; }
break;
case 4:
	{ RET(';'); }
break;
case 5:
{ lineno++; }
break;
case 6:
{ RET(XBEGIN); }
break;
case 7:
	{ RET(XEND); }
break;
case 8:
{ if (infunc) ERROR "illegal nested function" SYNTAX; RET(FUNC); }
break;
case 9:
{ if (!infunc) ERROR "return not in function" SYNTAX; RET(RETURN); }
break;
case 10:
	{ RET(AND); }
break;
case 11:
	{ RET(BOR); }
break;
case 12:
	{ RET(NOT); }
break;
case 13:
	{ yylval.i = NE; RET(NE); }
break;
case 14:
	{ yylval.i = MATCH; RET(MATCHOP); }
break;
case 15:
	{ yylval.i = NOTMATCH; RET(MATCHOP); }
break;
case 16:
	{ yylval.i = LT; RET(LT); }
break;
case 17:
	{ yylval.i = LE; RET(LE); }
break;
case 18:
	{ yylval.i = EQ; RET(EQ); }
break;
case 19:
	{ yylval.i = GE; RET(GE); }
break;
case 20:
	{ yylval.i = GT; RET(GT); }
break;
case 21:
	{ yylval.i = APPEND; RET(APPEND); }
break;
case 22:
	{ yylval.i = INCR; RET(INCR); }
break;
case 23:
	{ yylval.i = DECR; RET(DECR); }
break;
case 24:
	{ yylval.i = ADDEQ; RET(ASGNOP); }
break;
case 25:
	{ yylval.i = SUBEQ; RET(ASGNOP); }
break;
case 26:
	{ yylval.i = MULTEQ; RET(ASGNOP); }
break;
case 27:
	{ yylval.i = DIVEQ; RET(ASGNOP); }
break;
case 28:
	{ yylval.i = MODEQ; RET(ASGNOP); }
break;
case 29:
	{ yylval.i = POWEQ; RET(ASGNOP); }
break;
case 30:
{ yylval.i = POWEQ; RET(ASGNOP); }
break;
case 31:
	{ yylval.i = ASSIGN; RET(ASGNOP); }
break;
case 32:
	{ RET(POWER); }
break;
case 33:
	{ RET(POWER); }
break;
case 34:
{ yylval.cp = fieldadr(atoi(yytext+1)); RET(FIELD); }
break;
case 35:
{ unputstr("(NF)"); return(INDIRECT); }
break;
case 36:
{ int c, n;
		  c = input(); unput(c);
		  if (c == '(' || c == '[' || (infunc && (n=isarg(yytext+1)) >= 0)) {
			unputstr(yytext+1);
			return(INDIRECT);
		  } else {
			yylval.cp = setsymtab(yytext+1, "", 0.0, STR|NUM, symtab);
			RET(IVAR);
		  }
		}
break;
case 37:
	{ RET(INDIRECT); }
break;
case 38:
	{ yylval.cp = setsymtab(yytext, "", 0.0, NUM, symtab); RET(VARNF); }
break;
case 39:
{
		  yylval.cp = setsymtab(yytext, tostring(yytext), atof(yytext), CON|NUM, symtab);
		/* should this also have STR set? */
		  RET(NUMBER); }
break;
case 40:
{ RET(WHILE); }
break;
case 41:
	{ RET(FOR); }
break;
case 42:
	{ RET(DO); }
break;
case 43:
	{ RET(IF); }
break;
case 44:
	{ RET(ELSE); }
break;
case 45:
	{ RET(NEXT); }
break;
case 46:
{ RET(NEXTFILE); }
break;
case 47:
	{ RET(EXIT); }
break;
case 48:
{ RET(BREAK); }
break;
case 49:
{ RET(CONTINUE); }
break;
case 50:
{ yylval.i = PRINT; RET(PRINT); }
break;
case 51:
{ yylval.i = PRINTF; RET(PRINTF); }
break;
case 52:
{ yylval.i = SPRINTF; RET(SPRINTF); }
break;
case 53:
{ yylval.i = SPLIT; RET(SPLIT); }
break;
case 54:
{ RET(SUBSTR); }
break;
case 55:
	{ yylval.i = SUB; RET(SUB); }
break;
case 56:
	{ yylval.i = GSUB; RET(GSUB); }
break;
case 57:
{ RET(INDEX); }
break;
case 58:
{ RET(MATCHFCN); }
break;
case 59:
	{ RET(IN); }
break;
case 60:
{ RET(GETLINE); }
break;
case 61:
{ RET(CLOSE); }
break;
case 62:
{ RET(DELETE); }
break;
case 63:
{ yylval.i = FLENGTH; RET(BLTIN); }
break;
case 64:
	{ yylval.i = FLOG; RET(BLTIN); }
break;
case 65:
	{ yylval.i = FINT; RET(BLTIN); }
break;
case 66:
	{ yylval.i = FEXP; RET(BLTIN); }
break;
case 67:
	{ yylval.i = FSQRT; RET(BLTIN); }
break;
case 68:
	{ yylval.i = FSIN; RET(BLTIN); }
break;
case 69:
	{ yylval.i = FCOS; RET(BLTIN); }
break;
case 70:
{ yylval.i = FATAN; RET(BLTIN); }
break;
case 71:
{ yylval.i = FSYSTEM; RET(BLTIN); }
break;
case 72:
	{ yylval.i = FRAND; RET(BLTIN); }
break;
case 73:
{ yylval.i = FSRAND; RET(BLTIN); }
break;
case 74:
{ yylval.i = FTOUPPER; RET(BLTIN); }
break;
case 75:
{ yylval.i = FTOLOWER; RET(BLTIN); }
break;
case 76:
{ yylval.i = FFLUSH; RET(BLTIN); }
break;
case 77:
{ int n, c;
		  c = input(); unput(c);	/* look for '(' */
		  if (c != '(' && infunc && (n=isarg(yytext)) >= 0) {
			yylval.i = n;
			RET(ARG);
		  } else {
			yylval.cp = setsymtab(yytext, "", 0.0, STR|NUM, symtab);
			if (c == '(') {
				RET(CALL);
			} else {
				RET(VAR);
			}
		  }
		}
break;
case 78:
	{ BEGIN str; caddreset(gs); }
break;
case 79:
	{ if (--bracecnt < 0) ERROR "extra }" SYNTAX; BEGIN sc; RET(';'); }
break;
case 80:
	{ if (--brackcnt < 0) ERROR "extra ]" SYNTAX; RET(']'); }
break;
case 81:
	{ if (--parencnt < 0) ERROR "extra )" SYNTAX; RET(')'); }
break;
case 82:
	{ if (yytext[0] == '{') bracecnt++;
		  else if (yytext[0] == '[') brackcnt++;
		  else if (yytext[0] == '(') parencnt++;
		  RET(yylval.i = yytext[0]); /* everything else */ }
break;
case 83:
{ cadd(gs, '\\'); cadd(gs, yytext[1]); }
break;
case 84:
	{ ERROR "newline in regular expression %.10s...", gs->cbuf SYNTAX; lineno++; BEGIN A; }
break;
case 85:
{ BEGIN A;
		  cadd(gs, 0);
		  yylval.s = tostring(gs->cbuf);
		  unput('/');
		  RET(REGEXPR); }
break;
case 86:
	{ CADD; }
break;
case 87:
	{ BEGIN A;
		  cadd(gs, 0); s = tostring(gs->cbuf);
		  cunadd(gs);
		  cadd(gs, ' '); cadd(gs, 0);
		  yylval.cp = setsymtab(gs->cbuf, s, 0.0, CON|STR, symtab);
		  RET(STRING); }
break;
case 88:
	{ ERROR "newline in string %.10s...", gs->cbuf SYNTAX; lineno++; BEGIN A; }
break;
case 89:
{ cadd(gs, '"'); }
break;
case 90:
{ cadd(gs, '\n'); }
break;
case 91:
{ cadd(gs, '\t'); }
break;
case 92:
{ cadd(gs, '\f'); }
break;
case 93:
{ cadd(gs, '\r'); }
break;
case 94:
{ cadd(gs, '\b'); }
break;
case 95:
{ cadd(gs, '\v'); }
break;
case 96:
{ cadd(gs, '\007'); }
break;
case 97:
{ cadd(gs, '\\'); }
break;
case 98:
{ int n;
		  sscanf(yytext+1, "%o", &n); cadd(gs, n); }
break;
case 99:
{ int n;	/* ANSI permits any number! */
		  sscanf(yytext+2, "%x", &n); cadd(gs, n); }
break;
case 100:
{ cadd(gs, yytext[1]); }
break;
case 101:
	{ CADD; }
break;
case -1:
break;
default:
fprintf(yyout,"bad switch yylook %d",nstr);
} return(0); }
/* end of yylex */

void startreg(void)	/* start parsing a regular expression */
{
	BEGIN reg;
	caddreset(gs);
}

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


void unputstr(char *s)	/* put a string back on input */
{
	int i;

	for (i = strlen(s)-1; i >= 0; i--)
		unput(s[i]);
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
int yyvstop[] = {
0,

82,
0,

3,
82,
0,

1,
0,

12,
82,
0,

78,
82,
0,

2,
82,
0,

37,
82,
0,

82,
0,

82,
0,

81,
82,
0,

82,
0,

82,
0,

82,
0,

82,
0,

82,
0,

39,
82,
0,

4,
82,
0,

16,
82,
0,

31,
82,
0,

20,
82,
0,

77,
82,
0,

77,
82,
0,

77,
82,
0,

77,
82,
0,

82,
0,

80,
82,
0,

33,
82,
0,

77,
82,
0,

77,
82,
0,

77,
82,
0,

77,
82,
0,

77,
82,
0,

77,
82,
0,

77,
82,
0,

77,
82,
0,

77,
82,
0,

77,
82,
0,

77,
82,
0,

77,
82,
0,

77,
82,
0,

77,
82,
0,

77,
82,
0,

77,
82,
0,

82,
0,

79,
82,
0,

14,
82,
0,

101,
0,

88,
0,

87,
101,
0,

101,
0,

86,
0,

84,
0,

85,
86,
0,

86,
0,

3,
0,

13,
0,

15,
0,

2,
0,

34,
0,

36,
0,

36,
0,

28,
0,

10,
0,

32,
0,

26,
0,

22,
0,

24,
0,

23,
0,

25,
0,

39,
0,

27,
0,

39,
0,

39,
0,

17,
0,

18,
0,

19,
0,

21,
0,

77,
0,

77,
0,

77,
0,

38,
77,
0,

5,
0,

29,
0,

77,
0,

77,
0,

77,
0,

77,
0,

77,
0,

42,
77,
0,

77,
0,

77,
0,

77,
0,

77,
0,

77,
0,

77,
0,

77,
0,

43,
77,
0,

59,
77,
0,

77,
0,

77,
0,

77,
0,

77,
0,

77,
0,

77,
0,

77,
0,

77,
0,

77,
0,

77,
0,

77,
0,

77,
0,

77,
0,

77,
0,

77,
0,

11,
0,

100,
0,

89,
100,
0,

98,
100,
0,

97,
100,
0,

96,
100,
0,

94,
100,
0,

92,
100,
0,

90,
100,
0,

93,
100,
0,

91,
100,
0,

95,
100,
0,

100,
0,

83,
0,

35,
36,
0,

30,
0,

39,
0,

77,
0,

7,
77,
0,

77,
0,

77,
0,

77,
0,

77,
0,

69,
77,
0,

77,
0,

77,
0,

77,
0,

66,
77,
0,

77,
0,

41,
77,
0,

77,
0,

77,
0,

77,
0,

77,
0,

65,
77,
0,

77,
0,

64,
77,
0,

77,
0,

77,
0,

77,
0,

77,
0,

77,
0,

68,
77,
0,

77,
0,

77,
0,

77,
0,

77,
0,

55,
77,
0,

77,
0,

77,
0,

77,
0,

77,
0,

98,
0,

99,
0,

77,
0,

77,
0,

77,
0,

77,
0,

77,
0,

77,
0,

44,
77,
0,

47,
77,
0,

77,
0,

8,
77,
0,

77,
0,

56,
77,
0,

77,
0,

77,
0,

77,
0,

45,
77,
0,

77,
0,

72,
77,
0,

77,
0,

77,
0,

77,
0,

67,
77,
0,

77,
0,

77,
0,

77,
0,

77,
0,

77,
0,

77,
0,

98,
0,

6,
77,
0,

70,
77,
0,

48,
77,
0,

61,
77,
0,

77,
0,

77,
0,

77,
0,

77,
0,

77,
0,

57,
77,
0,

77,
0,

58,
77,
0,

77,
0,

50,
77,
0,

77,
0,

53,
77,
0,

77,
0,

73,
77,
0,

77,
0,

77,
0,

77,
0,

77,
0,

40,
77,
0,

77,
0,

62,
77,
0,

76,
77,
0,

77,
0,

77,
0,

63,
77,
0,

77,
0,

51,
77,
0,

9,
77,
0,

77,
0,

54,
77,
0,

71,
77,
0,

77,
0,

77,
0,

77,
0,

77,
0,

60,
77,
0,

77,
0,

52,
77,
0,

75,
77,
0,

74,
77,
0,

49,
77,
0,

8,
77,
0,

46,
77,
0,
0};
# define YYTYPE int
struct yywork { YYTYPE verify, advance; } yycrank[] = {
0,0,	0,0,	3,13,	0,0,	
0,0,	0,0,	0,0,	0,0,	
0,0,	0,0,	3,14,	3,15,	
0,0,	0,0,	0,0,	0,0,	
0,0,	14,67,	0,0,	0,0,	
0,0,	5,59,	0,0,	0,0,	
0,0,	0,0,	0,0,	0,0,	
0,0,	5,59,	5,60,	0,0,	
0,0,	0,0,	3,16,	3,17,	
3,18,	3,19,	3,20,	3,21,	
14,67,	37,95,	3,22,	3,23,	
3,24,	70,0,	3,25,	3,26,	
3,27,	3,28,	6,61,	0,0,	
0,0,	0,0,	5,61,	0,0,	
0,0,	3,28,	0,0,	0,0,	
3,29,	3,30,	3,31,	3,32,	
0,0,	21,75,	3,33,	3,34,	
5,59,	10,65,	3,35,	23,76,	
3,33,	0,0,	16,68,	0,0,	
5,59,	0,0,	0,0,	3,36,	
0,0,	0,0,	0,0,	0,0,	
0,0,	5,59,	0,0,	24,78,	
9,63,	20,74,	23,77,	5,59,	
27,83,	3,37,	3,38,	3,39,	
9,63,	9,64,	3,40,	3,41,	
3,42,	3,43,	3,44,	3,45,	
3,46,	24,79,	3,47,	25,80,	
6,62,	3,48,	3,49,	3,50,	
5,62,	3,51,	10,66,	3,52,	
3,53,	3,54,	30,87,	31,88,	
3,55,	32,89,	32,90,	25,81,	
34,92,	3,56,	3,57,	3,58,	
4,16,	4,17,	4,18,	4,19,	
4,20,	4,21,	9,65,	9,63,	
4,22,	4,23,	4,24,	16,69,	
4,25,	4,26,	4,27,	9,63,	
35,93,	36,94,	39,96,	40,97,	
41,98,	43,101,	42,99,	44,103,	
9,63,	42,100,	4,29,	4,30,	
4,31,	4,32,	9,63,	43,102,	
45,105,	4,34,	47,110,	44,104,	
4,35,	49,114,	46,108,	50,115,	
18,70,	45,106,	47,111,	48,112,	
51,116,	4,36,	52,117,	45,107,	
18,70,	18,0,	52,118,	9,66,	
46,109,	48,113,	54,125,	55,126,	
56,127,	76,142,	82,86,	4,37,	
4,38,	4,39,	92,145,	93,146,	
4,40,	4,41,	4,42,	4,43,	
4,44,	4,45,	4,46,	97,147,	
4,47,	98,148,	99,149,	4,48,	
4,49,	4,50,	101,152,	4,51,	
100,150,	4,52,	4,53,	4,54,	
103,153,	100,151,	4,55,	18,70,	
105,156,	53,119,	82,86,	4,56,	
4,57,	4,58,	106,157,	18,70,	
53,120,	53,121,	53,122,	104,154,	
107,158,	53,123,	108,159,	109,160,	
18,70,	53,124,	104,155,	111,161,	
112,163,	113,164,	18,70,	19,71,	
19,71,	19,71,	19,71,	19,71,	
19,71,	19,71,	19,71,	19,71,	
19,71,	114,165,	115,166,	111,162,	
116,167,	117,168,	118,169,	119,170,	
19,72,	19,72,	19,72,	19,72,	
19,72,	19,72,	19,72,	19,72,	
19,72,	19,72,	19,72,	19,72,	
19,72,	19,73,	19,72,	19,72,	
19,72,	19,72,	19,72,	19,72,	
19,72,	19,72,	19,72,	19,72,	
19,72,	19,72,	121,173,	122,174,	
123,175,	124,176,	19,72,	126,179,	
19,72,	19,72,	19,72,	19,72,	
19,72,	19,72,	19,72,	19,72,	
19,72,	19,72,	19,72,	19,72,	
19,72,	19,72,	19,72,	19,72,	
19,72,	19,72,	19,72,	19,72,	
19,72,	19,72,	19,72,	19,72,	
19,72,	19,72,	26,82,	26,82,	
26,82,	26,82,	26,82,	26,82,	
26,82,	26,82,	26,82,	26,82,	
28,84,	145,182,	28,85,	28,85,	
28,85,	28,85,	28,85,	28,85,	
28,85,	28,85,	28,85,	28,85,	
33,91,	33,91,	33,91,	33,91,	
33,91,	33,91,	33,91,	33,91,	
33,91,	33,91,	120,171,	28,86,	
147,183,	148,184,	149,185,	150,186,	
120,172,	33,91,	33,91,	33,91,	
33,91,	33,91,	33,91,	33,91,	
33,91,	33,91,	33,91,	33,91,	
33,91,	33,91,	33,91,	33,91,	
33,91,	33,91,	33,91,	33,91,	
33,91,	33,91,	33,91,	33,91,	
33,91,	33,91,	33,91,	28,86,	
152,187,	153,188,	154,189,	33,91,	
156,190,	33,91,	33,91,	33,91,	
33,91,	33,91,	33,91,	33,91,	
33,91,	33,91,	33,91,	33,91,	
33,91,	33,91,	33,91,	33,91,	
33,91,	33,91,	33,91,	33,91,	
33,91,	33,91,	33,91,	33,91,	
33,91,	33,91,	33,91,	62,128,	
158,191,	66,140,	159,192,	160,193,	
161,194,	163,195,	165,196,	62,128,	
62,0,	66,140,	66,0,	71,71,	
71,71,	71,71,	71,71,	71,71,	
71,71,	71,71,	71,71,	71,71,	
71,71,	72,72,	72,72,	72,72,	
72,72,	72,72,	72,72,	72,72,	
72,72,	72,72,	72,72,	166,197,	
62,129,	167,198,	168,199,	169,200,	
73,72,	73,72,	73,72,	73,72,	
73,72,	73,72,	73,72,	73,72,	
73,72,	73,72,	62,130,	171,201,	
66,140,	172,202,	125,177,	72,72,	
173,203,	174,204,	62,128,	175,205,	
66,140,	176,206,	73,141,	125,178,	
177,207,	178,208,	179,209,	62,128,	
182,211,	66,140,	73,72,	183,212,	
184,213,	62,128,	185,214,	66,140,	
84,84,	84,84,	84,84,	84,84,	
84,84,	84,84,	84,84,	84,84,	
84,84,	84,84,	130,180,	130,180,	
130,180,	130,180,	130,180,	130,180,	
130,180,	130,180,	62,131,	186,215,	
187,216,	84,86,	190,217,	62,132,	
62,133,	191,218,	192,219,	194,220,	
62,134,	195,221,	196,222,	197,223,	
198,224,	200,225,	201,226,	202,227,	
62,135,	204,228,	205,229,	206,230,	
62,136,	207,231,	62,137,	208,232,	
62,138,	209,233,	62,139,	215,234,	
216,235,	86,143,	217,236,	86,143,	
218,237,	84,86,	86,144,	86,144,	
86,144,	86,144,	86,144,	86,144,	
86,144,	86,144,	86,144,	86,144,	
139,181,	139,181,	139,181,	139,181,	
139,181,	139,181,	139,181,	139,181,	
139,181,	139,181,	219,238,	221,239,	
223,240,	224,241,	225,242,	227,243,	
229,244,	139,181,	139,181,	139,181,	
139,181,	139,181,	139,181,	230,245,	
231,246,	141,72,	141,72,	141,72,	
141,72,	141,72,	141,72,	141,72,	
141,72,	141,72,	141,72,	143,144,	
143,144,	143,144,	143,144,	143,144,	
143,144,	143,144,	143,144,	143,144,	
143,144,	232,247,	234,248,	237,249,	
238,250,	139,181,	139,181,	139,181,	
139,181,	139,181,	139,181,	141,72,	
180,210,	180,210,	180,210,	180,210,	
180,210,	180,210,	180,210,	180,210,	
240,251,	243,252,	246,253,	247,254,	
248,255,	249,256,	251,257,	0,0,	
0,0};
struct yysvf yysvec[] = {
0,	0,	0,
yycrank+0,	0,		0,	
yycrank+0,	0,		0,	
yycrank+-1,	0,		0,	
yycrank+-95,	yysvec+3,	0,	
yycrank+-20,	0,		0,	
yycrank+-16,	yysvec+5,	0,	
yycrank+0,	0,		0,	
yycrank+0,	0,		0,	
yycrank+-87,	0,		0,	
yycrank+-22,	yysvec+9,	0,	
yycrank+0,	0,		0,	
yycrank+0,	0,		0,	
yycrank+0,	0,		yyvstop+1,
yycrank+8,	0,		yyvstop+3,
yycrank+0,	0,		yyvstop+6,
yycrank+13,	0,		yyvstop+8,
yycrank+0,	0,		yyvstop+11,
yycrank+-167,	0,		yyvstop+14,
yycrank+191,	0,		yyvstop+17,
yycrank+28,	0,		yyvstop+20,
yycrank+27,	0,		yyvstop+22,
yycrank+0,	0,		yyvstop+24,
yycrank+29,	0,		yyvstop+27,
yycrank+44,	0,		yyvstop+29,
yycrank+62,	0,		yyvstop+31,
yycrank+266,	0,		yyvstop+33,
yycrank+31,	0,		yyvstop+35,
yycrank+278,	0,		yyvstop+37,
yycrank+0,	0,		yyvstop+40,
yycrank+57,	0,		yyvstop+43,
yycrank+58,	0,		yyvstop+46,
yycrank+60,	0,		yyvstop+49,
yycrank+288,	0,		yyvstop+52,
yycrank+55,	yysvec+33,	yyvstop+55,
yycrank+66,	yysvec+33,	yyvstop+58,
yycrank+75,	yysvec+33,	yyvstop+61,
yycrank+31,	0,		yyvstop+64,
yycrank+0,	0,		yyvstop+66,
yycrank+85,	0,		yyvstop+69,
yycrank+31,	yysvec+33,	yyvstop+72,
yycrank+34,	yysvec+33,	yyvstop+75,
yycrank+42,	yysvec+33,	yyvstop+78,
yycrank+48,	yysvec+33,	yyvstop+81,
yycrank+43,	yysvec+33,	yyvstop+84,
yycrank+58,	yysvec+33,	yyvstop+87,
yycrank+65,	yysvec+33,	yyvstop+90,
yycrank+60,	yysvec+33,	yyvstop+93,
yycrank+70,	yysvec+33,	yyvstop+96,
yycrank+68,	yysvec+33,	yyvstop+99,
yycrank+66,	yysvec+33,	yyvstop+102,
yycrank+58,	yysvec+33,	yyvstop+105,
yycrank+77,	yysvec+33,	yyvstop+108,
yycrank+112,	yysvec+33,	yyvstop+111,
yycrank+71,	yysvec+33,	yyvstop+114,
yycrank+79,	yysvec+33,	yyvstop+117,
yycrank+60,	0,		yyvstop+120,
yycrank+0,	0,		yyvstop+122,
yycrank+0,	0,		yyvstop+125,
yycrank+0,	0,		yyvstop+128,
yycrank+0,	0,		yyvstop+130,
yycrank+0,	0,		yyvstop+132,
yycrank+-410,	0,		yyvstop+135,
yycrank+0,	0,		yyvstop+137,
yycrank+0,	0,		yyvstop+139,
yycrank+0,	0,		yyvstop+141,
yycrank+-412,	0,		yyvstop+144,
yycrank+0,	yysvec+14,	yyvstop+146,
yycrank+0,	0,		yyvstop+148,
yycrank+0,	0,		yyvstop+150,
yycrank+-35,	yysvec+18,	yyvstop+152,
yycrank+375,	0,		yyvstop+154,
yycrank+385,	yysvec+19,	yyvstop+156,
yycrank+400,	yysvec+19,	yyvstop+158,
yycrank+0,	0,		yyvstop+160,
yycrank+0,	0,		yyvstop+162,
yycrank+124,	0,		yyvstop+164,
yycrank+0,	0,		yyvstop+166,
yycrank+0,	0,		yyvstop+168,
yycrank+0,	0,		yyvstop+170,
yycrank+0,	0,		yyvstop+172,
yycrank+0,	0,		yyvstop+174,
yycrank+117,	yysvec+26,	yyvstop+176,
yycrank+0,	0,		yyvstop+178,
yycrank+436,	0,		yyvstop+180,
yycrank+0,	yysvec+28,	yyvstop+182,
yycrank+490,	0,		0,	
yycrank+0,	0,		yyvstop+184,
yycrank+0,	0,		yyvstop+186,
yycrank+0,	0,		yyvstop+188,
yycrank+0,	0,		yyvstop+190,
yycrank+0,	yysvec+33,	yyvstop+192,
yycrank+119,	yysvec+33,	yyvstop+194,
yycrank+123,	yysvec+33,	yyvstop+196,
yycrank+0,	yysvec+33,	yyvstop+198,
yycrank+0,	0,		yyvstop+201,
yycrank+0,	0,		yyvstop+203,
yycrank+102,	yysvec+33,	yyvstop+205,
yycrank+100,	yysvec+33,	yyvstop+207,
yycrank+91,	yysvec+33,	yyvstop+209,
yycrank+98,	yysvec+33,	yyvstop+211,
yycrank+98,	yysvec+33,	yyvstop+213,
yycrank+0,	yysvec+33,	yyvstop+215,
yycrank+97,	yysvec+33,	yyvstop+218,
yycrank+122,	yysvec+33,	yyvstop+220,
yycrank+108,	yysvec+33,	yyvstop+222,
yycrank+108,	yysvec+33,	yyvstop+224,
yycrank+118,	yysvec+33,	yyvstop+226,
yycrank+114,	yysvec+33,	yyvstop+228,
yycrank+114,	yysvec+33,	yyvstop+230,
yycrank+0,	yysvec+33,	yyvstop+232,
yycrank+135,	yysvec+33,	yyvstop+235,
yycrank+126,	yysvec+33,	yyvstop+238,
yycrank+134,	yysvec+33,	yyvstop+240,
yycrank+133,	yysvec+33,	yyvstop+242,
yycrank+130,	yysvec+33,	yyvstop+244,
yycrank+147,	yysvec+33,	yyvstop+246,
yycrank+143,	yysvec+33,	yyvstop+248,
yycrank+138,	yysvec+33,	yyvstop+250,
yycrank+145,	yysvec+33,	yyvstop+252,
yycrank+238,	yysvec+33,	yyvstop+254,
yycrank+168,	yysvec+33,	yyvstop+256,
yycrank+186,	yysvec+33,	yyvstop+258,
yycrank+186,	yysvec+33,	yyvstop+260,
yycrank+170,	yysvec+33,	yyvstop+262,
yycrank+354,	yysvec+33,	yyvstop+264,
yycrank+182,	yysvec+33,	yyvstop+266,
yycrank+0,	0,		yyvstop+268,
yycrank+0,	0,		yyvstop+270,
yycrank+0,	0,		yyvstop+272,
yycrank+446,	0,		yyvstop+275,
yycrank+0,	0,		yyvstop+278,
yycrank+0,	0,		yyvstop+281,
yycrank+0,	0,		yyvstop+284,
yycrank+0,	0,		yyvstop+287,
yycrank+0,	0,		yyvstop+290,
yycrank+0,	0,		yyvstop+293,
yycrank+0,	0,		yyvstop+296,
yycrank+0,	0,		yyvstop+299,
yycrank+500,	0,		yyvstop+302,
yycrank+0,	0,		yyvstop+304,
yycrank+525,	yysvec+19,	yyvstop+306,
yycrank+0,	0,		yyvstop+309,
yycrank+535,	0,		0,	
yycrank+0,	yysvec+143,	yyvstop+311,
yycrank+252,	yysvec+33,	yyvstop+313,
yycrank+0,	yysvec+33,	yyvstop+315,
yycrank+238,	yysvec+33,	yyvstop+318,
yycrank+252,	yysvec+33,	yyvstop+320,
yycrank+235,	yysvec+33,	yyvstop+322,
yycrank+235,	yysvec+33,	yyvstop+324,
yycrank+0,	yysvec+33,	yyvstop+326,
yycrank+279,	yysvec+33,	yyvstop+329,
yycrank+280,	yysvec+33,	yyvstop+331,
yycrank+266,	yysvec+33,	yyvstop+333,
yycrank+0,	yysvec+33,	yyvstop+335,
yycrank+267,	yysvec+33,	yyvstop+338,
yycrank+0,	yysvec+33,	yyvstop+340,
yycrank+313,	yysvec+33,	yyvstop+343,
yycrank+306,	yysvec+33,	yyvstop+345,
yycrank+317,	yysvec+33,	yyvstop+347,
yycrank+315,	yysvec+33,	yyvstop+349,
yycrank+0,	yysvec+33,	yyvstop+351,
yycrank+314,	yysvec+33,	yyvstop+354,
yycrank+0,	yysvec+33,	yyvstop+356,
yycrank+319,	yysvec+33,	yyvstop+359,
yycrank+327,	yysvec+33,	yyvstop+361,
yycrank+335,	yysvec+33,	yyvstop+363,
yycrank+346,	yysvec+33,	yyvstop+365,
yycrank+330,	yysvec+33,	yyvstop+367,
yycrank+0,	yysvec+33,	yyvstop+369,
yycrank+354,	yysvec+33,	yyvstop+372,
yycrank+356,	yysvec+33,	yyvstop+374,
yycrank+348,	yysvec+33,	yyvstop+376,
yycrank+355,	yysvec+33,	yyvstop+378,
yycrank+352,	yysvec+33,	yyvstop+380,
yycrank+353,	yysvec+33,	yyvstop+383,
yycrank+361,	yysvec+33,	yyvstop+385,
yycrank+361,	yysvec+33,	yyvstop+387,
yycrank+366,	yysvec+33,	yyvstop+389,
yycrank+556,	0,		yyvstop+391,
yycrank+0,	yysvec+139,	yyvstop+393,
yycrank+398,	yysvec+33,	yyvstop+395,
yycrank+429,	yysvec+33,	yyvstop+397,
yycrank+373,	yysvec+33,	yyvstop+399,
yycrank+381,	yysvec+33,	yyvstop+401,
yycrank+398,	yysvec+33,	yyvstop+403,
yycrank+388,	yysvec+33,	yyvstop+405,
yycrank+0,	yysvec+33,	yyvstop+407,
yycrank+0,	yysvec+33,	yyvstop+410,
yycrank+391,	yysvec+33,	yyvstop+413,
yycrank+393,	yysvec+33,	yyvstop+415,
yycrank+405,	yysvec+33,	yyvstop+418,
yycrank+0,	yysvec+33,	yyvstop+420,
yycrank+391,	yysvec+33,	yyvstop+423,
yycrank+397,	yysvec+33,	yyvstop+425,
yycrank+410,	yysvec+33,	yyvstop+427,
yycrank+413,	yysvec+33,	yyvstop+429,
yycrank+400,	yysvec+33,	yyvstop+432,
yycrank+0,	yysvec+33,	yyvstop+434,
yycrank+403,	yysvec+33,	yyvstop+437,
yycrank+402,	yysvec+33,	yyvstop+439,
yycrank+409,	yysvec+33,	yyvstop+441,
yycrank+0,	yysvec+33,	yyvstop+443,
yycrank+421,	yysvec+33,	yyvstop+446,
yycrank+406,	yysvec+33,	yyvstop+448,
yycrank+422,	yysvec+33,	yyvstop+450,
yycrank+406,	yysvec+33,	yyvstop+452,
yycrank+415,	yysvec+33,	yyvstop+454,
yycrank+428,	yysvec+33,	yyvstop+456,
yycrank+0,	0,		yyvstop+458,
yycrank+0,	yysvec+33,	yyvstop+460,
yycrank+0,	yysvec+33,	yyvstop+463,
yycrank+0,	yysvec+33,	yyvstop+466,
yycrank+0,	yysvec+33,	yyvstop+469,
yycrank+421,	yysvec+33,	yyvstop+472,
yycrank+431,	yysvec+33,	yyvstop+474,
yycrank+430,	yysvec+33,	yyvstop+476,
yycrank+431,	yysvec+33,	yyvstop+478,
yycrank+448,	yysvec+33,	yyvstop+480,
yycrank+0,	yysvec+33,	yyvstop+482,
yycrank+455,	yysvec+33,	yyvstop+485,
yycrank+0,	yysvec+33,	yyvstop+487,
yycrank+455,	yysvec+33,	yyvstop+490,
yycrank+459,	yysvec+33,	yyvstop+492,
yycrank+452,	yysvec+33,	yyvstop+495,
yycrank+0,	yysvec+33,	yyvstop+497,
yycrank+447,	yysvec+33,	yyvstop+500,
yycrank+0,	yysvec+33,	yyvstop+502,
yycrank+450,	yysvec+33,	yyvstop+505,
yycrank+462,	yysvec+33,	yyvstop+507,
yycrank+471,	yysvec+33,	yyvstop+509,
yycrank+492,	yysvec+33,	yyvstop+511,
yycrank+0,	yysvec+33,	yyvstop+513,
yycrank+477,	yysvec+33,	yyvstop+516,
yycrank+0,	yysvec+33,	yyvstop+518,
yycrank+0,	yysvec+33,	yyvstop+521,
yycrank+484,	yysvec+33,	yyvstop+524,
yycrank+495,	yysvec+33,	yyvstop+526,
yycrank+0,	yysvec+33,	yyvstop+528,
yycrank+504,	yysvec+33,	yyvstop+531,
yycrank+0,	yysvec+33,	yyvstop+533,
yycrank+0,	yysvec+33,	yyvstop+536,
yycrank+511,	yysvec+33,	yyvstop+539,
yycrank+0,	yysvec+33,	yyvstop+541,
yycrank+0,	yysvec+33,	yyvstop+544,
yycrank+500,	yysvec+33,	yyvstop+547,
yycrank+501,	yysvec+33,	yyvstop+549,
yycrank+515,	yysvec+33,	yyvstop+551,
yycrank+507,	yysvec+33,	yyvstop+553,
yycrank+0,	yysvec+33,	yyvstop+555,
yycrank+517,	yysvec+33,	yyvstop+558,
yycrank+0,	yysvec+33,	yyvstop+560,
yycrank+0,	yysvec+33,	yyvstop+563,
yycrank+0,	yysvec+33,	yyvstop+566,
yycrank+0,	yysvec+33,	yyvstop+569,
yycrank+0,	yysvec+33,	yyvstop+572,
yycrank+0,	yysvec+33,	yyvstop+575,
0,	0,	0};
struct yywork *yytop = yycrank+618;
struct yysvf *yybgin = yysvec+1;
Uchar yymatch[] = {
00  ,01  ,01  ,01  ,01  ,01  ,01  ,01  ,
01  ,011 ,012 ,01  ,01  ,01  ,01  ,01  ,
01  ,01  ,01  ,01  ,01  ,01  ,01  ,01  ,
01  ,01  ,01  ,01  ,01  ,01  ,01  ,01  ,
011 ,01  ,01  ,01  ,01  ,01  ,01  ,01  ,
01  ,01  ,01  ,01  ,01  ,01  ,01  ,01  ,
'0' ,'0' ,'0' ,'0' ,'0' ,'0' ,'0' ,'0' ,
'8' ,'8' ,01  ,01  ,01  ,01  ,01  ,01  ,
01  ,'A' ,'A' ,'A' ,'A' ,'A' ,'A' ,'G' ,
'G' ,'G' ,'G' ,'G' ,'G' ,'G' ,'G' ,'G' ,
'G' ,'G' ,'G' ,'G' ,'G' ,'G' ,'G' ,'G' ,
'G' ,'G' ,'G' ,01  ,01  ,01  ,01  ,'G' ,
01  ,'A' ,'A' ,'A' ,'A' ,'A' ,'A' ,'G' ,
'G' ,'G' ,'G' ,'G' ,'G' ,'G' ,'G' ,'G' ,
'G' ,'G' ,'G' ,'G' ,'G' ,'G' ,'G' ,'G' ,
'G' ,'G' ,'G' ,01  ,01  ,01  ,01  ,01  ,
01  ,01  ,01  ,01  ,01  ,01  ,01  ,01  ,
01  ,01  ,01  ,01  ,01  ,01  ,01  ,01  ,
01  ,01  ,01  ,01  ,01  ,01  ,01  ,01  ,
01  ,01  ,01  ,01  ,01  ,01  ,01  ,01  ,
01  ,01  ,01  ,01  ,01  ,01  ,01  ,01  ,
01  ,01  ,01  ,01  ,01  ,01  ,01  ,01  ,
01  ,01  ,01  ,01  ,01  ,01  ,01  ,01  ,
01  ,01  ,01  ,01  ,01  ,01  ,01  ,01  ,
01  ,01  ,01  ,01  ,01  ,01  ,01  ,01  ,
01  ,01  ,01  ,01  ,01  ,01  ,01  ,01  ,
01  ,01  ,01  ,01  ,01  ,01  ,01  ,01  ,
01  ,01  ,01  ,01  ,01  ,01  ,01  ,01  ,
01  ,01  ,01  ,01  ,01  ,01  ,01  ,01  ,
01  ,01  ,01  ,01  ,01  ,01  ,01  ,01  ,
01  ,01  ,01  ,01  ,01  ,01  ,01  ,01  ,
01  ,01  ,01  ,01  ,01  ,01  ,01  ,01  ,
0};
Uchar yyextra[] = {
0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,
0};
int yylineno =1;
# define YYU(x) x
char yytext[YYLMAX];
struct yysvf *yylstate [YYLMAX], **yylsp, **yyolsp;
Uchar yysbuf[YYLMAX];
Uchar *yysptr = yysbuf;
int *yyfnd;
extern struct yysvf *yyestate;
int yyprevious = YYNEWLINE;
# ifdef LEXDEBUG
extern void allprint(char);
# endif
yylook(void){
	struct yysvf *yystate, **lsp;
	struct yywork *yyt;
	struct yysvf *yyz;
	int yych;
	struct yywork *yyr;
# ifdef LEXDEBUG
	int debug;
# endif
	Uchar *yylastch;
	/* start off machines */
# ifdef LEXDEBUG
	debug = 0;
# endif
	if (!yymorfg)
		yylastch = (Uchar*)yytext;
	else {
		yymorfg=0;
		yylastch = (Uchar*)yytext+yyleng;
		}
	for(;;){
		lsp = yylstate;
		yyestate = yystate = yybgin;
		if (yyprevious==YYNEWLINE) yystate++;
		for (;;){
# ifdef LEXDEBUG
			if(debug)fprintf(yyout,"state %d\n",yystate-yysvec-1);
# endif
			yyt = yystate->yystoff;
			if(yyt == yycrank){		/* may not be any transitions */
				yyz = yystate->yyother;
				if(yyz == 0)break;
				if(yyz->yystoff == yycrank)break;
				}
			*yylastch++ = yych = input();
		tryagain:
# ifdef LEXDEBUG
			if(debug){
				fprintf(yyout,"char ");
				allprint(yych);
				putchar('\n');
				}
# endif
			yyr = yyt;
			if ( (long)yyt > (long)yycrank){
				yyt = yyr + yych;
				if (yyt <= yytop && yyt->verify+yysvec == yystate){
					if(yyt->advance+yysvec == YYLERR)	/* error transitions */
						{unput(*--yylastch);break;}
					*lsp++ = yystate = yyt->advance+yysvec;
					goto contin;
					}
				}
# ifdef YYOPTIM
			else if((long)yyt < (long)yycrank) {		/* r < yycrank */
				yyt = yyr = yycrank+(yycrank-yyt);
# ifdef LEXDEBUG
				if(debug)fprintf(yyout,"compressed state\n");
# endif
				yyt = yyt + yych;
				if(yyt <= yytop && yyt->verify+yysvec == yystate){
					if(yyt->advance+yysvec == YYLERR)	/* error transitions */
						{unput(*--yylastch);break;}
					*lsp++ = yystate = yyt->advance+yysvec;
					goto contin;
					}
				yyt = yyr + YYU(yymatch[yych]);
# ifdef LEXDEBUG
				if(debug){
					fprintf(yyout,"try fall back character ");
					allprint(YYU(yymatch[yych]));
					putchar('\n');
					}
# endif
				if(yyt <= yytop && yyt->verify+yysvec == yystate){
					if(yyt->advance+yysvec == YYLERR)	/* error transition */
						{unput(*--yylastch);break;}
					*lsp++ = yystate = yyt->advance+yysvec;
					goto contin;
					}
				}
			if ((yystate = yystate->yyother) && (yyt= yystate->yystoff) != yycrank){
# ifdef LEXDEBUG
				if(debug)fprintf(yyout,"fall back to state %d\n",yystate-yysvec-1);
# endif
				goto tryagain;
				}
# endif
			else
				{unput(*--yylastch);break;}
		contin:
# ifdef LEXDEBUG
			if(debug){
				fprintf(yyout,"state %d char ",yystate-yysvec-1);
				allprint(yych);
				putchar('\n');
				}
# endif
			;
			}
# ifdef LEXDEBUG
		if(debug){
			fprintf(yyout,"stopped at %d with ",*(lsp-1)-yysvec-1);
			allprint(yych);
			putchar('\n');
			}
# endif
		while (lsp-- > yylstate){
			*yylastch-- = 0;
			if (*lsp != 0 && (yyfnd= (*lsp)->yystops) && *yyfnd > 0){
				yyolsp = lsp;
				if(yyextra[*yyfnd]){		/* must backup */
					while(yyback((*lsp)->yystops,-*yyfnd) != 1 && lsp > yylstate){
						lsp--;
						unput(*yylastch--);
						}
					}
				yyprevious = YYU(*yylastch);
				yylsp = lsp;
				yyleng = yylastch-(Uchar*)yytext+1;
				yytext[yyleng] = 0;
# ifdef LEXDEBUG
				if(debug){
					fprintf(yyout,"\nmatch '%s'", yytext);
					fprintf(yyout," action %d\n",*yyfnd);
					}
# endif
				return(*yyfnd++);
				}
			unput(*yylastch);
			}
		if (yytext[0] == 0  /* && feof(yyin) */)
			{
			yysptr=yysbuf;
			return(0);
			}
		yyprevious = input();
		yytext[0] = yyprevious;
		if (yyprevious>0)
			output(yyprevious);
		yylastch = (Uchar*)yytext;
# ifdef LEXDEBUG
		if(debug)putchar('\n');
# endif
		}
	return(0);	/* shut up the compiler; i have no idea what should be returned */
	}
yyback(int *p, int m)
{
if (p==0) return(0);
while (*p)
	{
	if (*p++ == m)
		return(1);
	}
return(0);
}
	/* the following are only used in the lex library */
yyinput(void){
	return(input());
}
void
yyoutput(int c)
{
	output(c);
}
void
yyunput(int c)
{
	unput(c);
}
