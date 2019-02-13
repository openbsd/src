/*    perly.y
 *
 *    Copyright (c) 1991-2002, 2003, 2004, 2005, 2006 Larry Wall
 *    Copyright (c) 2007, 2008, 2009, 2010, 2011 by Larry Wall and others
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
 * The main job of of this grammar is to call the various newFOO()
 * functions in op.c to build a syntax tree of OP structs.
 * It relies on the lexer in toke.c to do the tokenizing.
 *
 * Note: due to the way that the cleanup code works WRT to freeing ops on
 * the parse stack, it is dangerous to assign to the $n variables within
 * an action.
 */

/*  Make the parser re-entrant. */

%pure-parser

%start grammar

%union {
    I32	ival; /* __DEFAULT__ (marker for regen_perly.pl;
				must always be 1st union member) */
    char *pval;
    OP *opval;
    GV *gvval;
}

%token <ival> GRAMPROG GRAMEXPR GRAMBLOCK GRAMBARESTMT GRAMFULLSTMT GRAMSTMTSEQ

%token <ival> '{' '}' '[' ']' '-' '+' '@' '%' '&' '=' '.'

%token <opval> BAREWORD METHOD FUNCMETH THING PMFUNC PRIVATEREF QWLIST
%token <opval> FUNC0OP FUNC0SUB UNIOPSUB LSTOPSUB
%token <opval> PLUGEXPR PLUGSTMT
%token <pval> LABEL
%token <ival> FORMAT SUB SIGSUB ANONSUB ANON_SIGSUB PACKAGE USE
%token <ival> WHILE UNTIL IF UNLESS ELSE ELSIF CONTINUE FOR
%token <ival> GIVEN WHEN DEFAULT
%token <ival> LOOPEX DOTDOT YADAYADA
%token <ival> FUNC0 FUNC1 FUNC UNIOP LSTOP
%token <ival> RELOP EQOP MULOP ADDOP
%token <ival> DOLSHARP DO HASHBRACK NOAMP
%token <ival> LOCAL MY REQUIRE
%token <ival> COLONATTR FORMLBRACK FORMRBRACK

%type <ival> grammar remember mremember
%type <ival>  startsub startanonsub startformsub

%type <ival> mintro

%type <opval> stmtseq fullstmt labfullstmt barestmt block mblock else
%type <opval> expr term subscripted scalar ary hsh arylen star amper sideff
%type <opval> sliceme kvslice gelem
%type <opval> listexpr nexpr texpr iexpr mexpr mnexpr
%type <opval> optlistexpr optexpr optrepl indirob listop method
%type <opval> formname subname proto cont my_scalar my_var
%type <opval> refgen_topic formblock
%type <opval> subattrlist myattrlist myattrterm myterm
%type <opval> termbinop termunop anonymous termdo
%type <ival>  sigslurpsigil
%type <opval> sigvarname sigdefault sigscalarelem sigslurpelem
%type <opval> sigelem siglist siglistornull subsignature optsubsignature
%type <opval> subbody optsubbody sigsubbody optsigsubbody
%type <opval> formstmtseq formline formarg

%nonassoc <ival> PREC_LOW
%nonassoc LOOPEX

%left <ival> OROP DOROP
%left <ival> ANDOP
%right <ival> NOTOP
%nonassoc LSTOP LSTOPSUB
%left <ival> ','
%right <ival> ASSIGNOP
%right <ival> '?' ':'
%nonassoc DOTDOT
%left <ival> OROR DORDOR
%left <ival> ANDAND
%left <ival> BITOROP
%left <ival> BITANDOP
%nonassoc EQOP
%nonassoc RELOP
%nonassoc UNIOP UNIOPSUB
%nonassoc REQUIRE
%left <ival> SHIFTOP
%left ADDOP
%left MULOP
%left <ival> MATCHOP
%right <ival> '!' '~' UMINUS REFGEN
%right <ival> POWOP
%nonassoc <ival> PREINC PREDEC POSTINC POSTDEC POSTJOIN
%left <ival> ARROW
%nonassoc <ival> ')'
%left <ival> '('
%left '[' '{'

%% /* RULES */

/* Top-level choice of what kind of thing yyparse was called to parse */
grammar	:	GRAMPROG
			{
			  parser->expect = XSTATE;
                          $<ival>$ = 0;
			}
		remember stmtseq
			{
			  newPROG(block_end($3,$4));
			  PL_compiling.cop_seq = 0;
			  $$ = 0;
			}
	|	GRAMEXPR
			{
			  parser->expect = XTERM;
                          $<ival>$ = 0;
			}
		optexpr
			{
			  PL_eval_root = $3;
			  $$ = 0;
			}
	|	GRAMBLOCK
			{
			  parser->expect = XBLOCK;
                          $<ival>$ = 0;
			}
		block
			{
			  PL_pad_reset_pending = TRUE;
			  PL_eval_root = $3;
			  $$ = 0;
			  yyunlex();
			  parser->yychar = yytoken = YYEOF;
			}
	|	GRAMBARESTMT
			{
			  parser->expect = XSTATE;
                          $<ival>$ = 0;
			}
		barestmt
			{
			  PL_pad_reset_pending = TRUE;
			  PL_eval_root = $3;
			  $$ = 0;
			  yyunlex();
			  parser->yychar = yytoken = YYEOF;
			}
	|	GRAMFULLSTMT
			{
			  parser->expect = XSTATE;
                          $<ival>$ = 0;
			}
		fullstmt
			{
			  PL_pad_reset_pending = TRUE;
			  PL_eval_root = $3;
			  $$ = 0;
			  yyunlex();
			  parser->yychar = yytoken = YYEOF;
			}
	|	GRAMSTMTSEQ
			{
			  parser->expect = XSTATE;
                          $<ival>$ = 0;
			}
		stmtseq
			{
			  PL_eval_root = $3;
			  $$ = 0;
			}
	;

/* An ordinary block */
block	:	'{' remember stmtseq '}'
			{ if (parser->copline > (line_t)$1)
			      parser->copline = (line_t)$1;
			  $$ = block_end($2, $3);
			}
	;

/* format body */
formblock:	'=' remember ';' FORMRBRACK formstmtseq ';' '.'
			{ if (parser->copline > (line_t)$1)
			      parser->copline = (line_t)$1;
			  $$ = block_end($2, $5);
			}
	;

remember:	/* NULL */	/* start a full lexical scope */
			{ $$ = block_start(TRUE);
			  parser->parsed_sub = 0; }
	;

mblock	:	'{' mremember stmtseq '}'
			{ if (parser->copline > (line_t)$1)
			      parser->copline = (line_t)$1;
			  $$ = block_end($2, $3);
			}
	;

mremember:	/* NULL */	/* start a partial lexical scope */
			{ $$ = block_start(FALSE);
			  parser->parsed_sub = 0; }
	;

/* A sequence of statements in the program */
stmtseq	:	/* NULL */
			{ $$ = NULL; }
	|	stmtseq fullstmt
			{   $$ = op_append_list(OP_LINESEQ, $1, $2);
			    PL_pad_reset_pending = TRUE;
			    if ($1 && $2)
				PL_hints |= HINT_BLOCK_SCOPE;
			}
	;

/* A sequence of format lines */
formstmtseq:	/* NULL */
			{ $$ = NULL; }
	|	formstmtseq formline
			{   $$ = op_append_list(OP_LINESEQ, $1, $2);
			    PL_pad_reset_pending = TRUE;
			    if ($1 && $2)
				PL_hints |= HINT_BLOCK_SCOPE;
			}
	;

/* A statement in the program, including optional labels */
fullstmt:	barestmt
			{
			  $$ = $1 ? newSTATEOP(0, NULL, $1) : NULL;
			}
	|	labfullstmt
			{ $$ = $1; }
	;

labfullstmt:	LABEL barestmt
			{
			  $$ = newSTATEOP(SVf_UTF8 * $1[strlen($1)+1], $1, $2);
			}
	|	LABEL labfullstmt
			{
			  $$ = newSTATEOP(SVf_UTF8 * $1[strlen($1)+1], $1, $2);
			}
	;

/* A bare statement, lacking label and other aspects of state op */
barestmt:	PLUGSTMT
			{ $$ = $1; }
	|	FORMAT startformsub formname formblock
			{
			  CV *fmtcv = PL_compcv;
			  newFORM($2, $3, $4);
			  $$ = NULL;
			  if (CvOUTSIDE(fmtcv) && !CvEVAL(CvOUTSIDE(fmtcv))) {
			      pad_add_weakref(fmtcv);
			  }
			  parser->parsed_sub = 1;
			}
	|	SUB subname startsub
                    /* sub declaration or definition not within scope
                       of 'use feature "signatures"'*/
			{
                          init_named_cv(PL_compcv, $2);
			  parser->in_my = 0;
			  parser->in_my_stash = NULL;
			}
                    proto subattrlist optsubbody
			{
			  SvREFCNT_inc_simple_void(PL_compcv);
			  $2->op_type == OP_CONST
			      ? newATTRSUB($3, $2, $5, $6, $7)
			      : newMYSUB($3, $2, $5, $6, $7)
			  ;
			  $$ = NULL;
			  intro_my();
			  parser->parsed_sub = 1;
			}
	|	SIGSUB subname startsub
                    /* sub declaration or definition under 'use feature
                     * "signatures"'. (Note that a signature isn't
                     * allowed in a declaration)
                     */
			{
                          init_named_cv(PL_compcv, $2);
			  parser->in_my = 0;
			  parser->in_my_stash = NULL;
			}
                    subattrlist optsigsubbody
			{
			  SvREFCNT_inc_simple_void(PL_compcv);
			  $2->op_type == OP_CONST
			      ? newATTRSUB($3, $2, NULL, $5, $6)
			      : newMYSUB(  $3, $2, NULL, $5, $6)
			  ;
			  $$ = NULL;
			  intro_my();
			  parser->parsed_sub = 1;
			}
	|	PACKAGE BAREWORD BAREWORD ';'
			{
			  package($3);
			  if ($2)
			      package_version($2);
			  $$ = NULL;
			}
	|	USE startsub
			{ CvSPECIAL_on(PL_compcv); /* It's a BEGIN {} */ }
		BAREWORD BAREWORD optlistexpr ';'
			{
			  SvREFCNT_inc_simple_void(PL_compcv);
			  utilize($1, $2, $4, $5, $6);
			  parser->parsed_sub = 1;
			  $$ = NULL;
			}
	|	IF '(' remember mexpr ')' mblock else
			{
			  $$ = block_end($3,
			      newCONDOP(0, $4, op_scope($6), $7));
			  parser->copline = (line_t)$1;
			}
	|	UNLESS '(' remember mexpr ')' mblock else
			{
			  $$ = block_end($3,
                              newCONDOP(0, $4, $7, op_scope($6)));
			  parser->copline = (line_t)$1;
			}
	|	GIVEN '(' remember mexpr ')' mblock
			{
			  $$ = block_end($3, newGIVENOP($4, op_scope($6), 0));
			  parser->copline = (line_t)$1;
			}
	|	WHEN '(' remember mexpr ')' mblock
			{ $$ = block_end($3, newWHENOP($4, op_scope($6))); }
	|	DEFAULT block
			{ $$ = newWHENOP(0, op_scope($2)); }
	|	WHILE '(' remember texpr ')' mintro mblock cont
			{
			  $$ = block_end($3,
				  newWHILEOP(0, 1, NULL,
				      $4, $7, $8, $6));
			  parser->copline = (line_t)$1;
			}
	|	UNTIL '(' remember iexpr ')' mintro mblock cont
			{
			  $$ = block_end($3,
				  newWHILEOP(0, 1, NULL,
				      $4, $7, $8, $6));
			  parser->copline = (line_t)$1;
			}
	|	FOR '(' remember mnexpr ';'
			{ parser->expect = XTERM; }
		texpr ';'
			{ parser->expect = XTERM; }
		mintro mnexpr ')'
		mblock
			{
			  OP *initop = $4;
			  OP *forop = newWHILEOP(0, 1, NULL,
				      scalar($7), $13, $11, $10);
			  if (initop) {
			      forop = op_prepend_elem(OP_LINESEQ, initop,
				  op_append_elem(OP_LINESEQ,
				      newOP(OP_UNSTACK, OPf_SPECIAL),
				      forop));
			  }
			  PL_hints |= HINT_BLOCK_SCOPE;
			  $$ = block_end($3, forop);
			  parser->copline = (line_t)$1;
			}
	|	FOR MY remember my_scalar '(' mexpr ')' mblock cont
			{
			  $$ = block_end($3, newFOROP(0, $4, $6, $8, $9));
			  parser->copline = (line_t)$1;
			}
	|	FOR scalar '(' remember mexpr ')' mblock cont
			{
			  $$ = block_end($4, newFOROP(0,
				      op_lvalue($2, OP_ENTERLOOP), $5, $7, $8));
			  parser->copline = (line_t)$1;
			}
	|	FOR my_refgen remember my_var
			{ parser->in_my = 0; $<opval>$ = my($4); }
		'(' mexpr ')' mblock cont
			{
			  $$ = block_end(
				$3,
				newFOROP(0,
					 op_lvalue(
					    newUNOP(OP_REFGEN, 0,
						    $<opval>5),
					    OP_ENTERLOOP),
					 $7, $9, $10)
			  );
			  parser->copline = (line_t)$1;
			}
	|	FOR REFGEN refgen_topic '(' remember mexpr ')' mblock cont
			{
			  $$ = block_end($5, newFOROP(
				0, op_lvalue(newUNOP(OP_REFGEN, 0,
						     $3),
					     OP_ENTERLOOP), $6, $8, $9));
			  parser->copline = (line_t)$1;
			}
	|	FOR '(' remember mexpr ')' mblock cont
			{
			  $$ = block_end($3,
				  newFOROP(0, NULL, $4, $6, $7));
			  parser->copline = (line_t)$1;
			}
	|	block cont
			{
			  /* a block is a loop that happens once */
			  $$ = newWHILEOP(0, 1, NULL,
				  NULL, $1, $2, 0);
			}
	|	PACKAGE BAREWORD BAREWORD '{' remember
			{
			  package($3);
			  if ($2) {
			      package_version($2);
			  }
			}
		stmtseq '}'
			{
			  /* a block is a loop that happens once */
			  $$ = newWHILEOP(0, 1, NULL,
				  NULL, block_end($5, $7), NULL, 0);
			  if (parser->copline > (line_t)$4)
			      parser->copline = (line_t)$4;
			}
	|	sideff ';'
			{
			  $$ = $1;
			}
	|	YADAYADA ';'
			{
			  $$ = newLISTOP(OP_DIE, 0, newOP(OP_PUSHMARK, 0),
				newSVOP(OP_CONST, 0, newSVpvs("Unimplemented")));
			}
	|	';'
			{
			  $$ = NULL;
			  parser->copline = NOLINE;
			}
	;

/* Format line */
formline:	THING formarg
			{ OP *list;
			  if ($2) {
			      OP *term = $2;
			      list = op_append_elem(OP_LIST, $1, term);
			  }
			  else {
			      list = $1;
			  }
			  if (parser->copline == NOLINE)
			       parser->copline = CopLINE(PL_curcop)-1;
			  else parser->copline--;
			  $$ = newSTATEOP(0, NULL,
					  op_convert_list(OP_FORMLINE, 0, list));
			}
	;

formarg	:	/* NULL */
			{ $$ = NULL; }
	|	FORMLBRACK stmtseq FORMRBRACK
			{ $$ = op_unscope($2); }
	;

/* An expression which may have a side-effect */
sideff	:	error
			{ $$ = NULL; }
	|	expr
			{ $$ = $1; }
	|	expr IF expr
			{ $$ = newLOGOP(OP_AND, 0, $3, $1); }
	|	expr UNLESS expr
			{ $$ = newLOGOP(OP_OR, 0, $3, $1); }
	|	expr WHILE expr
			{ $$ = newLOOPOP(OPf_PARENS, 1, scalar($3), $1); }
	|	expr UNTIL iexpr
			{ $$ = newLOOPOP(OPf_PARENS, 1, $3, $1); }
	|	expr FOR expr
			{ $$ = newFOROP(0, NULL, $3, $1, NULL);
			  parser->copline = (line_t)$2; }
	|	expr WHEN expr
			{ $$ = newWHENOP($3, op_scope($1)); }
	;

/* else and elsif blocks */
else	:	/* NULL */
			{ $$ = NULL; }
	|	ELSE mblock
			{
			  ($2)->op_flags |= OPf_PARENS;
			  $$ = op_scope($2);
			}
	|	ELSIF '(' mexpr ')' mblock else
			{ parser->copline = (line_t)$1;
			    $$ = newCONDOP(0,
				newSTATEOP(OPf_SPECIAL,NULL,$3),
				op_scope($5), $6);
			  PL_hints |= HINT_BLOCK_SCOPE;
			}
	;

/* Continue blocks */
cont	:	/* NULL */
			{ $$ = NULL; }
	|	CONTINUE block
			{ $$ = op_scope($2); }
	;

/* determine whether there are any new my declarations */
mintro	:	/* NULL */
			{ $$ = (PL_min_intro_pending &&
			    PL_max_intro_pending >=  PL_min_intro_pending);
			  intro_my(); }

/* Normal expression */
nexpr	:	/* NULL */
			{ $$ = NULL; }
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

formname:	BAREWORD	{ $$ = $1; }
	|	/* NULL */	{ $$ = NULL; }
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
subname	:	BAREWORD
	|	PRIVATEREF
	;

/* Subroutine prototype */
proto	:	/* NULL */
			{ $$ = NULL; }
	|	THING
	;

/* Optional list of subroutine attributes */
subattrlist:	/* NULL */
			{ $$ = NULL; }
	|	COLONATTR THING
			{ $$ = $2; }
	|	COLONATTR
			{ $$ = NULL; }
	;

/* List of attributes for a "my" variable declaration */
myattrlist:	COLONATTR THING
			{ $$ = $2; }
	|	COLONATTR
			{ $$ = NULL; }
	;



/* --------------------------------------
 * subroutine signature parsing
 */

/* the '' or 'foo' part of a '$' or '@foo' etc signature variable  */
sigvarname:     /* NULL */
			{ parser->in_my = 0; $$ = NULL; }
        |       PRIVATEREF
                        { parser->in_my = 0; $$ = $1; }
	;

sigslurpsigil:
                '@'
                        { $$ = '@'; }
        |       '%'
                        { $$ = '%'; }

/* @, %, @foo, %foo */
sigslurpelem: sigslurpsigil sigvarname sigdefault/* def only to catch errors */ 
                        {
                            I32 sigil   = $1;
                            OP *var     = $2;
                            OP *defexpr = $3;

                            if (parser->sig_slurpy)
                                yyerror("Multiple slurpy parameters not allowed");
                            parser->sig_slurpy = (char)sigil;

                            if (defexpr)
                                yyerror("A slurpy parameter may not have "
                                        "a default value");

                            $$ = var ? newSTATEOP(0, NULL, var) : NULL;
                        }
	;

/* default part of sub signature scalar element: i.e. '= default_expr' */
sigdefault:	/* NULL */
			{ $$ = NULL; }
        |       ASSIGNOP
                        { $$ = newOP(OP_NULL, 0); }
        |       ASSIGNOP term
                        { $$ = $2; }


/* subroutine signature scalar element: e.g. '$x', '$=', '$x = $default' */
sigscalarelem:
                '$' sigvarname sigdefault
                        {
                            OP *var     = $2;
                            OP *defexpr = $3;

                            if (parser->sig_slurpy)
                                yyerror("Slurpy parameter not last");

                            parser->sig_elems++;

                            if (defexpr) {
                                parser->sig_optelems++;

                                if (   defexpr->op_type == OP_NULL
                                    && !(defexpr->op_flags & OPf_KIDS))
                                {
                                    /* handle '$=' special case */
                                    if (var)
                                        yyerror("Optional parameter "
                                                    "lacks default expression");
                                    op_free(defexpr);
                                }
                                else { 
                                    /* a normal '=default' expression */ 
                                    OP *defop = (OP*)alloc_LOGOP(OP_ARGDEFELEM,
                                                        defexpr,
                                                        LINKLIST(defexpr));
                                    /* re-purpose op_targ to hold @_ index */
                                    defop->op_targ =
                                        (PADOFFSET)(parser->sig_elems - 1);

                                    if (var) {
                                        var->op_flags |= OPf_STACKED;
                                        (void)op_sibling_splice(var,
                                                        NULL, 0, defop);
                                        scalar(defop);
                                    }
                                    else
                                        var = newUNOP(OP_NULL, 0, defop);

                                    LINKLIST(var);
                                    /* NB: normally the first child of a
                                     * logop is executed before the logop,
                                     * and it pushes a boolean result
                                     * ready for the logop. For ARGDEFELEM,
                                     * the op itself does the boolean
                                     * calculation, so set the first op to
                                     * it instead.
                                     */
                                    var->op_next = defop;
                                    defexpr->op_next = var;
                                }
                            }
                            else {
                                if (parser->sig_optelems)
                                    yyerror("Mandatory parameter "
                                            "follows optional parameter");
                            }

                            $$ = var ? newSTATEOP(0, NULL, var) : NULL;
                        }
	;


/* subroutine signature element: e.g. '$x = $default' or '%h' */
sigelem:        sigscalarelem
                        { parser->in_my = KEY_sigvar; $$ = $1; }
        |       sigslurpelem
                        { parser->in_my = KEY_sigvar; $$ = $1; }
	;

/* list of subroutine signature elements */
siglist:
	 	siglist ','
			{ $$ = $1; }
	|	siglist ',' sigelem
			{
			  $$ = op_append_list(OP_LINESEQ, $1, $3);
			}
        |	sigelem  %prec PREC_LOW
			{ $$ = $1; }
	;

/* () or (....) */
siglistornull:		/* NULL */
			{ $$ = NULL; }
	|	siglist
			{ $$ = $1; }

/* optional subroutine signature */
optsubsignature:	/* NULL */
			{ $$ = NULL; }
	|	subsignature
			{ $$ = $1; }

/* Subroutine signature */
subsignature:	'('
                        {
                            ENTER;
                            SAVEIV(parser->sig_elems);
                            SAVEIV(parser->sig_optelems);
                            SAVEI8(parser->sig_slurpy);
                            parser->sig_elems    = 0;
                            parser->sig_optelems = 0;
                            parser->sig_slurpy   = 0;
                            parser->in_my        = KEY_sigvar;
                        }
                siglistornull
                ')'
			{
                            OP            *sigops = $3;
                            UNOP_AUX_item *aux;
                            OP            *check;

			    if (!FEATURE_SIGNATURES_IS_ENABLED)
			        Perl_croak(aTHX_ "Experimental "
                                    "subroutine signatures not enabled");

                            /* We shouldn't get here otherwise */
                            Perl_ck_warner_d(aTHX_
                                packWARN(WARN_EXPERIMENTAL__SIGNATURES),
                                "The signatures feature is experimental");

                            aux = (UNOP_AUX_item*)PerlMemShared_malloc(
                                sizeof(UNOP_AUX_item) * 3);
                            aux[0].iv = parser->sig_elems;
                            aux[1].iv = parser->sig_optelems;
                            aux[2].iv = parser->sig_slurpy;
                            check = newUNOP_AUX(OP_ARGCHECK, 0, NULL, aux);
                            sigops = op_prepend_elem(OP_LINESEQ, check, sigops);
                            sigops = op_prepend_elem(OP_LINESEQ,
                                                newSTATEOP(0, NULL, NULL),
                                                sigops);
                            /* a nextstate at the end handles context
                             * correctly for an empty sub body */
                            $$ = op_append_elem(OP_LINESEQ,
                                                sigops,
                                                newSTATEOP(0, NULL, NULL));

                            parser->in_my = 0;
                            /* tell the toker that attrributes can follow
                             * this sig, but only so that the toker
                             * can skip through any (illegal) trailing
                             * attribute text then give a useful error
                             * message about "attributes before sig",
                             * rather than falling over ina mess at
                             * unrecognised syntax.
                             */
                            parser->expect = XATTRBLOCK;
                            parser->sig_seen = TRUE;
                            LEAVE;
			}
	;

/* Optional subroutine body (for named subroutine declaration) */
optsubbody:	subbody { $$ = $1; }
	|	';'	{ $$ = NULL; }
	;


/* Subroutine body (without signature) */
subbody:	remember  '{' stmtseq '}'
			{
			  if (parser->copline > (line_t)$2)
			      parser->copline = (line_t)$2;
			  $$ = block_end($1, $3);
			}
	;


/* optional [ Subroutine body with optional signature ] (for named
 * subroutine declaration) */
optsigsubbody:	sigsubbody { $$ = $1; }
	|	';'	   { $$ = NULL; }

/* Subroutine body with optional signature */
sigsubbody:	remember optsubsignature '{' stmtseq '}'
			{
			  if (parser->copline > (line_t)$3)
			      parser->copline = (line_t)$3;
			  $$ = block_end($1,
				op_append_list(OP_LINESEQ, $2, $4));
 			}
 	;


/* Ordinary expressions; logical combinations */
expr	:	expr ANDOP expr
			{ $$ = newLOGOP(OP_AND, 0, $1, $3); }
	|	expr OROP expr
			{ $$ = newLOGOP($2, 0, $1, $3); }
	|	expr DOROP expr
			{ $$ = newLOGOP(OP_DOR, 0, $1, $3); }
	|	listexpr %prec PREC_LOW
	;

/* Expressions are a list of terms joined by commas */
listexpr:	listexpr ','
			{ $$ = $1; }
	|	listexpr ',' term
			{
			  OP* term = $3;
			  $$ = op_append_elem(OP_LIST, $1, term);
			}
	|	term %prec PREC_LOW
	;

/* List operators */
listop	:	LSTOP indirob listexpr /* map {...} @args or print $fh @args */
			{ $$ = op_convert_list($1, OPf_STACKED,
				op_prepend_elem(OP_LIST, newGVREF($1,$2), $3) );
			}
	|	FUNC '(' indirob expr ')'      /* print ($fh @args */
			{ $$ = op_convert_list($1, OPf_STACKED,
				op_prepend_elem(OP_LIST, newGVREF($1,$3), $4) );
			}
	|	term ARROW method '(' optexpr ')' /* $foo->bar(list) */
			{ $$ = op_convert_list(OP_ENTERSUB, OPf_STACKED,
				op_append_elem(OP_LIST,
				    op_prepend_elem(OP_LIST, scalar($1), $5),
				    newMETHOP(OP_METHOD, 0, $3)));
			}
	|	term ARROW method                     /* $foo->bar */
			{ $$ = op_convert_list(OP_ENTERSUB, OPf_STACKED,
				op_append_elem(OP_LIST, scalar($1),
				    newMETHOP(OP_METHOD, 0, $3)));
			}
	|	METHOD indirob optlistexpr           /* new Class @args */
			{ $$ = op_convert_list(OP_ENTERSUB, OPf_STACKED,
				op_append_elem(OP_LIST,
				    op_prepend_elem(OP_LIST, $2, $3),
				    newMETHOP(OP_METHOD, 0, $1)));
			}
	|	FUNCMETH indirob '(' optexpr ')'    /* method $object (@args) */
			{ $$ = op_convert_list(OP_ENTERSUB, OPf_STACKED,
				op_append_elem(OP_LIST,
				    op_prepend_elem(OP_LIST, $2, $4),
				    newMETHOP(OP_METHOD, 0, $1)));
			}
	|	LSTOP optlistexpr                    /* print @args */
			{ $$ = op_convert_list($1, 0, $2); }
	|	FUNC '(' optexpr ')'                 /* print (@args) */
			{ $$ = op_convert_list($1, 0, $3); }
	|	LSTOPSUB startanonsub block /* sub f(&@);   f { foo } ... */
			{ SvREFCNT_inc_simple_void(PL_compcv);
			  $<opval>$ = newANONATTRSUB($2, 0, NULL, $3); }
		    optlistexpr		%prec LSTOP  /* ... @bar */
			{ $$ = newUNOP(OP_ENTERSUB, OPf_STACKED,
				 op_append_elem(OP_LIST,
				   op_prepend_elem(OP_LIST, $<opval>4, $5), $1));
			}
	;

/* Names of methods. May use $object->$methodname */
method	:	METHOD
	|	scalar
	;

/* Some kind of subscripted expression */
subscripted:    gelem '{' expr ';' '}'        /* *main::{something} */
                        /* In this and all the hash accessors, ';' is
                         * provided by the tokeniser */
			{ $$ = newBINOP(OP_GELEM, 0, $1, scalar($3)); }
	|	scalar '[' expr ']'          /* $array[$element] */
			{ $$ = newBINOP(OP_AELEM, 0, oopsAV($1), scalar($3));
			}
	|	term ARROW '[' expr ']'      /* somearef->[$element] */
			{ $$ = newBINOP(OP_AELEM, 0,
					ref(newAVREF($1),OP_RV2AV),
					scalar($4));
			}
	|	subscripted '[' expr ']'    /* $foo->[$bar]->[$baz] */
			{ $$ = newBINOP(OP_AELEM, 0,
					ref(newAVREF($1),OP_RV2AV),
					scalar($3));
			}
	|	scalar '{' expr ';' '}'    /* $foo{bar();} */
			{ $$ = newBINOP(OP_HELEM, 0, oopsHV($1), jmaybe($3));
			}
	|	term ARROW '{' expr ';' '}' /* somehref->{bar();} */
			{ $$ = newBINOP(OP_HELEM, 0,
					ref(newHVREF($1),OP_RV2HV),
					jmaybe($4)); }
	|	subscripted '{' expr ';' '}' /* $foo->[bar]->{baz;} */
			{ $$ = newBINOP(OP_HELEM, 0,
					ref(newHVREF($1),OP_RV2HV),
					jmaybe($3)); }
	|	term ARROW '(' ')'          /* $subref->() */
			{ $$ = newUNOP(OP_ENTERSUB, OPf_STACKED,
				   newCVREF(0, scalar($1)));
			  if (parser->expect == XBLOCK)
			      parser->expect = XOPERATOR;
			}
	|	term ARROW '(' expr ')'     /* $subref->(@args) */
			{ $$ = newUNOP(OP_ENTERSUB, OPf_STACKED,
				   op_append_elem(OP_LIST, $4,
				       newCVREF(0, scalar($1))));
			  if (parser->expect == XBLOCK)
			      parser->expect = XOPERATOR;
			}

	|	subscripted '(' expr ')'   /* $foo->{bar}->(@args) */
			{ $$ = newUNOP(OP_ENTERSUB, OPf_STACKED,
				   op_append_elem(OP_LIST, $3,
					       newCVREF(0, scalar($1))));
			  if (parser->expect == XBLOCK)
			      parser->expect = XOPERATOR;
			}
	|	subscripted '(' ')'        /* $foo->{bar}->() */
			{ $$ = newUNOP(OP_ENTERSUB, OPf_STACKED,
				   newCVREF(0, scalar($1)));
			  if (parser->expect == XBLOCK)
			      parser->expect = XOPERATOR;
			}
	|	'(' expr ')' '[' expr ']'            /* list slice */
			{ $$ = newSLICEOP(0, $5, $2); }
	|	QWLIST '[' expr ']'            /* list literal slice */
			{ $$ = newSLICEOP(0, $3, $1); }
	|	'(' ')' '[' expr ']'                 /* empty list slice! */
			{ $$ = newSLICEOP(0, $4, NULL); }
    ;

/* Binary operators between terms */
termbinop:	term ASSIGNOP term                     /* $x = $y, $x += $y */
			{ $$ = newASSIGNOP(OPf_STACKED, $1, $2, $3); }
	|	term POWOP term                        /* $x ** $y */
			{ $$ = newBINOP($2, 0, scalar($1), scalar($3)); }
	|	term MULOP term                        /* $x * $y, $x x $y */
			{   if ($2 != OP_REPEAT)
				scalar($1);
			    $$ = newBINOP($2, 0, $1, scalar($3));
			}
	|	term ADDOP term                        /* $x + $y */
			{ $$ = newBINOP($2, 0, scalar($1), scalar($3)); }
	|	term SHIFTOP term                      /* $x >> $y, $x << $y */
			{ $$ = newBINOP($2, 0, scalar($1), scalar($3)); }
	|	term RELOP term                        /* $x > $y, etc. */
			{ $$ = newBINOP($2, 0, scalar($1), scalar($3)); }
	|	term EQOP term                         /* $x == $y, $x eq $y */
			{ $$ = newBINOP($2, 0, scalar($1), scalar($3)); }
	|	term BITANDOP term                     /* $x & $y */
			{ $$ = newBINOP($2, 0, scalar($1), scalar($3)); }
	|	term BITOROP term                      /* $x | $y */
			{ $$ = newBINOP($2, 0, scalar($1), scalar($3)); }
	|	term DOTDOT term                       /* $x..$y, $x...$y */
			{ $$ = newRANGE($2, scalar($1), scalar($3)); }
	|	term ANDAND term                       /* $x && $y */
			{ $$ = newLOGOP(OP_AND, 0, $1, $3); }
	|	term OROR term                         /* $x || $y */
			{ $$ = newLOGOP(OP_OR, 0, $1, $3); }
	|	term DORDOR term                       /* $x // $y */
			{ $$ = newLOGOP(OP_DOR, 0, $1, $3); }
	|	term MATCHOP term                      /* $x =~ /$y/ */
			{ $$ = bind_match($2, $1, $3); }
    ;

/* Unary operators and terms */
termunop : '-' term %prec UMINUS                       /* -$x */
			{ $$ = newUNOP(OP_NEGATE, 0, scalar($2)); }
	|	'+' term %prec UMINUS                  /* +$x */
			{ $$ = $2; }

	|	'!' term                               /* !$x */
			{ $$ = newUNOP(OP_NOT, 0, scalar($2)); }
	|	'~' term                               /* ~$x */
			{ $$ = newUNOP($1, 0, scalar($2)); }
	|	term POSTINC                           /* $x++ */
			{ $$ = newUNOP(OP_POSTINC, 0,
					op_lvalue(scalar($1), OP_POSTINC)); }
	|	term POSTDEC                           /* $x-- */
			{ $$ = newUNOP(OP_POSTDEC, 0,
					op_lvalue(scalar($1), OP_POSTDEC));}
	|	term POSTJOIN    /* implicit join after interpolated ->@ */
			{ $$ = op_convert_list(OP_JOIN, 0,
				       op_append_elem(
					OP_LIST,
					newSVREF(scalar(
					    newSVOP(OP_CONST,0,
						    newSVpvs("\""))
					)),
					$1
				       ));
			}
	|	PREINC term                            /* ++$x */
			{ $$ = newUNOP(OP_PREINC, 0,
					op_lvalue(scalar($2), OP_PREINC)); }
	|	PREDEC term                            /* --$x */
			{ $$ = newUNOP(OP_PREDEC, 0,
					op_lvalue(scalar($2), OP_PREDEC)); }

    ;

/* Constructors for anonymous data */
anonymous:	'[' expr ']'
			{ $$ = newANONLIST($2); }
	|	'[' ']'
			{ $$ = newANONLIST(NULL);}
	|	HASHBRACK expr ';' '}'	%prec '(' /* { foo => "Bar" } */
			{ $$ = newANONHASH($2); }
	|	HASHBRACK ';' '}'	%prec '(' /* { } (';' by tokener) */
			{ $$ = newANONHASH(NULL); }
	|	ANONSUB     startanonsub proto subattrlist subbody    %prec '('
			{ SvREFCNT_inc_simple_void(PL_compcv);
			  $$ = newANONATTRSUB($2, $3, $4, $5); }
	|	ANON_SIGSUB startanonsub subattrlist sigsubbody %prec '('
			{ SvREFCNT_inc_simple_void(PL_compcv);
			  $$ = newANONATTRSUB($2, NULL, $3, $4); }
    ;

/* Things called with "do" */
termdo	:       DO term	%prec UNIOP                     /* do $filename */
			{ $$ = dofile($2, $1);}
	|	DO block	%prec '('               /* do { code */
			{ $$ = newUNOP(OP_NULL, OPf_SPECIAL, op_scope($2));}
        ;

term	:	termbinop
	|	termunop
	|	anonymous
	|	termdo
	|	term '?' term ':' term
			{ $$ = newCONDOP(0, $1, $3, $5); }
	|	REFGEN term                          /* \$x, \@y, \%z */
			{ $$ = newUNOP(OP_REFGEN, 0, $2); }
	|	MY REFGEN term
			{ $$ = newUNOP(OP_REFGEN, 0, localize($3,1)); }
	|	myattrterm	%prec UNIOP
			{ $$ = $1; }
	|	LOCAL term	%prec UNIOP
			{ $$ = localize($2,0); }
	|	'(' expr ')'
			{ $$ = sawparens($2); }
	|	QWLIST
			{ $$ = $1; }
	|	'(' ')'
			{ $$ = sawparens(newNULLLIST()); }
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
	|	sliceme '[' expr ']'                     /* array slice */
			{ $$ = op_prepend_elem(OP_ASLICE,
				newOP(OP_PUSHMARK, 0),
				    newLISTOP(OP_ASLICE, 0,
					list($3),
					ref($1, OP_ASLICE)));
			  if ($$ && $1)
			      $$->op_private |=
				  $1->op_private & OPpSLICEWARNING;
			}
	|	kvslice '[' expr ']'                 /* array key/value slice */
			{ $$ = op_prepend_elem(OP_KVASLICE,
				newOP(OP_PUSHMARK, 0),
				    newLISTOP(OP_KVASLICE, 0,
					list($3),
					ref(oopsAV($1), OP_KVASLICE)));
			  if ($$ && $1)
			      $$->op_private |=
				  $1->op_private & OPpSLICEWARNING;
			}
	|	sliceme '{' expr ';' '}'                 /* @hash{@keys} */
			{ $$ = op_prepend_elem(OP_HSLICE,
				newOP(OP_PUSHMARK, 0),
				    newLISTOP(OP_HSLICE, 0,
					list($3),
					ref(oopsHV($1), OP_HSLICE)));
			  if ($$ && $1)
			      $$->op_private |=
				  $1->op_private & OPpSLICEWARNING;
			}
	|	kvslice '{' expr ';' '}'                 /* %hash{@keys} */
			{ $$ = op_prepend_elem(OP_KVHSLICE,
				newOP(OP_PUSHMARK, 0),
				    newLISTOP(OP_KVHSLICE, 0,
					list($3),
					ref($1, OP_KVHSLICE)));
			  if ($$ && $1)
			      $$->op_private |=
				  $1->op_private & OPpSLICEWARNING;
			}
	|	THING	%prec '('
			{ $$ = $1; }
	|	amper                                /* &foo; */
			{ $$ = newUNOP(OP_ENTERSUB, 0, scalar($1)); }
	|	amper '(' ')'                 /* &foo() or foo() */
			{ $$ = newUNOP(OP_ENTERSUB, OPf_STACKED, scalar($1));
			}
	|	amper '(' expr ')'          /* &foo(@args) or foo(@args) */
			{
			  $$ = newUNOP(OP_ENTERSUB, OPf_STACKED,
				op_append_elem(OP_LIST, $3, scalar($1)));
			}
	|	NOAMP subname optlistexpr       /* foo @args (no parens) */
			{ $$ = newUNOP(OP_ENTERSUB, OPf_STACKED,
			    op_append_elem(OP_LIST, $3, scalar($2)));
			}
	|	term ARROW '$' '*'
			{ $$ = newSVREF($1); }
	|	term ARROW '@' '*'
			{ $$ = newAVREF($1); }
	|	term ARROW '%' '*'
			{ $$ = newHVREF($1); }
	|	term ARROW '&' '*'
			{ $$ = newUNOP(OP_ENTERSUB, 0,
				       scalar(newCVREF($3,$1))); }
	|	term ARROW '*' '*'	%prec '('
			{ $$ = newGVREF(0,$1); }
	|	LOOPEX  /* loop exiting command (goto, last, dump, etc) */
			{ $$ = newOP($1, OPf_SPECIAL);
			    PL_hints |= HINT_BLOCK_SCOPE; }
	|	LOOPEX term
			{ $$ = newLOOPEX($1,$2); }
	|	NOTOP listexpr                       /* not $foo */
			{ $$ = newUNOP(OP_NOT, 0, scalar($2)); }
	|	UNIOP                                /* Unary op, $_ implied */
			{ $$ = newOP($1, 0); }
	|	UNIOP block                          /* eval { foo }* */
			{ $$ = newUNOP($1, 0, $2); }
	|	UNIOP term                           /* Unary op */
			{ $$ = newUNOP($1, 0, $2); }
	|	REQUIRE                              /* require, $_ implied */
			{ $$ = newOP(OP_REQUIRE, $1 ? OPf_SPECIAL : 0); }
	|	REQUIRE term                         /* require Foo */
			{ $$ = newUNOP(OP_REQUIRE, $1 ? OPf_SPECIAL : 0, $2); }
	|	UNIOPSUB
			{ $$ = newUNOP(OP_ENTERSUB, OPf_STACKED, scalar($1)); }
	|	UNIOPSUB term                        /* Sub treated as unop */
			{ $$ = newUNOP(OP_ENTERSUB, OPf_STACKED,
			    op_append_elem(OP_LIST, $2, scalar($1))); }
	|	FUNC0                                /* Nullary operator */
			{ $$ = newOP($1, 0); }
	|	FUNC0 '(' ')'
			{ $$ = newOP($1, 0);}
	|	FUNC0OP       /* Same as above, but op created in toke.c */
			{ $$ = $1; }
	|	FUNC0OP '(' ')'
			{ $$ = $1; }
	|	FUNC0SUB                             /* Sub treated as nullop */
			{ $$ = newUNOP(OP_ENTERSUB, OPf_STACKED, scalar($1)); }
	|	FUNC1 '(' ')'                        /* not () */
			{ $$ = ($1 == OP_NOT)
                          ? newUNOP($1, 0, newSVOP(OP_CONST, 0, newSViv(0)))
                          : newOP($1, OPf_SPECIAL); }
	|	FUNC1 '(' expr ')'                   /* not($foo) */
			{ $$ = newUNOP($1, 0, $3); }
	|	PMFUNC /* m//, s///, qr//, tr/// */
			{
			    if (   $1->op_type != OP_TRANS
			        && $1->op_type != OP_TRANSR
				&& (((PMOP*)$1)->op_pmflags & PMf_HAS_CV))
			    {
				$<ival>$ = start_subparse(FALSE, CVf_ANON);
				SAVEFREESV(PL_compcv);
			    } else
				$<ival>$ = 0;
			}
		    '(' listexpr optrepl ')'
			{ $$ = pmruntime($1, $4, $5, 1, $<ival>2); }
	|	BAREWORD
	|	listop
	|	PLUGEXPR
	;

/* "my" declarations, with optional attributes */
myattrterm:	MY myterm myattrlist
			{ $$ = my_attrs($2,$3); }
	|	MY myterm
			{ $$ = localize($2,1); }
	|	MY REFGEN myterm myattrlist
			{ $$ = newUNOP(OP_REFGEN, 0, my_attrs($3,$4)); }
	;

/* Things that can be "my"'d */
myterm	:	'(' expr ')'
			{ $$ = sawparens($2); }
	|	'(' ')'
			{ $$ = sawparens(newNULLLIST()); }

	|	scalar	%prec '('
			{ $$ = $1; }
	|	hsh 	%prec '('
			{ $$ = $1; }
	|	ary 	%prec '('
			{ $$ = $1; }
	;

/* Basic list expressions */
optlistexpr:	/* NULL */ %prec PREC_LOW
			{ $$ = NULL; }
	|	listexpr    %prec PREC_LOW
			{ $$ = $1; }
	;

optexpr:	/* NULL */
			{ $$ = NULL; }
	|	expr
			{ $$ = $1; }
	;

optrepl:	/* NULL */
			{ $$ = NULL; }
	|	'/' expr
			{ $$ = $2; }
	;

/* A little bit of trickery to make "for my $foo (@bar)" actually be
   lexical */
my_scalar:	scalar
			{ parser->in_my = 0; $$ = my($1); }
	;

my_var	:	scalar
	|	ary
	|	hsh
	;

refgen_topic:	my_var
	|	amper
	;

my_refgen:	MY REFGEN
	|	REFGEN MY
	;

amper	:	'&' indirob
			{ $$ = newCVREF($1,$2); }
	;

scalar	:	'$' indirob
			{ $$ = newSVREF($2); }
	;

ary	:	'@' indirob
			{ $$ = newAVREF($2);
			  if ($$) $$->op_private |= $1;
			}
	;

hsh	:	'%' indirob
			{ $$ = newHVREF($2);
			  if ($$) $$->op_private |= $1;
			}
	;

arylen	:	DOLSHARP indirob
			{ $$ = newAVREF($2); }
	|	term ARROW DOLSHARP '*'
			{ $$ = newAVREF($1); }
	;

star	:	'*' indirob
			{ $$ = newGVREF(0,$2); }
	;

sliceme	:	ary
	|	term ARROW '@'
			{ $$ = newAVREF($1); }
	;

kvslice	:	hsh
	|	term ARROW '%'
			{ $$ = newHVREF($1); }
	;

gelem	:	star
	|	term ARROW '*'
			{ $$ = newGVREF(0,$1); }
	;

/* Indirect objects */
indirob	:	BAREWORD
			{ $$ = scalar($1); }
	|	scalar %prec PREC_LOW
			{ $$ = scalar($1); }
	|	block
			{ $$ = op_scope($1); }

	|	PRIVATEREF
			{ $$ = $1; }
	;
