/* A Bison parser, made from p-exp.y
   by GNU bison 1.35.  */

#define YYBISON 1  /* Identify Bison output.  */

# define	INT	257
# define	FLOAT	258
# define	STRING	259
# define	FIELDNAME	260
# define	NAME	261
# define	TYPENAME	262
# define	NAME_OR_INT	263
# define	STRUCT	264
# define	CLASS	265
# define	SIZEOF	266
# define	COLONCOLON	267
# define	ERROR	268
# define	VARIABLE	269
# define	THIS	270
# define	TRUEKEYWORD	271
# define	FALSEKEYWORD	272
# define	ABOVE_COMMA	273
# define	ASSIGN	274
# define	NOT	275
# define	OR	276
# define	XOR	277
# define	ANDAND	278
# define	NOTEQUAL	279
# define	LEQ	280
# define	GEQ	281
# define	LSH	282
# define	RSH	283
# define	DIV	284
# define	MOD	285
# define	UNARY	286
# define	INCREMENT	287
# define	DECREMENT	288
# define	ARROW	289
# define	BLOCKNAME	290

#line 46 "p-exp.y"


#include "defs.h"
#include "gdb_string.h"
#include <ctype.h>
#include "expression.h"
#include "value.h"
#include "parser-defs.h"
#include "language.h"
#include "p-lang.h"
#include "bfd.h" /* Required by objfiles.h.  */
#include "symfile.h" /* Required by objfiles.h.  */
#include "objfiles.h" /* For have_full_symbols and have_partial_symbols */
#include "block.h"

/* Remap normal yacc parser interface names (yyparse, yylex, yyerror, etc),
   as well as gratuitiously global symbol names, so we can have multiple
   yacc generated parsers in gdb.  Note that these are only the variables
   produced by yacc.  If other parser generators (bison, byacc, etc) produce
   additional global names that conflict at link time, then those parser
   generators need to be fixed instead of adding those names to this list. */

#define	yymaxdepth pascal_maxdepth
#define	yyparse	pascal_parse
#define	yylex	pascal_lex
#define	yyerror	pascal_error
#define	yylval	pascal_lval
#define	yychar	pascal_char
#define	yydebug	pascal_debug
#define	yypact	pascal_pact	
#define	yyr1	pascal_r1			
#define	yyr2	pascal_r2			
#define	yydef	pascal_def		
#define	yychk	pascal_chk		
#define	yypgo	pascal_pgo		
#define	yyact	pascal_act
#define	yyexca	pascal_exca
#define yyerrflag pascal_errflag
#define yynerrs	pascal_nerrs
#define	yyps	pascal_ps
#define	yypv	pascal_pv
#define	yys	pascal_s
#define	yy_yys	pascal_yys
#define	yystate	pascal_state
#define	yytmp	pascal_tmp
#define	yyv	pascal_v
#define	yy_yyv	pascal_yyv
#define	yyval	pascal_val
#define	yylloc	pascal_lloc
#define yyreds	pascal_reds		/* With YYDEBUG defined */
#define yytoks	pascal_toks		/* With YYDEBUG defined */
#define yyname	pascal_name		/* With YYDEBUG defined */
#define yyrule	pascal_rule		/* With YYDEBUG defined */
#define yylhs	pascal_yylhs
#define yylen	pascal_yylen
#define yydefred pascal_yydefred
#define yydgoto	pascal_yydgoto
#define yysindex pascal_yysindex
#define yyrindex pascal_yyrindex
#define yygindex pascal_yygindex
#define yytable	 pascal_yytable
#define yycheck	 pascal_yycheck

#ifndef YYDEBUG
#define	YYDEBUG 1		/* Default to yydebug support */
#endif

#define YYFPRINTF parser_fprintf

int yyparse (void);

static int yylex (void);

void
yyerror (char *);

static char * uptok (char *, int);

#line 129 "p-exp.y"
#ifndef YYSTYPE
typedef union
  {
    LONGEST lval;
    struct {
      LONGEST val;
      struct type *type;
    } typed_val_int;
    struct {
      DOUBLEST dval;
      struct type *type;
    } typed_val_float;
    struct symbol *sym;
    struct type *tval;
    struct stoken sval;
    struct ttype tsym;
    struct symtoken ssym;
    int voidval;
    struct block *bval;
    enum exp_opcode opcode;
    struct internalvar *ivar;

    struct type **tvec;
    int *ivec;
  } yystype;
# define YYSTYPE yystype
# define YYSTYPE_IS_TRIVIAL 1
#endif
#line 154 "p-exp.y"

/* YYSTYPE gets defined by %union */
static int
parse_number (char *, int, int, YYSTYPE *);

static struct type *current_type;

static void push_current_type (void);
static void pop_current_type (void);
static int search_field;
#ifndef YYDEBUG
# define YYDEBUG 0
#endif



#define	YYFINAL		123
#define	YYFLAG		-32768
#define	YYNTBASE	52

/* YYTRANSLATE(YYLEX) -- Bison token number corresponding to YYLEX. */
#define YYTRANSLATE(x) ((unsigned)(x) <= 290 ? yytranslate[x] : 70)

/* YYTRANSLATE[YYLEX] -- Bison token number corresponding to YYLEX. */
static const char yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
      47,    50,    39,    37,    19,    38,    45,    40,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
      28,    26,    29,     2,    36,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,    46,     2,    51,    48,     2,     2,     2,     2,     2,
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
       2,     2,     2,     2,     2,     2,     1,     3,     4,     5,
       6,     7,     8,     9,    10,    11,    12,    13,    14,    15,
      16,    17,    18,    20,    21,    22,    23,    24,    25,    27,
      30,    31,    32,    33,    34,    35,    41,    42,    43,    44,
      49
};

#if YYDEBUG
static const short yyprhs[] =
{
       0,     0,     1,     4,     6,     8,    10,    12,    16,    19,
      22,    25,    28,    33,    38,    39,    44,    45,    51,    52,
      58,    59,    61,    65,    70,    74,    78,    82,    86,    90,
      94,    98,   102,   106,   110,   114,   118,   122,   126,   130,
     134,   138,   142,   146,   148,   150,   152,   154,   156,   158,
     160,   165,   167,   169,   171,   175,   179,   183,   185,   188,
     190,   192,   194,   198,   201,   203,   206,   209,   211,   213,
     215,   217,   219
};
static const short yyrhs[] =
{
      -1,    53,    54,     0,    56,     0,    55,     0,    66,     0,
      57,     0,    56,    19,    57,     0,    57,    48,     0,    36,
      57,     0,    38,    57,     0,    22,    57,     0,    42,    47,
      57,    50,     0,    43,    47,    57,    50,     0,     0,    57,
      45,    58,     6,     0,     0,    57,    46,    59,    56,    51,
       0,     0,    57,    47,    60,    61,    50,     0,     0,    57,
       0,    61,    19,    57,     0,    66,    47,    57,    50,     0,
      47,    56,    50,     0,    57,    39,    57,     0,    57,    40,
      57,     0,    57,    34,    57,     0,    57,    35,    57,     0,
      57,    37,    57,     0,    57,    38,    57,     0,    57,    32,
      57,     0,    57,    33,    57,     0,    57,    26,    57,     0,
      57,    27,    57,     0,    57,    30,    57,     0,    57,    31,
      57,     0,    57,    28,    57,     0,    57,    29,    57,     0,
      57,    25,    57,     0,    57,    24,    57,     0,    57,    23,
      57,     0,    57,    21,    57,     0,    17,     0,    18,     0,
       3,     0,     9,     0,     4,     0,    63,     0,    15,     0,
      12,    47,    66,    50,     0,     5,     0,    16,     0,    49,
       0,    62,    13,    68,     0,    62,    13,    68,     0,    67,
      13,    68,     0,    64,     0,    13,    68,     0,    69,     0,
      67,     0,    65,     0,    67,    13,    39,     0,    48,    67,
       0,     8,     0,    10,    68,     0,    11,    68,     0,     7,
       0,    49,     0,     8,     0,     9,     0,     7,     0,    49,
       0
};

#endif

#if YYDEBUG
/* YYRLINE[YYN] -- source line where rule number YYN was defined. */
static const short yyrline[] =
{
       0,   234,   234,   240,   242,   245,   252,   253,   258,   264,
     270,   274,   278,   282,   286,   286,   299,   299,   326,   326,
     338,   339,   341,   345,   360,   366,   370,   374,   378,   382,
     386,   390,   394,   398,   402,   406,   410,   414,   418,   422,
     426,   430,   434,   438,   444,   450,   457,   468,   475,   478,
     482,   490,   515,   542,   559,   570,   586,   601,   602,   636,
     708,   719,   720,   725,   727,   729,   732,   740,   741,   742,
     743,   746,   747
};
#endif


#if (YYDEBUG) || defined YYERROR_VERBOSE

/* YYTNAME[TOKEN_NUM] -- String name of the token TOKEN_NUM. */
static const char *const yytname[] =
{
  "$", "error", "$undefined.", "INT", "FLOAT", "STRING", "FIELDNAME", 
  "NAME", "TYPENAME", "NAME_OR_INT", "STRUCT", "CLASS", "SIZEOF", 
  "COLONCOLON", "ERROR", "VARIABLE", "THIS", "TRUEKEYWORD", 
  "FALSEKEYWORD", "','", "ABOVE_COMMA", "ASSIGN", "NOT", "OR", "XOR", 
  "ANDAND", "'='", "NOTEQUAL", "'<'", "'>'", "LEQ", "GEQ", "LSH", "RSH", 
  "DIV", "MOD", "'@'", "'+'", "'-'", "'*'", "'/'", "UNARY", "INCREMENT", 
  "DECREMENT", "ARROW", "'.'", "'['", "'('", "'^'", "BLOCKNAME", "')'", 
  "']'", "start", "@1", "normal_start", "type_exp", "exp1", "exp", "@2", 
  "@3", "@4", "arglist", "block", "variable", "qualified_name", "ptype", 
  "type", "typebase", "name", "name_not_typename", 0
};
#endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives. */
static const short yyr1[] =
{
       0,    53,    52,    54,    54,    55,    56,    56,    57,    57,
      57,    57,    57,    57,    58,    57,    59,    57,    60,    57,
      61,    61,    61,    57,    57,    57,    57,    57,    57,    57,
      57,    57,    57,    57,    57,    57,    57,    57,    57,    57,
      57,    57,    57,    57,    57,    57,    57,    57,    57,    57,
      57,    57,    57,    62,    62,    63,    64,    63,    63,    63,
      65,    66,    66,    67,    67,    67,    67,    68,    68,    68,
      68,    69,    69
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN. */
static const short yyr2[] =
{
       0,     0,     2,     1,     1,     1,     1,     3,     2,     2,
       2,     2,     4,     4,     0,     4,     0,     5,     0,     5,
       0,     1,     3,     4,     3,     3,     3,     3,     3,     3,
       3,     3,     3,     3,     3,     3,     3,     3,     3,     3,
       3,     3,     3,     1,     1,     1,     1,     1,     1,     1,
       4,     1,     1,     1,     3,     3,     3,     1,     2,     1,
       1,     1,     3,     2,     1,     2,     2,     1,     1,     1,
       1,     1,     1
};

/* YYDEFACT[S] -- default rule to reduce with in state S when YYTABLE
   doesn't specify something else to do.  Zero means the default is an
   error. */
static const short yydefact[] =
{
       1,     0,    45,    47,    51,    71,    64,    46,     0,     0,
       0,     0,    49,    52,    43,    44,     0,     0,     0,     0,
       0,     0,     0,    72,     2,     4,     3,     6,     0,    48,
      57,    61,     5,    60,    59,    67,    69,    70,    68,    65,
      66,     0,    58,    11,     0,     9,    10,     0,     0,     0,
      63,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      14,    16,    18,     8,     0,     0,     0,     0,    60,     0,
       0,    24,     7,    42,    41,    40,    39,    33,    34,    37,
      38,    35,    36,    31,    32,    27,    28,    29,    30,    25,
      26,     0,     0,    20,    55,     0,    62,    56,    50,     0,
      12,    13,    15,     0,    21,     0,    23,    17,     0,    19,
      22,     0,     0,     0
};

static const short yydefgoto[] =
{
     121,     1,    24,    25,    26,    27,   101,   102,   103,   115,
      28,    29,    30,    31,    44,    33,    39,    34
};

static const short yypact[] =
{
  -32768,    88,-32768,-32768,-32768,-32768,-32768,-32768,     6,     6,
     -42,     6,-32768,-32768,-32768,-32768,    88,    88,    88,   -40,
     -27,    88,     8,    12,-32768,-32768,     2,   201,    20,-32768,
  -32768,-32768,   -19,    21,-32768,-32768,-32768,-32768,-32768,-32768,
  -32768,     8,-32768,   -36,   -19,   -36,   -36,    88,    88,    10,
  -32768,    88,    88,    88,    88,    88,    88,    88,    88,    88,
      88,    88,    88,    88,    88,    88,    88,    88,    88,    88,
  -32768,-32768,-32768,-32768,     6,    88,    15,    13,    49,   117,
     145,-32768,   201,   201,   226,   250,   273,   294,   294,   311,
     311,   311,   311,    28,    28,    28,    28,    68,    68,   -36,
     -36,    64,    88,    88,    59,   173,-32768,-32768,-32768,    38,
  -32768,-32768,-32768,     7,   201,    11,-32768,-32768,    88,-32768,
     201,    78,    79,-32768
};

static const short yypgoto[] =
{
  -32768,-32768,-32768,-32768,   -18,   -16,-32768,-32768,-32768,-32768,
  -32768,-32768,-32768,-32768,    16,   -14,    -5,-32768
};


#define	YYLAST		359


static const short yytable[] =
{
      43,    45,    46,    49,    40,    41,    42,    47,    50,    70,
      71,    72,    73,    35,    36,    37,     6,    32,     8,     9,
      48,    51,    35,    36,    37,   -53,    51,    78,    75,    51,
     118,    79,    80,    74,    76,    82,    83,    84,    85,    86,
      87,    88,    89,    90,    91,    92,    93,    94,    95,    96,
      97,    98,    99,   100,   106,    38,    22,    77,   117,   105,
      81,   119,   109,   108,    38,    66,    67,    68,    69,   104,
     112,   107,   -54,    70,    71,    72,    73,   106,   122,   123,
       0,     0,     0,     0,   113,     0,     0,   114,     0,     0,
       0,     2,     3,     4,     0,     5,     6,     7,     8,     9,
      10,    11,   120,    12,    13,    14,    15,    68,    69,     0,
      16,     0,     0,    70,    71,    72,    73,     0,     0,     0,
       0,     0,     0,     0,    17,     0,    18,     0,     0,     0,
      19,    20,     0,     0,     0,    21,    22,    23,    52,     0,
      53,    54,    55,    56,    57,    58,    59,    60,    61,    62,
      63,    64,    65,     0,    66,    67,    68,    69,     0,     0,
       0,     0,    70,    71,    72,    73,    52,   110,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      65,     0,    66,    67,    68,    69,     0,     0,     0,     0,
      70,    71,    72,    73,    52,   111,    53,    54,    55,    56,
      57,    58,    59,    60,    61,    62,    63,    64,    65,     0,
      66,    67,    68,    69,     0,     0,     0,     0,    70,    71,
      72,    73,    52,   116,    53,    54,    55,    56,    57,    58,
      59,    60,    61,    62,    63,    64,    65,     0,    66,    67,
      68,    69,     0,     0,     0,     0,    70,    71,    72,    73,
      54,    55,    56,    57,    58,    59,    60,    61,    62,    63,
      64,    65,     0,    66,    67,    68,    69,     0,     0,     0,
       0,    70,    71,    72,    73,    55,    56,    57,    58,    59,
      60,    61,    62,    63,    64,    65,     0,    66,    67,    68,
      69,     0,     0,     0,     0,    70,    71,    72,    73,    56,
      57,    58,    59,    60,    61,    62,    63,    64,    65,     0,
      66,    67,    68,    69,     0,     0,     0,     0,    70,    71,
      72,    73,    58,    59,    60,    61,    62,    63,    64,    65,
       0,    66,    67,    68,    69,     0,     0,     0,     0,    70,
      71,    72,    73,    62,    63,    64,    65,     0,    66,    67,
      68,    69,     0,     0,     0,     0,    70,    71,    72,    73
};

static const short yycheck[] =
{
      16,    17,    18,    21,     9,    47,    11,    47,    22,    45,
      46,    47,    48,     7,     8,     9,     8,     1,    10,    11,
      47,    19,     7,     8,     9,    13,    19,    41,    47,    19,
      19,    47,    48,    13,    13,    51,    52,    53,    54,    55,
      56,    57,    58,    59,    60,    61,    62,    63,    64,    65,
      66,    67,    68,    69,    39,    49,    48,    41,    51,    75,
      50,    50,    13,    50,    49,    37,    38,    39,    40,    74,
       6,    76,    13,    45,    46,    47,    48,    39,     0,     0,
      -1,    -1,    -1,    -1,   102,    -1,    -1,   103,    -1,    -1,
      -1,     3,     4,     5,    -1,     7,     8,     9,    10,    11,
      12,    13,   118,    15,    16,    17,    18,    39,    40,    -1,
      22,    -1,    -1,    45,    46,    47,    48,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    36,    -1,    38,    -1,    -1,    -1,
      42,    43,    -1,    -1,    -1,    47,    48,    49,    21,    -1,
      23,    24,    25,    26,    27,    28,    29,    30,    31,    32,
      33,    34,    35,    -1,    37,    38,    39,    40,    -1,    -1,
      -1,    -1,    45,    46,    47,    48,    21,    50,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    -1,    37,    38,    39,    40,    -1,    -1,    -1,    -1,
      45,    46,    47,    48,    21,    50,    23,    24,    25,    26,
      27,    28,    29,    30,    31,    32,    33,    34,    35,    -1,
      37,    38,    39,    40,    -1,    -1,    -1,    -1,    45,    46,
      47,    48,    21,    50,    23,    24,    25,    26,    27,    28,
      29,    30,    31,    32,    33,    34,    35,    -1,    37,    38,
      39,    40,    -1,    -1,    -1,    -1,    45,    46,    47,    48,
      24,    25,    26,    27,    28,    29,    30,    31,    32,    33,
      34,    35,    -1,    37,    38,    39,    40,    -1,    -1,    -1,
      -1,    45,    46,    47,    48,    25,    26,    27,    28,    29,
      30,    31,    32,    33,    34,    35,    -1,    37,    38,    39,
      40,    -1,    -1,    -1,    -1,    45,    46,    47,    48,    26,
      27,    28,    29,    30,    31,    32,    33,    34,    35,    -1,
      37,    38,    39,    40,    -1,    -1,    -1,    -1,    45,    46,
      47,    48,    28,    29,    30,    31,    32,    33,    34,    35,
      -1,    37,    38,    39,    40,    -1,    -1,    -1,    -1,    45,
      46,    47,    48,    32,    33,    34,    35,    -1,    37,    38,
      39,    40,    -1,    -1,    -1,    -1,    45,    46,    47,    48
};
/* -*-C-*-  Note some compilers choke on comments on `#line' lines.  */
#line 3 "/usr/share/bison/bison.simple"

/* Skeleton output parser for bison,

   Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002 Free Software
   Foundation, Inc.

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

/* This is the parser code that is written into each bison parser when
   the %semantic_parser declaration is not specified in the grammar.
   It was written by Richard Stallman by simplifying the hairy parser
   used when %semantic_parser is specified.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

#if ! defined (yyoverflow) || defined (YYERROR_VERBOSE)

/* The parser invokes alloca or xmalloc; define the necessary symbols.  */

# if YYSTACK_USE_ALLOCA
#  define YYSTACK_ALLOC alloca
# else
#  ifndef YYSTACK_USE_ALLOCA
#   if defined (alloca) || defined (_ALLOCA_H)
#    define YYSTACK_ALLOC alloca
#   else
#    ifdef __GNUC__
#     define YYSTACK_ALLOC __builtin_alloca
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's `empty if-body' warning. */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
# else
#  if defined (__STDC__) || defined (__cplusplus)
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   define YYSIZE_T size_t
#  endif
#  define YYSTACK_ALLOC xmalloc
#  define YYSTACK_FREE free
# endif
#endif /* ! defined (yyoverflow) || defined (YYERROR_VERBOSE) */


#if (! defined (yyoverflow) \
     && (! defined (__cplusplus) \
	 || (YYLTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  short yyss;
  YYSTYPE yyvs;
# if YYLSP_NEEDED
  YYLTYPE yyls;
# endif
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAX (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# if YYLSP_NEEDED
#  define YYSTACK_BYTES(N) \
     ((N) * (sizeof (short) + sizeof (YYSTYPE) + sizeof (YYLTYPE))	\
      + 2 * YYSTACK_GAP_MAX)
# else
#  define YYSTACK_BYTES(N) \
     ((N) * (sizeof (short) + sizeof (YYSTYPE))				\
      + YYSTACK_GAP_MAX)
# endif

/* Copy COUNT objects from FROM to TO.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if 1 < __GNUC__
#   define YYCOPY(To, From, Count) \
      __builtin_memcpy (To, From, (Count) * sizeof (*(From)))
#  else
#   define YYCOPY(To, From, Count)		\
      do					\
	{					\
	  register YYSIZE_T yyi;		\
	  for (yyi = 0; yyi < (Count); yyi++)	\
	    (To)[yyi] = (From)[yyi];		\
	}					\
      while (0)
#  endif
# endif

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack)					\
    do									\
      {									\
	YYSIZE_T yynewbytes;						\
	YYCOPY (&yyptr->Stack, Stack, yysize);				\
	Stack = &yyptr->Stack;						\
	yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAX;	\
	yyptr += yynewbytes / sizeof (*yyptr);				\
      }									\
    while (0)

#endif


#if ! defined (YYSIZE_T) && defined (__SIZE_TYPE__)
# define YYSIZE_T __SIZE_TYPE__
#endif
#if ! defined (YYSIZE_T) && defined (size_t)
# define YYSIZE_T size_t
#endif
#if ! defined (YYSIZE_T)
# if defined (__STDC__) || defined (__cplusplus)
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# endif
#endif
#if ! defined (YYSIZE_T)
# define YYSIZE_T unsigned int
#endif

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		-2
#define YYEOF		0
#define YYACCEPT	goto yyacceptlab
#define YYABORT 	goto yyabortlab
#define YYERROR		goto yyerrlab1
/* Like YYERROR except do call yyerror.  This remains here temporarily
   to ease the transition to the new meaning of YYERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  */
#define YYFAIL		goto yyerrlab
#define YYRECOVERING()  (!!yyerrstatus)
#define YYBACKUP(Token, Value)					\
do								\
  if (yychar == YYEMPTY && yylen == 1)				\
    {								\
      yychar = (Token);						\
      yylval = (Value);						\
      yychar1 = YYTRANSLATE (yychar);				\
      YYPOPSTACK;						\
      goto yybackup;						\
    }								\
  else								\
    { 								\
      yyerror ("syntax error: cannot back up");			\
      YYERROR;							\
    }								\
while (0)

#define YYTERROR	1
#define YYERRCODE	256


/* YYLLOC_DEFAULT -- Compute the default location (before the actions
   are run).

   When YYLLOC_DEFAULT is run, CURRENT is set the location of the
   first token.  By default, to implement support for ranges, extend
   its range to the last symbol.  */

#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)       	\
   Current.last_line   = Rhs[N].last_line;	\
   Current.last_column = Rhs[N].last_column;
#endif


/* YYLEX -- calling `yylex' with the right arguments.  */

#if YYPURE
# if YYLSP_NEEDED
#  ifdef YYLEX_PARAM
#   define YYLEX		yylex (&yylval, &yylloc, YYLEX_PARAM)
#  else
#   define YYLEX		yylex (&yylval, &yylloc)
#  endif
# else /* !YYLSP_NEEDED */
#  ifdef YYLEX_PARAM
#   define YYLEX		yylex (&yylval, YYLEX_PARAM)
#  else
#   define YYLEX		yylex (&yylval)
#  endif
# endif /* !YYLSP_NEEDED */
#else /* !YYPURE */
# define YYLEX			yylex ()
#endif /* !YYPURE */


/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)			\
do {						\
  if (yydebug)					\
    YYFPRINTF Args;				\
} while (0)
/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
#endif /* !YYDEBUG */

/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef	YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   SIZE_MAX < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#if YYMAXDEPTH == 0
# undef YYMAXDEPTH
#endif

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif

#ifdef YYERROR_VERBOSE

# ifndef yystrlen
#  if defined (__GLIBC__) && defined (_STRING_H)
#   define yystrlen strlen
#  else
/* Return the length of YYSTR.  */
static YYSIZE_T
#   if defined (__STDC__) || defined (__cplusplus)
yystrlen (const char *yystr)
#   else
yystrlen (yystr)
     const char *yystr;
#   endif
{
  register const char *yys = yystr;

  while (*yys++ != '\0')
    continue;

  return yys - yystr - 1;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined (__GLIBC__) && defined (_STRING_H) && defined (_GNU_SOURCE)
#   define yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
static char *
#   if defined (__STDC__) || defined (__cplusplus)
yystpcpy (char *yydest, const char *yysrc)
#   else
yystpcpy (yydest, yysrc)
     char *yydest;
     const char *yysrc;
#   endif
{
  register char *yyd = yydest;
  register const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif
#endif

#line 315 "/usr/share/bison/bison.simple"


/* The user can define YYPARSE_PARAM as the name of an argument to be passed
   into yyparse.  The argument should have type void *.
   It should actually point to an object.
   Grammar actions can access the variable by casting it
   to the proper pointer type.  */

#ifdef YYPARSE_PARAM
# if defined (__STDC__) || defined (__cplusplus)
#  define YYPARSE_PARAM_ARG void *YYPARSE_PARAM
#  define YYPARSE_PARAM_DECL
# else
#  define YYPARSE_PARAM_ARG YYPARSE_PARAM
#  define YYPARSE_PARAM_DECL void *YYPARSE_PARAM;
# endif
#else /* !YYPARSE_PARAM */
# define YYPARSE_PARAM_ARG
# define YYPARSE_PARAM_DECL
#endif /* !YYPARSE_PARAM */

/* Prevent warning if -Wstrict-prototypes.  */
#ifdef __GNUC__
# ifdef YYPARSE_PARAM
int yyparse (void *);
# else
int yyparse (void);
# endif
#endif

/* YY_DECL_VARIABLES -- depending whether we use a pure parser,
   variables are global, or local to YYPARSE.  */

#define YY_DECL_NON_LSP_VARIABLES			\
/* The lookahead symbol.  */				\
int yychar;						\
							\
/* The semantic value of the lookahead symbol. */	\
YYSTYPE yylval;						\
							\
/* Number of parse errors so far.  */			\
int yynerrs;

#if YYLSP_NEEDED
# define YY_DECL_VARIABLES			\
YY_DECL_NON_LSP_VARIABLES			\
						\
/* Location data for the lookahead symbol.  */	\
YYLTYPE yylloc;
#else
# define YY_DECL_VARIABLES			\
YY_DECL_NON_LSP_VARIABLES
#endif


/* If nonreentrant, generate the variables here. */

#if !YYPURE
YY_DECL_VARIABLES
#endif  /* !YYPURE */

int
yyparse (YYPARSE_PARAM_ARG)
     YYPARSE_PARAM_DECL
{
  /* If reentrant, generate the variables here. */
#if YYPURE
  YY_DECL_VARIABLES
#endif  /* !YYPURE */

  register int yystate;
  register int yyn;
  int yyresult;
  /* Number of tokens to shift before error messages enabled.  */
  int yyerrstatus;
  /* Lookahead token as an internal (translated) token number.  */
  int yychar1 = 0;

  /* Three stacks and their tools:
     `yyss': related to states,
     `yyvs': related to semantic values,
     `yyls': related to locations.

     Refer to the stacks thru separate pointers, to allow yyoverflow
     to xreallocate them elsewhere.  */

  /* The state stack. */
  short	yyssa[YYINITDEPTH];
  short *yyss = yyssa;
  register short *yyssp;

  /* The semantic value stack.  */
  YYSTYPE yyvsa[YYINITDEPTH];
  YYSTYPE *yyvs = yyvsa;
  register YYSTYPE *yyvsp;

#if YYLSP_NEEDED
  /* The location stack.  */
  YYLTYPE yylsa[YYINITDEPTH];
  YYLTYPE *yyls = yylsa;
  YYLTYPE *yylsp;
#endif

#if YYLSP_NEEDED
# define YYPOPSTACK   (yyvsp--, yyssp--, yylsp--)
#else
# define YYPOPSTACK   (yyvsp--, yyssp--)
#endif

  YYSIZE_T yystacksize = YYINITDEPTH;


  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;
#if YYLSP_NEEDED
  YYLTYPE yyloc;
#endif

  /* When reducing, the number of symbols on the RHS of the reduced
     rule. */
  int yylen;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY;		/* Cause a token to be read.  */

  /* Initialize stack pointers.
     Waste one element of value and location stack
     so that they stay on the same level as the state stack.
     The wasted elements are never initialized.  */

  yyssp = yyss;
  yyvsp = yyvs;
#if YYLSP_NEEDED
  yylsp = yyls;
#endif
  goto yysetstate;

/*------------------------------------------------------------.
| yynewstate -- Push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
 yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed. so pushing a state here evens the stacks.
     */
  yyssp++;

 yysetstate:
  *yyssp = yystate;

  if (yyssp >= yyss + yystacksize - 1)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
	/* Give user a chance to xreallocate the stack. Use copies of
	   these so that the &'s don't force the real ones into
	   memory.  */
	YYSTYPE *yyvs1 = yyvs;
	short *yyss1 = yyss;

	/* Each stack pointer address is followed by the size of the
	   data in use in that stack, in bytes.  */
# if YYLSP_NEEDED
	YYLTYPE *yyls1 = yyls;
	/* This used to be a conditional around just the two extra args,
	   but that might be undefined if yyoverflow is a macro.  */
	yyoverflow ("parser stack overflow",
		    &yyss1, yysize * sizeof (*yyssp),
		    &yyvs1, yysize * sizeof (*yyvsp),
		    &yyls1, yysize * sizeof (*yylsp),
		    &yystacksize);
	yyls = yyls1;
# else
	yyoverflow ("parser stack overflow",
		    &yyss1, yysize * sizeof (*yyssp),
		    &yyvs1, yysize * sizeof (*yyvsp),
		    &yystacksize);
# endif
	yyss = yyss1;
	yyvs = yyvs1;
      }
#else /* no yyoverflow */
# ifndef YYSTACK_RELOCATE
      goto yyoverflowlab;
# else
      /* Extend the stack our own way.  */
      if (yystacksize >= YYMAXDEPTH)
	goto yyoverflowlab;
      yystacksize *= 2;
      if (yystacksize > YYMAXDEPTH)
	yystacksize = YYMAXDEPTH;

      {
	short *yyss1 = yyss;
	union yyalloc *yyptr =
	  (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
	if (! yyptr)
	  goto yyoverflowlab;
	YYSTACK_RELOCATE (yyss);
	YYSTACK_RELOCATE (yyvs);
# if YYLSP_NEEDED
	YYSTACK_RELOCATE (yyls);
# endif
# undef YYSTACK_RELOCATE
	if (yyss1 != yyssa)
	  YYSTACK_FREE (yyss1);
      }
# endif
#endif /* no yyoverflow */

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;
#if YYLSP_NEEDED
      yylsp = yyls + yysize - 1;
#endif

      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
		  (unsigned long int) yystacksize));

      if (yyssp >= yyss + yystacksize - 1)
	YYABORT;
    }

  YYDPRINTF ((stderr, "Entering state %d\n", yystate));

  goto yybackup;


/*-----------.
| yybackup.  |
`-----------*/
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
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = YYLEX;
    }

  /* Convert token to internal form (in yychar1) for indexing tables with */

  if (yychar <= 0)		/* This means end of input. */
    {
      yychar1 = 0;
      yychar = YYEOF;		/* Don't call YYLEX any more */

      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yychar1 = YYTRANSLATE (yychar);

#if YYDEBUG
     /* We have to keep this `#if YYDEBUG', since we use variables
	which are defined only if `YYDEBUG' is set.  */
      if (yydebug)
	{
	  YYFPRINTF (stderr, "Next token is %d (%s",
		     yychar, yytname[yychar1]);
	  /* Give the individual parser a way to print the precise
	     meaning of a token, for further debugging info.  */
# ifdef YYPRINT
	  YYPRINT (stderr, yychar, yylval);
# endif
	  YYFPRINTF (stderr, ")\n");
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
  YYDPRINTF ((stderr, "Shifting token %d (%s), ",
	      yychar, yytname[yychar1]));

  /* Discard the token being shifted unless it is eof.  */
  if (yychar != YYEOF)
    yychar = YYEMPTY;

  *++yyvsp = yylval;
#if YYLSP_NEEDED
  *++yylsp = yylloc;
#endif

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  yystate = yyn;
  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- Do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     `$$ = $1'.

     Otherwise, the following line sets YYVAL to the semantic value of
     the lookahead token.  This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];

#if YYLSP_NEEDED
  /* Similarly for the default location.  Let the user run additional
     commands if for instance locations are ranges.  */
  yyloc = yylsp[1-yylen];
  YYLLOC_DEFAULT (yyloc, (yylsp - yylen), yylen);
#endif

#if YYDEBUG
  /* We have to keep this `#if YYDEBUG', since we use variables which
     are defined only if `YYDEBUG' is set.  */
  if (yydebug)
    {
      int yyi;

      YYFPRINTF (stderr, "Reducing via rule %d (line %d), ",
		 yyn, yyrline[yyn]);

      /* Print the symbols being reduced, and their result.  */
      for (yyi = yyprhs[yyn]; yyrhs[yyi] > 0; yyi++)
	YYFPRINTF (stderr, "%s ", yytname[yyrhs[yyi]]);
      YYFPRINTF (stderr, " -> %s\n", yytname[yyr1[yyn]]);
    }
#endif

  switch (yyn) {

case 1:
#line 234 "p-exp.y"
{ current_type = NULL;
		  search_field = 0;
		}
    break;
case 2:
#line 237 "p-exp.y"
{}
    break;
case 5:
#line 246 "p-exp.y"
{ write_exp_elt_opcode(OP_TYPE);
			  write_exp_elt_type(yyvsp[0].tval);
			  write_exp_elt_opcode(OP_TYPE);
			  current_type = yyvsp[0].tval; }
    break;
case 7:
#line 254 "p-exp.y"
{ write_exp_elt_opcode (BINOP_COMMA); }
    break;
case 8:
#line 259 "p-exp.y"
{ write_exp_elt_opcode (UNOP_IND);
			  if (current_type) 
			    current_type = TYPE_TARGET_TYPE (current_type); }
    break;
case 9:
#line 265 "p-exp.y"
{ write_exp_elt_opcode (UNOP_ADDR); 
			  if (current_type)
			    current_type = TYPE_POINTER_TYPE (current_type); }
    break;
case 10:
#line 271 "p-exp.y"
{ write_exp_elt_opcode (UNOP_NEG); }
    break;
case 11:
#line 275 "p-exp.y"
{ write_exp_elt_opcode (UNOP_LOGICAL_NOT); }
    break;
case 12:
#line 279 "p-exp.y"
{ write_exp_elt_opcode (UNOP_PREINCREMENT); }
    break;
case 13:
#line 283 "p-exp.y"
{ write_exp_elt_opcode (UNOP_PREDECREMENT); }
    break;
case 14:
#line 286 "p-exp.y"
{ search_field = 1; }
    break;
case 15:
#line 289 "p-exp.y"
{ write_exp_elt_opcode (STRUCTOP_STRUCT);
			  write_exp_string (yyvsp[0].sval); 
			  write_exp_elt_opcode (STRUCTOP_STRUCT);
			  search_field = 0; 
			  if (current_type)
			    { while (TYPE_CODE (current_type) == TYPE_CODE_PTR)
				current_type = TYPE_TARGET_TYPE (current_type);
			      current_type = lookup_struct_elt_type (
				current_type, yyvsp[0].sval.ptr, 0); };
			 }
    break;
case 16:
#line 301 "p-exp.y"
{ char *arrayname; 
			  int arrayfieldindex;
			  arrayfieldindex = is_pascal_string_type (
				current_type, NULL, NULL,
				NULL, NULL, &arrayname); 
			  if (arrayfieldindex) 
			    {
			      struct stoken stringsval;
			      stringsval.ptr = alloca (strlen (arrayname) + 1);
			      stringsval.length = strlen (arrayname);
			      strcpy (stringsval.ptr, arrayname);
			      current_type = TYPE_FIELD_TYPE (current_type,
				arrayfieldindex - 1); 
			      write_exp_elt_opcode (STRUCTOP_STRUCT);
			      write_exp_string (stringsval); 
			      write_exp_elt_opcode (STRUCTOP_STRUCT);
			    }
			  push_current_type ();  }
    break;
case 17:
#line 320 "p-exp.y"
{ pop_current_type ();
			  write_exp_elt_opcode (BINOP_SUBSCRIPT);
			  if (current_type)
			    current_type = TYPE_TARGET_TYPE (current_type); }
    break;
case 18:
#line 329 "p-exp.y"
{ push_current_type ();
			  start_arglist (); }
    break;
case 19:
#line 332 "p-exp.y"
{ write_exp_elt_opcode (OP_FUNCALL);
			  write_exp_elt_longcst ((LONGEST) end_arglist ());
			  write_exp_elt_opcode (OP_FUNCALL); 
			  pop_current_type (); }
    break;
case 21:
#line 340 "p-exp.y"
{ arglist_len = 1; }
    break;
case 22:
#line 342 "p-exp.y"
{ arglist_len++; }
    break;
case 23:
#line 346 "p-exp.y"
{ if (current_type)
			    {
			      /* Allow automatic dereference of classes.  */
			      if ((TYPE_CODE (current_type) == TYPE_CODE_PTR)
				  && (TYPE_CODE (TYPE_TARGET_TYPE (current_type)) == TYPE_CODE_CLASS)
				  && (TYPE_CODE (yyvsp[-3].tval) == TYPE_CODE_CLASS))
				write_exp_elt_opcode (UNOP_IND);
			    }
			  write_exp_elt_opcode (UNOP_CAST);
			  write_exp_elt_type (yyvsp[-3].tval);
			  write_exp_elt_opcode (UNOP_CAST); 
			  current_type = yyvsp[-3].tval; }
    break;
case 24:
#line 361 "p-exp.y"
{ }
    break;
case 25:
#line 367 "p-exp.y"
{ write_exp_elt_opcode (BINOP_MUL); }
    break;
case 26:
#line 371 "p-exp.y"
{ write_exp_elt_opcode (BINOP_DIV); }
    break;
case 27:
#line 375 "p-exp.y"
{ write_exp_elt_opcode (BINOP_INTDIV); }
    break;
case 28:
#line 379 "p-exp.y"
{ write_exp_elt_opcode (BINOP_REM); }
    break;
case 29:
#line 383 "p-exp.y"
{ write_exp_elt_opcode (BINOP_ADD); }
    break;
case 30:
#line 387 "p-exp.y"
{ write_exp_elt_opcode (BINOP_SUB); }
    break;
case 31:
#line 391 "p-exp.y"
{ write_exp_elt_opcode (BINOP_LSH); }
    break;
case 32:
#line 395 "p-exp.y"
{ write_exp_elt_opcode (BINOP_RSH); }
    break;
case 33:
#line 399 "p-exp.y"
{ write_exp_elt_opcode (BINOP_EQUAL); }
    break;
case 34:
#line 403 "p-exp.y"
{ write_exp_elt_opcode (BINOP_NOTEQUAL); }
    break;
case 35:
#line 407 "p-exp.y"
{ write_exp_elt_opcode (BINOP_LEQ); }
    break;
case 36:
#line 411 "p-exp.y"
{ write_exp_elt_opcode (BINOP_GEQ); }
    break;
case 37:
#line 415 "p-exp.y"
{ write_exp_elt_opcode (BINOP_LESS); }
    break;
case 38:
#line 419 "p-exp.y"
{ write_exp_elt_opcode (BINOP_GTR); }
    break;
case 39:
#line 423 "p-exp.y"
{ write_exp_elt_opcode (BINOP_BITWISE_AND); }
    break;
case 40:
#line 427 "p-exp.y"
{ write_exp_elt_opcode (BINOP_BITWISE_XOR); }
    break;
case 41:
#line 431 "p-exp.y"
{ write_exp_elt_opcode (BINOP_BITWISE_IOR); }
    break;
case 42:
#line 435 "p-exp.y"
{ write_exp_elt_opcode (BINOP_ASSIGN); }
    break;
case 43:
#line 439 "p-exp.y"
{ write_exp_elt_opcode (OP_BOOL);
			  write_exp_elt_longcst ((LONGEST) yyvsp[0].lval);
			  write_exp_elt_opcode (OP_BOOL); }
    break;
case 44:
#line 445 "p-exp.y"
{ write_exp_elt_opcode (OP_BOOL);
			  write_exp_elt_longcst ((LONGEST) yyvsp[0].lval);
			  write_exp_elt_opcode (OP_BOOL); }
    break;
case 45:
#line 451 "p-exp.y"
{ write_exp_elt_opcode (OP_LONG);
			  write_exp_elt_type (yyvsp[0].typed_val_int.type);
			  write_exp_elt_longcst ((LONGEST)(yyvsp[0].typed_val_int.val));
			  write_exp_elt_opcode (OP_LONG); }
    break;
case 46:
#line 458 "p-exp.y"
{ YYSTYPE val;
			  parse_number (yyvsp[0].ssym.stoken.ptr, yyvsp[0].ssym.stoken.length, 0, &val);
			  write_exp_elt_opcode (OP_LONG);
			  write_exp_elt_type (val.typed_val_int.type);
			  write_exp_elt_longcst ((LONGEST)val.typed_val_int.val);
			  write_exp_elt_opcode (OP_LONG);
			}
    break;
case 47:
#line 469 "p-exp.y"
{ write_exp_elt_opcode (OP_DOUBLE);
			  write_exp_elt_type (yyvsp[0].typed_val_float.type);
			  write_exp_elt_dblcst (yyvsp[0].typed_val_float.dval);
			  write_exp_elt_opcode (OP_DOUBLE); }
    break;
case 50:
#line 483 "p-exp.y"
{ write_exp_elt_opcode (OP_LONG);
			  write_exp_elt_type (builtin_type_int);
			  CHECK_TYPEDEF (yyvsp[-1].tval);
			  write_exp_elt_longcst ((LONGEST) TYPE_LENGTH (yyvsp[-1].tval));
			  write_exp_elt_opcode (OP_LONG); }
    break;
case 51:
#line 491 "p-exp.y"
{ /* C strings are converted into array constants with
			     an explicit null byte added at the end.  Thus
			     the array upper bound is the string length.
			     There is no such thing in C as a completely empty
			     string. */
			  char *sp = yyvsp[0].sval.ptr; int count = yyvsp[0].sval.length;
			  while (count-- > 0)
			    {
			      write_exp_elt_opcode (OP_LONG);
			      write_exp_elt_type (builtin_type_char);
			      write_exp_elt_longcst ((LONGEST)(*sp++));
			      write_exp_elt_opcode (OP_LONG);
			    }
			  write_exp_elt_opcode (OP_LONG);
			  write_exp_elt_type (builtin_type_char);
			  write_exp_elt_longcst ((LONGEST)'\0');
			  write_exp_elt_opcode (OP_LONG);
			  write_exp_elt_opcode (OP_ARRAY);
			  write_exp_elt_longcst ((LONGEST) 0);
			  write_exp_elt_longcst ((LONGEST) (yyvsp[0].sval.length));
			  write_exp_elt_opcode (OP_ARRAY); }
    break;
case 52:
#line 516 "p-exp.y"
{ 
			  struct value * this_val;
			  struct type * this_type;
			  write_exp_elt_opcode (OP_THIS);
			  write_exp_elt_opcode (OP_THIS); 
			  /* we need type of this */
			  this_val = value_of_this (0); 
			  if (this_val)
			    this_type = this_val->type;
			  else
			    this_type = NULL;
			  if (this_type)
			    {
			      if (TYPE_CODE (this_type) == TYPE_CODE_PTR)
				{
				  this_type = TYPE_TARGET_TYPE (this_type);
				  write_exp_elt_opcode (UNOP_IND);
				}
			    }
		
			  current_type = this_type;
			}
    break;
case 53:
#line 543 "p-exp.y"
{
			  if (yyvsp[0].ssym.sym != 0)
			      yyval.bval = SYMBOL_BLOCK_VALUE (yyvsp[0].ssym.sym);
			  else
			    {
			      struct symtab *tem =
				  lookup_symtab (copy_name (yyvsp[0].ssym.stoken));
			      if (tem)
				yyval.bval = BLOCKVECTOR_BLOCK (BLOCKVECTOR (tem), STATIC_BLOCK);
			      else
				error ("No file or function \"%s\".",
				       copy_name (yyvsp[0].ssym.stoken));
			    }
			}
    break;
case 54:
#line 560 "p-exp.y"
{ struct symbol *tem
			    = lookup_symbol (copy_name (yyvsp[0].sval), yyvsp[-2].bval,
					     VAR_DOMAIN, (int *) NULL,
					     (struct symtab **) NULL);
			  if (!tem || SYMBOL_CLASS (tem) != LOC_BLOCK)
			    error ("No function \"%s\" in specified context.",
				   copy_name (yyvsp[0].sval));
			  yyval.bval = SYMBOL_BLOCK_VALUE (tem); }
    break;
case 55:
#line 571 "p-exp.y"
{ struct symbol *sym;
			  sym = lookup_symbol (copy_name (yyvsp[0].sval), yyvsp[-2].bval,
					       VAR_DOMAIN, (int *) NULL,
					       (struct symtab **) NULL);
			  if (sym == 0)
			    error ("No symbol \"%s\" in specified context.",
				   copy_name (yyvsp[0].sval));

			  write_exp_elt_opcode (OP_VAR_VALUE);
			  /* block_found is set by lookup_symbol.  */
			  write_exp_elt_block (block_found);
			  write_exp_elt_sym (sym);
			  write_exp_elt_opcode (OP_VAR_VALUE); }
    break;
case 56:
#line 587 "p-exp.y"
{
			  struct type *type = yyvsp[-2].tval;
			  if (TYPE_CODE (type) != TYPE_CODE_STRUCT
			      && TYPE_CODE (type) != TYPE_CODE_UNION)
			    error ("`%s' is not defined as an aggregate type.",
				   TYPE_NAME (type));

			  write_exp_elt_opcode (OP_SCOPE);
			  write_exp_elt_type (type);
			  write_exp_string (yyvsp[0].sval);
			  write_exp_elt_opcode (OP_SCOPE);
			}
    break;
case 58:
#line 603 "p-exp.y"
{
			  char *name = copy_name (yyvsp[0].sval);
			  struct symbol *sym;
			  struct minimal_symbol *msymbol;

			  sym =
			    lookup_symbol (name, (const struct block *) NULL,
					   VAR_DOMAIN, (int *) NULL,
					   (struct symtab **) NULL);
			  if (sym)
			    {
			      write_exp_elt_opcode (OP_VAR_VALUE);
			      write_exp_elt_block (NULL);
			      write_exp_elt_sym (sym);
			      write_exp_elt_opcode (OP_VAR_VALUE);
			      break;
			    }

			  msymbol = lookup_minimal_symbol (name, NULL, NULL);
			  if (msymbol != NULL)
			    {
			      write_exp_msymbol (msymbol,
						 lookup_function_type (builtin_type_int),
						 builtin_type_int);
			    }
			  else
			    if (!have_full_symbols () && !have_partial_symbols ())
			      error ("No symbol table is loaded.  Use the \"file\" command.");
			    else
			      error ("No symbol \"%s\" in current context.", name);
			}
    break;
case 59:
#line 637 "p-exp.y"
{ struct symbol *sym = yyvsp[0].ssym.sym;

			  if (sym)
			    {
			      if (symbol_read_needs_frame (sym))
				{
				  if (innermost_block == 0 ||
				      contained_in (block_found,
						    innermost_block))
				    innermost_block = block_found;
				}

			      write_exp_elt_opcode (OP_VAR_VALUE);
			      /* We want to use the selected frame, not
				 another more inner frame which happens to
				 be in the same block.  */
			      write_exp_elt_block (NULL);
			      write_exp_elt_sym (sym);
			      write_exp_elt_opcode (OP_VAR_VALUE);
			      current_type = sym->type; }
			  else if (yyvsp[0].ssym.is_a_field_of_this)
			    {
			      struct value * this_val;
			      struct type * this_type;
			      /* Object pascal: it hangs off of `this'.  Must
			         not inadvertently convert from a method call
				 to data ref.  */
			      if (innermost_block == 0 ||
				  contained_in (block_found, innermost_block))
				innermost_block = block_found;
			      write_exp_elt_opcode (OP_THIS);
			      write_exp_elt_opcode (OP_THIS);
			      write_exp_elt_opcode (STRUCTOP_PTR);
			      write_exp_string (yyvsp[0].ssym.stoken);
			      write_exp_elt_opcode (STRUCTOP_PTR);
			      /* we need type of this */
			      this_val = value_of_this (0); 
			      if (this_val)
				this_type = this_val->type;
			      else
				this_type = NULL;
			      if (this_type)
				current_type = lookup_struct_elt_type (
				  this_type,
				  copy_name (yyvsp[0].ssym.stoken), 0);
			      else
				current_type = NULL; 
			    }
			  else
			    {
			      struct minimal_symbol *msymbol;
			      char *arg = copy_name (yyvsp[0].ssym.stoken);

			      msymbol =
				lookup_minimal_symbol (arg, NULL, NULL);
			      if (msymbol != NULL)
				{
				  write_exp_msymbol (msymbol,
						     lookup_function_type (builtin_type_int),
						     builtin_type_int);
				}
			      else if (!have_full_symbols () && !have_partial_symbols ())
				error ("No symbol table is loaded.  Use the \"file\" command.");
			      else
				error ("No symbol \"%s\" in current context.",
				       copy_name (yyvsp[0].ssym.stoken));
			    }
			}
    break;
case 62:
#line 721 "p-exp.y"
{ yyval.tval = lookup_member_type (builtin_type_int, yyvsp[-2].tval); }
    break;
case 63:
#line 726 "p-exp.y"
{ yyval.tval = lookup_pointer_type (yyvsp[0].tval); }
    break;
case 64:
#line 728 "p-exp.y"
{ yyval.tval = yyvsp[0].tsym.type; }
    break;
case 65:
#line 730 "p-exp.y"
{ yyval.tval = lookup_struct (copy_name (yyvsp[0].sval),
					      expression_context_block); }
    break;
case 66:
#line 733 "p-exp.y"
{ yyval.tval = lookup_struct (copy_name (yyvsp[0].sval),
					      expression_context_block); }
    break;
case 67:
#line 740 "p-exp.y"
{ yyval.sval = yyvsp[0].ssym.stoken; }
    break;
case 68:
#line 741 "p-exp.y"
{ yyval.sval = yyvsp[0].ssym.stoken; }
    break;
case 69:
#line 742 "p-exp.y"
{ yyval.sval = yyvsp[0].tsym.stoken; }
    break;
case 70:
#line 743 "p-exp.y"
{ yyval.sval = yyvsp[0].ssym.stoken; }
    break;
}

#line 705 "/usr/share/bison/bison.simple"


  yyvsp -= yylen;
  yyssp -= yylen;
#if YYLSP_NEEDED
  yylsp -= yylen;
#endif

#if YYDEBUG
  if (yydebug)
    {
      short *yyssp1 = yyss - 1;
      YYFPRINTF (stderr, "state stack now");
      while (yyssp1 != yyssp)
	YYFPRINTF (stderr, " %d", *++yyssp1);
      YYFPRINTF (stderr, "\n");
    }
#endif

  *++yyvsp = yyval;
#if YYLSP_NEEDED
  *++yylsp = yyloc;
#endif

  /* Now `shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTBASE] + *yyssp;
  if (yystate >= 0 && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTBASE];

  goto yynewstate;


/*------------------------------------.
| yyerrlab -- here on detecting error |
`------------------------------------*/
yyerrlab:
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;

#ifdef YYERROR_VERBOSE
      yyn = yypact[yystate];

      if (yyn > YYFLAG && yyn < YYLAST)
	{
	  YYSIZE_T yysize = 0;
	  char *yymsg;
	  int yyx, yycount;

	  yycount = 0;
	  /* Start YYX at -YYN if negative to avoid negative indexes in
	     YYCHECK.  */
	  for (yyx = yyn < 0 ? -yyn : 0;
	       yyx < (int) (sizeof (yytname) / sizeof (char *)); yyx++)
	    if (yycheck[yyx + yyn] == yyx)
	      yysize += yystrlen (yytname[yyx]) + 15, yycount++;
	  yysize += yystrlen ("parse error, unexpected ") + 1;
	  yysize += yystrlen (yytname[YYTRANSLATE (yychar)]);
	  yymsg = (char *) YYSTACK_ALLOC (yysize);
	  if (yymsg != 0)
	    {
	      char *yyp = yystpcpy (yymsg, "parse error, unexpected ");
	      yyp = yystpcpy (yyp, yytname[YYTRANSLATE (yychar)]);

	      if (yycount < 5)
		{
		  yycount = 0;
		  for (yyx = yyn < 0 ? -yyn : 0;
		       yyx < (int) (sizeof (yytname) / sizeof (char *));
		       yyx++)
		    if (yycheck[yyx + yyn] == yyx)
		      {
			const char *yyq = ! yycount ? ", expecting " : " or ";
			yyp = yystpcpy (yyp, yyq);
			yyp = yystpcpy (yyp, yytname[yyx]);
			yycount++;
		      }
		}
	      yyerror (yymsg);
	      YYSTACK_FREE (yymsg);
	    }
	  else
	    yyerror ("parse error; also virtual memory exhausted");
	}
      else
#endif /* defined (YYERROR_VERBOSE) */
	yyerror ("parse error");
    }
  goto yyerrlab1;


/*--------------------------------------------------.
| yyerrlab1 -- error raised explicitly by an action |
`--------------------------------------------------*/
yyerrlab1:
  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
	 error, discard it.  */

      /* return failure if at end of input */
      if (yychar == YYEOF)
	YYABORT;
      YYDPRINTF ((stderr, "Discarding token %d (%s).\n",
		  yychar, yytname[yychar1]));
      yychar = YYEMPTY;
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */

  yyerrstatus = 3;		/* Each real token shifted decrements this */

  goto yyerrhandle;


/*-------------------------------------------------------------------.
| yyerrdefault -- current state does not do anything special for the |
| error token.                                                       |
`-------------------------------------------------------------------*/
yyerrdefault:
#if 0
  /* This is wrong; only states that explicitly want error tokens
     should shift them.  */

  /* If its default is to accept any token, ok.  Otherwise pop it.  */
  yyn = yydefact[yystate];
  if (yyn)
    goto yydefault;
#endif


/*---------------------------------------------------------------.
| yyerrpop -- pop the current state because it cannot handle the |
| error token                                                    |
`---------------------------------------------------------------*/
yyerrpop:
  if (yyssp == yyss)
    YYABORT;
  yyvsp--;
  yystate = *--yyssp;
#if YYLSP_NEEDED
  yylsp--;
#endif

#if YYDEBUG
  if (yydebug)
    {
      short *yyssp1 = yyss - 1;
      YYFPRINTF (stderr, "Error: state stack now");
      while (yyssp1 != yyssp)
	YYFPRINTF (stderr, " %d", *++yyssp1);
      YYFPRINTF (stderr, "\n");
    }
#endif

/*--------------.
| yyerrhandle.  |
`--------------*/
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

  YYDPRINTF ((stderr, "Shifting error token, "));

  *++yyvsp = yylval;
#if YYLSP_NEEDED
  *++yylsp = yylloc;
#endif

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturn;

/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturn;

/*---------------------------------------------.
| yyoverflowab -- parser overflow comes here.  |
`---------------------------------------------*/
yyoverflowlab:
  yyerror ("parser stack overflow");
  yyresult = 2;
  /* Fall through.  */

yyreturn:
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
  return yyresult;
}
#line 757 "p-exp.y"


/* Take care of parsing a number (anything that starts with a digit).
   Set yylval and return the token type; update lexptr.
   LEN is the number of characters in it.  */

/*** Needs some error checking for the float case ***/

static int
parse_number (p, len, parsed_float, putithere)
     char *p;
     int len;
     int parsed_float;
     YYSTYPE *putithere;
{
  /* FIXME: Shouldn't these be unsigned?  We don't deal with negative values
     here, and we do kind of silly things like cast to unsigned.  */
  LONGEST n = 0;
  LONGEST prevn = 0;
  ULONGEST un;

  int i = 0;
  int c;
  int base = input_radix;
  int unsigned_p = 0;

  /* Number of "L" suffixes encountered.  */
  int long_p = 0;

  /* We have found a "L" or "U" suffix.  */
  int found_suffix = 0;

  ULONGEST high_bit;
  struct type *signed_type;
  struct type *unsigned_type;

  if (parsed_float)
    {
      /* It's a float since it contains a point or an exponent.  */
      char c;
      int num = 0;	/* number of tokens scanned by scanf */
      char saved_char = p[len];

      p[len] = 0;	/* null-terminate the token */
      if (sizeof (putithere->typed_val_float.dval) <= sizeof (float))
	num = sscanf (p, "%g%c", (float *) &putithere->typed_val_float.dval,&c);
      else if (sizeof (putithere->typed_val_float.dval) <= sizeof (double))
	num = sscanf (p, "%lg%c", (double *) &putithere->typed_val_float.dval,&c);
      else
	{
#ifdef SCANF_HAS_LONG_DOUBLE
	  num = sscanf (p, "%Lg%c", &putithere->typed_val_float.dval,&c);
#else
	  /* Scan it into a double, then assign it to the long double.
	     This at least wins with values representable in the range
	     of doubles. */
	  double temp;
	  num = sscanf (p, "%lg%c", &temp,&c);
	  putithere->typed_val_float.dval = temp;
#endif
	}
      p[len] = saved_char;	/* restore the input stream */
      if (num != 1) 		/* check scanf found ONLY a float ... */
	return ERROR;
      /* See if it has `f' or `l' suffix (float or long double).  */

      c = tolower (p[len - 1]);

      if (c == 'f')
	putithere->typed_val_float.type = builtin_type_float;
      else if (c == 'l')
	putithere->typed_val_float.type = builtin_type_long_double;
      else if (isdigit (c) || c == '.')
	putithere->typed_val_float.type = builtin_type_double;
      else
	return ERROR;

      return FLOAT;
    }

  /* Handle base-switching prefixes 0x, 0t, 0d, 0 */
  if (p[0] == '0')
    switch (p[1])
      {
      case 'x':
      case 'X':
	if (len >= 3)
	  {
	    p += 2;
	    base = 16;
	    len -= 2;
	  }
	break;

      case 't':
      case 'T':
      case 'd':
      case 'D':
	if (len >= 3)
	  {
	    p += 2;
	    base = 10;
	    len -= 2;
	  }
	break;

      default:
	base = 8;
	break;
      }

  while (len-- > 0)
    {
      c = *p++;
      if (c >= 'A' && c <= 'Z')
	c += 'a' - 'A';
      if (c != 'l' && c != 'u')
	n *= base;
      if (c >= '0' && c <= '9')
	{
	  if (found_suffix)
	    return ERROR;
	  n += i = c - '0';
	}
      else
	{
	  if (base > 10 && c >= 'a' && c <= 'f')
	    {
	      if (found_suffix)
		return ERROR;
	      n += i = c - 'a' + 10;
	    }
	  else if (c == 'l')
	    {
	      ++long_p;
	      found_suffix = 1;
	    }
	  else if (c == 'u')
	    {
	      unsigned_p = 1;
	      found_suffix = 1;
	    }
	  else
	    return ERROR;	/* Char not a digit */
	}
      if (i >= base)
	return ERROR;		/* Invalid digit in this base */

      /* Portably test for overflow (only works for nonzero values, so make
	 a second check for zero).  FIXME: Can't we just make n and prevn
	 unsigned and avoid this?  */
      if (c != 'l' && c != 'u' && (prevn >= n) && n != 0)
	unsigned_p = 1;		/* Try something unsigned */

      /* Portably test for unsigned overflow.
	 FIXME: This check is wrong; for example it doesn't find overflow
	 on 0x123456789 when LONGEST is 32 bits.  */
      if (c != 'l' && c != 'u' && n != 0)
	{	
	  if ((unsigned_p && (ULONGEST) prevn >= (ULONGEST) n))
	    error ("Numeric constant too large.");
	}
      prevn = n;
    }

  /* An integer constant is an int, a long, or a long long.  An L
     suffix forces it to be long; an LL suffix forces it to be long
     long.  If not forced to a larger size, it gets the first type of
     the above that it fits in.  To figure out whether it fits, we
     shift it right and see whether anything remains.  Note that we
     can't shift sizeof (LONGEST) * HOST_CHAR_BIT bits or more in one
     operation, because many compilers will warn about such a shift
     (which always produces a zero result).  Sometimes TARGET_INT_BIT
     or TARGET_LONG_BIT will be that big, sometimes not.  To deal with
     the case where it is we just always shift the value more than
     once, with fewer bits each time.  */

  un = (ULONGEST)n >> 2;
  if (long_p == 0
      && (un >> (TARGET_INT_BIT - 2)) == 0)
    {
      high_bit = ((ULONGEST)1) << (TARGET_INT_BIT-1);

      /* A large decimal (not hex or octal) constant (between INT_MAX
	 and UINT_MAX) is a long or unsigned long, according to ANSI,
	 never an unsigned int, but this code treats it as unsigned
	 int.  This probably should be fixed.  GCC gives a warning on
	 such constants.  */

      unsigned_type = builtin_type_unsigned_int;
      signed_type = builtin_type_int;
    }
  else if (long_p <= 1
	   && (un >> (TARGET_LONG_BIT - 2)) == 0)
    {
      high_bit = ((ULONGEST)1) << (TARGET_LONG_BIT-1);
      unsigned_type = builtin_type_unsigned_long;
      signed_type = builtin_type_long;
    }
  else
    {
      int shift;
      if (sizeof (ULONGEST) * HOST_CHAR_BIT < TARGET_LONG_LONG_BIT)
	/* A long long does not fit in a LONGEST.  */
	shift = (sizeof (ULONGEST) * HOST_CHAR_BIT - 1);
      else
	shift = (TARGET_LONG_LONG_BIT - 1);
      high_bit = (ULONGEST) 1 << shift;
      unsigned_type = builtin_type_unsigned_long_long;
      signed_type = builtin_type_long_long;
    }

   putithere->typed_val_int.val = n;

   /* If the high bit of the worked out type is set then this number
      has to be unsigned. */

   if (unsigned_p || (n & high_bit))
     {
       putithere->typed_val_int.type = unsigned_type;
     }
   else
     {
       putithere->typed_val_int.type = signed_type;
     }

   return INT;
}


struct type_push
{
  struct type *stored;
  struct type_push *next;
};

static struct type_push *tp_top = NULL;

static void
push_current_type (void)
{
  struct type_push *tpnew;
  tpnew = (struct type_push *) xmalloc (sizeof (struct type_push));
  tpnew->next = tp_top;
  tpnew->stored = current_type;
  current_type = NULL;
  tp_top = tpnew; 
}

static void
pop_current_type (void)
{
  struct type_push *tp = tp_top;
  if (tp)
    {
      current_type = tp->stored;
      tp_top = tp->next;
      xfree (tp);
    }
}

struct token
{
  char *operator;
  int token;
  enum exp_opcode opcode;
};

static const struct token tokentab3[] =
  {
    {"shr", RSH, BINOP_END},
    {"shl", LSH, BINOP_END},
    {"and", ANDAND, BINOP_END},
    {"div", DIV, BINOP_END},
    {"not", NOT, BINOP_END},
    {"mod", MOD, BINOP_END},
    {"inc", INCREMENT, BINOP_END},
    {"dec", DECREMENT, BINOP_END},
    {"xor", XOR, BINOP_END}
  };

static const struct token tokentab2[] =
  {
    {"or", OR, BINOP_END},
    {"<>", NOTEQUAL, BINOP_END},
    {"<=", LEQ, BINOP_END},
    {">=", GEQ, BINOP_END},
    {":=", ASSIGN, BINOP_END},
    {"::", COLONCOLON, BINOP_END} };

/* Allocate uppercased var */
/* make an uppercased copy of tokstart */
static char * uptok (tokstart, namelen)
  char *tokstart;
  int namelen;
{
  int i;
  char *uptokstart = (char *)xmalloc(namelen+1);
  for (i = 0;i <= namelen;i++)
    {
      if ((tokstart[i]>='a' && tokstart[i]<='z'))
        uptokstart[i] = tokstart[i]-('a'-'A');
      else
        uptokstart[i] = tokstart[i];
    }
  uptokstart[namelen]='\0';
  return uptokstart;
}
/* Read one token, getting characters through lexptr.  */


static int
yylex ()
{
  int c;
  int namelen;
  unsigned int i;
  char *tokstart;
  char *uptokstart;
  char *tokptr;
  char *p;
  int explen, tempbufindex;
  static char *tempbuf;
  static int tempbufsize;

 retry:

  prev_lexptr = lexptr;

  tokstart = lexptr;
  explen = strlen (lexptr);
  /* See if it is a special token of length 3.  */
  if (explen > 2)
    for (i = 0; i < sizeof (tokentab3) / sizeof (tokentab3[0]); i++)
      if (strncasecmp (tokstart, tokentab3[i].operator, 3) == 0
          && (!isalpha (tokentab3[i].operator[0]) || explen == 3
              || (!isalpha (tokstart[3]) && !isdigit (tokstart[3]) && tokstart[3] != '_')))
        {
          lexptr += 3;
          yylval.opcode = tokentab3[i].opcode;
          return tokentab3[i].token;
        }

  /* See if it is a special token of length 2.  */
  if (explen > 1)
  for (i = 0; i < sizeof (tokentab2) / sizeof (tokentab2[0]); i++)
      if (strncasecmp (tokstart, tokentab2[i].operator, 2) == 0
          && (!isalpha (tokentab2[i].operator[0]) || explen == 2
              || (!isalpha (tokstart[2]) && !isdigit (tokstart[2]) && tokstart[2] != '_')))
        {
          lexptr += 2;
          yylval.opcode = tokentab2[i].opcode;
          return tokentab2[i].token;
        }

  switch (c = *tokstart)
    {
    case 0:
      return 0;

    case ' ':
    case '\t':
    case '\n':
      lexptr++;
      goto retry;

    case '\'':
      /* We either have a character constant ('0' or '\177' for example)
	 or we have a quoted symbol reference ('foo(int,int)' in object pascal
	 for example). */
      lexptr++;
      c = *lexptr++;
      if (c == '\\')
	c = parse_escape (&lexptr);
      else if (c == '\'')
	error ("Empty character constant.");

      yylval.typed_val_int.val = c;
      yylval.typed_val_int.type = builtin_type_char;

      c = *lexptr++;
      if (c != '\'')
	{
	  namelen = skip_quoted (tokstart) - tokstart;
	  if (namelen > 2)
	    {
	      lexptr = tokstart + namelen;
	      if (lexptr[-1] != '\'')
		error ("Unmatched single quote.");
	      namelen -= 2;
              tokstart++;
              uptokstart = uptok(tokstart,namelen);
	      goto tryname;
	    }
	  error ("Invalid character constant.");
	}
      return INT;

    case '(':
      paren_depth++;
      lexptr++;
      return c;

    case ')':
      if (paren_depth == 0)
	return 0;
      paren_depth--;
      lexptr++;
      return c;

    case ',':
      if (comma_terminates && paren_depth == 0)
	return 0;
      lexptr++;
      return c;

    case '.':
      /* Might be a floating point number.  */
      if (lexptr[1] < '0' || lexptr[1] > '9')
	goto symbol;		/* Nope, must be a symbol. */
      /* FALL THRU into number case.  */

    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      {
	/* It's a number.  */
	int got_dot = 0, got_e = 0, toktype;
	char *p = tokstart;
	int hex = input_radix > 10;

	if (c == '0' && (p[1] == 'x' || p[1] == 'X'))
	  {
	    p += 2;
	    hex = 1;
	  }
	else if (c == '0' && (p[1]=='t' || p[1]=='T' || p[1]=='d' || p[1]=='D'))
	  {
	    p += 2;
	    hex = 0;
	  }

	for (;; ++p)
	  {
	    /* This test includes !hex because 'e' is a valid hex digit
	       and thus does not indicate a floating point number when
	       the radix is hex.  */
	    if (!hex && !got_e && (*p == 'e' || *p == 'E'))
	      got_dot = got_e = 1;
	    /* This test does not include !hex, because a '.' always indicates
	       a decimal floating point number regardless of the radix.  */
	    else if (!got_dot && *p == '.')
	      got_dot = 1;
	    else if (got_e && (p[-1] == 'e' || p[-1] == 'E')
		     && (*p == '-' || *p == '+'))
	      /* This is the sign of the exponent, not the end of the
		 number.  */
	      continue;
	    /* We will take any letters or digits.  parse_number will
	       complain if past the radix, or if L or U are not final.  */
	    else if ((*p < '0' || *p > '9')
		     && ((*p < 'a' || *p > 'z')
				  && (*p < 'A' || *p > 'Z')))
	      break;
	  }
	toktype = parse_number (tokstart, p - tokstart, got_dot|got_e, &yylval);
        if (toktype == ERROR)
	  {
	    char *err_copy = (char *) alloca (p - tokstart + 1);

	    memcpy (err_copy, tokstart, p - tokstart);
	    err_copy[p - tokstart] = 0;
	    error ("Invalid number \"%s\".", err_copy);
	  }
	lexptr = p;
	return toktype;
      }

    case '+':
    case '-':
    case '*':
    case '/':
    case '|':
    case '&':
    case '^':
    case '~':
    case '!':
    case '@':
    case '<':
    case '>':
    case '[':
    case ']':
    case '?':
    case ':':
    case '=':
    case '{':
    case '}':
    symbol:
      lexptr++;
      return c;

    case '"':

      /* Build the gdb internal form of the input string in tempbuf,
	 translating any standard C escape forms seen.  Note that the
	 buffer is null byte terminated *only* for the convenience of
	 debugging gdb itself and printing the buffer contents when
	 the buffer contains no embedded nulls.  Gdb does not depend
	 upon the buffer being null byte terminated, it uses the length
	 string instead.  This allows gdb to handle C strings (as well
	 as strings in other languages) with embedded null bytes */

      tokptr = ++tokstart;
      tempbufindex = 0;

      do {
	/* Grow the static temp buffer if necessary, including allocating
	   the first one on demand. */
	if (tempbufindex + 1 >= tempbufsize)
	  {
	    tempbuf = (char *) xrealloc (tempbuf, tempbufsize += 64);
	  }

	switch (*tokptr)
	  {
	  case '\0':
	  case '"':
	    /* Do nothing, loop will terminate. */
	    break;
	  case '\\':
	    tokptr++;
	    c = parse_escape (&tokptr);
	    if (c == -1)
	      {
		continue;
	      }
	    tempbuf[tempbufindex++] = c;
	    break;
	  default:
	    tempbuf[tempbufindex++] = *tokptr++;
	    break;
	  }
      } while ((*tokptr != '"') && (*tokptr != '\0'));
      if (*tokptr++ != '"')
	{
	  error ("Unterminated string in expression.");
	}
      tempbuf[tempbufindex] = '\0';	/* See note above */
      yylval.sval.ptr = tempbuf;
      yylval.sval.length = tempbufindex;
      lexptr = tokptr;
      return (STRING);
    }

  if (!(c == '_' || c == '$'
	|| (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')))
    /* We must have come across a bad character (e.g. ';').  */
    error ("Invalid character '%c' in expression.", c);

  /* It's a name.  See how long it is.  */
  namelen = 0;
  for (c = tokstart[namelen];
       (c == '_' || c == '$' || (c >= '0' && c <= '9')
	|| (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '<');)
    {
      /* Template parameter lists are part of the name.
	 FIXME: This mishandles `print $a<4&&$a>3'.  */
      if (c == '<')
	{
	  int i = namelen;
	  int nesting_level = 1;
	  while (tokstart[++i])
	    {
	      if (tokstart[i] == '<')
		nesting_level++;
	      else if (tokstart[i] == '>')
		{
		  if (--nesting_level == 0)
		    break;
		}
	    }
	  if (tokstart[i] == '>')
	    namelen = i;
	  else
	    break;
	}

      /* do NOT uppercase internals because of registers !!! */
      c = tokstart[++namelen];
    }

  uptokstart = uptok(tokstart,namelen);

  /* The token "if" terminates the expression and is NOT
     removed from the input stream.  */
  if (namelen == 2 && uptokstart[0] == 'I' && uptokstart[1] == 'F')
    {
      return 0;
    }

  lexptr += namelen;

  tryname:

  /* Catch specific keywords.  Should be done with a data structure.  */
  switch (namelen)
    {
    case 6:
      if (DEPRECATED_STREQ (uptokstart, "OBJECT"))
	return CLASS;
      if (DEPRECATED_STREQ (uptokstart, "RECORD"))
	return STRUCT;
      if (DEPRECATED_STREQ (uptokstart, "SIZEOF"))
	return SIZEOF;
      break;
    case 5:
      if (DEPRECATED_STREQ (uptokstart, "CLASS"))
	return CLASS;
      if (DEPRECATED_STREQ (uptokstart, "FALSE"))
	{
          yylval.lval = 0;
          return FALSEKEYWORD;
        }
      break;
    case 4:
      if (DEPRECATED_STREQ (uptokstart, "TRUE"))
	{
          yylval.lval = 1;
  	  return TRUEKEYWORD;
        }
      if (DEPRECATED_STREQ (uptokstart, "SELF"))
        {
          /* here we search for 'this' like
             inserted in FPC stabs debug info */
	  static const char this_name[] = "this";

	  if (lookup_symbol (this_name, expression_context_block,
			     VAR_DOMAIN, (int *) NULL,
			     (struct symtab **) NULL))
	    return THIS;
	}
      break;
    default:
      break;
    }

  yylval.sval.ptr = tokstart;
  yylval.sval.length = namelen;

  if (*tokstart == '$')
    {
      /* $ is the normal prefix for pascal hexadecimal values
        but this conflicts with the GDB use for debugger variables
        so in expression to enter hexadecimal values
        we still need to use C syntax with 0xff  */
      write_dollar_variable (yylval.sval);
      return VARIABLE;
    }

  /* Use token-type BLOCKNAME for symbols that happen to be defined as
     functions or symtabs.  If this is not so, then ...
     Use token-type TYPENAME for symbols that happen to be defined
     currently as names of types; NAME for other symbols.
     The caller is not constrained to care about the distinction.  */
  {
    char *tmp = copy_name (yylval.sval);
    struct symbol *sym;
    int is_a_field_of_this = 0;
    int is_a_field = 0;
    int hextype;


    if (search_field && current_type)
      is_a_field = (lookup_struct_elt_type (current_type, tmp, 1) != NULL);	
    if (is_a_field)
      sym = NULL;
    else
      sym = lookup_symbol (tmp, expression_context_block,
			   VAR_DOMAIN,
			   &is_a_field_of_this,
			   (struct symtab **) NULL);
    /* second chance uppercased (as Free Pascal does).  */
    if (!sym && !is_a_field_of_this && !is_a_field)
      {
       for (i = 0; i <= namelen; i++)
         {
           if ((tmp[i] >= 'a' && tmp[i] <= 'z'))
             tmp[i] -= ('a'-'A');
         }
       if (search_field && current_type)
	 is_a_field = (lookup_struct_elt_type (current_type, tmp, 1) != NULL);	
       if (is_a_field)
	 sym = NULL;
       else
	 sym = lookup_symbol (tmp, expression_context_block,
                        VAR_DOMAIN,
                        &is_a_field_of_this,
                        (struct symtab **) NULL);
       if (sym || is_a_field_of_this || is_a_field)
         for (i = 0; i <= namelen; i++)
           {
             if ((tokstart[i] >= 'a' && tokstart[i] <= 'z'))
               tokstart[i] -= ('a'-'A');
           }
      }
    /* Third chance Capitalized (as GPC does).  */
    if (!sym && !is_a_field_of_this && !is_a_field)
      {
       for (i = 0; i <= namelen; i++)
         {
           if (i == 0)
             {
              if ((tmp[i] >= 'a' && tmp[i] <= 'z'))
                tmp[i] -= ('a'-'A');
             }
           else
           if ((tmp[i] >= 'A' && tmp[i] <= 'Z'))
             tmp[i] -= ('A'-'a');
          }
       if (search_field && current_type)
	 is_a_field = (lookup_struct_elt_type (current_type, tmp, 1) != NULL);	
       if (is_a_field)
	 sym = NULL;
       else
	 sym = lookup_symbol (tmp, expression_context_block,
                         VAR_DOMAIN,
                         &is_a_field_of_this,
                         (struct symtab **) NULL);
       if (sym || is_a_field_of_this || is_a_field)
          for (i = 0; i <= namelen; i++)
            {
              if (i == 0)
                {
                  if ((tokstart[i] >= 'a' && tokstart[i] <= 'z'))
                    tokstart[i] -= ('a'-'A');
                }
              else
                if ((tokstart[i] >= 'A' && tokstart[i] <= 'Z'))
                  tokstart[i] -= ('A'-'a');
            }
      }

    if (is_a_field)
      {
	tempbuf = (char *) xrealloc (tempbuf, namelen + 1);
	strncpy (tempbuf, tokstart, namelen); tempbuf [namelen] = 0;
	yylval.sval.ptr = tempbuf;
	yylval.sval.length = namelen; 
	return FIELDNAME;
      } 
    /* Call lookup_symtab, not lookup_partial_symtab, in case there are
       no psymtabs (coff, xcoff, or some future change to blow away the
       psymtabs once once symbols are read).  */
    if ((sym && SYMBOL_CLASS (sym) == LOC_BLOCK) ||
        lookup_symtab (tmp))
      {
	yylval.ssym.sym = sym;
	yylval.ssym.is_a_field_of_this = is_a_field_of_this;
	return BLOCKNAME;
      }
    if (sym && SYMBOL_CLASS (sym) == LOC_TYPEDEF)
        {
#if 1
	  /* Despite the following flaw, we need to keep this code enabled.
	     Because we can get called from check_stub_method, if we don't
	     handle nested types then it screws many operations in any
	     program which uses nested types.  */
	  /* In "A::x", if x is a member function of A and there happens
	     to be a type (nested or not, since the stabs don't make that
	     distinction) named x, then this code incorrectly thinks we
	     are dealing with nested types rather than a member function.  */

	  char *p;
	  char *namestart;
	  struct symbol *best_sym;

	  /* Look ahead to detect nested types.  This probably should be
	     done in the grammar, but trying seemed to introduce a lot
	     of shift/reduce and reduce/reduce conflicts.  It's possible
	     that it could be done, though.  Or perhaps a non-grammar, but
	     less ad hoc, approach would work well.  */

	  /* Since we do not currently have any way of distinguishing
	     a nested type from a non-nested one (the stabs don't tell
	     us whether a type is nested), we just ignore the
	     containing type.  */

	  p = lexptr;
	  best_sym = sym;
	  while (1)
	    {
	      /* Skip whitespace.  */
	      while (*p == ' ' || *p == '\t' || *p == '\n')
		++p;
	      if (*p == ':' && p[1] == ':')
		{
		  /* Skip the `::'.  */
		  p += 2;
		  /* Skip whitespace.  */
		  while (*p == ' ' || *p == '\t' || *p == '\n')
		    ++p;
		  namestart = p;
		  while (*p == '_' || *p == '$' || (*p >= '0' && *p <= '9')
			 || (*p >= 'a' && *p <= 'z')
			 || (*p >= 'A' && *p <= 'Z'))
		    ++p;
		  if (p != namestart)
		    {
		      struct symbol *cur_sym;
		      /* As big as the whole rest of the expression, which is
			 at least big enough.  */
		      char *ncopy = alloca (strlen (tmp)+strlen (namestart)+3);
		      char *tmp1;

		      tmp1 = ncopy;
		      memcpy (tmp1, tmp, strlen (tmp));
		      tmp1 += strlen (tmp);
		      memcpy (tmp1, "::", 2);
		      tmp1 += 2;
		      memcpy (tmp1, namestart, p - namestart);
		      tmp1[p - namestart] = '\0';
		      cur_sym = lookup_symbol (ncopy, expression_context_block,
					       VAR_DOMAIN, (int *) NULL,
					       (struct symtab **) NULL);
		      if (cur_sym)
			{
			  if (SYMBOL_CLASS (cur_sym) == LOC_TYPEDEF)
			    {
			      best_sym = cur_sym;
			      lexptr = p;
			    }
			  else
			    break;
			}
		      else
			break;
		    }
		  else
		    break;
		}
	      else
		break;
	    }

	  yylval.tsym.type = SYMBOL_TYPE (best_sym);
#else /* not 0 */
	  yylval.tsym.type = SYMBOL_TYPE (sym);
#endif /* not 0 */
	  return TYPENAME;
        }
    if ((yylval.tsym.type = lookup_primitive_typename (tmp)) != 0)
	return TYPENAME;

    /* Input names that aren't symbols but ARE valid hex numbers,
       when the input radix permits them, can be names or numbers
       depending on the parse.  Note we support radixes > 16 here.  */
    if (!sym &&
        ((tokstart[0] >= 'a' && tokstart[0] < 'a' + input_radix - 10) ||
         (tokstart[0] >= 'A' && tokstart[0] < 'A' + input_radix - 10)))
      {
 	YYSTYPE newlval;	/* Its value is ignored.  */
	hextype = parse_number (tokstart, namelen, 0, &newlval);
	if (hextype == INT)
	  {
	    yylval.ssym.sym = sym;
	    yylval.ssym.is_a_field_of_this = is_a_field_of_this;
	    return NAME_OR_INT;
	  }
      }

    free(uptokstart);
    /* Any other kind of symbol */
    yylval.ssym.sym = sym;
    yylval.ssym.is_a_field_of_this = is_a_field_of_this;
    return NAME;
  }
}

void
yyerror (msg)
     char *msg;
{
  if (prev_lexptr)
    lexptr = prev_lexptr;

  error ("A %s in expression, near `%s'.", (msg ? msg : "error"), lexptr);
}
