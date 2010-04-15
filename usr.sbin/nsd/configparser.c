
/* A Bison parser, made by GNU Bison 2.4.1.  */

/* Skeleton implementation for Bison's Yacc-like parsers in C
   
      Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003, 2004, 2005, 2006
   Free Software Foundation, Inc.
   
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.
   
   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output.  */
#define YYBISON 1

/* Bison version.  */
#define YYBISON_VERSION "2.4.1"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1

/* Using locations.  */
#define YYLSP_NEEDED 0



/* Copy the first part of user declarations.  */

/* Line 189 of yacc.c  */
#line 10 "configparser.y"

#include <config.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "options.h"
#include "util.h"
#include "configyyrename.h"
int c_lex(void);
void c_error(const char *message);

#ifdef __cplusplus
extern "C"
#endif /* __cplusplus */

/* these need to be global, otherwise they cannot be used inside yacc */
extern config_parser_state_t* cfg_parser;
static int server_settings_seen = 0;

#if 0
#define OUTYY(s)  printf s /* used ONLY when debugging */
#else
#define OUTYY(s)
#endif



/* Line 189 of yacc.c  */
#line 105 "configparser.c"

/* Enabling traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 0
#endif

/* Enabling the token table.  */
#ifndef YYTOKEN_TABLE
# define YYTOKEN_TABLE 0
#endif


/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     SPACE = 258,
     LETTER = 259,
     NEWLINE = 260,
     COMMENT = 261,
     COLON = 262,
     ANY = 263,
     ZONESTR = 264,
     STRING = 265,
     VAR_SERVER = 266,
     VAR_NAME = 267,
     VAR_IP_ADDRESS = 268,
     VAR_DEBUG_MODE = 269,
     VAR_IP4_ONLY = 270,
     VAR_IP6_ONLY = 271,
     VAR_DATABASE = 272,
     VAR_IDENTITY = 273,
     VAR_NSID = 274,
     VAR_LOGFILE = 275,
     VAR_SERVER_COUNT = 276,
     VAR_TCP_COUNT = 277,
     VAR_PIDFILE = 278,
     VAR_PORT = 279,
     VAR_STATISTICS = 280,
     VAR_CHROOT = 281,
     VAR_USERNAME = 282,
     VAR_ZONESDIR = 283,
     VAR_XFRDFILE = 284,
     VAR_DIFFFILE = 285,
     VAR_XFRD_RELOAD_TIMEOUT = 286,
     VAR_TCP_QUERY_COUNT = 287,
     VAR_TCP_TIMEOUT = 288,
     VAR_IPV4_EDNS_SIZE = 289,
     VAR_IPV6_EDNS_SIZE = 290,
     VAR_ZONEFILE = 291,
     VAR_ZONE = 292,
     VAR_ALLOW_NOTIFY = 293,
     VAR_REQUEST_XFR = 294,
     VAR_NOTIFY = 295,
     VAR_PROVIDE_XFR = 296,
     VAR_NOTIFY_RETRY = 297,
     VAR_OUTGOING_INTERFACE = 298,
     VAR_ALLOW_AXFR_FALLBACK = 299,
     VAR_KEY = 300,
     VAR_ALGORITHM = 301,
     VAR_SECRET = 302,
     VAR_AXFR = 303,
     VAR_UDP = 304,
     VAR_VERBOSITY = 305,
     VAR_HIDE_VERSION = 306
   };
#endif
/* Tokens.  */
#define SPACE 258
#define LETTER 259
#define NEWLINE 260
#define COMMENT 261
#define COLON 262
#define ANY 263
#define ZONESTR 264
#define STRING 265
#define VAR_SERVER 266
#define VAR_NAME 267
#define VAR_IP_ADDRESS 268
#define VAR_DEBUG_MODE 269
#define VAR_IP4_ONLY 270
#define VAR_IP6_ONLY 271
#define VAR_DATABASE 272
#define VAR_IDENTITY 273
#define VAR_NSID 274
#define VAR_LOGFILE 275
#define VAR_SERVER_COUNT 276
#define VAR_TCP_COUNT 277
#define VAR_PIDFILE 278
#define VAR_PORT 279
#define VAR_STATISTICS 280
#define VAR_CHROOT 281
#define VAR_USERNAME 282
#define VAR_ZONESDIR 283
#define VAR_XFRDFILE 284
#define VAR_DIFFFILE 285
#define VAR_XFRD_RELOAD_TIMEOUT 286
#define VAR_TCP_QUERY_COUNT 287
#define VAR_TCP_TIMEOUT 288
#define VAR_IPV4_EDNS_SIZE 289
#define VAR_IPV6_EDNS_SIZE 290
#define VAR_ZONEFILE 291
#define VAR_ZONE 292
#define VAR_ALLOW_NOTIFY 293
#define VAR_REQUEST_XFR 294
#define VAR_NOTIFY 295
#define VAR_PROVIDE_XFR 296
#define VAR_NOTIFY_RETRY 297
#define VAR_OUTGOING_INTERFACE 298
#define VAR_ALLOW_AXFR_FALLBACK 299
#define VAR_KEY 300
#define VAR_ALGORITHM 301
#define VAR_SECRET 302
#define VAR_AXFR 303
#define VAR_UDP 304
#define VAR_VERBOSITY 305
#define VAR_HIDE_VERSION 306




#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
{

/* Line 214 of yacc.c  */
#line 40 "configparser.y"

	char*	str;



/* Line 214 of yacc.c  */
#line 249 "configparser.c"
} YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
#endif


/* Copy the second part of user declarations.  */


/* Line 264 of yacc.c  */
#line 261 "configparser.c"

#ifdef short
# undef short
#endif

#ifdef YYTYPE_UINT8
typedef YYTYPE_UINT8 yytype_uint8;
#else
typedef unsigned char yytype_uint8;
#endif

#ifdef YYTYPE_INT8
typedef YYTYPE_INT8 yytype_int8;
#elif (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
typedef signed char yytype_int8;
#else
typedef short int yytype_int8;
#endif

#ifdef YYTYPE_UINT16
typedef YYTYPE_UINT16 yytype_uint16;
#else
typedef unsigned short int yytype_uint16;
#endif

#ifdef YYTYPE_INT16
typedef YYTYPE_INT16 yytype_int16;
#else
typedef short int yytype_int16;
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif ! defined YYSIZE_T && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned int
# endif
#endif

#define YYSIZE_MAXIMUM ((YYSIZE_T) -1)

#ifndef YY_
# if YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(msgid) dgettext ("bison-runtime", msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(msgid) msgid
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(e) ((void) (e))
#else
# define YYUSE(e) /* empty */
#endif

/* Identity function, used to suppress warnings about constant conditions.  */
#ifndef lint
# define YYID(n) (n)
#else
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static int
YYID (int yyi)
#else
static int
YYID (yyi)
    int yyi;
#endif
{
  return yyi;
}
#endif

#if ! defined yyoverflow || YYERROR_VERBOSE

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#     ifndef _STDLIB_H
#      define _STDLIB_H 1
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's `empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (YYID (0))
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined _STDLIB_H \
       && ! ((defined YYMALLOC || defined malloc) \
	     && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef _STDLIB_H
#    define _STDLIB_H 1
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* ! defined yyoverflow || YYERROR_VERBOSE */


#if (! defined yyoverflow \
     && (! defined __cplusplus \
	 || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yytype_int16 yyss_alloc;
  YYSTYPE yyvs_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (yytype_int16) + sizeof (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

/* Copy COUNT objects from FROM to TO.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(To, From, Count) \
      __builtin_memcpy (To, From, (Count) * sizeof (*(From)))
#  else
#   define YYCOPY(To, From, Count)		\
      do					\
	{					\
	  YYSIZE_T yyi;				\
	  for (yyi = 0; yyi < (Count); yyi++)	\
	    (To)[yyi] = (From)[yyi];		\
	}					\
      while (YYID (0))
#  endif
# endif

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)				\
    do									\
      {									\
	YYSIZE_T yynewbytes;						\
	YYCOPY (&yyptr->Stack_alloc, Stack, yysize);			\
	Stack = &yyptr->Stack_alloc;					\
	yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
	yyptr += yynewbytes / sizeof (*yyptr);				\
      }									\
    while (YYID (0))

#endif

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  2
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   90

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  52
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  50
/* YYNRULES -- Number of rules.  */
#define YYNRULES  92
/* YYNRULES -- Number of states.  */
#define YYNSTATES  140

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   306

#define YYTRANSLATE(YYX)						\
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const yytype_uint8 yytranslate[] =
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
      45,    46,    47,    48,    49,    50,    51
};

#if YYDEBUG
/* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
   YYRHS.  */
static const yytype_uint8 yyprhs[] =
{
       0,     0,     3,     4,     7,    10,    13,    16,    18,    21,
      22,    24,    26,    28,    30,    32,    34,    36,    38,    40,
      42,    44,    46,    48,    50,    52,    54,    56,    58,    60,
      62,    64,    66,    68,    70,    72,    75,    78,    81,    84,
      87,    90,    93,    96,    99,   102,   105,   108,   111,   114,
     117,   120,   123,   126,   129,   132,   135,   138,   141,   144,
     147,   149,   152,   154,   156,   158,   160,   162,   164,   166,
     168,   170,   172,   175,   178,   182,   185,   188,   192,   196,
     200,   203,   207,   210,   213,   215,   218,   220,   222,   224,
     226,   229,   232
};

/* YYRHS -- A `-1'-separated list of the rules' RHS.  */
static const yytype_int8 yyrhs[] =
{
      53,     0,    -1,    -1,    53,    54,    -1,    55,    56,    -1,
      83,    84,    -1,    96,    97,    -1,    11,    -1,    56,    57,
      -1,    -1,    58,    -1,    59,    -1,    62,    -1,    63,    -1,
      64,    -1,    65,    -1,    66,    -1,    67,    -1,    68,    -1,
      69,    -1,    70,    -1,    71,    -1,    72,    -1,    73,    -1,
      74,    -1,    75,    -1,    76,    -1,    77,    -1,    78,    -1,
      79,    -1,    80,    -1,    81,    -1,    82,    -1,    60,    -1,
      61,    -1,    13,    10,    -1,    14,    10,    -1,    50,    10,
      -1,    51,    10,    -1,    15,    10,    -1,    16,    10,    -1,
      17,    10,    -1,    18,    10,    -1,    19,    10,    -1,    20,
      10,    -1,    21,    10,    -1,    22,    10,    -1,    23,    10,
      -1,    24,    10,    -1,    25,    10,    -1,    26,    10,    -1,
      27,    10,    -1,    28,    10,    -1,    30,    10,    -1,    29,
      10,    -1,    31,    10,    -1,    32,    10,    -1,    33,    10,
      -1,    34,    10,    -1,    35,    10,    -1,    37,    -1,    84,
      85,    -1,    85,    -1,    86,    -1,    87,    -1,    88,    -1,
      89,    -1,    91,    -1,    92,    -1,    93,    -1,    94,    -1,
      95,    -1,    12,    10,    -1,    36,    10,    -1,    38,    10,
      10,    -1,    39,    90,    -1,    10,    10,    -1,    48,    10,
      10,    -1,    49,    10,    10,    -1,    40,    10,    10,    -1,
      42,    10,    -1,    41,    10,    10,    -1,    43,    10,    -1,
      44,    10,    -1,    45,    -1,    97,    98,    -1,    98,    -1,
      99,    -1,   100,    -1,   101,    -1,    12,    10,    -1,    46,
      10,    -1,    47,    10,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,    62,    62,    62,    63,    63,    64,    67,    75,    75,
      76,    76,    76,    77,    77,    77,    77,    77,    78,    78,
      78,    78,    79,    79,    79,    79,    80,    80,    80,    81,
      81,    81,    82,    82,    82,    83,   105,   113,   121,   129,
     137,   145,   151,   157,   177,   183,   191,   199,   205,   211,
     219,   225,   231,   237,   243,   249,   257,   265,   273,   281,
     291,   313,   313,   314,   314,   314,   315,   315,   315,   315,
     316,   316,   317,   326,   335,   346,   350,   362,   375,   389,
     402,   410,   421,   433,   443,   459,   459,   460,   460,   460,
     461,   470,   479
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || YYTOKEN_TABLE
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "SPACE", "LETTER", "NEWLINE", "COMMENT",
  "COLON", "ANY", "ZONESTR", "STRING", "VAR_SERVER", "VAR_NAME",
  "VAR_IP_ADDRESS", "VAR_DEBUG_MODE", "VAR_IP4_ONLY", "VAR_IP6_ONLY",
  "VAR_DATABASE", "VAR_IDENTITY", "VAR_NSID", "VAR_LOGFILE",
  "VAR_SERVER_COUNT", "VAR_TCP_COUNT", "VAR_PIDFILE", "VAR_PORT",
  "VAR_STATISTICS", "VAR_CHROOT", "VAR_USERNAME", "VAR_ZONESDIR",
  "VAR_XFRDFILE", "VAR_DIFFFILE", "VAR_XFRD_RELOAD_TIMEOUT",
  "VAR_TCP_QUERY_COUNT", "VAR_TCP_TIMEOUT", "VAR_IPV4_EDNS_SIZE",
  "VAR_IPV6_EDNS_SIZE", "VAR_ZONEFILE", "VAR_ZONE", "VAR_ALLOW_NOTIFY",
  "VAR_REQUEST_XFR", "VAR_NOTIFY", "VAR_PROVIDE_XFR", "VAR_NOTIFY_RETRY",
  "VAR_OUTGOING_INTERFACE", "VAR_ALLOW_AXFR_FALLBACK", "VAR_KEY",
  "VAR_ALGORITHM", "VAR_SECRET", "VAR_AXFR", "VAR_UDP", "VAR_VERBOSITY",
  "VAR_HIDE_VERSION", "$accept", "toplevelvars", "toplevelvar",
  "serverstart", "contents_server", "content_server", "server_ip_address",
  "server_debug_mode", "server_verbosity", "server_hide_version",
  "server_ip4_only", "server_ip6_only", "server_database",
  "server_identity", "server_nsid", "server_logfile",
  "server_server_count", "server_tcp_count", "server_pidfile",
  "server_port", "server_statistics", "server_chroot", "server_username",
  "server_zonesdir", "server_difffile", "server_xfrdfile",
  "server_xfrd_reload_timeout", "server_tcp_query_count",
  "server_tcp_timeout", "server_ipv4_edns_size", "server_ipv6_edns_size",
  "zonestart", "contents_zone", "content_zone", "zone_name",
  "zone_zonefile", "zone_allow_notify", "zone_request_xfr",
  "zone_request_xfr_data", "zone_notify", "zone_notify_retry",
  "zone_provide_xfr", "zone_outgoing_interface",
  "zone_allow_axfr_fallback", "keystart", "contents_key", "content_key",
  "key_name", "key_algorithm", "key_secret", 0
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[YYLEX-NUM] -- Internal token number corresponding to
   token YYLEX-NUM.  */
static const yytype_uint16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,   288,   289,   290,   291,   292,   293,   294,
     295,   296,   297,   298,   299,   300,   301,   302,   303,   304,
     305,   306
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,    52,    53,    53,    54,    54,    54,    55,    56,    56,
      57,    57,    57,    57,    57,    57,    57,    57,    57,    57,
      57,    57,    57,    57,    57,    57,    57,    57,    57,    57,
      57,    57,    57,    57,    57,    58,    59,    60,    61,    62,
      63,    64,    65,    66,    67,    68,    69,    70,    71,    72,
      73,    74,    75,    76,    77,    78,    79,    80,    81,    82,
      83,    84,    84,    85,    85,    85,    85,    85,    85,    85,
      85,    85,    86,    87,    88,    89,    90,    90,    90,    91,
      92,    93,    94,    95,    96,    97,    97,    98,    98,    98,
      99,   100,   101
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     0,     2,     2,     2,     2,     1,     2,     0,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       1,     2,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     2,     2,     3,     2,     2,     3,     3,     3,
       2,     3,     2,     2,     1,     2,     1,     1,     1,     1,
       2,     2,     2
};

/* YYDEFACT[STATE-NAME] -- Default rule to reduce with in state
   STATE-NUM when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       2,     0,     1,     7,    60,    84,     3,     9,     0,     0,
       4,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       5,    62,    63,    64,    65,    66,    67,    68,    69,    70,
      71,     0,     0,     0,     6,    86,    87,    88,    89,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     8,    10,    11,    33,    34,    12,
      13,    14,    15,    16,    17,    18,    19,    20,    21,    22,
      23,    24,    25,    26,    27,    28,    29,    30,    31,    32,
      72,    73,     0,     0,     0,     0,    75,     0,     0,    80,
      82,    83,    61,    90,    91,    92,    85,    35,    36,    39,
      40,    41,    42,    43,    44,    45,    46,    47,    48,    49,
      50,    51,    52,    54,    53,    55,    56,    57,    58,    59,
      37,    38,    74,    76,     0,     0,    79,    81,    77,    78
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int8 yydefgoto[] =
{
      -1,     1,     6,     7,    10,    64,    65,    66,    67,    68,
      69,    70,    71,    72,    73,    74,    75,    76,    77,    78,
      79,    80,    81,    82,    83,    84,    85,    86,    87,    88,
      89,     8,    20,    21,    22,    23,    24,    25,    96,    26,
      27,    28,    29,    30,     9,    34,    35,    36,    37,    38
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -26
static const yytype_int8 yypact[] =
{
     -26,     0,   -26,   -26,   -26,   -26,   -26,   -26,    23,    -5,
      -1,    -8,    -7,    -6,    -9,    -4,    -2,    26,    28,    33,
      23,   -26,   -26,   -26,   -26,   -26,   -26,   -26,   -26,   -26,
     -26,    34,    36,    37,    -5,   -26,   -26,   -26,   -26,    38,
      41,    42,    43,    44,    45,    46,    47,    48,    50,    58,
      59,    60,    61,    62,    63,    64,    65,    66,    67,    68,
      69,    70,    71,    72,   -26,   -26,   -26,   -26,   -26,   -26,
     -26,   -26,   -26,   -26,   -26,   -26,   -26,   -26,   -26,   -26,
     -26,   -26,   -26,   -26,   -26,   -26,   -26,   -26,   -26,   -26,
     -26,   -26,    73,    74,    75,    76,   -26,    77,    78,   -26,
     -26,   -26,   -26,   -26,   -26,   -26,   -26,   -26,   -26,   -26,
     -26,   -26,   -26,   -26,   -26,   -26,   -26,   -26,   -26,   -26,
     -26,   -26,   -26,   -26,   -26,   -26,   -26,   -26,   -26,   -26,
     -26,   -26,   -26,   -26,    79,    80,   -26,   -26,   -26,   -26
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int8 yypgoto[] =
{
     -26,   -26,   -26,   -26,   -26,   -26,   -26,   -26,   -26,   -26,
     -26,   -26,   -26,   -26,   -26,   -26,   -26,   -26,   -26,   -26,
     -26,   -26,   -26,   -26,   -26,   -26,   -26,   -26,   -26,   -26,
     -26,   -26,   -26,   -15,   -26,   -26,   -26,   -26,   -26,   -26,
     -26,   -26,   -26,   -26,   -26,   -26,   -25,   -26,   -26,   -26
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If zero, do what YYDEFACT says.
   If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -1
static const yytype_uint8 yytable[] =
{
       2,    93,    90,    91,    92,   102,    97,    31,    98,   106,
       0,     3,    39,    40,    41,    42,    43,    44,    45,    46,
      47,    48,    49,    50,    51,    52,    53,    54,    55,    56,
      57,    58,    59,    60,    61,    11,    99,     4,   100,    94,
      95,    32,    33,   101,   103,     5,   104,   105,   107,    62,
      63,   108,   109,   110,   111,   112,   113,   114,   115,    12,
     116,    13,    14,    15,    16,    17,    18,    19,   117,   118,
     119,   120,   121,   122,   123,   124,   125,   126,   127,   128,
     129,   130,   131,   132,   133,   134,   135,   136,   137,   138,
     139
};

static const yytype_int8 yycheck[] =
{
       0,    10,    10,    10,    10,    20,    10,    12,    10,    34,
      -1,    11,    13,    14,    15,    16,    17,    18,    19,    20,
      21,    22,    23,    24,    25,    26,    27,    28,    29,    30,
      31,    32,    33,    34,    35,    12,    10,    37,    10,    48,
      49,    46,    47,    10,    10,    45,    10,    10,    10,    50,
      51,    10,    10,    10,    10,    10,    10,    10,    10,    36,
      10,    38,    39,    40,    41,    42,    43,    44,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,    53,     0,    11,    37,    45,    54,    55,    83,    96,
      56,    12,    36,    38,    39,    40,    41,    42,    43,    44,
      84,    85,    86,    87,    88,    89,    91,    92,    93,    94,
      95,    12,    46,    47,    97,    98,    99,   100,   101,    13,
      14,    15,    16,    17,    18,    19,    20,    21,    22,    23,
      24,    25,    26,    27,    28,    29,    30,    31,    32,    33,
      34,    35,    50,    51,    57,    58,    59,    60,    61,    62,
      63,    64,    65,    66,    67,    68,    69,    70,    71,    72,
      73,    74,    75,    76,    77,    78,    79,    80,    81,    82,
      10,    10,    10,    10,    48,    49,    90,    10,    10,    10,
      10,    10,    85,    10,    10,    10,    98,    10,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10
};

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		(-2)
#define YYEOF		0

#define YYACCEPT	goto yyacceptlab
#define YYABORT		goto yyabortlab
#define YYERROR		goto yyerrorlab


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
      yytoken = YYTRANSLATE (yychar);				\
      YYPOPSTACK (1);						\
      goto yybackup;						\
    }								\
  else								\
    {								\
      yyerror (YY_("syntax error: cannot back up")); \
      YYERROR;							\
    }								\
while (YYID (0))


#define YYTERROR	1
#define YYERRCODE	256


/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

#define YYRHSLOC(Rhs, K) ((Rhs)[K])
#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)				\
    do									\
      if (YYID (N))                                                    \
	{								\
	  (Current).first_line   = YYRHSLOC (Rhs, 1).first_line;	\
	  (Current).first_column = YYRHSLOC (Rhs, 1).first_column;	\
	  (Current).last_line    = YYRHSLOC (Rhs, N).last_line;		\
	  (Current).last_column  = YYRHSLOC (Rhs, N).last_column;	\
	}								\
      else								\
	{								\
	  (Current).first_line   = (Current).last_line   =		\
	    YYRHSLOC (Rhs, 0).last_line;				\
	  (Current).first_column = (Current).last_column =		\
	    YYRHSLOC (Rhs, 0).last_column;				\
	}								\
    while (YYID (0))
#endif


/* YY_LOCATION_PRINT -- Print the location on the stream.
   This macro was not mandated originally: define only if we know
   we won't break user code: when these are the locations we know.  */

#ifndef YY_LOCATION_PRINT
# if YYLTYPE_IS_TRIVIAL
#  define YY_LOCATION_PRINT(File, Loc)			\
     fprintf (File, "%d.%d-%d.%d",			\
	      (Loc).first_line, (Loc).first_column,	\
	      (Loc).last_line,  (Loc).last_column)
# else
#  define YY_LOCATION_PRINT(File, Loc) ((void) 0)
# endif
#endif


/* YYLEX -- calling `yylex' with the right arguments.  */

#ifdef YYLEX_PARAM
# define YYLEX yylex (YYLEX_PARAM)
#else
# define YYLEX yylex ()
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
} while (YYID (0))

# define YY_SYMBOL_PRINT(Title, Type, Value, Location)			  \
do {									  \
  if (yydebug)								  \
    {									  \
      YYFPRINTF (stderr, "%s ", Title);					  \
      yy_symbol_print (stderr,						  \
		  Type, Value); \
      YYFPRINTF (stderr, "\n");						  \
    }									  \
} while (YYID (0))


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep)
#else
static void
yy_symbol_value_print (yyoutput, yytype, yyvaluep)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
#endif
{
  if (!yyvaluep)
    return;
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# else
  YYUSE (yyoutput);
# endif
  switch (yytype)
    {
      default:
	break;
    }
}


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep)
#else
static void
yy_symbol_print (yyoutput, yytype, yyvaluep)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
#endif
{
  if (yytype < YYNTOKENS)
    YYFPRINTF (yyoutput, "token %s (", yytname[yytype]);
  else
    YYFPRINTF (yyoutput, "nterm %s (", yytname[yytype]);

  yy_symbol_value_print (yyoutput, yytype, yyvaluep);
  YYFPRINTF (yyoutput, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_stack_print (yytype_int16 *yybottom, yytype_int16 *yytop)
#else
static void
yy_stack_print (yybottom, yytop)
    yytype_int16 *yybottom;
    yytype_int16 *yytop;
#endif
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)				\
do {								\
  if (yydebug)							\
    yy_stack_print ((Bottom), (Top));				\
} while (YYID (0))


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_reduce_print (YYSTYPE *yyvsp, int yyrule)
#else
static void
yy_reduce_print (yyvsp, yyrule)
    YYSTYPE *yyvsp;
    int yyrule;
#endif
{
  int yynrhs = yyr2[yyrule];
  int yyi;
  unsigned long int yylno = yyrline[yyrule];
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %lu):\n",
	     yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr, yyrhs[yyprhs[yyrule] + yyi],
		       &(yyvsp[(yyi + 1) - (yynrhs)])
		       		       );
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)		\
do {					\
  if (yydebug)				\
    yy_reduce_print (yyvsp, Rule); \
} while (YYID (0))

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
# define YY_SYMBOL_PRINT(Title, Type, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef	YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif



#if YYERROR_VERBOSE

# ifndef yystrlen
#  if defined __GLIBC__ && defined _STRING_H
#   define yystrlen strlen
#  else
/* Return the length of YYSTR.  */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static YYSIZE_T
yystrlen (const char *yystr)
#else
static YYSIZE_T
yystrlen (yystr)
    const char *yystr;
#endif
{
  YYSIZE_T yylen;
  for (yylen = 0; yystr[yylen]; yylen++)
    continue;
  return yylen;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#   define yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static char *
yystpcpy (char *yydest, const char *yysrc)
#else
static char *
yystpcpy (yydest, yysrc)
    char *yydest;
    const char *yysrc;
#endif
{
  char *yyd = yydest;
  const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif

# ifndef yytnamerr
/* Copy to YYRES the contents of YYSTR after stripping away unnecessary
   quotes and backslashes, so that it's suitable for yyerror.  The
   heuristic is that double-quoting is unnecessary unless the string
   contains an apostrophe, a comma, or backslash (other than
   backslash-backslash).  YYSTR is taken from yytname.  If YYRES is
   null, do not copy; instead, return the length of what the result
   would have been.  */
static YYSIZE_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      YYSIZE_T yyn = 0;
      char const *yyp = yystr;

      for (;;)
	switch (*++yyp)
	  {
	  case '\'':
	  case ',':
	    goto do_not_strip_quotes;

	  case '\\':
	    if (*++yyp != '\\')
	      goto do_not_strip_quotes;
	    /* Fall through.  */
	  default:
	    if (yyres)
	      yyres[yyn] = *yyp;
	    yyn++;
	    break;

	  case '"':
	    if (yyres)
	      yyres[yyn] = '\0';
	    return yyn;
	  }
    do_not_strip_quotes: ;
    }

  if (! yyres)
    return yystrlen (yystr);

  return yystpcpy (yyres, yystr) - yyres;
}
# endif

/* Copy into YYRESULT an error message about the unexpected token
   YYCHAR while in state YYSTATE.  Return the number of bytes copied,
   including the terminating null byte.  If YYRESULT is null, do not
   copy anything; just return the number of bytes that would be
   copied.  As a special case, return 0 if an ordinary "syntax error"
   message will do.  Return YYSIZE_MAXIMUM if overflow occurs during
   size calculation.  */
static YYSIZE_T
yysyntax_error (char *yyresult, int yystate, int yychar)
{
  int yyn = yypact[yystate];

  if (! (YYPACT_NINF < yyn && yyn <= YYLAST))
    return 0;
  else
    {
      int yytype = YYTRANSLATE (yychar);
      YYSIZE_T yysize0 = yytnamerr (0, yytname[yytype]);
      YYSIZE_T yysize = yysize0;
      YYSIZE_T yysize1;
      int yysize_overflow = 0;
      enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
      char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
      int yyx;

# if 0
      /* This is so xgettext sees the translatable formats that are
	 constructed on the fly.  */
      YY_("syntax error, unexpected %s");
      YY_("syntax error, unexpected %s, expecting %s");
      YY_("syntax error, unexpected %s, expecting %s or %s");
      YY_("syntax error, unexpected %s, expecting %s or %s or %s");
      YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s");
# endif
      char *yyfmt;
      char const *yyf;
      static char const yyunexpected[] = "syntax error, unexpected %s";
      static char const yyexpecting[] = ", expecting %s";
      static char const yyor[] = " or %s";
      char yyformat[sizeof yyunexpected
		    + sizeof yyexpecting - 1
		    + ((YYERROR_VERBOSE_ARGS_MAXIMUM - 2)
		       * (sizeof yyor - 1))];
      char const *yyprefix = yyexpecting;

      /* Start YYX at -YYN if negative to avoid negative indexes in
	 YYCHECK.  */
      int yyxbegin = yyn < 0 ? -yyn : 0;

      /* Stay within bounds of both yycheck and yytname.  */
      int yychecklim = YYLAST - yyn + 1;
      int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
      int yycount = 1;

      yyarg[0] = yytname[yytype];
      yyfmt = yystpcpy (yyformat, yyunexpected);

      for (yyx = yyxbegin; yyx < yyxend; ++yyx)
	if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR)
	  {
	    if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
	      {
		yycount = 1;
		yysize = yysize0;
		yyformat[sizeof yyunexpected - 1] = '\0';
		break;
	      }
	    yyarg[yycount++] = yytname[yyx];
	    yysize1 = yysize + yytnamerr (0, yytname[yyx]);
	    yysize_overflow |= (yysize1 < yysize);
	    yysize = yysize1;
	    yyfmt = yystpcpy (yyfmt, yyprefix);
	    yyprefix = yyor;
	  }

      yyf = YY_(yyformat);
      yysize1 = yysize + yystrlen (yyf);
      yysize_overflow |= (yysize1 < yysize);
      yysize = yysize1;

      if (yysize_overflow)
	return YYSIZE_MAXIMUM;

      if (yyresult)
	{
	  /* Avoid sprintf, as that infringes on the user's name space.
	     Don't have undefined behavior even if the translation
	     produced a string with the wrong number of "%s"s.  */
	  char *yyp = yyresult;
	  int yyi = 0;
	  while ((*yyp = *yyf) != '\0')
	    {
	      if (*yyp == '%' && yyf[1] == 's' && yyi < yycount)
		{
		  yyp += yytnamerr (yyp, yyarg[yyi++]);
		  yyf += 2;
		}
	      else
		{
		  yyp++;
		  yyf++;
		}
	    }
	}
      return yysize;
    }
}
#endif /* YYERROR_VERBOSE */


/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep)
#else
static void
yydestruct (yymsg, yytype, yyvaluep)
    const char *yymsg;
    int yytype;
    YYSTYPE *yyvaluep;
#endif
{
  YYUSE (yyvaluep);

  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  switch (yytype)
    {

      default:
	break;
    }
}

/* Prevent warnings from -Wmissing-prototypes.  */
#ifdef YYPARSE_PARAM
#if defined __STDC__ || defined __cplusplus
int yyparse (void *YYPARSE_PARAM);
#else
int yyparse ();
#endif
#else /* ! YYPARSE_PARAM */
#if defined __STDC__ || defined __cplusplus
int yyparse (void);
#else
int yyparse ();
#endif
#endif /* ! YYPARSE_PARAM */


/* The lookahead symbol.  */
int yychar;

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval;

/* Number of syntax errors so far.  */
int yynerrs;



/*-------------------------.
| yyparse or yypush_parse.  |
`-------------------------*/

#ifdef YYPARSE_PARAM
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (void *YYPARSE_PARAM)
#else
int
yyparse (YYPARSE_PARAM)
    void *YYPARSE_PARAM;
#endif
#else /* ! YYPARSE_PARAM */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (void)
#else
int
yyparse ()

#endif
#endif
{


    int yystate;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus;

    /* The stacks and their tools:
       `yyss': related to states.
       `yyvs': related to semantic values.

       Refer to the stacks thru separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* The state stack.  */
    yytype_int16 yyssa[YYINITDEPTH];
    yytype_int16 *yyss;
    yytype_int16 *yyssp;

    /* The semantic value stack.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs;
    YYSTYPE *yyvsp;

    YYSIZE_T yystacksize;

  int yyn;
  int yyresult;
  /* Lookahead token as an internal (translated) token number.  */
  int yytoken;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;

#if YYERROR_VERBOSE
  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYSIZE_T yymsg_alloc = sizeof yymsgbuf;
#endif

#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  yytoken = 0;
  yyss = yyssa;
  yyvs = yyvsa;
  yystacksize = YYINITDEPTH;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY; /* Cause a token to be read.  */

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
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;

 yysetstate:
  *yyssp = yystate;

  if (yyss + yystacksize - 1 <= yyssp)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
	/* Give user a chance to reallocate the stack.  Use copies of
	   these so that the &'s don't force the real ones into
	   memory.  */
	YYSTYPE *yyvs1 = yyvs;
	yytype_int16 *yyss1 = yyss;

	/* Each stack pointer address is followed by the size of the
	   data in use in that stack, in bytes.  This used to be a
	   conditional around just the two extra args, but that might
	   be undefined if yyoverflow is a macro.  */
	yyoverflow (YY_("memory exhausted"),
		    &yyss1, yysize * sizeof (*yyssp),
		    &yyvs1, yysize * sizeof (*yyvsp),
		    &yystacksize);

	yyss = yyss1;
	yyvs = yyvs1;
      }
#else /* no yyoverflow */
# ifndef YYSTACK_RELOCATE
      goto yyexhaustedlab;
# else
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
	goto yyexhaustedlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
	yystacksize = YYMAXDEPTH;

      {
	yytype_int16 *yyss1 = yyss;
	union yyalloc *yyptr =
	  (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
	if (! yyptr)
	  goto yyexhaustedlab;
	YYSTACK_RELOCATE (yyss_alloc, yyss);
	YYSTACK_RELOCATE (yyvs_alloc, yyvs);
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

      if (yyss + yystacksize - 1 <= yyssp)
	YYABORT;
    }

  YYDPRINTF ((stderr, "Entering state %d\n", yystate));

  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;

/*-----------.
| yybackup.  |
`-----------*/
yybackup:

  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yyn == YYPACT_NINF)
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either YYEMPTY or YYEOF or a valid lookahead symbol.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = YYLEX;
    }

  if (yychar <= YYEOF)
    {
      yychar = yytoken = YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yyn == 0 || yyn == YYTABLE_NINF)
	goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);

  /* Discard the shifted token.  */
  yychar = YYEMPTY;

  yystate = yyn;
  *++yyvsp = yylval;

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


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
        case 7:

/* Line 1455 of yacc.c  */
#line 68 "configparser.y"
    { OUTYY(("\nP(server:)\n")); 
		if(server_settings_seen) {
			yyerror("duplicate server: element.");
		}
		server_settings_seen = 1;
	}
    break;

  case 35:

/* Line 1455 of yacc.c  */
#line 84 "configparser.y"
    { 
		OUTYY(("P(server_ip_address:%s)\n", (yyvsp[(2) - (2)].str))); 
		if(cfg_parser->current_ip_address_option) {
			cfg_parser->current_ip_address_option->next = 
				(ip_address_option_t*)region_alloc(
				cfg_parser->opt->region, sizeof(ip_address_option_t));
			cfg_parser->current_ip_address_option = 
				cfg_parser->current_ip_address_option->next;
			cfg_parser->current_ip_address_option->next=0;
		} else {
			cfg_parser->current_ip_address_option = 
				(ip_address_option_t*)region_alloc(
				cfg_parser->opt->region, sizeof(ip_address_option_t));
			cfg_parser->current_ip_address_option->next=0;
			cfg_parser->opt->ip_addresses = cfg_parser->current_ip_address_option;
		}

		cfg_parser->current_ip_address_option->address = 
			region_strdup(cfg_parser->opt->region, (yyvsp[(2) - (2)].str));
	}
    break;

  case 36:

/* Line 1455 of yacc.c  */
#line 106 "configparser.y"
    { 
		OUTYY(("P(server_debug_mode:%s)\n", (yyvsp[(2) - (2)].str))); 
		if(strcmp((yyvsp[(2) - (2)].str), "yes") != 0 && strcmp((yyvsp[(2) - (2)].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->opt->debug_mode = (strcmp((yyvsp[(2) - (2)].str), "yes")==0);
	}
    break;

  case 37:

/* Line 1455 of yacc.c  */
#line 114 "configparser.y"
    { 
		OUTYY(("P(server_verbosity:%s)\n", (yyvsp[(2) - (2)].str))); 
		if(atoi((yyvsp[(2) - (2)].str)) == 0 && strcmp((yyvsp[(2) - (2)].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->opt->verbosity = atoi((yyvsp[(2) - (2)].str));
	}
    break;

  case 38:

/* Line 1455 of yacc.c  */
#line 122 "configparser.y"
    { 
		OUTYY(("P(server_hide_version:%s)\n", (yyvsp[(2) - (2)].str))); 
		if(strcmp((yyvsp[(2) - (2)].str), "yes") != 0 && strcmp((yyvsp[(2) - (2)].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->opt->hide_version = (strcmp((yyvsp[(2) - (2)].str), "yes")==0);
	}
    break;

  case 39:

/* Line 1455 of yacc.c  */
#line 130 "configparser.y"
    { 
		OUTYY(("P(server_ip4_only:%s)\n", (yyvsp[(2) - (2)].str))); 
		if(strcmp((yyvsp[(2) - (2)].str), "yes") != 0 && strcmp((yyvsp[(2) - (2)].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->opt->ip4_only = (strcmp((yyvsp[(2) - (2)].str), "yes")==0);
	}
    break;

  case 40:

/* Line 1455 of yacc.c  */
#line 138 "configparser.y"
    { 
		OUTYY(("P(server_ip6_only:%s)\n", (yyvsp[(2) - (2)].str))); 
		if(strcmp((yyvsp[(2) - (2)].str), "yes") != 0 && strcmp((yyvsp[(2) - (2)].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->opt->ip6_only = (strcmp((yyvsp[(2) - (2)].str), "yes")==0);
	}
    break;

  case 41:

/* Line 1455 of yacc.c  */
#line 146 "configparser.y"
    { 
		OUTYY(("P(server_database:%s)\n", (yyvsp[(2) - (2)].str))); 
		cfg_parser->opt->database = region_strdup(cfg_parser->opt->region, (yyvsp[(2) - (2)].str));
	}
    break;

  case 42:

/* Line 1455 of yacc.c  */
#line 152 "configparser.y"
    { 
		OUTYY(("P(server_identity:%s)\n", (yyvsp[(2) - (2)].str))); 
		cfg_parser->opt->identity = region_strdup(cfg_parser->opt->region, (yyvsp[(2) - (2)].str));
	}
    break;

  case 43:

/* Line 1455 of yacc.c  */
#line 158 "configparser.y"
    { 
		unsigned char* nsid = 0;
		uint16_t nsid_len = 0;

		OUTYY(("P(server_nsid:%s)\n", (yyvsp[(2) - (2)].str)));

                if (strlen((yyvsp[(2) - (2)].str)) % 2 != 0) {
			yyerror("the NSID must be a hex string of an even length.");
		} else {
			nsid_len = strlen((yyvsp[(2) - (2)].str)) / 2;
			nsid = xalloc(nsid_len);
			if (hex_pton((yyvsp[(2) - (2)].str), nsid, nsid_len) == -1)
				yyerror("hex string cannot be parsed in NSID.");
			else
				cfg_parser->opt->nsid = region_strdup(cfg_parser->opt->region, (yyvsp[(2) - (2)].str));
			free(nsid);
		}
	}
    break;

  case 44:

/* Line 1455 of yacc.c  */
#line 178 "configparser.y"
    { 
		OUTYY(("P(server_logfile:%s)\n", (yyvsp[(2) - (2)].str))); 
		cfg_parser->opt->logfile = region_strdup(cfg_parser->opt->region, (yyvsp[(2) - (2)].str));
	}
    break;

  case 45:

/* Line 1455 of yacc.c  */
#line 184 "configparser.y"
    { 
		OUTYY(("P(server_server_count:%s)\n", (yyvsp[(2) - (2)].str))); 
		if(atoi((yyvsp[(2) - (2)].str)) <= 0)
			yyerror("number greater than zero expected");
		else cfg_parser->opt->server_count = atoi((yyvsp[(2) - (2)].str));
	}
    break;

  case 46:

/* Line 1455 of yacc.c  */
#line 192 "configparser.y"
    { 
		OUTYY(("P(server_tcp_count:%s)\n", (yyvsp[(2) - (2)].str))); 
		if(atoi((yyvsp[(2) - (2)].str)) <= 0)
			yyerror("number greater than zero expected");
		else cfg_parser->opt->tcp_count = atoi((yyvsp[(2) - (2)].str));
	}
    break;

  case 47:

/* Line 1455 of yacc.c  */
#line 200 "configparser.y"
    { 
		OUTYY(("P(server_pidfile:%s)\n", (yyvsp[(2) - (2)].str))); 
		cfg_parser->opt->pidfile = region_strdup(cfg_parser->opt->region, (yyvsp[(2) - (2)].str));
	}
    break;

  case 48:

/* Line 1455 of yacc.c  */
#line 206 "configparser.y"
    { 
		OUTYY(("P(server_port:%s)\n", (yyvsp[(2) - (2)].str))); 
		cfg_parser->opt->port = region_strdup(cfg_parser->opt->region, (yyvsp[(2) - (2)].str));
	}
    break;

  case 49:

/* Line 1455 of yacc.c  */
#line 212 "configparser.y"
    { 
		OUTYY(("P(server_statistics:%s)\n", (yyvsp[(2) - (2)].str))); 
		if(atoi((yyvsp[(2) - (2)].str)) == 0 && strcmp((yyvsp[(2) - (2)].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->opt->statistics = atoi((yyvsp[(2) - (2)].str));
	}
    break;

  case 50:

/* Line 1455 of yacc.c  */
#line 220 "configparser.y"
    { 
		OUTYY(("P(server_chroot:%s)\n", (yyvsp[(2) - (2)].str))); 
		cfg_parser->opt->chroot = region_strdup(cfg_parser->opt->region, (yyvsp[(2) - (2)].str));
	}
    break;

  case 51:

/* Line 1455 of yacc.c  */
#line 226 "configparser.y"
    { 
		OUTYY(("P(server_username:%s)\n", (yyvsp[(2) - (2)].str))); 
		cfg_parser->opt->username = region_strdup(cfg_parser->opt->region, (yyvsp[(2) - (2)].str));
	}
    break;

  case 52:

/* Line 1455 of yacc.c  */
#line 232 "configparser.y"
    { 
		OUTYY(("P(server_zonesdir:%s)\n", (yyvsp[(2) - (2)].str))); 
		cfg_parser->opt->zonesdir = region_strdup(cfg_parser->opt->region, (yyvsp[(2) - (2)].str));
	}
    break;

  case 53:

/* Line 1455 of yacc.c  */
#line 238 "configparser.y"
    { 
		OUTYY(("P(server_difffile:%s)\n", (yyvsp[(2) - (2)].str))); 
		cfg_parser->opt->difffile = region_strdup(cfg_parser->opt->region, (yyvsp[(2) - (2)].str));
	}
    break;

  case 54:

/* Line 1455 of yacc.c  */
#line 244 "configparser.y"
    { 
		OUTYY(("P(server_xfrdfile:%s)\n", (yyvsp[(2) - (2)].str))); 
		cfg_parser->opt->xfrdfile = region_strdup(cfg_parser->opt->region, (yyvsp[(2) - (2)].str));
	}
    break;

  case 55:

/* Line 1455 of yacc.c  */
#line 250 "configparser.y"
    { 
		OUTYY(("P(server_xfrd_reload_timeout:%s)\n", (yyvsp[(2) - (2)].str))); 
		if(atoi((yyvsp[(2) - (2)].str)) == 0 && strcmp((yyvsp[(2) - (2)].str), "0") != 0)
			yyerror("number expected");
		cfg_parser->opt->xfrd_reload_timeout = atoi((yyvsp[(2) - (2)].str));
	}
    break;

  case 56:

/* Line 1455 of yacc.c  */
#line 258 "configparser.y"
    { 
		OUTYY(("P(server_tcp_query_count:%s)\n", (yyvsp[(2) - (2)].str))); 
		if(atoi((yyvsp[(2) - (2)].str)) == 0 && strcmp((yyvsp[(2) - (2)].str), "0") != 0)
			yyerror("number expected");
		cfg_parser->opt->tcp_query_count = atoi((yyvsp[(2) - (2)].str));
	}
    break;

  case 57:

/* Line 1455 of yacc.c  */
#line 266 "configparser.y"
    { 
		OUTYY(("P(server_tcp_timeout:%s)\n", (yyvsp[(2) - (2)].str))); 
		if(atoi((yyvsp[(2) - (2)].str)) == 0 && strcmp((yyvsp[(2) - (2)].str), "0") != 0)
			yyerror("number expected");
		cfg_parser->opt->tcp_timeout = atoi((yyvsp[(2) - (2)].str));
	}
    break;

  case 58:

/* Line 1455 of yacc.c  */
#line 274 "configparser.y"
    { 
		OUTYY(("P(server_ipv4_edns_size:%s)\n", (yyvsp[(2) - (2)].str))); 
		if(atoi((yyvsp[(2) - (2)].str)) == 0 && strcmp((yyvsp[(2) - (2)].str), "0") != 0)
			yyerror("number expected");
		cfg_parser->opt->ipv4_edns_size = atoi((yyvsp[(2) - (2)].str));
	}
    break;

  case 59:

/* Line 1455 of yacc.c  */
#line 282 "configparser.y"
    { 
		OUTYY(("P(server_ipv6_edns_size:%s)\n", (yyvsp[(2) - (2)].str))); 
		if(atoi((yyvsp[(2) - (2)].str)) == 0 && strcmp((yyvsp[(2) - (2)].str), "0") != 0)
			yyerror("number expected");
		cfg_parser->opt->ipv6_edns_size = atoi((yyvsp[(2) - (2)].str));
	}
    break;

  case 60:

/* Line 1455 of yacc.c  */
#line 292 "configparser.y"
    { 
		OUTYY(("\nP(zone:)\n")); 
		if(cfg_parser->current_zone) {
			if(!cfg_parser->current_zone->name) 
				c_error("previous zone has no name");
			else {
				if(!nsd_options_insert_zone(cfg_parser->opt, 
					cfg_parser->current_zone))
					c_error("duplicate zone");
			}
			if(!cfg_parser->current_zone->zonefile) 
				c_error("previous zone has no zonefile");
		}
		cfg_parser->current_zone = zone_options_create(cfg_parser->opt->region);
		cfg_parser->current_allow_notify = 0;
		cfg_parser->current_request_xfr = 0;
		cfg_parser->current_notify = 0;
		cfg_parser->current_provide_xfr = 0;
		cfg_parser->current_outgoing_interface = 0;
	}
    break;

  case 72:

/* Line 1455 of yacc.c  */
#line 318 "configparser.y"
    { 
		OUTYY(("P(zone_name:%s)\n", (yyvsp[(2) - (2)].str))); 
#ifndef NDEBUG
		assert(cfg_parser->current_zone);
#endif
		cfg_parser->current_zone->name = region_strdup(cfg_parser->opt->region, (yyvsp[(2) - (2)].str));
	}
    break;

  case 73:

/* Line 1455 of yacc.c  */
#line 327 "configparser.y"
    { 
		OUTYY(("P(zone_zonefile:%s)\n", (yyvsp[(2) - (2)].str))); 
#ifndef NDEBUG
		assert(cfg_parser->current_zone);
#endif
		cfg_parser->current_zone->zonefile = region_strdup(cfg_parser->opt->region, (yyvsp[(2) - (2)].str));
	}
    break;

  case 74:

/* Line 1455 of yacc.c  */
#line 336 "configparser.y"
    { 
		acl_options_t* acl = parse_acl_info(cfg_parser->opt->region, (yyvsp[(2) - (3)].str), (yyvsp[(3) - (3)].str));
		OUTYY(("P(zone_allow_notify:%s %s)\n", (yyvsp[(2) - (3)].str), (yyvsp[(3) - (3)].str))); 
		if(cfg_parser->current_allow_notify)
			cfg_parser->current_allow_notify->next = acl;
		else
			cfg_parser->current_zone->allow_notify = acl;
		cfg_parser->current_allow_notify = acl;
	}
    break;

  case 75:

/* Line 1455 of yacc.c  */
#line 347 "configparser.y"
    {
	}
    break;

  case 76:

/* Line 1455 of yacc.c  */
#line 351 "configparser.y"
    { 
		acl_options_t* acl = parse_acl_info(cfg_parser->opt->region, (yyvsp[(1) - (2)].str), (yyvsp[(2) - (2)].str));
		OUTYY(("P(zone_request_xfr:%s %s)\n", (yyvsp[(1) - (2)].str), (yyvsp[(2) - (2)].str))); 
		if(acl->blocked) c_error("blocked address used for request-xfr");
		if(acl->rangetype!=acl_range_single) c_error("address range used for request-xfr");
		if(cfg_parser->current_request_xfr)
			cfg_parser->current_request_xfr->next = acl;
		else
			cfg_parser->current_zone->request_xfr = acl;
		cfg_parser->current_request_xfr = acl;
	}
    break;

  case 77:

/* Line 1455 of yacc.c  */
#line 363 "configparser.y"
    { 
		acl_options_t* acl = parse_acl_info(cfg_parser->opt->region, (yyvsp[(2) - (3)].str), (yyvsp[(3) - (3)].str));
		acl->use_axfr_only = 1;
		OUTYY(("P(zone_request_xfr:%s %s)\n", (yyvsp[(2) - (3)].str), (yyvsp[(3) - (3)].str))); 
		if(acl->blocked) c_error("blocked address used for request-xfr");
		if(acl->rangetype!=acl_range_single) c_error("address range used for request-xfr");
		if(cfg_parser->current_request_xfr)
			cfg_parser->current_request_xfr->next = acl;
		else
			cfg_parser->current_zone->request_xfr = acl;
		cfg_parser->current_request_xfr = acl;
	}
    break;

  case 78:

/* Line 1455 of yacc.c  */
#line 376 "configparser.y"
    { 
		acl_options_t* acl = parse_acl_info(cfg_parser->opt->region, (yyvsp[(2) - (3)].str), (yyvsp[(3) - (3)].str));
		acl->allow_udp = 1;
		OUTYY(("P(zone_request_xfr:%s %s)\n", (yyvsp[(2) - (3)].str), (yyvsp[(3) - (3)].str))); 
		if(acl->blocked) c_error("blocked address used for request-xfr");
		if(acl->rangetype!=acl_range_single) c_error("address range used for request-xfr");
		if(cfg_parser->current_request_xfr)
			cfg_parser->current_request_xfr->next = acl;
		else
			cfg_parser->current_zone->request_xfr = acl;
		cfg_parser->current_request_xfr = acl;
	}
    break;

  case 79:

/* Line 1455 of yacc.c  */
#line 390 "configparser.y"
    { 
		acl_options_t* acl = parse_acl_info(cfg_parser->opt->region, (yyvsp[(2) - (3)].str), (yyvsp[(3) - (3)].str));
		OUTYY(("P(zone_notify:%s %s)\n", (yyvsp[(2) - (3)].str), (yyvsp[(3) - (3)].str))); 
		if(acl->blocked) c_error("blocked address used for notify");
		if(acl->rangetype!=acl_range_single) c_error("address range used for notify");
		if(cfg_parser->current_notify)
			cfg_parser->current_notify->next = acl;
		else
			cfg_parser->current_zone->notify = acl;
		cfg_parser->current_notify = acl;
	}
    break;

  case 80:

/* Line 1455 of yacc.c  */
#line 403 "configparser.y"
    { 
		OUTYY(("P(zone_notify_retry:%s)\n", (yyvsp[(2) - (2)].str))); 
		if(atoi((yyvsp[(2) - (2)].str)) == 0 && strcmp((yyvsp[(2) - (2)].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->current_zone->notify_retry = atoi((yyvsp[(2) - (2)].str));
	}
    break;

  case 81:

/* Line 1455 of yacc.c  */
#line 411 "configparser.y"
    { 
		acl_options_t* acl = parse_acl_info(cfg_parser->opt->region, (yyvsp[(2) - (3)].str), (yyvsp[(3) - (3)].str));
		OUTYY(("P(zone_provide_xfr:%s %s)\n", (yyvsp[(2) - (3)].str), (yyvsp[(3) - (3)].str))); 
		if(cfg_parser->current_provide_xfr)
			cfg_parser->current_provide_xfr->next = acl;
		else
			cfg_parser->current_zone->provide_xfr = acl;
		cfg_parser->current_provide_xfr = acl;
	}
    break;

  case 82:

/* Line 1455 of yacc.c  */
#line 422 "configparser.y"
    { 
		acl_options_t* acl = parse_acl_info(cfg_parser->opt->region, (yyvsp[(2) - (2)].str), "NOKEY");
		OUTYY(("P(zone_outgoing_interface:%s)\n", (yyvsp[(2) - (2)].str))); 

		if(cfg_parser->current_outgoing_interface)
			cfg_parser->current_outgoing_interface->next = acl;
		else
			cfg_parser->current_zone->outgoing_interface = acl;
		cfg_parser->current_outgoing_interface = acl;
	}
    break;

  case 83:

/* Line 1455 of yacc.c  */
#line 434 "configparser.y"
    { 
		OUTYY(("P(zone_allow_axfr_fallback:%s)\n", (yyvsp[(2) - (2)].str))); 
		if(strcmp((yyvsp[(2) - (2)].str), "yes") != 0 && strcmp((yyvsp[(2) - (2)].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->current_zone->allow_axfr_fallback = (strcmp((yyvsp[(2) - (2)].str), "yes")==0);
	}
    break;

  case 84:

/* Line 1455 of yacc.c  */
#line 444 "configparser.y"
    { 
		OUTYY(("\nP(key:)\n")); 
		if(cfg_parser->current_key) {
			if(!cfg_parser->current_key->name) c_error("previous key has no name");
			if(!cfg_parser->current_key->algorithm) c_error("previous key has no algorithm");
			if(!cfg_parser->current_key->secret) c_error("previous key has no secret blob");
			cfg_parser->current_key->next = key_options_create(cfg_parser->opt->region);
			cfg_parser->current_key = cfg_parser->current_key->next;
		} else {
			cfg_parser->current_key = key_options_create(cfg_parser->opt->region);
                	cfg_parser->opt->keys = cfg_parser->current_key;
		}
		cfg_parser->opt->numkeys++;
	}
    break;

  case 90:

/* Line 1455 of yacc.c  */
#line 462 "configparser.y"
    { 
		OUTYY(("P(key_name:%s)\n", (yyvsp[(2) - (2)].str))); 
#ifndef NDEBUG
		assert(cfg_parser->current_key);
#endif
		cfg_parser->current_key->name = region_strdup(cfg_parser->opt->region, (yyvsp[(2) - (2)].str));
	}
    break;

  case 91:

/* Line 1455 of yacc.c  */
#line 471 "configparser.y"
    { 
		OUTYY(("P(key_algorithm:%s)\n", (yyvsp[(2) - (2)].str))); 
#ifndef NDEBUG
		assert(cfg_parser->current_key);
#endif
		cfg_parser->current_key->algorithm = region_strdup(cfg_parser->opt->region, (yyvsp[(2) - (2)].str));
	}
    break;

  case 92:

/* Line 1455 of yacc.c  */
#line 480 "configparser.y"
    { 
		OUTYY(("key_secret:%s)\n", (yyvsp[(2) - (2)].str))); 
#ifndef NDEBUG
		assert(cfg_parser->current_key);
#endif
		cfg_parser->current_key->secret = region_strdup(cfg_parser->opt->region, (yyvsp[(2) - (2)].str));
	}
    break;



/* Line 1455 of yacc.c  */
#line 2175 "configparser.c"
      default: break;
    }
  YY_SYMBOL_PRINT ("-> $$ =", yyr1[yyn], &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);

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
#if ! YYERROR_VERBOSE
      yyerror (YY_("syntax error"));
#else
      {
	YYSIZE_T yysize = yysyntax_error (0, yystate, yychar);
	if (yymsg_alloc < yysize && yymsg_alloc < YYSTACK_ALLOC_MAXIMUM)
	  {
	    YYSIZE_T yyalloc = 2 * yysize;
	    if (! (yysize <= yyalloc && yyalloc <= YYSTACK_ALLOC_MAXIMUM))
	      yyalloc = YYSTACK_ALLOC_MAXIMUM;
	    if (yymsg != yymsgbuf)
	      YYSTACK_FREE (yymsg);
	    yymsg = (char *) YYSTACK_ALLOC (yyalloc);
	    if (yymsg)
	      yymsg_alloc = yyalloc;
	    else
	      {
		yymsg = yymsgbuf;
		yymsg_alloc = sizeof yymsgbuf;
	      }
	  }

	if (0 < yysize && yysize <= yymsg_alloc)
	  {
	    (void) yysyntax_error (yymsg, yystate, yychar);
	    yyerror (yymsg);
	  }
	else
	  {
	    yyerror (YY_("syntax error"));
	    if (yysize != 0)
	      goto yyexhaustedlab;
	  }
      }
#endif
    }



  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
	 error, discard it.  */

      if (yychar <= YYEOF)
	{
	  /* Return failure if at end of input.  */
	  if (yychar == YYEOF)
	    YYABORT;
	}
      else
	{
	  yydestruct ("Error: discarding",
		      yytoken, &yylval);
	  yychar = YYEMPTY;
	}
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:

  /* Pacify compilers like GCC when the user code never invokes
     YYERROR and the label yyerrorlab therefore never appears in user
     code.  */
  if (/*CONSTCOND*/ 0)
     goto yyerrorlab;

  /* Do not reclaim the symbols of the rule which action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
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


      yydestruct ("Error: popping",
		  yystos[yystate], yyvsp);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  *++yyvsp = yylval;


  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", yystos[yyn], yyvsp, yylsp);

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

#if !defined(yyoverflow) || YYERROR_VERBOSE
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (YY_("memory exhausted"));
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
  if (yychar != YYEMPTY)
     yydestruct ("Cleanup: discarding lookahead",
		 yytoken, &yylval);
  /* Do not reclaim the symbols of the rule which action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
		  yystos[*yyssp], yyvsp);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
#if YYERROR_VERBOSE
  if (yymsg != yymsgbuf)
    YYSTACK_FREE (yymsg);
#endif
  /* Make sure YYID is used.  */
  return YYID (yyresult);
}



/* Line 1675 of yacc.c  */
#line 489 "configparser.y"


/* parse helper routines could be here */

