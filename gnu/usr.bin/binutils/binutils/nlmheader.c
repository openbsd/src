
/*  A Bison parser, made from ./nlmheader.y with Bison version GNU Bison version 1.24
  */

#define YYBISON 1  /* Identify Bison output.  */

#define	CHECK	258
#define	CODESTART	259
#define	COPYRIGHT	260
#define	CUSTOM	261
#define	DATE	262
#define	DEBUG	263
#define	DESCRIPTION	264
#define	EXIT	265
#define	EXPORT	266
#define	FLAG_ON	267
#define	FLAG_OFF	268
#define	FULLMAP	269
#define	HELP	270
#define	IMPORT	271
#define	INPUT	272
#define	MAP	273
#define	MESSAGES	274
#define	MODULE	275
#define	MULTIPLE	276
#define	OS_DOMAIN	277
#define	OUTPUT	278
#define	PSEUDOPREEMPTION	279
#define	REENTRANT	280
#define	SCREENNAME	281
#define	SHARELIB	282
#define	STACK	283
#define	START	284
#define	SYNCHRONIZE	285
#define	THREADNAME	286
#define	TYPE	287
#define	VERBOSE	288
#define	VERSION	289
#define	XDCDATA	290
#define	STRING	291
#define	QUOTED_STRING	292

#line 1 "./nlmheader.y"
/* nlmheader.y - parse NLM header specification keywords.
     Copyright (C) 1993 Free Software Foundation, Inc.

This file is part of GNU Binutils.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* Written by Ian Lance Taylor <ian@cygnus.com>.

   This bison file parses the commands recognized by the NetWare NLM
   linker, except for lists of object files.  It stores the
   information in global variables.

   This implementation is based on the description in the NetWare Tool
   Maker Specification manual, edition 1.0.  */

#include <ansidecl.h>
#include <stdio.h>
#include <ctype.h>
#include "bfd.h"
#include "sysdep.h"
#include "bucomm.h"
#include "nlm/common.h"
#include "nlm/internal.h"
#include "nlmconv.h"

/* Information is stored in the structures pointed to by these
   variables.  */

Nlm_Internal_Fixed_Header *fixed_hdr;
Nlm_Internal_Variable_Header *var_hdr;
Nlm_Internal_Version_Header *version_hdr;
Nlm_Internal_Copyright_Header *copyright_hdr;
Nlm_Internal_Extended_Header *extended_hdr;

/* Procedure named by CHECK.  */
char *check_procedure;
/* File named by CUSTOM.  */
char *custom_file;
/* Whether to generate debugging information (DEBUG).  */
boolean debug_info;
/* Procedure named by EXIT.  */
char *exit_procedure;
/* Exported symbols (EXPORT).  */
struct string_list *export_symbols;
/* List of files from INPUT.  */
struct string_list *input_files;
/* Map file name (MAP, FULLMAP).  */
char *map_file;
/* Whether a full map has been requested (FULLMAP).  */
boolean full_map;
/* File named by HELP.  */
char *help_file;
/* Imported symbols (IMPORT).  */
struct string_list *import_symbols;
/* File named by MESSAGES.  */
char *message_file;
/* Autoload module list (MODULE).  */
struct string_list *modules;
/* File named by OUTPUT.  */
char *output_file;
/* File named by SHARELIB.  */
char *sharelib_file;
/* Start procedure name (START).  */
char *start_procedure;
/* VERBOSE.  */
boolean verbose;
/* RPC description file (XDCDATA).  */
char *rpc_file;

/* The number of serious errors that have occurred.  */
int parse_errors;

/* The current symbol prefix when reading a list of import or export
   symbols.  */
static char *symbol_prefix;

/* Parser error message handler.  */
#define yyerror(msg) nlmheader_error (msg);

/* Local functions.  */
static int yylex PARAMS ((void));
static void nlmlex_file_push PARAMS ((const char *));
static boolean nlmlex_file_open PARAMS ((const char *));
static int nlmlex_buf_init PARAMS ((void));
static char nlmlex_buf_add PARAMS ((int));
static long nlmlex_get_number PARAMS ((const char *));
static void nlmheader_identify PARAMS ((void));
static void nlmheader_warn PARAMS ((const char *, int));
static void nlmheader_error PARAMS ((const char *));
static struct string_list * string_list_cons PARAMS ((char *,
						      struct string_list *));
static struct string_list * string_list_append PARAMS ((struct string_list *,
							struct string_list *));
static struct string_list * string_list_append1 PARAMS ((struct string_list *,
							 char *));
static char *xstrdup PARAMS ((const char *));


#line 113 "./nlmheader.y"
typedef union
{
  char *string;
  struct string_list *list;
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



#define	YYFINAL		82
#define	YYFLAG		-32768
#define	YYNTBASE	40

#define YYTRANSLATE(x) ((unsigned)(x) <= 292 ? yytranslate[x] : 50)

static const char yytranslate[] = {     0,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,    38,
    39,     2,     2,     2,     2,     2,     2,     2,     2,     2,
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
     2,     2,     2,     2,     2,     1,     2,     3,     4,     5,
     6,     7,     8,     9,    10,    11,    12,    13,    14,    15,
    16,    17,    18,    19,    20,    21,    22,    23,    24,    25,
    26,    27,    28,    29,    30,    31,    32,    33,    34,    35,
    36,    37
};

#if YYDEBUG != 0
static const short yyprhs[] = {     0,
     0,     2,     3,     6,     9,    12,    15,    18,    23,    25,
    28,    31,    32,    36,    39,    42,    44,    47,    50,    51,
    55,    58,    60,    63,    66,    69,    71,    73,    76,    78,
    80,    83,    86,    89,    92,    94,    97,   100,   102,   107,
   111,   114,   115,   117,   119,   121,   124,   127,   131,   133,
   134
};

static const short yyrhs[] = {    41,
     0,     0,    42,    41,     0,     3,    36,     0,     4,    36,
     0,     5,    37,     0,     6,    36,     0,     7,    36,    36,
    36,     0,     8,     0,     9,    37,     0,    10,    36,     0,
     0,    11,    43,    45,     0,    12,    36,     0,    13,    36,
     0,    14,     0,    14,    36,     0,    15,    36,     0,     0,
    16,    44,    45,     0,    17,    49,     0,    18,     0,    18,
    36,     0,    19,    36,     0,    20,    49,     0,    21,     0,
    22,     0,    23,    36,     0,    24,     0,    25,     0,    26,
    37,     0,    27,    36,     0,    28,    36,     0,    29,    36,
     0,    30,     0,    31,    37,     0,    32,    36,     0,    33,
     0,    34,    36,    36,    36,     0,    34,    36,    36,     0,
    35,    36,     0,     0,    46,     0,    48,     0,    47,     0,
    46,    48,     0,    46,    47,     0,    38,    36,    39,     0,
    36,     0,     0,    36,    49,     0
};

#endif

#if YYDEBUG != 0
static const short yyrline[] = { 0,
   144,   150,   152,   157,   162,   167,   184,   188,   206,   210,
   226,   230,   235,   238,   243,   248,   253,   258,   262,   267,
   270,   274,   278,   282,   286,   290,   294,   298,   305,   309,
   313,   329,   333,   338,   342,   346,   362,   367,   371,   395,
   411,   419,   424,   434,   439,   443,   447,   455,   466,   482,
   487
};

static const char * const yytname[] = {   "$","error","$undefined.","CHECK",
"CODESTART","COPYRIGHT","CUSTOM","DATE","DEBUG","DESCRIPTION","EXIT","EXPORT",
"FLAG_ON","FLAG_OFF","FULLMAP","HELP","IMPORT","INPUT","MAP","MESSAGES","MODULE",
"MULTIPLE","OS_DOMAIN","OUTPUT","PSEUDOPREEMPTION","REENTRANT","SCREENNAME",
"SHARELIB","STACK","START","SYNCHRONIZE","THREADNAME","TYPE","VERBOSE","VERSION",
"XDCDATA","STRING","QUOTED_STRING","'('","')'","file","commands","command","@1",
"@2","symbol_list_opt","symbol_list","symbol_prefix","symbol","string_list",
""
};
#endif

static const short yyr1[] = {     0,
    40,    41,    41,    42,    42,    42,    42,    42,    42,    42,
    42,    43,    42,    42,    42,    42,    42,    42,    44,    42,
    42,    42,    42,    42,    42,    42,    42,    42,    42,    42,
    42,    42,    42,    42,    42,    42,    42,    42,    42,    42,
    42,    45,    45,    46,    46,    46,    46,    47,    48,    49,
    49
};

static const short yyr2[] = {     0,
     1,     0,     2,     2,     2,     2,     2,     4,     1,     2,
     2,     0,     3,     2,     2,     1,     2,     2,     0,     3,
     2,     1,     2,     2,     2,     1,     1,     2,     1,     1,
     2,     2,     2,     2,     1,     2,     2,     1,     4,     3,
     2,     0,     1,     1,     1,     2,     2,     3,     1,     0,
     2
};

static const short yydefact[] = {     2,
     0,     0,     0,     0,     0,     9,     0,     0,    12,     0,
     0,    16,     0,    19,    50,    22,     0,    50,    26,    27,
     0,    29,    30,     0,     0,     0,     0,    35,     0,     0,
    38,     0,     0,     1,     2,     4,     5,     6,     7,     0,
    10,    11,    42,    14,    15,    17,    18,    42,    50,    21,
    23,    24,    25,    28,    31,    32,    33,    34,    36,    37,
     0,    41,     3,     0,    49,     0,    13,    43,    45,    44,
    20,    51,    40,     8,     0,    47,    46,    39,    48,     0,
     0,     0
};

static const short yydefgoto[] = {    80,
    34,    35,    43,    48,    67,    68,    69,    70,    50
};

static const short yypact[] = {    -3,
    -1,     1,     2,     4,     5,-32768,     6,     8,-32768,     9,
    10,    11,    12,-32768,    13,    14,    16,    13,-32768,-32768,
    17,-32768,-32768,    18,    20,    21,    22,-32768,    23,    25,
-32768,    26,    27,-32768,    -3,-32768,-32768,-32768,-32768,    29,
-32768,-32768,    -2,-32768,-32768,-32768,-32768,    -2,    13,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
    30,-32768,-32768,    31,-32768,    32,-32768,    -2,-32768,-32768,
-32768,-32768,    33,-32768,     3,-32768,-32768,-32768,-32768,    38,
    51,-32768
};

static const short yypgoto[] = {-32768,
    19,-32768,-32768,-32768,    24,-32768,    -9,     7,    15
};


#define	YYLAST		75


static const short yytable[] = {     1,
     2,     3,     4,     5,     6,     7,     8,     9,    10,    11,
    12,    13,    14,    15,    16,    17,    18,    19,    20,    21,
    22,    23,    24,    25,    26,    27,    28,    29,    30,    31,
    32,    33,    53,    65,    36,    66,    37,    81,    38,    39,
    40,    79,    41,    42,    44,    45,    46,    47,    49,    51,
    82,    52,    54,    63,    55,    56,    57,    58,    76,    59,
    60,    61,    62,    72,    64,    73,    74,    75,    78,     0,
     0,    71,     0,     0,    77
};

static const short yycheck[] = {     3,
     4,     5,     6,     7,     8,     9,    10,    11,    12,    13,
    14,    15,    16,    17,    18,    19,    20,    21,    22,    23,
    24,    25,    26,    27,    28,    29,    30,    31,    32,    33,
    34,    35,    18,    36,    36,    38,    36,     0,    37,    36,
    36,    39,    37,    36,    36,    36,    36,    36,    36,    36,
     0,    36,    36,    35,    37,    36,    36,    36,    68,    37,
    36,    36,    36,    49,    36,    36,    36,    36,    36,    -1,
    -1,    48,    -1,    -1,    68
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
#line 159 "./nlmheader.y"
{
	    check_procedure = yyvsp[0].string;
	  ;
    break;}
case 5:
#line 163 "./nlmheader.y"
{
	    nlmheader_warn ("CODESTART is not implemented; sorry", -1);
	    free (yyvsp[0].string);
	  ;
    break;}
case 6:
#line 168 "./nlmheader.y"
{
	    int len;

	    strncpy (copyright_hdr->stamp, "CoPyRiGhT=", 10);
	    len = strlen (yyvsp[0].string);
	    if (len >= NLM_MAX_COPYRIGHT_MESSAGE_LENGTH)
	      {
		nlmheader_warn ("copyright string is too long",
				NLM_MAX_COPYRIGHT_MESSAGE_LENGTH - 1);
		len = NLM_MAX_COPYRIGHT_MESSAGE_LENGTH - 1;
	      }
	    copyright_hdr->copyrightMessageLength = len;
	    strncpy (copyright_hdr->copyrightMessage, yyvsp[0].string, len);
	    copyright_hdr->copyrightMessage[len] = '\0';
	    free (yyvsp[0].string);
	  ;
    break;}
case 7:
#line 185 "./nlmheader.y"
{
	    custom_file = yyvsp[0].string;
	  ;
    break;}
case 8:
#line 189 "./nlmheader.y"
{
	    /* We don't set the version stamp here, because we use the
	       version stamp to detect whether the required VERSION
	       keyword was given.  */
	    version_hdr->month = nlmlex_get_number (yyvsp[-2].string);
	    version_hdr->day = nlmlex_get_number (yyvsp[-1].string);
	    version_hdr->year = nlmlex_get_number (yyvsp[0].string);
	    free (yyvsp[-2].string);
	    free (yyvsp[-1].string);
	    free (yyvsp[0].string);
	    if (version_hdr->month < 1 || version_hdr->month > 12)
	      nlmheader_warn ("illegal month", -1);
	    if (version_hdr->day < 1 || version_hdr->day > 31)
	      nlmheader_warn ("illegal day", -1);
	    if (version_hdr->year < 1900 || version_hdr->year > 3000)
	      nlmheader_warn ("illegal year", -1);
	  ;
    break;}
case 9:
#line 207 "./nlmheader.y"
{
	    debug_info = true;
	  ;
    break;}
case 10:
#line 211 "./nlmheader.y"
{
	    int len;

	    len = strlen (yyvsp[0].string);
	    if (len > NLM_MAX_DESCRIPTION_LENGTH)
	      {
		nlmheader_warn ("description string is too long",
				NLM_MAX_DESCRIPTION_LENGTH);
		len = NLM_MAX_DESCRIPTION_LENGTH;
	      }
	    var_hdr->descriptionLength = len;
	    strncpy (var_hdr->descriptionText, yyvsp[0].string, len);
	    var_hdr->descriptionText[len] = '\0';
	    free (yyvsp[0].string);
	  ;
    break;}
case 11:
#line 227 "./nlmheader.y"
{
	    exit_procedure = yyvsp[0].string;
	  ;
    break;}
case 12:
#line 231 "./nlmheader.y"
{
	    symbol_prefix = NULL;
	  ;
    break;}
case 13:
#line 235 "./nlmheader.y"
{
	    export_symbols = string_list_append (export_symbols, yyvsp[0].list);
	  ;
    break;}
case 14:
#line 239 "./nlmheader.y"
{
	    fixed_hdr->flags |= nlmlex_get_number (yyvsp[0].string);
	    free (yyvsp[0].string);
	  ;
    break;}
case 15:
#line 244 "./nlmheader.y"
{
	    fixed_hdr->flags &=~ nlmlex_get_number (yyvsp[0].string);
	    free (yyvsp[0].string);
	  ;
    break;}
case 16:
#line 249 "./nlmheader.y"
{
	    map_file = "";
	    full_map = true;
	  ;
    break;}
case 17:
#line 254 "./nlmheader.y"
{
	    map_file = yyvsp[0].string;
	    full_map = true;
	  ;
    break;}
case 18:
#line 259 "./nlmheader.y"
{
	    help_file = yyvsp[0].string;
	  ;
    break;}
case 19:
#line 263 "./nlmheader.y"
{
	    symbol_prefix = NULL;
	  ;
    break;}
case 20:
#line 267 "./nlmheader.y"
{
	    import_symbols = string_list_append (import_symbols, yyvsp[0].list);
	  ;
    break;}
case 21:
#line 271 "./nlmheader.y"
{
	    input_files = string_list_append (input_files, yyvsp[0].list);
	  ;
    break;}
case 22:
#line 275 "./nlmheader.y"
{
	    map_file = "";
	  ;
    break;}
case 23:
#line 279 "./nlmheader.y"
{
	    map_file = yyvsp[0].string;
	  ;
    break;}
case 24:
#line 283 "./nlmheader.y"
{
	    message_file = yyvsp[0].string;
	  ;
    break;}
case 25:
#line 287 "./nlmheader.y"
{
	    modules = string_list_append (modules, yyvsp[0].list);
	  ;
    break;}
case 26:
#line 291 "./nlmheader.y"
{
	    fixed_hdr->flags |= 0x2;
	  ;
    break;}
case 27:
#line 295 "./nlmheader.y"
{
	    fixed_hdr->flags |= 0x10;
	  ;
    break;}
case 28:
#line 299 "./nlmheader.y"
{
	    if (output_file == NULL)
	      output_file = yyvsp[0].string;
	    else
	      nlmheader_warn ("ignoring duplicate OUTPUT statement", -1);
	  ;
    break;}
case 29:
#line 306 "./nlmheader.y"
{
	    fixed_hdr->flags |= 0x8;
	  ;
    break;}
case 30:
#line 310 "./nlmheader.y"
{
	    fixed_hdr->flags |= 0x1;
	  ;
    break;}
case 31:
#line 314 "./nlmheader.y"
{
	    int len;

	    len = strlen (yyvsp[0].string);
	    if (len >= NLM_MAX_SCREEN_NAME_LENGTH)
	      {
		nlmheader_warn ("screen name is too long",
				NLM_MAX_SCREEN_NAME_LENGTH);
		len = NLM_MAX_SCREEN_NAME_LENGTH;
	      }
	    var_hdr->screenNameLength = len;
	    strncpy (var_hdr->screenName, yyvsp[0].string, len);
	    var_hdr->screenName[NLM_MAX_SCREEN_NAME_LENGTH] = '\0';
	    free (yyvsp[0].string);
	  ;
    break;}
case 32:
#line 330 "./nlmheader.y"
{
	    sharelib_file = yyvsp[0].string;
	  ;
    break;}
case 33:
#line 334 "./nlmheader.y"
{
	    var_hdr->stackSize = nlmlex_get_number (yyvsp[0].string);
	    free (yyvsp[0].string);
	  ;
    break;}
case 34:
#line 339 "./nlmheader.y"
{
	    start_procedure = yyvsp[0].string;
	  ;
    break;}
case 35:
#line 343 "./nlmheader.y"
{
	    fixed_hdr->flags |= 0x4;
	  ;
    break;}
case 36:
#line 347 "./nlmheader.y"
{
	    int len;

	    len = strlen (yyvsp[0].string);
	    if (len >= NLM_MAX_THREAD_NAME_LENGTH)
	      {
		nlmheader_warn ("thread name is too long",
				NLM_MAX_THREAD_NAME_LENGTH);
		len = NLM_MAX_THREAD_NAME_LENGTH;
	      }
	    var_hdr->threadNameLength = len;
	    strncpy (var_hdr->threadName, yyvsp[0].string, len);
	    var_hdr->threadName[len] = '\0';
	    free (yyvsp[0].string);
	  ;
    break;}
case 37:
#line 363 "./nlmheader.y"
{
	    fixed_hdr->moduleType = nlmlex_get_number (yyvsp[0].string);
	    free (yyvsp[0].string);
	  ;
    break;}
case 38:
#line 368 "./nlmheader.y"
{
	    verbose = true;
	  ;
    break;}
case 39:
#line 372 "./nlmheader.y"
{
	    long val;

	    strncpy (version_hdr->stamp, "VeRsIoN#", 8);
	    version_hdr->majorVersion = nlmlex_get_number (yyvsp[-2].string);
	    val = nlmlex_get_number (yyvsp[-1].string);
	    if (val < 0 || val > 99)
	      nlmheader_warn ("illegal minor version number (must be between 0 and 99)",
			      -1);
	    else
	      version_hdr->minorVersion = val;
	    val = nlmlex_get_number (yyvsp[0].string);
	    if (val < 0)
	      nlmheader_warn ("illegal revision number (must be between 0 and 26)",
			      -1);
	    else if (val > 26)
	      version_hdr->revision = 0;
	    else
	      version_hdr->revision = val;
	    free (yyvsp[-2].string);
	    free (yyvsp[-1].string);
	    free (yyvsp[0].string);
	  ;
    break;}
case 40:
#line 396 "./nlmheader.y"
{
	    long val;

	    strncpy (version_hdr->stamp, "VeRsIoN#", 8);
	    version_hdr->majorVersion = nlmlex_get_number (yyvsp[-1].string);
	    val = nlmlex_get_number (yyvsp[0].string);
	    if (val < 0 || val > 99)
	      nlmheader_warn ("illegal minor version number (must be between 0 and 99)",
			      -1);
	    else
	      version_hdr->minorVersion = val;
	    version_hdr->revision = 0;
	    free (yyvsp[-1].string);
	    free (yyvsp[0].string);
	  ;
    break;}
case 41:
#line 412 "./nlmheader.y"
{
	    rpc_file = yyvsp[0].string;
	  ;
    break;}
case 42:
#line 421 "./nlmheader.y"
{
	    yyval.list = NULL;
	  ;
    break;}
case 43:
#line 425 "./nlmheader.y"
{
	    yyval.list = yyvsp[0].list;
	  ;
    break;}
case 44:
#line 436 "./nlmheader.y"
{
	    yyval.list = string_list_cons (yyvsp[0].string, NULL);
	  ;
    break;}
case 45:
#line 440 "./nlmheader.y"
{
	    yyval.list = NULL;
	  ;
    break;}
case 46:
#line 444 "./nlmheader.y"
{
	    yyval.list = string_list_append1 (yyvsp[-1].list, yyvsp[0].string);
	  ;
    break;}
case 47:
#line 448 "./nlmheader.y"
{
	    yyval.list = yyvsp[-1].list;
	  ;
    break;}
case 48:
#line 457 "./nlmheader.y"
{
	    if (symbol_prefix != NULL)
	      free (symbol_prefix);
	    symbol_prefix = yyvsp[-1].string;
	  ;
    break;}
case 49:
#line 468 "./nlmheader.y"
{
	    if (symbol_prefix == NULL)
	      yyval.string = yyvsp[0].string;
	    else
	      {
		yyval.string = xmalloc (strlen (symbol_prefix) + strlen (yyvsp[0].string) + 2);
		sprintf (yyval.string, "%s@%s", symbol_prefix, yyvsp[0].string);
		free (yyvsp[0].string);
	      }
	  ;
    break;}
case 50:
#line 484 "./nlmheader.y"
{
	    yyval.list = NULL;
	  ;
    break;}
case 51:
#line 488 "./nlmheader.y"
{
	    yyval.list = string_list_cons (yyvsp[-1].string, yyvsp[0].list);
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
#line 493 "./nlmheader.y"


/* If strerror is just a macro, we want to use the one from libiberty
   since it will handle undefined values.  */
#undef strerror
extern char *strerror ();

/* The lexer is simple, too simple for flex.  Keywords are only
   recognized at the start of lines.  Everything else must be an
   argument.  A comma is treated as whitespace.  */

/* The states the lexer can be in.  */

enum lex_state
{
  /* At the beginning of a line.  */
  BEGINNING_OF_LINE,
  /* In the middle of a line.  */
  IN_LINE
};

/* We need to keep a stack of files to handle file inclusion.  */

struct input
{
  /* The file to read from.  */
  FILE *file;
  /* The name of the file.  */
  char *name;
  /* The current line number.  */
  int lineno;
  /* The current state.  */
  enum lex_state state;
  /* The next file on the stack.  */
  struct input *next;
};

/* The current input file.  */

static struct input current;

/* The character which introduces comments.  */
#define COMMENT_CHAR '#'

/* Start the lexer going on the main input file.  */

boolean
nlmlex_file (name)
     const char *name;
{
  current.next = NULL;
  return nlmlex_file_open (name);
}

/* Start the lexer going on a subsidiary input file.  */

static void
nlmlex_file_push (name)
     const char *name;
{
  struct input *push;

  push = (struct input *) xmalloc (sizeof (struct input));
  *push = current;
  if (nlmlex_file_open (name))
    current.next = push;
  else
    {
      current = *push;
      free (push);
    }
}

/* Start lexing from a file.  */

static boolean
nlmlex_file_open (name)
     const char *name;
{
  current.file = fopen (name, "r");
  if (current.file == NULL)
    {
      fprintf (stderr, "%s:%s: %s\n", program_name, name, strerror (errno));
      ++parse_errors;
      return false;
    }
  current.name = xstrdup (name);
  current.lineno = 1;
  current.state = BEGINNING_OF_LINE;
  return true;
}

/* Table used to turn keywords into tokens.  */

struct keyword_tokens_struct
{
  const char *keyword;
  int token;
};

struct keyword_tokens_struct keyword_tokens[] =
{
  { "CHECK", CHECK },
  { "CODESTART", CODESTART },
  { "COPYRIGHT", COPYRIGHT },
  { "CUSTOM", CUSTOM },
  { "DATE", DATE },
  { "DEBUG", DEBUG },
  { "DESCRIPTION", DESCRIPTION },
  { "EXIT", EXIT },
  { "EXPORT", EXPORT },
  { "FLAG_ON", FLAG_ON },
  { "FLAG_OFF", FLAG_OFF },
  { "FULLMAP", FULLMAP },
  { "HELP", HELP },
  { "IMPORT", IMPORT },
  { "INPUT", INPUT },
  { "MAP", MAP },
  { "MESSAGES", MESSAGES },
  { "MODULE", MODULE },
  { "MULTIPLE", MULTIPLE },
  { "OS_DOMAIN", OS_DOMAIN },
  { "OUTPUT", OUTPUT },
  { "PSEUDOPREEMPTION", PSEUDOPREEMPTION },
  { "REENTRANT", REENTRANT },
  { "SCREENNAME", SCREENNAME },
  { "SHARELIB", SHARELIB },
  { "STACK", STACK },
  { "STACKSIZE", STACK },
  { "START", START },
  { "SYNCHRONIZE", SYNCHRONIZE },
  { "THREADNAME", THREADNAME },
  { "TYPE", TYPE },
  { "VERBOSE", VERBOSE },
  { "VERSION", VERSION },
  { "XDCDATA", XDCDATA }
};

#define KEYWORD_COUNT (sizeof (keyword_tokens) / sizeof (keyword_tokens[0]))

/* The lexer accumulates strings in these variables.  */
static char *lex_buf;
static int lex_size;
static int lex_pos;

/* Start accumulating strings into the buffer.  */
#define BUF_INIT() \
  ((void) (lex_buf != NULL ? lex_pos = 0 : nlmlex_buf_init ()))

static int
nlmlex_buf_init ()
{
  lex_size = 10;
  lex_buf = xmalloc (lex_size + 1);
  lex_pos = 0;
  return 0;
}

/* Finish a string in the buffer.  */
#define BUF_FINISH() ((void) (lex_buf[lex_pos] = '\0'))

/* Accumulate a character into the buffer.  */
#define BUF_ADD(c) \
  ((void) (lex_pos < lex_size \
	   ? lex_buf[lex_pos++] = (c) \
	   : nlmlex_buf_add (c)))

static char
nlmlex_buf_add (c)
     int c;
{
  if (lex_pos >= lex_size)
    {
      lex_size *= 2;
      lex_buf = xrealloc (lex_buf, lex_size + 1);
    }

  return lex_buf[lex_pos++] = c;
}

/* The lexer proper.  This is called by the bison generated parsing
   code.  */

static int
yylex ()
{
  int c;

tail_recurse:

  c = getc (current.file);

  /* Commas are treated as whitespace characters.  */
  while (isspace ((unsigned char) c) || c == ',')
    {
      current.state = IN_LINE;
      if (c == '\n')
	{
	  ++current.lineno;
	  current.state = BEGINNING_OF_LINE;
	}
      c = getc (current.file);
    }

  /* At the end of the file we either pop to the previous file or
     finish up.  */
  if (c == EOF)
    {
      fclose (current.file);
      free (current.name);
      if (current.next == NULL)
	return 0;
      else
	{
	  struct input *next;

	  next = current.next;
	  current = *next;
	  free (next);
	  goto tail_recurse;
	}
    }

  /* A comment character always means to drop everything until the
     next newline.  */
  if (c == COMMENT_CHAR)
    {
      do
	{
	  c = getc (current.file);
	}
      while (c != '\n');
      ++current.lineno;
      current.state = BEGINNING_OF_LINE;
      goto tail_recurse;
    }

  /* An '@' introduces an include file.  */
  if (c == '@')
    {
      do
	{
	  c = getc (current.file);
	  if (c == '\n')
	    ++current.lineno;
	}
      while (isspace ((unsigned char) c));
      BUF_INIT ();
      while (! isspace ((unsigned char) c) && c != EOF)
	{
	  BUF_ADD (c);
	  c = getc (current.file);
	}
      BUF_FINISH ();

      ungetc (c, current.file);
      
      nlmlex_file_push (lex_buf);
      goto tail_recurse;
    }

  /* A non-space character at the start of a line must be the start of
     a keyword.  */
  if (current.state == BEGINNING_OF_LINE)
    {
      BUF_INIT ();
      while (isalnum ((unsigned char) c) || c == '_')
	{
	  if (islower ((unsigned char) c))
	    BUF_ADD (toupper ((unsigned char) c));
	  else
	    BUF_ADD (c);
	  c = getc (current.file);
	}
      BUF_FINISH ();

      if (c != EOF && ! isspace ((unsigned char) c) && c != ',')
	{
	  nlmheader_identify ();
	  fprintf (stderr, "%s:%d: illegal character in keyword: %c\n",
		   current.name, current.lineno, c);
	}
      else
	{
	  int i;

	  for (i = 0; i < KEYWORD_COUNT; i++)
	    {
	      if (lex_buf[0] == keyword_tokens[i].keyword[0]
		  && strcmp (lex_buf, keyword_tokens[i].keyword) == 0)
		{
		  /* Pushing back the final whitespace avoids worrying
		     about \n here.  */
		  ungetc (c, current.file);
		  current.state = IN_LINE;
		  return keyword_tokens[i].token;
		}
	    }
	  
	  nlmheader_identify ();
	  fprintf (stderr, "%s:%d: unrecognized keyword: %s\n",
		   current.name, current.lineno, lex_buf);
	}

      ++parse_errors;
      /* Treat the rest of this line as a comment.  */
      ungetc (COMMENT_CHAR, current.file);
      goto tail_recurse;
    }

  /* Parentheses just represent themselves.  */
  if (c == '(' || c == ')')
    return c;

  /* Handle quoted strings.  */
  if (c == '"' || c == '\'')
    {
      int quote;
      int start_lineno;

      quote = c;
      start_lineno = current.lineno;

      c = getc (current.file);
      BUF_INIT ();
      while (c != quote && c != EOF)
	{
	  BUF_ADD (c);
	  if (c == '\n')
	    ++current.lineno;
	  c = getc (current.file);
	}
      BUF_FINISH ();

      if (c == EOF)
	{
	  nlmheader_identify ();
	  fprintf (stderr, "%s:%d: end of file in quoted string\n",
		   current.name, start_lineno);
	  ++parse_errors;
	}

      /* FIXME: Possible memory leak.  */
      yylval.string = xstrdup (lex_buf);
      return QUOTED_STRING;
    }

  /* Gather a generic argument.  */
  BUF_INIT ();
  while (! isspace (c)
	 && c != ','
	 && c != COMMENT_CHAR
	 && c != '('
	 && c != ')')
    {
      BUF_ADD (c);
      c = getc (current.file);
    }
  BUF_FINISH ();

  ungetc (c, current.file);

  /* FIXME: Possible memory leak.  */
  yylval.string = xstrdup (lex_buf);
  return STRING;
}

/* Get a number from a string.  */

static long
nlmlex_get_number (s)
     const char *s;
{
  long ret;
  char *send;

  ret = strtol (s, &send, 10);
  if (*send != '\0')
    nlmheader_warn ("bad number", -1);
  return ret;
}

/* Prefix the nlmconv warnings with a note as to where they come from.
   We don't use program_name on every warning, because then some
   versions of the emacs next-error function can't recognize the line
   number.  */

static void
nlmheader_identify ()
{
  static int done;

  if (! done)
    {
      fprintf (stderr, "%s: problems in NLM command language input:\n",
	       program_name);
      done = 1;
    }
}

/* Issue a warning.  */

static void
nlmheader_warn (s, imax)
     const char *s;
     int imax;
{
  nlmheader_identify ();
  fprintf (stderr, "%s:%d: %s", current.name, current.lineno, s);
  if (imax != -1)
    fprintf (stderr, " (max %d)", imax);
  fprintf (stderr, "\n");
}

/* Report an error.  */

static void
nlmheader_error (s)
     const char *s;
{
  nlmheader_warn (s, -1);
  ++parse_errors;
}

/* Add a string to a string list.  */

static struct string_list *
string_list_cons (s, l)
     char *s;
     struct string_list *l;
{
  struct string_list *ret;

  ret = (struct string_list *) xmalloc (sizeof (struct string_list));
  ret->next = l;
  ret->string = s;
  return ret;
}

/* Append a string list to another string list.  */

static struct string_list *
string_list_append (l1, l2)
     struct string_list *l1;
     struct string_list *l2;
{
  register struct string_list **pp;

  for (pp = &l1; *pp != NULL; pp = &(*pp)->next)
    ;
  *pp = l2;
  return l1;
}

/* Append a string to a string list.  */

static struct string_list *
string_list_append1 (l, s)
     struct string_list *l;
     char *s;
{
  struct string_list *n;
  register struct string_list **pp;

  n = (struct string_list *) xmalloc (sizeof (struct string_list));
  n->next = NULL;
  n->string = s;
  for (pp = &l; *pp != NULL; pp = &(*pp)->next)
    ;
  *pp = n;
  return l;
}

/* Duplicate a string in memory.  */

static char *
xstrdup (s)
     const char *s;
{
  unsigned long len;
  char *ret;

  len = strlen (s);
  ret = xmalloc (len + 1);
  strcpy (ret, s);
  return ret;
}
