
/*  A Bison parser, made from ./sysinfo.y with Bison version GNU Bison version 1.24
  */

#define YYBISON 1  /* Identify Bison output.  */

#define	COND	258
#define	REPEAT	259
#define	TYPE	260
#define	NAME	261
#define	NUMBER	262
#define	UNIT	263

#line 1 "./sysinfo.y"

#include <stdio.h>
#include <stdlib.h>

extern char *word;
extern char writecode;
extern int number;
extern int unit;
char nice_name[1000];
char *it;
int sofar;
int width;
int code;
char * repeat;
char *oldrepeat;
char *name;
int rdepth;
char *loop [] = {"","n","m","/*BAD*/"};
char *names[] = {" ","[n]","[n][m]"};
char *pnames[]= {"","*","**"};

#line 24 "./sysinfo.y"
typedef union {
 int i;
 char *s;
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

#ifndef YYDEBUG
#define YYDEBUG 1
#endif

#include <stdio.h>

#ifndef __cplusplus
#ifndef __STDC__
#define const
#endif
#endif



#define	YYFINAL		55
#define	YYFLAG		-32768
#define	YYNTBASE	11

#define YYTRANSLATE(x) ((unsigned)(x) <= 263 ? yytranslate[x] : 29)

static const char yytranslate[] = {     0,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     5,
     6,     2,     2,     2,     2,     2,     2,     2,     2,     2,
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
     2,     2,     2,     2,     2,     1,     2,     3,     4,     7,
     8,     9,    10
};

#if YYDEBUG != 0
static const short yyprhs[] = {     0,
     0,     1,     4,     7,     8,     9,    16,    19,    22,    25,
    26,    27,    34,    35,    42,    43,    54,    56,    57,    61,
    64,    68,    69,    70,    74,    75
};

static const short yyrhs[] = {    -1,
    12,    13,     0,    14,    13,     0,     0,     0,     5,     8,
     9,    15,    16,     6,     0,    21,    16,     0,    19,    16,
     0,    17,    16,     0,     0,     0,     5,     4,     8,    18,
    16,     6,     0,     0,     5,     3,     8,    20,    16,     6,
     0,     0,     5,    24,     5,    23,    25,     6,    26,    22,
    27,     6,     0,     7,     0,     0,     5,     8,     6,     0,
     9,    10,     0,     5,     8,     6,     0,     0,     0,     5,
    28,     6,     0,     0,    28,     5,     8,     8,     6,     0
};

#endif

#if YYDEBUG != 0
static const short yyrline[] = { 0,
    38,    59,    75,    76,    79,   130,   150,   152,   153,   154,
   157,   186,   204,   217,   230,   233,   338,   340,   343,   348,
   354,   356,   359,   360,   362,   363
};

static const char * const yytname[] = {   "$","error","$undefined.","COND","REPEAT",
"'('","')'","TYPE","NAME","NUMBER","UNIT","top","@1","it_list","it","@2","it_field_list",
"repeat_it_field","@3","cond_it_field","@4","it_field","@5","attr_type","attr_desc",
"attr_size","attr_id","enums","enum_list",""
};
#endif

static const short yyr1[] = {     0,
    12,    11,    13,    13,    15,    14,    16,    16,    16,    16,
    18,    17,    20,    19,    22,    21,    23,    23,    24,    25,
    26,    26,    27,    27,    28,    28
};

static const short yyr2[] = {     0,
     0,     2,     2,     0,     0,     6,     2,     2,     2,     0,
     0,     6,     0,     6,     0,    10,     1,     0,     3,     2,
     3,     0,     0,     3,     0,     5
};

static const short yydefact[] = {     1,
     4,     0,     2,     4,     0,     3,     5,    10,     0,     0,
    10,    10,    10,     0,     0,     0,     0,     6,     9,     8,
     7,    13,    11,     0,    18,    10,    10,    19,    17,     0,
     0,     0,     0,     0,    14,    12,    20,    22,     0,    15,
     0,    23,    21,    25,     0,     0,    16,     0,    24,     0,
     0,    26,     0,     0,     0
};

static const short yydefgoto[] = {    53,
     1,     3,     4,     8,    10,    11,    27,    12,    26,    13,
    42,    30,    17,    34,    40,    45,    46
};

static const short yypact[] = {-32768,
     3,     2,-32768,     3,     4,-32768,-32768,     6,     0,     8,
     6,     6,     6,     9,    10,    11,     7,-32768,-32768,-32768,
-32768,-32768,-32768,    14,    15,     6,     6,-32768,-32768,    12,
    17,    18,    -1,    19,-32768,-32768,-32768,    21,    20,-32768,
    23,    22,-32768,-32768,    24,     1,-32768,    25,-32768,    26,
    29,-32768,    31,    32,-32768
};

static const short yypgoto[] = {-32768,
-32768,    33,-32768,-32768,   -11,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768
};


#define	YYLAST		37


static const short yytable[] = {    19,
    20,    21,    14,    15,    16,    48,    49,     2,    37,     5,
     9,    25,     7,    18,    31,    32,    22,    23,    24,    28,
    33,    29,    35,    36,    38,    39,    44,    41,    43,    47,
    54,    55,    50,    51,    52,     0,     6
};

static const short yycheck[] = {    11,
    12,    13,     3,     4,     5,     5,     6,     5,    10,     8,
     5,     5,     9,     6,    26,    27,     8,     8,     8,     6,
     9,     7,     6,     6,     6,     5,     5,     8,     6,     6,
     0,     0,     8,     8,     6,    -1,     4
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

case 1:
#line 38 "./sysinfo.y"
{
  switch (writecode)
    {
    case 'i':
      printf("#ifdef SYSROFF_SWAP_IN\n");
      break; 
    case 'p':
      printf("#ifdef SYSROFF_p\n");
      break; 
    case 'd':
      break;
    case 'g':
      printf("#ifdef SYSROFF_SWAP_OUT\n");
      break;
    case 'c':
      printf("#ifdef SYSROFF_PRINT\n");
      printf("#include <stdio.h>\n");
      printf("#include <stdlib.h>\n");
      break;
    }
 ;
    break;}
case 2:
#line 59 "./sysinfo.y"
{
  switch (writecode) {
  case 'i':
  case 'p':
  case 'g':
  case 'c':
    printf("#endif\n");
    break; 
  case 'd':
    break;
  }
;
    break;}
case 5:
#line 81 "./sysinfo.y"
{
	it = yyvsp[-1].s; code = yyvsp[0].i;
	switch (writecode) 
	  {
	  case 'd':
	    printf("\n\n\n#define IT_%s_CODE 0x%x\n", it,code);
	    printf("struct IT_%s { \n", it);
	    break;
	  case 'i':
	    printf("void sysroff_swap_%s_in(ptr)\n",yyvsp[-1].s);
	    printf("struct IT_%s *ptr;\n", it);
	    printf("{\n");
	    printf("char raw[255];\n");
	    printf("\tint idx = 0 ;\n");
	    printf("\tint size;\n");
	    printf("memset(raw,0,255);\n");	
	    printf("memset(ptr,0,sizeof(*ptr));\n");
	    printf("size = fillup(raw);\n");
	    break;
	  case 'g':
	    printf("void sysroff_swap_%s_out(file,ptr)\n",yyvsp[-1].s);
	    printf("FILE * file;\n");
	    printf("struct IT_%s *ptr;\n", it);
	    printf("{\n");
	    printf("\tchar raw[255];\n");
	    printf("\tint idx = 16 ;\n");
	    printf("\tmemset (raw, 0, 255);\n");
	    printf("\tcode = IT_%s_CODE;\n", it);
	    break;
	  case 'o':
	    printf("void sysroff_swap_%s_out(abfd,ptr)\n",yyvsp[-1].s);
	    printf("bfd * abfd;\n");
	    printf("struct IT_%s *ptr;\n",it);
	    printf("{\n");
	    printf("int idx = 0 ;\n");
	    break;
	  case 'c':
	    printf("void sysroff_print_%s_out(ptr)\n",yyvsp[-1].s);
	    printf("struct IT_%s *ptr;\n", it);
	    printf("{\n");
	    printf("itheader(\"%s\", IT_%s_CODE);\n",yyvsp[-1].s,yyvsp[-1].s);
	    break;

	  case 't':
	    break;
	  }

      ;
    break;}
case 6:
#line 131 "./sysinfo.y"
{
  switch (writecode) {
  case 'd': 
    printf("};\n");
    break;
  case 'g':
    printf("\tchecksum(file,raw, idx, IT_%s_CODE);\n", it);
    
  case 'i':

  case 'o':
  case 'c':
    printf("}\n");
  }
;
    break;}
case 11:
#line 158 "./sysinfo.y"
{
	  rdepth++;
	  switch (writecode) 
	    {
	    case 'c':
	      if (rdepth==1)
	      printf("\tprintf(\"repeat %%d\\n\", %s);\n",yyvsp[0].s);
	      if (rdepth==2)
	      printf("\tprintf(\"repeat %%d\\n\", %s[n]);\n",yyvsp[0].s);
	    case 'i':
	    case 'g':
	    case 'o':

	      if (rdepth==1) 
		{
	      printf("\t{ int n; for (n = 0; n < %s; n++) {\n",    yyvsp[0].s);
	    }
	      if (rdepth == 2) {
	      printf("\t{ int m; for (m = 0; m < %s[n]; m++) {\n",    yyvsp[0].s);
	    }		

	      break;
	    }

	  oldrepeat = repeat;
         repeat = yyvsp[0].s;
	;
    break;}
case 12:
#line 188 "./sysinfo.y"
{
	  repeat = oldrepeat;
	  oldrepeat =0;
	  rdepth--;
	  switch (writecode)
	    {
	    case 'i':
	    case 'g':
	    case 'o':
	    case 'c':
	  printf("\t}}\n");
	}
	;
    break;}
case 13:
#line 205 "./sysinfo.y"
{
	  switch (writecode) 
	    {
	    case 'i':
	    case 'g':
	    case 'o':
	    case 'c':
	      printf("\tif (%s) {\n", yyvsp[0].s);
	      break;
	    }
	;
    break;}
case 14:
#line 218 "./sysinfo.y"
{
	  switch (writecode)
	    {
	    case 'i':
	    case 'g':
	    case 'o':
	    case 'c':
	  printf("\t}\n");
	}
	;
    break;}
case 15:
#line 232 "./sysinfo.y"
{name = yyvsp[0].s; ;
    break;}
case 16:
#line 234 "./sysinfo.y"
{
	  char *desc = yyvsp[-8].s;
	  char *type = yyvsp[-6].s;
	  int size = yyvsp[-5].i;
	  char *id = yyvsp[-3].s;
char *p = names[rdepth];
char *ptr = pnames[rdepth];
	  switch (writecode) 
	    {
	    case 'g':
	      if (size % 8) 
		{
		  
		  printf("\twriteBITS(ptr->%s%s,raw,&idx,%d);\n",
			 id,
			 names[rdepth], size);

		}
	      else {
		printf("\twrite%s(ptr->%s%s,raw,&idx,%d,file);\n",
		       type,
		       id,
		       names[rdepth],size/8);
		}
	      break;	      
	    case 'i':
	      {

		if (rdepth >= 1)

		  {
		    printf("if (!ptr->%s) ptr->%s = (%s*)xcalloc(%s, sizeof(ptr->%s[0]));\n", 
			   id, 
			   id,
			   type,
			   repeat,
			   id);
		  }

		if (rdepth == 2)
		  {
		    printf("if (!ptr->%s[n]) ptr->%s[n] = (%s**)xcalloc(%s[n], sizeof(ptr->%s[n][0]));\n", 
			   id, 
			   id,
			   type,
			   repeat,
			   id);
		  }

	      }

	      if (size % 8) 
		{
		  printf("\tptr->%s%s = getBITS(raw,&idx, %d,size);\n",
			 id,
			 names[rdepth], 
			 size);
		}
	      else {
		printf("\tptr->%s%s = get%s(raw,&idx, %d,size);\n",
		       id,
		       names[rdepth],
		       type,
		       size/8);
		}
	      break;
	    case 'o':
	      printf("\tput%s(raw,%d,%d,&idx,ptr->%s%s);\n", type,size/8,size%8,id,names[rdepth]);
	      break;
	    case 'd':
	      if (repeat) 
		printf("\t/* repeat %s */\n", repeat);

		  if (type[0] == 'I') {
		  printf("\tint %s%s; \t/* %s */\n",ptr,id, desc);
		}
		  else if (type[0] =='C') {
		  printf("\tchar %s*%s;\t /* %s */\n",ptr,id, desc);
		}
	      else {
		printf("\tbarray %s%s;\t /* %s */\n",ptr,id, desc);
	      }
		  break;
		case 'c':
	      printf("tabout();\n");
		  printf("\tprintf(\"/*%-30s*/ ptr->%s = \");\n", desc, id);

		  if (type[0] == 'I')
		  printf("\tprintf(\"%%d\\n\",ptr->%s%s);\n", id,p);
		  else   if (type[0] == 'C')
		  printf("\tprintf(\"%%s\\n\",ptr->%s%s);\n", id,p);

		  else   if (type[0] == 'B') 
		    {
		  printf("\tpbarray(&ptr->%s%s);\n", id,p);
		}
	      else abort();
		  break;
		}
	;
    break;}
case 17:
#line 339 "./sysinfo.y"
{ yyval.s = yyvsp[0].s; ;
    break;}
case 18:
#line 340 "./sysinfo.y"
{ yyval.s = "INT";;
    break;}
case 19:
#line 345 "./sysinfo.y"
{ yyval.s = yyvsp[-1].s; ;
    break;}
case 20:
#line 350 "./sysinfo.y"
{ yyval.i = yyvsp[-1].i * yyvsp[0].i; ;
    break;}
case 21:
#line 355 "./sysinfo.y"
{ yyval.s = yyvsp[-1].s; ;
    break;}
case 22:
#line 356 "./sysinfo.y"
{ yyval.s = "dummy";;
    break;}
case 26:
#line 364 "./sysinfo.y"
{ 
	  switch (writecode) 
	    {
	    case 'd':
	      printf("#define %s %s\n", yyvsp[-2].s,yyvsp[-1].s);
	      break;
	    case 'c':
		printf("if (ptr->%s%s == %s) { tabout(); printf(\"%s\\n\");}\n", name, names[rdepth],yyvsp[-1].s,yyvsp[-2].s);
	    }
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
#line 379 "./sysinfo.y"

/* four modes

   -d write structure defintions for sysroff in host format
   -i write functions to swap into sysroff format in
   -o write functions to swap into sysroff format out
   -c write code to print info in human form */

int yydebug;
char writecode;

int 
main(ac,av)
int ac;
char **av;
{
  yydebug=0;
  if (ac > 1)
    writecode = av[1][1];
if (writecode == 'd')
  {
    printf("typedef struct { unsigned char *data; int len; } barray; \n");
    printf("typedef  int INT;\n");
    printf("typedef  char * CHARS;\n");

  }
  yyparse();
return 0;
}

int yyerror(s)
     char *s;
{
  fprintf(stderr, "%s\n" , s);
}
