/*    perly.y
 *
 *    Copyright (c) 1991-2002, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 * 'I see,' laughed Strider.  'I look foul and feel fair.  Is that it?
 * All that is gold does not glitter, not all those who wander are lost.'
 */

/* This file holds the grammar for the Perl language. If edited, you need
 * to run regen_perly.pl, which re-creates the files perly.h, perly.tab
 * and perly.act which are derived from this.
 *
 * The main job of of this grammar is to call the various newFOO()
 * functions in op.c to build a syntax tree of OP structs.
 * It relies on the lexer in toke.c to do the tokenizing.
 */

%{
#include "EXTERN.h"
#define PERL_IN_PERLY_C
#include "perl.h"
#ifdef EBCDIC
#undef YYDEBUG
#endif
#define dep() deprecate("\"do\" to call subroutines")

/* stuff included here to make perly_c.diff apply better */

#define yydebug	    PL_yydebug
#define yynerrs	    PL_yynerrs
#define yyerrflag   PL_yyerrflag
#define yychar	    PL_yychar
#define yyval	    PL_yyval
#define yylval	    PL_yylval

struct ysv {
    short* yyss;
    YYSTYPE* yyvs;
    int oldyydebug;
    int oldyynerrs;
    int oldyyerrflag;
    int oldyychar;
    YYSTYPE oldyyval;
    YYSTYPE oldyylval;
};

static void yydestruct(pTHX_ void *ptr);

%}

%start prog

%{
#if 0 /* get this from perly.h instead */
%}

%union {
    I32	ival;
    char *pval;
    OP *opval;
    GV *gvval;
}

%{
#endif /* 0 */

#ifdef USE_PURE_BISON
#define YYLEX_PARAM (&yychar)
#define yylex yylex_r
#endif

%}

%token <ival> '{'

%token <opval> WORD METHOD FUNCMETH THING PMFUNC PRIVATEREF
%token <opval> FUNC0SUB UNIOPSUB LSTOPSUB
%token <pval> LABEL
%token <ival> FORMAT SUB ANONSUB PACKAGE USE
%token <ival> WHILE UNTIL IF UNLESS ELSE ELSIF CONTINUE FOR
%token <ival> LOOPEX DOTDOT
%token <ival> FUNC0 FUNC1 FUNC UNIOP LSTOP
%token <ival> RELOP EQOP MULOP ADDOP
%token <ival> DOLSHARP DO HASHBRACK NOAMP
%token <ival> LOCAL MY MYSUB
%token COLONATTR

%type <ival> prog decl format startsub startanonsub startformsub
%type <ival> progstart remember mremember '&'
%type <opval> block mblock lineseq line loop cond else
%type <opval> expr term subscripted scalar ary hsh arylen star amper sideff
%type <opval> argexpr nexpr texpr iexpr mexpr mnexpr mtexpr miexpr
%type <opval> listexpr listexprcom indirob listop method
%type <opval> formname subname proto subbody cont my_scalar
%type <opval> subattrlist myattrlist mysubrout myattrterm myterm
%type <opval> termbinop termunop anonymous termdo
%type <pval> label

%nonassoc PREC_LOW
%nonassoc LOOPEX

%left <ival> OROP
%left ANDOP
%right NOTOP
%nonassoc LSTOP LSTOPSUB
%left ','
%right <ival> ASSIGNOP
%right '?' ':'
%nonassoc DOTDOT
%left OROR
%left ANDAND
%left <ival> BITOROP
%left <ival> BITANDOP
%nonassoc EQOP
%nonassoc RELOP
%nonassoc UNIOP UNIOPSUB
%left <ival> SHIFTOP
%left ADDOP
%left MULOP
%left <ival> MATCHOP
%right '!' '~' UMINUS REFGEN
%right <ival> POWOP
%nonassoc PREINC PREDEC POSTINC POSTDEC
%left ARROW
%nonassoc <ival> ')'
%left '('
%left '[' '{'

%% /* RULES */

/* The whole program */
prog	:	progstart
	/*CONTINUED*/	lineseq
			{ $$ = $1; newPROG(block_end($1,$2)); }
	;

/* An ordinary block */
block	:	'{' remember lineseq '}'
			{ if (PL_copline > (line_t)$1)
			      PL_copline = (line_t)$1;
			  $$ = block_end($2, $3); }
	;

remember:	/* NULL */	/* start a full lexical scope */
			{ $$ = block_start(TRUE); }
	;

progstart:
		{
#if defined(YYDEBUG) && defined(DEBUGGING)
		    yydebug = (DEBUG_p_TEST);
#endif
		    PL_expect = XSTATE; $$ = block_start(TRUE);
		}
	;


mblock	:	'{' mremember lineseq '}'
			{ if (PL_copline > (line_t)$1)
			      PL_copline = (line_t)$1;
			  $$ = block_end($2, $3); }
	;

mremember:	/* NULL */	/* start a partial lexical scope */
			{ $$ = block_start(FALSE); }
	;

/* A collection of "lines" in the program */
lineseq	:	/* NULL */
			{ $$ = Nullop; }
	|	lineseq decl
			{ $$ = $1; }
	|	lineseq line
			{   $$ = append_list(OP_LINESEQ,
				(LISTOP*)$1, (LISTOP*)$2);
			    PL_pad_reset_pending = TRUE;
			    if ($1 && $2) PL_hints |= HINT_BLOCK_SCOPE; }
	;

/* A "line" in the program */
line	:	label cond
			{ $$ = newSTATEOP(0, $1, $2); }
	|	loop	/* loops add their own labels */
	|	label ';'
			{ if ($1 != Nullch) {
			      $$ = newSTATEOP(0, $1, newOP(OP_NULL, 0));
			    }
			    else {
			      $$ = Nullop;
			      PL_copline = NOLINE;
			    }
			    PL_expect = XSTATE; }
	|	label sideff ';'
			{ $$ = newSTATEOP(0, $1, $2);
			  PL_expect = XSTATE; }
	;

/* An expression which may have a side-effect */
sideff	:	error
			{ $$ = Nullop; }
	|	expr
			{ $$ = $1; }
	|	expr IF expr
			{ $$ = newLOGOP(OP_AND, 0, $3, $1); }
	|	expr UNLESS expr
			{ $$ = newLOGOP(OP_OR, 0, $3, $1); }
	|	expr WHILE expr
			{ $$ = newLOOPOP(OPf_PARENS, 1, scalar($3), $1); }
	|	expr UNTIL iexpr
			{ $$ = newLOOPOP(OPf_PARENS, 1, $3, $1);}
	|	expr FOR expr
			{ $$ = newFOROP(0, Nullch, (line_t)$2,
					Nullop, $3, $1, Nullop); }
	;

/* else and elsif blocks */
else	:	/* NULL */
			{ $$ = Nullop; }
	|	ELSE mblock
			{ ($2)->op_flags |= OPf_PARENS; $$ = scope($2); }
	|	ELSIF '(' mexpr ')' mblock else
			{ PL_copline = (line_t)$1;
			    $$ = newCONDOP(0, $3, scope($5), $6);
			    PL_hints |= HINT_BLOCK_SCOPE; }
	;

/* Real conditional expressions */
cond	:	IF '(' remember mexpr ')' mblock else
			{ PL_copline = (line_t)$1;
			    $$ = block_end($3,
				   newCONDOP(0, $4, scope($6), $7)); }
	|	UNLESS '(' remember miexpr ')' mblock else
			{ PL_copline = (line_t)$1;
			    $$ = block_end($3,
				   newCONDOP(0, $4, scope($6), $7)); }
	;

/* Continue blocks */
cont	:	/* NULL */
			{ $$ = Nullop; }
	|	CONTINUE block
			{ $$ = scope($2); }
	;

/* Loops: while, until, for, and a bare block */
loop	:	label WHILE '(' remember mtexpr ')' mblock cont
			{ PL_copline = (line_t)$2;
			    $$ = block_end($4,
				   newSTATEOP(0, $1,
				     newWHILEOP(0, 1, (LOOP*)Nullop,
						$2, $5, $7, $8))); }
	|	label UNTIL '(' remember miexpr ')' mblock cont
			{ PL_copline = (line_t)$2;
			    $$ = block_end($4,
				   newSTATEOP(0, $1,
				     newWHILEOP(0, 1, (LOOP*)Nullop,
						$2, $5, $7, $8))); }
	|	label FOR MY remember my_scalar '(' mexpr ')' mblock cont
			{ $$ = block_end($4,
				 newFOROP(0, $1, (line_t)$2, $5, $7, $9, $10)); }
	|	label FOR scalar '(' remember mexpr ')' mblock cont
			{ $$ = block_end($5,
				 newFOROP(0, $1, (line_t)$2, mod($3, OP_ENTERLOOP),
					  $6, $8, $9)); }
	|	label FOR '(' remember mexpr ')' mblock cont
			{ $$ = block_end($4,
				 newFOROP(0, $1, (line_t)$2, Nullop, $5, $7, $8)); }
	|	label FOR '(' remember mnexpr ';' mtexpr ';' mnexpr ')' mblock
			/* basically fake up an initialize-while lineseq */
			{ OP *forop;
			  PL_copline = (line_t)$2;
			  forop = newSTATEOP(0, $1,
					    newWHILEOP(0, 1, (LOOP*)Nullop,
						$2, scalar($7),
						$11, $9));
			  if ($5) {
				forop = append_elem(OP_LINESEQ,
                                        newSTATEOP(0, ($1?savepv($1):Nullch),
						   $5),
					forop);
			  }

			  $$ = block_end($4, forop); }
	|	label block cont  /* a block is a loop that happens once */
			{ $$ = newSTATEOP(0, $1,
				 newWHILEOP(0, 1, (LOOP*)Nullop,
					    NOLINE, Nullop, $2, $3)); }
	;

/* Normal expression */
nexpr	:	/* NULL */
			{ $$ = Nullop; }
	|	sideff
	;

/* Boolean expression */
texpr	:	/* NULL means true */
			{ (void)scan_num("1", &yylval); $$ = yylval.opval; }
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

mtexpr	:	texpr
			{ $$ = $1; intro_my(); }
	;

miexpr	:	iexpr
			{ $$ = $1; intro_my(); }
	;

/* Optional "MAIN:"-style loop labels */
label	:	/* empty */
			{ $$ = Nullch; }
	|	LABEL
	;

/* Some kind of declaration - does not take part in the parse tree */
decl	:	format
			{ $$ = 0; }
	|	subrout
			{ $$ = 0; }
	|	mysubrout
			{ $$ = 0; }
	|	package
			{ $$ = 0; }
	|	use
			{ $$ = 0; }
	;

format	:	FORMAT startformsub formname block
			{ newFORM($2, $3, $4); }
	;

formname:	WORD		{ $$ = $1; }
	|	/* NULL */	{ $$ = Nullop; }
	;

/* Unimplemented "my sub foo { }" */
mysubrout:	MYSUB startsub subname proto subattrlist subbody
			{ newMYSUB($2, $3, $4, $5, $6); }
	;

/* Subroutine definition */
subrout	:	SUB startsub subname proto subattrlist subbody
			{ newATTRSUB($2, $3, $4, $5, $6); }
	;

startsub:	/* NULL */	/* start a regular subroutine scope */
			{ $$ = start_subparse(FALSE, 0); }
	;

startanonsub:	/* NULL */	/* start an anonymous subroutine scope */
			{ $$ = start_subparse(FALSE, CVf_ANON); }
	;

startformsub:	/* NULL */	/* start a format subroutine scope */
			{ $$ = start_subparse(TRUE, 0); }
	;

/* Name of a subroutine - must be a bareword, could be special */
subname	:	WORD	{ STRLEN n_a; char *name = SvPV(((SVOP*)$1)->op_sv,n_a);
			  if (strEQ(name, "BEGIN") || strEQ(name, "END")
			      || strEQ(name, "INIT") || strEQ(name, "CHECK"))
			      CvSPECIAL_on(PL_compcv);
			  $$ = $1; }
	;

/* Subroutine prototype */
proto	:	/* NULL */
			{ $$ = Nullop; }
	|	THING
	;

/* Optional list of subroutine attributes */
subattrlist:	/* NULL */
			{ $$ = Nullop; }
	|	COLONATTR THING
			{ $$ = $2; }
	|	COLONATTR
			{ $$ = Nullop; }
	;

/* List of attributes for a "my" variable declaration */
myattrlist:	COLONATTR THING
			{ $$ = $2; }
	|	COLONATTR
			{ $$ = Nullop; }
	;

/* Subroutine body - either null or a block */
subbody	:	block	{ $$ = $1; }
	|	';'	{ $$ = Nullop; PL_expect = XSTATE; }
	;

package :	PACKAGE WORD ';'
			{ package($2); }
	|	PACKAGE ';'
			{ package(Nullop); }
	;

use	:	USE startsub
			{ CvSPECIAL_on(PL_compcv); /* It's a BEGIN {} */ }
		    WORD WORD listexpr ';'
			{ utilize($1, $2, $4, $5, $6); }
	;

/* Ordinary expressions; logical combinations */
expr	:	expr ANDOP expr
			{ $$ = newLOGOP(OP_AND, 0, $1, $3); }
	|	expr OROP expr
			{ $$ = newLOGOP($2, 0, $1, $3); }
	|	argexpr %prec PREC_LOW
	;

/* Expressions are a list of terms joined by commas */
argexpr	:	argexpr ','
			{ $$ = $1; }
	|	argexpr ',' term
			{ $$ = append_elem(OP_LIST, $1, $3); }
	|	term %prec PREC_LOW
	;

/* List operators */
listop	:	LSTOP indirob argexpr          /* print $fh @args */
			{ $$ = convert($1, OPf_STACKED,
				prepend_elem(OP_LIST, newGVREF($1,$2), $3) ); }
	|	FUNC '(' indirob expr ')'      /* print ($fh @args */
			{ $$ = convert($1, OPf_STACKED,
				prepend_elem(OP_LIST, newGVREF($1,$3), $4) ); }
	|	term ARROW method '(' listexprcom ')' /* $foo->bar(list) */
			{ $$ = convert(OP_ENTERSUB, OPf_STACKED,
				append_elem(OP_LIST,
				    prepend_elem(OP_LIST, scalar($1), $5),
				    newUNOP(OP_METHOD, 0, $3))); }
	|	term ARROW method                     /* $foo->bar */
			{ $$ = convert(OP_ENTERSUB, OPf_STACKED,
				append_elem(OP_LIST, scalar($1),
				    newUNOP(OP_METHOD, 0, $3))); }
	|	METHOD indirob listexpr              /* new Class @args */
			{ $$ = convert(OP_ENTERSUB, OPf_STACKED,
				append_elem(OP_LIST,
				    prepend_elem(OP_LIST, $2, $3),
				    newUNOP(OP_METHOD, 0, $1))); }
	|	FUNCMETH indirob '(' listexprcom ')' /* method $object (@args) */
			{ $$ = convert(OP_ENTERSUB, OPf_STACKED,
				append_elem(OP_LIST,
				    prepend_elem(OP_LIST, $2, $4),
				    newUNOP(OP_METHOD, 0, $1))); }
	|	LSTOP listexpr                       /* print @args */
			{ $$ = convert($1, 0, $2); }
	|	FUNC '(' listexprcom ')'             /* print (@args) */
			{ $$ = convert($1, 0, $3); }
	|	LSTOPSUB startanonsub block          /* map { foo } ... */
			{ $3 = newANONATTRSUB($2, 0, Nullop, $3); }
		    listexpr		%prec LSTOP  /* ... @bar */
			{ $$ = newUNOP(OP_ENTERSUB, OPf_STACKED,
				 append_elem(OP_LIST,
				   prepend_elem(OP_LIST, $3, $5), $1)); }
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
			    PL_expect = XOPERATOR; }
	|	scalar '[' expr ']'          /* $array[$element] */
			{ $$ = newBINOP(OP_AELEM, 0, oopsAV($1), scalar($3)); }
	|	term ARROW '[' expr ']'      /* somearef->[$element] */
			{ $$ = newBINOP(OP_AELEM, 0,
					ref(newAVREF($1),OP_RV2AV),
					scalar($4));}
	|	subscripted '[' expr ']'    /* $foo->[$bar]->[$baz] */
			{ $$ = newBINOP(OP_AELEM, 0,
					ref(newAVREF($1),OP_RV2AV),
					scalar($3));}
	|	scalar '{' expr ';' '}'    /* $foo->{bar();} */
			{ $$ = newBINOP(OP_HELEM, 0, oopsHV($1), jmaybe($3));
			    PL_expect = XOPERATOR; }
	|	term ARROW '{' expr ';' '}' /* somehref->{bar();} */
			{ $$ = newBINOP(OP_HELEM, 0,
					ref(newHVREF($1),OP_RV2HV),
					jmaybe($4));
			    PL_expect = XOPERATOR; }
	|	subscripted '{' expr ';' '}' /* $foo->[bar]->{baz;} */
			{ $$ = newBINOP(OP_HELEM, 0,
					ref(newHVREF($1),OP_RV2HV),
					jmaybe($3));
			    PL_expect = XOPERATOR; }
	|	term ARROW '(' ')'          /* $subref->() */
			{ $$ = newUNOP(OP_ENTERSUB, OPf_STACKED,
				   newCVREF(0, scalar($1))); }
	|	term ARROW '(' expr ')'     /* $subref->(@args) */
			{ $$ = newUNOP(OP_ENTERSUB, OPf_STACKED,
				   append_elem(OP_LIST, $4,
				       newCVREF(0, scalar($1)))); }

	|	subscripted '(' expr ')'   /* $foo->{bar}->(@args) */
			{ $$ = newUNOP(OP_ENTERSUB, OPf_STACKED,
				   append_elem(OP_LIST, $3,
					       newCVREF(0, scalar($1)))); }
	|	subscripted '(' ')'        /* $foo->{bar}->() */
			{ $$ = newUNOP(OP_ENTERSUB, OPf_STACKED,
				   newCVREF(0, scalar($1))); }
    ;

/* Binary operators between terms */
termbinop	:	term ASSIGNOP term             /* $x = $y */
			{ $$ = newASSIGNOP(OPf_STACKED, $1, $2, $3); }
	|	term POWOP term                        /* $x ** $y */
			{ $$ = newBINOP($2, 0, scalar($1), scalar($3)); }
	|	term MULOP term                        /* $x * $y, $x x $y */
			{   if ($2 != OP_REPEAT)
				scalar($1);
			    $$ = newBINOP($2, 0, $1, scalar($3)); }
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
			{ $$ = newRANGE($2, scalar($1), scalar($3));}
	|	term ANDAND term                       /* $x && $y */
			{ $$ = newLOGOP(OP_AND, 0, $1, $3); }
	|	term OROR term                         /* $x || $y */
			{ $$ = newLOGOP(OP_OR, 0, $1, $3); }
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
			{ $$ = newUNOP(OP_COMPLEMENT, 0, scalar($2));}
	|	term POSTINC                           /* $x++ */
			{ $$ = newUNOP(OP_POSTINC, 0,
					mod(scalar($1), OP_POSTINC)); }
	|	term POSTDEC                           /* $x-- */
			{ $$ = newUNOP(OP_POSTDEC, 0,
					mod(scalar($1), OP_POSTDEC)); }
	|	PREINC term                            /* ++$x */
			{ $$ = newUNOP(OP_PREINC, 0,
					mod(scalar($2), OP_PREINC)); }
	|	PREDEC term                            /* --$x */
			{ $$ = newUNOP(OP_PREDEC, 0,
					mod(scalar($2), OP_PREDEC)); }

    ;

/* Constructors for anonymous data */
anonymous:	'[' expr ']'
			{ $$ = newANONLIST($2); }
	|	'[' ']'
			{ $$ = newANONLIST(Nullop); }
	|	HASHBRACK expr ';' '}'	%prec '(' /* { foo => "Bar" } */
			{ $$ = newANONHASH($2); }
	|	HASHBRACK ';' '}'	%prec '(' /* { } (';' by tokener) */
			{ $$ = newANONHASH(Nullop); }
	|	ANONSUB startanonsub proto subattrlist block	%prec '('
			{ $$ = newANONATTRSUB($2, $3, $4, $5); }

    ;

/* Things called with "do" */
termdo	:       DO term	%prec UNIOP                     /* do $filename */
			{ $$ = dofile($2); }
	|	DO block	%prec '('               /* do { code */
			{ $$ = newUNOP(OP_NULL, OPf_SPECIAL, scope($2)); }
	|	DO WORD '(' ')'                         /* do somesub() */
			{ $$ = newUNOP(OP_ENTERSUB,
			    OPf_SPECIAL|OPf_STACKED,
			    prepend_elem(OP_LIST,
				scalar(newCVREF(
				    (OPpENTERSUB_AMPER<<8),
				    scalar($2)
				)),Nullop)); dep();}
	|	DO WORD '(' expr ')'                    /* do somesub(@args) */
			{ $$ = newUNOP(OP_ENTERSUB,
			    OPf_SPECIAL|OPf_STACKED,
			    append_elem(OP_LIST,
				$4,
				scalar(newCVREF(
				    (OPpENTERSUB_AMPER<<8),
				    scalar($2)
				)))); dep();}
	|	DO scalar '(' ')'                      /* do $subref () */
			{ $$ = newUNOP(OP_ENTERSUB, OPf_SPECIAL|OPf_STACKED,
			    prepend_elem(OP_LIST,
				scalar(newCVREF(0,scalar($2))), Nullop)); dep();}
	|	DO scalar '(' expr ')'                 /* do $subref (@args) */
			{ $$ = newUNOP(OP_ENTERSUB, OPf_SPECIAL|OPf_STACKED,
			    prepend_elem(OP_LIST,
				$4,
				scalar(newCVREF(0,scalar($2))))); dep();}

        ;

term	:	termbinop
	|	termunop
	|	anonymous
	|	termdo
	|	term '?' term ':' term
			{ $$ = newCONDOP(0, $1, $3, $5); }
	|	REFGEN term                          /* \$x, \@y, \%z */
			{ $$ = newUNOP(OP_REFGEN, 0, mod($2,OP_REFGEN)); }
	|	myattrterm	%prec UNIOP
			{ $$ = $1; }
	|	LOCAL term	%prec UNIOP
			{ $$ = localize($2,$1); }
	|	'(' expr ')'
			{ $$ = sawparens($2); }
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
	|	'(' expr ')' '[' expr ']'            /* list slice */
			{ $$ = newSLICEOP(0, $5, $2); }
	|	'(' ')' '[' expr ']'                 /* empty list slice! */
			{ $$ = newSLICEOP(0, $4, Nullop); }
	|	ary '[' expr ']'                     /* array slice */
			{ $$ = prepend_elem(OP_ASLICE,
				newOP(OP_PUSHMARK, 0),
				    newLISTOP(OP_ASLICE, 0,
					list($3),
					ref($1, OP_ASLICE))); }
	|	ary '{' expr ';' '}'                 /* @hash{@keys} */
			{ $$ = prepend_elem(OP_HSLICE,
				newOP(OP_PUSHMARK, 0),
				    newLISTOP(OP_HSLICE, 0,
					list($3),
					ref(oopsHV($1), OP_HSLICE)));
			    PL_expect = XOPERATOR; }
	|	THING	%prec '('
			{ $$ = $1; }
	|	amper                                /* &foo; */
			{ $$ = newUNOP(OP_ENTERSUB, 0, scalar($1)); }
	|	amper '(' ')'                        /* &foo() */
			{ $$ = newUNOP(OP_ENTERSUB, OPf_STACKED, scalar($1)); }
	|	amper '(' expr ')'                   /* &foo(@args) */
			{ $$ = newUNOP(OP_ENTERSUB, OPf_STACKED,
			    append_elem(OP_LIST, $3, scalar($1))); }
	|	NOAMP WORD listexpr                  /* foo(@args) */
			{ $$ = newUNOP(OP_ENTERSUB, OPf_STACKED,
			    append_elem(OP_LIST, $3, scalar($2))); }
	|	LOOPEX  /* loop exiting command (goto, last, dump, etc) */
			{ $$ = newOP($1, OPf_SPECIAL);
			    PL_hints |= HINT_BLOCK_SCOPE; }
	|	LOOPEX term
			{ $$ = newLOOPEX($1,$2); }
	|	NOTOP argexpr                        /* not $foo */
			{ $$ = newUNOP(OP_NOT, 0, scalar($2)); }
	|	UNIOP                                /* Unary op, $_ implied */
			{ $$ = newOP($1, 0); }
	|	UNIOP block                          /* eval { foo }, I *think* */
			{ $$ = newUNOP($1, 0, $2); }
	|	UNIOP term                           /* Unary op */
			{ $$ = newUNOP($1, 0, $2); }
	|	UNIOPSUB term                        /* Sub treated as unop */
			{ $$ = newUNOP(OP_ENTERSUB, OPf_STACKED,
			    append_elem(OP_LIST, $2, scalar($1))); }
	|	FUNC0                                /* Nullary operator */
			{ $$ = newOP($1, 0); }
	|	FUNC0 '(' ')'
			{ $$ = newOP($1, 0); }
	|	FUNC0SUB                             /* Sub treated as nullop */
			{ $$ = newUNOP(OP_ENTERSUB, OPf_STACKED,
				scalar($1)); }
	|	FUNC1 '(' ')'                        /* not () */
			{ $$ = newOP($1, OPf_SPECIAL); }
	|	FUNC1 '(' expr ')'                   /* not($foo) */
			{ $$ = newUNOP($1, 0, $3); }
	|	PMFUNC '(' term ')'                  /* split (/foo/) */
			{ $$ = pmruntime($1, $3, Nullop); }
	|	PMFUNC '(' term ',' term ')'         /* split (/foo/,$bar) */
			{ $$ = pmruntime($1, $3, $5); }
	|	WORD
	|	listop
	;

/* "my" declarations, with optional attributes */
myattrterm:	MY myterm myattrlist
			{ $$ = my_attrs($2,$3); }
	|	MY myterm
			{ $$ = localize($2,$1); }
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
listexpr:	/* NULL */ %prec PREC_LOW
			{ $$ = Nullop; }
	|	argexpr    %prec PREC_LOW
			{ $$ = $1; }
	;

listexprcom:	/* NULL */
			{ $$ = Nullop; }
	|	expr
			{ $$ = $1; }
	|	expr ','
			{ $$ = $1; }
	;

/* A little bit of trickery to make "for my $foo (@bar)" actually be
   lexical */
my_scalar:	scalar
			{ PL_in_my = 0; $$ = my($1); }
	;

amper	:	'&' indirob
			{ $$ = newCVREF($1,$2); }
	;

scalar	:	'$' indirob
			{ $$ = newSVREF($2); }
	;

ary	:	'@' indirob
			{ $$ = newAVREF($2); }
	;

hsh	:	'%' indirob
			{ $$ = newHVREF($2); }
	;

arylen	:	DOLSHARP indirob
			{ $$ = newAVREF($2); }
	;

star	:	'*' indirob
			{ $$ = newGVREF(0,$2); }
	;

/* Indirect objects */
indirob	:	WORD
			{ $$ = scalar($1); }
	|	scalar %prec PREC_LOW
			{ $$ = scalar($1);  }
	|	block
			{ $$ = scope($1); }

	|	PRIVATEREF
			{ $$ = $1; }
	;

%% /* PROGRAM */

/* more stuff added to make perly_c.diff easier to apply */

#ifdef yyparse
#undef yyparse
#endif
#define yyparse() Perl_yyparse(pTHX)

