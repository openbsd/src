
/*  A Bison parser, made from itbl-parse.y
    by GNU Bison version 1.28  */

#define YYBISON 1  /* Identify Bison output.  */

#define	DREG	257
#define	CREG	258
#define	GREG	259
#define	IMMED	260
#define	ADDR	261
#define	INSN	262
#define	NUM	263
#define	ID	264
#define	NL	265
#define	PNUM	266

#line 21 "itbl-parse.y"


/* 

Yacc grammar for instruction table entries.

=======================================================================
Original Instruction table specification document:

	    MIPS Coprocessor Table Specification
	    ====================================

This document describes the format of the MIPS coprocessor table.  The
table specifies a list of valid functions, data registers and control
registers that can be used in coprocessor instructions.  This list,
together with the coprocessor instruction classes listed below,
specifies the complete list of coprocessor instructions that will
be recognized and assembled by the GNU assembler.  In effect,
this makes the GNU assembler table-driven, where the table is
specified by the programmer.

The table is an ordinary text file that the GNU assembler reads when
it starts.  Using the information in the table, the assembler
generates an internal list of valid coprocessor registers and
functions.  The assembler uses this internal list in addition to the
standard MIPS registers and instructions which are built-in to the 
assembler during code generation.

To specify the coprocessor table when invoking the GNU assembler, use
the command line option "--itbl file", where file is the
complete name of the table, including path and extension.

Examples:

	    gas -t cop.tbl test.s -o test.o
	    gas -t /usr/local/lib/cop.tbl test.s -o test.o
	    gas --itbl d:\gnu\data\cop.tbl test.s -o test.o

Only one table may be supplied during a single invocation of
the assembler.


Instruction classes
===================

Below is a list of the valid coprocessor instruction classes for
any given coprocessor "z".  These instructions are already recognized
by the assembler, and are listed here only for reference.

Class   format	    	    	      instructions
-------------------------------------------------
Class1:
	op base rt offset
	    	    	    	    	    	    	    LWCz rt,offset (base)
	    	    	    	    	    	    	    SWCz rt,offset (base)
Class2:
	COPz sub rt rd 0
	    	    	    	    	    	    	    MTCz rt,rd
	    	    	    	    	    	    	    MFCz rt,rd
	    	    	    	    	    	    	    CTCz rt,rd
	    	    	    	    	    	    	    CFCz rt,rd
Class3:
	COPz CO cofun
	    	    	    	    	    	    	    COPz cofun
Class4:
	COPz BC br offset
	    	    	    	    	    	    	    BCzT offset
	    	    	    	    	    	    	    BCzF offset
Class5:
	COPz sub rt rd 0
	    	    	    	    	    	    	    DMFCz rt,rd
	    	    	    	    	    	    	    DMTCz rt,rd
Class6:
	op base rt offset
	    	    	    	    	    	    	    LDCz rt,offset (base)
	    	    	    	    	    	    	    SDCz rt,offset (base)
Class7:
	COPz BC br offset
	    	    	    	    	    	    	    BCzTL offset
	    	    	    	    	    	    	    BCzFL offset

The coprocessor table defines coprocessor-specific registers that can
be used with all of the above classes of instructions, where
appropriate.  It also defines additional coprocessor-specific
functions for Class3 (COPz cofun) instructions, Thus, the table allows
the programmer to use convenient mnemonics and operands for these
functions, instead of the COPz mmenmonic and cofun operand.

The names of the MIPS general registers and their aliases are defined
by the assembler and will be recognized as valid register names by the
assembler when used (where allowed) in coprocessor instructions.
However, the names and values of all coprocessor data and control
register mnemonics must be specified in the coprocessor table.


Table Grammar
=============

Here is the grammar for the coprocessor table:

	    table -> entry*

	    entry -> [z entrydef] [comment] '\n'

	    entrydef -> type name val
	    entrydef -> 'insn' name val funcdef ; type of entry (instruction)

	    z -> 'p'['0'..'3']	    	     ; processor number 
	    type -> ['dreg' | 'creg' | 'greg' ]	     ; type of entry (register)
	; 'dreg', 'creg' or 'greg' specifies a data, control, or general
	;	    register mnemonic, respectively
	    name -> [ltr|dec]*	    	     ; mnemonic of register/function
	    val -> [dec|hex]	    	     ; register/function number (integer constant)

	    funcdef -> frange flags fields
	    	    	    	; bitfield range for opcode
	    	    	    	; list of fields' formats
	    fields -> field*
	    field -> [','] ftype frange flags
	    flags -> ['*' flagexpr]
	    flagexpr -> '[' flagexpr ']'
	    flagexpr -> val '|' flagexpr 
	    ftype -> [ type | 'immed' | 'addr' ]
	; 'immed' specifies an immediate value; see grammar for "val" above
	    	; 'addr' specifies a C identifier; name of symbol to be resolved at 
	;	    link time
	    frange -> ':' val '-' val	; starting to ending bit positions, where
	    	    	    	; where 0 is least significant bit
	    frange -> (null)	    	; default range of 31-0 will be assumed

	    comment -> [';'|'#'] [char]*
	    char -> any printable character
	    ltr -> ['a'..'z'|'A'..'Z'] 
	    dec -> ['0'..'9']*	    	    	    	    	     ; value in decimal
	    hex -> '0x'['0'..'9' | 'a'..'f' | 'A'..'F']*	; value in hexidecimal 


Examples
========

Example 1:

The table:

	    p1 dreg d1 1	     ; data register "d1" for COP1 has value 1
	    p1 creg c3 3	     ; ctrl register "c3" for COP1 has value 3
	    p3 func fill 0x1f:24-20	      ; function "fill" for COP3 has value 31 and 
	    	    	; no fields

will allow the assembler to accept the following coprocessor instructions:

	    LWC1 d1,0x100 ($2)
	    fill

Here, the general purpose register "$2", and instruction "LWC1", are standard 
mnemonics built-in to the MIPS assembler.  


Example 2:

The table:

	    p3 dreg d3 3	     ; data register "d3" for COP3 has value 3
	    p3 creg c2 22	     ; control register "c2" for COP3 has value 22
	    p3 func fee 0x1f:24-20 dreg:17-13 creg:12-8 immed:7-0 
	    	; function "fee" for COP3 has value 31, and 3 fields 
	    	; consisting of a data register, a control register, 
	    	; and an immediate value.

will allow the assembler to accept the following coprocessor instruction:

	    fee d3,c2,0x1

and will emit the object code:

	    31-26  25 24-20 19-18  17-13 12-8  7-0
	    COPz   CO fun	    	      dreg  creg  immed
	    010011 1  11111 00	     00011 10110 00000001 

	    0x4ff07601


Example 3:

The table:

	    p3 dreg d3 3	     ; data register "d3" for COP3 has value 3
	    p3 creg c2 22	     ; control register "c2" for COP3 has value 22
	    p3 func fuu 0x01f00001 dreg:17-13 creg:12-8

will allow the assembler to accept the following coprocessor
instruction:

	    fuu d3,c2

and will emit the object code:

	    31-26  25 24-20 19-18  17-13 12-8  7-0
	    COPz   CO fun	    	      dreg  creg  
	    010011 1  11111 00	     00011 10110 00000001 

	    0x4ff07601

In this way, the programmer can force arbitrary bits of an instruction
to have predefined values.

=======================================================================
Additional notes:

Encoding of ranges:
To handle more than one bit position range within an instruction,
use 0s to mask out the ranges which don't apply.
May decide to modify the syntax to allow commas separate multiple 
ranges within an instruction (range','range).

Changes in grammar:
	The number of parms argument to the function entry
was deleted from the original format such that we now count the fields.

----
FIXME! should really change lexical analyzer 
to recognize 'dreg' etc. in context sensative way.
Currently function names or mnemonics may be incorrectly parsed as keywords

FIXME! hex is ambiguous with any digit

*/

#include <stdio.h>
#include "itbl-ops.h"

/* #define DEBUG */

#ifdef DEBUG
#ifndef DBG_LVL
#define DBG_LVL 1
#endif
#else
#define DBG_LVL 0
#endif

#if DBG_LVL >= 1
#define DBG(x) printf x
#else
#define DBG(x) 
#endif

#if DBG_LVL >= 2
#define DBGL2(x) printf x
#else
#define DBGL2(x) 
#endif

static int sbit, ebit;
static struct itbl_entry *insn=0;
extern int insntbl_line;
int yyparse PARAMS ((void));
int yylex PARAMS ((void));
static int yyerror PARAMS ((const char *));


#line 283 "itbl-parse.y"
typedef union 
  {
    char *str;
    int num;
    int processor;
    unsigned long val;
  } YYSTYPE;
#include <stdio.h>

#ifndef __cplusplus
#ifndef __STDC__
#define const
#endif
#endif



#define	YYFINAL		51
#define	YYFLAG		-32768
#define	YYNTBASE	20

#define YYTRANSLATE(x) ((unsigned)(x) <= 266 ? yytranslate[x] : 34)

static const char yytranslate[] = {     0,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,    17,     2,    13,    19,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,    18,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
    15,     2,    16,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,    14,     2,     2,     2,     2,     2,     2,
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
     7,     8,     9,    10,    11,    12
};

#if YYDEBUG != 0
static const short yyprhs[] = {     0,
     0,     2,     5,     6,    12,    13,    23,    25,    28,    32,
    35,    36,    38,    40,    42,    46,    50,    54,    56,    59,
    60,    65,    66,    68,    70,    72,    74,    76,    78
};

static const short yyrhs[] = {    21,
     0,    22,    21,     0,     0,    30,    31,    32,    33,    11,
     0,     0,    30,     8,    32,    33,    29,    28,    23,    24,
    11,     0,    11,     0,     1,    11,     0,    13,    26,    24,
     0,    26,    24,     0,     0,    31,     0,     7,     0,     6,
     0,    25,    29,    28,     0,     9,    14,    27,     0,    15,
    27,    16,     0,     9,     0,    17,    27,     0,     0,    18,
     9,    19,     9,     0,     0,    12,     0,     3,     0,     4,
     0,     5,     0,    10,     0,     9,     0,     9,     0
};

#endif

#if YYDEBUG != 0
static const short yyrline[] = { 0,
   300,   304,   306,   309,   316,   323,   324,   325,   328,   330,
   331,   334,   340,   345,   352,   361,   366,   370,   376,   382,
   388,   395,   402,   410,   416,   421,   428,   436,   444
};
#endif


#if YYDEBUG != 0 || defined (YYERROR_VERBOSE)

static const char * const yytname[] = {   "$","error","$undefined.","DREG","CREG",
"GREG","IMMED","ADDR","INSN","NUM","ID","NL","PNUM","','","'|'","'['","']'",
"'*'","':'","'-'","insntbl","entrys","entry","@1","fieldspecs","ftype","fieldspec",
"flagexpr","flags","range","pnum","regtype","name","value", NULL
};
#endif

static const short yyr1[] = {     0,
    20,    21,    21,    22,    23,    22,    22,    22,    24,    24,
    24,    25,    25,    25,    26,    27,    27,    27,    28,    28,
    29,    29,    30,    31,    31,    31,    32,    -1,    33
};

static const short yyr2[] = {     0,
     1,     2,     0,     5,     0,     9,     1,     2,     3,     2,
     0,     1,     1,     1,     3,     3,     3,     1,     2,     0,
     4,     0,     1,     1,     1,     1,     1,     1,     1
};

static const short yydefact[] = {     0,
     0,     7,    23,     1,     0,     0,     8,     2,    24,    25,
    26,     0,     0,    27,     0,     0,    29,    22,     0,     0,
    20,     4,     0,     0,     5,     0,    18,     0,    19,    11,
    21,     0,     0,    14,    13,     0,     0,    22,    11,    12,
    16,    17,    11,     6,    20,    10,     9,    15,     0,     0,
     0
};

static const short yydefgoto[] = {    49,
     4,     5,    30,    37,    38,    39,    29,    25,    21,     6,
    40,    15,    18
};

static const short yypact[] = {     0,
    -9,-32768,-32768,-32768,     0,    12,-32768,-32768,-32768,-32768,
-32768,     3,     3,-32768,     9,     9,-32768,    -8,     8,    19,
    15,-32768,    10,    -6,-32768,    24,    20,    -6,-32768,     1,
-32768,    -6,    21,-32768,-32768,    18,    25,    -8,     1,-32768,
-32768,-32768,     1,-32768,    15,-32768,-32768,-32768,    35,    38,
-32768
};

static const short yypgoto[] = {-32768,
    34,-32768,-32768,   -13,-32768,     4,    -1,    -4,     5,-32768,
    36,    31,    29
};


#define	YYLAST		45


static const short yytable[] = {    -3,
     1,     7,    27,     9,    10,    11,    34,    35,    28,    20,
     2,     3,    14,    36,     9,    10,    11,    17,    22,    12,
     9,    10,    11,    34,    35,    46,    33,    23,    26,    47,
    41,    24,    31,    32,    50,    44,    42,    51,     8,    43,
    48,    13,    45,    16,    19
};

static const short yycheck[] = {     0,
     1,    11,     9,     3,     4,     5,     6,     7,    15,    18,
    11,    12,    10,    13,     3,     4,     5,     9,    11,     8,
     3,     4,     5,     6,     7,    39,    28,     9,    19,    43,
    32,    17,     9,    14,     0,    11,    16,     0,     5,    36,
    45,     6,    38,    13,    16
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
#line 311 "itbl-parse.y"
{
	    DBG (("line %d: entry pnum=%d type=%d name=%s value=x%x\n", 
	    	    insntbl_line, yyvsp[-4].num, yyvsp[-3].num, yyvsp[-2].str, yyvsp[-1].val));
	    itbl_add_reg (yyvsp[-4].num, yyvsp[-3].num, yyvsp[-2].str, yyvsp[-1].val);
	  ;
    break;}
case 5:
#line 317 "itbl-parse.y"
{
	    DBG (("line %d: entry pnum=%d type=INSN name=%s value=x%x",
	    	    insntbl_line, yyvsp[-5].num, yyvsp[-3].str, yyvsp[-2].val));
	    DBG ((" sbit=%d ebit=%d flags=0x%x\n", sbit, ebit, yyvsp[0].val));
	    insn=itbl_add_insn (yyvsp[-5].num, yyvsp[-3].str, yyvsp[-2].val, sbit, ebit, yyvsp[0].val);
	  ;
    break;}
case 12:
#line 336 "itbl-parse.y"
{
	    DBGL2 (("ftype\n"));
	    yyval.num = yyvsp[0].num;
	  ;
    break;}
case 13:
#line 341 "itbl-parse.y"
{
	    DBGL2 (("addr\n"));
	    yyval.num = ADDR;
	  ;
    break;}
case 14:
#line 346 "itbl-parse.y"
{
	    DBGL2 (("immed\n"));
	    yyval.num = IMMED;
	  ;
    break;}
case 15:
#line 354 "itbl-parse.y"
{
	    DBG (("line %d: field type=%d sbit=%d ebit=%d, flags=0x%x\n", 
	    	    insntbl_line, yyvsp[-2].num, sbit, ebit, yyvsp[0].val));
	    itbl_add_operand (insn, yyvsp[-2].num, sbit, ebit, yyvsp[0].val);
	  ;
    break;}
case 16:
#line 363 "itbl-parse.y"
{
	    yyval.val = yyvsp[-2].num | yyvsp[0].val;
	  ;
    break;}
case 17:
#line 367 "itbl-parse.y"
{
	    yyval.val = yyvsp[-1].val;
	  ;
    break;}
case 18:
#line 371 "itbl-parse.y"
{
	    yyval.val = yyvsp[0].num;
	  ;
    break;}
case 19:
#line 378 "itbl-parse.y"
{
	    DBGL2 (("flags=%d\n", yyvsp[0].val));
	    yyval.val = yyvsp[0].val;
	  ;
    break;}
case 20:
#line 383 "itbl-parse.y"
{
	    yyval.val = 0;
	  ;
    break;}
case 21:
#line 390 "itbl-parse.y"
{
	    DBGL2 (("range %d %d\n", yyvsp[-2].num, yyvsp[0].num));
	    sbit = yyvsp[-2].num;
	    ebit = yyvsp[0].num;
	  ;
    break;}
case 22:
#line 396 "itbl-parse.y"
{
	    sbit = 31;
	    ebit = 0;
	  ;
    break;}
case 23:
#line 404 "itbl-parse.y"
{
	    DBGL2 (("pnum=%d\n",yyvsp[0].num));
	    yyval.num = yyvsp[0].num;
	  ;
    break;}
case 24:
#line 412 "itbl-parse.y"
{
	    DBGL2 (("dreg\n"));
	    yyval.num = DREG;
	  ;
    break;}
case 25:
#line 417 "itbl-parse.y"
{
	    DBGL2 (("creg\n"));
	    yyval.num = CREG;
	  ;
    break;}
case 26:
#line 422 "itbl-parse.y"
{
	    DBGL2 (("greg\n"));
	    yyval.num = GREG;
	  ;
    break;}
case 27:
#line 430 "itbl-parse.y"
{
	    DBGL2 (("name=%s\n",yyvsp[0].str));
	    yyval.str = yyvsp[0].str; 
	  ;
    break;}
case 28:
#line 438 "itbl-parse.y"
{
	    DBGL2 (("num=%d\n",yyvsp[0].num));
	    yyval.num = yyvsp[0].num;
	  ;
    break;}
case 29:
#line 446 "itbl-parse.y"
{
	    DBGL2 (("val=x%x\n",yyvsp[0].num));
	    yyval.val = yyvsp[0].num;
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
#line 451 "itbl-parse.y"


static int
yyerror (msg)
     const char *msg;
{
  printf ("line %d: %s\n", insntbl_line, msg);
  return 0;
}
