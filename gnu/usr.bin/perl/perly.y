/*    perly.y
 *
 *    Copyright (c) 1991-1994, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 * 'I see,' laughed Strider.  'I look foul and feel fair.  Is that it?
 * All that is gold does not glitter, not all those that wander are lost.'
 */

%{
#include "EXTERN.h"
#include "perl.h"

static void
dep()
{
    deprecate("\"do\" to call subroutines");
}

%}

%start prog

%union {
    I32	ival;
    char *pval;
    OP *opval;
    GV *gvval;
}

%token <ival> '{' ')'

%token <opval> WORD METHOD FUNCMETH THING PMFUNC PRIVATEREF
%token <opval> FUNC0SUB UNIOPSUB LSTOPSUB
%token <pval> LABEL
%token <ival> FORMAT SUB ANONSUB PACKAGE USE
%token <ival> WHILE UNTIL IF UNLESS ELSE ELSIF CONTINUE FOR
%token <ival> LOOPEX DOTDOT
%token <ival> FUNC0 FUNC1 FUNC
%token <ival> RELOP EQOP MULOP ADDOP
%token <ival> DOLSHARP DO LOCAL HASHBRACK NOAMP

%type <ival> prog decl format remember startsub '&'
%type <opval> block lineseq line loop cond nexpr else argexpr
%type <opval> expr term scalar ary hsh arylen star amper sideff
%type <opval> listexpr listexprcom indirob
%type <opval> texpr listop method proto
%type <pval> label
%type <opval> cont

%left <ival> OROP
%left ANDOP
%right NOTOP
%nonassoc <ival> LSTOP
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
%nonassoc <ival> UNIOP
%left <ival> SHIFTOP
%left ADDOP
%left MULOP
%left <ival> MATCHOP
%right '!' '~' UMINUS REFGEN
%right <ival> POWOP
%nonassoc PREINC PREDEC POSTINC POSTDEC
%left ARROW
%left '('

%% /* RULES */

prog	:	/* NULL */
		{
#if defined(YYDEBUG) && defined(DEBUGGING)
		    yydebug = (debug & 1);
#endif
		    expect = XSTATE;
		}
	/*CONTINUED*/	lineseq
			{ newPROG($2); }
	;

block	:	'{' remember lineseq '}'
			{ $$ = block_end($1,$2,$3); }
	;

remember:	/* NULL */	/* start a lexical scope */
			{ $$ = block_start(); }
	;

lineseq	:	/* NULL */
			{ $$ = Nullop; }
	|	lineseq decl
			{ $$ = $1; }
	|	lineseq line
			{   $$ = append_list(OP_LINESEQ,
				(LISTOP*)$1, (LISTOP*)$2);
			    pad_reset_pending = TRUE;
			    if ($1 && $2) hints |= HINT_BLOCK_SCOPE; }
	;

line	:	label cond
			{ $$ = newSTATEOP(0, $1, $2); }
	|	loop	/* loops add their own labels */
	|	label ';'
			{ if ($1 != Nullch) {
			      $$ = newSTATEOP(0, $1, newOP(OP_NULL, 0));
			    }
			    else {
			      $$ = Nullop;
			      copline = NOLINE;
			    }
			    expect = XSTATE; }
	|	label sideff ';'
			{ $$ = newSTATEOP(0, $1, $2);
			  expect = XSTATE; }
	;

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
	|	expr UNTIL expr
			{ $$ = newLOOPOP(OPf_PARENS, 1, invert(scalar($3)), $1);}
	;

else	:	/* NULL */
			{ $$ = Nullop; }
	|	ELSE block
			{ $$ = scope($2); }
	|	ELSIF '(' expr ')' block else
			{ copline = $1;
			    $$ = newSTATEOP(0, 0,
				newCONDOP(0, $3, scope($5), $6));
			    hints |= HINT_BLOCK_SCOPE; }
	;

cond	:	IF '(' expr ')' block else
			{ copline = $1;
			    $$ = newCONDOP(0, $3, scope($5), $6); }
	|	UNLESS '(' expr ')' block else
			{ copline = $1;
			    $$ = newCONDOP(0,
				invert(scalar($3)), scope($5), $6); }
	|	IF block block else
			{ copline = $1;
			    deprecate("if BLOCK BLOCK");
			    $$ = newCONDOP(0, scope($2), scope($3), $4); }
	|	UNLESS block block else
			{ copline = $1;
			    deprecate("unless BLOCK BLOCK");
			    $$ = newCONDOP(0, invert(scalar(scope($2))),
						scope($3), $4); }
	;

cont	:	/* NULL */
			{ $$ = Nullop; }
	|	CONTINUE block
			{ $$ = scope($2); }
	;

loop	:	label WHILE '(' texpr ')' block cont
			{ copline = $2;
			    $$ = newSTATEOP(0, $1,
				    newWHILEOP(0, 1, (LOOP*)Nullop,
					$4, $6, $7) ); }
	|	label UNTIL '(' expr ')' block cont
			{ copline = $2;
			    $$ = newSTATEOP(0, $1,
				    newWHILEOP(0, 1, (LOOP*)Nullop,
					invert(scalar($4)), $6, $7) ); }
	|	label WHILE block block cont
			{ copline = $2;
			    $$ = newSTATEOP(0, $1,
				    newWHILEOP(0, 1, (LOOP*)Nullop,
					scope($3), $4, $5) ); }
	|	label UNTIL block block cont
			{ copline = $2;
			    $$ = newSTATEOP(0, $1,
				    newWHILEOP(0, 1, (LOOP*)Nullop,
					invert(scalar(scope($3))), $4, $5)); }
	|	label FOR scalar '(' expr ')' block cont
			{ $$ = newFOROP(0, $1, $2, mod($3, OP_ENTERLOOP),
				$5, $7, $8); }
	|	label FOR '(' expr ')' block cont
			{ $$ = newFOROP(0, $1, $2, Nullop, $4, $6, $7); }
	|	label FOR '(' nexpr ';' texpr ';' nexpr ')' block
			/* basically fake up an initialize-while lineseq */
			{  copline = $2;
			    $$ = append_elem(OP_LINESEQ,
				    newSTATEOP(0, $1, scalar($4)),
				    newSTATEOP(0, $1,
					newWHILEOP(0, 1, (LOOP*)Nullop,
					    scalar($6), $10, scalar($8)) )); }
	|	label block cont  /* a block is a loop that happens once */
			{ $$ = newSTATEOP(0,
				$1, newWHILEOP(0, 1, (LOOP*)Nullop,
					Nullop, $2, $3)); }
	;

nexpr	:	/* NULL */
			{ $$ = Nullop; }
	|	sideff
	;

texpr	:	/* NULL means true */
			{ (void)scan_num("1"); $$ = yylval.opval; }
	|	expr
	;

label	:	/* empty */
			{ $$ = Nullch; }
	|	LABEL
	;

decl	:	format
			{ $$ = 0; }
	|	subrout
			{ $$ = 0; }
	|	package
			{ $$ = 0; }
	|	use
			{ $$ = 0; }
	;

format	:	FORMAT startsub WORD block
			{ newFORM($2, $3, $4); }
	|	FORMAT startsub block
			{ newFORM($2, Nullop, $3); }
	;

subrout	:	SUB startsub WORD proto block
			{ newSUB($2, $3, $4, $5); }
	|	SUB startsub WORD proto ';'
			{ newSUB($2, $3, $4, Nullop); expect = XSTATE; }
	;

proto	:	/* NULL */
			{ $$ = Nullop; }
	|	THING
	;
		
startsub:	/* NULL */	/* start a subroutine scope */
			{ $$ = start_subparse(); }
	;

package :	PACKAGE WORD ';'
			{ package($2); }
	|	PACKAGE ';'
			{ package(Nullop); }
	;

use	:	USE startsub WORD listexpr ';'
			{ utilize($1, $2, $3, $4); }
	;

expr	:	expr ANDOP expr
			{ $$ = newLOGOP(OP_AND, 0, $1, $3); }
	|	expr OROP expr
			{ $$ = newLOGOP($2, 0, $1, $3); }
	|	argexpr
	;

argexpr	:	argexpr ','
			{ $$ = $1; }
	|	argexpr ',' term
			{ $$ = append_elem(OP_LIST, $1, $3); }
	|	term
	;

listop	:	LSTOP indirob argexpr
			{ $$ = convert($1, OPf_STACKED,
				prepend_elem(OP_LIST, newGVREF($1,$2), $3) ); }
	|	FUNC '(' indirob expr ')'
			{ $$ = convert($1, OPf_STACKED,
				prepend_elem(OP_LIST, newGVREF($1,$3), $4) ); }
	|	term ARROW method '(' listexprcom ')'
			{ $$ = convert(OP_ENTERSUB, OPf_STACKED,
				append_elem(OP_LIST,
				    prepend_elem(OP_LIST, $1, $5),
				    newUNOP(OP_METHOD, 0, $3))); }
	|	METHOD indirob listexpr
			{ $$ = convert(OP_ENTERSUB, OPf_STACKED,
				append_elem(OP_LIST,
				    prepend_elem(OP_LIST, $2, $3),
				    newUNOP(OP_METHOD, 0, $1))); }
	|	FUNCMETH indirob '(' listexprcom ')'
			{ $$ = convert(OP_ENTERSUB, OPf_STACKED,
				append_elem(OP_LIST,
				    prepend_elem(OP_LIST, $2, $4),
				    newUNOP(OP_METHOD, 0, $1))); }
	|	LSTOP listexpr
			{ $$ = convert($1, 0, $2); }
	|	FUNC '(' listexprcom ')'
			{ $$ = convert($1, 0, $3); }
	|	LSTOPSUB startsub block listexpr	%prec LSTOP
			{ $$ = newUNOP(OP_ENTERSUB, OPf_STACKED,
			    append_elem(OP_LIST,
			      prepend_elem(OP_LIST, newANONSUB($2, 0, $3), $4),
			      $1)); }
	;

method	:	METHOD
	|	scalar
	;

term	:	term ASSIGNOP term
			{ $$ = newASSIGNOP(OPf_STACKED, $1, $2, $3); }
	|	term POWOP term
			{ $$ = newBINOP($2, 0, scalar($1), scalar($3)); }
	|	term MULOP term
			{   if ($2 != OP_REPEAT)
				scalar($1);
			    $$ = newBINOP($2, 0, $1, scalar($3)); }
	|	term ADDOP term
			{ $$ = newBINOP($2, 0, scalar($1), scalar($3)); }
	|	term SHIFTOP term
			{ $$ = newBINOP($2, 0, scalar($1), scalar($3)); }
	|	term RELOP term
			{ $$ = newBINOP($2, 0, scalar($1), scalar($3)); }
	|	term EQOP term
			{ $$ = newBINOP($2, 0, scalar($1), scalar($3)); }
	|	term BITANDOP term
			{ $$ = newBINOP($2, 0, scalar($1), scalar($3)); }
	|	term BITOROP term
			{ $$ = newBINOP($2, 0, scalar($1), scalar($3)); }
	|	term DOTDOT term
			{ $$ = newRANGE($2, scalar($1), scalar($3));}
	|	term ANDAND term
			{ $$ = newLOGOP(OP_AND, 0, $1, $3); }
	|	term OROR term
			{ $$ = newLOGOP(OP_OR, 0, $1, $3); }
	|	term '?' term ':' term
			{ $$ = newCONDOP(0, $1, $3, $5); }
	|	term MATCHOP term
			{ $$ = bind_match($2, $1, $3); }

	|	'-' term %prec UMINUS
			{ $$ = newUNOP(OP_NEGATE, 0, scalar($2)); }
	|	'+' term %prec UMINUS
			{ $$ = $2; }
	|	'!' term
			{ $$ = newUNOP(OP_NOT, 0, scalar($2)); }
	|	'~' term
			{ $$ = newUNOP(OP_COMPLEMENT, 0, scalar($2));}
	|	REFGEN term
			{ $$ = newUNOP(OP_REFGEN, 0, mod($2,OP_REFGEN)); }
	|	term POSTINC
			{ $$ = newUNOP(OP_POSTINC, 0,
					mod(scalar($1), OP_POSTINC)); }
	|	term POSTDEC
			{ $$ = newUNOP(OP_POSTDEC, 0,
					mod(scalar($1), OP_POSTDEC)); }
	|	PREINC term
			{ $$ = newUNOP(OP_PREINC, 0,
					mod(scalar($2), OP_PREINC)); }
	|	PREDEC term
			{ $$ = newUNOP(OP_PREDEC, 0,
					mod(scalar($2), OP_PREDEC)); }
	|	LOCAL term	%prec UNIOP
			{ $$ = localize($2,$1); }
	|	'(' expr ')'
			{ $$ = sawparens($2); }
	|	'(' ')'
			{ $$ = sawparens(newNULLLIST()); }
	|	'[' expr ']'				%prec '('
			{ $$ = newANONLIST($2); }
	|	'[' ']'					%prec '('
			{ $$ = newANONLIST(Nullop); }
	|	HASHBRACK expr ';' '}'			%prec '('
			{ $$ = newANONHASH($2); }
	|	HASHBRACK ';' '}'				%prec '('
			{ $$ = newANONHASH(Nullop); }
	|	ANONSUB startsub proto block			%prec '('
			{ $$ = newANONSUB($2, $3, $4); }
	|	scalar	%prec '('
			{ $$ = $1; }
	|	star '{' expr ';' '}'
			{ $$ = newBINOP(OP_GELEM, 0, newGVREF(0,$1), $3); }
	|	star	%prec '('
			{ $$ = $1; }
	|	scalar '[' expr ']'	%prec '('
			{ $$ = newBINOP(OP_AELEM, 0, oopsAV($1), scalar($3)); }
	|	term ARROW '[' expr ']'	%prec '('
			{ $$ = newBINOP(OP_AELEM, 0,
					ref(newAVREF($1),OP_RV2AV),
					scalar($4));}
	|	term '[' expr ']'	%prec '('
			{ assertref($1); $$ = newBINOP(OP_AELEM, 0,
					ref(newAVREF($1),OP_RV2AV),
					scalar($3));}
	|	hsh 	%prec '('
			{ $$ = $1; }
	|	ary 	%prec '('
			{ $$ = $1; }
	|	arylen 	%prec '('
			{ $$ = newUNOP(OP_AV2ARYLEN, 0, ref($1, OP_AV2ARYLEN));}
	|	scalar '{' expr ';' '}'	%prec '('
			{ $$ = newBINOP(OP_HELEM, 0, oopsHV($1), jmaybe($3));
			    expect = XOPERATOR; }
	|	term ARROW '{' expr ';' '}'	%prec '('
			{ $$ = newBINOP(OP_HELEM, 0,
					ref(newHVREF($1),OP_RV2HV),
					jmaybe($4));
			    expect = XOPERATOR; }
	|	term '{' expr ';' '}'	%prec '('
			{ assertref($1); $$ = newBINOP(OP_HELEM, 0,
					ref(newHVREF($1),OP_RV2HV),
					jmaybe($3));
			    expect = XOPERATOR; }
	|	'(' expr ')' '[' expr ']'	%prec '('
			{ $$ = newSLICEOP(0, $5, $2); }
	|	'(' ')' '[' expr ']'	%prec '('
			{ $$ = newSLICEOP(0, $4, Nullop); }
	|	ary '[' expr ']'	%prec '('
			{ $$ = prepend_elem(OP_ASLICE,
				newOP(OP_PUSHMARK, 0),
				    newLISTOP(OP_ASLICE, 0,
					list($3),
					ref($1, OP_ASLICE))); }
	|	ary '{' expr ';' '}'	%prec '('
			{ $$ = prepend_elem(OP_HSLICE,
				newOP(OP_PUSHMARK, 0),
				    newLISTOP(OP_HSLICE, 0,
					list($3),
					ref(oopsHV($1), OP_HSLICE)));
			    expect = XOPERATOR; }
	|	THING	%prec '('
			{ $$ = $1; }
	|	amper
			{ $$ = newUNOP(OP_ENTERSUB, 0, scalar($1)); }
	|	amper '(' ')'
			{ $$ = newUNOP(OP_ENTERSUB, OPf_STACKED, scalar($1)); }
	|	amper '(' expr ')'
			{ $$ = newUNOP(OP_ENTERSUB, OPf_STACKED,
			    append_elem(OP_LIST, $3, scalar($1))); }
	|	NOAMP WORD listexpr
			{ $$ = newUNOP(OP_ENTERSUB, OPf_STACKED,
			    append_elem(OP_LIST, $3, scalar($2))); }
	|	DO term	%prec UNIOP
			{ $$ = newUNOP(OP_DOFILE, 0, scalar($2)); }
	|	DO block	%prec '('
			{ $$ = newUNOP(OP_NULL, OPf_SPECIAL, scope($2)); }
	|	DO WORD '(' ')'
			{ $$ = newUNOP(OP_ENTERSUB,
			    OPf_SPECIAL|OPf_STACKED,
			    prepend_elem(OP_LIST,
				scalar(newCVREF(
				    (OPpENTERSUB_AMPER<<8),
				    scalar($2)
				)),Nullop)); dep();}
	|	DO WORD '(' expr ')'
			{ $$ = newUNOP(OP_ENTERSUB,
			    OPf_SPECIAL|OPf_STACKED,
			    append_elem(OP_LIST,
				$4,
				scalar(newCVREF(
				    (OPpENTERSUB_AMPER<<8),
				    scalar($2)
				)))); dep();}
	|	DO scalar '(' ')'
			{ $$ = newUNOP(OP_ENTERSUB, OPf_SPECIAL|OPf_STACKED,
			    prepend_elem(OP_LIST,
				scalar(newCVREF(0,scalar($2))), Nullop)); dep();}
	|	DO scalar '(' expr ')'
			{ $$ = newUNOP(OP_ENTERSUB, OPf_SPECIAL|OPf_STACKED,
			    prepend_elem(OP_LIST,
				$4,
				scalar(newCVREF(0,scalar($2))))); dep();}
	|	LOOPEX
			{ $$ = newOP($1, OPf_SPECIAL);
			    hints |= HINT_BLOCK_SCOPE; }
	|	LOOPEX term
			{ $$ = newLOOPEX($1,$2); }
	|	NOTOP argexpr
			{ $$ = newUNOP(OP_NOT, 0, scalar($2)); }
	|	UNIOP
			{ $$ = newOP($1, 0); }
	|	UNIOP block
			{ $$ = newUNOP($1, 0, $2); }
	|	UNIOP term
			{ $$ = newUNOP($1, 0, $2); }
	|	UNIOPSUB term
			{ $$ = newUNOP(OP_ENTERSUB, OPf_STACKED,
			    append_elem(OP_LIST, $2, scalar($1))); }
	|	FUNC0
			{ $$ = newOP($1, 0); }
	|	FUNC0 '(' ')'
			{ $$ = newOP($1, 0); }
	|	FUNC0SUB
			{ $$ = newUNOP(OP_ENTERSUB, 0,
				scalar($1)); }
	|	FUNC1 '(' ')'
			{ $$ = newOP($1, OPf_SPECIAL); }
	|	FUNC1 '(' expr ')'
			{ $$ = newUNOP($1, 0, $3); }
	|	PMFUNC '(' term ')'
			{ $$ = pmruntime($1, $3, Nullop); }
	|	PMFUNC '(' term ',' term ')'
			{ $$ = pmruntime($1, $3, $5); }
	|	WORD
	|	listop
	;

listexpr:	/* NULL */
			{ $$ = Nullop; }
	|	argexpr
			{ $$ = $1; }
	;

listexprcom:	/* NULL */
			{ $$ = Nullop; }
	|	expr
			{ $$ = $1; }
	|	expr ','
			{ $$ = $1; }
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

indirob	:	WORD
			{ $$ = scalar($1); }
	|	scalar
			{ $$ = scalar($1);  }
	|	block
			{ $$ = scope($1); }

	|	PRIVATEREF
			{ $$ = $1; }
	;

%% /* PROGRAM */
