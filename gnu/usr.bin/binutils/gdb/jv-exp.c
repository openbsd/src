/* A Bison parser, made from jv-exp.y
   by GNU bison 1.35.  */

#define YYBISON 1  /* Identify Bison output.  */

# define	INTEGER_LITERAL	257
# define	FLOATING_POINT_LITERAL	258
# define	IDENTIFIER	259
# define	STRING_LITERAL	260
# define	BOOLEAN_LITERAL	261
# define	TYPENAME	262
# define	NAME_OR_INT	263
# define	ERROR	264
# define	LONG	265
# define	SHORT	266
# define	BYTE	267
# define	INT	268
# define	CHAR	269
# define	BOOLEAN	270
# define	DOUBLE	271
# define	FLOAT	272
# define	VARIABLE	273
# define	ASSIGN_MODIFY	274
# define	SUPER	275
# define	NEW	276
# define	OROR	277
# define	ANDAND	278
# define	EQUAL	279
# define	NOTEQUAL	280
# define	LEQ	281
# define	GEQ	282
# define	LSH	283
# define	RSH	284
# define	INCREMENT	285
# define	DECREMENT	286

#line 38 "jv-exp.y"


#include "defs.h"
#include "gdb_string.h"
#include <ctype.h>
#include "expression.h"
#include "value.h"
#include "parser-defs.h"
#include "language.h"
#include "jv-lang.h"
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

#define	yymaxdepth java_maxdepth
#define	yyparse	java_parse
#define	yylex	java_lex
#define	yyerror	java_error
#define	yylval	java_lval
#define	yychar	java_char
#define	yydebug	java_debug
#define	yypact	java_pact	
#define	yyr1	java_r1			
#define	yyr2	java_r2			
#define	yydef	java_def		
#define	yychk	java_chk		
#define	yypgo	java_pgo		
#define	yyact	java_act		
#define	yyexca	java_exca
#define yyerrflag java_errflag
#define yynerrs	java_nerrs
#define	yyps	java_ps
#define	yypv	java_pv
#define	yys	java_s
#define	yy_yys	java_yys
#define	yystate	java_state
#define	yytmp	java_tmp
#define	yyv	java_v
#define	yy_yyv	java_yyv
#define	yyval	java_val
#define	yylloc	java_lloc
#define yyreds	java_reds		/* With YYDEBUG defined */
#define yytoks	java_toks		/* With YYDEBUG defined */
#define yyname	java_name		/* With YYDEBUG defined */
#define yyrule	java_rule		/* With YYDEBUG defined */
#define yylhs	java_yylhs
#define yylen	java_yylen
#define yydefred java_yydefred
#define yydgoto	java_yydgoto
#define yysindex java_yysindex
#define yyrindex java_yyrindex
#define yygindex java_yygindex
#define yytable	 java_yytable
#define yycheck	 java_yycheck

#ifndef YYDEBUG
#define	YYDEBUG 1		/* Default to yydebug support */
#endif

#define YYFPRINTF parser_fprintf

int yyparse (void);

static int yylex (void);

void yyerror (char *);

static struct type *java_type_from_name (struct stoken);
static void push_expression_name (struct stoken);
static void push_fieldnames (struct stoken);

static struct expression *copy_exp (struct expression *, int);
static void insert_exp (int, struct expression *);


#line 126 "jv-exp.y"
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
    struct block *bval;
    enum exp_opcode opcode;
    struct internalvar *ivar;
    int *ivec;
  } yystype;
# define YYSTYPE yystype
# define YYSTYPE_IS_TRIVIAL 1
#endif
#line 148 "jv-exp.y"

/* YYSTYPE gets defined by %union */
static int parse_number (char *, int, int, YYSTYPE *);
#ifndef YYDEBUG
# define YYDEBUG 0
#endif



#define	YYFINAL		207
#define	YYFLAG		-32768
#define	YYNTBASE	56

/* YYTRANSLATE(YYLEX) -- Bison token number corresponding to YYLEX. */
#define YYTRANSLATE(x) ((unsigned)(x) <= 286 ? yytranslate[x] : 111)

/* YYTRANSLATE[YYLEX] -- Bison token number corresponding to YYLEX. */
static const char yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    54,     2,     2,     2,    43,    30,     2,
      48,    49,    41,    39,    23,    40,    46,    42,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,    55,     2,
      33,    24,    34,    25,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,    47,     2,    52,    29,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    50,    28,    51,    53,     2,     2,     2,
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
      16,    17,    18,    19,    20,    21,    22,    26,    27,    31,
      32,    35,    36,    37,    38,    44,    45
};

#if YYDEBUG
static const short yyprhs[] =
{
       0,     0,     2,     4,     6,     8,    10,    12,    14,    16,
      18,    20,    22,    24,    26,    28,    30,    32,    34,    36,
      38,    40,    42,    44,    46,    48,    51,    54,    56,    58,
      60,    62,    64,    66,    70,    72,    76,    78,    80,    82,
      86,    88,    90,    92,    94,    98,   100,   102,   108,   110,
     114,   115,   117,   122,   127,   129,   132,   136,   139,   143,
     145,   146,   150,   154,   159,   166,   173,   178,   183,   188,
     190,   192,   194,   196,   198,   201,   204,   206,   208,   211,
     214,   217,   219,   222,   225,   227,   230,   233,   235,   241,
     246,   252,   254,   258,   262,   266,   268,   272,   276,   278,
     282,   286,   288,   292,   296,   300,   304,   306,   310,   314,
     316,   320,   322,   326,   328,   332,   334,   338,   340,   344,
     346,   352,   354,   356,   360,   364,   366,   368,   370,   372
};
static const short yyrhs[] =
{
      72,     0,    57,     0,    58,     0,    61,     0,    67,     0,
       6,     0,     3,     0,     9,     0,     4,     0,     7,     0,
      59,     0,    62,     0,    16,     0,    63,     0,    64,     0,
      13,     0,    12,     0,    14,     0,    11,     0,    15,     0,
      18,     0,    17,     0,    68,     0,    65,     0,    61,    83,
       0,    68,    83,     0,     5,     0,    71,     0,    70,     0,
      71,     0,     5,     0,     9,     0,    68,    46,    70,     0,
     110,     0,    72,    23,   110,     0,    74,     0,    80,     0,
      60,     0,    48,   110,    49,     0,    77,     0,    85,     0,
      86,     0,    87,     0,    75,    78,    76,     0,    50,     0,
      51,     0,    22,    66,    48,    79,    49,     0,   110,     0,
      78,    23,   110,     0,     0,    78,     0,    22,    61,    81,
      84,     0,    22,    65,    81,    84,     0,    82,     0,    81,
      82,     0,    47,   110,    52,     0,    47,    52,     0,    83,
      47,    52,     0,    83,     0,     0,    73,    46,    70,     0,
      19,    46,    70,     0,    68,    48,    79,    49,     0,    73,
      46,    70,    48,    79,    49,     0,    21,    46,    70,    48,
      79,    49,     0,    68,    47,   110,    52,     0,    19,    47,
     110,    52,     0,    74,    47,   110,    52,     0,    73,     0,
      68,     0,    19,     0,    89,     0,    90,     0,    88,    44,
       0,    88,    45,     0,    92,     0,    93,     0,    39,    91,
       0,    40,    91,     0,    41,    91,     0,    94,     0,    44,
      91,     0,    45,    91,     0,    88,     0,    53,    91,     0,
      54,    91,     0,    95,     0,    48,    61,    84,    49,    91,
       0,    48,   110,    49,    94,     0,    48,    68,    83,    49,
      94,     0,    91,     0,    96,    41,    91,     0,    96,    42,
      91,     0,    96,    43,    91,     0,    96,     0,    97,    39,
      96,     0,    97,    40,    96,     0,    97,     0,    98,    37,
      97,     0,    98,    38,    97,     0,    98,     0,    99,    33,
      98,     0,    99,    34,    98,     0,    99,    35,    98,     0,
      99,    36,    98,     0,    99,     0,   100,    31,    99,     0,
     100,    32,    99,     0,   100,     0,   101,    30,   100,     0,
     101,     0,   102,    29,   101,     0,   102,     0,   103,    28,
     102,     0,   103,     0,   104,    27,   103,     0,   104,     0,
     105,    26,   104,     0,   105,     0,   105,    25,   110,    55,
     106,     0,   106,     0,   108,     0,   109,    24,   106,     0,
     109,    20,   106,     0,    69,     0,    19,     0,    85,     0,
      87,     0,   107,     0
};

#endif

#if YYDEBUG
/* YYRLINE[YYN] -- source line where rule number YYN was defined. */
static const short yyrline[] =
{
       0,   204,   205,   208,   216,   218,   221,   230,   236,   244,
     249,   254,   264,   266,   270,   272,   275,   278,   280,   282,
     284,   288,   291,   302,   307,   311,   314,   318,   320,   323,
     325,   328,   330,   333,   357,   358,   362,   364,   367,   369,
     370,   371,   372,   373,   374,   381,   386,   391,   397,   400,
     404,   407,   410,   414,   419,   421,   424,   428,   431,   435,
     437,   441,   444,   449,   452,   454,   458,   476,   478,   482,
     484,   486,   488,   489,   492,   497,   502,   504,   505,   506,
     508,   510,   513,   518,   523,   525,   527,   529,   532,   537,
     558,   565,   567,   569,   571,   575,   577,   579,   583,   585,
     587,   592,   594,   596,   598,   600,   605,   607,   609,   613,
     615,   619,   621,   624,   626,   630,   632,   636,   638,   642,
     644,   648,   650,   653,   656,   662,   665,   667,   668,   672
};
#endif


#if (YYDEBUG) || defined YYERROR_VERBOSE

/* YYTNAME[TOKEN_NUM] -- String name of the token TOKEN_NUM. */
static const char *const yytname[] =
{
  "$", "error", "$undefined.", "INTEGER_LITERAL", "FLOATING_POINT_LITERAL", 
  "IDENTIFIER", "STRING_LITERAL", "BOOLEAN_LITERAL", "TYPENAME", 
  "NAME_OR_INT", "ERROR", "LONG", "SHORT", "BYTE", "INT", "CHAR", 
  "BOOLEAN", "DOUBLE", "FLOAT", "VARIABLE", "ASSIGN_MODIFY", "SUPER", 
  "NEW", "','", "'='", "'?'", "OROR", "ANDAND", "'|'", "'^'", "'&'", 
  "EQUAL", "NOTEQUAL", "'<'", "'>'", "LEQ", "GEQ", "LSH", "RSH", "'+'", 
  "'-'", "'*'", "'/'", "'%'", "INCREMENT", "DECREMENT", "'.'", "'['", 
  "'('", "')'", "'{'", "'}'", "']'", "'~'", "'!'", "':'", "start", 
  "type_exp", "PrimitiveOrArrayType", "StringLiteral", "Literal", 
  "PrimitiveType", "NumericType", "IntegralType", "FloatingPointType", 
  "ClassOrInterfaceType", "ClassType", "ArrayType", "Name", "ForcedName", 
  "SimpleName", "QualifiedName", "exp1", "Primary", "PrimaryNoNewArray", 
  "lcurly", "rcurly", "ClassInstanceCreationExpression", "ArgumentList", 
  "ArgumentList_opt", "ArrayCreationExpression", "DimExprs", "DimExpr", 
  "Dims", "Dims_opt", "FieldAccess", "MethodInvocation", "ArrayAccess", 
  "PostfixExpression", "PostIncrementExpression", 
  "PostDecrementExpression", "UnaryExpression", "PreIncrementExpression", 
  "PreDecrementExpression", "UnaryExpressionNotPlusMinus", 
  "CastExpression", "MultiplicativeExpression", "AdditiveExpression", 
  "ShiftExpression", "RelationalExpression", "EqualityExpression", 
  "AndExpression", "ExclusiveOrExpression", "InclusiveOrExpression", 
  "ConditionalAndExpression", "ConditionalOrExpression", 
  "ConditionalExpression", "AssignmentExpression", "Assignment", 
  "LeftHandSide", "Expression", 0
};
#endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives. */
static const short yyr1[] =
{
       0,    56,    56,    57,    58,    58,    59,    60,    60,    60,
      60,    60,    61,    61,    62,    62,    63,    63,    63,    63,
      63,    64,    64,    65,    66,    67,    67,    68,    68,    69,
      69,    70,    70,    71,    72,    72,    73,    73,    74,    74,
      74,    74,    74,    74,    74,    75,    76,    77,    78,    78,
      79,    79,    80,    80,    81,    81,    82,    83,    83,    84,
      84,    85,    85,    86,    86,    86,    87,    87,    87,    88,
      88,    88,    88,    88,    89,    90,    91,    91,    91,    91,
      91,    91,    92,    93,    94,    94,    94,    94,    95,    95,
      95,    96,    96,    96,    96,    97,    97,    97,    98,    98,
      98,    99,    99,    99,    99,    99,   100,   100,   100,   101,
     101,   102,   102,   103,   103,   104,   104,   105,   105,   106,
     106,   107,   107,   108,   108,   109,   109,   109,   109,   110
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN. */
static const short yyr2[] =
{
       0,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     2,     2,     1,     1,     1,
       1,     1,     1,     3,     1,     3,     1,     1,     1,     3,
       1,     1,     1,     1,     3,     1,     1,     5,     1,     3,
       0,     1,     4,     4,     1,     2,     3,     2,     3,     1,
       0,     3,     3,     4,     6,     6,     4,     4,     4,     1,
       1,     1,     1,     1,     2,     2,     1,     1,     2,     2,
       2,     1,     2,     2,     1,     2,     2,     1,     5,     4,
       5,     1,     3,     3,     3,     1,     3,     3,     1,     3,
       3,     1,     3,     3,     3,     3,     1,     3,     3,     1,
       3,     1,     3,     1,     3,     1,     3,     1,     3,     1,
       5,     1,     1,     3,     3,     1,     1,     1,     1,     1
};

/* YYDEFACT[S] -- default rule to reduce with in state S when YYTABLE
   doesn't specify something else to do.  Zero means the default is an
   error. */
static const short yydefact[] =
{
       0,     7,     9,    27,     6,    10,     8,    19,    17,    16,
      18,    20,    13,    22,    21,    71,     0,     0,     0,     0,
       0,     0,     0,     0,    45,     0,     0,     2,     3,    11,
      38,     4,    12,    14,    15,     5,    70,   125,    29,    28,
       1,    69,    36,     0,    40,    37,    41,    42,    43,    84,
      72,    73,    91,    76,    77,    81,    87,    95,    98,   101,
     106,   109,   111,   113,   115,   117,   119,   121,   129,   122,
       0,    34,     0,     0,     0,    27,     0,    24,     0,    23,
      28,     8,    71,    70,    41,    43,    78,    79,    80,    82,
      83,    60,    70,     0,    85,    86,     0,    25,     0,     0,
      50,    26,     0,     0,     0,     0,    48,    74,    75,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      31,    32,    62,     0,     0,     0,    60,    54,    60,    50,
       0,    59,     0,     0,    39,    57,     0,    33,     0,    51,
       0,    35,    61,     0,     0,    46,    44,    92,    93,    94,
      96,    97,    99,   100,   102,   103,   104,   105,   107,   108,
     110,   112,   114,   116,     0,   118,   124,   123,    67,    50,
       0,     0,    55,    52,    53,     0,     0,     0,    89,    58,
      66,    63,    50,    68,    49,     0,     0,    56,    47,    88,
      90,     0,   120,    65,    64,     0,     0,     0
};

static const short yydefgoto[] =
{
     205,    27,    28,    29,    30,    31,    32,    33,    34,    77,
      78,    35,    83,    37,    38,    80,    40,    41,    42,    43,
     156,    44,   149,   150,    45,   136,   137,   141,   142,    84,
      47,    85,    49,    50,    51,    52,    53,    54,    55,    56,
      57,    58,    59,    60,    61,    62,    63,    64,    65,    66,
      67,    68,    69,    70,   106
};

static const short yypact[] =
{
     202,-32768,-32768,    -3,-32768,-32768,     4,-32768,-32768,-32768,
  -32768,-32768,-32768,-32768,-32768,     7,   -21,   217,   306,   306,
     306,   306,   306,   202,-32768,   306,   306,-32768,-32768,-32768,
  -32768,    20,-32768,-32768,-32768,-32768,    42,-32768,-32768,    10,
      23,    29,    31,   358,-32768,-32768,    13,-32768,    24,    -6,
  -32768,-32768,-32768,-32768,-32768,-32768,-32768,    54,    91,    14,
      30,   102,    68,    92,   100,    32,   111,-32768,-32768,-32768,
      37,-32768,    53,   358,    53,-32768,   103,   103,   109,   113,
  -32768,-32768,    96,    77,-32768,-32768,-32768,-32768,-32768,-32768,
  -32768,    20,    42,   112,-32768,-32768,   110,   118,    53,   254,
     358,   118,   358,    53,   358,    -1,-32768,-32768,-32768,   306,
     306,   306,   306,   306,   306,   306,   306,   306,   306,   306,
     306,   306,   306,   306,   306,   306,   358,   306,   306,   306,
  -32768,-32768,-32768,   115,   120,   358,   123,-32768,   123,   358,
     358,   118,   122,    60,    65,-32768,   121,-32768,   124,   151,
     126,-32768,   129,   128,   358,-32768,-32768,-32768,-32768,-32768,
      54,    54,    91,    91,    14,    14,    14,    14,    30,    30,
     102,    68,    92,   100,   127,    32,-32768,-32768,-32768,   358,
     131,   254,-32768,-32768,-32768,   135,   306,    65,-32768,-32768,
  -32768,-32768,   358,-32768,-32768,   306,   137,-32768,-32768,-32768,
  -32768,   139,-32768,-32768,-32768,   178,   189,-32768
};

static const short yypgoto[] =
{
  -32768,-32768,-32768,-32768,-32768,     9,-32768,-32768,-32768,-32768,
  -32768,-32768,    19,-32768,   -58,     0,-32768,-32768,-32768,-32768,
  -32768,-32768,   147,  -132,-32768,   117,   -35,   -16,    -9,     6,
  -32768,    12,-32768,-32768,-32768,   -17,-32768,-32768,  -131,-32768,
      36,    41,   -36,    43,    73,    74,    72,    75,    76,-32768,
    -118,-32768,-32768,-32768,    18
};


#define	YYLAST		412


static const short yytable[] =
{
      39,    86,    87,    88,    89,    90,    46,   185,    94,    95,
     176,   177,    48,   188,   132,    97,   134,   -31,    71,    36,
     101,   -31,   154,    39,   -32,    74,    76,  -126,   -32,    46,
     -30,  -126,    91,  -127,   -30,    48,    79,  -127,   107,   108,
     147,    93,    92,    39,  -128,   152,   102,   196,  -128,    46,
     155,   114,   115,    72,    73,    48,   200,   128,   130,   125,
     201,   129,   131,   116,   117,   118,   119,    96,     1,     2,
      75,     4,     5,    39,    81,   103,   143,   202,   104,    46,
     164,   165,   166,   167,    82,    48,    16,    17,    98,    99,
     100,   133,   157,   158,   159,   109,   110,   111,   122,    39,
      39,   182,    39,   182,    39,    46,    46,   146,    46,   187,
      46,    48,    48,    23,    48,    24,    48,   148,    25,    26,
     151,   123,   153,    98,   140,   100,    39,   183,   124,   184,
     112,   113,    46,   120,   121,    39,   126,   127,    48,    39,
      39,    46,    72,    73,   174,    46,    46,    48,   160,   161,
     135,    48,    48,   180,    39,   162,   163,   139,   148,    98,
      46,   144,   145,   168,   169,   146,    48,   178,   179,   199,
     181,   186,   194,   189,   154,   191,   190,   192,   206,    39,
     193,    39,   195,   197,   198,    46,   203,    46,   204,   207,
     105,    48,    39,    48,   138,   170,   172,   171,    46,   180,
     173,     0,     0,   175,    48,     1,     2,     3,     4,     5,
       0,     6,     0,     7,     8,     9,    10,    11,    12,    13,
      14,    15,    75,    16,    17,     0,     0,     0,     7,     8,
       9,    10,    11,    12,    13,    14,     0,     0,     0,     0,
       0,    18,    19,    20,     0,     0,    21,    22,     0,     0,
      23,     0,    24,     0,     0,    25,    26,     1,     2,     3,
       4,     5,     0,     6,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    15,     0,    16,    17,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    18,    19,    20,     0,     0,    21,    22,
       0,     0,    23,     0,    24,     0,   145,    25,    26,     1,
       2,    75,     4,     5,     0,    81,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    82,     0,    16,    17,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    18,    19,    20,     0,     0,
      21,    22,     0,     0,    23,     0,    24,     0,     0,    25,
      26,     1,     2,     3,     4,     5,     0,     6,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    15,     0,    16,
      17,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    18,    19,    20,
       0,     0,    21,    22,     0,     0,    23,     0,    24,     0,
       0,    25,    26
};

static const short yycheck[] =
{
       0,    18,    19,    20,    21,    22,     0,   139,    25,    26,
     128,   129,     0,   144,    72,    31,    74,    20,     0,     0,
      36,    24,    23,    23,    20,    46,    17,    20,    24,    23,
      20,    24,    23,    20,    24,    23,    17,    24,    44,    45,
      98,    23,    23,    43,    20,   103,    23,   179,    24,    43,
      51,    37,    38,    46,    47,    43,   187,    20,     5,    27,
     192,    24,     9,    33,    34,    35,    36,    47,     3,     4,
       5,     6,     7,    73,     9,    46,    92,   195,    47,    73,
     116,   117,   118,   119,    19,    73,    21,    22,    46,    47,
      48,    73,   109,   110,   111,    41,    42,    43,    30,    99,
     100,   136,   102,   138,   104,    99,   100,    47,   102,    49,
     104,    99,   100,    48,   102,    50,   104,    99,    53,    54,
     102,    29,   104,    46,    47,    48,   126,   136,    28,   138,
      39,    40,   126,    31,    32,   135,    25,    26,   126,   139,
     140,   135,    46,    47,   126,   139,   140,   135,   112,   113,
      47,   139,   140,   135,   154,   114,   115,    48,   140,    46,
     154,    49,    52,   120,   121,    47,   154,    52,    48,   186,
      47,    49,   154,    52,    23,    49,    52,    48,     0,   179,
      52,   181,    55,    52,    49,   179,    49,   181,    49,     0,
      43,   179,   192,   181,    77,   122,   124,   123,   192,   181,
     125,    -1,    -1,   127,   192,     3,     4,     5,     6,     7,
      -1,     9,    -1,    11,    12,    13,    14,    15,    16,    17,
      18,    19,     5,    21,    22,    -1,    -1,    -1,    11,    12,
      13,    14,    15,    16,    17,    18,    -1,    -1,    -1,    -1,
      -1,    39,    40,    41,    -1,    -1,    44,    45,    -1,    -1,
      48,    -1,    50,    -1,    -1,    53,    54,     3,     4,     5,
       6,     7,    -1,     9,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    19,    -1,    21,    22,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    39,    40,    41,    -1,    -1,    44,    45,
      -1,    -1,    48,    -1,    50,    -1,    52,    53,    54,     3,
       4,     5,     6,     7,    -1,     9,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    19,    -1,    21,    22,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    39,    40,    41,    -1,    -1,
      44,    45,    -1,    -1,    48,    -1,    50,    -1,    -1,    53,
      54,     3,     4,     5,     6,     7,    -1,     9,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    19,    -1,    21,
      22,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    39,    40,    41,
      -1,    -1,    44,    45,    -1,    -1,    48,    -1,    50,    -1,
      -1,    53,    54
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

case 3:
#line 209 "jv-exp.y"
{
		  write_exp_elt_opcode(OP_TYPE);
		  write_exp_elt_type(yyvsp[0].tval);
		  write_exp_elt_opcode(OP_TYPE);
		}
    break;
case 6:
#line 223 "jv-exp.y"
{
		  write_exp_elt_opcode (OP_STRING);
		  write_exp_string (yyvsp[0].sval);
		  write_exp_elt_opcode (OP_STRING);
		}
    break;
case 7:
#line 232 "jv-exp.y"
{ write_exp_elt_opcode (OP_LONG);
		  write_exp_elt_type (yyvsp[0].typed_val_int.type);
		  write_exp_elt_longcst ((LONGEST)(yyvsp[0].typed_val_int.val));
		  write_exp_elt_opcode (OP_LONG); }
    break;
case 8:
#line 237 "jv-exp.y"
{ YYSTYPE val;
		  parse_number (yyvsp[0].sval.ptr, yyvsp[0].sval.length, 0, &val);
		  write_exp_elt_opcode (OP_LONG);
		  write_exp_elt_type (val.typed_val_int.type);
		  write_exp_elt_longcst ((LONGEST)val.typed_val_int.val);
		  write_exp_elt_opcode (OP_LONG);
		}
    break;
case 9:
#line 245 "jv-exp.y"
{ write_exp_elt_opcode (OP_DOUBLE);
		  write_exp_elt_type (yyvsp[0].typed_val_float.type);
		  write_exp_elt_dblcst (yyvsp[0].typed_val_float.dval);
		  write_exp_elt_opcode (OP_DOUBLE); }
    break;
case 10:
#line 250 "jv-exp.y"
{ write_exp_elt_opcode (OP_LONG);
		  write_exp_elt_type (java_boolean_type);
		  write_exp_elt_longcst ((LONGEST)yyvsp[0].lval);
		  write_exp_elt_opcode (OP_LONG); }
    break;
case 13:
#line 267 "jv-exp.y"
{ yyval.tval = java_boolean_type; }
    break;
case 16:
#line 277 "jv-exp.y"
{ yyval.tval = java_byte_type; }
    break;
case 17:
#line 279 "jv-exp.y"
{ yyval.tval = java_short_type; }
    break;
case 18:
#line 281 "jv-exp.y"
{ yyval.tval = java_int_type; }
    break;
case 19:
#line 283 "jv-exp.y"
{ yyval.tval = java_long_type; }
    break;
case 20:
#line 285 "jv-exp.y"
{ yyval.tval = java_char_type; }
    break;
case 21:
#line 290 "jv-exp.y"
{ yyval.tval = java_float_type; }
    break;
case 22:
#line 292 "jv-exp.y"
{ yyval.tval = java_double_type; }
    break;
case 23:
#line 304 "jv-exp.y"
{ yyval.tval = java_type_from_name (yyvsp[0].sval); }
    break;
case 25:
#line 313 "jv-exp.y"
{ yyval.tval = java_array_type (yyvsp[-1].tval, yyvsp[0].lval); }
    break;
case 26:
#line 315 "jv-exp.y"
{ yyval.tval = java_array_type (java_type_from_name (yyvsp[-1].sval), yyvsp[0].lval); }
    break;
case 33:
#line 335 "jv-exp.y"
{ yyval.sval.length = yyvsp[-2].sval.length + yyvsp[0].sval.length + 1;
		  if (yyvsp[-2].sval.ptr + yyvsp[-2].sval.length + 1 == yyvsp[0].sval.ptr
		      && yyvsp[-2].sval.ptr[yyvsp[-2].sval.length] == '.')
		    yyval.sval.ptr = yyvsp[-2].sval.ptr;  /* Optimization. */
		  else
		    {
		      yyval.sval.ptr = (char *) xmalloc (yyval.sval.length + 1);
		      make_cleanup (free, yyval.sval.ptr);
		      sprintf (yyval.sval.ptr, "%.*s.%.*s",
			       yyvsp[-2].sval.length, yyvsp[-2].sval.ptr, yyvsp[0].sval.length, yyvsp[0].sval.ptr);
		} }
    break;
case 35:
#line 359 "jv-exp.y"
{ write_exp_elt_opcode (BINOP_COMMA); }
    break;
case 44:
#line 375 "jv-exp.y"
{ write_exp_elt_opcode (OP_ARRAY);
		  write_exp_elt_longcst ((LONGEST) 0);
		  write_exp_elt_longcst ((LONGEST) yyvsp[0].lval);
		  write_exp_elt_opcode (OP_ARRAY); }
    break;
case 45:
#line 383 "jv-exp.y"
{ start_arglist (); }
    break;
case 46:
#line 388 "jv-exp.y"
{ yyval.lval = end_arglist () - 1; }
    break;
case 47:
#line 393 "jv-exp.y"
{ internal_error (__FILE__, __LINE__,
				  _("FIXME - ClassInstanceCreationExpression")); }
    break;
case 48:
#line 399 "jv-exp.y"
{ arglist_len = 1; }
    break;
case 49:
#line 401 "jv-exp.y"
{ arglist_len++; }
    break;
case 50:
#line 406 "jv-exp.y"
{ arglist_len = 0; }
    break;
case 52:
#line 412 "jv-exp.y"
{ internal_error (__FILE__, __LINE__,
				  _("FIXME - ArrayCreationExpression")); }
    break;
case 53:
#line 415 "jv-exp.y"
{ internal_error (__FILE__, __LINE__,
				  _("FIXME - ArrayCreationExpression")); }
    break;
case 57:
#line 430 "jv-exp.y"
{ yyval.lval = 1; }
    break;
case 58:
#line 432 "jv-exp.y"
{ yyval.lval = yyvsp[-2].lval + 1; }
    break;
case 60:
#line 438 "jv-exp.y"
{ yyval.lval = 0; }
    break;
case 61:
#line 443 "jv-exp.y"
{ push_fieldnames (yyvsp[0].sval); }
    break;
case 62:
#line 445 "jv-exp.y"
{ push_fieldnames (yyvsp[0].sval); }
    break;
case 63:
#line 451 "jv-exp.y"
{ error (_("Method invocation not implemented")); }
    break;
case 64:
#line 453 "jv-exp.y"
{ error (_("Method invocation not implemented")); }
    break;
case 65:
#line 455 "jv-exp.y"
{ error (_("Method invocation not implemented")); }
    break;
case 66:
#line 460 "jv-exp.y"
{
                  /* Emit code for the Name now, then exchange it in the
		     expout array with the Expression's code.  We could
		     introduce a OP_SWAP code or a reversed version of
		     BINOP_SUBSCRIPT, but that makes the rest of GDB pay
		     for our parsing kludges.  */
		  struct expression *name_expr;

		  push_expression_name (yyvsp[-3].sval);
		  name_expr = copy_exp (expout, expout_ptr);
		  expout_ptr -= name_expr->nelts;
		  insert_exp (expout_ptr-length_of_subexp (expout, expout_ptr),
			      name_expr);
		  free (name_expr);
		  write_exp_elt_opcode (BINOP_SUBSCRIPT);
		}
    break;
case 67:
#line 477 "jv-exp.y"
{ write_exp_elt_opcode (BINOP_SUBSCRIPT); }
    break;
case 68:
#line 479 "jv-exp.y"
{ write_exp_elt_opcode (BINOP_SUBSCRIPT); }
    break;
case 70:
#line 485 "jv-exp.y"
{ push_expression_name (yyvsp[0].sval); }
    break;
case 74:
#line 494 "jv-exp.y"
{ write_exp_elt_opcode (UNOP_POSTINCREMENT); }
    break;
case 75:
#line 499 "jv-exp.y"
{ write_exp_elt_opcode (UNOP_POSTDECREMENT); }
    break;
case 79:
#line 507 "jv-exp.y"
{ write_exp_elt_opcode (UNOP_NEG); }
    break;
case 80:
#line 509 "jv-exp.y"
{ write_exp_elt_opcode (UNOP_IND); }
    break;
case 82:
#line 515 "jv-exp.y"
{ write_exp_elt_opcode (UNOP_PREINCREMENT); }
    break;
case 83:
#line 520 "jv-exp.y"
{ write_exp_elt_opcode (UNOP_PREDECREMENT); }
    break;
case 85:
#line 526 "jv-exp.y"
{ write_exp_elt_opcode (UNOP_COMPLEMENT); }
    break;
case 86:
#line 528 "jv-exp.y"
{ write_exp_elt_opcode (UNOP_LOGICAL_NOT); }
    break;
case 88:
#line 534 "jv-exp.y"
{ write_exp_elt_opcode (UNOP_CAST);
		  write_exp_elt_type (java_array_type (yyvsp[-3].tval, yyvsp[-2].lval));
		  write_exp_elt_opcode (UNOP_CAST); }
    break;
case 89:
#line 538 "jv-exp.y"
{
		  int exp_size = expout_ptr;
		  int last_exp_size = length_of_subexp(expout, expout_ptr);
		  struct type *type;
		  int i;
		  int base = expout_ptr - last_exp_size - 3;
		  if (base < 0 || expout->elts[base+2].opcode != OP_TYPE)
		    error (_("Invalid cast expression"));
		  type = expout->elts[base+1].type;
		  /* Remove the 'Expression' and slide the
		     UnaryExpressionNotPlusMinus down to replace it. */
		  for (i = 0;  i < last_exp_size;  i++)
		    expout->elts[base + i] = expout->elts[base + i + 3];
		  expout_ptr -= 3;
		  if (TYPE_CODE (type) == TYPE_CODE_STRUCT)
		    type = lookup_pointer_type (type);
		  write_exp_elt_opcode (UNOP_CAST);
		  write_exp_elt_type (type);
		  write_exp_elt_opcode (UNOP_CAST);
		}
    break;
case 90:
#line 559 "jv-exp.y"
{ write_exp_elt_opcode (UNOP_CAST);
		  write_exp_elt_type (java_array_type (java_type_from_name (yyvsp[-3].sval), yyvsp[-2].lval));
		  write_exp_elt_opcode (UNOP_CAST); }
    break;
case 92:
#line 568 "jv-exp.y"
{ write_exp_elt_opcode (BINOP_MUL); }
    break;
case 93:
#line 570 "jv-exp.y"
{ write_exp_elt_opcode (BINOP_DIV); }
    break;
case 94:
#line 572 "jv-exp.y"
{ write_exp_elt_opcode (BINOP_REM); }
    break;
case 96:
#line 578 "jv-exp.y"
{ write_exp_elt_opcode (BINOP_ADD); }
    break;
case 97:
#line 580 "jv-exp.y"
{ write_exp_elt_opcode (BINOP_SUB); }
    break;
case 99:
#line 586 "jv-exp.y"
{ write_exp_elt_opcode (BINOP_LSH); }
    break;
case 100:
#line 588 "jv-exp.y"
{ write_exp_elt_opcode (BINOP_RSH); }
    break;
case 102:
#line 595 "jv-exp.y"
{ write_exp_elt_opcode (BINOP_LESS); }
    break;
case 103:
#line 597 "jv-exp.y"
{ write_exp_elt_opcode (BINOP_GTR); }
    break;
case 104:
#line 599 "jv-exp.y"
{ write_exp_elt_opcode (BINOP_LEQ); }
    break;
case 105:
#line 601 "jv-exp.y"
{ write_exp_elt_opcode (BINOP_GEQ); }
    break;
case 107:
#line 608 "jv-exp.y"
{ write_exp_elt_opcode (BINOP_EQUAL); }
    break;
case 108:
#line 610 "jv-exp.y"
{ write_exp_elt_opcode (BINOP_NOTEQUAL); }
    break;
case 110:
#line 616 "jv-exp.y"
{ write_exp_elt_opcode (BINOP_BITWISE_AND); }
    break;
case 112:
#line 622 "jv-exp.y"
{ write_exp_elt_opcode (BINOP_BITWISE_XOR); }
    break;
case 114:
#line 627 "jv-exp.y"
{ write_exp_elt_opcode (BINOP_BITWISE_IOR); }
    break;
case 116:
#line 633 "jv-exp.y"
{ write_exp_elt_opcode (BINOP_LOGICAL_AND); }
    break;
case 118:
#line 639 "jv-exp.y"
{ write_exp_elt_opcode (BINOP_LOGICAL_OR); }
    break;
case 120:
#line 645 "jv-exp.y"
{ write_exp_elt_opcode (TERNOP_COND); }
    break;
case 123:
#line 655 "jv-exp.y"
{ write_exp_elt_opcode (BINOP_ASSIGN); }
    break;
case 124:
#line 657 "jv-exp.y"
{ write_exp_elt_opcode (BINOP_ASSIGN_MODIFY);
		  write_exp_elt_opcode (yyvsp[-1].opcode);
		  write_exp_elt_opcode (BINOP_ASSIGN_MODIFY); }
    break;
case 125:
#line 664 "jv-exp.y"
{ push_expression_name (yyvsp[0].sval); }
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
#line 676 "jv-exp.y"

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
  ULONGEST n = 0;
  ULONGEST limit, limit_div_base;

  int c;
  int base = input_radix;

  struct type *type;

  if (parsed_float)
    {
      /* It's a float since it contains a point or an exponent.  */
      char c;
      int num = 0;	/* number of tokens scanned by scanf */
      char saved_char = p[len];

      p[len] = 0;	/* null-terminate the token */
      if (sizeof (putithere->typed_val_float.dval) <= sizeof (float))
	num = sscanf (p, "%g%c", (float *) &putithere->typed_val_float.dval, &c);
      else if (sizeof (putithere->typed_val_float.dval) <= sizeof (double))
	num = sscanf (p, "%lg%c", (double *) &putithere->typed_val_float.dval, &c);
      else
	{
#ifdef SCANF_HAS_LONG_DOUBLE
	  num = sscanf (p, "%Lg%c", &putithere->typed_val_float.dval, &c);
#else
	  /* Scan it into a double, then assign it to the long double.
	     This at least wins with values representable in the range
	     of doubles. */
	  double temp;
	  num = sscanf (p, "%lg%c", &temp, &c);
	  putithere->typed_val_float.dval = temp;
#endif
	}
      p[len] = saved_char;	/* restore the input stream */
      if (num != 1) 		/* check scanf found ONLY a float ... */
	return ERROR;
      /* See if it has `f' or `d' suffix (float or double).  */

      c = tolower (p[len - 1]);

      if (c == 'f' || c == 'F')
	putithere->typed_val_float.type = builtin_type_float;
      else if (isdigit (c) || c == '.' || c == 'd' || c == 'D')
	putithere->typed_val_float.type = builtin_type_double;
      else
	return ERROR;

      return FLOATING_POINT_LITERAL;
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

  c = p[len-1];
  /* A paranoid calculation of (1<<64)-1. */
  limit = (ULONGEST)0xffffffff;
  limit = ((limit << 16) << 16) | limit;
  if (c == 'l' || c == 'L')
    {
      type = java_long_type;
      len--;
    }
  else
    {
      type = java_int_type;
    }
  limit_div_base = limit / (ULONGEST) base;

  while (--len >= 0)
    {
      c = *p++;
      if (c >= '0' && c <= '9')
	c -= '0';
      else if (c >= 'A' && c <= 'Z')
	c -= 'A' - 10;
      else if (c >= 'a' && c <= 'z')
	c -= 'a' - 10;
      else
	return ERROR;	/* Char not a digit */
      if (c >= base)
	return ERROR;
      if (n > limit_div_base
	  || (n *= base) > limit - c)
	error (_("Numeric constant too large"));
      n += c;
	}

  /* If the type is bigger than a 32-bit signed integer can be, implicitly
     promote to long.  Java does not do this, so mark it as builtin_type_uint64
     rather than java_long_type.  0x80000000 will become -0x80000000 instead
     of 0x80000000L, because we don't know the sign at this point.
  */
  if (type == java_int_type && n > (ULONGEST)0x80000000)
    type = builtin_type_uint64;

  putithere->typed_val_int.val = n;
  putithere->typed_val_int.type = type;

  return INTEGER_LITERAL;
}

struct token
{
  char *operator;
  int token;
  enum exp_opcode opcode;
};

static const struct token tokentab3[] =
  {
    {">>=", ASSIGN_MODIFY, BINOP_RSH},
    {"<<=", ASSIGN_MODIFY, BINOP_LSH}
  };

static const struct token tokentab2[] =
  {
    {"+=", ASSIGN_MODIFY, BINOP_ADD},
    {"-=", ASSIGN_MODIFY, BINOP_SUB},
    {"*=", ASSIGN_MODIFY, BINOP_MUL},
    {"/=", ASSIGN_MODIFY, BINOP_DIV},
    {"%=", ASSIGN_MODIFY, BINOP_REM},
    {"|=", ASSIGN_MODIFY, BINOP_BITWISE_IOR},
    {"&=", ASSIGN_MODIFY, BINOP_BITWISE_AND},
    {"^=", ASSIGN_MODIFY, BINOP_BITWISE_XOR},
    {"++", INCREMENT, BINOP_END},
    {"--", DECREMENT, BINOP_END},
    {"&&", ANDAND, BINOP_END},
    {"||", OROR, BINOP_END},
    {"<<", LSH, BINOP_END},
    {">>", RSH, BINOP_END},
    {"==", EQUAL, BINOP_END},
    {"!=", NOTEQUAL, BINOP_END},
    {"<=", LEQ, BINOP_END},
    {">=", GEQ, BINOP_END}
  };

/* Read one token, getting characters through lexptr.  */

static int
yylex ()
{
  int c;
  int namelen;
  unsigned int i;
  char *tokstart;
  char *tokptr;
  int tempbufindex;
  static char *tempbuf;
  static int tempbufsize;
  
 retry:

  prev_lexptr = lexptr;

  tokstart = lexptr;
  /* See if it is a special token of length 3.  */
  for (i = 0; i < sizeof tokentab3 / sizeof tokentab3[0]; i++)
    if (strncmp (tokstart, tokentab3[i].operator, 3) == 0)
      {
	lexptr += 3;
	yylval.opcode = tokentab3[i].opcode;
	return tokentab3[i].token;
      }

  /* See if it is a special token of length 2.  */
  for (i = 0; i < sizeof tokentab2 / sizeof tokentab2[0]; i++)
    if (strncmp (tokstart, tokentab2[i].operator, 2) == 0)
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
	 or we have a quoted symbol reference ('foo(int,int)' in C++
	 for example). */
      lexptr++;
      c = *lexptr++;
      if (c == '\\')
	c = parse_escape (&lexptr);
      else if (c == '\'')
	error (_("Empty character constant"));

      yylval.typed_val_int.val = c;
      yylval.typed_val_int.type = java_char_type;

      c = *lexptr++;
      if (c != '\'')
	{
	  namelen = skip_quoted (tokstart) - tokstart;
	  if (namelen > 2)
	    {
	      lexptr = tokstart + namelen;
	      if (lexptr[-1] != '\'')
		error (_("Unmatched single quote"));
	      namelen -= 2;
	      tokstart++;
	      goto tryname;
	    }
	  error (_("Invalid character constant"));
	}
      return INTEGER_LITERAL;

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
	    error (_("Invalid number \"%s\""), err_copy);
	  }
	lexptr = p;
	return toktype;
      }

    case '+':
    case '-':
    case '*':
    case '/':
    case '%':
    case '|':
    case '&':
    case '^':
    case '~':
    case '!':
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
	  error (_("Unterminated string in expression"));
	}
      tempbuf[tempbufindex] = '\0';	/* See note above */
      yylval.sval.ptr = tempbuf;
      yylval.sval.length = tempbufindex;
      lexptr = tokptr;
      return (STRING_LITERAL);
    }

  if (!(c == '_' || c == '$'
	|| (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')))
    /* We must have come across a bad character (e.g. ';').  */
    error (_("Invalid character '%c' in expression"), c);

  /* It's a name.  See how long it is.  */
  namelen = 0;
  for (c = tokstart[namelen];
       (c == '_'
	|| c == '$'
	|| (c >= '0' && c <= '9')
	|| (c >= 'a' && c <= 'z')
	|| (c >= 'A' && c <= 'Z')
	|| c == '<');
       )
    {
      if (c == '<')
	{
	  int i = namelen;
	  while (tokstart[++i] && tokstart[i] != '>');
	  if (tokstart[i] == '>')
	    namelen = i;
	}
       c = tokstart[++namelen];
     }

  /* The token "if" terminates the expression and is NOT 
     removed from the input stream.  */
  if (namelen == 2 && tokstart[0] == 'i' && tokstart[1] == 'f')
    {
      return 0;
    }

  lexptr += namelen;

  tryname:

  /* Catch specific keywords.  Should be done with a data structure.  */
  switch (namelen)
    {
    case 7:
      if (DEPRECATED_STREQN (tokstart, "boolean", 7))
	return BOOLEAN;
      break;
    case 6:
      if (DEPRECATED_STREQN (tokstart, "double", 6))      
	return DOUBLE;
      break;
    case 5:
      if (DEPRECATED_STREQN (tokstart, "short", 5))
	return SHORT;
      if (DEPRECATED_STREQN (tokstart, "false", 5))
	{
	  yylval.lval = 0;
	  return BOOLEAN_LITERAL;
	}
      if (DEPRECATED_STREQN (tokstart, "super", 5))
	return SUPER;
      if (DEPRECATED_STREQN (tokstart, "float", 5))
	return FLOAT;
      break;
    case 4:
      if (DEPRECATED_STREQN (tokstart, "long", 4))
	return LONG;
      if (DEPRECATED_STREQN (tokstart, "byte", 4))
	return BYTE;
      if (DEPRECATED_STREQN (tokstart, "char", 4))
	return CHAR;
      if (DEPRECATED_STREQN (tokstart, "true", 4))
	{
	  yylval.lval = 1;
	  return BOOLEAN_LITERAL;
	}
      break;
    case 3:
      if (strncmp (tokstart, "int", 3) == 0)
	return INT;
      if (strncmp (tokstart, "new", 3) == 0)
	return NEW;
      break;
    default:
      break;
    }

  yylval.sval.ptr = tokstart;
  yylval.sval.length = namelen;

  if (*tokstart == '$')
    {
      write_dollar_variable (yylval.sval);
      return VARIABLE;
    }

  /* Input names that aren't symbols but ARE valid hex numbers,
     when the input radix permits them, can be names or numbers
     depending on the parse.  Note we support radixes > 16 here.  */
  if (((tokstart[0] >= 'a' && tokstart[0] < 'a' + input_radix - 10) ||
       (tokstart[0] >= 'A' && tokstart[0] < 'A' + input_radix - 10)))
    {
      YYSTYPE newlval;	/* Its value is ignored.  */
      int hextype = parse_number (tokstart, namelen, 0, &newlval);
      if (hextype == INTEGER_LITERAL)
	return NAME_OR_INT;
    }
  return IDENTIFIER;
}

void
yyerror (msg)
     char *msg;
{
  if (prev_lexptr)
    lexptr = prev_lexptr;

  if (msg)
    error (_("%s: near `%s'"), msg, lexptr);
  else
    error (_("error in expression, near `%s'"), lexptr);
}

static struct type *
java_type_from_name (name)
     struct stoken name;
 
{
  char *tmp = copy_name (name);
  struct type *typ = java_lookup_class (tmp);
  if (typ == NULL || TYPE_CODE (typ) != TYPE_CODE_STRUCT)
    error (_("No class named `%s'"), tmp);
  return typ;
}

/* If NAME is a valid variable name in this scope, push it and return 1.
   Otherwise, return 0. */

static int
push_variable (struct stoken name)
{
  char *tmp = copy_name (name);
  int is_a_field_of_this = 0;
  struct symbol *sym;
  sym = lookup_symbol (tmp, expression_context_block, VAR_DOMAIN,
		       &is_a_field_of_this, (struct symtab **) NULL);
  if (sym && SYMBOL_CLASS (sym) != LOC_TYPEDEF)
    {
      if (symbol_read_needs_frame (sym))
	{
	  if (innermost_block == 0 ||
	      contained_in (block_found, innermost_block))
	    innermost_block = block_found;
	}

      write_exp_elt_opcode (OP_VAR_VALUE);
      /* We want to use the selected frame, not another more inner frame
	 which happens to be in the same block.  */
      write_exp_elt_block (NULL);
      write_exp_elt_sym (sym);
      write_exp_elt_opcode (OP_VAR_VALUE);
      return 1;
    }
  if (is_a_field_of_this)
    {
      /* it hangs off of `this'.  Must not inadvertently convert from a
	 method call to data ref.  */
      if (innermost_block == 0 || 
	  contained_in (block_found, innermost_block))
	innermost_block = block_found;
      write_exp_elt_opcode (OP_THIS);
      write_exp_elt_opcode (OP_THIS);
      write_exp_elt_opcode (STRUCTOP_PTR);
      write_exp_string (name);
      write_exp_elt_opcode (STRUCTOP_PTR);
      return 1;
    }
  return 0;
}

/* Assuming a reference expression has been pushed, emit the
   STRUCTOP_STRUCT ops to access the field named NAME.  If NAME is a
   qualified name (has '.'), generate a field access for each part. */

static void
push_fieldnames (name)
     struct stoken name;
{
  int i;
  struct stoken token;
  token.ptr = name.ptr;
  for (i = 0;  ;  i++)
    {
      if (i == name.length || name.ptr[i] == '.')
	{
	  /* token.ptr is start of current field name. */
	  token.length = &name.ptr[i] - token.ptr;
	  write_exp_elt_opcode (STRUCTOP_STRUCT);
	  write_exp_string (token);
	  write_exp_elt_opcode (STRUCTOP_STRUCT);
	  token.ptr += token.length + 1;
	}
      if (i >= name.length)
	break;
    }
}

/* Helper routine for push_expression_name.
   Handle a qualified name, where DOT_INDEX is the index of the first '.' */

static void
push_qualified_expression_name (struct stoken name, int dot_index)
{
  struct stoken token;
  char *tmp;
  struct type *typ;

  token.ptr = name.ptr;
  token.length = dot_index;

  if (push_variable (token))
    {
      token.ptr = name.ptr + dot_index + 1;
      token.length = name.length - dot_index - 1;
      push_fieldnames (token);
      return;
    }

  token.ptr = name.ptr;
  for (;;)
    {
      token.length = dot_index;
      tmp = copy_name (token);
      typ = java_lookup_class (tmp);
      if (typ != NULL)
	{
	  if (dot_index == name.length)
	    {
	      write_exp_elt_opcode(OP_TYPE);
	      write_exp_elt_type(typ);
	      write_exp_elt_opcode(OP_TYPE);
	      return;
	    }
	  dot_index++;  /* Skip '.' */
	  name.ptr += dot_index;
	  name.length -= dot_index;
	  dot_index = 0;
	  while (dot_index < name.length && name.ptr[dot_index] != '.') 
	    dot_index++;
	  token.ptr = name.ptr;
	  token.length = dot_index;
	  write_exp_elt_opcode (OP_SCOPE);
	  write_exp_elt_type (typ);
	  write_exp_string (token);
	  write_exp_elt_opcode (OP_SCOPE); 
	  if (dot_index < name.length)
	    {
	      dot_index++;
	      name.ptr += dot_index;
	      name.length -= dot_index;
	      push_fieldnames (name);
	    }
	  return;
	}
      else if (dot_index >= name.length)
	break;
      dot_index++;  /* Skip '.' */
      while (dot_index < name.length && name.ptr[dot_index] != '.')
	dot_index++;
    }
  error (_("unknown type `%.*s'"), name.length, name.ptr);
}

/* Handle Name in an expression (or LHS).
   Handle VAR, TYPE, TYPE.FIELD1....FIELDN and VAR.FIELD1....FIELDN. */

static void
push_expression_name (name)
     struct stoken name;
{
  char *tmp;
  struct type *typ;
  char *ptr;
  int i;

  for (i = 0;  i < name.length;  i++)
    {
      if (name.ptr[i] == '.')
	{
	  /* It's a Qualified Expression Name. */
	  push_qualified_expression_name (name, i);
	  return;
	}
    }

  /* It's a Simple Expression Name. */
  
  if (push_variable (name))
    return;
  tmp = copy_name (name);
  typ = java_lookup_class (tmp);
  if (typ != NULL)
    {
      write_exp_elt_opcode(OP_TYPE);
      write_exp_elt_type(typ);
      write_exp_elt_opcode(OP_TYPE);
    }
  else
    {
      struct minimal_symbol *msymbol;

      msymbol = lookup_minimal_symbol (tmp, NULL, NULL);
      if (msymbol != NULL)
	{
	  write_exp_msymbol (msymbol,
			     lookup_function_type (builtin_type_int),
			     builtin_type_int);
	}
      else if (!have_full_symbols () && !have_partial_symbols ())
	error (_("No symbol table is loaded.  Use the \"file\" command"));
      else
	error (_("No symbol \"%s\" in current context"), tmp);
    }

}


/* The following two routines, copy_exp and insert_exp, aren't specific to
   Java, so they could go in parse.c, but their only purpose is to support
   the parsing kludges we use in this file, so maybe it's best to isolate
   them here.  */

/* Copy the expression whose last element is at index ENDPOS - 1 in EXPR
   into a freshly xmalloc'ed struct expression.  Its language_defn is set
   to null.  */
static struct expression *
copy_exp (expr, endpos)
     struct expression *expr;
     int endpos;
{
  int len = length_of_subexp (expr, endpos);
  struct expression *new
    = (struct expression *) xmalloc (sizeof (*new) + EXP_ELEM_TO_BYTES (len));
  new->nelts = len;
  memcpy (new->elts, expr->elts + endpos - len, EXP_ELEM_TO_BYTES (len));
  new->language_defn = 0;

  return new;
}

/* Insert the expression NEW into the current expression (expout) at POS.  */
static void
insert_exp (pos, new)
     int pos;
     struct expression *new;
{
  int newlen = new->nelts;

  /* Grow expout if necessary.  In this function's only use at present,
     this should never be necessary.  */
  if (expout_ptr + newlen > expout_size)
    {
      expout_size = max (expout_size * 2, expout_ptr + newlen + 10);
      expout = (struct expression *)
	xrealloc ((char *) expout, (sizeof (struct expression)
				    + EXP_ELEM_TO_BYTES (expout_size)));
    }

  {
    int i;

    for (i = expout_ptr - 1; i >= pos; i--)
      expout->elts[i + newlen] = expout->elts[i];
  }
  
  memcpy (expout->elts + pos, new->elts, EXP_ELEM_TO_BYTES (newlen));
  expout_ptr += newlen;
}
