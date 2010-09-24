/*    perly.y
 *
 *    Copyright (c) 1991-2002, 2003, 2004, 2005, 2006 Larry Wall
 *    Copyright (c) 2007, 2008 by Larry Wall and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 * 'I see,' laughed Strider.  'I look foul and feel fair.  Is that it?
 *  All that is gold does not glitter, not all those who wander are lost.'
 *
 *     [p.171 of _The Lord of the Rings_, I/x: "Strider"]
 */

/*
 * This file holds the grammar for the Perl language. If edited, you need
 * to run regen_perly.pl, which re-creates the files perly.h, perly.tab
 * and perly.act which are derived from this.
 *
 * Note that these derived files are included and compiled twice; once
 * from perly.c, and once from madly.c. The second time, a number of MAD
 * macros are defined, which compile in extra code that allows the parse
 * tree to be accurately dumped. In particular:
 *
 * MAD            defined if compiling madly.c
 * DO_MAD(A)      expands to A  under madly.c, to null otherwise
 * IF_MAD(a,b)    expands to A under madly.c, to B otherwise
 * TOKEN_GETMAD() expands to token_getmad() under madly.c, to null otherwise
 * TOKEN_FREE()   similarly
 * OP_GETMAD()    similarly
 * IVAL(i)        expands to (i)->tk_lval.ival or (i)
 * PVAL(p)        expands to (p)->tk_lval.pval or (p)
 *
 * The main job of of this grammar is to call the various newFOO()
 * functions in op.c to build a syntax tree of OP structs.
 * It relies on the lexer in toke.c to do the tokenizing.
 *
 * Note: due to the way that the cleanup code works WRT to freeing ops on
 * the parse stack, it is dangerous to assign to the $n variables within
 * an action.
 */

/*  Make the parser re-entrant. */

%pure_parser

/* FIXME for MAD - is the new mintro on while and until important?  */

%start prog

%union {
    I32	ival; /* __DEFAULT__ (marker for regen_perly.pl;
				must always be 1st union member) */
    char *pval;
    OP *opval;
    GV *gvval;
#ifdef PERL_IN_MADLY_C
    TOKEN* p_tkval;
    TOKEN* i_tkval;
#else
    char *p_tkval;
    I32	i_tkval;
#endif
#ifdef PERL_MAD
    TOKEN* tkval;
#endif
}

%token <i_tkval> '{' '}' '[' ']' '-' '+' '$' '@' '%' '*' '&' ';'

%token <opval> WORD METHOD FUNCMETH THING PMFUNC PRIVATEREF
%token <opval> FUNC0SUB UNIOPSUB LSTOPSUB
%token <opval> PLUGEXPR PLUGSTMT
%token <p_tkval> LABEL
%token <i_tkval> FORMAT SUB ANONSUB PACKAGE USE
%token <i_tkval> WHILE UNTIL IF UNLESS ELSE ELSIF CONTINUE FOR
%token <i_tkval> GIVEN WHEN DEFAULT
%token <i_tkval> LOOPEX DOTDOT YADAYADA
%token <i_tkval> FUNC0 FUNC1 FUNC UNIOP LSTOP
%token <i_tkval> RELOP EQOP MULOP ADDOP
%token <i_tkval> DOLSHARP DO HASHBRACK NOAMP
%token <i_tkval> LOCAL MY MYSUB REQUIRE
%token <i_tkval> COLONATTR

%type <ival> prog progstart remember mremember
%type <ival>  startsub startanonsub startformsub
/* FIXME for MAD - are these two ival? */
%type <ival> mydefsv mintro

%type <opval> decl format subrout mysubrout package use peg

%type <opval> block mblock lineseq line loop cond else
%type <opval> expr term subscripted scalar ary hsh arylen star amper sideff
%type <opval> argexpr nexpr texpr iexpr mexpr mnexpr miexpr
%type <opval> listexpr listexprcom indirob listop method
%type <opval> formname subname proto subbody cont my_scalar
%type <opval> subattrlist myattrlist myattrterm myterm
%type <opval> termbinop termunop anonymous termdo
%type <opval> switch case
%type <p_tkval> label

%nonassoc <i_tkval> PREC_LOW
%nonassoc LOOPEX

%left <i_tkval> OROP DOROP
%left <i_tkval> ANDOP
%right <i_tkval> NOTOP
%nonassoc LSTOP LSTOPSUB
%left <i_tkval> ','
%right <i_tkval> ASSIGNOP
%right <i_tkval> '?' ':'
%nonassoc DOTDOT YADAYADA
%left <i_tkval> OROR DORDOR
%left <i_tkval> ANDAND
%left <i_tkval> BITOROP
%left <i_tkval> BITANDOP
%nonassoc EQOP
%nonassoc RELOP
%nonassoc UNIOP UNIOPSUB
%nonassoc REQUIRE
%left <i_tkval> SHIFTOP
%left ADDOP
%left MULOP
%left <i_tkval> MATCHOP
%right <i_tkval> '!' '~' UMINUS REFGEN
%right <i_tkval> POWOP
%nonassoc <i_tkval> PREINC PREDEC POSTINC POSTDEC
%left <i_tkval> ARROW
%nonassoc <i_tkval> ')'
%left <i_tkval> '('
%left '[' '{'

%token <i_tkval> PEG

%% /* RULES */

/* The whole program */
prog	:	progstart
	/*CONTINUED*/	lineseq
			{ $$ = $1; newPROG(block_end($1,$2)); }
	;

/* An ordinary block */
block	:	'{' remember lineseq '}'
			{ if (PL_parser->copline > (line_t)IVAL($1))
			      PL_parser->copline = (line_t)IVAL($1);
			  $$ = block_end($2, $3);
			  TOKEN_GETMAD($1,$$,'{');
			  TOKEN_GETMAD($4,$$,'}');
			}
	;

remember:	/* NULL */	/* start a full lexical scope */
			{ $$ = block_start(TRUE); }
	;

mydefsv:	/* NULL */	/* lexicalize $_ */
			{ $$ = (I32) Perl_allocmy(aTHX_ STR_WITH_LEN("$_"), 0); }
	;

progstart:
		{
		    PL_parser->expect = XSTATE; $$ = block_start(TRUE);
		}
	;


mblock	:	'{' mremember lineseq '}'
			{ if (PL_parser->copline > (line_t)IVAL($1))
			      PL_parser->copline = (line_t)IVAL($1);
			  $$ = block_end($2, $3);
			  TOKEN_GETMAD($1,$$,'{');
			  TOKEN_GETMAD($4,$$,'}');
			}
	;

mremember:	/* NULL */	/* start a partial lexical scope */
			{ $$ = block_start(FALSE); }
	;

/* A collection of "lines" in the program */
lineseq	:	/* NULL */
			{ $$ = (OP*)NULL; }
	|	lineseq decl
			{
			$$ = IF_MAD(
				append_list(OP_LINESEQ,
			    	    (LISTOP*)$1, (LISTOP*)$2),
				$1);
			}
	|	lineseq line
			{   $$ = append_list(OP_LINESEQ,
				(LISTOP*)$1, (LISTOP*)$2);
			    PL_pad_reset_pending = TRUE;
			    if ($1 && $2)
				PL_hints |= HINT_BLOCK_SCOPE;
			}
	;

/* A "line" in the program */
line	:	label cond
			{ $$ = newSTATEOP(0, PVAL($1), $2);
			  TOKEN_GETMAD($1,((LISTOP*)$$)->op_first,'L'); }
	|	loop	/* loops add their own labels */
	|	switch  /* ... and so do switches */
			{ $$ = $1; }
	|	label case
			{ $$ = newSTATEOP(0, PVAL($1), $2); }
	|	label ';'
			{
			  if (PVAL($1)) {
			      $$ = newSTATEOP(0, PVAL($1), newOP(OP_NULL, 0));
			      TOKEN_GETMAD($1,$$,'L');
			      TOKEN_GETMAD($2,((LISTOP*)$$)->op_first,';');
			  }
			  else {
			      $$ = IF_MAD(
					newOP(OP_NULL, 0),
					(OP*)NULL);
                              PL_parser->copline = NOLINE;
			      TOKEN_FREE($1);
			      TOKEN_GETMAD($2,$$,';');
			  }
			  PL_parser->expect = XSTATE;
			}
	|	label sideff ';'
			{
			  $$ = newSTATEOP(0, PVAL($1), $2);
			  PL_parser->expect = XSTATE;
			  DO_MAD({
			      /* sideff might already have a nexstate */
			      OP* op = ((LISTOP*)$$)->op_first;
			      if (op) {
				  while (op->op_sibling &&
				     op->op_sibling->op_type == OP_NEXTSTATE)
					op = op->op_sibling;
				  token_getmad($1,op,'L');
				  token_getmad($3,op,';');
			      }
			  })
			}
	|	label PLUGSTMT
			{ $$ = newSTATEOP(0, PVAL($1), $2); }
	;

/* An expression which may have a side-effect */
sideff	:	error
			{ $$ = (OP*)NULL; }
	|	expr
			{ $$ = $1; }
	|	expr IF expr
			{ $$ = newLOGOP(OP_AND, 0, $3, $1);
			  TOKEN_GETMAD($2,$$,'i');
			}
	|	expr UNLESS expr
			{ $$ = newLOGOP(OP_OR, 0, $3, $1);
			  TOKEN_GETMAD($2,$$,'i');
			}
	|	expr WHILE expr
			{ $$ = newLOOPOP(OPf_PARENS, 1, scalar($3), $1);
			  TOKEN_GETMAD($2,$$,'w');
			}
	|	expr UNTIL iexpr
			{ $$ = newLOOPOP(OPf_PARENS, 1, $3, $1);
			  TOKEN_GETMAD($2,$$,'w');
			}
	|	expr FOR expr
			{ $$ = newFOROP(0, NULL, (line_t)IVAL($2),
					(OP*)NULL, $3, $1, (OP*)NULL);
			  TOKEN_GETMAD($2,((LISTOP*)$$)->op_first->op_sibling,'w');
			}
	|	expr WHEN expr
			{ $$ = newWHENOP($3, scope($1)); }
	;

/* else and elsif blocks */
else	:	/* NULL */
			{ $$ = (OP*)NULL; }
	|	ELSE mblock
			{ ($2)->op_flags |= OPf_PARENS; $$ = scope($2);
			  TOKEN_GETMAD($1,$$,'o');
			}
	|	ELSIF '(' mexpr ')' mblock else
			{ PL_parser->copline = (line_t)IVAL($1);
			    $$ = newCONDOP(0, newSTATEOP(OPf_SPECIAL,NULL,$3), scope($5), $6);
			    PL_hints |= HINT_BLOCK_SCOPE;
			  TOKEN_GETMAD($1,$$,'I');
			  TOKEN_GETMAD($2,$$,'(');
			  TOKEN_GETMAD($4,$$,')');
			}
	;

/* Real conditional expressions */
cond	:	IF '(' remember mexpr ')' mblock else
			{ PL_parser->copline = (line_t)IVAL($1);
			    $$ = block_end($3,
				   newCONDOP(0, $4, scope($6), $7));
			  TOKEN_GETMAD($1,$$,'I');
			  TOKEN_GETMAD($2,$$,'(');
			  TOKEN_GETMAD($5,$$,')');
			}
	|	UNLESS '(' remember miexpr ')' mblock else
			{ PL_parser->copline = (line_t)IVAL($1);
			    $$ = block_end($3,
				   newCONDOP(0, $4, scope($6), $7));
			  TOKEN_GETMAD($1,$$,'I');
			  TOKEN_GETMAD($2,$$,'(');
			  TOKEN_GETMAD($5,$$,')');
			}
	;

/* Cases for a switch statement */
case	:	WHEN '(' remember mexpr ')' mblock
	{ $$ = block_end($3,
		newWHENOP($4, scope($6))); }
	|	DEFAULT block
	{ $$ = newWHENOP(0, scope($2)); }
	;

/* Continue blocks */
cont	:	/* NULL */
			{ $$ = (OP*)NULL; }
	|	CONTINUE block
			{ $$ = scope($2);
			  TOKEN_GETMAD($1,$$,'o');
			}
	;

/* Loops: while, until, for, and a bare block */
loop	:	label WHILE '(' remember texpr ')' mintro mblock cont
			{ OP *innerop;
			  PL_parser->copline = (line_t)IVAL($2);
			    $$ = block_end($4,
				   newSTATEOP(0, PVAL($1),
				     innerop = newWHILEOP(0, 1, (LOOP*)(OP*)NULL,
						IVAL($2), $5, $8, $9, $7)));
			  TOKEN_GETMAD($1,innerop,'L');
			  TOKEN_GETMAD($2,innerop,'W');
			  TOKEN_GETMAD($3,innerop,'(');
			  TOKEN_GETMAD($6,innerop,')');
			}

	|	label UNTIL '(' remember iexpr ')' mintro mblock cont
			{ OP *innerop;
			  PL_parser->copline = (line_t)IVAL($2);
			    $$ = block_end($4,
				   newSTATEOP(0, PVAL($1),
				     innerop = newWHILEOP(0, 1, (LOOP*)(OP*)NULL,
						IVAL($2), $5, $8, $9, $7)));
			  TOKEN_GETMAD($1,innerop,'L');
			  TOKEN_GETMAD($2,innerop,'W');
			  TOKEN_GETMAD($3,innerop,'(');
			  TOKEN_GETMAD($6,innerop,')');
			}
	|	label FOR MY remember my_scalar '(' mexpr ')' mblock cont
			{ OP *innerop;
			  $$ = block_end($4,
			     innerop = newFOROP(0, PVAL($1), (line_t)IVAL($2),
					    $5, $7, $9, $10));
			  TOKEN_GETMAD($1,((LISTOP*)innerop)->op_first,'L');
			  TOKEN_GETMAD($2,((LISTOP*)innerop)->op_first->op_sibling,'W');
			  TOKEN_GETMAD($3,((LISTOP*)innerop)->op_first->op_sibling,'d');
			  TOKEN_GETMAD($6,((LISTOP*)innerop)->op_first->op_sibling,'(');
			  TOKEN_GETMAD($8,((LISTOP*)innerop)->op_first->op_sibling,')');
			}
	|	label FOR scalar '(' remember mexpr ')' mblock cont
			{ OP *innerop;
			  $$ = block_end($5,
			     innerop = newFOROP(0, PVAL($1), (line_t)IVAL($2),
				    mod($3, OP_ENTERLOOP), $6, $8, $9));
			  TOKEN_GETMAD($1,((LISTOP*)innerop)->op_first,'L');
			  TOKEN_GETMAD($2,((LISTOP*)innerop)->op_first->op_sibling,'W');
			  TOKEN_GETMAD($4,((LISTOP*)innerop)->op_first->op_sibling,'(');
			  TOKEN_GETMAD($7,((LISTOP*)innerop)->op_first->op_sibling,')');
			}
	|	label FOR '(' remember mexpr ')' mblock cont
			{ OP *innerop;
			  $$ = block_end($4,
			     innerop = newFOROP(0, PVAL($1), (line_t)IVAL($2),
						    (OP*)NULL, $5, $7, $8));
			  TOKEN_GETMAD($1,((LISTOP*)innerop)->op_first,'L');
			  TOKEN_GETMAD($2,((LISTOP*)innerop)->op_first->op_sibling,'W');
			  TOKEN_GETMAD($3,((LISTOP*)innerop)->op_first->op_sibling,'(');
			  TOKEN_GETMAD($6,((LISTOP*)innerop)->op_first->op_sibling,')');
			}
	|	label FOR '(' remember mnexpr ';' texpr ';' mintro mnexpr ')'
	    	    mblock
			/* basically fake up an initialize-while lineseq */
			{ OP *forop;
			  PL_parser->copline = (line_t)IVAL($2);
			  forop = newSTATEOP(0, PVAL($1),
					    newWHILEOP(0, 1, (LOOP*)(OP*)NULL,
						IVAL($2), scalar($7),
						$12, $10, $9));
#ifdef MAD
			  forop = newUNOP(OP_NULL, 0, append_elem(OP_LINESEQ,
				newSTATEOP(0,
					   CopLABEL_alloc(($1)->tk_lval.pval),
					   ($5 ? $5 : newOP(OP_NULL, 0)) ),
				forop));

			  token_getmad($2,forop,'3');
			  token_getmad($3,forop,'(');
			  token_getmad($6,forop,'1');
			  token_getmad($8,forop,'2');
			  token_getmad($11,forop,')');
			  token_getmad($1,forop,'L');
#else
			  if ($5) {
				forop = append_elem(OP_LINESEQ,
                                        newSTATEOP(0, CopLABEL_alloc($1), $5),
					forop);
			  }


#endif
			  $$ = block_end($4, forop); }
	|	label block cont  /* a block is a loop that happens once */
			{ $$ = newSTATEOP(0, PVAL($1),
				 newWHILEOP(0, 1, (LOOP*)(OP*)NULL,
					    NOLINE, (OP*)NULL, $2, $3, 0));
			  TOKEN_GETMAD($1,((LISTOP*)$$)->op_first,'L'); }
	;

/* Switch blocks */
switch	:	label GIVEN '(' remember mydefsv mexpr ')' mblock
			{ PL_parser->copline = (line_t) IVAL($2);
			    $$ = block_end($4,
				newSTATEOP(0, PVAL($1),
				    newGIVENOP($6, scope($8),
					(PADOFFSET) $5) )); }
	;

/* determine whether there are any new my declarations */
mintro	:	/* NULL */
			{ $$ = (PL_min_intro_pending &&
			    PL_max_intro_pending >=  PL_min_intro_pending);
			  intro_my(); }

/* Normal expression */
nexpr	:	/* NULL */
			{ $$ = (OP*)NULL; }
	|	sideff
	;

/* Boolean expression */
texpr	:	/* NULL means true */
			{ YYSTYPE tmplval;
			  (void)scan_num("1", &tmplval);
			  $$ = tmplval.opval; }
	|	expr
	;

/* Inverted boolean expression */
iexpr	:	expr
			{ $$ = invert(scalar($1)); }
	;

/* Expression with its own lexical scope */
mexpr	:	expr
			{ $$ = $1; intro_my(); }
	;

mnexpr	:	nexpr
			{ $$ = $1; intro_my(); }
	;

miexpr	:	iexpr
			{ $$ = $1; intro_my(); }
	;

/* Optional "MAIN:"-style loop labels */
label	:	/* empty */
			{
#ifdef MAD
			  YYSTYPE tmplval;
			  tmplval.pval = NULL;
			  $$ = newTOKEN(OP_NULL, tmplval, 0);
#else
			  $$ = NULL;
#endif
			}
	|	LABEL
	;

/* Some kind of declaration - just hang on peg in the parse tree */
decl	:	format
			{ $$ = $1; }
	|	subrout
			{ $$ = $1; }
	|	mysubrout
			{ $$ = $1; }
	|	package
			{ $$ = $1; }
	|	use
			{ $$ = $1; }

    /* these two are only used by MAD */

	|	peg
			{ $$ = $1; }
	;

peg	:	PEG
			{ $$ = newOP(OP_NULL,0);
			  TOKEN_GETMAD($1,$$,'p');
			}
	;

format	:	FORMAT startformsub formname block
			{
			  CV *fmtcv = PL_compcv;
			  SvREFCNT_inc_simple_void(PL_compcv);
#ifdef MAD
			  $$ = newFORM($2, $3, $4);
			  prepend_madprops($1->tk_mad, $$, 'F');
			  $1->tk_mad = 0;
			  token_free($1);
#else
			  newFORM($2, $3, $4);
			  $$ = (OP*)NULL;
#endif
			  if (CvOUTSIDE(fmtcv) && !CvUNIQUE(CvOUTSIDE(fmtcv))) {
			    SvREFCNT_inc_simple_void(fmtcv);
			    pad_add_anon((SV*)fmtcv, OP_NULL);
			  }
			}
	;

formname:	WORD		{ $$ = $1; }
	|	/* NULL */	{ $$ = (OP*)NULL; }
	;

/* Unimplemented "my sub foo { }" */
mysubrout:	MYSUB startsub subname proto subattrlist subbody
			{ SvREFCNT_inc_simple_void(PL_compcv);
#ifdef MAD
			  $$ = newMYSUB($2, $3, $4, $5, $6);
			  token_getmad($1,$$,'d');
#else
			  newMYSUB($2, $3, $4, $5, $6);
			  $$ = (OP*)NULL;
#endif
			}
	;

/* Subroutine definition */
subrout	:	SUB startsub subname proto subattrlist subbody
			{ SvREFCNT_inc_simple_void(PL_compcv);
#ifdef MAD
			  {
			      OP* o = newSVOP(OP_ANONCODE, 0,
				(SV*)newATTRSUB($2, $3, $4, $5, $6));
			      $$ = newOP(OP_NULL,0);
			      op_getmad(o,$$,'&');
			      op_getmad($3,$$,'n');
			      op_getmad($4,$$,'s');
			      op_getmad($5,$$,'a');
			      token_getmad($1,$$,'d');
			      append_madprops($6->op_madprop, $$, 0);
			      $6->op_madprop = 0;
			    }
#else
			  newATTRSUB($2, $3, $4, $5, $6);
			  $$ = (OP*)NULL;
#endif
			}
	;

startsub:	/* NULL */	/* start a regular subroutine scope */
			{ $$ = start_subparse(FALSE, 0);
			    SAVEFREESV(PL_compcv); }

	;

startanonsub:	/* NULL */	/* start an anonymous subroutine scope */
			{ $$ = start_subparse(FALSE, CVf_ANON);
			    SAVEFREESV(PL_compcv); }
	;

startformsub:	/* NULL */	/* start a format subroutine scope */
			{ $$ = start_subparse(TRUE, 0);
			    SAVEFREESV(PL_compcv); }
	;

/* Name of a subroutine - must be a bareword, could be special */
subname	:	WORD	{ const char *const name = SvPV_nolen_const(((SVOP*)$1)->op_sv);
			  if (strEQ(name, "BEGIN") || strEQ(name, "END")
			      || strEQ(name, "INIT") || strEQ(name, "CHECK")
			      || strEQ(name, "UNITCHECK"))
			      CvSPECIAL_on(PL_compcv);
			  $$ = $1; }
	;

/* Subroutine prototype */
proto	:	/* NULL */
			{ $$ = (OP*)NULL; }
	|	THING
	;

/* Optional list of subroutine attributes */
subattrlist:	/* NULL */
			{ $$ = (OP*)NULL; }
	|	COLONATTR THING
			{ $$ = $2;
			  TOKEN_GETMAD($1,$$,':');
			}
	|	COLONATTR
			{ $$ = IF_MAD(
				    newOP(OP_NULL, 0),
				    (OP*)NULL
				);
			  TOKEN_GETMAD($1,$$,':');
			}
	;

/* List of attributes for a "my" variable declaration */
myattrlist:	COLONATTR THING
			{ $$ = $2;
			  TOKEN_GETMAD($1,$$,':');
			}
	|	COLONATTR
			{ $$ = IF_MAD(
				    newOP(OP_NULL, 0),
				    (OP*)NULL
				);
			  TOKEN_GETMAD($1,$$,':');
			}
	;

/* Subroutine body - either null or a block */
subbody	:	block	{ $$ = $1; }
	|	';'	{ $$ = IF_MAD(
				    newOP(OP_NULL,0),
				    (OP*)NULL
				);
			  PL_parser->expect = XSTATE;
			  TOKEN_GETMAD($1,$$,';');
			}
	;

package :	PACKAGE WORD WORD ';'
			{
#ifdef MAD
			  $$ = package($3);
			  token_getmad($1,$$,'o');
			  if ($2)
			      package_version($2);
			  token_getmad($4,$$,';');
#else
			  package($3);
			  if ($2)
			      package_version($2);
			  $$ = (OP*)NULL;
#endif
			}
	;

use	:	USE startsub
			{ CvSPECIAL_on(PL_compcv); /* It's a BEGIN {} */ }
		    WORD WORD listexpr ';'
			{ SvREFCNT_inc_simple_void(PL_compcv);
#ifdef MAD
			  $$ = utilize(IVAL($1), $2, $4, $5, $6);
			  token_getmad($1,$$,'o');
			  token_getmad($7,$$,';');
			  if (PL_parser->rsfp_filters &&
				      AvFILLp(PL_parser->rsfp_filters) >= 0)
			      append_madprops(newMADPROP('!', MAD_NULL, NULL, 0), $$, 0);
#else
			  utilize(IVAL($1), $2, $4, $5, $6);
			  $$ = (OP*)NULL;
#endif
			}
	;

/* Ordinary expressions; logical combinations */
expr	:	expr ANDOP expr
			{ $$ = newLOGOP(OP_AND, 0, $1, $3);
			  TOKEN_GETMAD($2,$$,'o');
			}
	|	expr OROP expr
			{ $$ = newLOGOP(IVAL($2), 0, $1, $3);
			  TOKEN_GETMAD($2,$$,'o');
			}
	|	expr DOROP expr
			{ $$ = newLOGOP(OP_DOR, 0, $1, $3);
			  TOKEN_GETMAD($2,$$,'o');
			}
	|	argexpr %prec PREC_LOW
	;

/* Expressions are a list of terms joined by commas */
argexpr	:	argexpr ','
			{
#ifdef MAD
			  OP* op = newNULLLIST();
			  token_getmad($2,op,',');
			  $$ = append_elem(OP_LIST, $1, op);
#else
			  $$ = $1;
#endif
			}
	|	argexpr ',' term
			{ 
			  OP* term = $3;
			  DO_MAD(
			      term = newUNOP(OP_NULL, 0, term);
			      token_getmad($2,term,',');
			  )
			  $$ = append_elem(OP_LIST, $1, term);
			}
	|	term %prec PREC_LOW
	;

/* List operators */
listop	:	LSTOP indirob argexpr /* map {...} @args or print $fh @args */
			{ $$ = convert(IVAL($1), OPf_STACKED,
				prepend_elem(OP_LIST, newGVREF(IVAL($1),$2), $3) );
			  TOKEN_GETMAD($1,$$,'o');
			}
	|	FUNC '(' indirob expr ')'      /* print ($fh @args */
			{ $$ = convert(IVAL($1), OPf_STACKED,
				prepend_elem(OP_LIST, newGVREF(IVAL($1),$3), $4) );
			  TOKEN_GETMAD($1,$$,'o');
			  TOKEN_GETMAD($2,$$,'(');
			  TOKEN_GETMAD($5,$$,')');
			}
	|	term ARROW method '(' listexprcom ')' /* $foo->bar(list) */
			{ $$ = convert(OP_ENTERSUB, OPf_STACKED,
				append_elem(OP_LIST,
				    prepend_elem(OP_LIST, scalar($1), $5),
				    newUNOP(OP_METHOD, 0, $3)));
			  TOKEN_GETMAD($2,$$,'A');
			  TOKEN_GETMAD($4,$$,'(');
			  TOKEN_GETMAD($6,$$,')');
			}
	|	term ARROW method                     /* $foo->bar */
			{ $$ = convert(OP_ENTERSUB, OPf_STACKED,
				append_elem(OP_LIST, scalar($1),
				    newUNOP(OP_METHOD, 0, $3)));
			  TOKEN_GETMAD($2,$$,'A');
			}
	|	METHOD indirob listexpr              /* new Class @args */
			{ $$ = convert(OP_ENTERSUB, OPf_STACKED,
				append_elem(OP_LIST,
				    prepend_elem(OP_LIST, $2, $3),
				    newUNOP(OP_METHOD, 0, $1)));
			}
	|	FUNCMETH indirob '(' listexprcom ')' /* method $object (@args) */
			{ $$ = convert(OP_ENTERSUB, OPf_STACKED,
				append_elem(OP_LIST,
				    prepend_elem(OP_LIST, $2, $4),
				    newUNOP(OP_METHOD, 0, $1)));
			  TOKEN_GETMAD($3,$$,'(');
			  TOKEN_GETMAD($5,$$,')');
			}
	|	LSTOP listexpr                       /* print @args */
			{ $$ = convert(IVAL($1), 0, $2);
			  TOKEN_GETMAD($1,$$,'o');
			}
	|	FUNC '(' listexprcom ')'             /* print (@args) */
			{ $$ = convert(IVAL($1), 0, $3);
			  TOKEN_GETMAD($1,$$,'o');
			  TOKEN_GETMAD($2,$$,'(');
			  TOKEN_GETMAD($4,$$,')');
			}
	|	LSTOPSUB startanonsub block /* sub f(&@);   f { foo } ... */
			{ SvREFCNT_inc_simple_void(PL_compcv);
			  $<opval>$ = newANONATTRSUB($2, 0, (OP*)NULL, $3); }
		    listexpr		%prec LSTOP  /* ... @bar */
			{ $$ = newUNOP(OP_ENTERSUB, OPf_STACKED,
				 append_elem(OP_LIST,
				   prepend_elem(OP_LIST, $<opval>4, $5), $1));
			}
	;

/* Names of methods. May use $object->$methodname */
method	:	METHOD
	|	scalar
	;

/* Some kind of subscripted expression */
subscripted:    star '{' expr ';' '}'        /* *main::{something} */
                        /* In this and all the hash accessors, ';' is
                         * provided by the tokeniser */
			{ $$ = newBINOP(OP_GELEM, 0, $1, scalar($3));
			    PL_parser->expect = XOPERATOR;
			  TOKEN_GETMAD($2,$$,'{');
			  TOKEN_GETMAD($4,$$,';');
			  TOKEN_GETMAD($5,$$,'}');
			}
	|	scalar '[' expr ']'          /* $array[$element] */
			{ $$ = newBINOP(OP_AELEM, 0, oopsAV($1), scalar($3));
			  TOKEN_GETMAD($2,$$,'[');
			  TOKEN_GETMAD($4,$$,']');
			}
	|	term ARROW '[' expr ']'      /* somearef->[$element] */
			{ $$ = newBINOP(OP_AELEM, 0,
					ref(newAVREF($1),OP_RV2AV),
					scalar($4));
			  TOKEN_GETMAD($2,$$,'a');
			  TOKEN_GETMAD($3,$$,'[');
			  TOKEN_GETMAD($5,$$,']');
			}
	|	subscripted '[' expr ']'    /* $foo->[$bar]->[$baz] */
			{ $$ = newBINOP(OP_AELEM, 0,
					ref(newAVREF($1),OP_RV2AV),
					scalar($3));
			  TOKEN_GETMAD($2,$$,'[');
			  TOKEN_GETMAD($4,$$,']');
			}
	|	scalar '{' expr ';' '}'    /* $foo->{bar();} */
			{ $$ = newBINOP(OP_HELEM, 0, oopsHV($1), jmaybe($3));
			    PL_parser->expect = XOPERATOR;
			  TOKEN_GETMAD($2,$$,'{');
			  TOKEN_GETMAD($4,$$,';');
			  TOKEN_GETMAD($5,$$,'}');
			}
	|	term ARROW '{' expr ';' '}' /* somehref->{bar();} */
			{ $$ = newBINOP(OP_HELEM, 0,
					ref(newHVREF($1),OP_RV2HV),
					jmaybe($4));
			    PL_parser->expect = XOPERATOR;
			  TOKEN_GETMAD($2,$$,'a');
			  TOKEN_GETMAD($3,$$,'{');
			  TOKEN_GETMAD($5,$$,';');
			  TOKEN_GETMAD($6,$$,'}');
			}
	|	subscripted '{' expr ';' '}' /* $foo->[bar]->{baz;} */
			{ $$ = newBINOP(OP_HELEM, 0,
					ref(newHVREF($1),OP_RV2HV),
					jmaybe($3));
			    PL_parser->expect = XOPERATOR;
			  TOKEN_GETMAD($2,$$,'{');
			  TOKEN_GETMAD($4,$$,';');
			  TOKEN_GETMAD($5,$$,'}');
			}
	|	term ARROW '(' ')'          /* $subref->() */
			{ $$ = newUNOP(OP_ENTERSUB, OPf_STACKED,
				   newCVREF(0, scalar($1)));
			  TOKEN_GETMAD($2,$$,'a');
			  TOKEN_GETMAD($3,$$,'(');
			  TOKEN_GETMAD($4,$$,')');
			}
	|	term ARROW '(' expr ')'     /* $subref->(@args) */
			{ $$ = newUNOP(OP_ENTERSUB, OPf_STACKED,
				   append_elem(OP_LIST, $4,
				       newCVREF(0, scalar($1))));
			  TOKEN_GETMAD($2,$$,'a');
			  TOKEN_GETMAD($3,$$,'(');
			  TOKEN_GETMAD($5,$$,')');
			}

	|	subscripted '(' expr ')'   /* $foo->{bar}->(@args) */
			{ $$ = newUNOP(OP_ENTERSUB, OPf_STACKED,
				   append_elem(OP_LIST, $3,
					       newCVREF(0, scalar($1))));
			  TOKEN_GETMAD($2,$$,'(');
			  TOKEN_GETMAD($4,$$,')');
			}
	|	subscripted '(' ')'        /* $foo->{bar}->() */
			{ $$ = newUNOP(OP_ENTERSUB, OPf_STACKED,
				   newCVREF(0, scalar($1)));
			  TOKEN_GETMAD($2,$$,'(');
			  TOKEN_GETMAD($3,$$,')');
			}
	|	'(' expr ')' '[' expr ']'            /* list slice */
			{ $$ = newSLICEOP(0, $5, $2);
			  TOKEN_GETMAD($1,$$,'(');
			  TOKEN_GETMAD($3,$$,')');
			  TOKEN_GETMAD($4,$$,'[');
			  TOKEN_GETMAD($6,$$,']');
			}
	|	'(' ')' '[' expr ']'                 /* empty list slice! */
			{ $$ = newSLICEOP(0, $4, (OP*)NULL);
			  TOKEN_GETMAD($1,$$,'(');
			  TOKEN_GETMAD($2,$$,')');
			  TOKEN_GETMAD($3,$$,'[');
			  TOKEN_GETMAD($5,$$,']');
			}
    ;

/* Binary operators between terms */
termbinop:	term ASSIGNOP term                     /* $x = $y */
			{ $$ = newASSIGNOP(OPf_STACKED, $1, IVAL($2), $3);
			  TOKEN_GETMAD($2,$$,'o');
			}
	|	term POWOP term                        /* $x ** $y */
			{ $$ = newBINOP(IVAL($2), 0, scalar($1), scalar($3));
			  TOKEN_GETMAD($2,$$,'o');
			}
	|	term MULOP term                        /* $x * $y, $x x $y */
			{   if (IVAL($2) != OP_REPEAT)
				scalar($1);
			    $$ = newBINOP(IVAL($2), 0, $1, scalar($3));
			  TOKEN_GETMAD($2,$$,'o');
			}
	|	term ADDOP term                        /* $x + $y */
			{ $$ = newBINOP(IVAL($2), 0, scalar($1), scalar($3));
			  TOKEN_GETMAD($2,$$,'o');
			}
	|	term SHIFTOP term                      /* $x >> $y, $x << $y */
			{ $$ = newBINOP(IVAL($2), 0, scalar($1), scalar($3));
			  TOKEN_GETMAD($2,$$,'o');
			}
	|	term RELOP term                        /* $x > $y, etc. */
			{ $$ = newBINOP(IVAL($2), 0, scalar($1), scalar($3));
			  TOKEN_GETMAD($2,$$,'o');
			}
	|	term EQOP term                         /* $x == $y, $x eq $y */
			{ $$ = newBINOP(IVAL($2), 0, scalar($1), scalar($3));
			  TOKEN_GETMAD($2,$$,'o');
			}
	|	term BITANDOP term                     /* $x & $y */
			{ $$ = newBINOP(IVAL($2), 0, scalar($1), scalar($3));
			  TOKEN_GETMAD($2,$$,'o');
			}
	|	term BITOROP term                      /* $x | $y */
			{ $$ = newBINOP(IVAL($2), 0, scalar($1), scalar($3));
			  TOKEN_GETMAD($2,$$,'o');
			}
	|	term DOTDOT term                       /* $x..$y, $x...$y */
			{
			  $$ = newRANGE(IVAL($2), scalar($1), scalar($3));
			  DO_MAD({
			      UNOP *op;
			      op = (UNOP*)$$;
			      op = (UNOP*)op->op_first;	/* get to flop */
			      op = (UNOP*)op->op_first;	/* get to flip */
			      op = (UNOP*)op->op_first;	/* get to range */
			      token_getmad($2,(OP*)op,'o');
			    })
			}
	|	term ANDAND term                       /* $x && $y */
			{ $$ = newLOGOP(OP_AND, 0, $1, $3);
			  TOKEN_GETMAD($2,$$,'o');
			}
	|	term OROR term                         /* $x || $y */
			{ $$ = newLOGOP(OP_OR, 0, $1, $3);
			  TOKEN_GETMAD($2,$$,'o');
			}
	|	term DORDOR term                       /* $x // $y */
			{ $$ = newLOGOP(OP_DOR, 0, $1, $3);
			  TOKEN_GETMAD($2,$$,'o');
			}
	|	term MATCHOP term                      /* $x =~ /$y/ */
			{ $$ = bind_match(IVAL($2), $1, $3);
			  TOKEN_GETMAD($2,
				($$->op_type == OP_NOT
				    ? ((UNOP*)$$)->op_first : $$),
				'~');
			}
    ;

/* Unary operators and terms */
termunop : '-' term %prec UMINUS                       /* -$x */
			{ $$ = newUNOP(OP_NEGATE, 0, scalar($2));
			  TOKEN_GETMAD($1,$$,'o');
			}
	|	'+' term %prec UMINUS                  /* +$x */
			{ $$ = IF_MAD(
				    newUNOP(OP_NULL, 0, $2),
				    $2
				);
			  TOKEN_GETMAD($1,$$,'+');
			}
	|	'!' term                               /* !$x */
			{ $$ = newUNOP(OP_NOT, 0, scalar($2));
			  TOKEN_GETMAD($1,$$,'o');
			}
	|	'~' term                               /* ~$x */
			{ $$ = newUNOP(OP_COMPLEMENT, 0, scalar($2));
			  TOKEN_GETMAD($1,$$,'o');
			}
	|	term POSTINC                           /* $x++ */
			{ $$ = newUNOP(OP_POSTINC, 0,
					mod(scalar($1), OP_POSTINC));
			  TOKEN_GETMAD($2,$$,'o');
			}
	|	term POSTDEC                           /* $x-- */
			{ $$ = newUNOP(OP_POSTDEC, 0,
					mod(scalar($1), OP_POSTDEC));
			  TOKEN_GETMAD($2,$$,'o');
			}
	|	PREINC term                            /* ++$x */
			{ $$ = newUNOP(OP_PREINC, 0,
					mod(scalar($2), OP_PREINC));
			  TOKEN_GETMAD($1,$$,'o');
			}
	|	PREDEC term                            /* --$x */
			{ $$ = newUNOP(OP_PREDEC, 0,
					mod(scalar($2), OP_PREDEC));
			  TOKEN_GETMAD($1,$$,'o');
			}

    ;

/* Constructors for anonymous data */
anonymous:	'[' expr ']'
			{ $$ = newANONLIST($2);
			  TOKEN_GETMAD($1,$$,'[');
			  TOKEN_GETMAD($3,$$,']');
			}
	|	'[' ']'
			{ $$ = newANONLIST((OP*)NULL);
			  TOKEN_GETMAD($1,$$,'[');
			  TOKEN_GETMAD($2,$$,']');
			}
	|	HASHBRACK expr ';' '}'	%prec '(' /* { foo => "Bar" } */
			{ $$ = newANONHASH($2);
			  TOKEN_GETMAD($1,$$,'{');
			  TOKEN_GETMAD($3,$$,';');
			  TOKEN_GETMAD($4,$$,'}');
			}
	|	HASHBRACK ';' '}'	%prec '(' /* { } (';' by tokener) */
			{ $$ = newANONHASH((OP*)NULL);
			  TOKEN_GETMAD($1,$$,'{');
			  TOKEN_GETMAD($2,$$,';');
			  TOKEN_GETMAD($3,$$,'}');
			}
	|	ANONSUB startanonsub proto subattrlist block	%prec '('
			{ SvREFCNT_inc_simple_void(PL_compcv);
			  $$ = newANONATTRSUB($2, $3, $4, $5);
			  TOKEN_GETMAD($1,$$,'o');
			  OP_GETMAD($3,$$,'s');
			  OP_GETMAD($4,$$,'a');
			}

    ;

/* Things called with "do" */
termdo	:       DO term	%prec UNIOP                     /* do $filename */
			{ $$ = dofile($2, IVAL($1));
			  TOKEN_GETMAD($1,$$,'o');
			}
	|	DO block	%prec '('               /* do { code */
			{ $$ = newUNOP(OP_NULL, OPf_SPECIAL, scope($2));
			  TOKEN_GETMAD($1,$$,'D');
			}
	|	DO WORD '(' ')'                         /* do somesub() */
			{ $$ = newUNOP(OP_ENTERSUB,
			    OPf_SPECIAL|OPf_STACKED,
			    prepend_elem(OP_LIST,
				scalar(newCVREF(
				    (OPpENTERSUB_AMPER<<8),
				    scalar($2)
				)),(OP*)NULL)); dep();
			  TOKEN_GETMAD($1,$$,'o');
			  TOKEN_GETMAD($3,$$,'(');
			  TOKEN_GETMAD($4,$$,')');
			}
	|	DO WORD '(' expr ')'                    /* do somesub(@args) */
			{ $$ = newUNOP(OP_ENTERSUB,
			    OPf_SPECIAL|OPf_STACKED,
			    append_elem(OP_LIST,
				$4,
				scalar(newCVREF(
				    (OPpENTERSUB_AMPER<<8),
				    scalar($2)
				)))); dep();
			  TOKEN_GETMAD($1,$$,'o');
			  TOKEN_GETMAD($3,$$,'(');
			  TOKEN_GETMAD($5,$$,')');
			}
	|	DO scalar '(' ')'                      /* do $subref () */
			{ $$ = newUNOP(OP_ENTERSUB, OPf_SPECIAL|OPf_STACKED,
			    prepend_elem(OP_LIST,
				scalar(newCVREF(0,scalar($2))), (OP*)NULL)); dep();
			  TOKEN_GETMAD($1,$$,'o');
			  TOKEN_GETMAD($3,$$,'(');
			  TOKEN_GETMAD($4,$$,')');
			}
	|	DO scalar '(' expr ')'                 /* do $subref (@args) */
			{ $$ = newUNOP(OP_ENTERSUB, OPf_SPECIAL|OPf_STACKED,
			    prepend_elem(OP_LIST,
				$4,
				scalar(newCVREF(0,scalar($2))))); dep();
			  TOKEN_GETMAD($1,$$,'o');
			  TOKEN_GETMAD($3,$$,'(');
			  TOKEN_GETMAD($5,$$,')');
			}

        ;

term	:	termbinop
	|	termunop
	|	anonymous
	|	termdo
	|	term '?' term ':' term
			{ $$ = newCONDOP(0, $1, $3, $5);
			  TOKEN_GETMAD($2,$$,'?');
			  TOKEN_GETMAD($4,$$,':');
			}
	|	REFGEN term                          /* \$x, \@y, \%z */
			{ $$ = newUNOP(OP_REFGEN, 0, mod($2,OP_REFGEN));
			  TOKEN_GETMAD($1,$$,'o');
			}
	|	myattrterm	%prec UNIOP
			{ $$ = $1; }
	|	LOCAL term	%prec UNIOP
			{ $$ = localize($2,IVAL($1));
			  TOKEN_GETMAD($1,$$,'k');
			}
	|	'(' expr ')'
			{ $$ = sawparens(IF_MAD(newUNOP(OP_NULL,0,$2), $2));
			  TOKEN_GETMAD($1,$$,'(');
			  TOKEN_GETMAD($3,$$,')');
			}
	|	'(' ')'
			{ $$ = sawparens(newNULLLIST());
			  TOKEN_GETMAD($1,$$,'(');
			  TOKEN_GETMAD($2,$$,')');
			}
	|	scalar	%prec '('
			{ $$ = $1; }
	|	star	%prec '('
			{ $$ = $1; }
	|	hsh 	%prec '('
			{ $$ = $1; }
	|	ary 	%prec '('
			{ $$ = $1; }
	|	arylen 	%prec '('                    /* $#x, $#{ something } */
			{ $$ = newUNOP(OP_AV2ARYLEN, 0, ref($1, OP_AV2ARYLEN));}
	|       subscripted
			{ $$ = $1; }
	|	ary '[' expr ']'                     /* array slice */
			{ $$ = prepend_elem(OP_ASLICE,
				newOP(OP_PUSHMARK, 0),
				    newLISTOP(OP_ASLICE, 0,
					list($3),
					ref($1, OP_ASLICE)));
			  TOKEN_GETMAD($2,$$,'[');
			  TOKEN_GETMAD($4,$$,']');
			}
	|	ary '{' expr ';' '}'                 /* @hash{@keys} */
			{ $$ = prepend_elem(OP_HSLICE,
				newOP(OP_PUSHMARK, 0),
				    newLISTOP(OP_HSLICE, 0,
					list($3),
					ref(oopsHV($1), OP_HSLICE)));
			    PL_parser->expect = XOPERATOR;
			  TOKEN_GETMAD($2,$$,'{');
			  TOKEN_GETMAD($4,$$,';');
			  TOKEN_GETMAD($5,$$,'}');
			}
	|	THING	%prec '('
			{ $$ = $1; }
	|	amper                                /* &foo; */
			{ $$ = newUNOP(OP_ENTERSUB, 0, scalar($1)); }
	|	amper '(' ')'                        /* &foo() */
			{ $$ = newUNOP(OP_ENTERSUB, OPf_STACKED, scalar($1));
			  TOKEN_GETMAD($2,$$,'(');
			  TOKEN_GETMAD($3,$$,')');
			}
	|	amper '(' expr ')'                   /* &foo(@args) */
			{
			  $$ = newUNOP(OP_ENTERSUB, OPf_STACKED,
				append_elem(OP_LIST, $3, scalar($1)));
			  DO_MAD({
			      OP* op = $$;
			      if (op->op_type == OP_CONST) { /* defeat const fold */
				op = (OP*)op->op_madprop->mad_val;
			      }
			      token_getmad($2,op,'(');
			      token_getmad($4,op,')');
			  })
			}
	|	NOAMP WORD listexpr                  /* foo(@args) */
			{ $$ = newUNOP(OP_ENTERSUB, OPf_STACKED,
			    append_elem(OP_LIST, $3, scalar($2)));
			  TOKEN_GETMAD($1,$$,'o');
			}
	|	LOOPEX  /* loop exiting command (goto, last, dump, etc) */
			{ $$ = newOP(IVAL($1), OPf_SPECIAL);
			    PL_hints |= HINT_BLOCK_SCOPE;
			  TOKEN_GETMAD($1,$$,'o');
			}
	|	LOOPEX term
			{ $$ = newLOOPEX(IVAL($1),$2);
			  TOKEN_GETMAD($1,$$,'o');
			}
	|	NOTOP argexpr                        /* not $foo */
			{ $$ = newUNOP(OP_NOT, 0, scalar($2));
			  TOKEN_GETMAD($1,$$,'o');
			}
	|	UNIOP                                /* Unary op, $_ implied */
			{ $$ = newOP(IVAL($1), 0);
			  TOKEN_GETMAD($1,$$,'o');
			}
	|	UNIOP block                          /* eval { foo }* */
			{ $$ = newUNOP(IVAL($1), 0, $2);
			  TOKEN_GETMAD($1,$$,'o');
			}
	|	UNIOP term                           /* Unary op */
			{ $$ = newUNOP(IVAL($1), 0, $2);
			  TOKEN_GETMAD($1,$$,'o');
			}
	|	REQUIRE                              /* require, $_ implied */
			{ $$ = newOP(OP_REQUIRE, $1 ? OPf_SPECIAL : 0);
			  TOKEN_GETMAD($1,$$,'o');
			}
	|	REQUIRE term                         /* require Foo */
			{ $$ = newUNOP(OP_REQUIRE, $1 ? OPf_SPECIAL : 0, $2);
			  TOKEN_GETMAD($1,$$,'o');
			}
	|	UNIOPSUB
			{ $$ = newUNOP(OP_ENTERSUB, OPf_STACKED, scalar($1)); }
	|	UNIOPSUB term                        /* Sub treated as unop */
			{ $$ = newUNOP(OP_ENTERSUB, OPf_STACKED,
			    append_elem(OP_LIST, $2, scalar($1))); }
	|	FUNC0                                /* Nullary operator */
			{ $$ = newOP(IVAL($1), 0);
			  TOKEN_GETMAD($1,$$,'o');
			}
	|	FUNC0 '(' ')'
			{ $$ = newOP(IVAL($1), 0);
			  TOKEN_GETMAD($1,$$,'o');
			  TOKEN_GETMAD($2,$$,'(');
			  TOKEN_GETMAD($3,$$,')');
			}
	|	FUNC0SUB                             /* Sub treated as nullop */
			{ $$ = newUNOP(OP_ENTERSUB, OPf_STACKED,
				scalar($1)); }
	|	FUNC1 '(' ')'                        /* not () */
			{ $$ = (IVAL($1) == OP_NOT)
			    ? newUNOP(IVAL($1), 0, newSVOP(OP_CONST, 0, newSViv(0)))
			    : newOP(IVAL($1), OPf_SPECIAL);

			  TOKEN_GETMAD($1,$$,'o');
			  TOKEN_GETMAD($2,$$,'(');
			  TOKEN_GETMAD($3,$$,')');
			}
	|	FUNC1 '(' expr ')'                   /* not($foo) */
			{ $$ = newUNOP(IVAL($1), 0, $3);
			  TOKEN_GETMAD($1,$$,'o');
			  TOKEN_GETMAD($2,$$,'(');
			  TOKEN_GETMAD($4,$$,')');
			}
	|	PMFUNC '(' argexpr ')'		/* m//, s///, tr/// */
			{ $$ = pmruntime($1, $3, 1);
			  TOKEN_GETMAD($2,$$,'(');
			  TOKEN_GETMAD($4,$$,')');
			}
	|	WORD
	|	listop
	|	YADAYADA
			{
			  $$ = newLISTOP(OP_DIE, 0, newOP(OP_PUSHMARK, 0),
				newSVOP(OP_CONST, 0, newSVpvs("Unimplemented")));
			  TOKEN_GETMAD($1,$$,'X');
			}
	|	PLUGEXPR
	;

/* "my" declarations, with optional attributes */
myattrterm:	MY myterm myattrlist
			{ $$ = my_attrs($2,$3);
			  DO_MAD(
			      token_getmad($1,$$,'d');
			      append_madprops($3->op_madprop, $$, 'a');
			      $3->op_madprop = 0;
			  )
			}
	|	MY myterm
			{ $$ = localize($2,IVAL($1));
			  TOKEN_GETMAD($1,$$,'d');
			}
	;

/* Things that can be "my"'d */
myterm	:	'(' expr ')'
			{ $$ = sawparens($2);
			  TOKEN_GETMAD($1,$$,'(');
			  TOKEN_GETMAD($3,$$,')');
			}
	|	'(' ')'
			{ $$ = sawparens(newNULLLIST());
			  TOKEN_GETMAD($1,$$,'(');
			  TOKEN_GETMAD($2,$$,')');
			}
	|	scalar	%prec '('
			{ $$ = $1; }
	|	hsh 	%prec '('
			{ $$ = $1; }
	|	ary 	%prec '('
			{ $$ = $1; }
	;

/* Basic list expressions */
listexpr:	/* NULL */ %prec PREC_LOW
			{ $$ = (OP*)NULL; }
	|	argexpr    %prec PREC_LOW
			{ $$ = $1; }
	;

listexprcom:	/* NULL */
			{ $$ = (OP*)NULL; }
	|	expr
			{ $$ = $1; }
	|	expr ','
			{
#ifdef MAD
			  OP* op = newNULLLIST();
			  token_getmad($2,op,',');
			  $$ = append_elem(OP_LIST, $1, op);
#else
			  $$ = $1;
#endif

			}
	;

/* A little bit of trickery to make "for my $foo (@bar)" actually be
   lexical */
my_scalar:	scalar
			{ PL_parser->in_my = 0; $$ = my($1); }
	;

amper	:	'&' indirob
			{ $$ = newCVREF(IVAL($1),$2);
			  TOKEN_GETMAD($1,$$,'&');
			}
	;

scalar	:	'$' indirob
			{ $$ = newSVREF($2);
			  TOKEN_GETMAD($1,$$,'$');
			}
	;

ary	:	'@' indirob
			{ $$ = newAVREF($2);
			  TOKEN_GETMAD($1,$$,'@');
			}
	;

hsh	:	'%' indirob
			{ $$ = newHVREF($2);
			  TOKEN_GETMAD($1,$$,'%');
			}
	;

arylen	:	DOLSHARP indirob
			{ $$ = newAVREF($2);
			  TOKEN_GETMAD($1,$$,'l');
			}
	;

star	:	'*' indirob
			{ $$ = newGVREF(0,$2);
			  TOKEN_GETMAD($1,$$,'*');
			}
	;

/* Indirect objects */
indirob	:	WORD
			{ $$ = scalar($1); }
	|	scalar %prec PREC_LOW
			{ $$ = scalar($1); }
	|	block
			{ $$ = scope($1); }

	|	PRIVATEREF
			{ $$ = $1; }
	;
