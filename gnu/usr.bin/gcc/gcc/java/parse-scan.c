/* A Bison parser, made from /home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y
   by GNU bison 1.35.  */

#define YYBISON 1  /* Identify Bison output.  */

# define	PLUS_TK	257
# define	MINUS_TK	258
# define	MULT_TK	259
# define	DIV_TK	260
# define	REM_TK	261
# define	LS_TK	262
# define	SRS_TK	263
# define	ZRS_TK	264
# define	AND_TK	265
# define	XOR_TK	266
# define	OR_TK	267
# define	BOOL_AND_TK	268
# define	BOOL_OR_TK	269
# define	EQ_TK	270
# define	NEQ_TK	271
# define	GT_TK	272
# define	GTE_TK	273
# define	LT_TK	274
# define	LTE_TK	275
# define	PLUS_ASSIGN_TK	276
# define	MINUS_ASSIGN_TK	277
# define	MULT_ASSIGN_TK	278
# define	DIV_ASSIGN_TK	279
# define	REM_ASSIGN_TK	280
# define	LS_ASSIGN_TK	281
# define	SRS_ASSIGN_TK	282
# define	ZRS_ASSIGN_TK	283
# define	AND_ASSIGN_TK	284
# define	XOR_ASSIGN_TK	285
# define	OR_ASSIGN_TK	286
# define	PUBLIC_TK	287
# define	PRIVATE_TK	288
# define	PROTECTED_TK	289
# define	STATIC_TK	290
# define	FINAL_TK	291
# define	SYNCHRONIZED_TK	292
# define	VOLATILE_TK	293
# define	TRANSIENT_TK	294
# define	NATIVE_TK	295
# define	PAD_TK	296
# define	ABSTRACT_TK	297
# define	MODIFIER_TK	298
# define	STRICT_TK	299
# define	DECR_TK	300
# define	INCR_TK	301
# define	DEFAULT_TK	302
# define	IF_TK	303
# define	THROW_TK	304
# define	BOOLEAN_TK	305
# define	DO_TK	306
# define	IMPLEMENTS_TK	307
# define	THROWS_TK	308
# define	BREAK_TK	309
# define	IMPORT_TK	310
# define	ELSE_TK	311
# define	INSTANCEOF_TK	312
# define	RETURN_TK	313
# define	VOID_TK	314
# define	CATCH_TK	315
# define	INTERFACE_TK	316
# define	CASE_TK	317
# define	EXTENDS_TK	318
# define	FINALLY_TK	319
# define	SUPER_TK	320
# define	WHILE_TK	321
# define	CLASS_TK	322
# define	SWITCH_TK	323
# define	CONST_TK	324
# define	TRY_TK	325
# define	FOR_TK	326
# define	NEW_TK	327
# define	CONTINUE_TK	328
# define	GOTO_TK	329
# define	PACKAGE_TK	330
# define	THIS_TK	331
# define	ASSERT_TK	332
# define	BYTE_TK	333
# define	SHORT_TK	334
# define	INT_TK	335
# define	LONG_TK	336
# define	CHAR_TK	337
# define	INTEGRAL_TK	338
# define	FLOAT_TK	339
# define	DOUBLE_TK	340
# define	FP_TK	341
# define	ID_TK	342
# define	REL_QM_TK	343
# define	REL_CL_TK	344
# define	NOT_TK	345
# define	NEG_TK	346
# define	ASSIGN_ANY_TK	347
# define	ASSIGN_TK	348
# define	OP_TK	349
# define	CP_TK	350
# define	OCB_TK	351
# define	CCB_TK	352
# define	OSB_TK	353
# define	CSB_TK	354
# define	SC_TK	355
# define	C_TK	356
# define	DOT_TK	357
# define	STRING_LIT_TK	358
# define	CHAR_LIT_TK	359
# define	INT_LIT_TK	360
# define	FP_LIT_TK	361
# define	TRUE_TK	362
# define	FALSE_TK	363
# define	BOOL_LIT_TK	364
# define	NULL_TK	365

#line 37 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"

#define JC1_LITE

#include "config.h"
#include "system.h"

#include "obstack.h"
#include "toplev.h"

#define obstack_chunk_alloc xmalloc
#define obstack_chunk_free free

extern char *input_filename;
extern FILE *finput, *out;

/* Obstack for the lexer.  */
struct obstack temporary_obstack;

/* The current parser context.  */
static struct parser_ctxt *ctxp;

/* Error and warning counts, current line number, because they're used
   elsewhere  */
int java_error_count;
int java_warning_count;
int lineno;

/* Tweak default rules when necessary.  */
static int absorber;
#define USE_ABSORBER absorber = 0

/* Keep track of the current package name.  */
static const char *package_name;

/* Keep track of whether things have be listed before.  */
static int previous_output;

/* Record modifier uses  */
static int modifier_value;

/* Record (almost) cyclomatic complexity.  */
static int complexity; 

/* Keeps track of number of bracket pairs after a variable declarator
   id.  */
static int bracket_count; 

/* Numbers anonymous classes */
static int anonymous_count;

/* This is used to record the current class context.  */
struct class_context
{
  char *name;
  struct class_context *next;
};

/* The global class context.  */
static struct class_context *current_class_context;

/* A special constant used to represent an anonymous context.  */
static const char *anonymous_context = "ANONYMOUS";

/* Count of method depth.  */
static int method_depth; 

/* Record a method declaration  */
struct method_declarator {
  const char *method_name;
  const char *args;
};
#define NEW_METHOD_DECLARATOR(D,N,A)					     \
{									     \
  (D) = 								     \
    (struct method_declarator *)xmalloc (sizeof (struct method_declarator)); \
  (D)->method_name = (N);						     \
  (D)->args = (A);							     \
}

/* Two actions for this grammar */
static int make_class_name_recursive PARAMS ((struct obstack *stack,
					      struct class_context *ctx));
static char *get_class_name PARAMS ((void));
static void report_class_declaration PARAMS ((const char *));
static void report_main_declaration PARAMS ((struct method_declarator *));
static void push_class_context PARAMS ((const char *));
static void pop_class_context PARAMS ((void));

void report PARAMS ((void)); 

#include "lex.h"
#include "parse.h"

#line 131 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
#ifndef YYSTYPE
typedef union {
  char *node;
  struct method_declarator *declarator;
  int value;			/* For modifiers */
} yystype;
# define YYSTYPE yystype
# define YYSTYPE_IS_TRIVIAL 1
#endif
#line 137 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"

extern int flag_assert;

#include "lex.c"
#ifndef YYDEBUG
# define YYDEBUG 1
#endif



#define	YYFINAL		616
#define	YYFLAG		-32768
#define	YYNTBASE	112

/* YYTRANSLATE(YYLEX) -- Bison token number corresponding to YYLEX. */
#define YYTRANSLATE(x) ((unsigned)(x) <= 365 ? yytranslate[x] : 265)

/* YYTRANSLATE[YYLEX] -- Bison token number corresponding to YYLEX. */
static const char yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
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
      16,    17,    18,    19,    20,    21,    22,    23,    24,    25,
      26,    27,    28,    29,    30,    31,    32,    33,    34,    35,
      36,    37,    38,    39,    40,    41,    42,    43,    44,    45,
      46,    47,    48,    49,    50,    51,    52,    53,    54,    55,
      56,    57,    58,    59,    60,    61,    62,    63,    64,    65,
      66,    67,    68,    69,    70,    71,    72,    73,    74,    75,
      76,    77,    78,    79,    80,    81,    82,    83,    84,    85,
      86,    87,    88,    89,    90,    91,    92,    93,    94,    95,
      96,    97,    98,    99,   100,   101,   102,   103,   104,   105,
     106,   107,   108,   109,   110,   111
};

#if YYDEBUG
static const short yyprhs[] =
{
       0,     0,     2,     4,     6,     8,    10,    12,    14,    16,
      18,    20,    22,    24,    26,    28,    30,    32,    34,    37,
      40,    42,    44,    46,    50,    52,    53,    55,    57,    59,
      62,    65,    68,    72,    74,    77,    79,    82,    86,    88,
      90,    94,   100,   102,   104,   106,   108,   111,   112,   120,
     121,   128,   129,   132,   133,   136,   138,   142,   145,   149,
     151,   154,   156,   158,   160,   162,   164,   166,   168,   170,
     172,   176,   181,   183,   187,   189,   193,   195,   199,   201,
     203,   204,   208,   212,   216,   221,   226,   230,   235,   239,
     241,   245,   248,   252,   253,   256,   258,   262,   264,   266,
     269,   271,   275,   280,   285,   291,   295,   300,   303,   307,
     311,   316,   321,   327,   335,   342,   344,   346,   347,   352,
     353,   359,   360,   366,   367,   374,   377,   381,   384,   388,
     390,   393,   395,   397,   399,   401,   403,   406,   409,   413,
     417,   422,   424,   428,   431,   435,   437,   440,   442,   444,
     446,   449,   452,   456,   458,   460,   462,   464,   466,   468,
     470,   472,   474,   476,   478,   480,   482,   484,   486,   488,
     490,   492,   494,   496,   498,   500,   502,   504,   507,   510,
     513,   516,   518,   520,   522,   524,   526,   528,   530,   536,
     544,   552,   558,   561,   565,   569,   574,   576,   579,   582,
     584,   587,   591,   594,   599,   602,   605,   607,   615,   623,
     630,   638,   645,   648,   651,   652,   654,   656,   657,   659,
     661,   665,   668,   672,   675,   679,   682,   686,   690,   696,
     700,   703,   707,   713,   719,   721,   725,   729,   734,   736,
     739,   745,   748,   750,   752,   754,   756,   760,   762,   764,
     766,   768,   770,   774,   778,   782,   786,   790,   796,   801,
     803,   808,   814,   820,   827,   828,   835,   836,   844,   848,
     852,   854,   858,   862,   866,   870,   875,   880,   885,   890,
     892,   895,   899,   902,   906,   910,   914,   918,   923,   929,
     936,   942,   949,   954,   959,   961,   963,   965,   967,   970,
     973,   975,   977,   980,   983,   985,   988,   991,   993,   996,
     999,  1001,  1007,  1012,  1017,  1023,  1025,  1029,  1033,  1037,
    1039,  1043,  1047,  1049,  1053,  1057,  1061,  1063,  1067,  1071,
    1075,  1079,  1083,  1085,  1089,  1093,  1095,  1099,  1101,  1105,
    1107,  1111,  1113,  1117,  1119,  1123,  1125,  1131,  1133,  1135,
    1139,  1141,  1143,  1145,  1147,  1149,  1151
};
static const short yyrhs[] =
{
     125,     0,   106,     0,   107,     0,   110,     0,   105,     0,
     104,     0,   111,     0,   115,     0,   116,     0,    84,     0,
      87,     0,    51,     0,   117,     0,   120,     0,   121,     0,
     117,     0,   117,     0,   115,   236,     0,   121,   236,     0,
     122,     0,   123,     0,   124,     0,   121,   103,   124,     0,
      88,     0,     0,   128,     0,   126,     0,   127,     0,   128,
     126,     0,   128,   127,     0,   126,   127,     0,   128,   126,
     127,     0,   129,     0,   126,   129,     0,   132,     0,   127,
     132,     0,    76,   121,   101,     0,   130,     0,   131,     0,
      56,   121,   101,     0,    56,   121,   103,     5,   101,     0,
     134,     0,   165,     0,   186,     0,    44,     0,   133,    44,
       0,     0,   133,    68,   124,   137,   138,   135,   140,     0,
       0,    68,   124,   137,   138,   136,   140,     0,     0,    64,
     118,     0,     0,    53,   139,     0,   119,     0,   139,   102,
     119,     0,    97,    98,     0,    97,   141,    98,     0,   142,
       0,   141,   142,     0,   143,     0,   158,     0,   160,     0,
     178,     0,   144,     0,   149,     0,   134,     0,   165,     0,
     186,     0,   114,   145,   101,     0,   133,   114,   145,   101,
       0,   146,     0,   145,   102,   146,     0,   147,     0,   147,
      94,   148,     0,   124,     0,   147,    99,   100,     0,   263,
       0,   176,     0,     0,   151,   150,   157,     0,   114,   152,
     155,     0,    60,   152,   155,     0,   133,   114,   152,   155,
       0,   133,    60,   152,   155,     0,   124,    95,    96,     0,
     124,    95,   153,    96,     0,   152,    99,   100,     0,   154,
       0,   153,   102,   154,     0,   114,   147,     0,   133,   114,
     147,     0,     0,    54,   156,     0,   118,     0,   156,   102,
     118,     0,   178,     0,   101,     0,   159,   178,     0,    44,
       0,   161,   155,   162,     0,   133,   161,   155,   162,     0,
     161,   155,   162,   101,     0,   133,   161,   155,   162,   101,
       0,   122,    95,    96,     0,   122,    95,   153,    96,     0,
      97,    98,     0,    97,   163,    98,     0,    97,   179,    98,
       0,    97,   163,   179,    98,     0,   164,    95,    96,   101,
       0,   164,    95,   232,    96,   101,     0,   121,   103,    66,
      95,   232,    96,   101,     0,   121,   103,    66,    95,    96,
     101,     0,    77,     0,    66,     0,     0,    62,   124,   166,
     171,     0,     0,   133,    62,   124,   167,   171,     0,     0,
      62,   124,   170,   168,   171,     0,     0,   133,    62,   124,
     170,   169,   171,     0,    64,   119,     0,   170,   102,   119,
       0,    97,    98,     0,    97,   172,    98,     0,   173,     0,
     172,   173,     0,   174,     0,   175,     0,   134,     0,   165,
       0,   144,     0,   151,   101,     0,    97,    98,     0,    97,
     177,    98,     0,    97,   102,    98,     0,    97,   177,   102,
      98,     0,   148,     0,   177,   102,   148,     0,    97,    98,
       0,    97,   179,    98,     0,   180,     0,   179,   180,     0,
     181,     0,   183,     0,   134,     0,   182,   101,     0,   114,
     145,     0,   133,   114,   145,     0,   185,     0,   188,     0,
     192,     0,   193,     0,   202,     0,   206,     0,   185,     0,
     189,     0,   194,     0,   203,     0,   207,     0,   178,     0,
     186,     0,   190,     0,   195,     0,   205,     0,   213,     0,
     214,     0,   215,     0,   218,     0,   216,     0,   220,     0,
     217,     0,   101,     0,   124,    90,     0,   187,   183,     0,
     187,   184,     0,   191,   101,     0,   260,     0,   244,     0,
     245,     0,   241,     0,   242,     0,   238,     0,   227,     0,
      49,    95,   263,    96,   183,     0,    49,    95,   263,    96,
     184,    57,   183,     0,    49,    95,   263,    96,   184,    57,
     184,     0,    69,    95,   263,    96,   196,     0,    97,    98,
       0,    97,   199,    98,     0,    97,   197,    98,     0,    97,
     197,   199,    98,     0,   198,     0,   197,   198,     0,   199,
     179,     0,   200,     0,   199,   200,     0,    63,   264,    90,
       0,    48,    90,     0,    67,    95,   263,    96,     0,   201,
     183,     0,   201,   184,     0,    52,     0,   204,   183,    67,
      95,   263,    96,   101,     0,   209,   101,   263,   101,   211,
      96,   183,     0,   209,   101,   101,   211,    96,   183,     0,
     209,   101,   263,   101,   211,    96,   184,     0,   209,   101,
     101,   211,    96,   184,     0,    72,    95,     0,   208,   210,
       0,     0,   212,     0,   182,     0,     0,   212,     0,   191,
       0,   212,   102,   191,     0,    55,   101,     0,    55,   124,
     101,     0,    74,   101,     0,    74,   124,   101,     0,    59,
     101,     0,    59,   263,   101,     0,    50,   263,   101,     0,
      78,   263,    90,   263,   101,     0,    78,   263,   101,     0,
      78,     1,     0,    78,   263,     1,     0,   219,    95,   263,
      96,   178,     0,   219,    95,   263,    96,     1,     0,    44,
       0,    71,   178,   221,     0,    71,   178,   223,     0,    71,
     178,   221,   223,     0,   222,     0,   221,   222,     0,    61,
      95,   154,    96,   178,     0,    65,   178,     0,   225,     0,
     233,     0,   113,     0,    77,     0,    95,   263,    96,     0,
     227,     0,   237,     0,   238,     0,   239,     0,   226,     0,
     121,   103,    77,     0,   121,   103,    68,     0,   120,   103,
      68,     0,   115,   103,    68,     0,    60,   103,    68,     0,
      73,   118,    95,   232,    96,     0,    73,   118,    95,    96,
       0,   228,     0,   231,   124,    95,    96,     0,   231,   124,
      95,    96,   140,     0,   231,   124,    95,   232,    96,     0,
     231,   124,    95,   232,    96,   140,     0,     0,    73,   118,
      95,    96,   229,   140,     0,     0,    73,   118,    95,   232,
      96,   230,   140,     0,   121,   103,    73,     0,   224,   103,
      73,     0,   263,     0,   232,   102,   263,     0,   232,   102,
       1,     0,    73,   115,   234,     0,    73,   117,   234,     0,
      73,   115,   234,   236,     0,    73,   117,   234,   236,     0,
      73,   117,   236,   176,     0,    73,   115,   236,   176,     0,
     235,     0,   234,   235,     0,    99,   263,   100,     0,    99,
     100,     0,   236,    99,   100,     0,   224,   103,   124,     0,
      66,   103,   124,     0,   121,    95,    96,     0,   121,    95,
     232,    96,     0,   224,   103,   124,    95,    96,     0,   224,
     103,   124,    95,   232,    96,     0,    66,   103,   124,    95,
      96,     0,    66,   103,   124,    95,   232,    96,     0,   121,
      99,   263,   100,     0,   225,    99,   263,   100,     0,   224,
       0,   121,     0,   241,     0,   242,     0,   240,    47,     0,
     240,    46,     0,   244,     0,   245,     0,     3,   243,     0,
       4,   243,     0,   246,     0,    47,   243,     0,    46,   243,
       0,   240,     0,    91,   243,     0,    92,   243,     0,   247,
       0,    95,   115,   236,    96,   243,     0,    95,   115,    96,
     243,     0,    95,   263,    96,   246,     0,    95,   121,   236,
      96,   246,     0,   243,     0,   248,     5,   243,     0,   248,
       6,   243,     0,   248,     7,   243,     0,   248,     0,   249,
       3,   248,     0,   249,     4,   248,     0,   249,     0,   250,
       8,   249,     0,   250,     9,   249,     0,   250,    10,   249,
       0,   250,     0,   251,    20,   250,     0,   251,    18,   250,
       0,   251,    21,   250,     0,   251,    19,   250,     0,   251,
      58,   116,     0,   251,     0,   252,    16,   251,     0,   252,
      17,   251,     0,   252,     0,   253,    11,   252,     0,   253,
       0,   254,    12,   253,     0,   254,     0,   255,    13,   254,
       0,   255,     0,   256,    14,   255,     0,   256,     0,   257,
      15,   256,     0,   257,     0,   257,    89,   263,    90,   258,
       0,   258,     0,   260,     0,   261,   262,   259,     0,   121,
       0,   237,     0,   239,     0,    93,     0,    94,     0,   259,
       0,   263,     0
};

#endif

#if YYDEBUG
/* YYRLINE[YYN] -- source line where rule number YYN was defined. */
static const short yyrline[] =
{
       0,   210,   215,   217,   218,   219,   220,   221,   225,   227,
     230,   236,   241,   248,   250,   253,   257,   261,   265,   271,
     279,   281,   284,   288,   295,   300,   301,   302,   303,   304,
     305,   306,   307,   310,   312,   315,   317,   320,   325,   327,
     330,   334,   338,   340,   341,   347,   356,   367,   367,   374,
     374,   379,   380,   383,   384,   387,   390,   394,   397,   401,
     403,   406,   408,   409,   410,   413,   415,   416,   417,   418,
     422,   425,   429,   432,   435,   437,   440,   443,   447,   449,
     453,   453,   460,   463,   464,   466,   473,   480,   486,   489,
     491,   497,   513,   529,   530,   533,   536,   540,   542,   546,
     550,   560,   562,   565,   567,   573,   576,   580,   582,   583,
     584,   588,   590,   593,   595,   599,   601,   606,   606,   610,
     610,   613,   613,   616,   616,   621,   623,   626,   629,   633,
     635,   638,   640,   641,   642,   645,   649,   654,   656,   657,
     658,   661,   663,   667,   669,   672,   674,   677,   679,   680,
     683,   687,   690,   694,   696,   697,   698,   699,   700,   703,
     705,   706,   707,   708,   711,   713,   714,   715,   716,   717,
     718,   719,   720,   721,   722,   723,   726,   730,   735,   739,
     745,   749,   751,   752,   753,   754,   755,   756,   759,   763,
     768,   773,   777,   779,   780,   781,   784,   786,   789,   794,
     796,   799,   801,   804,   808,   812,   816,   820,   825,   827,
     830,   832,   835,   839,   842,   843,   844,   847,   848,   851,
     853,   856,   858,   863,   865,   868,   870,   873,   877,   879,
     880,   882,   885,   887,   890,   895,   897,   898,   901,   903,
     906,   910,   915,   917,   920,   922,   923,   924,   925,   926,
     927,   928,   932,   936,   939,   941,   943,   947,   949,   950,
     951,   952,   953,   954,   957,   957,   961,   961,   966,   969,
     972,   974,   975,   978,   980,   981,   982,   985,   986,   989,
     991,   994,   998,  1001,  1005,  1007,  1013,  1016,  1018,  1019,
    1020,  1021,  1024,  1027,  1030,  1032,  1034,  1035,  1038,  1042,
    1046,  1048,  1049,  1050,  1051,  1054,  1058,  1062,  1064,  1065,
    1066,  1069,  1071,  1072,  1073,  1076,  1078,  1079,  1080,  1083,
    1085,  1086,  1089,  1091,  1092,  1093,  1096,  1098,  1099,  1100,
    1101,  1102,  1105,  1107,  1108,  1111,  1113,  1116,  1118,  1121,
    1123,  1126,  1128,  1132,  1134,  1138,  1140,  1144,  1146,  1149,
    1153,  1156,  1157,  1160,  1162,  1165,  1169
};
#endif


#if (YYDEBUG) || defined YYERROR_VERBOSE

/* YYTNAME[TOKEN_NUM] -- String name of the token TOKEN_NUM. */
static const char *const yytname[] =
{
  "$", "error", "$undefined.", "PLUS_TK", "MINUS_TK", "MULT_TK", "DIV_TK", 
  "REM_TK", "LS_TK", "SRS_TK", "ZRS_TK", "AND_TK", "XOR_TK", "OR_TK", 
  "BOOL_AND_TK", "BOOL_OR_TK", "EQ_TK", "NEQ_TK", "GT_TK", "GTE_TK", 
  "LT_TK", "LTE_TK", "PLUS_ASSIGN_TK", "MINUS_ASSIGN_TK", 
  "MULT_ASSIGN_TK", "DIV_ASSIGN_TK", "REM_ASSIGN_TK", "LS_ASSIGN_TK", 
  "SRS_ASSIGN_TK", "ZRS_ASSIGN_TK", "AND_ASSIGN_TK", "XOR_ASSIGN_TK", 
  "OR_ASSIGN_TK", "PUBLIC_TK", "PRIVATE_TK", "PROTECTED_TK", "STATIC_TK", 
  "FINAL_TK", "SYNCHRONIZED_TK", "VOLATILE_TK", "TRANSIENT_TK", 
  "NATIVE_TK", "PAD_TK", "ABSTRACT_TK", "MODIFIER_TK", "STRICT_TK", 
  "DECR_TK", "INCR_TK", "DEFAULT_TK", "IF_TK", "THROW_TK", "BOOLEAN_TK", 
  "DO_TK", "IMPLEMENTS_TK", "THROWS_TK", "BREAK_TK", "IMPORT_TK", 
  "ELSE_TK", "INSTANCEOF_TK", "RETURN_TK", "VOID_TK", "CATCH_TK", 
  "INTERFACE_TK", "CASE_TK", "EXTENDS_TK", "FINALLY_TK", "SUPER_TK", 
  "WHILE_TK", "CLASS_TK", "SWITCH_TK", "CONST_TK", "TRY_TK", "FOR_TK", 
  "NEW_TK", "CONTINUE_TK", "GOTO_TK", "PACKAGE_TK", "THIS_TK", 
  "ASSERT_TK", "BYTE_TK", "SHORT_TK", "INT_TK", "LONG_TK", "CHAR_TK", 
  "INTEGRAL_TK", "FLOAT_TK", "DOUBLE_TK", "FP_TK", "ID_TK", "REL_QM_TK", 
  "REL_CL_TK", "NOT_TK", "NEG_TK", "ASSIGN_ANY_TK", "ASSIGN_TK", "OP_TK", 
  "CP_TK", "OCB_TK", "CCB_TK", "OSB_TK", "CSB_TK", "SC_TK", "C_TK", 
  "DOT_TK", "STRING_LIT_TK", "CHAR_LIT_TK", "INT_LIT_TK", "FP_LIT_TK", 
  "TRUE_TK", "FALSE_TK", "BOOL_LIT_TK", "NULL_TK", "goal", "literal", 
  "type", "primitive_type", "reference_type", "class_or_interface_type", 
  "class_type", "interface_type", "array_type", "name", "simple_name", 
  "qualified_name", "identifier", "compilation_unit", 
  "import_declarations", "type_declarations", "package_declaration", 
  "import_declaration", "single_type_import_declaration", 
  "type_import_on_demand_declaration", "type_declaration", "modifiers", 
  "class_declaration", "@1", "@2", "super", "interfaces", 
  "interface_type_list", "class_body", "class_body_declarations", 
  "class_body_declaration", "class_member_declaration", 
  "field_declaration", "variable_declarators", "variable_declarator", 
  "variable_declarator_id", "variable_initializer", "method_declaration", 
  "@3", "method_header", "method_declarator", "formal_parameter_list", 
  "formal_parameter", "throws", "class_type_list", "method_body", 
  "static_initializer", "static", "constructor_declaration", 
  "constructor_declarator", "constructor_body", 
  "explicit_constructor_invocation", "this_or_super", 
  "interface_declaration", "@4", "@5", "@6", "@7", "extends_interfaces", 
  "interface_body", "interface_member_declarations", 
  "interface_member_declaration", "constant_declaration", 
  "abstract_method_declaration", "array_initializer", 
  "variable_initializers", "block", "block_statements", "block_statement", 
  "local_variable_declaration_statement", "local_variable_declaration", 
  "statement", "statement_nsi", "statement_without_trailing_substatement", 
  "empty_statement", "label_decl", "labeled_statement", 
  "labeled_statement_nsi", "expression_statement", "statement_expression", 
  "if_then_statement", "if_then_else_statement", 
  "if_then_else_statement_nsi", "switch_statement", "switch_block", 
  "switch_block_statement_groups", "switch_block_statement_group", 
  "switch_labels", "switch_label", "while_expression", "while_statement", 
  "while_statement_nsi", "do_statement_begin", "do_statement", 
  "for_statement", "for_statement_nsi", "for_header", "for_begin", 
  "for_init", "for_update", "statement_expression_list", 
  "break_statement", "continue_statement", "return_statement", 
  "throw_statement", "assert_statement", "synchronized_statement", 
  "synchronized", "try_statement", "catches", "catch_clause", "finally", 
  "primary", "primary_no_new_array", "type_literals", 
  "class_instance_creation_expression", "anonymous_class_creation", "@8", 
  "@9", "something_dot_new", "argument_list", "array_creation_expression", 
  "dim_exprs", "dim_expr", "dims", "field_access", "method_invocation", 
  "array_access", "postfix_expression", "post_increment_expression", 
  "post_decrement_expression", "unary_expression", 
  "pre_increment_expression", "pre_decrement_expression", 
  "unary_expression_not_plus_minus", "cast_expression", 
  "multiplicative_expression", "additive_expression", "shift_expression", 
  "relational_expression", "equality_expression", "and_expression", 
  "exclusive_or_expression", "inclusive_or_expression", 
  "conditional_and_expression", "conditional_or_expression", 
  "conditional_expression", "assignment_expression", "assignment", 
  "left_hand_side", "assignment_operator", "expression", 
  "constant_expression", 0
};
#endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives. */
static const short yyr1[] =
{
       0,   112,   113,   113,   113,   113,   113,   113,   114,   114,
     115,   115,   115,   116,   116,   117,   118,   119,   120,   120,
     121,   121,   122,   123,   124,   125,   125,   125,   125,   125,
     125,   125,   125,   126,   126,   127,   127,   128,   129,   129,
     130,   131,   132,   132,   132,   133,   133,   135,   134,   136,
     134,   137,   137,   138,   138,   139,   139,   140,   140,   141,
     141,   142,   142,   142,   142,   143,   143,   143,   143,   143,
     144,   144,   145,   145,   146,   146,   147,   147,   148,   148,
     150,   149,   151,   151,   151,   151,   152,   152,   152,   153,
     153,   154,   154,   155,   155,   156,   156,   157,   157,   158,
     159,   160,   160,   160,   160,   161,   161,   162,   162,   162,
     162,   163,   163,   163,   163,   164,   164,   166,   165,   167,
     165,   168,   165,   169,   165,   170,   170,   171,   171,   172,
     172,   173,   173,   173,   173,   174,   175,   176,   176,   176,
     176,   177,   177,   178,   178,   179,   179,   180,   180,   180,
     181,   182,   182,   183,   183,   183,   183,   183,   183,   184,
     184,   184,   184,   184,   185,   185,   185,   185,   185,   185,
     185,   185,   185,   185,   185,   185,   186,   187,   188,   189,
     190,   191,   191,   191,   191,   191,   191,   191,   192,   193,
     194,   195,   196,   196,   196,   196,   197,   197,   198,   199,
     199,   200,   200,   201,   202,   203,   204,   205,   206,   206,
     207,   207,   208,   209,   210,   210,   210,   211,   211,   212,
     212,   213,   213,   214,   214,   215,   215,   216,   217,   217,
     217,   217,   218,   218,   219,   220,   220,   220,   221,   221,
     222,   223,   224,   224,   225,   225,   225,   225,   225,   225,
     225,   225,   225,   226,   226,   226,   226,   227,   227,   227,
     227,   227,   227,   227,   229,   228,   230,   228,   231,   231,
     232,   232,   232,   233,   233,   233,   233,   233,   233,   234,
     234,   235,   236,   236,   237,   237,   238,   238,   238,   238,
     238,   238,   239,   239,   240,   240,   240,   240,   241,   242,
     243,   243,   243,   243,   243,   244,   245,   246,   246,   246,
     246,   247,   247,   247,   247,   248,   248,   248,   248,   249,
     249,   249,   250,   250,   250,   250,   251,   251,   251,   251,
     251,   251,   252,   252,   252,   253,   253,   254,   254,   255,
     255,   256,   256,   257,   257,   258,   258,   259,   259,   260,
     261,   261,   261,   262,   262,   263,   264
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN. */
static const short yyr2[] =
{
       0,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     2,     2,
       1,     1,     1,     3,     1,     0,     1,     1,     1,     2,
       2,     2,     3,     1,     2,     1,     2,     3,     1,     1,
       3,     5,     1,     1,     1,     1,     2,     0,     7,     0,
       6,     0,     2,     0,     2,     1,     3,     2,     3,     1,
       2,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       3,     4,     1,     3,     1,     3,     1,     3,     1,     1,
       0,     3,     3,     3,     4,     4,     3,     4,     3,     1,
       3,     2,     3,     0,     2,     1,     3,     1,     1,     2,
       1,     3,     4,     4,     5,     3,     4,     2,     3,     3,
       4,     4,     5,     7,     6,     1,     1,     0,     4,     0,
       5,     0,     5,     0,     6,     2,     3,     2,     3,     1,
       2,     1,     1,     1,     1,     1,     2,     2,     3,     3,
       4,     1,     3,     2,     3,     1,     2,     1,     1,     1,
       2,     2,     3,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     2,     2,     2,
       2,     1,     1,     1,     1,     1,     1,     1,     5,     7,
       7,     5,     2,     3,     3,     4,     1,     2,     2,     1,
       2,     3,     2,     4,     2,     2,     1,     7,     7,     6,
       7,     6,     2,     2,     0,     1,     1,     0,     1,     1,
       3,     2,     3,     2,     3,     2,     3,     3,     5,     3,
       2,     3,     5,     5,     1,     3,     3,     4,     1,     2,
       5,     2,     1,     1,     1,     1,     3,     1,     1,     1,
       1,     1,     3,     3,     3,     3,     3,     5,     4,     1,
       4,     5,     5,     6,     0,     6,     0,     7,     3,     3,
       1,     3,     3,     3,     3,     4,     4,     4,     4,     1,
       2,     3,     2,     3,     3,     3,     3,     4,     5,     6,
       5,     6,     4,     4,     1,     1,     1,     1,     2,     2,
       1,     1,     2,     2,     1,     2,     2,     1,     2,     2,
       1,     5,     4,     4,     5,     1,     3,     3,     3,     1,
       3,     3,     1,     3,     3,     3,     1,     3,     3,     3,
       3,     3,     1,     3,     3,     1,     3,     1,     3,     1,
       3,     1,     3,     1,     3,     1,     5,     1,     1,     3,
       1,     1,     1,     1,     1,     1,     1
};

/* YYDEFACT[S] -- default rule to reduce with in state S when YYTABLE
   doesn't specify something else to do.  Zero means the default is an
   error. */
static const short yydefact[] =
{
      25,    45,     0,     0,     0,     0,   176,     1,    27,    28,
      26,    33,    38,    39,    35,     0,    42,    43,    44,    24,
       0,    20,    21,    22,   117,    51,     0,    31,    34,    36,
      29,    30,    46,     0,     0,    40,     0,     0,     0,   121,
       0,    53,    37,     0,    32,   119,    51,     0,    23,    17,
     125,    15,     0,   118,     0,     0,    16,    52,     0,    49,
       0,   123,    53,    41,    12,     0,    10,    11,   127,     0,
       8,     9,    13,    14,    15,     0,   133,   135,     0,   134,
       0,   129,   131,   132,   126,   122,    55,    54,     0,   120,
       0,    47,     0,    93,    76,     0,    72,    74,    93,     0,
      18,    19,     0,     0,   136,   128,   130,     0,     0,    50,
     124,     0,     0,     0,     0,    83,    70,     0,     0,     0,
      82,   282,     0,    93,     0,    93,    56,    45,     0,    57,
      20,     0,    67,     0,    59,    61,    65,    66,    80,    62,
       0,    63,    93,    68,    64,    69,    48,    86,     0,     0,
       0,    89,    95,    94,    88,    76,    73,     0,     0,     0,
       0,     0,     0,     0,   245,     0,     0,     0,     0,     6,
       5,     2,     3,     4,     7,   244,     0,     0,   295,    75,
      79,   294,   242,   251,   247,   259,     0,   243,   248,   249,
     250,   307,   296,   297,   315,   300,   301,   304,   310,   319,
     322,   326,   332,   335,   337,   339,   341,   343,   345,   347,
     355,   348,     0,    78,    77,   283,    85,    71,    84,    45,
       0,     0,   206,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   143,     0,     8,    14,   295,    22,     0,   149,
     164,     0,   145,   147,     0,   148,   153,   165,     0,   154,
     166,     0,   155,   156,   167,     0,   157,     0,   168,   158,
     214,     0,   169,   170,   171,   173,   175,   172,     0,   174,
     247,   249,     0,   184,   185,   182,   183,   181,     0,    93,
      58,    60,     0,    99,     0,    91,     0,    87,     0,     0,
     295,   248,   250,   302,   303,   306,   305,     0,     0,     0,
      16,     0,   308,   309,     0,   295,     0,   137,     0,   141,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   299,
     298,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   353,   354,     0,     0,     0,   221,     0,   225,
       0,     0,     0,     0,   212,   223,     0,   230,     0,     0,
     151,   177,     0,   144,   146,   150,   234,   178,   180,   204,
       0,     0,   216,   219,   213,   215,     0,     0,   105,     0,
       0,    98,    81,    97,     0,   101,    92,    90,    96,   256,
     285,     0,   273,   279,     0,   274,     0,     0,     0,    18,
      19,   246,   139,   138,     0,   255,   254,   286,     0,   270,
       0,   253,   268,   252,   269,   284,     0,     0,   316,   317,
     318,   320,   321,   323,   324,   325,   328,   330,   327,   329,
       0,   331,   333,   334,   336,   338,   340,   342,   344,     0,
     349,     0,   227,   222,   226,     0,     0,     0,     0,   235,
     238,   236,   224,   231,     0,   229,   246,   152,     0,     0,
     217,     0,     0,   106,   102,   116,   245,   107,   295,     0,
       0,     0,   103,     0,     0,   280,   275,   278,   276,   277,
     258,     0,   312,     0,     0,   313,   140,   142,   287,     0,
     292,     0,   293,   260,     0,     0,     0,   203,     0,     0,
     241,   239,   237,     0,     0,   220,     0,   218,   217,     0,
     104,     0,   108,     0,     0,   109,   290,     0,   281,     0,
     257,   311,   314,   272,   271,   288,     0,   261,   262,   346,
       0,   188,     0,   153,     0,   160,   161,     0,   162,   163,
       0,     0,   191,     0,   228,     0,     0,     0,   233,   232,
       0,   110,     0,     0,   291,   265,     0,   289,   263,     0,
       0,   179,   205,     0,     0,     0,   192,     0,   196,     0,
     199,     0,     0,   209,     0,     0,   111,     0,   267,     0,
     189,   217,     0,   202,   356,     0,   194,   197,     0,   193,
     198,   200,   240,   207,   208,     0,     0,   112,     0,     0,
     217,   201,   195,   114,     0,     0,     0,     0,   113,     0,
     211,     0,   190,   210,     0,     0,     0
};

static const short yydefgoto[] =
{
     614,   175,   233,   176,    71,    72,    57,    50,   177,   178,
      21,    22,    23,     7,     8,     9,    10,    11,    12,    13,
      14,   238,   239,   111,    88,    41,    59,    87,   109,   133,
     134,   135,    77,    95,    96,    97,   179,   137,   282,    78,
      93,   150,   151,   115,   153,   382,   139,   140,   141,   142,
     385,   469,   470,    17,    38,    60,    55,    90,    39,    53,
      80,    81,    82,    83,   180,   310,   240,   590,   242,   243,
     244,   245,   532,   246,   247,   248,   249,   535,   250,   251,
     252,   253,   536,   254,   542,   567,   568,   569,   570,   255,
     256,   538,   257,   258,   259,   539,   260,   261,   374,   506,
     507,   262,   263,   264,   265,   266,   267,   268,   269,   449,
     450,   451,   181,   182,   183,   184,   185,   519,   556,   186,
     408,   187,   392,   393,   101,   188,   189,   190,   191,   192,
     193,   194,   195,   196,   197,   198,   199,   200,   201,   202,
     203,   204,   205,   206,   207,   208,   209,   210,   211,   212,
     344,   409,   585
};

static const short yypact[] =
{
     128,-32768,   -71,   -71,   -71,   -71,-32768,-32768,   187,    87,
     187,-32768,-32768,-32768,-32768,   150,-32768,-32768,-32768,-32768,
     213,-32768,-32768,-32768,   -40,   -26,   303,    87,-32768,-32768,
     187,    87,-32768,   -71,   -71,-32768,     7,   -71,   -37,   -28,
     -71,    68,-32768,   -71,    87,   -40,   -26,    60,-32768,-32768,
  -32768,   114,   906,-32768,   -71,   -37,-32768,-32768,   -71,-32768,
     -37,   -28,    68,-32768,-32768,   -71,-32768,-32768,-32768,   -71,
     110,-32768,-32768,-32768,   136,   369,-32768,-32768,   139,-32768,
    1354,-32768,-32768,-32768,-32768,-32768,-32768,    28,   161,-32768,
     -37,-32768,   175,   -34,   175,   344,-32768,    86,   -34,   177,
     212,   212,   -71,   -71,-32768,-32768,-32768,   -71,  1200,-32768,
  -32768,   161,   407,   -71,   191,-32768,-32768,   -71,  1604,   255,
  -32768,-32768,   282,   -34,   353,   -34,-32768,   227,  2516,-32768,
     253,   369,-32768,  1359,-32768,-32768,-32768,-32768,-32768,-32768,
     315,-32768,   367,-32768,-32768,-32768,-32768,-32768,   -71,   202,
      16,-32768,-32768,   330,-32768,-32768,-32768,  2312,  2312,  2312,
    2312,   336,   345,   243,-32768,  2312,  2312,  2312,  1472,-32768,
  -32768,-32768,-32768,-32768,-32768,-32768,   181,   347,   323,-32768,
  -32768,   361,   360,-32768,-32768,-32768,   -71,-32768,   368,-32768,
     395,   450,-32768,-32768,-32768,-32768,-32768,-32768,-32768,   429,
     533,   471,   455,   542,   472,   480,   489,   491,     0,-32768,
  -32768,-32768,   476,-32768,-32768,-32768,-32768,-32768,-32768,   421,
     430,  2312,-32768,   -19,  1656,   434,   437,   315,   443,    66,
    1219,  2312,-32768,   -71,   181,   347,   612,   451,    90,-32768,
  -32768,  2584,-32768,-32768,   445,-32768,-32768,-32768,  2992,-32768,
  -32768,   454,-32768,-32768,-32768,  2992,-32768,  2992,-32768,-32768,
    3112,   461,-32768,-32768,-32768,-32768,-32768,-32768,   457,-32768,
     171,   208,   450,   528,   530,-32768,-32768,-32768,   496,   367,
  -32768,-32768,   224,-32768,   467,   468,   -71,-32768,   248,   -71,
     133,-32768,-32768,-32768,-32768,-32768,-32768,   504,   -71,   483,
     483,   493,-32768,-32768,   160,   323,   490,-32768,   498,-32768,
     239,   521,   523,  1722,  1774,   266,    50,  2312,   499,-32768,
  -32768,  2312,  2312,  2312,  2312,  2312,  2312,  2312,  2312,  2312,
    2312,  2312,  2312,   243,  2312,  2312,  2312,  2312,  2312,  2312,
    2312,  2312,-32768,-32768,  2312,  2312,   500,-32768,   501,-32768,
     506,  2312,  2312,   295,-32768,-32768,   511,-32768,    18,   509,
     513,-32768,   -71,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
     551,   202,-32768,-32768,-32768,   518,  1840,  2312,-32768,    48,
     467,-32768,-32768,-32768,  2652,   520,   468,-32768,-32768,-32768,
     529,  1774,   483,-32768,   310,   483,   310,  1892,  2312,   -13,
     169,  3179,-32768,-32768,  1538,-32768,-32768,-32768,    74,-32768,
     532,-32768,-32768,-32768,-32768,   538,   534,  1958,-32768,-32768,
  -32768,   429,   429,   533,   533,   533,   471,   471,   471,   471,
     110,-32768,   455,   455,   542,   472,   480,   489,   491,   545,
  -32768,   527,-32768,-32768,-32768,   540,   547,   544,   315,   295,
  -32768,-32768,-32768,-32768,  2312,-32768,-32768,   513,   546,  3137,
    3137,   537,   550,-32768,   548,   345,   555,-32768,   658,  2720,
     556,  2788,-32768,  2010,   552,-32768,   212,-32768,   212,-32768,
     558,    99,-32768,  2312,  3179,-32768,-32768,-32768,-32768,  1393,
  -32768,  2076,-32768,   161,   106,  2312,  3060,-32768,   559,   248,
  -32768,-32768,-32768,   564,  2312,-32768,   557,   518,  3137,     8,
  -32768,   317,-32768,  2856,  2128,-32768,-32768,   250,-32768,   161,
     570,-32768,-32768,-32768,-32768,-32768,   290,-32768,   161,-32768,
     573,-32768,   613,   615,  3060,-32768,-32768,  3060,-32768,-32768,
     575,   -22,-32768,   578,-32768,   581,  2992,   582,-32768,-32768,
     584,-32768,   579,   291,-32768,-32768,   161,-32768,-32768,  2312,
    2992,-32768,-32768,  2194,   591,  2312,-32768,    94,-32768,  2380,
  -32768,   315,   583,-32768,  2992,  2246,-32768,   585,-32768,   586,
  -32768,  3137,   592,-32768,-32768,   597,-32768,-32768,  2448,-32768,
    2924,-32768,-32768,-32768,-32768,   594,   301,-32768,  3060,   596,
    3137,-32768,-32768,-32768,   607,   641,  3060,   620,-32768,  3060,
  -32768,  3060,-32768,-32768,   709,   718,-32768
};

static const short yypgoto[] =
{
  -32768,-32768,   111,   -27,   387,    30,  -107,    10,   189,    40,
     117,-32768,    -3,-32768,   711,    13,-32768,    21,-32768,-32768,
      19,    27,   617,-32768,-32768,   677,   662,-32768,  -109,-32768,
     595,-32768,   -86,  -100,   610,  -140,  -157,-32768,-32768,    -5,
      63,   452,  -283,   -64,-32768,-32768,-32768,-32768,-32768,   598,
     357,-32768,-32768,   -36,-32768,-32768,-32768,-32768,   691,   167,
  -32768,   661,-32768,-32768,    29,-32768,  -101,  -118,  -237,-32768,
     485,  -146,  -313,  -482,   704,  -447,-32768,-32768,-32768,  -185,
  -32768,-32768,-32768,-32768,-32768,-32768,   180,   182,  -413,   -94,
  -32768,-32768,-32768,-32768,-32768,-32768,-32768,    31,-32768,  -490,
     495,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
     307,   311,-32768,-32768,-32768,    85,-32768,-32768,-32768,-32768,
    -304,-32768,   459,  -113,     3,  1033,   230,  1053,   273,   356,
     462,   341,   543,   626,  -388,-32768,   254,   192,   219,   265,
     426,   428,   425,   427,   431,-32768,   275,   424,   669,-32768,
  -32768,   785,-32768
};


#define	YYLAST		3290


static const short yytable[] =
{
      24,    25,   146,   124,   364,   387,   152,   144,   285,   548,
     241,   309,    47,   485,   533,   340,    79,    19,   547,   453,
     113,    27,   136,    31,    37,    70,   564,    15,    29,    28,
      45,    46,   144,    48,   120,    15,    15,    15,    40,   283,
      48,   565,    20,    44,    79,    26,    29,   136,    70,   534,
      29,    28,   533,    70,    15,   533,   301,    15,    15,   216,
      52,   218,    92,    29,    84,   114,    94,    49,    86,    19,
      56,    15,   143,   100,    54,   373,   566,    51,   284,    75,
      51,    70,   347,   483,    49,    70,   122,   534,    49,   341,
     534,   599,    74,   481,    51,    19,   522,   143,    51,    92,
      94,   234,   367,   138,    70,   128,    70,    75,   454,   369,
     607,   370,   287,   494,   155,    74,   533,   126,   288,   455,
      74,    58,    70,   414,   533,   237,   353,   533,   138,   533,
     107,     1,    98,   360,    32,   131,   299,    49,    19,   149,
     304,    64,   564,    56,   463,   155,   386,    51,    74,     3,
     288,   534,    74,    51,    19,     4,   591,   565,    34,   534,
     131,    63,   534,    69,   534,   123,   125,   355,   236,   517,
     488,    74,     1,    74,    66,   591,   489,    67,    19,   100,
     118,   383,   388,   318,     2,   119,   103,   526,     6,    74,
       3,    69,   586,   300,    32,   520,     4,   290,   290,   290,
     290,   489,   528,    51,     5,   290,   290,   305,   489,    99,
     553,    70,    33,   270,   234,   380,   543,    43,    34,    69,
     348,   561,    85,   148,   562,   130,   356,    89,   313,     6,
     155,     1,   314,   234,   364,    99,   315,   100,   237,    43,
     104,    73,   103,     2,    69,   237,    32,   487,   130,     3,
     130,    70,   237,    64,   237,     4,   398,   110,   108,    99,
     286,    70,   457,   311,    73,   484,   471,  -187,   122,    73,
     112,   596,  -187,  -187,   505,   373,   364,   121,    74,   475,
      99,   236,   475,   155,   311,   605,    66,   371,     6,    67,
      19,   154,     1,   610,    64,   390,   612,    73,   613,    64,
     236,    73,   394,   396,  -186,   149,   430,   399,   400,  -186,
    -186,   122,    48,   415,    35,   149,    36,   235,    74,    56,
      73,   128,    73,   373,  -100,   381,   270,    66,    74,    51,
      67,    19,    66,   270,   411,    67,    19,   403,    73,   412,
     270,   404,   270,   413,    70,   270,   554,   500,   278,   362,
     531,   513,   489,   364,    19,   214,   447,   234,   271,   155,
     448,   290,   290,   290,   290,   290,   290,   290,   290,   290,
     290,   290,   290,    74,   290,   290,   290,   290,   290,   290,
     290,   237,   215,   550,   527,   411,   557,   577,   367,   148,
     412,   369,   489,   489,   413,   476,   373,   604,   478,   148,
     573,   272,   537,   489,    42,    19,    43,   168,   549,   122,
     555,    74,   128,    32,   580,   373,  -350,  -350,   313,   558,
      64,   113,   314,   477,   468,   479,   315,    73,   594,   102,
     235,    33,   289,   100,   321,   322,   323,    34,   290,   297,
     537,   290,   234,   537,   234,   116,   117,   578,   298,   235,
     312,     1,   531,    66,   217,   117,    67,    19,    64,   317,
     573,  -351,  -351,   580,   316,   594,   237,    73,   237,   270,
     592,   271,    70,   329,   330,   331,   332,    73,   271,   326,
     327,   328,   362,   336,   273,   271,   234,   271,  -352,  -352,
     271,    66,   337,   237,    67,    19,   319,   320,   293,   294,
     295,   296,   338,   147,   537,   339,   302,   303,    48,   236,
     237,   236,   537,   333,   272,   537,  -234,   537,   423,   424,
     425,   272,    73,   290,   290,   345,   149,   540,   272,   351,
     272,   237,   352,   272,   237,   290,   324,   325,   354,    74,
       1,   361,   234,   237,   270,   270,   365,    64,   426,   427,
     428,   429,   377,   236,   270,   368,   270,   237,   334,   335,
      73,   234,   376,   234,   384,   540,   237,   119,   540,   342,
     343,   237,   389,   235,  -296,  -296,  -297,  -297,   421,   422,
      66,   270,   391,    67,    19,   237,   401,   237,   397,   405,
     274,   406,   378,   270,   417,   237,   402,   273,   270,   432,
     433,   442,   443,   237,   273,   456,   237,   444,   237,   236,
     148,   273,   452,   273,   271,   117,   273,    16,   458,   270,
     459,   472,   270,   496,   473,    16,    16,    16,   236,   540,
     236,   270,   490,   491,   492,   495,   497,   540,   508,   499,
     540,   504,   540,   498,    16,   270,   509,    16,    16,   510,
    -115,   514,   518,   546,   270,  -264,   541,   272,   235,   270,
     235,    16,   418,   419,   420,   544,   270,  -266,   559,    76,
     560,   275,  -159,   270,   571,   270,   563,   572,   574,   575,
     576,   583,   598,   270,   593,   270,   597,   601,    73,   271,
     271,   270,   606,   600,   270,   603,   270,    76,   609,   271,
     -15,   271,   235,   274,    18,  -350,  -350,   313,   608,   615,
     274,   314,    18,    18,    18,   315,   611,   274,   616,   274,
     431,    30,   274,    62,    91,   132,   271,   156,   281,   279,
     379,    18,   272,   272,    18,    18,    61,   464,   271,   482,
     273,   106,   272,   271,   272,   372,   -15,   587,    18,   588,
     132,  -350,  -350,   313,   276,   375,   501,   314,   235,   395,
     502,   511,   434,   436,   271,   435,   437,   271,   440,   272,
     529,   438,     0,     0,     0,     0,   271,   235,     0,   235,
       0,   272,     0,     0,   275,     0,   272,     0,     0,     0,
     271,   275,     0,     0,     0,     0,     0,   277,   275,   271,
     275,     0,     0,   275,   271,     0,     0,   272,     0,     0,
     272,   271,   145,     0,     0,   273,   273,     0,   271,   272,
     271,     0,     0,     0,   521,   273,     0,   273,   271,     0,
     271,     0,     0,   272,     0,     0,   271,   145,     0,   271,
       0,   271,   272,     0,     0,     0,   274,   272,     0,     0,
       0,     0,   273,     0,   272,     0,     0,     0,     0,     0,
       0,   272,     0,   272,   273,     0,     0,   276,     0,   273,
       0,   272,     0,   272,   276,     0,     0,     0,     0,   272,
       0,   276,   272,   276,   272,     0,   276,     0,     0,     0,
     273,     0,     0,   273,     0,     0,     0,     0,     0,     0,
       0,     0,   273,   213,     0,     0,     0,     0,     0,     0,
     277,     0,     0,     0,     0,     0,   273,   277,     0,     0,
       0,   274,   274,     0,   277,   273,   277,   275,     0,   277,
     273,   274,     0,   274,     0,     0,     0,   273,     0,     0,
       0,     0,     0,     0,   273,     0,   273,     0,     0,     0,
       1,     0,   306,   213,   273,     0,   273,    64,   274,     0,
       0,     0,   273,     0,     0,   273,    65,   273,     3,     0,
     274,     0,     0,     0,     4,   274,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      66,     0,     0,    67,    19,     0,   274,     0,     0,   274,
       0,     0,   275,   275,    68,     0,   346,     0,   274,   350,
     276,     0,   275,     0,   275,   358,   359,     0,     0,     0,
       0,     0,   274,     0,     0,     0,     0,     0,     0,     0,
       0,   274,     0,     0,     0,     0,   274,     0,     0,   275,
       0,     0,     0,   274,     0,     0,     0,     0,     0,     0,
     274,   275,   274,   277,     0,     0,   275,     0,     0,     0,
     274,     0,   274,     0,     0,     0,     0,     0,   274,     0,
       0,   274,     0,   274,     0,     0,     0,   275,     0,     0,
     275,     0,     0,     0,     0,   276,   276,     0,     0,   275,
       0,     0,     0,     0,     0,   276,     0,   276,     0,   410,
       0,     0,   416,   275,     0,     0,     0,     0,     0,     0,
       0,     0,   275,     0,     0,     0,     0,   275,     0,     0,
       0,     0,   276,     0,   275,     0,   439,     0,   277,   277,
     441,   275,     0,   275,   276,     0,   445,   446,   277,   276,
     277,   275,     0,   275,     0,     0,     0,     0,     0,   275,
       0,     0,   275,     0,   275,     0,     0,     0,     0,     0,
     276,   461,   462,   276,     0,   277,     0,     0,     0,     0,
       0,     0,   276,     0,     0,     0,   474,   277,     0,     0,
       0,     0,   277,     0,     0,     0,   276,     0,     0,   213,
     291,   291,   291,   291,     0,   276,     0,     0,   291,   291,
     276,     0,     0,   277,     0,     0,   277,   276,     0,     0,
     292,   292,   292,   292,   276,   277,   276,     0,   292,   292,
     357,     0,   157,   158,   276,     0,   276,     0,     0,   277,
       0,     0,   276,     0,     0,   276,     0,   276,   277,   503,
       0,     0,     0,   277,   127,     0,     0,     0,     0,     0,
     277,    64,     0,     0,     0,     0,     0,   277,     0,   277,
      65,     0,     3,     0,     0,   159,   160,   277,     4,   277,
      64,     0,     0,     0,   524,   277,     0,     0,   277,   161,
     277,     0,     0,     0,    66,   162,     0,    67,    19,   545,
       0,     0,   163,     0,     0,     0,   164,   128,   129,     0,
       0,     6,     0,    66,     0,     0,    67,    19,     0,     0,
     165,   166,     0,     0,   167,     0,     0,     0,     0,     0,
       0,     0,     0,   169,   170,   171,   172,     0,     0,   173,
     174,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   579,     0,     0,     0,   582,     0,
     584,     0,     0,     0,   291,   291,   291,   291,   291,   291,
     291,   291,   291,   291,   291,   291,     0,   291,   291,   291,
     291,   291,   291,   291,   292,   292,   292,   292,   292,   292,
     292,   292,   292,   292,   292,   292,     0,   292,   292,   292,
     292,   292,   292,   292,   523,     0,   157,   158,     1,     0,
       0,     0,     0,   127,     0,    64,     0,     0,     0,     0,
      64,     0,     0,     0,    65,     0,     3,     0,     0,    65,
       0,     3,     4,     0,     0,     0,     0,     4,     0,     0,
       0,   291,     0,     0,   291,     0,     0,     0,    66,   159,
     160,    67,    19,    66,    64,     0,    67,    19,     0,     0,
       0,   292,   105,   161,   292,     0,   128,   280,     0,   162,
       6,     0,     0,     0,     0,     0,   163,     0,     0,     0,
     164,     0,     0,     0,     0,   157,   158,    66,     0,     0,
      67,    19,     0,     0,   165,   166,     0,     0,   167,     0,
       0,     0,     0,     0,     0,     0,     0,   169,   170,   171,
     172,     0,     0,   173,   174,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   291,   291,   159,   160,
       0,     0,     0,    64,     0,     0,     0,     0,   291,     0,
       0,     0,   161,     0,     0,     0,   292,   292,   162,     0,
       0,   157,   158,     0,     0,   163,     0,     0,   292,   164,
       0,     0,     0,     0,     0,     0,    66,     0,     0,    67,
      19,     0,     0,   165,   166,     0,     0,   167,     0,   168,
     307,     0,     0,     0,   308,     0,   169,   170,   171,   172,
       0,     0,   173,   174,   159,   160,     0,     0,     0,    64,
       0,     0,     0,     0,     0,     0,     0,     0,   161,     0,
       0,     0,     0,     0,   162,     0,     0,   157,   158,     0,
       0,   163,     0,     0,     0,   164,     0,     0,     0,     0,
       0,     0,    66,     0,     0,    67,    19,     0,     0,   165,
     166,     0,     0,   167,     0,   168,   486,     0,     0,     0,
       0,     0,   169,   170,   171,   172,     0,     0,   173,   174,
     159,   160,     0,     0,     0,    64,     0,     0,     0,   157,
     158,     0,     0,     0,   161,     0,     0,     0,     0,     0,
     162,     0,     0,     0,     0,     0,     0,   163,     0,     0,
       0,   164,     0,     0,     0,     0,     0,     0,    66,     0,
       0,    67,    19,     0,     0,   165,   166,     0,     0,   167,
       0,   168,   159,   160,     0,     0,     0,    64,   169,   170,
     171,   172,     0,     0,   173,   174,   161,     0,     0,     0,
       0,     0,   162,     0,     0,   157,   158,     0,     0,   163,
       0,     0,     0,   164,     0,     0,     0,     0,     0,     0,
      66,     0,     0,    67,    19,     0,     0,   165,   166,     0,
       0,   167,     0,     0,     0,     0,     0,   349,     0,     0,
     169,   170,   171,   172,     0,     0,   173,   174,   159,   160,
       0,     0,     0,    64,     0,     0,     0,   157,   158,     0,
       0,     0,   161,     0,     0,     0,     0,     0,   162,     0,
       0,     0,     0,     0,     0,   163,     0,     0,     0,   164,
       0,     0,     0,     0,     0,     0,    66,     0,     0,    67,
      19,     0,     0,   165,   166,     0,     0,   167,   407,     0,
     159,   160,     0,     0,     0,    64,   169,   170,   171,   172,
       0,     0,   173,   174,   161,     0,     0,     0,     0,     0,
     162,     0,     0,   157,   158,     0,     0,   163,     0,     0,
       0,   164,     0,     0,     0,     0,     0,     0,    66,     0,
       0,    67,    19,     0,     0,   165,   166,     0,     0,   167,
       0,     0,     0,     0,   121,     0,     0,     0,   169,   170,
     171,   172,     0,     0,   173,   174,   159,   160,     0,     0,
       0,    64,     0,     0,     0,   157,   158,     0,     0,     0,
     161,     0,     0,     0,     0,     0,   162,     0,     0,     0,
       0,     0,     0,   163,     0,     0,     0,   164,     0,     0,
       0,     0,     0,     0,    66,     0,     0,    67,    19,     0,
       0,   165,   166,     0,     0,   167,     0,     0,   159,   160,
       0,   460,     0,    64,   169,   170,   171,   172,     0,     0,
     173,   174,   161,     0,     0,     0,     0,     0,   162,     0,
       0,   157,   158,     0,     0,   163,     0,     0,     0,   164,
       0,     0,     0,     0,     0,     0,    66,     0,     0,    67,
      19,     0,     0,   165,   166,     0,     0,   167,   480,     0,
       0,     0,     0,     0,     0,     0,   169,   170,   171,   172,
       0,     0,   173,   174,   159,   160,     0,     0,     0,    64,
       0,     0,     0,   157,   158,     0,     0,     0,   161,     0,
       0,     0,     0,     0,   162,     0,     0,     0,     0,     0,
       0,   163,     0,     0,     0,   164,     0,     0,     0,     0,
       0,     0,    66,     0,     0,    67,    19,     0,     0,   165,
     166,     0,     0,   167,   493,     0,   159,   160,     0,     0,
       0,    64,   169,   170,   171,   172,     0,     0,   173,   174,
     161,     0,     0,     0,     0,     0,   162,     0,     0,   157,
     158,     0,     0,   163,     0,     0,     0,   164,     0,     0,
       0,     0,     0,     0,    66,     0,     0,    67,    19,     0,
       0,   165,   166,     0,     0,   167,   516,     0,     0,     0,
       0,     0,     0,     0,   169,   170,   171,   172,     0,     0,
     173,   174,   159,   160,     0,     0,     0,    64,     0,     0,
       0,   157,   158,     0,     0,     0,   161,     0,     0,     0,
       0,     0,   162,     0,     0,     0,     0,     0,     0,   163,
       0,     0,     0,   164,     0,     0,     0,     0,     0,     0,
      66,     0,     0,    67,    19,     0,     0,   165,   166,     0,
       0,   167,   525,     0,   159,   160,     0,     0,     0,    64,
     169,   170,   171,   172,     0,     0,   173,   174,   161,     0,
       0,     0,     0,     0,   162,     0,     0,   157,   158,     0,
       0,   163,     0,     0,     0,   164,     0,     0,     0,     0,
       0,     0,    66,     0,     0,    67,    19,     0,     0,   165,
     166,     0,     0,   167,   552,     0,     0,     0,     0,     0,
       0,     0,   169,   170,   171,   172,     0,     0,   173,   174,
     159,   160,     0,     0,     0,    64,     0,     0,     0,   157,
     158,     0,     0,     0,   161,     0,     0,     0,     0,     0,
     162,     0,     0,     0,     0,     0,     0,   163,     0,     0,
       0,   164,     0,     0,     0,     0,     0,     0,    66,     0,
       0,    67,    19,     0,     0,   165,   166,     0,     0,   167,
       0,     0,   159,   160,     0,   581,     0,    64,   169,   170,
     171,   172,     0,     0,   173,   174,   161,     0,     0,     0,
       0,     0,   162,     0,     0,   157,   158,     0,     0,   163,
       0,     0,     0,   164,     0,     0,     0,     0,     0,     0,
      66,     0,     0,    67,    19,     0,     0,   165,   166,     0,
       0,   167,   595,     0,     0,     0,     0,     0,     0,     0,
     169,   170,   171,   172,     0,     0,   173,   174,   159,   160,
       0,     0,     0,    64,     0,     0,     0,     0,     0,     0,
       0,     0,   161,     0,     0,     0,     0,     0,   162,     0,
       0,     0,     0,     0,     0,   163,     0,     0,     0,   164,
       0,     0,     0,     0,     0,     0,    66,     0,     0,    67,
      19,     0,     0,   165,   166,     0,     0,   167,     0,     0,
       0,     0,     0,     0,     0,     0,   169,   170,   171,   172,
       0,     0,   173,   174,   219,     0,   159,   160,   564,   220,
     221,    64,   222,     0,     0,   223,     0,     0,     0,   224,
     161,     0,     0,   565,     0,     0,   162,   225,     4,   226,
       0,   227,   228,   163,   229,     0,     0,   164,   230,     0,
       0,     0,     0,     0,    66,     0,     0,    67,    19,     0,
       0,     0,     0,     0,     0,   231,     0,   128,   589,     0,
       0,     6,     0,     0,   169,   170,   171,   172,     0,     0,
     173,   174,   219,     0,   159,   160,   564,   220,   221,    64,
     222,     0,     0,   223,     0,     0,     0,   224,   161,     0,
       0,   565,     0,     0,   162,   225,     4,   226,     0,   227,
     228,   163,   229,     0,     0,   164,   230,     0,     0,     0,
       0,     0,    66,     0,     0,    67,    19,     0,     0,     0,
       0,     0,     0,   231,     0,   128,   602,     0,     0,     6,
       0,     0,   169,   170,   171,   172,     0,     0,   173,   174,
     219,     0,   159,   160,     0,   220,   221,    64,   222,     0,
       0,   223,     0,     0,     0,   224,   161,     0,     0,     0,
       0,     0,   162,   225,     4,   226,     0,   227,   228,   163,
     229,     0,     0,   164,   230,     0,     0,     0,     0,     0,
      66,     0,     0,    67,    19,     0,     0,     0,     0,     0,
       0,   231,     0,   128,   232,     0,     0,     6,     0,     0,
     169,   170,   171,   172,     0,     0,   173,   174,   219,     0,
     159,   160,     0,   220,   221,    64,   222,     0,     0,   223,
       0,     0,     0,   224,   161,     0,     0,     0,     0,     0,
     162,   225,     4,   226,     0,   227,   228,   163,   229,     0,
       0,   164,   230,     0,     0,     0,     0,     0,    66,     0,
       0,    67,    19,     0,     0,     0,     0,     0,     0,   231,
       0,   128,   363,     0,     0,     6,     0,     0,   169,   170,
     171,   172,     0,     0,   173,   174,   219,     0,   159,   160,
       0,   220,   221,    64,   222,     0,     0,   223,     0,     0,
       0,   224,   161,     0,     0,     0,     0,     0,   465,   225,
       4,   226,     0,   227,   228,   163,   229,     0,     0,   466,
     230,     0,     0,     0,     0,     0,    66,     0,     0,    67,
      19,     0,     0,     0,     0,     0,     0,   231,     0,   128,
     467,     0,     0,     6,     0,     0,   169,   170,   171,   172,
       0,     0,   173,   174,   219,     0,   159,   160,     0,   220,
     221,    64,   222,     0,     0,   223,     0,     0,     0,   224,
     161,     0,     0,     0,     0,     0,   162,   225,     4,   226,
       0,   227,   228,   163,   229,     0,     0,   164,   230,     0,
       0,     0,     0,     0,    66,     0,     0,    67,    19,     0,
       0,     0,     0,     0,     0,   231,     0,   128,   512,     0,
       0,     6,     0,     0,   169,   170,   171,   172,     0,     0,
     173,   174,   219,     0,   159,   160,     0,   220,   221,    64,
     222,     0,     0,   223,     0,     0,     0,   224,   161,     0,
       0,     0,     0,     0,   162,   225,     4,   226,     0,   227,
     228,   163,   229,     0,     0,   164,   230,     0,     0,     0,
       0,     0,    66,     0,     0,    67,    19,     0,     0,     0,
       0,     0,     0,   231,     0,   128,   515,     0,     0,     6,
       0,     0,   169,   170,   171,   172,     0,     0,   173,   174,
     219,     0,   159,   160,     0,   220,   221,    64,   222,     0,
       0,   223,     0,     0,     0,   224,   161,     0,     0,     0,
       0,     0,   162,   225,     4,   226,     0,   227,   228,   163,
     229,     0,     0,   164,   230,     0,     0,     0,     0,     0,
      66,     0,     0,    67,    19,     0,     0,     0,     0,     0,
       0,   231,     0,   128,   551,     0,     0,     6,     0,     0,
     169,   170,   171,   172,     0,     0,   173,   174,   219,     0,
     159,   160,     0,   220,   221,    64,   222,     0,     0,   223,
       0,     0,     0,   224,   161,     0,     0,     0,     0,     0,
     162,   225,     4,   226,     0,   227,   228,   163,   229,     0,
       0,   164,   230,     0,     0,     0,     0,     0,    66,     0,
       0,    67,    19,     0,     0,     0,     0,     0,     0,   231,
       0,   128,     0,     0,     0,     6,     0,     0,   169,   170,
     171,   172,     0,     0,   173,   174,   366,     0,   159,   160,
       0,   220,   221,    64,   222,     0,     0,   223,     0,     0,
       0,   224,   161,     0,     0,     0,     0,     0,   162,   225,
       0,   226,     0,   227,   228,   163,   229,     0,     0,   164,
     230,     0,     0,     0,     0,     0,    66,     0,     0,    67,
      19,     0,     0,     0,     0,     0,     0,   231,     0,   128,
       0,     0,     0,     6,     0,     0,   169,   170,   171,   172,
       0,     0,   173,   174,   366,     0,   159,   160,     0,   530,
     221,    64,   222,     0,     0,   223,     0,     0,     0,   224,
     161,     0,     0,     0,     0,     0,   162,   225,     0,   226,
       0,   227,   228,   163,   229,     0,     0,   164,   230,     0,
       0,     0,     0,     0,    66,     0,     0,    67,    19,     0,
       0,     0,     0,     0,     0,   231,     1,   128,   159,   160,
       0,     6,     0,    64,   169,   170,   171,   172,     0,     0,
     173,   174,   161,     0,     0,     0,     0,     0,   162,     0,
       0,     0,     0,   159,   160,   163,     0,     0,    64,   164,
       0,     0,     0,     0,     0,     0,    66,   161,     0,    67,
      19,     0,     0,   162,     0,     0,     0,   231,     0,     0,
     163,     0,     0,     0,   164,     0,   169,   170,   171,   172,
       0,    66,   173,   174,    67,    19,     0,     0,     0,     0,
      64,     0,   231,     0,     0,     0,     0,     0,     0,   161,
       0,   169,   170,   171,   172,   162,     0,   173,   174,     0,
       0,     0,   163,     0,     0,     0,   164,     0,     0,     0,
       0,     0,     0,    66,     0,     0,    67,    19,     0,     0,
     165,   166,     0,     0,   167,     0,     0,     0,     0,     0,
       0,     0,     0,   169,   170,   171,   172,     0,     0,   173,
     174
};

static const short yycheck[] =
{
       3,     4,   111,   103,   241,   288,   113,   108,   148,     1,
     128,   168,     5,   401,   496,    15,    52,    88,   508,     1,
      54,     8,   108,    10,    64,    52,    48,     0,     9,     8,
      33,    34,   133,    36,    98,     8,     9,    10,    64,   140,
      43,    63,     2,    30,    80,     5,    27,   133,    75,   496,
      31,    30,   534,    80,    27,   537,   163,    30,    31,   123,
      97,   125,    65,    44,    54,    99,    69,    37,    58,    88,
      40,    44,   108,    70,   102,   260,    98,    37,   142,    52,
      40,   108,   101,    96,    54,   112,    99,   534,    58,    89,
     537,   581,    52,   397,    54,    88,   484,   133,    58,   102,
     103,   128,   248,   108,   131,    97,   133,    80,    90,   255,
     600,   257,    96,   417,   117,    75,   598,   107,   102,   101,
      80,    53,   149,    73,   606,   128,   227,   609,   133,   611,
     102,    44,    69,   233,    44,   108,   163,   107,    88,   112,
     167,    51,    48,   113,    96,   148,   286,   107,   108,    62,
     102,   598,   112,   113,    88,    68,   569,    63,    68,   606,
     133,   101,   609,    52,   611,   102,   103,   101,   128,   473,
      96,   131,    44,   133,    84,   588,   102,    87,    88,   176,
      94,   282,   289,   186,    56,    99,    75,   491,   101,   149,
      62,    80,    98,   163,    44,    96,    68,   157,   158,   159,
     160,   102,    96,   163,    76,   165,   166,   167,   102,    99,
     514,   238,    62,   128,   241,   279,   499,   103,    68,   108,
     223,   534,    55,   112,   537,   108,   229,    60,    95,   101,
     233,    44,    99,   260,   471,    99,   103,   234,   241,   103,
     101,    52,   131,    56,   133,   248,    44,   404,   131,    62,
     133,   278,   255,    51,   257,    68,    96,    90,    97,    99,
     149,   288,   362,   103,    75,    96,   384,    96,    99,    80,
      95,   575,   101,   102,   459,   460,   513,   100,   238,   392,
      99,   241,   395,   286,   103,   598,    84,   260,   101,    87,
      88,   100,    44,   606,    51,   298,   609,   108,   611,    51,
     260,   112,   299,   300,    96,   278,   333,   304,   305,   101,
     102,    99,   315,   316,   101,   288,   103,   128,   278,   289,
     131,    97,   133,   508,    97,   101,   241,    84,   288,   289,
      87,    88,    84,   248,    68,    87,    88,    98,   149,    73,
     255,   102,   257,    77,   371,   260,    96,   448,    95,   238,
     496,   469,   102,   590,    88,   100,    61,   384,   128,   362,
      65,   321,   322,   323,   324,   325,   326,   327,   328,   329,
     330,   331,   332,   333,   334,   335,   336,   337,   338,   339,
     340,   384,   100,    66,   493,    68,    96,    96,   534,   278,
      73,   537,   102,   102,    77,   392,   581,    96,   395,   288,
     546,   128,   496,   102,   101,    88,   103,    97,   509,    99,
     519,   371,    97,    44,   560,   600,    93,    94,    95,   528,
      51,    54,    99,   394,   384,   396,   103,   238,   574,    60,
     241,    62,   102,   430,     5,     6,     7,    68,   398,   103,
     534,   401,   469,   537,   471,   101,   102,   556,   103,   260,
     103,    44,   598,    84,   101,   102,    87,    88,    51,    99,
     606,    93,    94,   609,   103,   611,   469,   278,   471,   384,
     571,   241,   499,    18,    19,    20,    21,   288,   248,     8,
       9,    10,   371,    11,   128,   255,   513,   257,    93,    94,
     260,    84,    12,   496,    87,    88,    46,    47,   157,   158,
     159,   160,    13,    96,   598,    14,   165,   166,   511,   469,
     513,   471,   606,    58,   241,   609,    95,   611,   326,   327,
     328,   248,   333,   483,   484,    95,   499,   496,   255,    95,
     257,   534,    95,   260,   537,   495,     3,     4,    95,   499,
      44,    90,   569,   546,   459,   460,   101,    51,   329,   330,
     331,   332,    95,   513,   469,   101,   471,   560,    16,    17,
     371,   588,   101,   590,    97,   534,   569,    99,   537,    93,
      94,   574,    68,   384,    46,    47,    46,    47,   324,   325,
      84,   496,    99,    87,    88,   588,    96,   590,    95,    68,
     128,    68,    96,   508,    95,   598,    98,   241,   513,   334,
     335,   101,   101,   606,   248,    96,   609,   101,   611,   569,
     499,   255,   101,   257,   384,   102,   260,     0,    67,   534,
     102,   101,   537,    96,    95,     8,     9,    10,   588,   598,
     590,   546,   100,    95,   100,    90,    96,   606,   101,    95,
     609,    95,   611,    96,    27,   560,    96,    30,    31,   101,
      95,    95,   100,    96,   569,    97,    97,   384,   469,   574,
     471,    44,   321,   322,   323,   101,   581,    97,    95,    52,
      57,   128,    57,   588,    96,   590,   101,    96,    96,    95,
     101,    90,    96,   598,   101,   600,   101,    90,   499,   459,
     460,   606,    96,   101,   609,   101,   611,    80,    57,   469,
      88,   471,   513,   241,     0,    93,    94,    95,   101,     0,
     248,    99,     8,     9,    10,   103,    96,   255,     0,   257,
     333,    10,   260,    46,    62,   108,   496,   117,   133,   131,
     278,    27,   459,   460,    30,    31,    45,   380,   508,   398,
     384,    80,   469,   513,   471,   260,    88,   567,    44,   567,
     133,    93,    94,    95,   128,   260,   449,    99,   569,   300,
     449,   103,   336,   338,   534,   337,   339,   537,   344,   496,
     495,   340,    -1,    -1,    -1,    -1,   546,   588,    -1,   590,
      -1,   508,    -1,    -1,   241,    -1,   513,    -1,    -1,    -1,
     560,   248,    -1,    -1,    -1,    -1,    -1,   128,   255,   569,
     257,    -1,    -1,   260,   574,    -1,    -1,   534,    -1,    -1,
     537,   581,   108,    -1,    -1,   459,   460,    -1,   588,   546,
     590,    -1,    -1,    -1,   483,   469,    -1,   471,   598,    -1,
     600,    -1,    -1,   560,    -1,    -1,   606,   133,    -1,   609,
      -1,   611,   569,    -1,    -1,    -1,   384,   574,    -1,    -1,
      -1,    -1,   496,    -1,   581,    -1,    -1,    -1,    -1,    -1,
      -1,   588,    -1,   590,   508,    -1,    -1,   241,    -1,   513,
      -1,   598,    -1,   600,   248,    -1,    -1,    -1,    -1,   606,
      -1,   255,   609,   257,   611,    -1,   260,    -1,    -1,    -1,
     534,    -1,    -1,   537,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   546,   118,    -1,    -1,    -1,    -1,    -1,    -1,
     241,    -1,    -1,    -1,    -1,    -1,   560,   248,    -1,    -1,
      -1,   459,   460,    -1,   255,   569,   257,   384,    -1,   260,
     574,   469,    -1,   471,    -1,    -1,    -1,   581,    -1,    -1,
      -1,    -1,    -1,    -1,   588,    -1,   590,    -1,    -1,    -1,
      44,    -1,   167,   168,   598,    -1,   600,    51,   496,    -1,
      -1,    -1,   606,    -1,    -1,   609,    60,   611,    62,    -1,
     508,    -1,    -1,    -1,    68,   513,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      84,    -1,    -1,    87,    88,    -1,   534,    -1,    -1,   537,
      -1,    -1,   459,   460,    98,    -1,   221,    -1,   546,   224,
     384,    -1,   469,    -1,   471,   230,   231,    -1,    -1,    -1,
      -1,    -1,   560,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   569,    -1,    -1,    -1,    -1,   574,    -1,    -1,   496,
      -1,    -1,    -1,   581,    -1,    -1,    -1,    -1,    -1,    -1,
     588,   508,   590,   384,    -1,    -1,   513,    -1,    -1,    -1,
     598,    -1,   600,    -1,    -1,    -1,    -1,    -1,   606,    -1,
      -1,   609,    -1,   611,    -1,    -1,    -1,   534,    -1,    -1,
     537,    -1,    -1,    -1,    -1,   459,   460,    -1,    -1,   546,
      -1,    -1,    -1,    -1,    -1,   469,    -1,   471,    -1,   314,
      -1,    -1,   317,   560,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   569,    -1,    -1,    -1,    -1,   574,    -1,    -1,
      -1,    -1,   496,    -1,   581,    -1,   341,    -1,   459,   460,
     345,   588,    -1,   590,   508,    -1,   351,   352,   469,   513,
     471,   598,    -1,   600,    -1,    -1,    -1,    -1,    -1,   606,
      -1,    -1,   609,    -1,   611,    -1,    -1,    -1,    -1,    -1,
     534,   376,   377,   537,    -1,   496,    -1,    -1,    -1,    -1,
      -1,    -1,   546,    -1,    -1,    -1,   391,   508,    -1,    -1,
      -1,    -1,   513,    -1,    -1,    -1,   560,    -1,    -1,   404,
     157,   158,   159,   160,    -1,   569,    -1,    -1,   165,   166,
     574,    -1,    -1,   534,    -1,    -1,   537,   581,    -1,    -1,
     157,   158,   159,   160,   588,   546,   590,    -1,   165,   166,
       1,    -1,     3,     4,   598,    -1,   600,    -1,    -1,   560,
      -1,    -1,   606,    -1,    -1,   609,    -1,   611,   569,   454,
      -1,    -1,    -1,   574,    44,    -1,    -1,    -1,    -1,    -1,
     581,    51,    -1,    -1,    -1,    -1,    -1,   588,    -1,   590,
      60,    -1,    62,    -1,    -1,    46,    47,   598,    68,   600,
      51,    -1,    -1,    -1,   489,   606,    -1,    -1,   609,    60,
     611,    -1,    -1,    -1,    84,    66,    -1,    87,    88,   504,
      -1,    -1,    73,    -1,    -1,    -1,    77,    97,    98,    -1,
      -1,   101,    -1,    84,    -1,    -1,    87,    88,    -1,    -1,
      91,    92,    -1,    -1,    95,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   104,   105,   106,   107,    -1,    -1,   110,
     111,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   559,    -1,    -1,    -1,   563,    -1,
     565,    -1,    -1,    -1,   321,   322,   323,   324,   325,   326,
     327,   328,   329,   330,   331,   332,    -1,   334,   335,   336,
     337,   338,   339,   340,   321,   322,   323,   324,   325,   326,
     327,   328,   329,   330,   331,   332,    -1,   334,   335,   336,
     337,   338,   339,   340,     1,    -1,     3,     4,    44,    -1,
      -1,    -1,    -1,    44,    -1,    51,    -1,    -1,    -1,    -1,
      51,    -1,    -1,    -1,    60,    -1,    62,    -1,    -1,    60,
      -1,    62,    68,    -1,    -1,    -1,    -1,    68,    -1,    -1,
      -1,   398,    -1,    -1,   401,    -1,    -1,    -1,    84,    46,
      47,    87,    88,    84,    51,    -1,    87,    88,    -1,    -1,
      -1,   398,    98,    60,   401,    -1,    97,    98,    -1,    66,
     101,    -1,    -1,    -1,    -1,    -1,    73,    -1,    -1,    -1,
      77,    -1,    -1,    -1,    -1,     3,     4,    84,    -1,    -1,
      87,    88,    -1,    -1,    91,    92,    -1,    -1,    95,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   104,   105,   106,
     107,    -1,    -1,   110,   111,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   483,   484,    46,    47,
      -1,    -1,    -1,    51,    -1,    -1,    -1,    -1,   495,    -1,
      -1,    -1,    60,    -1,    -1,    -1,   483,   484,    66,    -1,
      -1,     3,     4,    -1,    -1,    73,    -1,    -1,   495,    77,
      -1,    -1,    -1,    -1,    -1,    -1,    84,    -1,    -1,    87,
      88,    -1,    -1,    91,    92,    -1,    -1,    95,    -1,    97,
      98,    -1,    -1,    -1,   102,    -1,   104,   105,   106,   107,
      -1,    -1,   110,   111,    46,    47,    -1,    -1,    -1,    51,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    60,    -1,
      -1,    -1,    -1,    -1,    66,    -1,    -1,     3,     4,    -1,
      -1,    73,    -1,    -1,    -1,    77,    -1,    -1,    -1,    -1,
      -1,    -1,    84,    -1,    -1,    87,    88,    -1,    -1,    91,
      92,    -1,    -1,    95,    -1,    97,    98,    -1,    -1,    -1,
      -1,    -1,   104,   105,   106,   107,    -1,    -1,   110,   111,
      46,    47,    -1,    -1,    -1,    51,    -1,    -1,    -1,     3,
       4,    -1,    -1,    -1,    60,    -1,    -1,    -1,    -1,    -1,
      66,    -1,    -1,    -1,    -1,    -1,    -1,    73,    -1,    -1,
      -1,    77,    -1,    -1,    -1,    -1,    -1,    -1,    84,    -1,
      -1,    87,    88,    -1,    -1,    91,    92,    -1,    -1,    95,
      -1,    97,    46,    47,    -1,    -1,    -1,    51,   104,   105,
     106,   107,    -1,    -1,   110,   111,    60,    -1,    -1,    -1,
      -1,    -1,    66,    -1,    -1,     3,     4,    -1,    -1,    73,
      -1,    -1,    -1,    77,    -1,    -1,    -1,    -1,    -1,    -1,
      84,    -1,    -1,    87,    88,    -1,    -1,    91,    92,    -1,
      -1,    95,    -1,    -1,    -1,    -1,    -1,   101,    -1,    -1,
     104,   105,   106,   107,    -1,    -1,   110,   111,    46,    47,
      -1,    -1,    -1,    51,    -1,    -1,    -1,     3,     4,    -1,
      -1,    -1,    60,    -1,    -1,    -1,    -1,    -1,    66,    -1,
      -1,    -1,    -1,    -1,    -1,    73,    -1,    -1,    -1,    77,
      -1,    -1,    -1,    -1,    -1,    -1,    84,    -1,    -1,    87,
      88,    -1,    -1,    91,    92,    -1,    -1,    95,    96,    -1,
      46,    47,    -1,    -1,    -1,    51,   104,   105,   106,   107,
      -1,    -1,   110,   111,    60,    -1,    -1,    -1,    -1,    -1,
      66,    -1,    -1,     3,     4,    -1,    -1,    73,    -1,    -1,
      -1,    77,    -1,    -1,    -1,    -1,    -1,    -1,    84,    -1,
      -1,    87,    88,    -1,    -1,    91,    92,    -1,    -1,    95,
      -1,    -1,    -1,    -1,   100,    -1,    -1,    -1,   104,   105,
     106,   107,    -1,    -1,   110,   111,    46,    47,    -1,    -1,
      -1,    51,    -1,    -1,    -1,     3,     4,    -1,    -1,    -1,
      60,    -1,    -1,    -1,    -1,    -1,    66,    -1,    -1,    -1,
      -1,    -1,    -1,    73,    -1,    -1,    -1,    77,    -1,    -1,
      -1,    -1,    -1,    -1,    84,    -1,    -1,    87,    88,    -1,
      -1,    91,    92,    -1,    -1,    95,    -1,    -1,    46,    47,
      -1,   101,    -1,    51,   104,   105,   106,   107,    -1,    -1,
     110,   111,    60,    -1,    -1,    -1,    -1,    -1,    66,    -1,
      -1,     3,     4,    -1,    -1,    73,    -1,    -1,    -1,    77,
      -1,    -1,    -1,    -1,    -1,    -1,    84,    -1,    -1,    87,
      88,    -1,    -1,    91,    92,    -1,    -1,    95,    96,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   104,   105,   106,   107,
      -1,    -1,   110,   111,    46,    47,    -1,    -1,    -1,    51,
      -1,    -1,    -1,     3,     4,    -1,    -1,    -1,    60,    -1,
      -1,    -1,    -1,    -1,    66,    -1,    -1,    -1,    -1,    -1,
      -1,    73,    -1,    -1,    -1,    77,    -1,    -1,    -1,    -1,
      -1,    -1,    84,    -1,    -1,    87,    88,    -1,    -1,    91,
      92,    -1,    -1,    95,    96,    -1,    46,    47,    -1,    -1,
      -1,    51,   104,   105,   106,   107,    -1,    -1,   110,   111,
      60,    -1,    -1,    -1,    -1,    -1,    66,    -1,    -1,     3,
       4,    -1,    -1,    73,    -1,    -1,    -1,    77,    -1,    -1,
      -1,    -1,    -1,    -1,    84,    -1,    -1,    87,    88,    -1,
      -1,    91,    92,    -1,    -1,    95,    96,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   104,   105,   106,   107,    -1,    -1,
     110,   111,    46,    47,    -1,    -1,    -1,    51,    -1,    -1,
      -1,     3,     4,    -1,    -1,    -1,    60,    -1,    -1,    -1,
      -1,    -1,    66,    -1,    -1,    -1,    -1,    -1,    -1,    73,
      -1,    -1,    -1,    77,    -1,    -1,    -1,    -1,    -1,    -1,
      84,    -1,    -1,    87,    88,    -1,    -1,    91,    92,    -1,
      -1,    95,    96,    -1,    46,    47,    -1,    -1,    -1,    51,
     104,   105,   106,   107,    -1,    -1,   110,   111,    60,    -1,
      -1,    -1,    -1,    -1,    66,    -1,    -1,     3,     4,    -1,
      -1,    73,    -1,    -1,    -1,    77,    -1,    -1,    -1,    -1,
      -1,    -1,    84,    -1,    -1,    87,    88,    -1,    -1,    91,
      92,    -1,    -1,    95,    96,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   104,   105,   106,   107,    -1,    -1,   110,   111,
      46,    47,    -1,    -1,    -1,    51,    -1,    -1,    -1,     3,
       4,    -1,    -1,    -1,    60,    -1,    -1,    -1,    -1,    -1,
      66,    -1,    -1,    -1,    -1,    -1,    -1,    73,    -1,    -1,
      -1,    77,    -1,    -1,    -1,    -1,    -1,    -1,    84,    -1,
      -1,    87,    88,    -1,    -1,    91,    92,    -1,    -1,    95,
      -1,    -1,    46,    47,    -1,   101,    -1,    51,   104,   105,
     106,   107,    -1,    -1,   110,   111,    60,    -1,    -1,    -1,
      -1,    -1,    66,    -1,    -1,     3,     4,    -1,    -1,    73,
      -1,    -1,    -1,    77,    -1,    -1,    -1,    -1,    -1,    -1,
      84,    -1,    -1,    87,    88,    -1,    -1,    91,    92,    -1,
      -1,    95,    96,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     104,   105,   106,   107,    -1,    -1,   110,   111,    46,    47,
      -1,    -1,    -1,    51,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    60,    -1,    -1,    -1,    -1,    -1,    66,    -1,
      -1,    -1,    -1,    -1,    -1,    73,    -1,    -1,    -1,    77,
      -1,    -1,    -1,    -1,    -1,    -1,    84,    -1,    -1,    87,
      88,    -1,    -1,    91,    92,    -1,    -1,    95,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   104,   105,   106,   107,
      -1,    -1,   110,   111,    44,    -1,    46,    47,    48,    49,
      50,    51,    52,    -1,    -1,    55,    -1,    -1,    -1,    59,
      60,    -1,    -1,    63,    -1,    -1,    66,    67,    68,    69,
      -1,    71,    72,    73,    74,    -1,    -1,    77,    78,    -1,
      -1,    -1,    -1,    -1,    84,    -1,    -1,    87,    88,    -1,
      -1,    -1,    -1,    -1,    -1,    95,    -1,    97,    98,    -1,
      -1,   101,    -1,    -1,   104,   105,   106,   107,    -1,    -1,
     110,   111,    44,    -1,    46,    47,    48,    49,    50,    51,
      52,    -1,    -1,    55,    -1,    -1,    -1,    59,    60,    -1,
      -1,    63,    -1,    -1,    66,    67,    68,    69,    -1,    71,
      72,    73,    74,    -1,    -1,    77,    78,    -1,    -1,    -1,
      -1,    -1,    84,    -1,    -1,    87,    88,    -1,    -1,    -1,
      -1,    -1,    -1,    95,    -1,    97,    98,    -1,    -1,   101,
      -1,    -1,   104,   105,   106,   107,    -1,    -1,   110,   111,
      44,    -1,    46,    47,    -1,    49,    50,    51,    52,    -1,
      -1,    55,    -1,    -1,    -1,    59,    60,    -1,    -1,    -1,
      -1,    -1,    66,    67,    68,    69,    -1,    71,    72,    73,
      74,    -1,    -1,    77,    78,    -1,    -1,    -1,    -1,    -1,
      84,    -1,    -1,    87,    88,    -1,    -1,    -1,    -1,    -1,
      -1,    95,    -1,    97,    98,    -1,    -1,   101,    -1,    -1,
     104,   105,   106,   107,    -1,    -1,   110,   111,    44,    -1,
      46,    47,    -1,    49,    50,    51,    52,    -1,    -1,    55,
      -1,    -1,    -1,    59,    60,    -1,    -1,    -1,    -1,    -1,
      66,    67,    68,    69,    -1,    71,    72,    73,    74,    -1,
      -1,    77,    78,    -1,    -1,    -1,    -1,    -1,    84,    -1,
      -1,    87,    88,    -1,    -1,    -1,    -1,    -1,    -1,    95,
      -1,    97,    98,    -1,    -1,   101,    -1,    -1,   104,   105,
     106,   107,    -1,    -1,   110,   111,    44,    -1,    46,    47,
      -1,    49,    50,    51,    52,    -1,    -1,    55,    -1,    -1,
      -1,    59,    60,    -1,    -1,    -1,    -1,    -1,    66,    67,
      68,    69,    -1,    71,    72,    73,    74,    -1,    -1,    77,
      78,    -1,    -1,    -1,    -1,    -1,    84,    -1,    -1,    87,
      88,    -1,    -1,    -1,    -1,    -1,    -1,    95,    -1,    97,
      98,    -1,    -1,   101,    -1,    -1,   104,   105,   106,   107,
      -1,    -1,   110,   111,    44,    -1,    46,    47,    -1,    49,
      50,    51,    52,    -1,    -1,    55,    -1,    -1,    -1,    59,
      60,    -1,    -1,    -1,    -1,    -1,    66,    67,    68,    69,
      -1,    71,    72,    73,    74,    -1,    -1,    77,    78,    -1,
      -1,    -1,    -1,    -1,    84,    -1,    -1,    87,    88,    -1,
      -1,    -1,    -1,    -1,    -1,    95,    -1,    97,    98,    -1,
      -1,   101,    -1,    -1,   104,   105,   106,   107,    -1,    -1,
     110,   111,    44,    -1,    46,    47,    -1,    49,    50,    51,
      52,    -1,    -1,    55,    -1,    -1,    -1,    59,    60,    -1,
      -1,    -1,    -1,    -1,    66,    67,    68,    69,    -1,    71,
      72,    73,    74,    -1,    -1,    77,    78,    -1,    -1,    -1,
      -1,    -1,    84,    -1,    -1,    87,    88,    -1,    -1,    -1,
      -1,    -1,    -1,    95,    -1,    97,    98,    -1,    -1,   101,
      -1,    -1,   104,   105,   106,   107,    -1,    -1,   110,   111,
      44,    -1,    46,    47,    -1,    49,    50,    51,    52,    -1,
      -1,    55,    -1,    -1,    -1,    59,    60,    -1,    -1,    -1,
      -1,    -1,    66,    67,    68,    69,    -1,    71,    72,    73,
      74,    -1,    -1,    77,    78,    -1,    -1,    -1,    -1,    -1,
      84,    -1,    -1,    87,    88,    -1,    -1,    -1,    -1,    -1,
      -1,    95,    -1,    97,    98,    -1,    -1,   101,    -1,    -1,
     104,   105,   106,   107,    -1,    -1,   110,   111,    44,    -1,
      46,    47,    -1,    49,    50,    51,    52,    -1,    -1,    55,
      -1,    -1,    -1,    59,    60,    -1,    -1,    -1,    -1,    -1,
      66,    67,    68,    69,    -1,    71,    72,    73,    74,    -1,
      -1,    77,    78,    -1,    -1,    -1,    -1,    -1,    84,    -1,
      -1,    87,    88,    -1,    -1,    -1,    -1,    -1,    -1,    95,
      -1,    97,    -1,    -1,    -1,   101,    -1,    -1,   104,   105,
     106,   107,    -1,    -1,   110,   111,    44,    -1,    46,    47,
      -1,    49,    50,    51,    52,    -1,    -1,    55,    -1,    -1,
      -1,    59,    60,    -1,    -1,    -1,    -1,    -1,    66,    67,
      -1,    69,    -1,    71,    72,    73,    74,    -1,    -1,    77,
      78,    -1,    -1,    -1,    -1,    -1,    84,    -1,    -1,    87,
      88,    -1,    -1,    -1,    -1,    -1,    -1,    95,    -1,    97,
      -1,    -1,    -1,   101,    -1,    -1,   104,   105,   106,   107,
      -1,    -1,   110,   111,    44,    -1,    46,    47,    -1,    49,
      50,    51,    52,    -1,    -1,    55,    -1,    -1,    -1,    59,
      60,    -1,    -1,    -1,    -1,    -1,    66,    67,    -1,    69,
      -1,    71,    72,    73,    74,    -1,    -1,    77,    78,    -1,
      -1,    -1,    -1,    -1,    84,    -1,    -1,    87,    88,    -1,
      -1,    -1,    -1,    -1,    -1,    95,    44,    97,    46,    47,
      -1,   101,    -1,    51,   104,   105,   106,   107,    -1,    -1,
     110,   111,    60,    -1,    -1,    -1,    -1,    -1,    66,    -1,
      -1,    -1,    -1,    46,    47,    73,    -1,    -1,    51,    77,
      -1,    -1,    -1,    -1,    -1,    -1,    84,    60,    -1,    87,
      88,    -1,    -1,    66,    -1,    -1,    -1,    95,    -1,    -1,
      73,    -1,    -1,    -1,    77,    -1,   104,   105,   106,   107,
      -1,    84,   110,   111,    87,    88,    -1,    -1,    -1,    -1,
      51,    -1,    95,    -1,    -1,    -1,    -1,    -1,    -1,    60,
      -1,   104,   105,   106,   107,    66,    -1,   110,   111,    -1,
      -1,    -1,    73,    -1,    -1,    -1,    77,    -1,    -1,    -1,
      -1,    -1,    -1,    84,    -1,    -1,    87,    88,    -1,    -1,
      91,    92,    -1,    -1,    95,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   104,   105,   106,   107,    -1,    -1,   110,
     111
};
#define YYPURE 1

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

/* The parser invokes alloca or malloc; define the necessary symbols.  */

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
#  define YYSTACK_ALLOC malloc
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
     to reallocate them elsewhere.  */

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
	/* Give user a chance to reallocate the stack. Use copies of
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

case 10:
#line 232 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{
		  /* use preset global here. FIXME */
		  yyval.node = xstrdup ("int");
		;
    break;}
case 11:
#line 237 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{
		  /* use preset global here. FIXME */
		  yyval.node = xstrdup ("double");
		;
    break;}
case 12:
#line 242 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{
		  /* use preset global here. FIXME */
		  yyval.node = xstrdup ("boolean");
		;
    break;}
case 18:
#line 267 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{
	          while (bracket_count-- > 0) 
		    yyval.node = concat ("[", yyvsp[-1].node, NULL);
		;
    break;}
case 19:
#line 272 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{
	          while (bracket_count-- > 0) 
		    yyval.node = concat ("[", yyvsp[-1].node, NULL);
		;
    break;}
case 23:
#line 290 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{ 
		  yyval.node = concat (yyvsp[-2].node, ".", yyvsp[0].node, NULL);
		;
    break;}
case 37:
#line 322 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{ package_name = yyvsp[-1].node; ;
    break;}
case 45:
#line 349 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{ 
		  if (yyvsp[0].value == PUBLIC_TK)
		    modifier_value++;
                  if (yyvsp[0].value == STATIC_TK)
                    modifier_value++;
	          USE_ABSORBER;
		;
    break;}
case 46:
#line 357 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{ 
		  if (yyvsp[0].value == PUBLIC_TK)
		    modifier_value++;
                  if (yyvsp[0].value == STATIC_TK)
                    modifier_value++;
		  USE_ABSORBER;
		;
    break;}
case 47:
#line 369 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{ 
		  report_class_declaration(yyvsp[-2].node);
		  modifier_value = 0;
                ;
    break;}
case 49:
#line 375 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{ report_class_declaration(yyvsp[-2].node); ;
    break;}
case 55:
#line 389 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{ USE_ABSORBER; ;
    break;}
case 56:
#line 391 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{ USE_ABSORBER; ;
    break;}
case 57:
#line 396 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{ pop_class_context (); ;
    break;}
case 58:
#line 398 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{ pop_class_context (); ;
    break;}
case 70:
#line 424 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{ USE_ABSORBER; ;
    break;}
case 71:
#line 426 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{ modifier_value = 0; ;
    break;}
case 76:
#line 442 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{ bracket_count = 0; USE_ABSORBER; ;
    break;}
case 77:
#line 444 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{ ++bracket_count; ;
    break;}
case 80:
#line 455 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{ ++method_depth; ;
    break;}
case 81:
#line 457 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{ --method_depth; ;
    break;}
case 82:
#line 462 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{ USE_ABSORBER; ;
    break;}
case 84:
#line 465 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{ modifier_value = 0; ;
    break;}
case 85:
#line 467 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{ 
                  report_main_declaration (yyvsp[-1].declarator);
		  modifier_value = 0;
		;
    break;}
case 86:
#line 475 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{ 
		  struct method_declarator *d;
		  NEW_METHOD_DECLARATOR (d, yyvsp[-2].node, NULL);
		  yyval.declarator = d;
		;
    break;}
case 87:
#line 481 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{ 
		  struct method_declarator *d;
		  NEW_METHOD_DECLARATOR (d, yyvsp[-3].node, yyvsp[-1].node);
		  yyval.declarator = d;
		;
    break;}
case 90:
#line 492 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{
		  yyval.node = concat (yyvsp[-2].node, ",", yyvsp[0].node, NULL);
		;
    break;}
case 91:
#line 499 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{ 
		  USE_ABSORBER;
		  if (bracket_count)
		    {
		      int i;
		      char *n = xmalloc (bracket_count + 1 + strlen (yyval.node));
		      for (i = 0; i < bracket_count; ++i)
			n[i] = '[';
		      strcpy (n + bracket_count, yyval.node);
		      yyval.node = n;
		    }
		  else
		    yyval.node = yyvsp[-1].node;
		;
    break;}
case 92:
#line 514 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{
		  if (bracket_count)
		    {
		      int i;
		      char *n = xmalloc (bracket_count + 1 + strlen (yyval.node));
		      for (i = 0; i < bracket_count; ++i)
			n[i] = '[';
		      strcpy (n + bracket_count, yyval.node);
		      yyval.node = n;
		    }
		  else
		    yyval.node = yyvsp[-1].node;
		;
    break;}
case 95:
#line 535 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{ USE_ABSORBER; ;
    break;}
case 96:
#line 537 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{ USE_ABSORBER; ;
    break;}
case 100:
#line 552 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{ USE_ABSORBER; ;
    break;}
case 102:
#line 563 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{ modifier_value = 0; ;
    break;}
case 104:
#line 568 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{ modifier_value = 0; ;
    break;}
case 105:
#line 575 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{ USE_ABSORBER; ;
    break;}
case 106:
#line 577 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{ USE_ABSORBER; ;
    break;}
case 113:
#line 594 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{ USE_ABSORBER; ;
    break;}
case 114:
#line 596 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{ USE_ABSORBER; ;
    break;}
case 117:
#line 608 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{ report_class_declaration (yyvsp[0].node); modifier_value = 0; ;
    break;}
case 119:
#line 611 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{ report_class_declaration (yyvsp[0].node); modifier_value = 0; ;
    break;}
case 121:
#line 614 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{ report_class_declaration (yyvsp[-1].node); modifier_value = 0; ;
    break;}
case 123:
#line 617 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{ report_class_declaration (yyvsp[-1].node); modifier_value = 0; ;
    break;}
case 127:
#line 628 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{ pop_class_context (); ;
    break;}
case 128:
#line 630 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{ pop_class_context (); ;
    break;}
case 151:
#line 689 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{ USE_ABSORBER; ;
    break;}
case 152:
#line 691 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{ modifier_value = 0; ;
    break;}
case 177:
#line 732 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{ USE_ABSORBER; ;
    break;}
case 188:
#line 760 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{ ++complexity; ;
    break;}
case 189:
#line 765 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{ ++complexity; ;
    break;}
case 190:
#line 770 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{ ++complexity; ;
    break;}
case 198:
#line 790 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{ ++complexity; ;
    break;}
case 203:
#line 805 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{ ++complexity; ;
    break;}
case 207:
#line 822 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{ ++complexity; ;
    break;}
case 213:
#line 840 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{ ++complexity; ;
    break;}
case 224:
#line 865 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{ ++complexity; ;
    break;}
case 227:
#line 874 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{ ++complexity; ;
    break;}
case 230:
#line 881 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{yyerror ("Missing term"); RECOVER;;
    break;}
case 231:
#line 883 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{yyerror ("';' expected"); RECOVER;;
    break;}
case 234:
#line 892 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{ USE_ABSORBER; ;
    break;}
case 240:
#line 907 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{ ++complexity; ;
    break;}
case 241:
#line 911 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{ ++complexity; ;
    break;}
case 252:
#line 933 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{ USE_ABSORBER; ;
    break;}
case 253:
#line 938 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{ USE_ABSORBER; ;
    break;}
case 254:
#line 940 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{ USE_ABSORBER; ;
    break;}
case 255:
#line 942 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{ USE_ABSORBER; ;
    break;}
case 256:
#line 944 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{ USE_ABSORBER; ;
    break;}
case 264:
#line 959 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{ report_class_declaration (anonymous_context); ;
    break;}
case 266:
#line 962 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{ report_class_declaration (anonymous_context); ;
    break;}
case 268:
#line 968 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{ USE_ABSORBER; ;
    break;}
case 282:
#line 1000 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{ bracket_count = 1; ;
    break;}
case 283:
#line 1002 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{ bracket_count++; ;
    break;}
case 286:
#line 1015 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{ USE_ABSORBER; ++complexity; ;
    break;}
case 287:
#line 1017 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{ USE_ABSORBER; ++complexity; ;
    break;}
case 288:
#line 1018 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{ ++complexity; ;
    break;}
case 289:
#line 1019 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{ ++complexity; ;
    break;}
case 290:
#line 1020 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{ ++complexity; ;
    break;}
case 291:
#line 1021 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{ ++complexity; ;
    break;}
case 292:
#line 1026 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{ USE_ABSORBER; ;
    break;}
case 295:
#line 1033 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{ USE_ABSORBER; ;
    break;}
case 342:
#line 1129 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{ ++complexity; ;
    break;}
case 344:
#line 1135 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{ ++complexity; ;
    break;}
case 346:
#line 1141 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{ ++complexity; ;
    break;}
case 350:
#line 1155 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"
{ USE_ABSORBER; ;
    break;}
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
#line 1173 "/home/mitchell/gcc-3.3.2/gcc-3.3.2/gcc/java/parse-scan.y"


/* Create a new parser context */

void
java_push_parser_context ()
{
  struct parser_ctxt *new = 
    (struct parser_ctxt *) xcalloc (1, sizeof (struct parser_ctxt));

  new->next = ctxp;
  ctxp = new;
}  

static void
push_class_context (name)
    const char *name;
{
  struct class_context *ctx;

  ctx = (struct class_context *) xmalloc (sizeof (struct class_context));
  ctx->name = (char *) name;
  ctx->next = current_class_context;
  current_class_context = ctx;
}

static void
pop_class_context ()
{
  struct class_context *ctx;

  if (current_class_context == NULL)
    return;

  ctx = current_class_context->next;
  if (current_class_context->name != anonymous_context)
    free (current_class_context->name);
  free (current_class_context);

  current_class_context = ctx;
  if (current_class_context == NULL)
    anonymous_count = 0;
}

/* Recursively construct the class name.  This is just a helper
   function for get_class_name().  */
static int
make_class_name_recursive (stack, ctx)
     struct obstack *stack;
     struct class_context *ctx;
{
  if (! ctx)
    return 0;

  make_class_name_recursive (stack, ctx->next);

  /* Replace an anonymous context with the appropriate counter value.  */
  if (ctx->name == anonymous_context)
    {
      char buf[50];
      ++anonymous_count;
      sprintf (buf, "%d", anonymous_count);
      ctx->name = xstrdup (buf);
    }

  obstack_grow (stack, ctx->name, strlen (ctx->name));
  obstack_1grow (stack, '$');

  return ISDIGIT (ctx->name[0]);
}

/* Return a newly allocated string holding the name of the class.  */
static char *
get_class_name ()
{
  char *result;
  int last_was_digit;
  struct obstack name_stack;

  obstack_init (&name_stack);

  /* Duplicate the logic of parse.y:maybe_make_nested_class_name().  */
  last_was_digit = make_class_name_recursive (&name_stack,
					      current_class_context->next);

  if (! last_was_digit
      && method_depth
      && current_class_context->name != anonymous_context)
    {
      char buf[50];
      ++anonymous_count;
      sprintf (buf, "%d", anonymous_count);
      obstack_grow (&name_stack, buf, strlen (buf));
      obstack_1grow (&name_stack, '$');
    }

  if (current_class_context->name == anonymous_context)
    {
      char buf[50];
      ++anonymous_count;
      sprintf (buf, "%d", anonymous_count);
      current_class_context->name = xstrdup (buf);
      obstack_grow0 (&name_stack, buf, strlen (buf));
    }
  else
    obstack_grow0 (&name_stack, current_class_context->name,
		   strlen (current_class_context->name));

  result = xstrdup (obstack_finish (&name_stack));
  obstack_free (&name_stack, NULL);

  return result;
}

/* Actions defined here */

static void
report_class_declaration (name)
     const char * name;
{
  extern int flag_dump_class, flag_list_filename;

  push_class_context (name);
  if (flag_dump_class)
    {
      char *name = get_class_name ();

      if (!previous_output)
	{
	  if (flag_list_filename)
	    fprintf (out, "%s: ", input_filename);
	  previous_output = 1;
	}

      if (package_name)
	fprintf (out, "%s.%s ", package_name, name);
      else
	fprintf (out, "%s ", name);

      free (name);
    }
}

static void
report_main_declaration (declarator)
     struct method_declarator *declarator;
{
  extern int flag_find_main;

  if (flag_find_main
      && modifier_value == 2
      && !strcmp (declarator->method_name, "main") 
      && declarator->args 
      && declarator->args [0] == '[' 
      && (! strcmp (declarator->args+1, "String")
	  || ! strcmp (declarator->args + 1, "java.lang.String"))
      && current_class_context)
    {
      if (!previous_output)
	{
	  char *name = get_class_name ();
	  if (package_name)
	    fprintf (out, "%s.%s ", package_name, name);
	  else
	    fprintf (out, "%s", name);
	  free (name);
	  previous_output = 1;
	}
    }
}

void
report ()
{
  extern int flag_complexity;
  if (flag_complexity)
    fprintf (out, "%s %d\n", input_filename, complexity);
}

/* Reset global status used by the report functions.  */

void reset_report ()
{
  previous_output = 0;
  package_name = NULL;
  current_class_context = NULL;
  complexity = 0;
}

void
yyerror (msg)
     const char *msg ATTRIBUTE_UNUSED;
{
  fprintf (stderr, "%s: %d: %s\n", input_filename, lineno, msg);
  exit (1);
}
