/* A Bison parser, made from /home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y, by GNU bison 1.75.  */

/* Skeleton parser for Yacc-like parsing with Bison,
   Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002 Free Software Foundation, Inc.

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

/* Written by Richard Stallman by simplifying the original so called
   ``semantic'' parser.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output.  */
#define YYBISON	1

/* Pure parsers.  */
#define YYPURE	1

/* Using locations.  */
#define YYLSP_NEEDED 0



/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     PLUS_TK = 258,
     MINUS_TK = 259,
     MULT_TK = 260,
     DIV_TK = 261,
     REM_TK = 262,
     LS_TK = 263,
     SRS_TK = 264,
     ZRS_TK = 265,
     AND_TK = 266,
     XOR_TK = 267,
     OR_TK = 268,
     BOOL_AND_TK = 269,
     BOOL_OR_TK = 270,
     EQ_TK = 271,
     NEQ_TK = 272,
     GT_TK = 273,
     GTE_TK = 274,
     LT_TK = 275,
     LTE_TK = 276,
     PLUS_ASSIGN_TK = 277,
     MINUS_ASSIGN_TK = 278,
     MULT_ASSIGN_TK = 279,
     DIV_ASSIGN_TK = 280,
     REM_ASSIGN_TK = 281,
     LS_ASSIGN_TK = 282,
     SRS_ASSIGN_TK = 283,
     ZRS_ASSIGN_TK = 284,
     AND_ASSIGN_TK = 285,
     XOR_ASSIGN_TK = 286,
     OR_ASSIGN_TK = 287,
     PUBLIC_TK = 288,
     PRIVATE_TK = 289,
     PROTECTED_TK = 290,
     STATIC_TK = 291,
     FINAL_TK = 292,
     SYNCHRONIZED_TK = 293,
     VOLATILE_TK = 294,
     TRANSIENT_TK = 295,
     NATIVE_TK = 296,
     PAD_TK = 297,
     ABSTRACT_TK = 298,
     MODIFIER_TK = 299,
     STRICT_TK = 300,
     DECR_TK = 301,
     INCR_TK = 302,
     DEFAULT_TK = 303,
     IF_TK = 304,
     THROW_TK = 305,
     BOOLEAN_TK = 306,
     DO_TK = 307,
     IMPLEMENTS_TK = 308,
     THROWS_TK = 309,
     BREAK_TK = 310,
     IMPORT_TK = 311,
     ELSE_TK = 312,
     INSTANCEOF_TK = 313,
     RETURN_TK = 314,
     VOID_TK = 315,
     CATCH_TK = 316,
     INTERFACE_TK = 317,
     CASE_TK = 318,
     EXTENDS_TK = 319,
     FINALLY_TK = 320,
     SUPER_TK = 321,
     WHILE_TK = 322,
     CLASS_TK = 323,
     SWITCH_TK = 324,
     CONST_TK = 325,
     TRY_TK = 326,
     FOR_TK = 327,
     NEW_TK = 328,
     CONTINUE_TK = 329,
     GOTO_TK = 330,
     PACKAGE_TK = 331,
     THIS_TK = 332,
     ASSERT_TK = 333,
     BYTE_TK = 334,
     SHORT_TK = 335,
     INT_TK = 336,
     LONG_TK = 337,
     CHAR_TK = 338,
     INTEGRAL_TK = 339,
     FLOAT_TK = 340,
     DOUBLE_TK = 341,
     FP_TK = 342,
     ID_TK = 343,
     REL_QM_TK = 344,
     REL_CL_TK = 345,
     NOT_TK = 346,
     NEG_TK = 347,
     ASSIGN_ANY_TK = 348,
     ASSIGN_TK = 349,
     OP_TK = 350,
     CP_TK = 351,
     OCB_TK = 352,
     CCB_TK = 353,
     OSB_TK = 354,
     CSB_TK = 355,
     SC_TK = 356,
     C_TK = 357,
     DOT_TK = 358,
     STRING_LIT_TK = 359,
     CHAR_LIT_TK = 360,
     INT_LIT_TK = 361,
     FP_LIT_TK = 362,
     TRUE_TK = 363,
     FALSE_TK = 364,
     BOOL_LIT_TK = 365,
     NULL_TK = 366
   };
#endif
#define PLUS_TK 258
#define MINUS_TK 259
#define MULT_TK 260
#define DIV_TK 261
#define REM_TK 262
#define LS_TK 263
#define SRS_TK 264
#define ZRS_TK 265
#define AND_TK 266
#define XOR_TK 267
#define OR_TK 268
#define BOOL_AND_TK 269
#define BOOL_OR_TK 270
#define EQ_TK 271
#define NEQ_TK 272
#define GT_TK 273
#define GTE_TK 274
#define LT_TK 275
#define LTE_TK 276
#define PLUS_ASSIGN_TK 277
#define MINUS_ASSIGN_TK 278
#define MULT_ASSIGN_TK 279
#define DIV_ASSIGN_TK 280
#define REM_ASSIGN_TK 281
#define LS_ASSIGN_TK 282
#define SRS_ASSIGN_TK 283
#define ZRS_ASSIGN_TK 284
#define AND_ASSIGN_TK 285
#define XOR_ASSIGN_TK 286
#define OR_ASSIGN_TK 287
#define PUBLIC_TK 288
#define PRIVATE_TK 289
#define PROTECTED_TK 290
#define STATIC_TK 291
#define FINAL_TK 292
#define SYNCHRONIZED_TK 293
#define VOLATILE_TK 294
#define TRANSIENT_TK 295
#define NATIVE_TK 296
#define PAD_TK 297
#define ABSTRACT_TK 298
#define MODIFIER_TK 299
#define STRICT_TK 300
#define DECR_TK 301
#define INCR_TK 302
#define DEFAULT_TK 303
#define IF_TK 304
#define THROW_TK 305
#define BOOLEAN_TK 306
#define DO_TK 307
#define IMPLEMENTS_TK 308
#define THROWS_TK 309
#define BREAK_TK 310
#define IMPORT_TK 311
#define ELSE_TK 312
#define INSTANCEOF_TK 313
#define RETURN_TK 314
#define VOID_TK 315
#define CATCH_TK 316
#define INTERFACE_TK 317
#define CASE_TK 318
#define EXTENDS_TK 319
#define FINALLY_TK 320
#define SUPER_TK 321
#define WHILE_TK 322
#define CLASS_TK 323
#define SWITCH_TK 324
#define CONST_TK 325
#define TRY_TK 326
#define FOR_TK 327
#define NEW_TK 328
#define CONTINUE_TK 329
#define GOTO_TK 330
#define PACKAGE_TK 331
#define THIS_TK 332
#define ASSERT_TK 333
#define BYTE_TK 334
#define SHORT_TK 335
#define INT_TK 336
#define LONG_TK 337
#define CHAR_TK 338
#define INTEGRAL_TK 339
#define FLOAT_TK 340
#define DOUBLE_TK 341
#define FP_TK 342
#define ID_TK 343
#define REL_QM_TK 344
#define REL_CL_TK 345
#define NOT_TK 346
#define NEG_TK 347
#define ASSIGN_ANY_TK 348
#define ASSIGN_TK 349
#define OP_TK 350
#define CP_TK 351
#define OCB_TK 352
#define CCB_TK 353
#define OSB_TK 354
#define CSB_TK 355
#define SC_TK 356
#define C_TK 357
#define DOT_TK 358
#define STRING_LIT_TK 359
#define CHAR_LIT_TK 360
#define INT_LIT_TK 361
#define FP_LIT_TK 362
#define TRUE_TK 363
#define FALSE_TK 364
#define BOOL_LIT_TK 365
#define NULL_TK 366




/* Copy the first part of user declarations.  */
#line 5 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"

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


/* Enabling traces.  */
#ifndef YYDEBUG
# define YYDEBUG 1
#endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 0
#endif

#ifndef YYSTYPE
#line 99 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
typedef union {
  char *node;
  struct method_declarator *declarator;
  int value;			/* For modifiers */
} yystype;
/* Line 193 of /usr/share/bison/yacc.c.  */
#line 395 "ps14046.c"
# define YYSTYPE yystype
# define YYSTYPE_IS_TRIVIAL 1
#endif

#ifndef YYLTYPE
typedef struct yyltype
{
  int first_line;
  int first_column;
  int last_line;
  int last_column;
} yyltype;
# define YYLTYPE yyltype
# define YYLTYPE_IS_TRIVIAL 1
#endif

/* Copy the second part of user declarations.  */
#line 105 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"

extern int flag_assert;

#include "lex.c"


/* Line 213 of /usr/share/bison/yacc.c.  */
#line 421 "ps14046.c"

#if ! defined (yyoverflow) || YYERROR_VERBOSE

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
#endif /* ! defined (yyoverflow) || YYERROR_VERBOSE */


#if (! defined (yyoverflow) \
     && (! defined (__cplusplus) \
	 || (YYLTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  short yyss;
  YYSTYPE yyvs;
  };

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAX (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (short) + sizeof (YYSTYPE))				\
      + YYSTACK_GAP_MAX)

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
	    (To)[yyi] = (From)[yyi];	\
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

#if defined (__STDC__) || defined (__cplusplus)
   typedef signed char yysigned_char;
#else
   typedef short yysigned_char;
#endif

/* YYFINAL -- State number of the termination state. */
#define YYFINAL  28
#define YYLAST   3334

/* YYNTOKENS -- Number of terminals. */
#define YYNTOKENS  112
/* YYNNTS -- Number of nonterminals. */
#define YYNNTS  154
/* YYNRULES -- Number of rules. */
#define YYNRULES  357
/* YYNRULES -- Number of states. */
#define YYNSTATES  616

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   366

#define YYTRANSLATE(X) \
  ((unsigned)(X) <= YYMAXUTOK ? yytranslate[X] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const unsigned char yytranslate[] =
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
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86,    87,    88,    89,    90,    91,    92,    93,    94,
      95,    96,    97,    98,    99,   100,   101,   102,   103,   104,
     105,   106,   107,   108,   109,   110,   111
};

#if YYDEBUG
/* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
   YYRHS.  */
static const unsigned short yyprhs[] =
{
       0,     0,     3,     5,     7,     9,    11,    13,    15,    17,
      19,    21,    23,    25,    27,    29,    31,    33,    35,    37,
      40,    43,    45,    47,    49,    53,    55,    56,    58,    60,
      62,    65,    68,    71,    75,    77,    80,    82,    85,    89,
      91,    93,    97,   103,   105,   107,   109,   111,   114,   115,
     123,   124,   131,   132,   135,   136,   139,   141,   145,   148,
     152,   154,   157,   159,   161,   163,   165,   167,   169,   171,
     173,   175,   179,   184,   186,   190,   192,   196,   198,   202,
     204,   206,   207,   211,   215,   219,   224,   229,   233,   238,
     242,   244,   248,   251,   255,   256,   259,   261,   265,   267,
     269,   272,   274,   278,   283,   288,   294,   298,   303,   306,
     310,   314,   319,   324,   330,   338,   345,   347,   349,   350,
     355,   356,   362,   363,   369,   370,   377,   380,   384,   387,
     391,   393,   396,   398,   400,   402,   404,   406,   409,   412,
     416,   420,   425,   427,   431,   434,   438,   440,   443,   445,
     447,   449,   452,   455,   459,   461,   463,   465,   467,   469,
     471,   473,   475,   477,   479,   481,   483,   485,   487,   489,
     491,   493,   495,   497,   499,   501,   503,   505,   507,   510,
     513,   516,   519,   521,   523,   525,   527,   529,   531,   533,
     539,   547,   555,   561,   564,   568,   572,   577,   579,   582,
     585,   587,   590,   594,   597,   602,   605,   608,   610,   618,
     626,   633,   641,   648,   651,   654,   655,   657,   659,   660,
     662,   664,   668,   671,   675,   678,   682,   685,   689,   693,
     699,   703,   706,   710,   716,   722,   724,   728,   732,   737,
     739,   742,   748,   751,   753,   755,   757,   759,   763,   765,
     767,   769,   771,   773,   777,   781,   785,   789,   793,   799,
     804,   806,   811,   817,   823,   830,   831,   838,   839,   847,
     851,   855,   857,   861,   865,   869,   873,   878,   883,   888,
     893,   895,   898,   902,   905,   909,   913,   917,   921,   926,
     932,   939,   945,   952,   957,   962,   964,   966,   968,   970,
     973,   976,   978,   980,   983,   986,   988,   991,   994,   996,
     999,  1002,  1004,  1010,  1015,  1020,  1026,  1028,  1032,  1036,
    1040,  1042,  1046,  1050,  1052,  1056,  1060,  1064,  1066,  1070,
    1074,  1078,  1082,  1086,  1088,  1092,  1096,  1098,  1102,  1104,
    1108,  1110,  1114,  1116,  1120,  1122,  1126,  1128,  1134,  1136,
    1138,  1142,  1144,  1146,  1148,  1150,  1152,  1154
};

/* YYRHS -- A `-1'-separated list of the rules' RHS. */
static const short yyrhs[] =
{
     113,     0,    -1,   126,    -1,   106,    -1,   107,    -1,   110,
      -1,   105,    -1,   104,    -1,   111,    -1,   116,    -1,   117,
      -1,    84,    -1,    87,    -1,    51,    -1,   118,    -1,   121,
      -1,   122,    -1,   118,    -1,   118,    -1,   116,   237,    -1,
     122,   237,    -1,   123,    -1,   124,    -1,   125,    -1,   122,
     103,   125,    -1,    88,    -1,    -1,   129,    -1,   127,    -1,
     128,    -1,   129,   127,    -1,   129,   128,    -1,   127,   128,
      -1,   129,   127,   128,    -1,   130,    -1,   127,   130,    -1,
     133,    -1,   128,   133,    -1,    76,   122,   101,    -1,   131,
      -1,   132,    -1,    56,   122,   101,    -1,    56,   122,   103,
       5,   101,    -1,   135,    -1,   166,    -1,   187,    -1,    44,
      -1,   134,    44,    -1,    -1,   134,    68,   125,   138,   139,
     136,   141,    -1,    -1,    68,   125,   138,   139,   137,   141,
      -1,    -1,    64,   119,    -1,    -1,    53,   140,    -1,   120,
      -1,   140,   102,   120,    -1,    97,    98,    -1,    97,   142,
      98,    -1,   143,    -1,   142,   143,    -1,   144,    -1,   159,
      -1,   161,    -1,   179,    -1,   145,    -1,   150,    -1,   135,
      -1,   166,    -1,   187,    -1,   115,   146,   101,    -1,   134,
     115,   146,   101,    -1,   147,    -1,   146,   102,   147,    -1,
     148,    -1,   148,    94,   149,    -1,   125,    -1,   148,    99,
     100,    -1,   264,    -1,   177,    -1,    -1,   152,   151,   158,
      -1,   115,   153,   156,    -1,    60,   153,   156,    -1,   134,
     115,   153,   156,    -1,   134,    60,   153,   156,    -1,   125,
      95,    96,    -1,   125,    95,   154,    96,    -1,   153,    99,
     100,    -1,   155,    -1,   154,   102,   155,    -1,   115,   148,
      -1,   134,   115,   148,    -1,    -1,    54,   157,    -1,   119,
      -1,   157,   102,   119,    -1,   179,    -1,   101,    -1,   160,
     179,    -1,    44,    -1,   162,   156,   163,    -1,   134,   162,
     156,   163,    -1,   162,   156,   163,   101,    -1,   134,   162,
     156,   163,   101,    -1,   123,    95,    96,    -1,   123,    95,
     154,    96,    -1,    97,    98,    -1,    97,   164,    98,    -1,
      97,   180,    98,    -1,    97,   164,   180,    98,    -1,   165,
      95,    96,   101,    -1,   165,    95,   233,    96,   101,    -1,
     122,   103,    66,    95,   233,    96,   101,    -1,   122,   103,
      66,    95,    96,   101,    -1,    77,    -1,    66,    -1,    -1,
      62,   125,   167,   172,    -1,    -1,   134,    62,   125,   168,
     172,    -1,    -1,    62,   125,   171,   169,   172,    -1,    -1,
     134,    62,   125,   171,   170,   172,    -1,    64,   120,    -1,
     171,   102,   120,    -1,    97,    98,    -1,    97,   173,    98,
      -1,   174,    -1,   173,   174,    -1,   175,    -1,   176,    -1,
     135,    -1,   166,    -1,   145,    -1,   152,   101,    -1,    97,
      98,    -1,    97,   178,    98,    -1,    97,   102,    98,    -1,
      97,   178,   102,    98,    -1,   149,    -1,   178,   102,   149,
      -1,    97,    98,    -1,    97,   180,    98,    -1,   181,    -1,
     180,   181,    -1,   182,    -1,   184,    -1,   135,    -1,   183,
     101,    -1,   115,   146,    -1,   134,   115,   146,    -1,   186,
      -1,   189,    -1,   193,    -1,   194,    -1,   203,    -1,   207,
      -1,   186,    -1,   190,    -1,   195,    -1,   204,    -1,   208,
      -1,   179,    -1,   187,    -1,   191,    -1,   196,    -1,   206,
      -1,   214,    -1,   215,    -1,   216,    -1,   219,    -1,   217,
      -1,   221,    -1,   218,    -1,   101,    -1,   125,    90,    -1,
     188,   184,    -1,   188,   185,    -1,   192,   101,    -1,   261,
      -1,   245,    -1,   246,    -1,   242,    -1,   243,    -1,   239,
      -1,   228,    -1,    49,    95,   264,    96,   184,    -1,    49,
      95,   264,    96,   185,    57,   184,    -1,    49,    95,   264,
      96,   185,    57,   185,    -1,    69,    95,   264,    96,   197,
      -1,    97,    98,    -1,    97,   200,    98,    -1,    97,   198,
      98,    -1,    97,   198,   200,    98,    -1,   199,    -1,   198,
     199,    -1,   200,   180,    -1,   201,    -1,   200,   201,    -1,
      63,   265,    90,    -1,    48,    90,    -1,    67,    95,   264,
      96,    -1,   202,   184,    -1,   202,   185,    -1,    52,    -1,
     205,   184,    67,    95,   264,    96,   101,    -1,   210,   101,
     264,   101,   212,    96,   184,    -1,   210,   101,   101,   212,
      96,   184,    -1,   210,   101,   264,   101,   212,    96,   185,
      -1,   210,   101,   101,   212,    96,   185,    -1,    72,    95,
      -1,   209,   211,    -1,    -1,   213,    -1,   183,    -1,    -1,
     213,    -1,   192,    -1,   213,   102,   192,    -1,    55,   101,
      -1,    55,   125,   101,    -1,    74,   101,    -1,    74,   125,
     101,    -1,    59,   101,    -1,    59,   264,   101,    -1,    50,
     264,   101,    -1,    78,   264,    90,   264,   101,    -1,    78,
     264,   101,    -1,    78,     1,    -1,    78,   264,     1,    -1,
     220,    95,   264,    96,   179,    -1,   220,    95,   264,    96,
       1,    -1,    44,    -1,    71,   179,   222,    -1,    71,   179,
     224,    -1,    71,   179,   222,   224,    -1,   223,    -1,   222,
     223,    -1,    61,    95,   155,    96,   179,    -1,    65,   179,
      -1,   226,    -1,   234,    -1,   114,    -1,    77,    -1,    95,
     264,    96,    -1,   228,    -1,   238,    -1,   239,    -1,   240,
      -1,   227,    -1,   122,   103,    77,    -1,   122,   103,    68,
      -1,   121,   103,    68,    -1,   116,   103,    68,    -1,    60,
     103,    68,    -1,    73,   119,    95,   233,    96,    -1,    73,
     119,    95,    96,    -1,   229,    -1,   232,   125,    95,    96,
      -1,   232,   125,    95,    96,   141,    -1,   232,   125,    95,
     233,    96,    -1,   232,   125,    95,   233,    96,   141,    -1,
      -1,    73,   119,    95,    96,   230,   141,    -1,    -1,    73,
     119,    95,   233,    96,   231,   141,    -1,   122,   103,    73,
      -1,   225,   103,    73,    -1,   264,    -1,   233,   102,   264,
      -1,   233,   102,     1,    -1,    73,   116,   235,    -1,    73,
     118,   235,    -1,    73,   116,   235,   237,    -1,    73,   118,
     235,   237,    -1,    73,   118,   237,   177,    -1,    73,   116,
     237,   177,    -1,   236,    -1,   235,   236,    -1,    99,   264,
     100,    -1,    99,   100,    -1,   237,    99,   100,    -1,   225,
     103,   125,    -1,    66,   103,   125,    -1,   122,    95,    96,
      -1,   122,    95,   233,    96,    -1,   225,   103,   125,    95,
      96,    -1,   225,   103,   125,    95,   233,    96,    -1,    66,
     103,   125,    95,    96,    -1,    66,   103,   125,    95,   233,
      96,    -1,   122,    99,   264,   100,    -1,   226,    99,   264,
     100,    -1,   225,    -1,   122,    -1,   242,    -1,   243,    -1,
     241,    47,    -1,   241,    46,    -1,   245,    -1,   246,    -1,
       3,   244,    -1,     4,   244,    -1,   247,    -1,    47,   244,
      -1,    46,   244,    -1,   241,    -1,    91,   244,    -1,    92,
     244,    -1,   248,    -1,    95,   116,   237,    96,   244,    -1,
      95,   116,    96,   244,    -1,    95,   264,    96,   247,    -1,
      95,   122,   237,    96,   247,    -1,   244,    -1,   249,     5,
     244,    -1,   249,     6,   244,    -1,   249,     7,   244,    -1,
     249,    -1,   250,     3,   249,    -1,   250,     4,   249,    -1,
     250,    -1,   251,     8,   250,    -1,   251,     9,   250,    -1,
     251,    10,   250,    -1,   251,    -1,   252,    20,   251,    -1,
     252,    18,   251,    -1,   252,    21,   251,    -1,   252,    19,
     251,    -1,   252,    58,   117,    -1,   252,    -1,   253,    16,
     252,    -1,   253,    17,   252,    -1,   253,    -1,   254,    11,
     253,    -1,   254,    -1,   255,    12,   254,    -1,   255,    -1,
     256,    13,   255,    -1,   256,    -1,   257,    14,   256,    -1,
     257,    -1,   258,    15,   257,    -1,   258,    -1,   258,    89,
     264,    90,   259,    -1,   259,    -1,   261,    -1,   262,   263,
     260,    -1,   122,    -1,   238,    -1,   240,    -1,    93,    -1,
      94,    -1,   260,    -1,   264,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const unsigned short yyrline[] =
{
       0,   177,   177,   182,   184,   185,   186,   187,   188,   192,
     194,   197,   203,   208,   215,   217,   220,   224,   228,   232,
     238,   246,   248,   251,   255,   262,   267,   268,   269,   270,
     271,   272,   273,   274,   277,   279,   282,   284,   287,   292,
     294,   297,   301,   305,   307,   308,   312,   321,   334,   332,
     340,   339,   344,   345,   348,   349,   352,   355,   359,   362,
     366,   368,   371,   373,   374,   375,   378,   380,   381,   382,
     383,   387,   390,   394,   397,   400,   402,   405,   408,   412,
     414,   420,   418,   425,   428,   429,   431,   438,   445,   451,
     454,   456,   462,   478,   494,   495,   498,   501,   505,   507,
     511,   515,   522,   524,   527,   529,   534,   537,   541,   543,
     544,   545,   549,   551,   553,   555,   559,   561,   568,   566,
     571,   570,   574,   573,   577,   576,   581,   583,   586,   589,
     593,   595,   598,   600,   601,   602,   605,   609,   614,   616,
     617,   618,   621,   623,   627,   629,   632,   634,   637,   639,
     640,   643,   647,   650,   654,   656,   657,   658,   659,   660,
     663,   665,   666,   667,   668,   671,   673,   674,   675,   676,
     677,   678,   679,   680,   681,   682,   683,   686,   690,   695,
     699,   704,   708,   710,   711,   712,   713,   714,   715,   718,
     722,   727,   732,   736,   738,   739,   740,   743,   745,   748,
     753,   755,   758,   760,   763,   767,   771,   775,   779,   784,
     786,   789,   791,   794,   798,   801,   802,   803,   806,   807,
     810,   812,   815,   817,   821,   823,   826,   828,   831,   835,
     837,   838,   840,   843,   845,   848,   853,   855,   856,   859,
     861,   864,   868,   873,   875,   878,   880,   881,   882,   883,
     884,   885,   886,   888,   892,   895,   897,   899,   903,   905,
     906,   907,   908,   909,   910,   915,   913,   918,   917,   922,
     925,   928,   930,   931,   934,   936,   937,   938,   940,   941,
     944,   946,   949,   953,   956,   960,   962,   966,   969,   971,
     972,   973,   974,   977,   980,   983,   985,   987,   988,   991,
     995,   999,  1001,  1002,  1003,  1004,  1007,  1011,  1015,  1017,
    1018,  1019,  1022,  1024,  1025,  1026,  1029,  1031,  1032,  1033,
    1036,  1038,  1039,  1042,  1044,  1045,  1046,  1049,  1051,  1052,
    1053,  1054,  1055,  1058,  1060,  1061,  1064,  1066,  1069,  1071,
    1074,  1076,  1079,  1081,  1085,  1087,  1091,  1093,  1097,  1099,
    1102,  1106,  1109,  1110,  1113,  1115,  1118,  1122
};
#endif

#if YYDEBUG || YYERROR_VERBOSE
/* YYTNME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals. */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "PLUS_TK", "MINUS_TK", "MULT_TK", "DIV_TK", 
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
  "TRUE_TK", "FALSE_TK", "BOOL_LIT_TK", "NULL_TK", "$accept", "goal", 
  "literal", "type", "primitive_type", "reference_type", 
  "class_or_interface_type", "class_type", "interface_type", "array_type", 
  "name", "simple_name", "qualified_name", "identifier", 
  "compilation_unit", "import_declarations", "type_declarations", 
  "package_declaration", "import_declaration", 
  "single_type_import_declaration", "type_import_on_demand_declaration", 
  "type_declaration", "modifiers", "class_declaration", "@1", "@2", 
  "super", "interfaces", "interface_type_list", "class_body", 
  "class_body_declarations", "class_body_declaration", 
  "class_member_declaration", "field_declaration", "variable_declarators", 
  "variable_declarator", "variable_declarator_id", "variable_initializer", 
  "method_declaration", "@3", "method_header", "method_declarator", 
  "formal_parameter_list", "formal_parameter", "throws", 
  "class_type_list", "method_body", "static_initializer", "static", 
  "constructor_declaration", "constructor_declarator", "constructor_body", 
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

# ifdef YYPRINT
/* YYTOKNUM[YYLEX-NUM] -- Internal token number corresponding to
   token YYLEX-NUM.  */
static const unsigned short yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,   288,   289,   290,   291,   292,   293,   294,
     295,   296,   297,   298,   299,   300,   301,   302,   303,   304,
     305,   306,   307,   308,   309,   310,   311,   312,   313,   314,
     315,   316,   317,   318,   319,   320,   321,   322,   323,   324,
     325,   326,   327,   328,   329,   330,   331,   332,   333,   334,
     335,   336,   337,   338,   339,   340,   341,   342,   343,   344,
     345,   346,   347,   348,   349,   350,   351,   352,   353,   354,
     355,   356,   357,   358,   359,   360,   361,   362,   363,   364,
     365,   366
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const unsigned short yyr1[] =
{
       0,   112,   113,   114,   114,   114,   114,   114,   114,   115,
     115,   116,   116,   116,   117,   117,   118,   119,   120,   121,
     121,   122,   122,   123,   124,   125,   126,   126,   126,   126,
     126,   126,   126,   126,   127,   127,   128,   128,   129,   130,
     130,   131,   132,   133,   133,   133,   134,   134,   136,   135,
     137,   135,   138,   138,   139,   139,   140,   140,   141,   141,
     142,   142,   143,   143,   143,   143,   144,   144,   144,   144,
     144,   145,   145,   146,   146,   147,   147,   148,   148,   149,
     149,   151,   150,   152,   152,   152,   152,   153,   153,   153,
     154,   154,   155,   155,   156,   156,   157,   157,   158,   158,
     159,   160,   161,   161,   161,   161,   162,   162,   163,   163,
     163,   163,   164,   164,   164,   164,   165,   165,   167,   166,
     168,   166,   169,   166,   170,   166,   171,   171,   172,   172,
     173,   173,   174,   174,   174,   174,   175,   176,   177,   177,
     177,   177,   178,   178,   179,   179,   180,   180,   181,   181,
     181,   182,   183,   183,   184,   184,   184,   184,   184,   184,
     185,   185,   185,   185,   185,   186,   186,   186,   186,   186,
     186,   186,   186,   186,   186,   186,   186,   187,   188,   189,
     190,   191,   192,   192,   192,   192,   192,   192,   192,   193,
     194,   195,   196,   197,   197,   197,   197,   198,   198,   199,
     200,   200,   201,   201,   202,   203,   204,   205,   206,   207,
     207,   208,   208,   209,   210,   211,   211,   211,   212,   212,
     213,   213,   214,   214,   215,   215,   216,   216,   217,   218,
     218,   218,   218,   219,   219,   220,   221,   221,   221,   222,
     222,   223,   224,   225,   225,   226,   226,   226,   226,   226,
     226,   226,   226,   226,   227,   227,   227,   227,   228,   228,
     228,   228,   228,   228,   228,   230,   229,   231,   229,   232,
     232,   233,   233,   233,   234,   234,   234,   234,   234,   234,
     235,   235,   236,   237,   237,   238,   238,   239,   239,   239,
     239,   239,   239,   240,   240,   241,   241,   241,   241,   242,
     243,   244,   244,   244,   244,   244,   245,   246,   247,   247,
     247,   247,   248,   248,   248,   248,   249,   249,   249,   249,
     250,   250,   250,   251,   251,   251,   251,   252,   252,   252,
     252,   252,   252,   253,   253,   253,   254,   254,   255,   255,
     256,   256,   257,   257,   258,   258,   259,   259,   260,   260,
     261,   262,   262,   262,   263,   263,   264,   265
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const unsigned char yyr2[] =
{
       0,     2,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     2,
       2,     1,     1,     1,     3,     1,     0,     1,     1,     1,
       2,     2,     2,     3,     1,     2,     1,     2,     3,     1,
       1,     3,     5,     1,     1,     1,     1,     2,     0,     7,
       0,     6,     0,     2,     0,     2,     1,     3,     2,     3,
       1,     2,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     3,     4,     1,     3,     1,     3,     1,     3,     1,
       1,     0,     3,     3,     3,     4,     4,     3,     4,     3,
       1,     3,     2,     3,     0,     2,     1,     3,     1,     1,
       2,     1,     3,     4,     4,     5,     3,     4,     2,     3,
       3,     4,     4,     5,     7,     6,     1,     1,     0,     4,
       0,     5,     0,     5,     0,     6,     2,     3,     2,     3,
       1,     2,     1,     1,     1,     1,     1,     2,     2,     3,
       3,     4,     1,     3,     2,     3,     1,     2,     1,     1,
       1,     2,     2,     3,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     2,     2,
       2,     2,     1,     1,     1,     1,     1,     1,     1,     5,
       7,     7,     5,     2,     3,     3,     4,     1,     2,     2,
       1,     2,     3,     2,     4,     2,     2,     1,     7,     7,
       6,     7,     6,     2,     2,     0,     1,     1,     0,     1,
       1,     3,     2,     3,     2,     3,     2,     3,     3,     5,
       3,     2,     3,     5,     5,     1,     3,     3,     4,     1,
       2,     5,     2,     1,     1,     1,     1,     3,     1,     1,
       1,     1,     1,     3,     3,     3,     3,     3,     5,     4,
       1,     4,     5,     5,     6,     0,     6,     0,     7,     3,
       3,     1,     3,     3,     3,     3,     4,     4,     4,     4,
       1,     2,     3,     2,     3,     3,     3,     3,     4,     5,
       6,     5,     6,     4,     4,     1,     1,     1,     1,     2,
       2,     1,     1,     2,     2,     1,     2,     2,     1,     2,
       2,     1,     5,     4,     4,     5,     1,     3,     3,     3,
       1,     3,     3,     1,     3,     3,     3,     1,     3,     3,
       3,     3,     3,     1,     3,     3,     1,     3,     1,     3,
       1,     3,     1,     3,     1,     3,     1,     5,     1,     1,
       3,     1,     1,     1,     1,     1,     1,     1
};

/* YYDEFACT[STATE-NAME] -- Default rule to reduce with in state
   STATE-NUM when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const unsigned short yydefact[] =
{
      26,    46,     0,     0,     0,     0,   177,     0,     2,    28,
      29,    27,    34,    39,    40,    36,     0,    43,    44,    45,
      25,     0,    21,    22,    23,   118,    52,     0,     1,    32,
      35,    37,    30,    31,    47,     0,     0,    41,     0,     0,
       0,   122,     0,    54,    38,     0,    33,   120,    52,     0,
      24,    18,   126,    16,     0,   119,     0,     0,    17,    53,
       0,    50,     0,   124,    54,    42,    13,     0,    11,    12,
     128,     0,     9,    10,    14,    15,    16,     0,   134,   136,
       0,   135,     0,   130,   132,   133,   127,   123,    56,    55,
       0,   121,     0,    48,     0,    94,    77,     0,    73,    75,
      94,     0,    19,    20,     0,     0,   137,   129,   131,     0,
       0,    51,   125,     0,     0,     0,     0,    84,    71,     0,
       0,     0,    83,   283,     0,    94,     0,    94,    57,    46,
       0,    58,    21,     0,    68,     0,    60,    62,    66,    67,
      81,    63,     0,    64,    94,    69,    65,    70,    49,    87,
       0,     0,     0,    90,    96,    95,    89,    77,    74,     0,
       0,     0,     0,     0,     0,     0,   246,     0,     0,     0,
       0,     7,     6,     3,     4,     5,     8,   245,     0,     0,
     296,    76,    80,   295,   243,   252,   248,   260,     0,   244,
     249,   250,   251,   308,   297,   298,   316,   301,   302,   305,
     311,   320,   323,   327,   333,   336,   338,   340,   342,   344,
     346,   348,   356,   349,     0,    79,    78,   284,    86,    72,
      85,    46,     0,     0,   207,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   144,     0,     9,    15,   296,    23,
       0,   150,   165,     0,   146,   148,     0,   149,   154,   166,
       0,   155,   167,     0,   156,   157,   168,     0,   158,     0,
     169,   159,   215,     0,   170,   171,   172,   174,   176,   173,
       0,   175,   248,   250,     0,   185,   186,   183,   184,   182,
       0,    94,    59,    61,     0,   100,     0,    92,     0,    88,
       0,     0,   296,   249,   251,   303,   304,   307,   306,     0,
       0,     0,    17,     0,   309,   310,     0,   296,     0,   138,
       0,   142,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   300,   299,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   354,   355,     0,     0,     0,   222,
       0,   226,     0,     0,     0,     0,   213,   224,     0,   231,
       0,     0,   152,   178,     0,   145,   147,   151,   235,   179,
     181,   205,     0,     0,   217,   220,   214,   216,     0,     0,
     106,     0,     0,    99,    82,    98,     0,   102,    93,    91,
      97,   257,   286,     0,   274,   280,     0,   275,     0,     0,
       0,    19,    20,   247,   140,   139,     0,   256,   255,   287,
       0,   271,     0,   254,   269,   253,   270,   285,     0,     0,
     317,   318,   319,   321,   322,   324,   325,   326,   329,   331,
     328,   330,     0,   332,   334,   335,   337,   339,   341,   343,
     345,     0,   350,     0,   228,   223,   227,     0,     0,     0,
       0,   236,   239,   237,   225,   232,     0,   230,   247,   153,
       0,     0,   218,     0,     0,   107,   103,   117,   246,   108,
     296,     0,     0,     0,   104,     0,     0,   281,   276,   279,
     277,   278,   259,     0,   313,     0,     0,   314,   141,   143,
     288,     0,   293,     0,   294,   261,     0,     0,     0,   204,
       0,     0,   242,   240,   238,     0,     0,   221,     0,   219,
     218,     0,   105,     0,   109,     0,     0,   110,   291,     0,
     282,     0,   258,   312,   315,   273,   272,   289,     0,   262,
     263,   347,     0,   189,     0,   154,     0,   161,   162,     0,
     163,   164,     0,     0,   192,     0,   229,     0,     0,     0,
     234,   233,     0,   111,     0,     0,   292,   266,     0,   290,
     264,     0,     0,   180,   206,     0,     0,     0,   193,     0,
     197,     0,   200,     0,     0,   210,     0,     0,   112,     0,
     268,     0,   190,   218,     0,   203,   357,     0,   195,   198,
       0,   194,   199,   201,   241,   208,   209,     0,     0,   113,
       0,     0,   218,   202,   196,   115,     0,     0,     0,     0,
     114,     0,   212,     0,   191,   211
};

/* YYDEFGOTO[NTERM-NUM]. */
static const short yydefgoto[] =
{
      -1,     7,   177,   235,   178,    73,    74,    59,    52,   179,
     180,    22,    23,    24,     8,     9,    10,    11,    12,    13,
      14,    15,   240,   241,   113,    90,    43,    61,    89,   111,
     135,   136,   137,    79,    97,    98,    99,   181,   139,   284,
      80,    95,   152,   153,   117,   155,   384,   141,   142,   143,
     144,   387,   471,   472,    18,    40,    62,    57,    92,    41,
      55,    82,    83,    84,    85,   182,   312,   242,   592,   244,
     245,   246,   247,   534,   248,   249,   250,   251,   537,   252,
     253,   254,   255,   538,   256,   544,   569,   570,   571,   572,
     257,   258,   540,   259,   260,   261,   541,   262,   263,   376,
     508,   509,   264,   265,   266,   267,   268,   269,   270,   271,
     451,   452,   453,   183,   184,   185,   186,   187,   521,   558,
     188,   410,   189,   394,   395,   103,   190,   191,   192,   193,
     194,   195,   196,   197,   198,   199,   200,   201,   202,   203,
     204,   205,   206,   207,   208,   209,   210,   211,   212,   213,
     214,   346,   411,   587
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -465
static const short yypact[] =
{
     237,  -465,   -32,   -32,   -32,   -32,  -465,    83,  -465,   124,
      -8,   124,  -465,  -465,  -465,  -465,   183,  -465,  -465,  -465,
    -465,   -38,  -465,  -465,  -465,    51,    97,   276,  -465,    -8,
    -465,  -465,   124,    -8,  -465,   -32,   -32,  -465,    17,   -32,
     134,    42,   -32,   150,  -465,   -32,    -8,    51,    97,   191,
    -465,  -465,  -465,   158,   297,  -465,   -32,   134,  -465,  -465,
     -32,  -465,   134,    42,   150,  -465,  -465,   -32,  -465,  -465,
    -465,   -32,   212,  -465,  -465,  -465,   160,   740,  -465,  -465,
     223,  -465,   549,  -465,  -465,  -465,  -465,  -465,  -465,   232,
     272,  -465,   134,  -465,   285,    32,   285,    68,  -465,   -14,
      32,   289,   293,   293,   -32,   -32,  -465,  -465,  -465,   -32,
     678,  -465,  -465,   272,   121,   -32,   305,  -465,  -465,   -32,
    1690,   328,  -465,  -465,   349,    32,   147,    32,  -465,   320,
    2602,  -465,   358,   740,  -465,   761,  -465,  -465,  -465,  -465,
    -465,  -465,   363,  -465,   346,  -465,  -465,  -465,  -465,  -465,
     -32,   202,    47,  -465,  -465,   367,  -465,  -465,  -465,  2398,
    2398,  2398,  2398,   362,   369,     0,  -465,  2398,  2398,  2398,
    1558,  -465,  -465,  -465,  -465,  -465,  -465,  -465,   253,   374,
     555,  -465,  -465,   378,   375,  -465,  -465,  -465,   -32,  -465,
      -2,  -465,   392,   467,  -465,  -465,  -465,  -465,  -465,  -465,
    -465,   277,   522,   490,   300,   523,   482,   484,   509,   489,
      23,  -465,  -465,  -465,   464,  -465,  -465,  -465,  -465,  -465,
    -465,   436,   440,  2398,  -465,    93,  1742,   449,   453,   363,
     457,   113,  1424,  2398,  -465,   -32,   253,   374,   461,   460,
     420,  -465,  -465,  2670,  -465,  -465,   475,  -465,  -465,  -465,
    3078,  -465,  -465,   483,  -465,  -465,  -465,  3078,  -465,  3078,
    -465,  -465,  3198,   487,  -465,  -465,  -465,  -465,  -465,  -465,
     499,  -465,   207,   292,   467,   516,   519,  -465,  -465,  -465,
     450,   346,  -465,  -465,   163,  -465,   488,   497,   -32,  -465,
     431,   -32,   241,  -465,  -465,  -465,  -465,  -465,  -465,   530,
     -32,   502,   502,   507,  -465,  -465,   303,   555,   508,  -465,
     514,  -465,   262,   538,   546,  1808,  1860,   239,   114,  2398,
     520,  -465,  -465,  2398,  2398,  2398,  2398,  2398,  2398,  2398,
    2398,  2398,  2398,  2398,  2398,     0,  2398,  2398,  2398,  2398,
    2398,  2398,  2398,  2398,  -465,  -465,  2398,  2398,   517,  -465,
     524,  -465,   526,  2398,  2398,   317,  -465,  -465,   528,  -465,
      28,   534,   518,  -465,   -32,  -465,  -465,  -465,  -465,  -465,
    -465,  -465,   572,   202,  -465,  -465,  -465,   550,  1926,  2398,
    -465,   122,   488,  -465,  -465,  -465,  2738,   540,   497,  -465,
    -465,  -465,   558,  1860,   502,  -465,   327,   502,   327,  1978,
    2398,   -75,    54,  1220,  -465,  -465,  1624,  -465,  -465,  -465,
     142,  -465,   557,  -465,  -465,  -465,  -465,   560,   561,  2044,
    -465,  -465,  -465,   277,   277,   522,   522,   522,   490,   490,
     490,   490,   212,  -465,   300,   300,   523,   482,   484,   509,
     489,   570,  -465,   566,  -465,  -465,  -465,   569,   577,   571,
     363,   317,  -465,  -465,  -465,  -465,  2398,  -465,  -465,   518,
     579,  3223,  3223,   574,   582,  -465,   586,   369,   588,  -465,
     591,  2806,   597,  2874,  -465,  2096,   595,  -465,   293,  -465,
     293,  -465,   602,   208,  -465,  2398,  1220,  -465,  -465,  -465,
    -465,  1490,  -465,  2162,  -465,   272,   249,  2398,  3146,  -465,
     604,   431,  -465,  -465,  -465,   601,  2398,  -465,   607,   550,
    3223,    12,  -465,   455,  -465,  2942,  2214,  -465,  -465,   323,
    -465,   272,   608,  -465,  -465,  -465,  -465,  -465,   325,  -465,
     272,  -465,   609,  -465,   649,   650,  3146,  -465,  -465,  3146,
    -465,  -465,   613,    34,  -465,   612,  -465,   619,  3078,   620,
    -465,  -465,   625,  -465,   629,   355,  -465,  -465,   272,  -465,
    -465,  2398,  3078,  -465,  -465,  2280,   631,  2398,  -465,    58,
    -465,  2466,  -465,   363,   632,  -465,  3078,  2332,  -465,   633,
    -465,   639,  -465,  3223,   638,  -465,  -465,   646,  -465,  -465,
    2534,  -465,  3010,  -465,  -465,  -465,  -465,   641,   371,  -465,
    3146,   647,  3223,  -465,  -465,  -465,   648,   690,  3146,   652,
    -465,  3146,  -465,  3146,  -465,  -465
};

/* YYPGOTO[NTERM-NUM].  */
static const short yypgoto[] =
{
    -465,  -465,  -465,   -11,   -10,   415,   -19,   -87,    19,   240,
     106,   102,  -465,    -3,  -465,   741,    41,  -465,    44,  -465,
    -465,    48,    16,   635,  -465,  -465,   706,   692,  -465,  -108,
    -465,   622,  -465,   -76,  -102,   640,  -136,  -164,  -465,  -465,
      29,    86,   478,  -282,   -70,  -465,  -465,  -465,  -465,  -465,
     627,   382,  -465,  -465,     7,  -465,  -465,  -465,  -465,   721,
     101,  -465,   689,  -465,  -465,    60,  -465,   -95,  -128,  -239,
    -465,   511,   468,  -313,  -429,   167,  -210,  -465,  -465,  -465,
    -255,  -465,  -465,  -465,  -465,  -465,  -465,   209,   211,  -436,
    -124,  -465,  -465,  -465,  -465,  -465,  -465,  -465,   -84,  -465,
    -464,   515,  -465,  -465,  -465,  -465,  -465,  -465,  -465,  -465,
    -465,   330,   332,  -465,  -465,  -465,    80,  -465,  -465,  -465,
    -465,  -380,  -465,   485,  -199,    65,  1069,   161,  1122,   324,
     381,   469,  -150,   539,   594,  -372,  -465,   243,     3,    76,
     235,   452,   454,   448,   451,   462,  -465,   302,   463,   752,
    -465,  -465,   873,  -465
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If zero, do what YYDEFACT says.
   If YYTABLE_NINF, parse error.  */
#define YYTABLE_NINF -354
static const short yytable[] =
{
      25,    26,   243,   126,   366,   148,   311,   375,   389,   295,
     296,   297,   298,   550,   287,   146,    16,   304,   305,   483,
      51,   485,    49,    58,   124,    16,    16,    16,   154,   455,
     122,   487,    47,    48,   138,    50,     1,    51,   342,   496,
     146,    51,    50,    71,    72,    16,   549,   285,    16,    16,
      29,    66,    33,    30,     3,   218,    20,   220,    31,   138,
       4,    81,    16,    37,    94,    38,   105,    72,    96,   535,
      77,    71,    72,    46,   286,    86,    30,    31,   303,    88,
     120,    31,   566,    28,    68,   121,   115,    69,    20,    81,
      51,  -352,  -352,     6,    31,   519,    58,   567,    77,    71,
      72,    94,    96,   150,    72,    20,   566,   535,    21,   130,
     535,    27,   343,   528,   524,    39,   157,   145,   456,   601,
     236,   567,   105,    72,    71,    72,   133,   239,   128,   457,
     151,   116,   568,   362,   355,   593,   555,   102,   609,   140,
     288,    72,   145,   289,    56,    53,   302,   157,    53,   290,
     486,   133,   388,   124,   593,   301,   588,   100,    87,   306,
      76,    42,    53,    91,   140,     1,    53,    19,     1,   118,
     119,   535,    66,   420,   421,   422,    19,    19,    19,   535,
       2,    20,   535,    76,   535,   320,     3,   416,    76,   385,
     125,   127,     4,   112,   349,   477,    19,   598,   477,    19,
      19,    20,    20,    60,   390,    68,   507,   375,    69,    20,
     272,   382,   132,    19,   357,    53,    76,   149,   465,   545,
      76,    53,   350,   563,   290,     6,   564,    34,   358,   364,
      72,    54,   157,   236,   366,   132,   238,   132,   490,    76,
     239,    76,   489,   102,   491,    35,    34,   239,   219,   119,
     484,    36,   236,    66,   239,   375,   239,    76,   473,   101,
     130,    45,   459,    45,   383,   292,   292,   292,   292,   150,
      72,    53,    58,   292,   292,   307,   366,   147,   373,   150,
      72,     1,   323,   324,   325,   157,    68,   607,   536,    69,
      20,   273,    65,     2,    75,   612,   151,   392,   614,     3,
     615,   102,   147,  -188,   522,     4,   151,   413,  -188,  -188,
     491,   101,   414,     5,    50,   417,   415,    75,   331,   332,
     333,   334,    75,   272,   106,   432,   536,    20,   375,   536,
     272,   425,   426,   427,   109,   523,   315,   272,     6,   272,
     316,     1,   272,   515,   317,   530,    76,   375,    66,   238,
      75,   491,   101,   366,    75,   502,   313,    67,   335,     3,
     405,   157,   364,    72,   406,     4,   396,   398,   238,   110,
     237,   401,   402,    75,   539,    75,   236,    44,   449,    45,
     114,    68,   450,   239,    69,    20,    76,   529,  -187,   123,
     536,    75,   124,  -187,  -187,    70,    76,    53,   536,   400,
     115,   536,   101,   536,   273,   156,   313,   428,   429,   430,
     431,   273,   539,   557,   542,   539,   551,  -101,   273,   556,
     273,   559,   560,   273,   170,   491,   124,   491,   216,   292,
     292,   292,   292,   292,   292,   292,   292,   292,   292,   292,
     292,    76,   292,   292,   292,   292,   292,   292,   292,   217,
     580,   579,   542,   280,   274,   542,   479,   491,   481,   478,
     130,   236,   480,   236,    34,   299,   272,   606,   239,   291,
     239,    66,   300,   491,   319,     1,   539,   314,   594,    76,
      75,   318,    66,   237,   539,  -353,  -353,   539,    36,   539,
     150,    72,   470,   338,     1,   239,   339,   102,   328,   329,
     330,    66,   237,   341,    68,   236,   292,    69,    20,   292,
      50,   275,   239,   321,   322,    68,   542,   151,    69,    20,
      75,   552,   340,   413,   542,   326,   327,   542,   414,   542,
      75,  -235,   415,   239,    68,   347,   239,    69,    20,   336,
     337,   272,   272,    20,   353,   239,   380,   273,   354,   -16,
     363,   272,   356,   272,  -351,  -351,   315,   344,   345,   239,
     316,   236,  -297,  -297,   317,  -298,  -298,   274,   239,   423,
     424,   434,   435,   239,   274,    75,   367,   238,   272,   238,
     236,   274,   236,   274,   370,   386,   274,   239,   378,   239,
     272,   292,   292,     1,   379,   272,   121,   239,   391,   276,
      66,   393,   399,   292,   403,   239,   407,    76,   239,    67,
     239,     3,   404,    75,   408,   419,   272,     4,   444,   272,
     119,   238,   273,   273,   275,   445,   237,   446,   272,   454,
     458,   275,   273,    68,   273,    17,    69,    20,   275,   460,
     275,   474,   272,   275,    17,    17,    17,   107,  -351,  -351,
     315,   272,   461,   475,   316,   493,   272,   492,   317,   273,
     497,   494,   498,   272,    17,   499,   501,    17,    17,   277,
     272,   273,   272,   500,   506,   510,   273,   238,   511,   -16,
     272,    17,   272,  -116,  -351,  -351,   315,   512,   272,    78,
     316,   272,   516,   272,   513,   520,   238,   273,   238,  -265,
     273,   543,   546,   548,   561,  -267,   562,  -160,   573,   273,
     274,   237,   276,   237,   565,   574,   576,    78,   369,   276,
     577,   585,   129,   273,   278,   371,   276,   372,   276,    66,
     578,   276,   273,   595,   599,   600,   603,   273,    67,   602,
       3,    75,   605,   608,   273,   134,     4,   611,   613,   610,
     433,   273,    32,   273,    64,   237,    93,   283,   381,   158,
     281,   273,    68,   273,   466,    69,    20,   275,    63,   273,
     134,   108,   273,   374,   273,   130,   131,   377,   589,     6,
     590,   503,   277,   504,    34,   274,   274,   397,   438,   277,
     436,    66,   439,   437,     0,   274,   277,   274,   277,   531,
     104,   277,    35,     0,   440,   129,     0,     0,    36,   442,
       0,   237,    66,     0,     0,     0,     0,     0,     0,     0,
       0,    67,   274,     3,    68,     0,     0,    69,    20,     4,
     237,     0,   237,     0,   274,     0,     0,   278,     0,   274,
       0,     0,   275,   275,   278,    68,     0,     0,    69,    20,
       0,   278,   275,   278,   275,   276,   278,     0,   130,   282,
     274,     0,     6,   274,     0,     0,     0,     0,     0,     0,
       0,     0,   274,     0,     0,     0,     0,     0,     0,   275,
       0,     0,   279,     0,     0,     0,   274,     0,     0,     0,
       0,   275,     0,     0,     0,   274,   275,     0,     0,     0,
     274,     0,     0,     0,     0,     0,     0,   274,     0,     0,
       0,     0,     0,     0,   274,     0,   274,   275,     0,     0,
     275,     0,     0,     0,   274,   277,   274,     0,     0,   275,
     276,   276,   274,     0,     0,   274,     0,   274,     0,     0,
     276,     0,   276,   275,     0,     0,     0,     0,     0,     0,
       0,     0,   275,     0,     0,     0,     0,   275,     0,     0,
       0,     0,     0,     0,   275,     0,   533,   276,     0,     0,
       0,   275,     0,   275,     0,     0,     0,     0,     0,   276,
     278,   275,     0,   275,   276,     0,     0,     0,     0,   275,
       0,     0,   275,   215,   275,   279,     0,     0,     0,     0,
     277,   277,   279,     0,   369,   276,     0,   371,   276,   279,
     277,   279,   277,     0,   279,     0,   575,   276,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     582,   276,     0,     0,     0,     0,     0,   277,     0,     0,
     276,     0,   308,   215,   596,   276,     0,     0,     0,   277,
       0,     0,   276,     0,   277,   278,   278,     0,     0,   276,
       0,   276,     0,     0,     0,   278,     0,   278,   533,   276,
       0,   276,     0,     0,     0,   277,   575,   276,   277,   582,
     276,   596,   276,     0,     0,     0,     0,   277,     0,     0,
       0,     0,   278,     0,     0,     0,   348,     0,     0,   352,
       0,   277,     0,     0,   278,   360,   361,     0,     0,   278,
     277,     0,     0,     0,     0,   277,     0,     0,     0,     0,
       0,     0,   277,     0,     0,     0,     0,     0,     0,   277,
     278,   277,     0,   278,     0,     0,     0,     0,   279,   277,
       0,   277,   278,     0,     0,     0,     0,   277,     0,     0,
     277,     0,   277,     0,     0,     0,   278,     0,     0,     0,
       0,     0,     0,     0,     0,   278,     0,     0,     0,     0,
     278,     0,     0,     0,     0,     0,     0,   278,     0,     0,
       0,     0,     0,     0,   278,     0,   278,     0,     0,   412,
       0,     0,   418,     0,   278,     0,   278,     0,     0,     0,
       0,     0,   278,     0,     0,   278,     0,   278,     0,     0,
       0,     0,     0,   279,   279,     0,   441,     0,     0,     0,
     443,     0,     0,   279,     0,   279,   447,   448,   293,   293,
     293,   293,     0,     0,     0,     0,   293,   293,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     279,   463,   464,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   279,     0,     0,     0,   476,   279,     0,     0,
       0,    66,     0,     0,     0,     0,     0,     0,     0,   215,
     163,   294,   294,   294,   294,     0,   164,     0,   279,   294,
     294,   279,     0,   165,     0,     0,     0,   166,     0,     0,
     279,     0,     0,     0,    68,     0,     0,    69,    20,     0,
       0,   167,   168,     0,   279,   169,     0,     0,     0,     0,
       0,     0,     0,   279,   171,   172,   173,   174,   279,   505,
     175,   176,     0,     0,     0,   279,     0,     0,     0,     0,
       0,     0,   279,     0,   279,     0,     0,     0,     0,     0,
       0,     0,   279,     0,   279,     0,     0,     0,     0,     0,
     279,     0,     0,   279,   526,   279,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   547,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   293,   293,   293,   293,   293,   293,   293,   293,
     293,   293,   293,   293,     0,   293,   293,   293,   293,   293,
     293,   293,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   359,     0,   159,   160,     0,
       0,     0,     0,     0,   581,     0,     0,     0,   584,     0,
     586,     0,     0,     0,     0,   294,   294,   294,   294,   294,
     294,   294,   294,   294,   294,   294,   294,     0,   294,   294,
     294,   294,   294,   294,   294,     0,     0,     0,     0,   293,
     161,   162,   293,     0,     0,    66,     0,     0,     0,     0,
       0,     0,     0,     0,   163,     0,     0,     0,     0,     0,
     164,   525,     0,   159,   160,     0,     0,   165,     0,     0,
       0,   166,     0,     0,     0,     0,     0,     0,    68,     0,
       0,    69,    20,     0,     0,   167,   168,     0,     0,   169,
       0,     0,   294,     0,     0,   294,     0,     0,   171,   172,
     173,   174,     0,     0,   175,   176,   161,   162,     0,     0,
       0,    66,     0,     0,     0,     0,     0,     0,     0,     0,
     163,     0,     0,     0,   293,   293,   164,     0,     0,     0,
       0,   159,   160,   165,     0,     0,   293,   166,     0,     0,
       0,     0,     0,     0,    68,     0,     0,    69,    20,     0,
       0,   167,   168,     0,     0,   169,     0,     0,     0,     0,
       0,     0,     0,     0,   171,   172,   173,   174,     0,     0,
     175,   176,     0,     0,   161,   162,     0,   294,   294,    66,
       0,     0,     0,     0,     0,     0,     0,     0,   163,   294,
       0,     0,     0,     0,   164,     0,     0,   159,   160,     0,
       0,   165,     0,     0,     0,   166,     0,     0,     0,     0,
       0,     0,    68,     0,     0,    69,    20,     0,     0,   167,
     168,     0,     0,   169,     0,   170,   309,     0,     0,     0,
     310,     0,   171,   172,   173,   174,     0,     0,   175,   176,
     161,   162,     0,     0,     0,    66,     0,     0,     0,     0,
       0,     0,     0,     0,   163,     0,     0,     0,     0,     0,
     164,     0,     0,   159,   160,     0,     0,   165,     0,     0,
       0,   166,     0,     0,     0,     0,     0,     0,    68,     0,
       0,    69,    20,     0,     0,   167,   168,     0,     0,   169,
       0,   170,   488,     0,     0,     0,     0,     0,   171,   172,
     173,   174,     0,     0,   175,   176,   161,   162,     0,     0,
       0,    66,     0,     0,     0,   159,   160,     0,     0,     0,
     163,     0,     0,     0,     0,     0,   164,     0,     0,     0,
       0,     0,     0,   165,     0,     0,     0,   166,     0,     0,
       0,     0,     0,     0,    68,     0,     0,    69,    20,     0,
       0,   167,   168,     0,     0,   169,     0,   170,   161,   162,
       0,     0,     0,    66,   171,   172,   173,   174,     0,     0,
     175,   176,   163,     0,     0,     0,     0,     0,   164,     0,
       0,   159,   160,     0,     0,   165,     0,     0,     0,   166,
       0,     0,     0,     0,     0,     0,    68,     0,     0,    69,
      20,     0,     0,   167,   168,     0,     0,   169,     0,     0,
       0,     0,     0,   351,     0,     0,   171,   172,   173,   174,
       0,     0,   175,   176,   161,   162,     0,     0,     0,    66,
       0,     0,     0,   159,   160,     0,     0,     0,   163,     0,
       0,     0,     0,     0,   164,     0,     0,     0,     0,     0,
       0,   165,     0,     0,     0,   166,     0,     0,     0,     0,
       0,     0,    68,     0,     0,    69,    20,     0,     0,   167,
     168,     0,     0,   169,   409,     0,   161,   162,     0,     0,
       0,    66,   171,   172,   173,   174,     0,     0,   175,   176,
     163,     0,     0,     0,     0,     0,   164,     0,     0,   159,
     160,     0,     0,   165,     0,     0,     0,   166,     0,     0,
       0,     0,     0,     0,    68,     0,     0,    69,    20,     0,
       0,   167,   168,     0,     0,   169,     0,     0,     0,     0,
     123,     0,     0,     0,   171,   172,   173,   174,     0,     0,
     175,   176,   161,   162,     0,     0,     0,    66,     0,     0,
       0,   159,   160,     0,     0,     0,   163,     0,     0,     0,
       0,     0,   164,     0,     0,     0,     0,     0,     0,   165,
       0,     0,     0,   166,     0,     0,     0,     0,     0,     0,
      68,     0,     0,    69,    20,     0,     0,   167,   168,     0,
       0,   169,     0,     0,   161,   162,     0,   462,     0,    66,
     171,   172,   173,   174,     0,     0,   175,   176,   163,     0,
       0,     0,     0,     0,   164,     0,     0,   159,   160,     0,
       0,   165,     0,     0,     0,   166,     0,     0,     0,     0,
       0,     0,    68,     0,     0,    69,    20,     0,     0,   167,
     168,     0,     0,   169,   482,     0,     0,     0,     0,     0,
       0,     0,   171,   172,   173,   174,     0,     0,   175,   176,
     161,   162,     0,     0,     0,    66,     0,     0,     0,   159,
     160,     0,     0,     0,   163,     0,     0,     0,     0,     0,
     164,     0,     0,     0,     0,     0,     0,   165,     0,     0,
       0,   166,     0,     0,     0,     0,     0,     0,    68,     0,
       0,    69,    20,     0,     0,   167,   168,     0,     0,   169,
     495,     0,   161,   162,     0,     0,     0,    66,   171,   172,
     173,   174,     0,     0,   175,   176,   163,     0,     0,     0,
       0,     0,   164,     0,     0,   159,   160,     0,     0,   165,
       0,     0,     0,   166,     0,     0,     0,     0,     0,     0,
      68,     0,     0,    69,    20,     0,     0,   167,   168,     0,
       0,   169,   518,     0,     0,     0,     0,     0,     0,     0,
     171,   172,   173,   174,     0,     0,   175,   176,   161,   162,
       0,     0,     0,    66,     0,     0,     0,   159,   160,     0,
       0,     0,   163,     0,     0,     0,     0,     0,   164,     0,
       0,     0,     0,     0,     0,   165,     0,     0,     0,   166,
       0,     0,     0,     0,     0,     0,    68,     0,     0,    69,
      20,     0,     0,   167,   168,     0,     0,   169,   527,     0,
     161,   162,     0,     0,     0,    66,   171,   172,   173,   174,
       0,     0,   175,   176,   163,     0,     0,     0,     0,     0,
     164,     0,     0,   159,   160,     0,     0,   165,     0,     0,
       0,   166,     0,     0,     0,     0,     0,     0,    68,     0,
       0,    69,    20,     0,     0,   167,   168,     0,     0,   169,
     554,     0,     0,     0,     0,     0,     0,     0,   171,   172,
     173,   174,     0,     0,   175,   176,   161,   162,     0,     0,
       0,    66,     0,     0,     0,   159,   160,     0,     0,     0,
     163,     0,     0,     0,     0,     0,   164,     0,     0,     0,
       0,     0,     0,   165,     0,     0,     0,   166,     0,     0,
       0,     0,     0,     0,    68,     0,     0,    69,    20,     0,
       0,   167,   168,     0,     0,   169,     0,     0,   161,   162,
       0,   583,     0,    66,   171,   172,   173,   174,     0,     0,
     175,   176,   163,     0,     0,     0,     0,     0,   164,     0,
       0,   159,   160,     0,     0,   165,     0,     0,     0,   166,
       0,     0,     0,     0,     0,     0,    68,     0,     0,    69,
      20,     0,     0,   167,   168,     0,     0,   169,   597,     0,
       0,     0,     0,     0,     0,     0,   171,   172,   173,   174,
       0,     0,   175,   176,   161,   162,     0,     0,     0,    66,
       0,     0,     0,     0,     0,     0,     0,     0,   163,     0,
       0,     0,     0,     0,   164,     0,     0,     0,     0,     0,
       0,   165,     0,     0,     0,   166,     0,     0,     0,     0,
       0,     0,    68,     0,     0,    69,    20,     0,     0,   167,
     168,     0,     0,   169,     0,     0,     0,     0,     0,     0,
       0,     0,   171,   172,   173,   174,     0,     0,   175,   176,
     221,     0,   161,   162,   566,   222,   223,    66,   224,     0,
       0,   225,     0,     0,     0,   226,   163,     0,     0,   567,
       0,     0,   164,   227,     4,   228,     0,   229,   230,   165,
     231,     0,     0,   166,   232,     0,     0,     0,     0,     0,
      68,     0,     0,    69,    20,     0,     0,     0,     0,     0,
       0,   233,     0,   130,   591,     0,     0,     6,     0,     0,
     171,   172,   173,   174,     0,     0,   175,   176,   221,     0,
     161,   162,   566,   222,   223,    66,   224,     0,     0,   225,
       0,     0,     0,   226,   163,     0,     0,   567,     0,     0,
     164,   227,     4,   228,     0,   229,   230,   165,   231,     0,
       0,   166,   232,     0,     0,     0,     0,     0,    68,     0,
       0,    69,    20,     0,     0,     0,     0,     0,     0,   233,
       0,   130,   604,     0,     0,     6,     0,     0,   171,   172,
     173,   174,     0,     0,   175,   176,   221,     0,   161,   162,
       0,   222,   223,    66,   224,     0,     0,   225,     0,     0,
       0,   226,   163,     0,     0,     0,     0,     0,   164,   227,
       4,   228,     0,   229,   230,   165,   231,     0,     0,   166,
     232,     0,     0,     0,     0,     0,    68,     0,     0,    69,
      20,     0,     0,     0,     0,     0,     0,   233,     0,   130,
     234,     0,     0,     6,     0,     0,   171,   172,   173,   174,
       0,     0,   175,   176,   221,     0,   161,   162,     0,   222,
     223,    66,   224,     0,     0,   225,     0,     0,     0,   226,
     163,     0,     0,     0,     0,     0,   164,   227,     4,   228,
       0,   229,   230,   165,   231,     0,     0,   166,   232,     0,
       0,     0,     0,     0,    68,     0,     0,    69,    20,     0,
       0,     0,     0,     0,     0,   233,     0,   130,   365,     0,
       0,     6,     0,     0,   171,   172,   173,   174,     0,     0,
     175,   176,   221,     0,   161,   162,     0,   222,   223,    66,
     224,     0,     0,   225,     0,     0,     0,   226,   163,     0,
       0,     0,     0,     0,   467,   227,     4,   228,     0,   229,
     230,   165,   231,     0,     0,   468,   232,     0,     0,     0,
       0,     0,    68,     0,     0,    69,    20,     0,     0,     0,
       0,     0,     0,   233,     0,   130,   469,     0,     0,     6,
       0,     0,   171,   172,   173,   174,     0,     0,   175,   176,
     221,     0,   161,   162,     0,   222,   223,    66,   224,     0,
       0,   225,     0,     0,     0,   226,   163,     0,     0,     0,
       0,     0,   164,   227,     4,   228,     0,   229,   230,   165,
     231,     0,     0,   166,   232,     0,     0,     0,     0,     0,
      68,     0,     0,    69,    20,     0,     0,     0,     0,     0,
       0,   233,     0,   130,   514,     0,     0,     6,     0,     0,
     171,   172,   173,   174,     0,     0,   175,   176,   221,     0,
     161,   162,     0,   222,   223,    66,   224,     0,     0,   225,
       0,     0,     0,   226,   163,     0,     0,     0,     0,     0,
     164,   227,     4,   228,     0,   229,   230,   165,   231,     0,
       0,   166,   232,     0,     0,     0,     0,     0,    68,     0,
       0,    69,    20,     0,     0,     0,     0,     0,     0,   233,
       0,   130,   517,     0,     0,     6,     0,     0,   171,   172,
     173,   174,     0,     0,   175,   176,   221,     0,   161,   162,
       0,   222,   223,    66,   224,     0,     0,   225,     0,     0,
       0,   226,   163,     0,     0,     0,     0,     0,   164,   227,
       4,   228,     0,   229,   230,   165,   231,     0,     0,   166,
     232,     0,     0,     0,     0,     0,    68,     0,     0,    69,
      20,     0,     0,     0,     0,     0,     0,   233,     0,   130,
     553,     0,     0,     6,     0,     0,   171,   172,   173,   174,
       0,     0,   175,   176,   221,     0,   161,   162,     0,   222,
     223,    66,   224,     0,     0,   225,     0,     0,     0,   226,
     163,     0,     0,     0,     0,     0,   164,   227,     4,   228,
       0,   229,   230,   165,   231,     0,     0,   166,   232,     0,
       0,     0,     0,     0,    68,     0,     0,    69,    20,     0,
       0,     0,     0,     0,     0,   233,     0,   130,     0,     0,
       0,     6,     0,     0,   171,   172,   173,   174,     0,     0,
     175,   176,   368,     0,   161,   162,     0,   222,   223,    66,
     224,     0,     0,   225,     0,     0,     0,   226,   163,     0,
       0,     0,     0,     0,   164,   227,     0,   228,     0,   229,
     230,   165,   231,     0,     0,   166,   232,     0,     0,     0,
       0,     0,    68,     0,     0,    69,    20,     0,     0,     0,
       0,     0,     0,   233,     0,   130,     0,     0,     0,     6,
       0,     0,   171,   172,   173,   174,     0,     0,   175,   176,
     368,     0,   161,   162,     0,   532,   223,    66,   224,     0,
       0,   225,     0,     0,     0,   226,   163,     0,     0,     0,
       0,     0,   164,   227,     0,   228,     0,   229,   230,   165,
     231,     0,     0,   166,   232,     0,     0,     0,     0,     0,
      68,     0,     0,    69,    20,     0,     0,     0,     0,     0,
       0,   233,     1,   130,   161,   162,     0,     6,     0,    66,
     171,   172,   173,   174,     0,     0,   175,   176,   163,     0,
       0,     0,     0,     0,   164,     0,     0,     0,     0,   161,
     162,   165,     0,     0,    66,   166,     0,     0,     0,     0,
       0,     0,    68,   163,     0,    69,    20,     0,     0,   164,
       0,     0,     0,   233,     0,     0,   165,     0,     0,     0,
     166,     0,   171,   172,   173,   174,     0,    68,   175,   176,
      69,    20,     0,     0,     0,     0,     0,     0,   233,     0,
       0,     0,     0,     0,     0,     0,     0,   171,   172,   173,
     174,     0,     0,   175,   176
};

static const short yycheck[] =
{
       3,     4,   130,   105,   243,   113,   170,   262,   290,   159,
     160,   161,   162,     1,   150,   110,     0,   167,   168,   399,
      39,    96,     5,    42,    99,     9,    10,    11,   115,     1,
     100,   403,    35,    36,   110,    38,    44,    56,    15,   419,
     135,    60,    45,    54,    54,    29,   510,   142,    32,    33,
       9,    51,    11,     9,    62,   125,    88,   127,    10,   135,
      68,    54,    46,   101,    67,   103,    77,    77,    71,   498,
      54,    82,    82,    32,   144,    56,    32,    29,   165,    60,
      94,    33,    48,     0,    84,    99,    54,    87,    88,    82,
     109,    93,    94,   101,    46,   475,   115,    63,    82,   110,
     110,   104,   105,   114,   114,    88,    48,   536,     2,    97,
     539,     5,    89,   493,   486,    64,   119,   110,    90,   583,
     130,    63,   133,   133,   135,   135,   110,   130,   109,   101,
     114,    99,    98,   235,   229,   571,   516,    72,   602,   110,
     151,   151,   135,    96,   102,    39,   165,   150,    42,   102,
      96,   135,   288,    99,   590,   165,    98,    71,    57,   169,
      54,    64,    56,    62,   135,    44,    60,     0,    44,   101,
     102,   600,    51,   323,   324,   325,     9,    10,    11,   608,
      56,    88,   611,    77,   613,   188,    62,    73,    82,   284,
     104,   105,    68,    92,   101,   394,    29,   577,   397,    32,
      33,    88,    88,    53,   291,    84,   461,   462,    87,    88,
     130,   281,   110,    46,   101,   109,   110,    96,    96,   501,
     114,   115,   225,   536,   102,   101,   539,    44,   231,   240,
     240,    97,   235,   243,   473,   133,   130,   135,    96,   133,
     243,   135,   406,   178,   102,    62,    44,   250,   101,   102,
     400,    68,   262,    51,   257,   510,   259,   151,   386,    99,
      97,   103,   364,   103,   101,   159,   160,   161,   162,   280,
     280,   165,   291,   167,   168,   169,   515,   110,   262,   290,
     290,    44,     5,     6,     7,   288,    84,   600,   498,    87,
      88,   130,   101,    56,    54,   608,   280,   300,   611,    62,
     613,   236,   135,    96,    96,    68,   290,    68,   101,   102,
     102,    99,    73,    76,   317,   318,    77,    77,    18,    19,
      20,    21,    82,   243,   101,   335,   536,    88,   583,   539,
     250,   328,   329,   330,   102,   485,    95,   257,   101,   259,
      99,    44,   262,   471,   103,    96,   240,   602,    51,   243,
     110,   102,    99,   592,   114,   450,   103,    60,    58,    62,
      98,   364,   373,   373,   102,    68,   301,   302,   262,    97,
     130,   306,   307,   133,   498,   135,   386,   101,    61,   103,
      95,    84,    65,   386,    87,    88,   280,   495,    96,   100,
     600,   151,    99,   101,   102,    98,   290,   291,   608,    96,
      54,   611,    99,   613,   243,   100,   103,   331,   332,   333,
     334,   250,   536,   521,   498,   539,   511,    97,   257,    96,
     259,    96,   530,   262,    97,   102,    99,   102,   100,   323,
     324,   325,   326,   327,   328,   329,   330,   331,   332,   333,
     334,   335,   336,   337,   338,   339,   340,   341,   342,   100,
     558,    96,   536,    95,   130,   539,   396,   102,   398,   394,
      97,   471,   397,   473,    44,   103,   386,    96,   471,   102,
     473,    51,   103,   102,    99,    44,   600,   103,   573,   373,
     240,   103,    51,   243,   608,    93,    94,   611,    68,   613,
     501,   501,   386,    11,    44,   498,    12,   432,     8,     9,
      10,    51,   262,    14,    84,   515,   400,    87,    88,   403,
     513,   130,   515,    46,    47,    84,   600,   501,    87,    88,
     280,    66,    13,    68,   608,     3,     4,   611,    73,   613,
     290,    95,    77,   536,    84,    95,   539,    87,    88,    16,
      17,   461,   462,    88,    95,   548,    96,   386,    95,    88,
      90,   471,    95,   473,    93,    94,    95,    93,    94,   562,
      99,   571,    46,    47,   103,    46,    47,   243,   571,   326,
     327,   336,   337,   576,   250,   335,   101,   471,   498,   473,
     590,   257,   592,   259,   101,    97,   262,   590,   101,   592,
     510,   485,   486,    44,    95,   515,    99,   600,    68,   130,
      51,    99,    95,   497,    96,   608,    68,   501,   611,    60,
     613,    62,    98,   373,    68,    95,   536,    68,   101,   539,
     102,   515,   461,   462,   243,   101,   386,   101,   548,   101,
      96,   250,   471,    84,   473,     0,    87,    88,   257,    67,
     259,   101,   562,   262,     9,    10,    11,    98,    93,    94,
      95,   571,   102,    95,    99,    95,   576,   100,   103,   498,
      90,   100,    96,   583,    29,    96,    95,    32,    33,   130,
     590,   510,   592,    96,    95,   101,   515,   571,    96,    88,
     600,    46,   602,    95,    93,    94,    95,   101,   608,    54,
      99,   611,    95,   613,   103,   100,   590,   536,   592,    97,
     539,    97,   101,    96,    95,    97,    57,    57,    96,   548,
     386,   471,   243,   473,   101,    96,    96,    82,   250,   250,
      95,    90,    44,   562,   130,   257,   257,   259,   259,    51,
     101,   262,   571,   101,   101,    96,    90,   576,    60,   101,
      62,   501,   101,    96,   583,   110,    68,    57,    96,   101,
     335,   590,    11,   592,    48,   515,    64,   135,   280,   119,
     133,   600,    84,   602,   382,    87,    88,   386,    47,   608,
     135,    82,   611,   262,   613,    97,    98,   262,   569,   101,
     569,   451,   243,   451,    44,   461,   462,   302,   340,   250,
     338,    51,   341,   339,    -1,   471,   257,   473,   259,   497,
      60,   262,    62,    -1,   342,    44,    -1,    -1,    68,   346,
      -1,   571,    51,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    60,   498,    62,    84,    -1,    -1,    87,    88,    68,
     590,    -1,   592,    -1,   510,    -1,    -1,   243,    -1,   515,
      -1,    -1,   461,   462,   250,    84,    -1,    -1,    87,    88,
      -1,   257,   471,   259,   473,   386,   262,    -1,    97,    98,
     536,    -1,   101,   539,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   548,    -1,    -1,    -1,    -1,    -1,    -1,   498,
      -1,    -1,   130,    -1,    -1,    -1,   562,    -1,    -1,    -1,
      -1,   510,    -1,    -1,    -1,   571,   515,    -1,    -1,    -1,
     576,    -1,    -1,    -1,    -1,    -1,    -1,   583,    -1,    -1,
      -1,    -1,    -1,    -1,   590,    -1,   592,   536,    -1,    -1,
     539,    -1,    -1,    -1,   600,   386,   602,    -1,    -1,   548,
     461,   462,   608,    -1,    -1,   611,    -1,   613,    -1,    -1,
     471,    -1,   473,   562,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   571,    -1,    -1,    -1,    -1,   576,    -1,    -1,
      -1,    -1,    -1,    -1,   583,    -1,   498,   498,    -1,    -1,
      -1,   590,    -1,   592,    -1,    -1,    -1,    -1,    -1,   510,
     386,   600,    -1,   602,   515,    -1,    -1,    -1,    -1,   608,
      -1,    -1,   611,   120,   613,   243,    -1,    -1,    -1,    -1,
     461,   462,   250,    -1,   536,   536,    -1,   539,   539,   257,
     471,   259,   473,    -1,   262,    -1,   548,   548,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     562,   562,    -1,    -1,    -1,    -1,    -1,   498,    -1,    -1,
     571,    -1,   169,   170,   576,   576,    -1,    -1,    -1,   510,
      -1,    -1,   583,    -1,   515,   461,   462,    -1,    -1,   590,
      -1,   592,    -1,    -1,    -1,   471,    -1,   473,   600,   600,
      -1,   602,    -1,    -1,    -1,   536,   608,   608,   539,   611,
     611,   613,   613,    -1,    -1,    -1,    -1,   548,    -1,    -1,
      -1,    -1,   498,    -1,    -1,    -1,   223,    -1,    -1,   226,
      -1,   562,    -1,    -1,   510,   232,   233,    -1,    -1,   515,
     571,    -1,    -1,    -1,    -1,   576,    -1,    -1,    -1,    -1,
      -1,    -1,   583,    -1,    -1,    -1,    -1,    -1,    -1,   590,
     536,   592,    -1,   539,    -1,    -1,    -1,    -1,   386,   600,
      -1,   602,   548,    -1,    -1,    -1,    -1,   608,    -1,    -1,
     611,    -1,   613,    -1,    -1,    -1,   562,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   571,    -1,    -1,    -1,    -1,
     576,    -1,    -1,    -1,    -1,    -1,    -1,   583,    -1,    -1,
      -1,    -1,    -1,    -1,   590,    -1,   592,    -1,    -1,   316,
      -1,    -1,   319,    -1,   600,    -1,   602,    -1,    -1,    -1,
      -1,    -1,   608,    -1,    -1,   611,    -1,   613,    -1,    -1,
      -1,    -1,    -1,   461,   462,    -1,   343,    -1,    -1,    -1,
     347,    -1,    -1,   471,    -1,   473,   353,   354,   159,   160,
     161,   162,    -1,    -1,    -1,    -1,   167,   168,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     498,   378,   379,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   510,    -1,    -1,    -1,   393,   515,    -1,    -1,
      -1,    51,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   406,
      60,   159,   160,   161,   162,    -1,    66,    -1,   536,   167,
     168,   539,    -1,    73,    -1,    -1,    -1,    77,    -1,    -1,
     548,    -1,    -1,    -1,    84,    -1,    -1,    87,    88,    -1,
      -1,    91,    92,    -1,   562,    95,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   571,   104,   105,   106,   107,   576,   456,
     110,   111,    -1,    -1,    -1,   583,    -1,    -1,    -1,    -1,
      -1,    -1,   590,    -1,   592,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   600,    -1,   602,    -1,    -1,    -1,    -1,    -1,
     608,    -1,    -1,   611,   491,   613,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   506,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   323,   324,   325,   326,   327,   328,   329,   330,
     331,   332,   333,   334,    -1,   336,   337,   338,   339,   340,
     341,   342,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,     1,    -1,     3,     4,    -1,
      -1,    -1,    -1,    -1,   561,    -1,    -1,    -1,   565,    -1,
     567,    -1,    -1,    -1,    -1,   323,   324,   325,   326,   327,
     328,   329,   330,   331,   332,   333,   334,    -1,   336,   337,
     338,   339,   340,   341,   342,    -1,    -1,    -1,    -1,   400,
      46,    47,   403,    -1,    -1,    51,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    60,    -1,    -1,    -1,    -1,    -1,
      66,     1,    -1,     3,     4,    -1,    -1,    73,    -1,    -1,
      -1,    77,    -1,    -1,    -1,    -1,    -1,    -1,    84,    -1,
      -1,    87,    88,    -1,    -1,    91,    92,    -1,    -1,    95,
      -1,    -1,   400,    -1,    -1,   403,    -1,    -1,   104,   105,
     106,   107,    -1,    -1,   110,   111,    46,    47,    -1,    -1,
      -1,    51,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      60,    -1,    -1,    -1,   485,   486,    66,    -1,    -1,    -1,
      -1,     3,     4,    73,    -1,    -1,   497,    77,    -1,    -1,
      -1,    -1,    -1,    -1,    84,    -1,    -1,    87,    88,    -1,
      -1,    91,    92,    -1,    -1,    95,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   104,   105,   106,   107,    -1,    -1,
     110,   111,    -1,    -1,    46,    47,    -1,   485,   486,    51,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    60,   497,
      -1,    -1,    -1,    -1,    66,    -1,    -1,     3,     4,    -1,
      -1,    73,    -1,    -1,    -1,    77,    -1,    -1,    -1,    -1,
      -1,    -1,    84,    -1,    -1,    87,    88,    -1,    -1,    91,
      92,    -1,    -1,    95,    -1,    97,    98,    -1,    -1,    -1,
     102,    -1,   104,   105,   106,   107,    -1,    -1,   110,   111,
      46,    47,    -1,    -1,    -1,    51,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    60,    -1,    -1,    -1,    -1,    -1,
      66,    -1,    -1,     3,     4,    -1,    -1,    73,    -1,    -1,
      -1,    77,    -1,    -1,    -1,    -1,    -1,    -1,    84,    -1,
      -1,    87,    88,    -1,    -1,    91,    92,    -1,    -1,    95,
      -1,    97,    98,    -1,    -1,    -1,    -1,    -1,   104,   105,
     106,   107,    -1,    -1,   110,   111,    46,    47,    -1,    -1,
      -1,    51,    -1,    -1,    -1,     3,     4,    -1,    -1,    -1,
      60,    -1,    -1,    -1,    -1,    -1,    66,    -1,    -1,    -1,
      -1,    -1,    -1,    73,    -1,    -1,    -1,    77,    -1,    -1,
      -1,    -1,    -1,    -1,    84,    -1,    -1,    87,    88,    -1,
      -1,    91,    92,    -1,    -1,    95,    -1,    97,    46,    47,
      -1,    -1,    -1,    51,   104,   105,   106,   107,    -1,    -1,
     110,   111,    60,    -1,    -1,    -1,    -1,    -1,    66,    -1,
      -1,     3,     4,    -1,    -1,    73,    -1,    -1,    -1,    77,
      -1,    -1,    -1,    -1,    -1,    -1,    84,    -1,    -1,    87,
      88,    -1,    -1,    91,    92,    -1,    -1,    95,    -1,    -1,
      -1,    -1,    -1,   101,    -1,    -1,   104,   105,   106,   107,
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
      -1,    91,    92,    -1,    -1,    95,    -1,    -1,    -1,    -1,
     100,    -1,    -1,    -1,   104,   105,   106,   107,    -1,    -1,
     110,   111,    46,    47,    -1,    -1,    -1,    51,    -1,    -1,
      -1,     3,     4,    -1,    -1,    -1,    60,    -1,    -1,    -1,
      -1,    -1,    66,    -1,    -1,    -1,    -1,    -1,    -1,    73,
      -1,    -1,    -1,    77,    -1,    -1,    -1,    -1,    -1,    -1,
      84,    -1,    -1,    87,    88,    -1,    -1,    91,    92,    -1,
      -1,    95,    -1,    -1,    46,    47,    -1,   101,    -1,    51,
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
      96,    -1,    46,    47,    -1,    -1,    -1,    51,   104,   105,
     106,   107,    -1,    -1,   110,   111,    60,    -1,    -1,    -1,
      -1,    -1,    66,    -1,    -1,     3,     4,    -1,    -1,    73,
      -1,    -1,    -1,    77,    -1,    -1,    -1,    -1,    -1,    -1,
      84,    -1,    -1,    87,    88,    -1,    -1,    91,    92,    -1,
      -1,    95,    96,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
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
      96,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   104,   105,
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
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    60,    -1,
      -1,    -1,    -1,    -1,    66,    -1,    -1,    -1,    -1,    -1,
      -1,    73,    -1,    -1,    -1,    77,    -1,    -1,    -1,    -1,
      -1,    -1,    84,    -1,    -1,    87,    88,    -1,    -1,    91,
      92,    -1,    -1,    95,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   104,   105,   106,   107,    -1,    -1,   110,   111,
      44,    -1,    46,    47,    48,    49,    50,    51,    52,    -1,
      -1,    55,    -1,    -1,    -1,    59,    60,    -1,    -1,    63,
      -1,    -1,    66,    67,    68,    69,    -1,    71,    72,    73,
      74,    -1,    -1,    77,    78,    -1,    -1,    -1,    -1,    -1,
      84,    -1,    -1,    87,    88,    -1,    -1,    -1,    -1,    -1,
      -1,    95,    -1,    97,    98,    -1,    -1,   101,    -1,    -1,
     104,   105,   106,   107,    -1,    -1,   110,   111,    44,    -1,
      46,    47,    48,    49,    50,    51,    52,    -1,    -1,    55,
      -1,    -1,    -1,    59,    60,    -1,    -1,    63,    -1,    -1,
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
      -1,    -1,    -1,    -1,    -1,    95,    -1,    97,    -1,    -1,
      -1,   101,    -1,    -1,   104,   105,   106,   107,    -1,    -1,
     110,   111,    44,    -1,    46,    47,    -1,    49,    50,    51,
      52,    -1,    -1,    55,    -1,    -1,    -1,    59,    60,    -1,
      -1,    -1,    -1,    -1,    66,    67,    -1,    69,    -1,    71,
      72,    73,    74,    -1,    -1,    77,    78,    -1,    -1,    -1,
      -1,    -1,    84,    -1,    -1,    87,    88,    -1,    -1,    -1,
      -1,    -1,    -1,    95,    -1,    97,    -1,    -1,    -1,   101,
      -1,    -1,   104,   105,   106,   107,    -1,    -1,   110,   111,
      44,    -1,    46,    47,    -1,    49,    50,    51,    52,    -1,
      -1,    55,    -1,    -1,    -1,    59,    60,    -1,    -1,    -1,
      -1,    -1,    66,    67,    -1,    69,    -1,    71,    72,    73,
      74,    -1,    -1,    77,    78,    -1,    -1,    -1,    -1,    -1,
      84,    -1,    -1,    87,    88,    -1,    -1,    -1,    -1,    -1,
      -1,    95,    44,    97,    46,    47,    -1,   101,    -1,    51,
     104,   105,   106,   107,    -1,    -1,   110,   111,    60,    -1,
      -1,    -1,    -1,    -1,    66,    -1,    -1,    -1,    -1,    46,
      47,    73,    -1,    -1,    51,    77,    -1,    -1,    -1,    -1,
      -1,    -1,    84,    60,    -1,    87,    88,    -1,    -1,    66,
      -1,    -1,    -1,    95,    -1,    -1,    73,    -1,    -1,    -1,
      77,    -1,   104,   105,   106,   107,    -1,    84,   110,   111,
      87,    88,    -1,    -1,    -1,    -1,    -1,    -1,    95,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   104,   105,   106,
     107,    -1,    -1,   110,   111
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const unsigned short yystos[] =
{
       0,    44,    56,    62,    68,    76,   101,   113,   126,   127,
     128,   129,   130,   131,   132,   133,   134,   135,   166,   187,
      88,   122,   123,   124,   125,   125,   125,   122,     0,   128,
     130,   133,   127,   128,    44,    62,    68,   101,   103,    64,
     167,   171,    64,   138,   101,   103,   128,   125,   125,     5,
     125,   118,   120,   122,    97,   172,   102,   169,   118,   119,
      53,   139,   168,   171,   138,   101,    51,    60,    84,    87,
      98,   115,   116,   117,   118,   121,   122,   134,   135,   145,
     152,   166,   173,   174,   175,   176,   120,   172,   120,   140,
     137,   172,   170,   139,   125,   153,   125,   146,   147,   148,
     153,    99,   237,   237,    60,   115,   101,    98,   174,   102,
      97,   141,   172,   136,    95,    54,    99,   156,   101,   102,
      94,    99,   156,   100,    99,   153,   146,   153,   120,    44,
      97,    98,   123,   134,   135,   142,   143,   144,   145,   150,
     152,   159,   160,   161,   162,   166,   179,   187,   141,    96,
     115,   134,   154,   155,   119,   157,   100,   125,   147,     3,
       4,    46,    47,    60,    66,    73,    77,    91,    92,    95,
      97,   104,   105,   106,   107,   110,   111,   114,   116,   121,
     122,   149,   177,   225,   226,   227,   228,   229,   232,   234,
     238,   239,   240,   241,   242,   243,   244,   245,   246,   247,
     248,   249,   250,   251,   252,   253,   254,   255,   256,   257,
     258,   259,   260,   261,   262,   264,   100,   100,   156,   101,
     156,    44,    49,    50,    52,    55,    59,    67,    69,    71,
      72,    74,    78,    95,    98,   115,   116,   121,   122,   125,
     134,   135,   179,   180,   181,   182,   183,   184,   186,   187,
     188,   189,   191,   192,   193,   194,   196,   202,   203,   205,
     206,   207,   209,   210,   214,   215,   216,   217,   218,   219,
     220,   221,   228,   239,   241,   242,   243,   245,   246,   261,
      95,   162,    98,   143,   151,   179,   156,   148,   115,    96,
     102,   102,   122,   238,   240,   244,   244,   244,   244,   103,
     103,   116,   118,   119,   244,   244,   116,   122,   264,    98,
     102,   149,   178,   103,   103,    95,    99,   103,   103,    99,
     125,    46,    47,     5,     6,     7,     3,     4,     8,     9,
      10,    18,    19,    20,    21,    58,    16,    17,    11,    12,
      13,    14,    15,    89,    93,    94,   263,    95,   264,   101,
     125,   101,   264,    95,    95,   179,    95,   101,   125,     1,
     264,   264,   146,    90,   115,    98,   181,   101,    44,   184,
     101,   184,   184,   134,   183,   192,   211,   213,   101,    95,
      96,   154,   156,   101,   158,   179,    97,   163,   148,   155,
     119,    68,   125,    99,   235,   236,   237,   235,   237,    95,
      96,   237,   237,    96,    98,    98,   102,    68,    68,    96,
     233,   264,   264,    68,    73,    77,    73,   125,   264,    95,
     244,   244,   244,   249,   249,   250,   250,   250,   251,   251,
     251,   251,   116,   117,   252,   252,   253,   254,   255,   256,
     257,   264,   260,   264,   101,   101,   101,   264,   264,    61,
      65,   222,   223,   224,   101,     1,    90,   101,    96,   146,
      67,   102,   101,   264,   264,    96,   163,    66,    77,    98,
     122,   164,   165,   180,   101,    95,   264,   236,   237,   177,
     237,   177,    96,   233,   244,    96,    96,   247,    98,   149,
      96,   102,   100,    95,   100,    96,   233,    90,    96,    96,
      96,    95,   179,   223,   224,   264,    95,   192,   212,   213,
     101,    96,   101,   103,    98,   180,    95,    98,    96,   233,
     100,   230,    96,   244,   247,     1,   264,    96,   233,   141,
      96,   259,    49,   184,   185,   186,   188,   190,   195,   202,
     204,   208,   210,    97,   197,   155,   101,   264,    96,   212,
       1,   179,    66,    98,    96,   233,    96,   141,   231,    96,
     141,    95,    57,   185,   185,   101,    48,    63,    98,   198,
     199,   200,   201,    96,    96,   184,    96,    95,   101,    96,
     141,   264,   184,   101,   264,    90,   264,   265,    98,   199,
     200,    98,   180,   201,   179,   101,   184,    96,   233,   101,
      96,   212,   101,    90,    98,   101,    96,   185,    96,   212,
     101,    57,   185,    96,   185,   185
};

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
#define YYABORT		goto yyabortlab
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
   are run).  */

#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)           \
  Current.first_line   = Rhs[1].first_line;      \
  Current.first_column = Rhs[1].first_column;    \
  Current.last_line    = Rhs[N].last_line;       \
  Current.last_column  = Rhs[N].last_column;
#endif

/* YYLEX -- calling `yylex' with the right arguments.  */

#ifdef YYLEX_PARAM
# define YYLEX	yylex (&yylval, YYLEX_PARAM)
#else
# define YYLEX	yylex (&yylval)
#endif

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
# define YYDSYMPRINT(Args)			\
do {						\
  if (yydebug)					\
    yysymprint Args;				\
} while (0)
/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
# define YYDSYMPRINT(Args)
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



#if YYERROR_VERBOSE

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

#endif /* !YYERROR_VERBOSE */



#if YYDEBUG
/*-----------------------------.
| Print this symbol on YYOUT.  |
`-----------------------------*/

static void
#if defined (__STDC__) || defined (__cplusplus)
yysymprint (FILE* yyout, int yytype, YYSTYPE yyvalue)
#else
yysymprint (yyout, yytype, yyvalue)
    FILE* yyout;
    int yytype;
    YYSTYPE yyvalue;
#endif
{
  /* Pacify ``unused variable'' warnings.  */
  (void) yyvalue;

  if (yytype < YYNTOKENS)
    {
      YYFPRINTF (yyout, "token %s (", yytname[yytype]);
# ifdef YYPRINT
      YYPRINT (yyout, yytoknum[yytype], yyvalue);
# endif
    }
  else
    YYFPRINTF (yyout, "nterm %s (", yytname[yytype]);

  switch (yytype)
    {
      default:
        break;
    }
  YYFPRINTF (yyout, ")");
}
#endif /* YYDEBUG. */


/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
#if defined (__STDC__) || defined (__cplusplus)
yydestruct (int yytype, YYSTYPE yyvalue)
#else
yydestruct (yytype, yyvalue)
    int yytype;
    YYSTYPE yyvalue;
#endif
{
  /* Pacify ``unused variable'' warnings.  */
  (void) yyvalue;

  switch (yytype)
    {
      default:
        break;
    }
}



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




int
yyparse (YYPARSE_PARAM_ARG)
     YYPARSE_PARAM_DECL
{
  /* The lookahead symbol.  */
int yychar;

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval;

/* Number of parse errors so far.  */
int yynerrs;

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

  /* The state stack.  */
  short	yyssa[YYINITDEPTH];
  short *yyss = yyssa;
  register short *yyssp;

  /* The semantic value stack.  */
  YYSTYPE yyvsa[YYINITDEPTH];
  YYSTYPE *yyvs = yyvsa;
  register YYSTYPE *yyvsp;



#define YYPOPSTACK   (yyvsp--, yyssp--)

  YYSIZE_T yystacksize = YYINITDEPTH;

  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;


  /* When reducing, the number of symbols on the RHS of the reduced
     rule.  */
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
	   data in use in that stack, in bytes.  This used to be a
	   conditional around just the two extra args, but that might
	   be undefined if yyoverflow is a macro.  */
	yyoverflow ("parser stack overflow",
		    &yyss1, yysize * sizeof (*yyssp),
		    &yyvs1, yysize * sizeof (*yyvsp),

		    &yystacksize);

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

#  undef YYSTACK_RELOCATE
	if (yyss1 != yyssa)
	  YYSTACK_FREE (yyss1);
      }
# endif
#endif /* no yyoverflow */

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;


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
  if (yyn == YYPACT_NINF)
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* yychar is either YYEMPTY or YYEOF
     or a valid token in external form.  */

  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = YYLEX;
    }

  /* Convert token to internal form (in yychar1) for indexing tables with.  */

  if (yychar <= 0)		/* This means end of input.  */
    {
      yychar1 = 0;
      yychar = YYEOF;		/* Don't call YYLEX any more.  */

      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yychar1 = YYTRANSLATE (yychar);

      /* We have to keep this `#if YYDEBUG', since we use variables
	 which are defined only if `YYDEBUG' is set.  */
      YYDPRINTF ((stderr, "Next token is "));
      YYDSYMPRINT ((stderr, yychar1, yylval));
      YYDPRINTF ((stderr, "\n"));
    }

  /* If the proper action on seeing token YYCHAR1 is to reduce or to
     detect an error, take that action.  */
  yyn += yychar1;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yychar1)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yyn == 0 || yyn == YYTABLE_NINF)
	goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  if (yyn == YYFINAL)
    YYACCEPT;

  /* Shift the lookahead token.  */
  YYDPRINTF ((stderr, "Shifting token %d (%s), ",
	      yychar, yytname[yychar1]));

  /* Discard the token being shifted unless it is eof.  */
  if (yychar != YYEOF)
    yychar = YYEMPTY;

  *++yyvsp = yylval;


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

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];



#if YYDEBUG
  /* We have to keep this `#if YYDEBUG', since we use variables which
     are defined only if `YYDEBUG' is set.  */
  if (yydebug)
    {
      int yyi;

      YYFPRINTF (stderr, "Reducing via rule %d (line %d), ",
		 yyn - 1, yyrline[yyn]);

      /* Print the symbols being reduced, and their result.  */
      for (yyi = yyprhs[yyn]; yyrhs[yyi] >= 0; yyi++)
	YYFPRINTF (stderr, "%s ", yytname[yyrhs[yyi]]);
      YYFPRINTF (stderr, " -> %s\n", yytname[yyr1[yyn]]);
    }
#endif
  switch (yyn)
    {
        case 11:
#line 199 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    {
		  /* use preset global here. FIXME */
		  yyval.node = xstrdup ("int");
		}
    break;

  case 12:
#line 204 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    {
		  /* use preset global here. FIXME */
		  yyval.node = xstrdup ("double");
		}
    break;

  case 13:
#line 209 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    {
		  /* use preset global here. FIXME */
		  yyval.node = xstrdup ("boolean");
		}
    break;

  case 19:
#line 234 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    {
	          while (bracket_count-- > 0) 
		    yyval.node = concat ("[", yyvsp[-1].node, NULL);
		}
    break;

  case 20:
#line 239 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    {
	          while (bracket_count-- > 0) 
		    yyval.node = concat ("[", yyvsp[-1].node, NULL);
		}
    break;

  case 24:
#line 257 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    { 
		  yyval.node = concat (yyvsp[-2].node, ".", yyvsp[0].node, NULL);
		}
    break;

  case 38:
#line 289 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    { package_name = yyvsp[-1].node; }
    break;

  case 46:
#line 314 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    { 
		  if (yyvsp[0].value == PUBLIC_TK)
		    modifier_value++;
                  if (yyvsp[0].value == STATIC_TK)
                    modifier_value++;
	          USE_ABSORBER;
		}
    break;

  case 47:
#line 322 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    { 
		  if (yyvsp[0].value == PUBLIC_TK)
		    modifier_value++;
                  if (yyvsp[0].value == STATIC_TK)
                    modifier_value++;
		  USE_ABSORBER;
		}
    break;

  case 48:
#line 334 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    { 
		  report_class_declaration(yyvsp[-2].node);
		  modifier_value = 0;
                }
    break;

  case 50:
#line 340 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    { report_class_declaration(yyvsp[-2].node); }
    break;

  case 56:
#line 354 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    { USE_ABSORBER; }
    break;

  case 57:
#line 356 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    { USE_ABSORBER; }
    break;

  case 58:
#line 361 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    { pop_class_context (); }
    break;

  case 59:
#line 363 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    { pop_class_context (); }
    break;

  case 71:
#line 389 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    { USE_ABSORBER; }
    break;

  case 72:
#line 391 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    { modifier_value = 0; }
    break;

  case 77:
#line 407 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    { bracket_count = 0; USE_ABSORBER; }
    break;

  case 78:
#line 409 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    { ++bracket_count; }
    break;

  case 81:
#line 420 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    { ++method_depth; }
    break;

  case 82:
#line 422 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    { --method_depth; }
    break;

  case 83:
#line 427 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    { USE_ABSORBER; }
    break;

  case 85:
#line 430 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    { modifier_value = 0; }
    break;

  case 86:
#line 432 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    { 
                  report_main_declaration (yyvsp[-1].declarator);
		  modifier_value = 0;
		}
    break;

  case 87:
#line 440 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    { 
		  struct method_declarator *d;
		  NEW_METHOD_DECLARATOR (d, yyvsp[-2].node, NULL);
		  yyval.declarator = d;
		}
    break;

  case 88:
#line 446 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    { 
		  struct method_declarator *d;
		  NEW_METHOD_DECLARATOR (d, yyvsp[-3].node, yyvsp[-1].node);
		  yyval.declarator = d;
		}
    break;

  case 91:
#line 457 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    {
		  yyval.node = concat (yyvsp[-2].node, ",", yyvsp[0].node, NULL);
		}
    break;

  case 92:
#line 464 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
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
		}
    break;

  case 93:
#line 479 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
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
		}
    break;

  case 96:
#line 500 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    { USE_ABSORBER; }
    break;

  case 97:
#line 502 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    { USE_ABSORBER; }
    break;

  case 101:
#line 517 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    { USE_ABSORBER; }
    break;

  case 103:
#line 525 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    { modifier_value = 0; }
    break;

  case 105:
#line 530 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    { modifier_value = 0; }
    break;

  case 106:
#line 536 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    { USE_ABSORBER; }
    break;

  case 107:
#line 538 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    { USE_ABSORBER; }
    break;

  case 114:
#line 554 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    { USE_ABSORBER; }
    break;

  case 115:
#line 556 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    { USE_ABSORBER; }
    break;

  case 118:
#line 568 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    { report_class_declaration (yyvsp[0].node); modifier_value = 0; }
    break;

  case 120:
#line 571 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    { report_class_declaration (yyvsp[0].node); modifier_value = 0; }
    break;

  case 122:
#line 574 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    { report_class_declaration (yyvsp[-1].node); modifier_value = 0; }
    break;

  case 124:
#line 577 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    { report_class_declaration (yyvsp[-1].node); modifier_value = 0; }
    break;

  case 128:
#line 588 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    { pop_class_context (); }
    break;

  case 129:
#line 590 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    { pop_class_context (); }
    break;

  case 152:
#line 649 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    { USE_ABSORBER; }
    break;

  case 153:
#line 651 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    { modifier_value = 0; }
    break;

  case 178:
#line 692 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    { USE_ABSORBER; }
    break;

  case 189:
#line 719 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    { ++complexity; }
    break;

  case 190:
#line 724 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    { ++complexity; }
    break;

  case 191:
#line 729 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    { ++complexity; }
    break;

  case 199:
#line 749 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    { ++complexity; }
    break;

  case 204:
#line 764 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    { ++complexity; }
    break;

  case 208:
#line 781 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    { ++complexity; }
    break;

  case 214:
#line 799 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    { ++complexity; }
    break;

  case 225:
#line 823 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    { ++complexity; }
    break;

  case 228:
#line 832 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    { ++complexity; }
    break;

  case 231:
#line 839 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    {yyerror ("Missing term"); RECOVER;}
    break;

  case 232:
#line 841 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    {yyerror ("';' expected"); RECOVER;}
    break;

  case 235:
#line 850 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    { USE_ABSORBER; }
    break;

  case 241:
#line 865 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    { ++complexity; }
    break;

  case 242:
#line 869 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    { ++complexity; }
    break;

  case 253:
#line 889 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    { USE_ABSORBER; }
    break;

  case 254:
#line 894 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    { USE_ABSORBER; }
    break;

  case 255:
#line 896 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    { USE_ABSORBER; }
    break;

  case 256:
#line 898 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    { USE_ABSORBER; }
    break;

  case 257:
#line 900 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    { USE_ABSORBER; }
    break;

  case 265:
#line 915 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    { report_class_declaration (anonymous_context); }
    break;

  case 267:
#line 918 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    { report_class_declaration (anonymous_context); }
    break;

  case 269:
#line 924 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    { USE_ABSORBER; }
    break;

  case 283:
#line 955 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    { bracket_count = 1; }
    break;

  case 284:
#line 957 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    { bracket_count++; }
    break;

  case 287:
#line 968 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    { USE_ABSORBER; ++complexity; }
    break;

  case 288:
#line 970 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    { USE_ABSORBER; ++complexity; }
    break;

  case 289:
#line 971 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    { ++complexity; }
    break;

  case 290:
#line 972 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    { ++complexity; }
    break;

  case 291:
#line 973 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    { ++complexity; }
    break;

  case 292:
#line 974 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    { ++complexity; }
    break;

  case 293:
#line 979 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    { USE_ABSORBER; }
    break;

  case 296:
#line 986 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    { USE_ABSORBER; }
    break;

  case 343:
#line 1082 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    { ++complexity; }
    break;

  case 345:
#line 1088 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    { ++complexity; }
    break;

  case 347:
#line 1094 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    { ++complexity; }
    break;

  case 351:
#line 1108 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"
    { USE_ABSORBER; }
    break;


    }

/* Line 1016 of /usr/share/bison/yacc.c.  */
#line 2935 "ps14046.c"

  yyvsp -= yylen;
  yyssp -= yylen;


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


  /* Now `shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
  if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTOKENS];

  goto yynewstate;


/*------------------------------------.
| yyerrlab -- here on detecting error |
`------------------------------------*/
yyerrlab:
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if YYERROR_VERBOSE
      yyn = yypact[yystate];

      if (YYPACT_NINF < yyn && yyn < YYLAST)
	{
	  YYSIZE_T yysize = 0;
	  int yytype = YYTRANSLATE (yychar);
	  char *yymsg;
	  int yyx, yycount;

	  yycount = 0;
	  /* Start YYX at -YYN if negative to avoid negative indexes in
	     YYCHECK.  */
	  for (yyx = yyn < 0 ? -yyn : 0;
	       yyx < (int) (sizeof (yytname) / sizeof (char *)); yyx++)
	    if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR)
	      yysize += yystrlen (yytname[yyx]) + 15, yycount++;
	  yysize += yystrlen ("parse error, unexpected ") + 1;
	  yysize += yystrlen (yytname[yytype]);
	  yymsg = (char *) YYSTACK_ALLOC (yysize);
	  if (yymsg != 0)
	    {
	      char *yyp = yystpcpy (yymsg, "parse error, unexpected ");
	      yyp = yystpcpy (yyp, yytname[yytype]);

	      if (yycount < 5)
		{
		  yycount = 0;
		  for (yyx = yyn < 0 ? -yyn : 0;
		       yyx < (int) (sizeof (yytname) / sizeof (char *));
		       yyx++)
		    if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR)
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
#endif /* YYERROR_VERBOSE */
	yyerror ("parse error");
    }
  goto yyerrlab1;


/*----------------------------------------------------.
| yyerrlab1 -- error raised explicitly by an action.  |
`----------------------------------------------------*/
yyerrlab1:
  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
	 error, discard it.  */

      /* Return failure if at end of input.  */
      if (yychar == YYEOF)
        {
	  /* Pop the error token.  */
          YYPOPSTACK;
	  /* Pop the rest of the stack.  */
	  while (yyssp > yyss)
	    {
	      YYDPRINTF ((stderr, "Error: popping "));
	      YYDSYMPRINT ((stderr,
			    yystos[*yyssp],
			    *yyvsp));
	      YYDPRINTF ((stderr, "\n"));
	      yydestruct (yystos[*yyssp], *yyvsp);
	      YYPOPSTACK;
	    }
	  YYABORT;
        }

      YYDPRINTF ((stderr, "Discarding token %d (%s).\n",
		  yychar, yytname[yychar1]));
      yydestruct (yychar1, yylval);
      yychar = YYEMPTY;
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */

  yyerrstatus = 3;	/* Each real token shifted decrements this.  */

  for (;;)
    {
      yyn = yypact[yystate];
      if (yyn != YYPACT_NINF)
	{
	  yyn += YYTERROR;
	  if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYTERROR)
	    {
	      yyn = yytable[yyn];
	      if (0 < yyn)
		break;
	    }
	}

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
	YYABORT;

      YYDPRINTF ((stderr, "Error: popping "));
      YYDSYMPRINT ((stderr,
		    yystos[*yyssp], *yyvsp));
      YYDPRINTF ((stderr, "\n"));

      yydestruct (yystos[yystate], *yyvsp);
      yyvsp--;
      yystate = *--yyssp;


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
    }

  if (yyn == YYFINAL)
    YYACCEPT;

  YYDPRINTF ((stderr, "Shifting error token, "));

  *++yyvsp = yylval;


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

#ifndef yyoverflow
/*----------------------------------------------.
| yyoverflowlab -- parser overflow comes here.  |
`----------------------------------------------*/
yyoverflowlab:
  yyerror ("parser stack overflow");
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
  return yyresult;
}


#line 1126 "/home/gdr/gcc-3.3.5/gcc-3.3.5/gcc/java/parse-scan.y"


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


