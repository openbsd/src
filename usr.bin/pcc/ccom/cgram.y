/*	$OpenBSD: cgram.y,v 1.10 2008/08/17 18:40:12 ragge Exp $	*/

/*
 * Copyright (c) 2003 Anders Magnusson (ragge@ludd.luth.se).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright(C) Caldera International Inc. 2001-2002. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * Redistributions of source code and documentation must retain the above
 * copyright notice, this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 * 	This product includes software developed or owned by Caldera
 *	International, Inc.
 * Neither the name of Caldera International, Inc. nor the names of other
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * USE OF THE SOFTWARE PROVIDED FOR UNDER THIS LICENSE BY CALDERA
 * INTERNATIONAL, INC. AND CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL CALDERA INTERNATIONAL, INC. BE LIABLE
 * FOR ANY DIRECT, INDIRECT INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OFLIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Comments for this grammar file. Ragge 021123
 *
 * ANSI support required rewrite of the function header and declaration
 * rules almost totally.
 *
 * The lex/yacc shared keywords are now split from the keywords used
 * in the rest of the compiler, to simplify use of other frontends.
 */

/*
 * At last count, there were 3 shift/reduce and no reduce/reduce conflicts
 * Two was funct_idn and the third was "dangling else".
 */

/*
 * Token used in C lex/yacc communications.
 */
%token	C_STRING	/* a string constant */
%token	C_ICON		/* an integer constant */
%token	C_FCON		/* a floating point constant */
%token	C_NAME		/* an identifier */
%token	C_TYPENAME	/* a typedef'd name */
%token	C_ANDAND	/* && */
%token	C_OROR		/* || */
%token	C_GOTO		/* unconditional goto */
%token	C_RETURN	/* return from function */
%token	C_TYPE		/* a type */
%token	C_CLASS		/* a storage class */
%token	C_ASOP		/* assignment ops */
%token	C_RELOP		/* <=, <, >=, > */
%token	C_EQUOP		/* ==, != */
%token	C_DIVOP		/* /, % */
%token	C_SHIFTOP	/* <<, >> */
%token	C_INCOP		/* ++, -- */
%token	C_UNOP		/* !, ~ */
%token	C_STROP		/* ., -> */
%token	C_STRUCT
%token	C_IF
%token	C_ELSE
%token	C_SWITCH
%token	C_BREAK
%token	C_CONTINUE
%token	C_WHILE	
%token	C_DO
%token	C_FOR
%token	C_DEFAULT
%token	C_CASE
%token	C_SIZEOF
%token	C_ENUM
%token	C_ELLIPSIS
%token	C_QUALIFIER
%token	C_FUNSPEC
%token	C_ASM
%token	NOMATCH

/*
 * Precedence
 */
%left ','
%right '=' C_ASOP
%right '?' ':'
%left C_OROR
%left C_ANDAND
%left '|'
%left '^'
%left '&'
%left C_EQUOP
%left C_RELOP
%left C_SHIFTOP
%left '+' '-'
%left '*' C_DIVOP
%right C_UNOP
%right C_INCOP C_SIZEOF
%left '[' '(' C_STROP
%{
# include "pass1.h"
# include <stdarg.h>
# include <string.h>
# include <stdlib.h>

int fun_inline;	/* Reading an inline function */
int oldstyle;	/* Current function being defined */
static struct symtab *xnf;
extern int enummer, tvaloff;
extern struct rstack *rpole;
static int ctval, widestr;

#define	NORETYP	SNOCREAT /* no return type, save in unused field in symtab */

static NODE *bdty(int op, ...);
static void fend(void);
static void fundef(NODE *tp, NODE *p);
static void olddecl(NODE *p);
static struct symtab *init_declarator(NODE *tn, NODE *p, int assign);
static void resetbc(int mask);
static void swend(void);
static void addcase(NODE *p);
static void adddef(void);
static void savebc(void);
static void swstart(int, TWORD);
static void genswitch(int, TWORD, struct swents **, int);
static NODE *structref(NODE *p, int f, char *name);
static char *mkpstr(char *str);
static struct symtab *clbrace(NODE *);
static NODE *cmop(NODE *l, NODE *r);
static NODE *xcmop(NODE *out, NODE *in, NODE *str);
static void mkxasm(char *str, NODE *p);
static NODE *xasmop(char *str, NODE *p);
static int maxstlen(char *str);
static char *stradd(char *old, char *new);
static NODE *biop(int op, NODE *l, NODE *r);
static void flend(void);

/*
 * State for saving current switch state (when nested switches).
 */
struct savbc {
	struct savbc *next;
	int brklab;
	int contlab;
	int flostat;
	int swx;
} *savbc, *savctx;

%}

%union {
	int intval;
	NODE *nodep;
	struct symtab *symp;
	struct rstack *rp;
	char *strp;
}

	/* define types */
%start ext_def_list

%type <intval> con_e ifelprefix ifprefix whprefix forprefix doprefix switchpart
		type_qualifier_list
%type <nodep> e .e term enum_dcl struct_dcl cast_type funct_idn declarator
		direct_declarator elist type_specifier merge_attribs
		parameter_declaration abstract_declarator initializer
		parameter_type_list parameter_list addrlbl
		declaration_specifiers pointer direct_abstract_declarator
		specifier_qualifier_list merge_specifiers nocon_e
		identifier_list arg_param_list
		designator_list designator xasm oplist oper cnstr funtype
%type <strp>	string C_STRING
%type <rp>	str_head
%type <symp>	xnfdeclarator clbrace enum_head

%type <intval> C_CLASS C_STRUCT C_RELOP C_DIVOP C_SHIFTOP
		C_ANDAND C_OROR C_STROP C_INCOP C_UNOP C_ASOP C_EQUOP

%type <nodep>  C_TYPE C_QUALIFIER C_ICON C_FCON
%type <strp>	C_NAME C_TYPENAME

%%

ext_def_list:	   ext_def_list external_def
		| { ftnend(); }
		;

external_def:	   funtype kr_args compoundstmt { fend(); }
		|  declaration  { blevel = 0; symclear(0); }
		|  asmstatement ';'
		|  ';'
		|  error { blevel = 0; }
		;

funtype:	  /* no type given */ declarator {
		    fundef(mkty(INT, 0, MKSUE(INT)), $1);
		    cftnsp->sflags |= NORETYP;
		}
		| declaration_specifiers declarator { fundef($1,$2); }
		;

kr_args:	  /* empty */
		| arg_dcl_list
		;

/*
 * Returns a node pointer or NULL, if no types at all given.
 * Type trees are checked for correctness and merged into one
 * type node in typenode().
 */
declaration_specifiers:
		   merge_attribs { $$ = typenode($1); }
		;

merge_attribs:	   C_CLASS { $$ = block(CLASS, NIL, NIL, $1, 0, 0); }
		|  C_CLASS merge_attribs { $$ = block(CLASS, $2, NIL, $1,0,0);}
		|  type_specifier { $$ = $1; }
		|  type_specifier merge_attribs { $1->n_left = $2; $$ = $1; }
		|  C_QUALIFIER { $$ = $1; }
		|  C_QUALIFIER merge_attribs { $1->n_left = $2; $$ = $1; }
		|  function_specifiers { $$ = NIL; }
		|  function_specifiers merge_attribs { $$ = $2; }
		;

function_specifiers:
		   C_FUNSPEC { fun_inline = 1; }
		;

type_specifier:	   C_TYPE { $$ = $1; }
		|  C_TYPENAME { 
			struct symtab *sp = lookup($1, 0);
			$$ = mkty(sp->stype, sp->sdf, sp->ssue);
			$$->n_sp = sp;
		}
		|  struct_dcl { $$ = $1; }
		|  enum_dcl { $$ = $1; }
		;

/*
 * Adds a pointer list to front of the declarators.
 * Note the UMUL right node pointer usage.
 */
declarator:	   pointer direct_declarator {
			$$ = $1; $1->n_right->n_left = $2;
		}
		|  direct_declarator { $$ = $1; }
		;

/*
 * Return an UMUL node type linked list of indirections.
 */
pointer:	   '*' { $$ = bdty(UMUL, NIL); $$->n_right = $$; }
		|  '*' type_qualifier_list {
			$$ = bdty(UMUL, NIL);
			$$->n_qual = $2;
			$$->n_right = $$;
		}
		|  '*' pointer {
			$$ = bdty(UMUL, $2);
			$$->n_right = $2->n_right;
		}
		|  '*' type_qualifier_list pointer {
			$$ = bdty(UMUL, $3);
			$$->n_qual = $2;
			$$->n_right = $3->n_right;
		}
		;

type_qualifier_list:
		   C_QUALIFIER { $$ = $1->n_type; nfree($1); }
		|  type_qualifier_list C_QUALIFIER {
			$$ = $1 | $2->n_type; nfree($2);
		}
		;

/*
 * Sets up a function declarator. The call node will have its parameters
 * connected to its right node pointer.
 */
direct_declarator: C_NAME { $$ = bdty(NAME, $1); }
		|  '(' declarator ')' { $$ = $2; }
		|  direct_declarator '[' nocon_e ']' { 
			$3 = optim($3);
			if ((blevel == 0 || rpole != NULL) && !nncon($3))
				uerror("array size not constant");
			/*
			 * Checks according to 6.7.5.2 clause 1:
			 * "...the expression shall have an integer type."
			 * "If the expression is a constant expression,
			 * it shall have a value greater than zero."
			 */
			if (!ISINTEGER($3->n_type))
				werror("array size is not an integer");
			else if ($3->n_op == ICON) {
				if ($3->n_lval < 0) {
					uerror("array size cannot be negative");
					$3->n_lval = 1;
				}
#ifdef notyet
				if ($3->n_lval == 0 && Wgcc)
					werror("gcc extension; zero size");
#endif
			}
			$$ = biop(LB, $1, $3);
		}
		|  direct_declarator '[' ']' {
			$$ = biop(LB, $1, bcon(NOOFFSET));
		}
		|  direct_declarator '(' parameter_type_list ')' {
			$$ = bdty(CALL, $1, $3);
		}
		|  direct_declarator '(' identifier_list ')' { 
			$$ = bdty(CALL, $1, $3);
			if (blevel != 0)
				uerror("function declaration in bad context");
			oldstyle = 1;
		}
		|  direct_declarator '(' ')' {
			ctval = tvaloff;
			$$ = bdty(UCALL, $1);
		}
		;

identifier_list:   C_NAME { $$ = bdty(NAME, $1); }
		|  identifier_list ',' C_NAME { $$ = cmop($1, bdty(NAME, $3)); }
		;

/*
 * Returns as parameter_list, but can add an additional ELLIPSIS node.
 */
parameter_type_list:
		   parameter_list { $$ = $1; }
		|  parameter_list ',' C_ELLIPSIS {
			$$ = cmop($1, biop(ELLIPSIS, NIL, NIL));
		}
		;

/*
 * Returns a linked lists of nodes of op CM with parameters on
 * its right and additional CM nodes of its left pointer.
 * No CM nodes if only one parameter.
 */
parameter_list:	   parameter_declaration { $$ = $1; }
		|  parameter_list ',' parameter_declaration {
			$$ = cmop($1, $3);
		}
		;

/*
 * Returns a node pointer to the declaration.
 */
parameter_declaration:
		   declaration_specifiers declarator {
			if ($1->n_lval != SNULL && $1->n_lval != REGISTER)
				uerror("illegal parameter class");
			$$ = tymerge($1, $2);
			nfree($1);
		
		}
		|  declaration_specifiers abstract_declarator { 
			$$ = tymerge($1, $2);
			nfree($1);
		}
		|  declaration_specifiers {
			$$ = tymerge($1, bdty(NAME, NULL));
			nfree($1);
		}
		;

abstract_declarator:
		   pointer { $$ = $1; $1->n_right->n_left = bdty(NAME, NULL); }
		|  direct_abstract_declarator { $$ = $1; }
		|  pointer direct_abstract_declarator { 
			$$ = $1; $1->n_right->n_left = $2;
		}
		;

direct_abstract_declarator:
		   '(' abstract_declarator ')' { $$ = $2; }
		|  '[' ']' { $$ = biop(LB, bdty(NAME, NULL), bcon(NOOFFSET)); }
		|  '[' con_e ']' { $$ = bdty(LB, bdty(NAME, NULL), $2); }
		|  direct_abstract_declarator '[' ']' {
			$$ = biop(LB, $1, bcon(NOOFFSET));
		}
		|  direct_abstract_declarator '[' con_e ']' {
			$$ = bdty(LB, $1, $3);
		}
		|  '(' ')' { $$ = bdty(UCALL, bdty(NAME, NULL)); }
		|  '(' parameter_type_list ')' {
			$$ = bdty(CALL, bdty(NAME, NULL), $2);
		}
		|  direct_abstract_declarator '(' ')' {
			$$ = bdty(UCALL, $1);
		}
		|  direct_abstract_declarator '(' parameter_type_list ')' {
			$$ = bdty(CALL, $1, $3);
		}
		;

/*
 * K&R arg declaration, between ) and {
 */
arg_dcl_list:	   arg_declaration
		|  arg_dcl_list arg_declaration
		;


arg_declaration:   declaration_specifiers arg_param_list ';' {
			nfree($1);
		}
		;

arg_param_list:	   declarator { olddecl(tymerge($<nodep>0, $1)); }
		|  arg_param_list ',' declarator {
			olddecl(tymerge($<nodep>0, $3));
		}
		;

/*
 * Declarations in beginning of blocks.
 */
block_item_list:   block_item
		|  block_item_list block_item
		;

block_item:	   declaration
		|  statement
		;

/*
 * Here starts the old YACC code.
 */

/*
 * Variables are declared in init_declarator.
 */
declaration:	   declaration_specifiers ';' { nfree($1); fun_inline = 0; }
		|  declaration_specifiers init_declarator_list ';' {
			nfree($1);
			fun_inline = 0;
		}
		;

/*
 * Normal declaration of variables. curtype contains the current type node.
 * Returns nothing, variables are declared in init_declarator.
 */
init_declarator_list:
		   init_declarator
		|  init_declarator_list ',' { $<nodep>$ = $<nodep>0; } init_declarator
		;

enum_dcl:	   enum_head '{' moe_list optcomma '}' { $$ = enumdcl($1); }
		|  C_ENUM C_NAME {  $$ = enumref($2); }
		;

enum_head:	   C_ENUM { $$ = enumhd(NULL); }
		|  C_ENUM C_NAME {  $$ = enumhd($2); }
		;

moe_list:	   moe
		|  moe_list ',' moe
		;

moe:		   C_NAME {  moedef($1); }
		|  C_TYPENAME {  moedef($1); }
		|  C_NAME '=' con_e { enummer = $3; moedef($1); }
		|  C_TYPENAME '=' con_e { enummer = $3; moedef($1); }
		;

struct_dcl:	   str_head '{' struct_dcl_list '}' empty {
			$$ = dclstruct($1); 
		}
		|  C_STRUCT C_NAME {  $$ = rstruct($2,$1); }
		|  str_head '{' '}' {
#ifndef GCC_COMPAT
			werror("gcc extension");
#endif
			$$ = dclstruct($1); 
		}
		;

empty:		   { /* Get yacc read the next token before reducing */ }
		|  NOMATCH
		;

str_head:	   C_STRUCT {  $$ = bstruct(NULL, $1);  }
		|  C_STRUCT C_NAME {  $$ = bstruct($2,$1);  }
		;

struct_dcl_list:   struct_declaration
		|  struct_dcl_list struct_declaration
		;

struct_declaration:
		   specifier_qualifier_list struct_declarator_list ';' {
			nfree($1);
		}
		;

specifier_qualifier_list:
		   merge_specifiers { $$ = typenode($1); }
		;

merge_specifiers:  type_specifier merge_specifiers { $1->n_left = $2;$$ = $1; }
		|  type_specifier { $$ = $1; }
		|  C_QUALIFIER merge_specifiers { $1->n_left = $2; $$ = $1; }
		|  C_QUALIFIER { $$ = $1; }
		;

struct_declarator_list:
		   struct_declarator { }
		|  struct_declarator_list ',' { $<nodep>$=$<nodep>0; } 
			struct_declarator { }
		;

struct_declarator: declarator {
			tymerge($<nodep>0, $1);
			soumemb($1, (char *)$1->n_sp, 0);
			nfree($1);
		}
		|  ':' con_e {
			if (fldchk($2))
				$2 = 1;
			falloc(NULL, $2, -1, $<nodep>0);
		}
		|  declarator ':' con_e {
			if (fldchk($3))
				$3 = 1;
			if ($1->n_op == NAME) {
				tymerge($<nodep>0, $1);
				soumemb($1, (char *)$1->n_sp, FIELD | $3);
				nfree($1);
			} else
				uerror("illegal declarator");
		}
		;

		/* always preceeded by attributes */
xnfdeclarator:	   declarator { $$ = xnf = init_declarator($<nodep>0, $1, 1); }
		;

/*
 * Handles declarations and assignments.
 * Returns nothing.
 */
init_declarator:   declarator { init_declarator($<nodep>0, $1, 0); }
		|  declarator C_ASM '(' string ')' {
#ifdef GCC_COMPAT
			pragma_renamed = newstring($4, strlen($4));
			init_declarator($<nodep>0, $1, 0);
#else
			werror("gcc extension");
			init_declarator($<nodep>0, $1, 0);
#endif
		}
		|  xnfdeclarator '=' e { simpleinit($1, $3); xnf = NULL; }
		|  xnfdeclarator '=' begbr init_list optcomma '}' {
			endinit();
			xnf = NULL;
		}
		|  xnfdeclarator '=' addrlbl { simpleinit($1, $3); xnf = NULL; }
		;

begbr:		   '{' { beginit($<symp>-1); }
		;

initializer:	   e %prec ',' {  $$ = $1; }
		|  addrlbl {  $$ = $1; }
		|  ibrace init_list optcomma '}' { $$ = NULL; }
		;

init_list:	   designation initializer { asginit($2); }
		|  init_list ','  designation initializer { asginit($4); }
		;

designation:	   designator_list '=' { desinit($1); }
		|  { /* empty */ }
		;

designator_list:   designator { $$ = $1; }
		|  designator_list designator { $$ = $2; $$->n_left = $1; }
		;

designator:	   '[' con_e ']' {
			if ($2 < 0) {
				uerror("designator must be non-negative");
				$2 = 0;
			}
			$$ = biop(LB, NIL, bcon($2));
		}
		|  C_STROP C_NAME {
			if ($1 != DOT)
				uerror("invalid designator");
			$$ = bdty(NAME, $2);
		}
		;

optcomma	:	/* VOID */
		|  ','
		;

ibrace:		   '{' {  ilbrace(); }
		;

/*	STATEMENTS	*/

compoundstmt:	   begin block_item_list '}' { flend(); }
		|  begin '}' { flend(); }
		;

begin:		  '{' {
			struct savbc *bc = tmpalloc(sizeof(struct savbc));
			if (blevel == 1) {
#ifdef STABS
				if (gflag)
					stabs_line(lineno);
#endif
				dclargs();
			}
#ifdef STABS
			if (gflag && blevel > 1)
				stabs_lbrac(blevel+1);
#endif
			++blevel;
			oldstyle = 0;
			bc->contlab = autooff;
			bc->next = savctx;
			savctx = bc;
			bccode();
			if (sspflag && blevel == 2)
				sspstart();
		}
		;

statement:	   e ';' { ecomp( $1 ); symclear(blevel); }
		|  compoundstmt
		|  ifprefix statement { plabel($1); reached = 1; }
		|  ifelprefix statement {
			if ($1 != NOLAB) {
				plabel( $1);
				reached = 1;
			}
		}
		|  whprefix statement {
			branch(contlab);
			plabel( brklab );
			if( (flostat&FBRK) || !(flostat&FLOOP))
				reached = 1;
			else
				reached = 0;
			resetbc(0);
		}
		|  doprefix statement C_WHILE '(' e ')' ';' {
			plabel(contlab);
			if (flostat & FCONT)
				reached = 1;
			if (reached)
				cbranch($5, bcon($1));
			else
				tfree($5);
			plabel( brklab);
			reached = 1;
			resetbc(0);
		}
		|  forprefix .e ')' statement
			{  plabel( contlab );
			    if( flostat&FCONT ) reached = 1;
			    if( $2 ) ecomp( $2 );
			    branch($1);
			    plabel( brklab );
			    if( (flostat&FBRK) || !(flostat&FLOOP) ) reached = 1;
			    else reached = 0;
			    resetbc(0);
			    }
		| switchpart statement
			{ if( reached ) branch( brklab );
			    plabel( $1 );
			    swend();
			    plabel( brklab);
			    if( (flostat&FBRK) || !(flostat&FDEF) ) reached = 1;
			    resetbc(FCONT);
			    }
		|  C_BREAK  ';' {
			if (brklab == NOLAB)
				uerror("illegal break");
			else if (reached)
				branch(brklab);
			flostat |= FBRK;
			reached = 0;
		}
		|  C_CONTINUE  ';' {
			if (contlab == NOLAB)
				uerror("illegal continue");
			else
				branch(contlab);
			flostat |= FCONT;
			goto rch;
		}
		|  C_RETURN  ';' {
			branch(retlab);
			if (cftnsp->stype != VOID && 
			    (cftnsp->sflags & NORETYP) == 0 &&
			    cftnsp->stype != VOID+FTN)
				uerror("return value required");
			rch:
			if (!reached && Wunreachable_code)
				werror( "statement is not reached");
			reached = 0;
		}
		|  C_RETURN e  ';' {
			register NODE *temp;

			temp = nametree(cftnsp);
			temp->n_type = DECREF(temp->n_type);
			temp = buildtree(RETURN, temp, $2);

			if (temp->n_type == VOID)
				ecomp(temp->n_right);
			else
				ecomp(buildtree(FORCE, temp->n_right, NIL));
			tfree(temp->n_left);
			nfree(temp);
			branch(retlab);
			reached = 0;
		}
		|  C_GOTO C_NAME ';' { gotolabel($2); goto rch; }
		|  C_GOTO '*' e ';' { ecomp(biop(GOTO, $3, NIL)); }
		|  asmstatement ';'
		|   ';'
		|  error  ';'
		|  error '}'
		|  label statement
		;

asmstatement:	   C_ASM mvol '(' string ')' { send_passt(IP_ASM, mkpstr($4)); }
		|  C_ASM mvol '(' string xasm ')' { mkxasm($4, $5); }
		;

mvol:		   /* empty */
		|  C_QUALIFIER { nfree($1); }
		;

xasm:		   ':' oplist { $$ = xcmop($2, NIL, NIL); }
		|  ':' oplist ':' oplist { $$ = xcmop($2, $4, NIL); }
		|  ':' oplist ':' oplist ':' cnstr { $$ = xcmop($2, $4, $6); }
		;

oplist:		   /* nothing */ { $$ = NIL; }
		|  oper { $$ = $1; }
		;

oper:		   string '(' e ')' { $$ = xasmop($1, $3); }
		|  oper ',' string '(' e ')' { $$ = cmop($1, xasmop($3, $5)); }
		;

cnstr:		   string { $$ = xasmop($1, bcon(0)); }
		|  cnstr ',' string { $$ = cmop($1, xasmop($3, bcon(0))); }
                ;

label:		   C_NAME ':' { deflabel($1); reached = 1; }
		|  C_TYPENAME ':' { deflabel($1); reached = 1; }
		|  C_CASE e ':' { addcase($2); reached = 1; }
		|  C_DEFAULT ':' { reached = 1; adddef(); flostat |= FDEF; }
		;

doprefix:	C_DO {
			savebc();
			brklab = getlab();
			contlab = getlab();
			plabel(  $$ = getlab());
			reached = 1;
		}
		;
ifprefix:	C_IF '(' e ')' {
			cbranch(buildtree(NOT, $3, NIL), bcon($$ = getlab()));
			reached = 1;
		}
		;
ifelprefix:	  ifprefix statement C_ELSE {
			if (reached)
				branch($$ = getlab());
			else
				$$ = NOLAB;
			plabel( $1);
			reached = 1;
		}
		;

whprefix:	  C_WHILE  '('  e  ')' {
			savebc();
			if ($3->n_op == ICON && $3->n_lval != 0)
				flostat = FLOOP;
			plabel( contlab = getlab());
			reached = 1;
			brklab = getlab();
			if (flostat == FLOOP)
				tfree($3);
			else
				cbranch(buildtree(NOT, $3, NIL), bcon(brklab));
		}
		;
forprefix:	  C_FOR  '('  .e  ';' .e  ';' {
			if ($3)
				ecomp($3);
			savebc();
			contlab = getlab();
			brklab = getlab();
			plabel( $$ = getlab());
			reached = 1;
			if ($5)
				cbranch(buildtree(NOT, $5, NIL), bcon(brklab));
			else
				flostat |= FLOOP;
		}
		|  C_FOR '(' incblev declaration .e ';' {
			blevel--;
			savebc();
			contlab = getlab();
			brklab = getlab();
			plabel( $$ = getlab());
			reached = 1;
			if ($5)
				cbranch(buildtree(NOT, $5, NIL), bcon(brklab));
			else
				flostat |= FLOOP;
		}
		;

incblev:	   { blevel++; }
		;

switchpart:	   C_SWITCH  '('  e  ')' {
			NODE *p;
			int num;
			TWORD t;

			savebc();
			brklab = getlab();
			if (($3->n_type != BOOL && $3->n_type > ULONGLONG) ||
			    $3->n_type < CHAR) {
				uerror("switch expression must have integer "
				       "type");
				t = INT;
			} else {
				$3 = intprom($3);
				t = $3->n_type;
			}
			p = tempnode(0, t, 0, MKSUE(t));
			num = regno(p);
			ecomp(buildtree(ASSIGN, p, $3));
			branch( $$ = getlab());
			swstart(num, t);
			reached = 0;
		}
		;
/*	EXPRESSIONS	*/
con_e:		{ $<rp>$ = rpole; rpole = NULL; } e %prec ',' {
			$$ = icons($2);
			rpole = $<rp>1;
		}
		;

nocon_e:	{ $<rp>$ = rpole; rpole = NULL; } e %prec ',' {
			rpole = $<rp>1;
			$$ = $2;
		}
		;

.e:		   e
		| 	{ $$=0; }
		;

elist:		   e %prec ','
		|  elist  ','  e { $$ = buildtree(CM, $1, $3); }
		|  elist  ','  cast_type { /* hack for stdarg */
			$3->n_op = TYPE;
			$$ = buildtree(CM, $1, $3);
		}
		;

/*
 * Precedence order of operators.
 */
e:		   e ',' e { $$ = buildtree(COMOP, $1, $3); }
		|  e '=' e {  $$ = buildtree(ASSIGN, $1, $3); }
		|  e C_ASOP e {  $$ = buildtree($2, $1, $3); }
		|  e '?' e ':' e {
			$$=buildtree(QUEST, $1, buildtree(COLON, $3, $5));
		}
		|  e C_OROR e { $$ = buildtree($2, $1, $3); }
		|  e C_ANDAND e { $$ = buildtree($2, $1, $3); }
		|  e '|' e { $$ = buildtree(OR, $1, $3); }
		|  e '^' e { $$ = buildtree(ER, $1, $3); }
		|  e '&' e { $$ = buildtree(AND, $1, $3); }
		|  e C_EQUOP  e { $$ = buildtree($2, $1, $3); }
		|  e C_RELOP e { $$ = buildtree($2, $1, $3); }
		|  e C_SHIFTOP e { $$ = buildtree($2, $1, $3); }
		|  e '+' e { $$ = buildtree(PLUS, $1, $3); }
		|  e '-' e { $$ = buildtree(MINUS, $1, $3); }
		|  e C_DIVOP e { $$ = buildtree($2, $1, $3); }
		|  e '*' e { $$ = buildtree(MUL, $1, $3); }
		|  e '=' addrlbl { $$ = buildtree(ASSIGN, $1, $3); }
		|  '(' begin block_item_list e ';' '}' ')' { $$ = $4; flend(); }
		|  term
		;

addrlbl:	  C_ANDAND C_NAME {
#ifdef GCC_COMPAT
			struct symtab *s = lookup($2, SLBLNAME);
			if (s->soffset == 0)
				s->soffset = -getlab();
			$$ = buildtree(ADDROF, nametree(s), NIL);
#else
			uerror("gcc extension");
#endif
		}
		;

term:		   term C_INCOP {  $$ = buildtree( $2, $1, bcon(1) ); }
		|  '*' term { $$ = buildtree(UMUL, $2, NIL); }
		|  '&' term {
			if( ISFTN($2->n_type)/* || ISARY($2->n_type) */){
#ifdef notdef
				werror( "& before array or function: ignored" );
#endif
				$$ = $2;
			} else
				$$ = buildtree(ADDROF, $2, NIL);
		}
		|  '-' term { $$ = buildtree(UMINUS, $2, NIL ); }
		|  '+' term { $$ = $2; }
		|  C_UNOP term { $$ = buildtree( $1, $2, NIL ); }
		|  C_INCOP term {
			$$ = buildtree($1 == INCR ? PLUSEQ : MINUSEQ,
			    $2, bcon(1));
		}
		|  C_SIZEOF term { $$ = doszof($2); }
		|  '(' cast_type ')' term  %prec C_INCOP {
			register NODE *q;
			$$ = buildtree(CAST, $2, $4);
			nfree($$->n_left);
			q = $$->n_right;
			nfree($$);
			$$ = q;
		}
		|  C_SIZEOF '(' cast_type ')'  %prec C_SIZEOF {
			$$ = doszof($3);
		}
		| '(' cast_type ')' clbrace init_list optcomma '}' {
			endinit();
			$$ = nametree($4);
		}
		|  term '[' e ']' {
			$$ = buildtree( UMUL,
			    buildtree( PLUS, $1, $3 ), NIL );
		}
		|  funct_idn  ')' { $$ = doacall($1, NIL); }
		|  funct_idn elist ')' { $$ = doacall($1, $2); }
		|  term C_STROP C_NAME { $$ = structref($1, $2, $3); }
		|  term C_STROP C_TYPENAME { $$ = structref($1, $2, $3); }
		|  C_NAME {
			struct symtab *sp;

			sp = lookup($1, 0);
			if (sp->sflags & SINLINE)
				inline_ref(sp);
			$$ = nametree(sp);
			if (sp->sflags & SDYNARRAY)
				$$ = buildtree(UMUL, $$, NIL);
		}
		|  C_ICON { $$ = $1; }
		|  C_FCON { $$ = $1; }
		|  string {  $$ = strend(widestr, $1); }
		|   '('  e  ')' { $$=$2; }
		;

clbrace:	   '{'	{ $$ = clbrace($<nodep>-1); }
		;

string:		   C_STRING { widestr = $1[0] == 'L'; $$ = stradd("", $1); }
		|  string C_STRING { $$ = stradd($1, $2); }
		;

cast_type:	   specifier_qualifier_list {
			$$ = tymerge($1, bdty(NAME, NULL));
			nfree($1);
		}
		|  specifier_qualifier_list abstract_declarator {
			$$ = tymerge($1, $2);
			nfree($1);
		}
		;

funct_idn:	   C_NAME  '(' {
			struct symtab *s = lookup($1, 0);
			if (s->stype == UNDEF) {
				register NODE *q;
				q = block(NAME, NIL, NIL, FTN|INT, 0, MKSUE(INT));
				q->n_sp = s;
				defid(q, EXTERN);
				nfree(q);
			}
			if (s->sflags & SINLINE)
				inline_ref(s);
			$$ = nametree(s);
		}
		|  term  '(' 
		;
%%

NODE *
mkty(TWORD t, union dimfun *d, struct suedef *sue)
{
	return block(TYPE, NIL, NIL, t, d, sue);
}

static NODE *
bdty(int op, ...)
{
	va_list ap;
	int val;
	register NODE *q;

	va_start(ap, op);
	q = biop(op, NIL, NIL);

	switch (op) {
	case UMUL:
	case UCALL:
		q->n_left = va_arg(ap, NODE *);
		q->n_rval = 0;
		break;

	case CALL:
		q->n_left = va_arg(ap, NODE *);
		q->n_right = va_arg(ap, NODE *);
		break;

	case LB:
		q->n_left = va_arg(ap, NODE *);
		if ((val = va_arg(ap, int)) <= 0) {
			uerror("array size must be positive");
			val = 1;
		}
		q->n_right = bcon(val);
		break;

	case NAME:
		q->n_sp = va_arg(ap, struct symtab *); /* XXX survive tymerge */
		break;

	default:
		cerror("bad bdty");
	}
	va_end(ap);

	return q;
}

static void
flend(void)
{
	if (sspflag && blevel == 2)
		sspend();
#ifdef STABS
	if (gflag && blevel > 2)
		stabs_rbrac(blevel);
#endif
	--blevel;
	if( blevel == 1 )
		blevel = 0;
	symclear(blevel); /* Clean ut the symbol table */
	if (autooff > maxautooff)
		maxautooff = autooff;
	autooff = savctx->contlab;
	savctx = savctx->next;
}

static void
savebc(void)
{
	struct savbc *bc = tmpalloc(sizeof(struct savbc));

	bc->brklab = brklab;
	bc->contlab = contlab;
	bc->flostat = flostat;
	bc->next = savbc;
	savbc = bc;
	flostat = 0;
}

static void
resetbc(int mask)
{
	flostat = savbc->flostat | (flostat&mask);
	contlab = savbc->contlab;
	brklab = savbc->brklab;
	savbc = savbc->next;
}

struct swdef {
	struct swdef *next;	/* Next in list */
	int deflbl;		/* Label for "default" */
	struct swents *ents;	/* Linked sorted list of case entries */
	int nents;		/* # of entries in list */
	int num;		/* Node value will end up in */
	TWORD type;		/* Type of switch expression */
} *swpole;

/*
 * add case to switch
 */
static void
addcase(NODE *p)
{
	struct swents **put, *w, *sw = tmpalloc(sizeof(struct swents));
	CONSZ val;

	p = optim(p);  /* change enum to ints */
	if (p->n_op != ICON || p->n_sp != NULL) {
		uerror( "non-constant case expression");
		return;
	}
	if (swpole == NULL) {
		uerror("case not in switch");
		return;
	}

	if (DEUNSIGN(swpole->type) != DEUNSIGN(p->n_type)) {
		val = p->n_lval;
		p = makety(p, swpole->type, 0, 0, MKSUE(swpole->type));
		if (p->n_op != ICON)
			cerror("could not cast case value to type of switch "
			       "expression");
		if (p->n_lval != val)
			werror("case expression truncated");
	}
	sw->sval = p->n_lval;
	tfree(p);
	put = &swpole->ents;
	if (ISUNSIGNED(swpole->type)) {
		for (w = swpole->ents;
		     w != NULL && (U_CONSZ)w->sval < (U_CONSZ)sw->sval;
		     w = w->next)
			put = &w->next;
	} else {
		for (w = swpole->ents; w != NULL && w->sval < sw->sval;
		     w = w->next)
			put = &w->next;
	}
	if (w != NULL && w->sval == sw->sval) {
		uerror("duplicate case in switch");
		return;
	}
	plabel(sw->slab = getlab());
	*put = sw;
	sw->next = w;
	swpole->nents++;
}

/*
 * add default case to switch
 */
static void
adddef(void)
{
	if (swpole == NULL)
		uerror("default not inside switch");
	else if (swpole->deflbl != 0)
		uerror("duplicate default in switch");
	else
		plabel( swpole->deflbl = getlab());
}

static void
swstart(int num, TWORD type)
{
	struct swdef *sw = tmpalloc(sizeof(struct swdef));

	sw->deflbl = sw->nents = 0;
	sw->ents = NULL;
	sw->next = swpole;
	sw->num = num;
	sw->type = type;
	swpole = sw;
}

/*
 * end a switch block
 */
static void
swend(void)
{
	struct swents *sw, **swp;
	int i;

	sw = tmpalloc(sizeof(struct swents));
	swp = tmpalloc(sizeof(struct swents *) * (swpole->nents+1));

	sw->slab = swpole->deflbl;
	swp[0] = sw;

	for (i = 1; i <= swpole->nents; i++) {
		swp[i] = swpole->ents;
		swpole->ents = swpole->ents->next;
	}
	genswitch(swpole->num, swpole->type, swp, swpole->nents);

	swpole = swpole->next;
}

/*
 * num: tempnode the value of the switch expression is in
 * type: type of the switch expression
 *
 * p points to an array of structures, each consisting
 * of a constant value and a label.
 * The first is >=0 if there is a default label;
 * its value is the label number
 * The entries p[1] to p[n] are the nontrivial cases
 * n is the number of case statements (length of list)
 */
static void
genswitch(int num, TWORD type, struct swents **p, int n)
{
	NODE *r, *q;
	int i;

	if (mygenswitch(num, type, p, n))
		return;

	/* simple switch code */
	for (i = 1; i <= n; ++i) {
		/* already in 1 */
		r = tempnode(num, type, 0, MKSUE(type));
		q = xbcon(p[i]->sval, NULL, type);
		r = buildtree(NE, r, clocal(q));
		cbranch(buildtree(NOT, r, NIL), bcon(p[i]->slab));
	}
	if (p[0]->slab > 0)
		branch(p[0]->slab);
}

/*
 * Declare a variable or prototype.
 */
static struct symtab *
init_declarator(NODE *tn, NODE *p, int assign)
{
	int class = tn->n_lval;
	NODE *typ;

	typ = tymerge(tn, p);
	typ->n_sp = lookup((char *)typ->n_sp, 0); /* XXX */

	if (fun_inline && ISFTN(typ->n_type))
		typ->n_sp->sflags |= SINLINE;

	if (ISFTN(typ->n_type) == 0) {
		if (assign) {
			defid(typ, class);
			typ->n_sp->sflags |= SASG;
			if (typ->n_sp->sflags & SDYNARRAY)
				uerror("can't initialize dynamic arrays");
			lcommdel(typ->n_sp);
		} else {
			nidcl(typ, class);
		}
	} else {
		if (assign)
			uerror("cannot initialise function");
		defid(typ, uclass(class));
	}
	nfree(p);
	return typ->n_sp;
}

/*
 * Declare function arguments.
 */
static void
funargs(NODE *p)
{
	if (p->n_op == ELLIPSIS)
		return;
	if (oldstyle) {
		p->n_op = TYPE;
		p->n_type = FARG;
	}
	p->n_sp = lookup((char *)p->n_sp, 0);/* XXX */
	if (ISFTN(p->n_type))
		p->n_type = INCREF(p->n_type);
	defid(p, PARAM);
}

/*
 * Declare a function.
 */
static void
fundef(NODE *tp, NODE *p)
{
	extern int prolab;
	struct symtab *s;
	NODE *q = p;
	int class = tp->n_lval, oclass;
	char *c;

	for (q = p; coptype(q->n_op) != LTYPE && q->n_left->n_op != NAME;
	    q = q->n_left)
		;
	if (q->n_op != CALL && q->n_op != UCALL) {
		uerror("invalid function definition");
		p = bdty(UCALL, p);
	}

	argoff = ARGINIT;
	ctval = tvaloff;
	blevel++;

	if (q->n_op == CALL && q->n_right->n_type != VOID) {
		/* declare function arguments */
		listf(q->n_right, funargs);
		ftnarg(q);
	}

	blevel--;
	tymerge(tp, p);
	s = p->n_sp = lookup((char *)p->n_sp, 0); /* XXX */

	oclass = s->sclass;
	if (class == STATIC && oclass == EXTERN)
		werror("%s was first declared extern, then static", s->sname);

	if (fun_inline) {
		/* special syntax for inline functions */
		if ((oclass == SNULL || oclass == USTATIC) &&
		    (class == STATIC || class == SNULL)) {
		/* Unreferenced, store it for (eventual) later use */
		/* Ignore it if it not declared static */
			s->sflags |= SINLINE;
			inline_start(s);
		} else if (class == EXTERN)
			class = EXTDEF;
	} else if (class == EXTERN)
		class = SNULL; /* same result */

	cftnsp = s;
	defid(p, class);
	prolab = getlab();
	c = cftnsp->soname;
	send_passt(IP_PROLOG, -1, -1, c, cftnsp->stype,
	    cftnsp->sclass == EXTDEF, prolab, ctval);
	blevel++;
#ifdef STABS
	if (gflag)
		stabs_func(s);
#endif
	nfree(tp);
	nfree(p);

}

static void
fend(void)
{
	if (blevel)
		cerror("function level error");
	ftnend();
	fun_inline = 0;
	cftnsp = NULL;
}

static NODE *
structref(NODE *p, int f, char *name)
{
	NODE *r;

	if (f == DOT)
		p = buildtree(ADDROF, p, NIL);
	r = biop(NAME, NIL, NIL);
	r->n_name = name;
	r = buildtree(STREF, p, r);
	return r;
}

static void
olddecl(NODE *p)
{
	struct symtab *s;

	s = lookup((char *)p->n_sp, 0);
	if (s->slevel != 1 || s->stype == UNDEF)
		uerror("parameter '%s' not defined", s->sname);
	else if (s->stype != FARG)
		uerror("parameter '%s' redefined", s->sname);
	s->stype = p->n_type;
	s->sdf = p->n_df;
	s->ssue = p->n_sue;
	nfree(p);
}

void
branch(int lbl)
{
	int r = reached++;
	ecomp(biop(GOTO, bcon(lbl), NIL));
	reached = r;
}

/*
 * Create a printable string based on an encoded string.
 */
static char *
mkpstr(char *str)
{
	char *s, *os;
	int v, l = strlen(str)+3; /* \t + \n + \0 */

	os = s = inlalloc(l);
	*s++ = '\t';
	for (; *str; ) {
		if (*str++ == '\\')
			v = esccon(&str);
		else
			v = str[-1];
		*s++ = v;
	}
	*s++ = '\n';
	*s = 0;
	return os;
}

/*
 * Estimate the max length a string will have in its internal 
 * representation based on number of \ characters.
 */
static int
maxstlen(char *str)
{
	int i;

	for (i = 0; *str; str++, i++)
		if (*str == '\\' || *str < 32 || *str > 0176)
			i += 3;
	return i;
}

static char *
voct(char *d, unsigned int v)
{
	v &= (1 << SZCHAR) - 1;
	*d++ = '\\';
	*d++ = v/64 + '0'; v &= 077;
	*d++ = v/8 + '0'; v &= 7;
	*d++ = v + '0';
	return d;
}
	

/*
 * Convert a string to internal format.  The resulting string may be no
 * more than len characters long.
 */
static void
fixstr(char *d, char *s, int len)
{
	unsigned int v;

	while (*s) {
		if (len <= 0)
			cerror("fixstr");
		if (*s == '\\') {
			s++;
			v = esccon(&s);
			d = voct(d, v);
			len -= 4;
		} else if (*s < ' ' || *s > 0176) {
			d = voct(d, *s++);
			len -= 4;
		} else
			*d++ = *s++, len--;
	}
	*d = 0;
}

/*
 * Add "raw" string new to cleaned string old.
 */
static char *
stradd(char *old, char *new)
{
	char *rv;
	int len;

	if (*new == 'L' && new[1] == '\"')
		widestr = 1, new++;
	if (*new == '\"') {
		new++;			 /* remove first " */
		new[strlen(new) - 1] = 0;/* remove last " */
	}
	len = strlen(old) + maxstlen(new) + 1;
	rv = tmpalloc(len);
	strlcpy(rv, old, len);
	fixstr(rv + strlen(old), new, maxstlen(new) + 1);
	return rv;
}

static struct symtab *
clbrace(NODE *p)
{
	struct symtab *sp;

	if (blevel == 0 && xnf != NULL)
		cerror("no level0 compound literals");

	sp = getsymtab("cl", STEMP);
	sp->stype = p->n_type;
	sp->squal = p->n_qual;
	sp->sdf = p->n_df;
	sp->ssue = p->n_sue;
	sp->sclass = blevel ? AUTO : STATIC;
	if (!ISARY(sp->stype) || sp->sdf->ddim != NOOFFSET) {
		sp->soffset = NOOFFSET;
		oalloc(sp, &autooff);
	}
	tfree(p);
	beginit(sp);
	return sp;
}

NODE *
biop(int op, NODE *l, NODE *r)
{
	return block(op, l, r, INT, 0, MKSUE(INT));
}

static NODE *
cmop(NODE *l, NODE *r)
{
	return biop(CM, l, r);
}

static NODE *
voidcon(void)
{
	return block(ICON, NIL, NIL, STRTY, 0, MKSUE(VOID));
}

/* Support for extended assembler a' la' gcc style follows below */

static NODE *
xmrg(NODE *out, NODE *in)
{
	NODE *p = in;

	if (p->n_op == XARG) {
		in = cmop(out, p);
	} else {
		while (p->n_left->n_op == CM)
			p = p->n_left;
		p->n_left = cmop(out, p->n_left);
	}
	return in;
}

/*
 * Put together in and out node lists in one list, and balance it with
 * the constraints on the right side of a CM node.
 */
static NODE *
xcmop(NODE *out, NODE *in, NODE *str)
{
	NODE *p, *q;

	if (out) {
		/* D out-list sanity check */
		for (p = out; p->n_op == CM; p = p->n_left) {
			q = p->n_right;
			if (q->n_name[0] != '=' && q->n_name[0] != '+')
				uerror("output missing =");
		}
		if (p->n_name[0] != '=' && p->n_name[0] != '+')
			uerror("output missing =");
		if (in == NIL)
			p = out;
		else
			p = xmrg(out, in);
	} else if (in) {
		p = in;
	} else
		p = voidcon();

	if (str == NIL)
		str = voidcon();
	return cmop(p, str);
}

/*
 * Generate a XARG node based on a string and an expression.
 */
static NODE *
xasmop(char *str, NODE *p)
{

	p = biop(XARG, p, NIL);
	p->n_name = isinlining ? newstring(str, strlen(str)+1) : str;
	return p;
}

/*
 * Generate a XASM node based on a string and an expression.
 */
static void
mkxasm(char *str, NODE *p)
{
	NODE *q;

	q = biop(XASM, p->n_left, p->n_right);
	q->n_name = isinlining ? newstring(str, strlen(str)+1) : str;
	nfree(p);
	ecomp(q);
}
