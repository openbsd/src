
/*  A Bison parser, made from m68k-parse.y
    by GNU Bison version 1.28  */

#define YYBISON 1  /* Identify Bison output.  */

#define	DR	257
#define	AR	258
#define	FPR	259
#define	FPCR	260
#define	LPC	261
#define	ZAR	262
#define	ZDR	263
#define	LZPC	264
#define	CREG	265
#define	INDEXREG	266
#define	EXPR	267

#line 27 "m68k-parse.y"


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
static int yylex PARAMS ((void));
static void yyerror PARAMS ((const char *));

/* The parser sets fields pointed to by this global variable.  */
static struct m68k_op *op;


#line 93 "m68k-parse.y"
typedef union
{
  struct m68k_indexreg indexreg;
  enum m68k_register reg;
  struct m68k_exp exp;
  unsigned long mask;
  int onereg;
} YYSTYPE;
#include <stdio.h>

#ifndef __cplusplus
#ifndef __STDC__
#define const
#endif
#endif



#define	YYFINAL		173
#define	YYFLAG		-32768
#define	YYNTBASE	25

#define YYTRANSLATE(x) ((unsigned)(x) <= 267 ? yytranslate[x] : 44)

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
     2,     2,     2,     2,     2,     1,     3,     4,     5,     6,
     7,     8,     9,    10,    11,    12,    13
};

#if YYDEBUG != 0
static const short yyprhs[] = {     0,
     0,     2,     4,     6,     8,    10,    12,    14,    16,    18,
    21,    24,    26,    30,    35,    40,    46,    52,    57,    61,
    65,    69,    77,    85,    92,    98,   105,   111,   118,   124,
   130,   135,   145,   153,   162,   169,   180,   189,   200,   209,
   218,   221,   225,   229,   235,   242,   253,   263,   274,   276,
   278,   280,   282,   284,   286,   288,   290,   292,   294,   296,
   298,   300,   302,   303,   305,   307,   309,   310,   313,   314,
   317,   318,   321,   323,   327,   331,   333,   335,   339,   343,
   347,   349,   351,   353
};

static const short yyrhs[] = {    26,
     0,    27,     0,    28,     0,     3,     0,     4,     0,     5,
     0,     6,     0,    11,     0,    13,     0,    14,    13,     0,
    15,    13,     0,    40,     0,    16,     4,    17,     0,    16,
     4,    17,    18,     0,    19,    16,     4,    17,     0,    16,
    13,    20,    34,    17,     0,    16,    34,    20,    13,    17,
     0,    13,    16,    34,    17,     0,    16,     7,    17,     0,
    16,     8,    17,     0,    16,    10,    17,     0,    16,    13,
    20,    34,    20,    29,    17,     0,    16,    13,    20,    34,
    20,    36,    17,     0,    16,    13,    20,    30,    37,    17,
     0,    16,    30,    20,    13,    17,     0,    13,    16,    34,
    20,    29,    17,     0,    16,    34,    20,    29,    17,     0,
    13,    16,    34,    20,    36,    17,     0,    16,    34,    20,
    36,    17,     0,    13,    16,    30,    37,    17,     0,    16,
    30,    37,    17,     0,    16,    21,    13,    37,    22,    20,
    29,    38,    17,     0,    16,    21,    13,    37,    22,    38,
    17,     0,    16,    21,    34,    22,    20,    29,    38,    17,
     0,    16,    21,    34,    22,    38,    17,     0,    16,    21,
    13,    20,    34,    20,    29,    22,    38,    17,     0,    16,
    21,    34,    20,    29,    22,    38,    17,     0,    16,    21,
    13,    20,    34,    20,    36,    22,    38,    17,     0,    16,
    21,    34,    20,    36,    22,    38,    17,     0,    16,    21,
    39,    30,    37,    22,    38,    17,     0,    35,    23,     0,
    35,    23,    18,     0,    35,    23,    19,     0,    35,    23,
    16,    13,    17,     0,    35,    23,    16,    39,    29,    17,
     0,    35,    23,    16,    13,    17,    23,    16,    39,    29,
    17,     0,    35,    23,    16,    13,    17,    23,    16,    13,
    17,     0,    35,    23,    16,    39,    29,    17,    23,    16,
    13,    17,     0,    12,     0,    31,     0,    12,     0,    32,
     0,    32,     0,     4,     0,     8,     0,     3,     0,     9,
     0,     4,     0,     7,     0,    33,     0,    10,     0,     8,
     0,     0,    34,     0,     7,     0,    10,     0,     0,    20,
    34,     0,     0,    20,    13,     0,     0,    13,    20,     0,
    42,     0,    42,    24,    41,     0,    43,    24,    41,     0,
    43,     0,    42,     0,    42,    24,    41,     0,    43,    24,
    41,     0,    43,    19,    43,     0,     3,     0,     4,     0,
     5,     0,     6,     0
};

#endif

#if YYDEBUG != 0
static const short yyrline[] = { 0,
   116,   118,   119,   124,   130,   135,   140,   145,   150,   155,
   160,   165,   177,   183,   188,   193,   203,   213,   223,   228,
   233,   238,   245,   256,   263,   269,   276,   282,   293,   303,
   310,   316,   324,   331,   338,   344,   352,   359,   371,   382,
   394,   403,   411,   419,   429,   436,   444,   451,   464,   466,
   478,   480,   491,   493,   494,   499,   501,   506,   508,   514,
   516,   517,   522,   527,   532,   534,   539,   544,   552,   558,
   566,   572,   580,   582,   586,   597,   602,   603,   607,   613,
   623,   628,   632,   636
};
#endif


#if YYDEBUG != 0 || defined (YYERROR_VERBOSE)

static const char * const yytname[] = {   "$","error","$undefined.","DR","AR",
"FPR","FPCR","LPC","ZAR","ZDR","LZPC","CREG","INDEXREG","EXPR","'#'","'&'","'('",
"')'","'+'","'-'","','","'['","']'","'@'","'/'","operand","generic_operand",
"motorola_operand","mit_operand","zireg","zdireg","zadr","zdr","apc","zapc",
"optzapc","zpc","optczapc","optcexpr","optexprc","reglist","ireglist","reglistpair",
"reglistreg", NULL
};
#endif

static const short yyr1[] = {     0,
    25,    25,    25,    26,    26,    26,    26,    26,    26,    26,
    26,    26,    27,    27,    27,    27,    27,    27,    27,    27,
    27,    27,    27,    27,    27,    27,    27,    27,    27,    27,
    27,    27,    27,    27,    27,    27,    27,    27,    27,    27,
    28,    28,    28,    28,    28,    28,    28,    28,    29,    29,
    30,    30,    31,    31,    31,    32,    32,    33,    33,    34,
    34,    34,    35,    35,    36,    36,    37,    37,    38,    38,
    39,    39,    40,    40,    40,    41,    41,    41,    41,    42,
    43,    43,    43,    43
};

static const short yyr2[] = {     0,
     1,     1,     1,     1,     1,     1,     1,     1,     1,     2,
     2,     1,     3,     4,     4,     5,     5,     4,     3,     3,
     3,     7,     7,     6,     5,     6,     5,     6,     5,     5,
     4,     9,     7,     8,     6,    10,     8,    10,     8,     8,
     2,     3,     3,     5,     6,    10,     9,    10,     1,     1,
     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
     1,     1,     0,     1,     1,     1,     0,     2,     0,     2,
     0,     2,     1,     3,     3,     1,     1,     3,     3,     3,
     1,     1,     1,     1
};

static const short yydefact[] = {    63,
    81,    82,    83,    84,    59,    62,    61,     8,     9,     0,
     0,     0,     0,     1,     2,     3,    60,    64,     0,    12,
    73,     0,     0,    10,    11,    56,    58,    59,    62,    57,
    61,    51,     0,    71,    67,    52,     0,     0,    41,     0,
     0,     0,    58,    67,     0,    13,    19,    20,    21,     0,
    67,     0,     0,     0,     0,     0,     0,    71,    42,    43,
    81,    82,    83,    84,    74,    77,    76,    80,    75,     0,
     0,    18,     0,    14,    67,     0,    72,     0,     0,    69,
    67,     0,    68,    31,    54,    65,    55,    66,    49,     0,
     0,    50,    53,     0,    15,     0,     0,     0,     0,    30,
     0,     0,     0,    16,     0,    68,    69,     0,     0,     0,
     0,     0,    25,    17,    27,    29,    44,    72,     0,    78,
    79,    26,    28,    24,     0,     0,     0,     0,     0,    69,
    69,    70,    69,    35,    69,     0,    45,    22,    23,     0,
     0,    69,    33,     0,     0,     0,     0,     0,    71,     0,
    69,    69,     0,    37,    39,    34,    40,     0,     0,     0,
     0,     0,    32,    47,     0,     0,    36,    38,    46,    48,
     0,     0,     0
};

static const short yydefgoto[] = {   171,
    14,    15,    16,    91,    35,    92,    93,    17,    83,    19,
    94,    55,   111,    53,    20,    65,    66,    67
};

static const short yypact[] = {    89,
    10,    11,    19,    23,-32768,-32768,-32768,-32768,    13,    -4,
    22,    57,    36,-32768,-32768,-32768,-32768,-32768,    18,-32768,
    33,    -2,   114,-32768,-32768,-32768,    46,    62,    66,-32768,
    67,-32768,    68,   131,    69,-32768,    70,   105,   147,   156,
   156,   156,-32768,    94,    25,   101,-32768,-32768,-32768,   114,
   100,    53,     9,   138,   108,   103,   112,   117,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,   119,    12,-32768,-32768,    64,
   130,-32768,   124,-32768,    94,    81,    64,   135,   124,   132,
    94,   150,-32768,-32768,-32768,-32768,-32768,-32768,-32768,   151,
   152,-32768,-32768,   153,-32768,   120,   146,   156,   156,-32768,
   154,   155,   157,-32768,   124,   144,   158,   159,   160,    73,
   162,   161,-32768,-32768,-32768,-32768,   163,-32768,   167,-32768,
-32768,-32768,-32768,-32768,   168,   170,   124,    73,   171,   169,
   169,-32768,   169,-32768,   169,   164,   172,-32768,-32768,   174,
   175,   169,-32768,   177,   176,   181,   182,   183,   178,   185,
   169,   169,   186,-32768,-32768,-32768,-32768,   136,   146,   179,
   187,   188,-32768,-32768,   189,   190,-32768,-32768,-32768,-32768,
   173,   194,-32768
};

static const short yypgoto[] = {-32768,
-32768,-32768,-32768,   -72,     1,-32768,    -7,-32768,     3,-32768,
   -65,   -31,  -103,   -58,-32768,   -40,   202,     6
};


#define	YYLAST		207


static const short yytable[] = {    97,
   101,    69,    18,   129,    36,    22,   108,   102,    24,    -4,
    -5,    26,    71,   109,    37,    36,    41,    30,    -6,    78,
    32,    42,    -7,    44,   119,    45,   145,   146,    23,   147,
    41,   148,   125,   -58,    25,    99,    52,   133,   153,   126,
    39,    72,    36,   103,    73,    36,    68,   161,   162,   112,
    75,    38,    76,    81,   140,   142,    40,   120,   121,    26,
    27,   141,    46,    28,    29,    30,    31,    43,    32,    33,
     5,     6,    79,     7,    80,    26,    85,    34,    47,   106,
    87,    30,    48,    49,    89,   132,   165,    50,    54,    56,
   159,     1,     2,     3,     4,     5,     6,   104,     7,     8,
   105,     9,    10,    11,    12,    26,    85,    13,    57,    86,
    87,    30,    88,    70,    89,    90,    26,    43,    74,    77,
     5,     6,    30,     7,    84,    32,    26,    85,    95,    96,
    86,    87,    30,    88,    43,    89,   117,     5,     6,   118,
     7,    43,    98,    51,     5,     6,   100,     7,    26,    85,
    82,   110,   164,    87,    30,   118,   107,    89,    61,    62,
    63,    64,    58,   127,    59,    60,   113,   114,   115,   116,
   122,   123,   172,   124,     0,     0,     0,   128,   134,   149,
   130,   131,   135,   137,   138,   136,   139,   143,   144,   132,
   158,   166,   154,   173,   150,   151,   152,   155,   156,   157,
   160,    21,   163,   167,   168,   169,   170
};

static const short yycheck[] = {    58,
    73,    42,     0,   107,    12,     0,    79,    73,    13,     0,
     0,     3,    44,    79,    12,    23,    19,     9,     0,    51,
    12,    24,     0,    23,    97,    23,   130,   131,    16,   133,
    19,   135,   105,    23,    13,    24,    34,   110,   142,   105,
    23,    17,    50,    75,    20,    53,    41,   151,   152,    81,
    50,    16,    50,    53,   127,   128,    24,    98,    99,     3,
     4,   127,    17,     7,     8,     9,    10,     4,    12,    13,
     7,     8,    20,    10,    22,     3,     4,    21,    17,    77,
     8,     9,    17,    17,    12,    13,   159,    20,    20,    20,
   149,     3,     4,     5,     6,     7,     8,    17,    10,    11,
    20,    13,    14,    15,    16,     3,     4,    19,     4,     7,
     8,     9,    10,    20,    12,    13,     3,     4,    18,    20,
     7,     8,     9,    10,    17,    12,     3,     4,    17,    13,
     7,     8,     9,    10,     4,    12,    17,     7,     8,    20,
    10,     4,    24,    13,     7,     8,    17,    10,     3,     4,
    13,    20,    17,     8,     9,    20,    22,    12,     3,     4,
     5,     6,    16,    20,    18,    19,    17,    17,    17,    17,
    17,    17,     0,    17,    -1,    -1,    -1,    20,    17,    16,
    22,    22,    22,    17,    17,    23,    17,    17,    20,    13,
    13,    13,    17,     0,    23,    22,    22,    17,    17,    17,
    16,     0,    17,    17,    17,    17,    17
};
/* -*-C-*-  Note some compilers choke on comments on `#line' lines.  */
#line 3 "/usr/share/misc/bison.simple"
/* This file comes from bison-1.28.  */

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
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

/* As a special exception, when this file is copied by Bison into a
   Bison output file, you may use that output file without restriction.
   This special exception was added by the Free Software Foundation
   in version 1.24 of Bison.  */

/* This is the parser code that is written into each bison parser
  when the %semantic_parser declaration is not specified in the grammar.
  It was written by Richard Stallman by simplifying the hairy parser
  used when %semantic_parser is specified.  */

#ifndef YYSTACK_USE_ALLOCA
#ifdef alloca
#define YYSTACK_USE_ALLOCA
#else /* alloca not defined */
#ifdef __GNUC__
#define YYSTACK_USE_ALLOCA
#define alloca __builtin_alloca
#else /* not GNU C.  */
#if (!defined (__STDC__) && defined (sparc)) || defined (__sparc__) || defined (__sparc) || defined (__sgi) || (defined (__sun) && defined (__i386))
#define YYSTACK_USE_ALLOCA
#include <alloca.h>
#else /* not sparc */
/* We think this test detects Watcom and Microsoft C.  */
/* This used to test MSDOS, but that is a bad idea
   since that symbol is in the user namespace.  */
#if (defined (_MSDOS) || defined (_MSDOS_)) && !defined (__TURBOC__)
#if 0 /* No need for malloc.h, which pollutes the namespace;
	 instead, just don't use alloca.  */
#include <malloc.h>
#endif
#else /* not MSDOS, or __TURBOC__ */
#if defined(_AIX)
/* I don't know what this was needed for, but it pollutes the namespace.
   So I turned it off.   rms, 2 May 1997.  */
/* #include <malloc.h>  */
 #pragma alloca
#define YYSTACK_USE_ALLOCA
#else /* not MSDOS, or __TURBOC__, or _AIX */
#if 0
#ifdef __hpux /* haible@ilog.fr says this works for HPUX 9.05 and up,
		 and on HPUX 10.  Eventually we can turn this on.  */
#define YYSTACK_USE_ALLOCA
#define alloca __builtin_alloca
#endif /* __hpux */
#endif
#endif /* not _AIX */
#endif /* not MSDOS, or __TURBOC__ */
#endif /* not sparc */
#endif /* not GNU C */
#endif /* alloca not defined */
#endif /* YYSTACK_USE_ALLOCA not defined */

#ifdef YYSTACK_USE_ALLOCA
#define YYSTACK_ALLOC alloca
#else
#define YYSTACK_ALLOC malloc
#endif

/* Note: there must be only one dollar sign in this file.
   It is replaced by the list of actions, each action
   as one case of the switch.  */

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		-2
#define YYEOF		0
#define YYACCEPT	goto yyacceptlab
#define YYABORT 	goto yyabortlab
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

/* Define __yy_memcpy.  Note that the size argument
   should be passed with type unsigned int, because that is what the non-GCC
   definitions require.  With GCC, __builtin_memcpy takes an arg
   of type size_t, but it can handle unsigned int.  */

#if __GNUC__ > 1		/* GNU C and GNU C++ define this.  */
#define __yy_memcpy(TO,FROM,COUNT)	__builtin_memcpy(TO,FROM,COUNT)
#else				/* not GNU C or C++ */
#ifndef __cplusplus

/* This is the most reliable way to avoid incompatibilities
   in available built-in functions on various systems.  */
static void
__yy_memcpy (to, from, count)
     char *to;
     char *from;
     unsigned int count;
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
__yy_memcpy (char *to, char *from, unsigned int count)
{
  register char *t = to;
  register char *f = from;
  register int i = count;

  while (i-- > 0)
    *t++ = *f++;
}

#endif
#endif

#line 217 "/usr/share/misc/bison.simple"

/* The user can define YYPARSE_PARAM as the name of an argument to be passed
   into yyparse.  The argument should have type void *.
   It should actually point to an object.
   Grammar actions can access the variable by casting it
   to the proper pointer type.  */

#ifdef YYPARSE_PARAM
#ifdef __cplusplus
#define YYPARSE_PARAM_ARG void *YYPARSE_PARAM
#define YYPARSE_PARAM_DECL
#else /* not __cplusplus */
#define YYPARSE_PARAM_ARG YYPARSE_PARAM
#define YYPARSE_PARAM_DECL void *YYPARSE_PARAM;
#endif /* not __cplusplus */
#else /* not YYPARSE_PARAM */
#define YYPARSE_PARAM_ARG
#define YYPARSE_PARAM_DECL
#endif /* not YYPARSE_PARAM */

/* Prevent warning if -Wstrict-prototypes.  */
#ifdef __GNUC__
#ifdef YYPARSE_PARAM
int yyparse (void *);
#else
int yyparse (void);
#endif
#endif

int
yyparse(YYPARSE_PARAM_ARG)
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
  int yyfree_stacks = 0;

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
	  if (yyfree_stacks)
	    {
	      free (yyss);
	      free (yyvs);
#ifdef YYLSP_NEEDED
	      free (yyls);
#endif
	    }
	  return 2;
	}
      yystacksize *= 2;
      if (yystacksize > YYMAXDEPTH)
	yystacksize = YYMAXDEPTH;
#ifndef YYSTACK_USE_ALLOCA
      yyfree_stacks = 1;
#endif
      yyss = (short *) YYSTACK_ALLOC (yystacksize * sizeof (*yyssp));
      __yy_memcpy ((char *)yyss, (char *)yyss1,
		   size * (unsigned int) sizeof (*yyssp));
      yyvs = (YYSTYPE *) YYSTACK_ALLOC (yystacksize * sizeof (*yyvsp));
      __yy_memcpy ((char *)yyvs, (char *)yyvs1,
		   size * (unsigned int) sizeof (*yyvsp));
#ifdef YYLSP_NEEDED
      yyls = (YYLTYPE *) YYSTACK_ALLOC (yystacksize * sizeof (*yylsp));
      __yy_memcpy ((char *)yyls, (char *)yyls1,
		   size * (unsigned int) sizeof (*yylsp));
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
#line 126 "m68k-parse.y"
{
		  op->mode = DREG;
		  op->reg = yyvsp[0].reg;
		;
    break;}
case 5:
#line 131 "m68k-parse.y"
{
		  op->mode = AREG;
		  op->reg = yyvsp[0].reg;
		;
    break;}
case 6:
#line 136 "m68k-parse.y"
{
		  op->mode = FPREG;
		  op->reg = yyvsp[0].reg;
		;
    break;}
case 7:
#line 141 "m68k-parse.y"
{
		  op->mode = CONTROL;
		  op->reg = yyvsp[0].reg;
		;
    break;}
case 8:
#line 146 "m68k-parse.y"
{
		  op->mode = CONTROL;
		  op->reg = yyvsp[0].reg;
		;
    break;}
case 9:
#line 151 "m68k-parse.y"
{
		  op->mode = ABSL;
		  op->disp = yyvsp[0].exp;
		;
    break;}
case 10:
#line 156 "m68k-parse.y"
{
		  op->mode = IMMED;
		  op->disp = yyvsp[0].exp;
		;
    break;}
case 11:
#line 161 "m68k-parse.y"
{
		  op->mode = IMMED;
		  op->disp = yyvsp[0].exp;
		;
    break;}
case 12:
#line 166 "m68k-parse.y"
{
		  op->mode = REGLST;
		  op->mask = yyvsp[0].mask;
		;
    break;}
case 13:
#line 179 "m68k-parse.y"
{
		  op->mode = AINDR;
		  op->reg = yyvsp[-1].reg;
		;
    break;}
case 14:
#line 184 "m68k-parse.y"
{
		  op->mode = AINC;
		  op->reg = yyvsp[-2].reg;
		;
    break;}
case 15:
#line 189 "m68k-parse.y"
{
		  op->mode = ADEC;
		  op->reg = yyvsp[-1].reg;
		;
    break;}
case 16:
#line 194 "m68k-parse.y"
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
#line 204 "m68k-parse.y"
{
		  op->reg = yyvsp[-3].reg;
		  op->disp = yyvsp[-1].exp;
		  if ((yyvsp[-3].reg >= ZADDR0 && yyvsp[-3].reg <= ZADDR7)
		      || yyvsp[-3].reg == ZPC)
		    op->mode = BASE;
		  else
		    op->mode = DISP;
		;
    break;}
case 18:
#line 214 "m68k-parse.y"
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
case 19:
#line 224 "m68k-parse.y"
{
		  op->mode = DISP;
		  op->reg = yyvsp[-1].reg;
		;
    break;}
case 20:
#line 229 "m68k-parse.y"
{
		  op->mode = BASE;
		  op->reg = yyvsp[-1].reg;
		;
    break;}
case 21:
#line 234 "m68k-parse.y"
{
		  op->mode = BASE;
		  op->reg = yyvsp[-1].reg;
		;
    break;}
case 22:
#line 239 "m68k-parse.y"
{
		  op->mode = BASE;
		  op->reg = yyvsp[-3].reg;
		  op->disp = yyvsp[-5].exp;
		  op->index = yyvsp[-1].indexreg;
		;
    break;}
case 23:
#line 246 "m68k-parse.y"
{
		  if (yyvsp[-3].reg == PC || yyvsp[-3].reg == ZPC)
		    yyerror (_("syntax error"));
		  op->mode = BASE;
		  op->reg = yyvsp[-1].reg;
		  op->disp = yyvsp[-5].exp;
		  op->index.reg = yyvsp[-3].reg;
		  op->index.size = SIZE_UNSPEC;
		  op->index.scale = 1;
		;
    break;}
case 24:
#line 257 "m68k-parse.y"
{
		  op->mode = BASE;
		  op->reg = yyvsp[-1].reg;
		  op->disp = yyvsp[-4].exp;
		  op->index = yyvsp[-2].indexreg;
		;
    break;}
case 25:
#line 264 "m68k-parse.y"
{
		  op->mode = BASE;
		  op->disp = yyvsp[-1].exp;
		  op->index = yyvsp[-3].indexreg;
		;
    break;}
case 26:
#line 270 "m68k-parse.y"
{
		  op->mode = BASE;
		  op->reg = yyvsp[-3].reg;
		  op->disp = yyvsp[-5].exp;
		  op->index = yyvsp[-1].indexreg;
		;
    break;}
case 27:
#line 277 "m68k-parse.y"
{
		  op->mode = BASE;
		  op->reg = yyvsp[-3].reg;
		  op->index = yyvsp[-1].indexreg;
		;
    break;}
case 28:
#line 283 "m68k-parse.y"
{
		  if (yyvsp[-3].reg == PC || yyvsp[-3].reg == ZPC)
		    yyerror (_("syntax error"));
		  op->mode = BASE;
		  op->reg = yyvsp[-1].reg;
		  op->disp = yyvsp[-5].exp;
		  op->index.reg = yyvsp[-3].reg;
		  op->index.size = SIZE_UNSPEC;
		  op->index.scale = 1;
		;
    break;}
case 29:
#line 294 "m68k-parse.y"
{
		  if (yyvsp[-3].reg == PC || yyvsp[-3].reg == ZPC)
		    yyerror (_("syntax error"));
		  op->mode = BASE;
		  op->reg = yyvsp[-1].reg;
		  op->index.reg = yyvsp[-3].reg;
		  op->index.size = SIZE_UNSPEC;
		  op->index.scale = 1;
		;
    break;}
case 30:
#line 304 "m68k-parse.y"
{
		  op->mode = BASE;
		  op->reg = yyvsp[-1].reg;
		  op->disp = yyvsp[-4].exp;
		  op->index = yyvsp[-2].indexreg;
		;
    break;}
case 31:
#line 311 "m68k-parse.y"
{
		  op->mode = BASE;
		  op->reg = yyvsp[-1].reg;
		  op->index = yyvsp[-2].indexreg;
		;
    break;}
case 32:
#line 317 "m68k-parse.y"
{
		  op->mode = POST;
		  op->reg = yyvsp[-5].reg;
		  op->disp = yyvsp[-6].exp;
		  op->index = yyvsp[-2].indexreg;
		  op->odisp = yyvsp[-1].exp;
		;
    break;}
case 33:
#line 325 "m68k-parse.y"
{
		  op->mode = POST;
		  op->reg = yyvsp[-3].reg;
		  op->disp = yyvsp[-4].exp;
		  op->odisp = yyvsp[-1].exp;
		;
    break;}
case 34:
#line 332 "m68k-parse.y"
{
		  op->mode = POST;
		  op->reg = yyvsp[-5].reg;
		  op->index = yyvsp[-2].indexreg;
		  op->odisp = yyvsp[-1].exp;
		;
    break;}
case 35:
#line 339 "m68k-parse.y"
{
		  op->mode = POST;
		  op->reg = yyvsp[-3].reg;
		  op->odisp = yyvsp[-1].exp;
		;
    break;}
case 36:
#line 345 "m68k-parse.y"
{
		  op->mode = PRE;
		  op->reg = yyvsp[-5].reg;
		  op->disp = yyvsp[-7].exp;
		  op->index = yyvsp[-3].indexreg;
		  op->odisp = yyvsp[-1].exp;
		;
    break;}
case 37:
#line 353 "m68k-parse.y"
{
		  op->mode = PRE;
		  op->reg = yyvsp[-5].reg;
		  op->index = yyvsp[-3].indexreg;
		  op->odisp = yyvsp[-1].exp;
		;
    break;}
case 38:
#line 360 "m68k-parse.y"
{
		  if (yyvsp[-5].reg == PC || yyvsp[-5].reg == ZPC)
		    yyerror (_("syntax error"));
		  op->mode = PRE;
		  op->reg = yyvsp[-3].reg;
		  op->disp = yyvsp[-7].exp;
		  op->index.reg = yyvsp[-5].reg;
		  op->index.size = SIZE_UNSPEC;
		  op->index.scale = 1;
		  op->odisp = yyvsp[-1].exp;
		;
    break;}
case 39:
#line 372 "m68k-parse.y"
{
		  if (yyvsp[-5].reg == PC || yyvsp[-5].reg == ZPC)
		    yyerror (_("syntax error"));
		  op->mode = PRE;
		  op->reg = yyvsp[-3].reg;
		  op->index.reg = yyvsp[-5].reg;
		  op->index.size = SIZE_UNSPEC;
		  op->index.scale = 1;
		  op->odisp = yyvsp[-1].exp;
		;
    break;}
case 40:
#line 383 "m68k-parse.y"
{
		  op->mode = PRE;
		  op->reg = yyvsp[-3].reg;
		  op->disp = yyvsp[-5].exp;
		  op->index = yyvsp[-4].indexreg;
		  op->odisp = yyvsp[-1].exp;
		;
    break;}
case 41:
#line 396 "m68k-parse.y"
{
		  /* We use optzapc to avoid a shift/reduce conflict.  */
		  if (yyvsp[-1].reg < ADDR0 || yyvsp[-1].reg > ADDR7)
		    yyerror (_("syntax error"));
		  op->mode = AINDR;
		  op->reg = yyvsp[-1].reg;
		;
    break;}
case 42:
#line 404 "m68k-parse.y"
{
		  /* We use optzapc to avoid a shift/reduce conflict.  */
		  if (yyvsp[-2].reg < ADDR0 || yyvsp[-2].reg > ADDR7)
		    yyerror (_("syntax error"));
		  op->mode = AINC;
		  op->reg = yyvsp[-2].reg;
		;
    break;}
case 43:
#line 412 "m68k-parse.y"
{
		  /* We use optzapc to avoid a shift/reduce conflict.  */
		  if (yyvsp[-2].reg < ADDR0 || yyvsp[-2].reg > ADDR7)
		    yyerror (_("syntax error"));
		  op->mode = ADEC;
		  op->reg = yyvsp[-2].reg;
		;
    break;}
case 44:
#line 420 "m68k-parse.y"
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
case 45:
#line 430 "m68k-parse.y"
{
		  op->mode = BASE;
		  op->reg = yyvsp[-5].reg;
		  op->disp = yyvsp[-2].exp;
		  op->index = yyvsp[-1].indexreg;
		;
    break;}
case 46:
#line 437 "m68k-parse.y"
{
		  op->mode = POST;
		  op->reg = yyvsp[-9].reg;
		  op->disp = yyvsp[-6].exp;
		  op->index = yyvsp[-1].indexreg;
		  op->odisp = yyvsp[-2].exp;
		;
    break;}
case 47:
#line 445 "m68k-parse.y"
{
		  op->mode = POST;
		  op->reg = yyvsp[-8].reg;
		  op->disp = yyvsp[-5].exp;
		  op->odisp = yyvsp[-1].exp;
		;
    break;}
case 48:
#line 452 "m68k-parse.y"
{
		  op->mode = PRE;
		  op->reg = yyvsp[-9].reg;
		  op->disp = yyvsp[-6].exp;
		  op->index = yyvsp[-5].indexreg;
		  op->odisp = yyvsp[-1].exp;
		;
    break;}
case 50:
#line 467 "m68k-parse.y"
{
		  yyval.indexreg.reg = yyvsp[0].reg;
		  yyval.indexreg.size = SIZE_UNSPEC;
		  yyval.indexreg.scale = 1;
		;
    break;}
case 52:
#line 481 "m68k-parse.y"
{
		  yyval.indexreg.reg = yyvsp[0].reg;
		  yyval.indexreg.size = SIZE_UNSPEC;
		  yyval.indexreg.scale = 1;
		;
    break;}
case 63:
#line 524 "m68k-parse.y"
{
		  yyval.reg = ZADDR0;
		;
    break;}
case 67:
#line 541 "m68k-parse.y"
{
		  yyval.reg = ZADDR0;
		;
    break;}
case 68:
#line 545 "m68k-parse.y"
{
		  yyval.reg = yyvsp[0].reg;
		;
    break;}
case 69:
#line 554 "m68k-parse.y"
{
		  yyval.exp.exp.X_op = O_absent;
		  yyval.exp.size = SIZE_UNSPEC;
		;
    break;}
case 70:
#line 559 "m68k-parse.y"
{
		  yyval.exp = yyvsp[0].exp;
		;
    break;}
case 71:
#line 568 "m68k-parse.y"
{
		  yyval.exp.exp.X_op = O_absent;
		  yyval.exp.size = SIZE_UNSPEC;
		;
    break;}
case 72:
#line 573 "m68k-parse.y"
{
		  yyval.exp = yyvsp[-1].exp;
		;
    break;}
case 74:
#line 583 "m68k-parse.y"
{
		  yyval.mask = yyvsp[-2].mask | yyvsp[0].mask;
		;
    break;}
case 75:
#line 587 "m68k-parse.y"
{
		  yyval.mask = (1 << yyvsp[-2].onereg) | yyvsp[0].mask;
		;
    break;}
case 76:
#line 599 "m68k-parse.y"
{
		  yyval.mask = 1 << yyvsp[0].onereg;
		;
    break;}
case 78:
#line 604 "m68k-parse.y"
{
		  yyval.mask = yyvsp[-2].mask | yyvsp[0].mask;
		;
    break;}
case 79:
#line 608 "m68k-parse.y"
{
		  yyval.mask = (1 << yyvsp[-2].onereg) | yyvsp[0].mask;
		;
    break;}
case 80:
#line 615 "m68k-parse.y"
{
		  if (yyvsp[-2].onereg <= yyvsp[0].onereg)
		    yyval.mask = (1 << (yyvsp[0].onereg + 1)) - 1 - ((1 << yyvsp[-2].onereg) - 1);
		  else
		    yyval.mask = (1 << (yyvsp[-2].onereg + 1)) - 1 - ((1 << yyvsp[0].onereg) - 1);
		;
    break;}
case 81:
#line 625 "m68k-parse.y"
{
		  yyval.onereg = yyvsp[0].reg - DATA0;
		;
    break;}
case 82:
#line 629 "m68k-parse.y"
{
		  yyval.onereg = yyvsp[0].reg - ADDR0 + 8;
		;
    break;}
case 83:
#line 633 "m68k-parse.y"
{
		  yyval.onereg = yyvsp[0].reg - FP0 + 16;
		;
    break;}
case 84:
#line 637 "m68k-parse.y"
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
#line 543 "/usr/share/misc/bison.simple"

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

 yyacceptlab:
  /* YYACCEPT comes here.  */
  if (yyfree_stacks)
    {
      free (yyss);
      free (yyvs);
#ifdef YYLSP_NEEDED
      free (yyls);
#endif
    }
  return 0;

 yyabortlab:
  /* YYABORT comes here.  */
  if (yyfree_stacks)
    {
      free (yyss);
      free (yyvs);
#ifdef YYLSP_NEEDED
      free (yyls);
#endif
    }
  return 1;
}
#line 647 "m68k-parse.y"


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
  int tail = 0;
  char *hold;

  if (*str == ' ')
    ++str;

  if (*str == '\0')
    return 0;

  /* Various special characters are just returned directly.  */
  switch (*str)
    {
    case '@':
      /* In MRI mode, this can be the start of an octal number.  */
      if (flag_mri)
	{
	  if (isdigit (str[1])
	      || ((str[1] == '+' || str[1] == '-')
		  && isdigit (str[2])))
	    break;
	}
      /* Fall through.  */
    case '#':
    case '&':
    case ',':
    case ')':
    case '/':
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
	      yyerror (_("illegal size specification"));
	      yylval.indexreg.size = SIZE_UNSPEC;
	      break;
	    }
	}

      yylval.indexreg.scale = 1;

      if (*s == '*' || *s == ':')
	{
	  expressionS scale;

	  ++s;

	  hold = input_line_pointer;
	  input_line_pointer = s;
	  expression (&scale);
	  s = input_line_pointer;
	  input_line_pointer = hold;

	  if (scale.X_op != O_constant)
	    yyerror (_("scale specification must resolve to a number"));
	  else
	    {
	      switch (scale.X_add_number)
		{
		case 1:
		case 2:
		case 4:
		case 8:
		  yylval.indexreg.scale = scale.X_add_number;
		  break;
		default:
		  yyerror (_("invalid scale value"));
		  break;
		}
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
    tail = 0;
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
	  break;
	}
      if (yylval.exp.size != SIZE_UNSPEC)
	tail = 2;
    }

#ifdef OBJ_ELF
  {
    /* Look for @PLTPC, etc.  */
    char *cp;

    yylval.exp.pic_reloc = pic_none;
    cp = s - tail;
    if (cp - 6 > str && cp[-6] == '@')
      {
	if (strncmp (cp - 6, "@PLTPC", 6) == 0)
	  {
	    yylval.exp.pic_reloc = pic_plt_pcrel;
	    tail += 6;
	  }
	else if (strncmp (cp - 6, "@GOTPC", 6) == 0)
	  {
	    yylval.exp.pic_reloc = pic_got_pcrel;
	    tail += 6;
	  }
      }
    else if (cp - 4 > str && cp[-4] == '@')
      {
	if (strncmp (cp - 4, "@PLT", 4) == 0)
	  {
	    yylval.exp.pic_reloc = pic_plt_off;
	    tail += 4;
	  }
	else if (strncmp (cp - 4, "@GOT", 4) == 0)
	  {
	    yylval.exp.pic_reloc = pic_got_off;
	    tail += 4;
	  }
      }
  }
#endif

  if (tail != 0)
    {
      c = s[-tail];
      s[-tail] = 0;
    }

  hold = input_line_pointer;
  input_line_pointer = str;
  expression (&yylval.exp.exp);
  str = input_line_pointer;
  input_line_pointer = hold;

  if (tail != 0)
    {
      s[-tail] = c;
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
