
/*  A Bison parser, made from ./config/m68k-parse.y with Bison version GNU Bison version 1.24
  */

#define YYBISON 1  /* Identify Bison output.  */

#define	DR	258
#define	AR	259
#define	FPR	260
#define	FPCR	261
#define	LPC	262
#define	ZAR	263
#define	ZDR	264
#define	LZPC	265
#define	CREG	266
#define	INDEXREG	267
#define	EXPR	268

#line 27 "./config/m68k-parse.y"


#include "as.h"
#include "tc-m68k.h"
#include "m68k-parse.h"

/* Remap normal yacc parser interface names (yyparse, yylex, yyerror,
   etc), as well as gratuitiously global symbol names If other parser
   generators (bison, byacc, etc) produce additional global names that
   conflict at link time, then those parser generators need to be
   fixed instead of adding those names to this list. */

#define	yymaxdepth m68k_maxdepth
#define	yyparse	m68k_parse
#define	yylex	m68k_lex
#define	yyerror	m68k_error
#define	yylval	m68k_lval
#define	yychar	m68k_char
#define	yydebug	m68k_debug
#define	yypact	m68k_pact	
#define	yyr1	m68k_r1			
#define	yyr2	m68k_r2			
#define	yydef	m68k_def		
#define	yychk	m68k_chk		
#define	yypgo	m68k_pgo		
#define	yyact	m68k_act		
#define	yyexca	m68k_exca
#define yyerrflag m68k_errflag
#define yynerrs	m68k_nerrs
#define	yyps	m68k_ps
#define	yypv	m68k_pv
#define	yys	m68k_s
#define	yy_yys	m68k_yys
#define	yystate	m68k_state
#define	yytmp	m68k_tmp
#define	yyv	m68k_v
#define	yy_yyv	m68k_yyv
#define	yyval	m68k_val
#define	yylloc	m68k_lloc
#define yyreds	m68k_reds		/* With YYDEBUG defined */
#define yytoks	m68k_toks		/* With YYDEBUG defined */
#define yylhs	m68k_yylhs
#define yylen	m68k_yylen
#define yydefred m68k_yydefred
#define yydgoto	m68k_yydgoto
#define yysindex m68k_yysindex
#define yyrindex m68k_yyrindex
#define yygindex m68k_yygindex
#define yytable	 m68k_yytable
#define yycheck	 m68k_yycheck

#ifndef YYDEBUG
#define YYDEBUG 1
#endif

/* Internal functions.  */

static enum m68k_register m68k_reg_parse PARAMS ((char **));
static int yylex PARAMS (());
static void yyerror PARAMS ((const char *));

/* The parser sets fields pointed to by this global variable.  */
static struct m68k_op *op;


#line 93 "./config/m68k-parse.y"
typedef union
{
  struct m68k_indexreg indexreg;
  enum m68k_register reg;
  struct m68k_exp exp;
  unsigned long mask;
  int onereg;
} YYSTYPE;

#ifndef YYLTYPE
typedef
  struct yyltype
    {
      int timestamp;
      int first_line;
      int first_column;
      int last_line;
      int last_column;
      char *text;
   }
  yyltype;

#define YYLTYPE yyltype
#endif

#include <stdio.h>

#ifndef __cplusplus
#ifndef __STDC__
#define const
#endif
#endif



#define	YYFINAL		168
#define	YYFLAG		-32768
#define	YYNTBASE	25

#define YYTRANSLATE(x) ((unsigned)(x) <= 268 ? yytranslate[x] : 44)

static const char yytranslate[] = {     0,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,    14,     2,     2,    15,     2,    16,
    17,     2,    18,    20,    19,     2,    24,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,    23,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
    21,     2,    22,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     1,     2,     3,     4,     5,
     6,     7,     8,     9,    10,    11,    12,    13
};

#if YYDEBUG != 0
static const short yyprhs[] = {     0,
     0,     2,     4,     6,     8,    10,    12,    14,    16,    18,
    21,    24,    26,    30,    35,    40,    46,    51,    55,    59,
    63,    71,    79,    86,    93,    99,   106,   112,   118,   123,
   133,   141,   150,   157,   168,   177,   188,   197,   206,   209,
   213,   217,   223,   230,   241,   251,   262,   264,   266,   268,
   270,   272,   274,   276,   278,   280,   282,   284,   286,   288,
   290,   291,   293,   295,   297,   298,   301,   302,   305,   306,
   309,   311,   315,   319,   321,   323,   327,   331,   335,   337,
   339,   341
};

static const short yyrhs[] = {    26,
     0,    27,     0,    28,     0,     3,     0,     4,     0,     5,
     0,     6,     0,    11,     0,    13,     0,    14,    13,     0,
    15,    13,     0,    40,     0,    16,     4,    17,     0,    16,
     4,    17,    18,     0,    19,    16,     4,    17,     0,    16,
    13,    20,    34,    17,     0,    13,    16,    34,    17,     0,
    16,     7,    17,     0,    16,     8,    17,     0,    16,    10,
    17,     0,    16,    13,    20,    34,    20,    29,    17,     0,
    16,    13,    20,    34,    20,    36,    17,     0,    16,    13,
    20,    30,    37,    17,     0,    13,    16,    34,    20,    29,
    17,     0,    16,    34,    20,    29,    17,     0,    13,    16,
    34,    20,    36,    17,     0,    16,    34,    20,    36,    17,
     0,    13,    16,    30,    37,    17,     0,    16,    30,    37,
    17,     0,    16,    21,    13,    37,    22,    20,    29,    38,
    17,     0,    16,    21,    13,    37,    22,    38,    17,     0,
    16,    21,    34,    22,    20,    29,    38,    17,     0,    16,
    21,    34,    22,    38,    17,     0,    16,    21,    13,    20,
    34,    20,    29,    22,    38,    17,     0,    16,    21,    34,
    20,    29,    22,    38,    17,     0,    16,    21,    13,    20,
    34,    20,    36,    22,    38,    17,     0,    16,    21,    34,
    20,    36,    22,    38,    17,     0,    16,    21,    39,    30,
    37,    22,    38,    17,     0,    35,    23,     0,    35,    23,
    18,     0,    35,    23,    19,     0,    35,    23,    16,    13,
    17,     0,    35,    23,    16,    39,    29,    17,     0,    35,
    23,    16,    13,    17,    23,    16,    39,    29,    17,     0,
    35,    23,    16,    13,    17,    23,    16,    13,    17,     0,
    35,    23,    16,    39,    29,    17,    23,    16,    13,    17,
     0,    12,     0,    31,     0,    12,     0,    32,     0,    32,
     0,     4,     0,     8,     0,     3,     0,     9,     0,     4,
     0,     7,     0,    33,     0,    10,     0,     8,     0,     0,
    34,     0,     7,     0,    10,     0,     0,    20,    34,     0,
     0,    20,    13,     0,     0,    13,    20,     0,    42,     0,
    42,    24,    41,     0,    43,    24,    41,     0,    43,     0,
    42,     0,    42,    24,    41,     0,    43,    24,    41,     0,
    43,    19,    43,     0,     3,     0,     4,     0,     5,     0,
     6,     0
};

#endif

#if YYDEBUG != 0
static const short yyrline[] = { 0,
   116,   118,   119,   124,   130,   135,   140,   145,   150,   155,
   160,   165,   177,   183,   188,   193,   203,   213,   218,   223,
   228,   235,   246,   253,   260,   266,   277,   287,   294,   300,
   308,   315,   322,   328,   336,   343,   355,   366,   378,   387,
   395,   403,   413,   420,   428,   435,   448,   450,   462,   464,
   475,   477,   478,   483,   485,   490,   492,   498,   500,   501,
   506,   511,   516,   518,   523,   528,   536,   542,   550,   556,
   564,   566,   570,   581,   586,   587,   591,   597,   604,   609,
   613,   617
};

static const char * const yytname[] = {   "$","error","$undefined.","DR","AR",
"FPR","FPCR","LPC","ZAR","ZDR","LZPC","CREG","INDEXREG","EXPR","'#'","'&'","'('",
"')'","'+'","'-'","','","'['","']'","'@'","'/'","operand","generic_operand",
"motorola_operand","mit_operand","zireg","zdireg","zadr","zdr","apc","zapc",
"optzapc","zpc","optczapc","optcexpr","optexprc","reglist","ireglist","reglistpair",
"reglistreg",""
};
#endif

static const short yyr1[] = {     0,
    25,    25,    25,    26,    26,    26,    26,    26,    26,    26,
    26,    26,    27,    27,    27,    27,    27,    27,    27,    27,
    27,    27,    27,    27,    27,    27,    27,    27,    27,    27,
    27,    27,    27,    27,    27,    27,    27,    27,    28,    28,
    28,    28,    28,    28,    28,    28,    29,    29,    30,    30,
    31,    31,    31,    32,    32,    33,    33,    34,    34,    34,
    35,    35,    36,    36,    37,    37,    38,    38,    39,    39,
    40,    40,    40,    41,    41,    41,    41,    42,    43,    43,
    43,    43
};

static const short yyr2[] = {     0,
     1,     1,     1,     1,     1,     1,     1,     1,     1,     2,
     2,     1,     3,     4,     4,     5,     4,     3,     3,     3,
     7,     7,     6,     6,     5,     6,     5,     5,     4,     9,
     7,     8,     6,    10,     8,    10,     8,     8,     2,     3,
     3,     5,     6,    10,     9,    10,     1,     1,     1,     1,
     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
     0,     1,     1,     1,     0,     2,     0,     2,     0,     2,
     1,     3,     3,     1,     1,     3,     3,     3,     1,     1,
     1,     1
};

static const short yydefact[] = {    61,
    79,    80,    81,    82,    57,    60,    59,     8,     9,     0,
     0,     0,     0,     1,     2,     3,    58,    62,     0,    12,
    71,     0,     0,    10,    11,    54,    56,    57,    60,    55,
    59,    49,     0,    69,    65,    50,     0,     0,    39,     0,
     0,     0,    56,    65,     0,    13,    18,    19,    20,     0,
    65,     0,     0,     0,     0,     0,     0,    69,    40,    41,
    79,    80,    81,    82,    72,    75,    74,    78,    73,     0,
    17,     0,    14,    65,     0,    70,     0,     0,    67,    65,
    66,    29,    52,    63,    53,    64,    47,     0,    48,    51,
     0,    15,     0,     0,     0,     0,    28,     0,     0,     0,
    16,     0,    66,    67,     0,     0,     0,     0,     0,    25,
    27,    42,    70,     0,    76,    77,    24,    26,    23,     0,
     0,     0,     0,     0,    67,    67,    68,    67,    33,    67,
     0,    43,    21,    22,     0,     0,    67,    31,     0,     0,
     0,     0,     0,    69,     0,    67,    67,     0,    35,    37,
    32,    38,     0,     0,     0,     0,     0,    30,    45,     0,
     0,    34,    36,    44,    46,     0,     0,     0
};

static const short yydefgoto[] = {   166,
    14,    15,    16,    88,    35,    89,    90,    17,    18,    19,
    91,    55,   108,    53,    20,    65,    66,    67
};

static const short yypact[] = {    74,
    19,    14,    33,    53,-32768,-32768,-32768,-32768,    45,    57,
    81,    55,    79,-32768,-32768,-32768,-32768,-32768,    83,-32768,
    86,    -2,    95,-32768,-32768,-32768,    94,   104,   119,-32768,
   121,-32768,   113,   112,   120,-32768,   122,   137,   116,   125,
   125,   125,-32768,   120,    -5,   126,-32768,-32768,-32768,    95,
   123,   117,   115,    65,   128,   105,   129,   134,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,   124,    36,-32768,-32768,   132,
-32768,   105,-32768,   120,    25,    65,   130,   105,   131,   120,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,   133,-32768,-32768,
   136,-32768,    54,    17,   125,   125,-32768,   138,   139,   140,
-32768,   105,   141,   142,   143,   144,    88,   146,   145,-32768,
-32768,   135,-32768,   147,-32768,-32768,-32768,-32768,-32768,   151,
   152,   105,    88,   153,   154,   154,-32768,   154,-32768,   154,
   155,   149,-32768,-32768,   156,   157,   154,-32768,   160,   158,
   159,   163,   164,   169,   161,   154,   154,   166,-32768,-32768,
-32768,-32768,   106,    17,   171,   168,   170,-32768,-32768,   172,
   173,-32768,-32768,-32768,-32768,   186,   188,-32768
};

static const short yypgoto[] = {-32768,
-32768,-32768,-32768,   -71,   -15,-32768,    -7,-32768,   -10,-32768,
   -68,   -33,   -98,   -58,-32768,   -39,   191,     9
};


#define	YYLAST		191


static const short yytable[] = {    94,
    98,    37,    69,    99,    36,   124,   105,    44,    22,   106,
    70,    71,    45,    -5,    72,    36,    41,    77,    -4,    26,
    83,    42,   114,    52,    85,    30,   140,   141,    87,   142,
   120,   143,    -6,   121,    74,   128,   -56,    80,   148,    75,
   100,   101,    36,    81,   102,    36,   109,   156,   157,    68,
   135,   137,    -7,   136,    41,   115,   116,    26,    27,    96,
    23,    28,    29,    30,    31,   103,    32,    33,    43,    24,
   112,     5,     6,   113,     7,    34,     1,     2,     3,     4,
     5,     6,   160,     7,     8,   154,     9,    10,    11,    12,
    26,    83,    13,    25,    38,    85,    30,    26,    43,    87,
   127,     5,     6,    30,     7,    39,    32,    26,    83,    40,
    46,    84,    85,    30,    86,    43,    87,    26,     5,     6,
    47,     7,   159,    30,    51,   113,    32,    61,    62,    63,
    64,    58,    50,    59,    60,    48,    78,    49,    79,    54,
    57,    56,    76,    73,    82,    92,    93,    95,    97,   110,
   107,   104,   111,     0,   117,   118,   119,   131,     0,     0,
   122,   123,   129,   132,   125,   126,   130,   133,   134,   138,
   144,   145,   127,   139,   149,   150,   155,   146,   147,   151,
   152,   153,   158,   161,   162,   167,   163,   168,   164,   165,
    21
};

static const short yycheck[] = {    58,
    72,    12,    42,    72,    12,   104,    78,    23,     0,    78,
    44,    17,    23,     0,    20,    23,    19,    51,     0,     3,
     4,    24,    94,    34,     8,     9,   125,   126,    12,   128,
   102,   130,     0,   102,    50,   107,    23,    53,   137,    50,
    74,    17,    50,    54,    20,    53,    80,   146,   147,    41,
   122,   123,     0,   122,    19,    95,    96,     3,     4,    24,
    16,     7,     8,     9,    10,    76,    12,    13,     4,    13,
    17,     7,     8,    20,    10,    21,     3,     4,     5,     6,
     7,     8,   154,    10,    11,   144,    13,    14,    15,    16,
     3,     4,    19,    13,    16,     8,     9,     3,     4,    12,
    13,     7,     8,     9,    10,    23,    12,     3,     4,    24,
    17,     7,     8,     9,    10,     4,    12,     3,     7,     8,
    17,    10,    17,     9,    13,    20,    12,     3,     4,     5,
     6,    16,    20,    18,    19,    17,    20,    17,    22,    20,
     4,    20,    20,    18,    17,    17,    13,    24,    17,    17,
    20,    22,    17,    -1,    17,    17,    17,    23,    -1,    -1,
    20,    20,    17,    17,    22,    22,    22,    17,    17,    17,
    16,    23,    13,    20,    17,    17,    16,    22,    22,    17,
    17,    13,    17,    13,    17,     0,    17,     0,    17,    17,
     0
};
/* -*-C-*-  Note some compilers choke on comments on `#line' lines.  */
#line 3 "/usr/unsupported/share/bison.simple"

/* Skeleton output parser for bison,
   Copyright (C) 1984, 1989, 1990 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* As a special exception, when this file is copied by Bison into a
   Bison output file, you may use that output file without restriction.
   This special exception was added by the Free Software Foundation
   in version 1.24 of Bison.  */

#ifndef alloca
#ifdef __GNUC__
#define alloca __builtin_alloca
#else /* not GNU C.  */
#if (!defined (__STDC__) && defined (sparc)) || defined (__sparc__) || defined (__sparc) || defined (__sgi)
#include <alloca.h>
#else /* not sparc */
#if defined (MSDOS) && !defined (__TURBOC__)
#include <malloc.h>
#else /* not MSDOS, or __TURBOC__ */
#if defined(_AIX)
#include <malloc.h>
 #pragma alloca
#else /* not MSDOS, __TURBOC__, or _AIX */
#ifdef __hpux
#ifdef __cplusplus
extern "C" {
void *alloca (unsigned int);
};
#else /* not __cplusplus */
void *alloca ();
#endif /* not __cplusplus */
#endif /* __hpux */
#endif /* not _AIX */
#endif /* not MSDOS, or __TURBOC__ */
#endif /* not sparc.  */
#endif /* not GNU C.  */
#endif /* alloca not defined.  */

/* This is the parser code that is written into each bison parser
  when the %semantic_parser declaration is not specified in the grammar.
  It was written by Richard Stallman by simplifying the hairy parser
  used when %semantic_parser is specified.  */

/* Note: there must be only one dollar sign in this file.
   It is replaced by the list of actions, each action
   as one case of the switch.  */

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		-2
#define YYEOF		0
#define YYACCEPT	return(0)
#define YYABORT 	return(1)
#define YYERROR		goto yyerrlab1
/* Like YYERROR except do call yyerror.
   This remains here temporarily to ease the
   transition to the new meaning of YYERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  */
#define YYFAIL		goto yyerrlab
#define YYRECOVERING()  (!!yyerrstatus)
#define YYBACKUP(token, value) \
do								\
  if (yychar == YYEMPTY && yylen == 1)				\
    { yychar = (token), yylval = (value);			\
      yychar1 = YYTRANSLATE (yychar);				\
      YYPOPSTACK;						\
      goto yybackup;						\
    }								\
  else								\
    { yyerror ("syntax error: cannot back up"); YYERROR; }	\
while (0)

#define YYTERROR	1
#define YYERRCODE	256

#ifndef YYPURE
#define YYLEX		yylex()
#endif

#ifdef YYPURE
#ifdef YYLSP_NEEDED
#ifdef YYLEX_PARAM
#define YYLEX		yylex(&yylval, &yylloc, YYLEX_PARAM)
#else
#define YYLEX		yylex(&yylval, &yylloc)
#endif
#else /* not YYLSP_NEEDED */
#ifdef YYLEX_PARAM
#define YYLEX		yylex(&yylval, YYLEX_PARAM)
#else
#define YYLEX		yylex(&yylval)
#endif
#endif /* not YYLSP_NEEDED */
#endif

/* If nonreentrant, generate the variables here */

#ifndef YYPURE

int	yychar;			/*  the lookahead symbol		*/
YYSTYPE	yylval;			/*  the semantic value of the		*/
				/*  lookahead symbol			*/

#ifdef YYLSP_NEEDED
YYLTYPE yylloc;			/*  location data for the lookahead	*/
				/*  symbol				*/
#endif

int yynerrs;			/*  number of parse errors so far       */
#endif  /* not YYPURE */

#if YYDEBUG != 0
int yydebug;			/*  nonzero means print parse trace	*/
/* Since this is uninitialized, it does not stop multiple parsers
   from coexisting.  */
#endif

/*  YYINITDEPTH indicates the initial size of the parser's stacks	*/

#ifndef	YYINITDEPTH
#define YYINITDEPTH 200
#endif

/*  YYMAXDEPTH is the maximum size the stacks can grow to
    (effective only if the built-in stack extension method is used).  */

#if YYMAXDEPTH == 0
#undef YYMAXDEPTH
#endif

#ifndef YYMAXDEPTH
#define YYMAXDEPTH 10000
#endif

/* Prevent warning if -Wstrict-prototypes.  */
#ifdef __GNUC__
int yyparse (void);
#endif

#if __GNUC__ > 1		/* GNU C and GNU C++ define this.  */
#define __yy_memcpy(FROM,TO,COUNT)	__builtin_memcpy(TO,FROM,COUNT)
#else				/* not GNU C or C++ */
#ifndef __cplusplus

/* This is the most reliable way to avoid incompatibilities
   in available built-in functions on various systems.  */
static void
__yy_memcpy (from, to, count)
     char *from;
     char *to;
     int count;
{
  register char *f = from;
  register char *t = to;
  register int i = count;

  while (i-- > 0)
    *t++ = *f++;
}

#else /* __cplusplus */

/* This is the most reliable way to avoid incompatibilities
   in available built-in functions on various systems.  */
static void
__yy_memcpy (char *from, char *to, int count)
{
  register char *f = from;
  register char *t = to;
  register int i = count;

  while (i-- > 0)
    *t++ = *f++;
}

#endif
#endif

#line 192 "/usr/unsupported/share/bison.simple"

/* The user can define YYPARSE_PARAM as the name of an argument to be passed
   into yyparse.  The argument should have type void *.
   It should actually point to an object.
   Grammar actions can access the variable by casting it
   to the proper pointer type.  */

#ifdef YYPARSE_PARAM
#define YYPARSE_PARAM_DECL void *YYPARSE_PARAM;
#else
#define YYPARSE_PARAM
#define YYPARSE_PARAM_DECL
#endif

int
yyparse(YYPARSE_PARAM)
     YYPARSE_PARAM_DECL
{
  register int yystate;
  register int yyn;
  register short *yyssp;
  register YYSTYPE *yyvsp;
  int yyerrstatus;	/*  number of tokens to shift before error messages enabled */
  int yychar1 = 0;		/*  lookahead token as an internal (translated) token number */

  short	yyssa[YYINITDEPTH];	/*  the state stack			*/
  YYSTYPE yyvsa[YYINITDEPTH];	/*  the semantic value stack		*/

  short *yyss = yyssa;		/*  refer to the stacks thru separate pointers */
  YYSTYPE *yyvs = yyvsa;	/*  to allow yyoverflow to reallocate them elsewhere */

#ifdef YYLSP_NEEDED
  YYLTYPE yylsa[YYINITDEPTH];	/*  the location stack			*/
  YYLTYPE *yyls = yylsa;
  YYLTYPE *yylsp;

#define YYPOPSTACK   (yyvsp--, yyssp--, yylsp--)
#else
#define YYPOPSTACK   (yyvsp--, yyssp--)
#endif

  int yystacksize = YYINITDEPTH;

#ifdef YYPURE
  int yychar;
  YYSTYPE yylval;
  int yynerrs;
#ifdef YYLSP_NEEDED
  YYLTYPE yylloc;
#endif
#endif

  YYSTYPE yyval;		/*  the variable used to return		*/
				/*  semantic values from the action	*/
				/*  routines				*/

  int yylen;

#if YYDEBUG != 0
  if (yydebug)
    fprintf(stderr, "Starting parse\n");
#endif

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY;		/* Cause a token to be read.  */

  /* Initialize stack pointers.
     Waste one element of value and location stack
     so that they stay on the same level as the state stack.
     The wasted elements are never initialized.  */

  yyssp = yyss - 1;
  yyvsp = yyvs;
#ifdef YYLSP_NEEDED
  yylsp = yyls;
#endif

/* Push a new state, which is found in  yystate  .  */
/* In all cases, when you get here, the value and location stacks
   have just been pushed. so pushing a state here evens the stacks.  */
yynewstate:

  *++yyssp = yystate;

  if (yyssp >= yyss + yystacksize - 1)
    {
      /* Give user a chance to reallocate the stack */
      /* Use copies of these so that the &'s don't force the real ones into memory. */
      YYSTYPE *yyvs1 = yyvs;
      short *yyss1 = yyss;
#ifdef YYLSP_NEEDED
      YYLTYPE *yyls1 = yyls;
#endif

      /* Get the current used size of the three stacks, in elements.  */
      int size = yyssp - yyss + 1;

#ifdef yyoverflow
      /* Each stack pointer address is followed by the size of
	 the data in use in that stack, in bytes.  */
#ifdef YYLSP_NEEDED
      /* This used to be a conditional around just the two extra args,
	 but that might be undefined if yyoverflow is a macro.  */
      yyoverflow("parser stack overflow",
		 &yyss1, size * sizeof (*yyssp),
		 &yyvs1, size * sizeof (*yyvsp),
		 &yyls1, size * sizeof (*yylsp),
		 &yystacksize);
#else
      yyoverflow("parser stack overflow",
		 &yyss1, size * sizeof (*yyssp),
		 &yyvs1, size * sizeof (*yyvsp),
		 &yystacksize);
#endif

      yyss = yyss1; yyvs = yyvs1;
#ifdef YYLSP_NEEDED
      yyls = yyls1;
#endif
#else /* no yyoverflow */
      /* Extend the stack our own way.  */
      if (yystacksize >= YYMAXDEPTH)
	{
	  yyerror("parser stack overflow");
	  return 2;
	}
      yystacksize *= 2;
      if (yystacksize > YYMAXDEPTH)
	yystacksize = YYMAXDEPTH;
      yyss = (short *) alloca (yystacksize * sizeof (*yyssp));
      __yy_memcpy ((char *)yyss1, (char *)yyss, size * sizeof (*yyssp));
      yyvs = (YYSTYPE *) alloca (yystacksize * sizeof (*yyvsp));
      __yy_memcpy ((char *)yyvs1, (char *)yyvs, size * sizeof (*yyvsp));
#ifdef YYLSP_NEEDED
      yyls = (YYLTYPE *) alloca (yystacksize * sizeof (*yylsp));
      __yy_memcpy ((char *)yyls1, (char *)yyls, size * sizeof (*yylsp));
#endif
#endif /* no yyoverflow */

      yyssp = yyss + size - 1;
      yyvsp = yyvs + size - 1;
#ifdef YYLSP_NEEDED
      yylsp = yyls + size - 1;
#endif

#if YYDEBUG != 0
      if (yydebug)
	fprintf(stderr, "Stack size increased to %d\n", yystacksize);
#endif

      if (yyssp >= yyss + yystacksize - 1)
	YYABORT;
    }

#if YYDEBUG != 0
  if (yydebug)
    fprintf(stderr, "Entering state %d\n", yystate);
#endif

  goto yybackup;
 yybackup:

/* Do appropriate processing given the current state.  */
/* Read a lookahead token if we need one and don't already have one.  */
/* yyresume: */

  /* First try to decide what to do without reference to lookahead token.  */

  yyn = yypact[yystate];
  if (yyn == YYFLAG)
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* yychar is either YYEMPTY or YYEOF
     or a valid token in external form.  */

  if (yychar == YYEMPTY)
    {
#if YYDEBUG != 0
      if (yydebug)
	fprintf(stderr, "Reading a token: ");
#endif
      yychar = YYLEX;
    }

  /* Convert token to internal form (in yychar1) for indexing tables with */

  if (yychar <= 0)		/* This means end of input. */
    {
      yychar1 = 0;
      yychar = YYEOF;		/* Don't call YYLEX any more */

#if YYDEBUG != 0
      if (yydebug)
	fprintf(stderr, "Now at end of input.\n");
#endif
    }
  else
    {
      yychar1 = YYTRANSLATE(yychar);

#if YYDEBUG != 0
      if (yydebug)
	{
	  fprintf (stderr, "Next token is %d (%s", yychar, yytname[yychar1]);
	  /* Give the individual parser a way to print the precise meaning
	     of a token, for further debugging info.  */
#ifdef YYPRINT
	  YYPRINT (stderr, yychar, yylval);
#endif
	  fprintf (stderr, ")\n");
	}
#endif
    }

  yyn += yychar1;
  if (yyn < 0 || yyn > YYLAST || yycheck[yyn] != yychar1)
    goto yydefault;

  yyn = yytable[yyn];

  /* yyn is what to do for this token type in this state.
     Negative => reduce, -yyn is rule number.
     Positive => shift, yyn is new state.
       New state is final state => don't bother to shift,
       just return success.
     0, or most negative number => error.  */

  if (yyn < 0)
    {
      if (yyn == YYFLAG)
	goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }
  else if (yyn == 0)
    goto yyerrlab;

  if (yyn == YYFINAL)
    YYACCEPT;

  /* Shift the lookahead token.  */

#if YYDEBUG != 0
  if (yydebug)
    fprintf(stderr, "Shifting token %d (%s), ", yychar, yytname[yychar1]);
#endif

  /* Discard the token being shifted unless it is eof.  */
  if (yychar != YYEOF)
    yychar = YYEMPTY;

  *++yyvsp = yylval;
#ifdef YYLSP_NEEDED
  *++yylsp = yylloc;
#endif

  /* count tokens shifted since error; after three, turn off error status.  */
  if (yyerrstatus) yyerrstatus--;

  yystate = yyn;
  goto yynewstate;

/* Do the default action for the current state.  */
yydefault:

  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;

/* Do a reduction.  yyn is the number of a rule to reduce with.  */
yyreduce:
  yylen = yyr2[yyn];
  if (yylen > 0)
    yyval = yyvsp[1-yylen]; /* implement default value of the action */

#if YYDEBUG != 0
  if (yydebug)
    {
      int i;

      fprintf (stderr, "Reducing via rule %d (line %d), ",
	       yyn, yyrline[yyn]);

      /* Print the symbols being reduced, and their result.  */
      for (i = yyprhs[yyn]; yyrhs[i] > 0; i++)
	fprintf (stderr, "%s ", yytname[yyrhs[i]]);
      fprintf (stderr, " -> %s\n", yytname[yyr1[yyn]]);
    }
#endif


  switch (yyn) {

case 4:
#line 126 "./config/m68k-parse.y"
{
		  op->mode = DREG;
		  op->reg = yyvsp[0].reg;
		;
    break;}
case 5:
#line 131 "./config/m68k-parse.y"
{
		  op->mode = AREG;
		  op->reg = yyvsp[0].reg;
		;
    break;}
case 6:
#line 136 "./config/m68k-parse.y"
{
		  op->mode = FPREG;
		  op->reg = yyvsp[0].reg;
		;
    break;}
case 7:
#line 141 "./config/m68k-parse.y"
{
		  op->mode = CONTROL;
		  op->reg = yyvsp[0].reg;
		;
    break;}
case 8:
#line 146 "./config/m68k-parse.y"
{
		  op->mode = CONTROL;
		  op->reg = yyvsp[0].reg;
		;
    break;}
case 9:
#line 151 "./config/m68k-parse.y"
{
		  op->mode = ABSL;
		  op->disp = yyvsp[0].exp;
		;
    break;}
case 10:
#line 156 "./config/m68k-parse.y"
{
		  op->mode = IMMED;
		  op->disp = yyvsp[0].exp;
		;
    break;}
case 11:
#line 161 "./config/m68k-parse.y"
{
		  op->mode = IMMED;
		  op->disp = yyvsp[0].exp;
		;
    break;}
case 12:
#line 166 "./config/m68k-parse.y"
{
		  op->mode = REGLST;
		  op->mask = yyvsp[0].mask;
		;
    break;}
case 13:
#line 179 "./config/m68k-parse.y"
{
		  op->mode = AINDR;
		  op->reg = yyvsp[-1].reg;
		;
    break;}
case 14:
#line 184 "./config/m68k-parse.y"
{
		  op->mode = AINC;
		  op->reg = yyvsp[-2].reg;
		;
    break;}
case 15:
#line 189 "./config/m68k-parse.y"
{
		  op->mode = ADEC;
		  op->reg = yyvsp[-1].reg;
		;
    break;}
case 16:
#line 194 "./config/m68k-parse.y"
{
		  op->reg = yyvsp[-1].reg;
		  op->disp = yyvsp[-3].exp;
		  if ((yyvsp[-1].reg >= ZADDR0 && yyvsp[-1].reg <= ZADDR7)
		      || yyvsp[-1].reg == ZPC)
		    op->mode = BASE;
		  else
		    op->mode = DISP;
		;
    break;}
case 17:
#line 204 "./config/m68k-parse.y"
{
		  op->reg = yyvsp[-1].reg;
		  op->disp = yyvsp[-3].exp;
		  if ((yyvsp[-1].reg >= ZADDR0 && yyvsp[-1].reg <= ZADDR7)
		      || yyvsp[-1].reg == ZPC)
		    op->mode = BASE;
		  else
		    op->mode = DISP;
		;
    break;}
case 18:
#line 214 "./config/m68k-parse.y"
{
		  op->mode = DISP;
		  op->reg = yyvsp[-1].reg;
		;
    break;}
case 19:
#line 219 "./config/m68k-parse.y"
{
		  op->mode = BASE;
		  op->reg = yyvsp[-1].reg;
		;
    break;}
case 20:
#line 224 "./config/m68k-parse.y"
{
		  op->mode = BASE;
		  op->reg = yyvsp[-1].reg;
		;
    break;}
case 21:
#line 229 "./config/m68k-parse.y"
{
		  op->mode = BASE;
		  op->reg = yyvsp[-3].reg;
		  op->disp = yyvsp[-5].exp;
		  op->index = yyvsp[-1].indexreg;
		;
    break;}
case 22:
#line 236 "./config/m68k-parse.y"
{
		  if (yyvsp[-3].reg == PC || yyvsp[-3].reg == ZPC)
		    yyerror ("syntax error");
		  op->mode = BASE;
		  op->reg = yyvsp[-1].reg;
		  op->disp = yyvsp[-5].exp;
		  op->index.reg = yyvsp[-3].reg;
		  op->index.size = SIZE_UNSPEC;
		  op->index.scale = 1;
		;
    break;}
case 23:
#line 247 "./config/m68k-parse.y"
{
		  op->mode = BASE;
		  op->reg = yyvsp[-1].reg;
		  op->disp = yyvsp[-4].exp;
		  op->index = yyvsp[-2].indexreg;
		;
    break;}
case 24:
#line 254 "./config/m68k-parse.y"
{
		  op->mode = BASE;
		  op->reg = yyvsp[-3].reg;
		  op->disp = yyvsp[-5].exp;
		  op->index = yyvsp[-1].indexreg;
		;
    break;}
case 25:
#line 261 "./config/m68k-parse.y"
{
		  op->mode = BASE;
		  op->reg = yyvsp[-3].reg;
		  op->index = yyvsp[-1].indexreg;
		;
    break;}
case 26:
#line 267 "./config/m68k-parse.y"
{
		  if (yyvsp[-3].reg == PC || yyvsp[-3].reg == ZPC)
		    yyerror ("syntax error");
		  op->mode = BASE;
		  op->reg = yyvsp[-1].reg;
		  op->disp = yyvsp[-5].exp;
		  op->index.reg = yyvsp[-3].reg;
		  op->index.size = SIZE_UNSPEC;
		  op->index.scale = 1;
		;
    break;}
case 27:
#line 278 "./config/m68k-parse.y"
{
		  if (yyvsp[-3].reg == PC || yyvsp[-3].reg == ZPC)
		    yyerror ("syntax error");
		  op->mode = BASE;
		  op->reg = yyvsp[-1].reg;
		  op->index.reg = yyvsp[-3].reg;
		  op->index.size = SIZE_UNSPEC;
		  op->index.scale = 1;
		;
    break;}
case 28:
#line 288 "./config/m68k-parse.y"
{
		  op->mode = BASE;
		  op->reg = yyvsp[-1].reg;
		  op->disp = yyvsp[-4].exp;
		  op->index = yyvsp[-2].indexreg;
		;
    break;}
case 29:
#line 295 "./config/m68k-parse.y"
{
		  op->mode = BASE;
		  op->reg = yyvsp[-1].reg;
		  op->index = yyvsp[-2].indexreg;
		;
    break;}
case 30:
#line 301 "./config/m68k-parse.y"
{
		  op->mode = POST;
		  op->reg = yyvsp[-5].reg;
		  op->disp = yyvsp[-6].exp;
		  op->index = yyvsp[-2].indexreg;
		  op->odisp = yyvsp[-1].exp;
		;
    break;}
case 31:
#line 309 "./config/m68k-parse.y"
{
		  op->mode = POST;
		  op->reg = yyvsp[-3].reg;
		  op->disp = yyvsp[-4].exp;
		  op->odisp = yyvsp[-1].exp;
		;
    break;}
case 32:
#line 316 "./config/m68k-parse.y"
{
		  op->mode = POST;
		  op->reg = yyvsp[-5].reg;
		  op->index = yyvsp[-2].indexreg;
		  op->odisp = yyvsp[-1].exp;
		;
    break;}
case 33:
#line 323 "./config/m68k-parse.y"
{
		  op->mode = POST;
		  op->reg = yyvsp[-3].reg;
		  op->odisp = yyvsp[-1].exp;
		;
    break;}
case 34:
#line 329 "./config/m68k-parse.y"
{
		  op->mode = PRE;
		  op->reg = yyvsp[-5].reg;
		  op->disp = yyvsp[-7].exp;
		  op->index = yyvsp[-3].indexreg;
		  op->odisp = yyvsp[-1].exp;
		;
    break;}
case 35:
#line 337 "./config/m68k-parse.y"
{
		  op->mode = PRE;
		  op->reg = yyvsp[-5].reg;
		  op->index = yyvsp[-3].indexreg;
		  op->odisp = yyvsp[-1].exp;
		;
    break;}
case 36:
#line 344 "./config/m68k-parse.y"
{
		  if (yyvsp[-5].reg == PC || yyvsp[-5].reg == ZPC)
		    yyerror ("syntax error");
		  op->mode = PRE;
		  op->reg = yyvsp[-3].reg;
		  op->disp = yyvsp[-7].exp;
		  op->index.reg = yyvsp[-5].reg;
		  op->index.size = SIZE_UNSPEC;
		  op->index.scale = 1;
		  op->odisp = yyvsp[-1].exp;
		;
    break;}
case 37:
#line 356 "./config/m68k-parse.y"
{
		  if (yyvsp[-5].reg == PC || yyvsp[-5].reg == ZPC)
		    yyerror ("syntax error");
		  op->mode = PRE;
		  op->reg = yyvsp[-3].reg;
		  op->index.reg = yyvsp[-5].reg;
		  op->index.size = SIZE_UNSPEC;
		  op->index.scale = 1;
		  op->odisp = yyvsp[-1].exp;
		;
    break;}
case 38:
#line 367 "./config/m68k-parse.y"
{
		  op->mode = PRE;
		  op->reg = yyvsp[-3].reg;
		  op->disp = yyvsp[-5].exp;
		  op->index = yyvsp[-4].indexreg;
		  op->odisp = yyvsp[-1].exp;
		;
    break;}
case 39:
#line 380 "./config/m68k-parse.y"
{
		  /* We use optzapc to avoid a shift/reduce conflict.  */
		  if (yyvsp[-1].reg < ADDR0 || yyvsp[-1].reg > ADDR7)
		    yyerror ("syntax error");
		  op->mode = AINDR;
		  op->reg = yyvsp[-1].reg;
		;
    break;}
case 40:
#line 388 "./config/m68k-parse.y"
{
		  /* We use optzapc to avoid a shift/reduce conflict.  */
		  if (yyvsp[-2].reg < ADDR0 || yyvsp[-2].reg > ADDR7)
		    yyerror ("syntax error");
		  op->mode = AINC;
		  op->reg = yyvsp[-2].reg;
		;
    break;}
case 41:
#line 396 "./config/m68k-parse.y"
{
		  /* We use optzapc to avoid a shift/reduce conflict.  */
		  if (yyvsp[-2].reg < ADDR0 || yyvsp[-2].reg > ADDR7)
		    yyerror ("syntax error");
		  op->mode = ADEC;
		  op->reg = yyvsp[-2].reg;
		;
    break;}
case 42:
#line 404 "./config/m68k-parse.y"
{
		  op->reg = yyvsp[-4].reg;
		  op->disp = yyvsp[-1].exp;
		  if ((yyvsp[-4].reg >= ZADDR0 && yyvsp[-4].reg <= ZADDR7)
		      || yyvsp[-4].reg == ZPC)
		    op->mode = BASE;
		  else
		    op->mode = DISP;
		;
    break;}
case 43:
#line 414 "./config/m68k-parse.y"
{
		  op->mode = BASE;
		  op->reg = yyvsp[-5].reg;
		  op->disp = yyvsp[-2].exp;
		  op->index = yyvsp[-1].indexreg;
		;
    break;}
case 44:
#line 421 "./config/m68k-parse.y"
{
		  op->mode = POST;
		  op->reg = yyvsp[-9].reg;
		  op->disp = yyvsp[-6].exp;
		  op->index = yyvsp[-1].indexreg;
		  op->odisp = yyvsp[-2].exp;
		;
    break;}
case 45:
#line 429 "./config/m68k-parse.y"
{
		  op->mode = POST;
		  op->reg = yyvsp[-8].reg;
		  op->disp = yyvsp[-5].exp;
		  op->odisp = yyvsp[-1].exp;
		;
    break;}
case 46:
#line 436 "./config/m68k-parse.y"
{
		  op->mode = PRE;
		  op->reg = yyvsp[-9].reg;
		  op->disp = yyvsp[-6].exp;
		  op->index = yyvsp[-5].indexreg;
		  op->odisp = yyvsp[-1].exp;
		;
    break;}
case 48:
#line 451 "./config/m68k-parse.y"
{
		  yyval.indexreg.reg = yyvsp[0].reg;
		  yyval.indexreg.size = SIZE_UNSPEC;
		  yyval.indexreg.scale = 1;
		;
    break;}
case 50:
#line 465 "./config/m68k-parse.y"
{
		  yyval.indexreg.reg = yyvsp[0].reg;
		  yyval.indexreg.size = SIZE_UNSPEC;
		  yyval.indexreg.scale = 1;
		;
    break;}
case 61:
#line 508 "./config/m68k-parse.y"
{
		  yyval.reg = ZADDR0;
		;
    break;}
case 65:
#line 525 "./config/m68k-parse.y"
{
		  yyval.reg = ZADDR0;
		;
    break;}
case 66:
#line 529 "./config/m68k-parse.y"
{
		  yyval.reg = yyvsp[0].reg;
		;
    break;}
case 67:
#line 538 "./config/m68k-parse.y"
{
		  yyval.exp.exp.X_op = O_absent;
		  yyval.exp.size = SIZE_UNSPEC;
		;
    break;}
case 68:
#line 543 "./config/m68k-parse.y"
{
		  yyval.exp = yyvsp[0].exp;
		;
    break;}
case 69:
#line 552 "./config/m68k-parse.y"
{
		  yyval.exp.exp.X_op = O_absent;
		  yyval.exp.size = SIZE_UNSPEC;
		;
    break;}
case 70:
#line 557 "./config/m68k-parse.y"
{
		  yyval.exp = yyvsp[-1].exp;
		;
    break;}
case 72:
#line 567 "./config/m68k-parse.y"
{
		  yyval.mask = yyvsp[-2].mask | yyvsp[0].mask;
		;
    break;}
case 73:
#line 571 "./config/m68k-parse.y"
{
		  yyval.mask = (1 << yyvsp[-2].onereg) | yyvsp[0].mask;
		;
    break;}
case 74:
#line 583 "./config/m68k-parse.y"
{
		  yyval.mask = 1 << yyvsp[0].onereg;
		;
    break;}
case 76:
#line 588 "./config/m68k-parse.y"
{
		  yyval.mask = yyvsp[-2].mask | yyvsp[0].mask;
		;
    break;}
case 77:
#line 592 "./config/m68k-parse.y"
{
		  yyval.mask = (1 << yyvsp[-2].onereg) | yyvsp[0].mask;
		;
    break;}
case 78:
#line 599 "./config/m68k-parse.y"
{
		  yyval.mask = (1 << (yyvsp[0].onereg + 1)) - 1 - ((1 << yyvsp[-2].onereg) - 1);
		;
    break;}
case 79:
#line 606 "./config/m68k-parse.y"
{
		  yyval.onereg = yyvsp[0].reg - DATA0;
		;
    break;}
case 80:
#line 610 "./config/m68k-parse.y"
{
		  yyval.onereg = yyvsp[0].reg - ADDR0 + 8;
		;
    break;}
case 81:
#line 614 "./config/m68k-parse.y"
{
		  yyval.onereg = yyvsp[0].reg - FP0 + 16;
		;
    break;}
case 82:
#line 618 "./config/m68k-parse.y"
{
		  if (yyvsp[0].reg == FPI)
		    yyval.onereg = 24;
		  else if (yyvsp[0].reg == FPS)
		    yyval.onereg = 25;
		  else
		    yyval.onereg = 26;
		;
    break;}
}
   /* the action file gets copied in in place of this dollarsign */
#line 487 "/usr/unsupported/share/bison.simple"

  yyvsp -= yylen;
  yyssp -= yylen;
#ifdef YYLSP_NEEDED
  yylsp -= yylen;
#endif

#if YYDEBUG != 0
  if (yydebug)
    {
      short *ssp1 = yyss - 1;
      fprintf (stderr, "state stack now");
      while (ssp1 != yyssp)
	fprintf (stderr, " %d", *++ssp1);
      fprintf (stderr, "\n");
    }
#endif

  *++yyvsp = yyval;

#ifdef YYLSP_NEEDED
  yylsp++;
  if (yylen == 0)
    {
      yylsp->first_line = yylloc.first_line;
      yylsp->first_column = yylloc.first_column;
      yylsp->last_line = (yylsp-1)->last_line;
      yylsp->last_column = (yylsp-1)->last_column;
      yylsp->text = 0;
    }
  else
    {
      yylsp->last_line = (yylsp+yylen-1)->last_line;
      yylsp->last_column = (yylsp+yylen-1)->last_column;
    }
#endif

  /* Now "shift" the result of the reduction.
     Determine what state that goes to,
     based on the state we popped back to
     and the rule number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTBASE] + *yyssp;
  if (yystate >= 0 && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTBASE];

  goto yynewstate;

yyerrlab:   /* here on detecting error */

  if (! yyerrstatus)
    /* If not already recovering from an error, report this error.  */
    {
      ++yynerrs;

#ifdef YYERROR_VERBOSE
      yyn = yypact[yystate];

      if (yyn > YYFLAG && yyn < YYLAST)
	{
	  int size = 0;
	  char *msg;
	  int x, count;

	  count = 0;
	  /* Start X at -yyn if nec to avoid negative indexes in yycheck.  */
	  for (x = (yyn < 0 ? -yyn : 0);
	       x < (sizeof(yytname) / sizeof(char *)); x++)
	    if (yycheck[x + yyn] == x)
	      size += strlen(yytname[x]) + 15, count++;
	  msg = (char *) malloc(size + 15);
	  if (msg != 0)
	    {
	      strcpy(msg, "parse error");

	      if (count < 5)
		{
		  count = 0;
		  for (x = (yyn < 0 ? -yyn : 0);
		       x < (sizeof(yytname) / sizeof(char *)); x++)
		    if (yycheck[x + yyn] == x)
		      {
			strcat(msg, count == 0 ? ", expecting `" : " or `");
			strcat(msg, yytname[x]);
			strcat(msg, "'");
			count++;
		      }
		}
	      yyerror(msg);
	      free(msg);
	    }
	  else
	    yyerror ("parse error; also virtual memory exceeded");
	}
      else
#endif /* YYERROR_VERBOSE */
	yyerror("parse error");
    }

  goto yyerrlab1;
yyerrlab1:   /* here on error raised explicitly by an action */

  if (yyerrstatus == 3)
    {
      /* if just tried and failed to reuse lookahead token after an error, discard it.  */

      /* return failure if at end of input */
      if (yychar == YYEOF)
	YYABORT;

#if YYDEBUG != 0
      if (yydebug)
	fprintf(stderr, "Discarding token %d (%s).\n", yychar, yytname[yychar1]);
#endif

      yychar = YYEMPTY;
    }

  /* Else will try to reuse lookahead token
     after shifting the error token.  */

  yyerrstatus = 3;		/* Each real token shifted decrements this */

  goto yyerrhandle;

yyerrdefault:  /* current state does not do anything special for the error token. */

#if 0
  /* This is wrong; only states that explicitly want error tokens
     should shift them.  */
  yyn = yydefact[yystate];  /* If its default is to accept any token, ok.  Otherwise pop it.*/
  if (yyn) goto yydefault;
#endif

yyerrpop:   /* pop the current state because it cannot handle the error token */

  if (yyssp == yyss) YYABORT;
  yyvsp--;
  yystate = *--yyssp;
#ifdef YYLSP_NEEDED
  yylsp--;
#endif

#if YYDEBUG != 0
  if (yydebug)
    {
      short *ssp1 = yyss - 1;
      fprintf (stderr, "Error: state stack now");
      while (ssp1 != yyssp)
	fprintf (stderr, " %d", *++ssp1);
      fprintf (stderr, "\n");
    }
#endif

yyerrhandle:

  yyn = yypact[yystate];
  if (yyn == YYFLAG)
    goto yyerrdefault;

  yyn += YYTERROR;
  if (yyn < 0 || yyn > YYLAST || yycheck[yyn] != YYTERROR)
    goto yyerrdefault;

  yyn = yytable[yyn];
  if (yyn < 0)
    {
      if (yyn == YYFLAG)
	goto yyerrpop;
      yyn = -yyn;
      goto yyreduce;
    }
  else if (yyn == 0)
    goto yyerrpop;

  if (yyn == YYFINAL)
    YYACCEPT;

#if YYDEBUG != 0
  if (yydebug)
    fprintf(stderr, "Shifting error token, ");
#endif

  *++yyvsp = yylval;
#ifdef YYLSP_NEEDED
  *++yylsp = yylloc;
#endif

  yystate = yyn;
  goto yynewstate;
}
#line 628 "./config/m68k-parse.y"


/* The string to parse is stored here, and modified by yylex.  */

static char *str;

/* The original string pointer.  */

static char *strorig;

/* If *CCP could be a register, return the register number and advance
   *CCP.  Otherwise don't change *CCP, and return 0.  */

static enum m68k_register
m68k_reg_parse (ccp)
     register char **ccp;
{
  char *start = *ccp;
  char c;
  char *p;
  symbolS *symbolp;

  if (flag_reg_prefix_optional)
    {
      if (*start == REGISTER_PREFIX)
	start++;
      p = start;
    }
  else
    {
      if (*start != REGISTER_PREFIX)
	return 0;
      p = start + 1;
    }

  if (! is_name_beginner (*p))
    return 0;

  p++;
  while (is_part_of_name (*p) && *p != '.' && *p != ':' && *p != '*')
    p++;

  c = *p;
  *p = 0;
  symbolp = symbol_find (start);
  *p = c;

  if (symbolp != NULL && S_GET_SEGMENT (symbolp) == reg_section)
    {
      *ccp = p;
      return S_GET_VALUE (symbolp);
    }

  /* In MRI mode, something like foo.bar can be equated to a register
     name.  */
  while (flag_mri && c == '.')
    {
      ++p;
      while (is_part_of_name (*p) && *p != '.' && *p != ':' && *p != '*')
	p++;
      c = *p;
      *p = '\0';
      symbolp = symbol_find (start);
      *p = c;
      if (symbolp != NULL && S_GET_SEGMENT (symbolp) == reg_section)
	{
	  *ccp = p;
	  return S_GET_VALUE (symbolp);
	}
    }

  return 0;
}

/* The lexer.  */

static int
yylex ()
{
  enum m68k_register reg;
  char *s;
  int parens;
  int c = 0;
  char *hold;

  if (*str == ' ')
    ++str;

  if (*str == '\0')
    return 0;

  /* Various special characters are just returned directly.  */
  switch (*str)
    {
    case '#':
    case '&':
    case ',':
    case ')':
    case '/':
    case '@':
    case '[':
    case ']':
      return *str++;
    case '+':
      /* It so happens that a '+' can only appear at the end of an
         operand.  If it appears anywhere else, it must be a unary
         plus on an expression.  */
      if (str[1] == '\0')
	return *str++;
      break;
    case '-':
      /* A '-' can only appear in -(ar), rn-rn, or ar@-.  If it
         appears anywhere else, it must be a unary minus on an
         expression.  */
      if (str[1] == '\0')
	return *str++;
      s = str + 1;
      if (*s == '(')
	++s;
      if (m68k_reg_parse (&s) != 0)
	return *str++;
      break;
    case '(':
      /* A '(' can only appear in `(reg)', `(expr,...', `([', `@(', or
         `)('.  If it appears anywhere else, it must be starting an
         expression.  */
      if (str[1] == '['
	  || (str > strorig
	      && (str[-1] == '@'
		  || str[-1] == ')')))
	return *str++;
      s = str + 1;
      if (m68k_reg_parse (&s) != 0)
	return *str++;
      /* Check for the case of '(expr,...' by scanning ahead.  If we
         find a comma outside of balanced parentheses, we return '('.
         If we find an unbalanced right parenthesis, then presumably
         the '(' really starts an expression.  */
      parens = 0;
      for (s = str + 1; *s != '\0'; s++)
	{
	  if (*s == '(')
	    ++parens;
	  else if (*s == ')')
	    {
	      if (parens == 0)
		break;
	      --parens;
	    }
	  else if (*s == ',' && parens == 0)
	    {
	      /* A comma can not normally appear in an expression, so
		 this is a case of '(expr,...'.  */
	      return *str++;
	    }
	}
    }

  /* See if it's a register.  */

  reg = m68k_reg_parse (&str);
  if (reg != 0)
    {
      int ret;

      yylval.reg = reg;

      if (reg >= DATA0 && reg <= DATA7)
	ret = DR;
      else if (reg >= ADDR0 && reg <= ADDR7)
	ret = AR;
      else if (reg >= FP0 && reg <= FP7)
	return FPR;
      else if (reg == FPI
	       || reg == FPS
	       || reg == FPC)
	return FPCR;
      else if (reg == PC)
	return LPC;
      else if (reg >= ZDATA0 && reg <= ZDATA7)
	ret = ZDR;
      else if (reg >= ZADDR0 && reg <= ZADDR7)
	ret = ZAR;
      else if (reg == ZPC)
	return LZPC;
      else
	return CREG;

      /* If we get here, we have a data or address register.  We
	 must check for a size or scale; if we find one, we must
	 return INDEXREG.  */

      s = str;

      if (*s != '.' && *s != ':' && *s != '*')
	return ret;

      yylval.indexreg.reg = reg;

      if (*s != '.' && *s != ':')
	yylval.indexreg.size = SIZE_UNSPEC;
      else
	{
	  ++s;
	  switch (*s)
	    {
	    case 'w':
	    case 'W':
	      yylval.indexreg.size = SIZE_WORD;
	      ++s;
	      break;
	    case 'l':
	    case 'L':
	      yylval.indexreg.size = SIZE_LONG;
	      ++s;
	      break;
	    default:
	      yyerror ("illegal size specification");
	      yylval.indexreg.size = SIZE_UNSPEC;
	      break;
	    }
	}

      if (*s != '*' && *s != ':')
	yylval.indexreg.scale = 1;
      else
	{
	  ++s;
	  switch (*s)
	    {
	    case '1':
	    case '2':
	    case '4':
	    case '8':
	      yylval.indexreg.scale = *s - '0';
	      ++s;
	      break;
	    default:
	      yyerror ("illegal scale specification");
	      yylval.indexreg.scale = 1;
	      break;
	    }
	}

      str = s;

      return INDEXREG;
    }

  /* It must be an expression.  Before we call expression, we need to
     look ahead to see if there is a size specification.  We must do
     that first, because otherwise foo.l will be treated as the symbol
     foo.l, rather than as the symbol foo with a long size
     specification.  The grammar requires that all expressions end at
     the end of the operand, or with ',', '(', ']', ')'.  */

  parens = 0;
  for (s = str; *s != '\0'; s++)
    {
      if (*s == '(')
	{
	  if (parens == 0
	      && s > str
	      && (s[-1] == ')' || isalnum ((unsigned char) s[-1])))
	    break;
	  ++parens;
	}
      else if (*s == ')')
	{
	  if (parens == 0)
	    break;
	  --parens;
	}
      else if (parens == 0
	       && (*s == ',' || *s == ']'))
	break;
    }

  yylval.exp.size = SIZE_UNSPEC;
  if (s <= str + 2
      || (s[-2] != '.' && s[-2] != ':'))
    s = NULL;
  else
    {
      switch (s[-1])
	{
	case 's':
	case 'S':
	case 'b':
	case 'B':
	  yylval.exp.size = SIZE_BYTE;
	  break;
	case 'w':
	case 'W':
	  yylval.exp.size = SIZE_WORD;
	  break;
	case 'l':
	case 'L':
	  yylval.exp.size = SIZE_LONG;
	  break;
	default:
	  s = NULL;
	  break;
	}
      if (yylval.exp.size != SIZE_UNSPEC)
	{
	  c = s[-2];
	  s[-2] = '\0';
	}
    }

  hold = input_line_pointer;
  input_line_pointer = str;
  expression (&yylval.exp.exp);
  str = input_line_pointer;
  input_line_pointer = hold;

  if (s != NULL)
    {
      s[-2] = c;
      str = s;
    }

  return EXPR;
}

/* Parse an m68k operand.  This is the only function which is called
   from outside this file.  */

int
m68k_ip_op (s, oparg)
     char *s;
     struct m68k_op *oparg;
{
  memset (oparg, 0, sizeof *oparg);
  oparg->error = NULL;
  oparg->index.reg = ZDATA0;
  oparg->index.scale = 1;
  oparg->disp.exp.X_op = O_absent;
  oparg->odisp.exp.X_op = O_absent;

  str = strorig = s;
  op = oparg;

  return yyparse ();
}

/* The error handler.  */

static void
yyerror (s)
     const char *s;
{
  op->error = s;
}
