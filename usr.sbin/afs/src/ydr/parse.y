/*
 * Copyright (c) 1995, 1996, 1997, 1998, 1999 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

%{
#ifdef HAVE_CONFIG_H
#include <config.h>
RCSID("$arla: parse.y,v 1.28 2003/01/20 07:12:58 lha Exp $");
#endif

#include <stdio.h>
#include <list.h>
#include <bool.h>
#include <roken.h>
#include "sym.h"
#include "types.h"
#include "output.h"
#include "lex.h"

void yyerror (char *);
static int varcnt = 0;

%}

%union {
     int constant;
     Symbol *sym;
     List *list;
     char *name;
     Type *type;
     StructEntry *sentry;
     Argument *arg;
     Bool bool;
     unsigned flags;
}

%token T_ENUM T_STRUCT T_CONST T_UNSIGNED T_ULONG T_INT T_CHAR T_STRING
%token T_LONG T_TYPEDEF T_OPAQUE T_IN T_OUT T_INOUT T_SPLIT T_MULTI
%token T_SHORT T_USHORT T_UCHAR T_ASIS T_PROC
%token T_LONGLONG T_ULONGLONG
%token <name> T_IDENTIFIER T_VERBATIM T_PACKAGE T_PREFIX T_ERROR_FUNCTION
%token <constant> T_CONSTANT
%token <sym> T_IDCONST T_IDTYPE

%type <constant> constant opt_constant opt_proc
%type <constant> param_type
%type <sym> enumentry type_decl proc_decl
%type <list> enumentries enumbody structbody memberdecls params attrs
%type <type> type
%type <sentry> memberdecl memberdecl2
%type <arg> param
%type <flags> flags
%type <name> attr

%start specification

%%

specification:	
		| specification declaration
		| specification directive
		| error ';'
		;

declaration:	type_decl { 
                 if (!sym_find_attr($1, "__nogenerate__")) {
     		     generate_header ($1, headerfile.stream);
		     generate_sizeof ($1, headerfile.stream);
		     generate_function ($1, ydrfile.stream, TRUE);
		     generate_function_prototype ($1, headerfile.stream, TRUE);
		     generate_function ($1, ydrfile.stream, FALSE);
		     generate_function_prototype ($1, headerfile.stream, FALSE);
		     generate_printfunction ($1, ydrfile.stream);
		     generate_printfunction_prototype ($1, headerfile.stream);
		     generate_freefunction ($1, ydrfile.stream);
		     generate_freefunction_prototype ($1, headerfile.stream);
		 }
		}
		| proc_decl { 
		     generate_client_stub ($1, clientfile.stream,
					   clienthdrfile.stream);
		     generate_server_stub ($1, serverfile.stream,
					   serverhdrfile.stream,
					   headerfile.stream);
		}
		;

type_decl	: T_ENUM T_IDENTIFIER enumbody ';'
		{ $$ = define_enum ($2, $3); varcnt = 0; }
		| T_STRUCT T_IDENTIFIER { define_struct($2); } structbody attrs ';'
		{ $$ = set_struct_body ($2, $4);
		  set_sym_attrs($$, $5); }
		| T_STRUCT type structbody attrs';'
		{ if($2->symbol && $2->symbol->type != YDR_TSTRUCT)
		    error_message (1, "%s is not a struct\n",
				   $2->symbol->name);
		  $$ = set_struct_body_sym ($2->symbol, $3);
		  set_sym_attrs($2->symbol, $4);
		}
		| T_TYPEDEF memberdecl ';'
		{ $$ = define_typedef ($2); }
		| T_CONST T_IDENTIFIER '=' constant ';'
		{ $$ = define_const ($2, $4);  }
		;

flags:		{ $$ = TSIMPLE; }
		| T_SPLIT { $$ = TSPLIT; }
		| T_MULTI { $$ = TSPLIT | TSIMPLE | TMULTI; }
		;

opt_proc:	{ $$ = 0; }/* empty */
		| T_PROC { $$ = 0; }
		;

proc_decl:	opt_proc T_IDENTIFIER '(' params ')' flags '=' constant ';'
		{ $$ = (Symbol *)emalloc(sizeof(Symbol));
	          $$->type = YDR_TPROC;
	          $$->name = $2;
		  $$->u.proc.package = package;
		  $$->u.proc.arguments = $4;
		  $$->u.proc.id = $8;
		  $$->u.proc.flags = $6; 
	          }
		;

params:		{ $$ = listnew(); }
		| param { $$ = listnew(); listaddhead ($$, $1); }
		| params ',' param
		{ listaddtail ($1, $3); $$ = $1; }
		;

param:		param_type memberdecl
		{ $$ = (Argument *)emalloc(sizeof(Argument));
		  $$->argtype = $1;
		  $$->name = $2->name;
		  $$->type = $2->type;
		  free($2); }
		;

param_type:	T_IN    { $$ = TIN; }
		| T_OUT { $$ = TOUT; }
		| T_INOUT { $$ = TINOUT; }
		| { $$ = TIN; }
		;

directive:	T_PACKAGE T_IDENTIFIER
		{ package = estrdup($2); 
		  listaddtail (packagelist, package);
		}
		| T_PREFIX T_IDENTIFIER
		{ prefix = $2; }
		| T_VERBATIM
		{ fprintf (headerfile.stream, "%s\n", $1); }
		| T_ERROR_FUNCTION T_IDENTIFIER
		{ error_function = $2; }
		;

enumbody:	'{' enumentries '}' { $$ = $2; }
		;

enumentries:	{ $$ = listnew ();  }
		| enumentry
		{ $$ = listnew(); listaddhead ($$, $1); }
		| enumentries ',' enumentry
		{ listaddtail ($1, $3); $$ = $1;}
		;

enumentry:	T_IDENTIFIER '=' constant
 		{ $$ = createenumentry ($1, $3); varcnt = $3 + 1; }
  		| T_IDENTIFIER
  		{ $$ = createenumentry ($1, varcnt++); }
  		;

memberdecl:	T_ASIS memberdecl2
		{ $$ = $2; $$->type->flags |= TASIS; }  
		| memberdecl2
		{ $$ = $1; }
		;  

memberdecl2:	type T_IDENTIFIER
		{ $$ = createstructentry ($2, $1); }
		| T_STRING T_IDENTIFIER '<' opt_constant '>'
		{ Type *t  = create_type (YDR_TSTRING, NULL, $4, NULL, NULL,0);
		  $$ = createstructentry ($2, t);
		}
		| T_STRING T_IDENTIFIER
		{ Type *t  = create_type (YDR_TSTRING, NULL, 0, NULL, NULL, 0);
		  $$ = createstructentry ($2, t);
		}
		| type T_IDENTIFIER '[' opt_constant ']' 
		{ Type *t  = create_type (YDR_TARRAY, NULL, $4, $1, NULL, 0);
		  $$ = createstructentry ($2, t); }
		| type T_IDENTIFIER '<' opt_constant '>'
		{ Type *t  = create_type (YDR_TVARRAY, NULL, $4, $1, NULL, 0);
		  $$ = createstructentry ($2, t); }
		| type T_IDENTIFIER '<' type '>'
		{ Type *t  = create_type (YDR_TVARRAY, NULL, 0, $1, $4, 0);
		  $$ = createstructentry ($2, t); }
		;

type:		long_or_int
                { $$ = create_type (YDR_TLONG, NULL, 0, NULL, NULL, 0); }
		| T_UNSIGNED
                { $$ = create_type (YDR_TULONG, NULL, 0, NULL, NULL, 0); }
		| T_ULONG
                { $$ = create_type (YDR_TULONG, NULL, 0, NULL, NULL, 0); }
		| T_UNSIGNED T_LONG
                { $$ = create_type (YDR_TULONG, NULL, 0, NULL, NULL, 0); }
		| T_LONGLONG
                { $$ = create_type (YDR_TLONGLONG, NULL, 0, NULL, NULL, 0); }
		| T_ULONGLONG
                { $$ = create_type (YDR_TULONGLONG, NULL, 0, NULL, NULL, 0); }
		| T_CHAR
                { $$ = create_type (YDR_TCHAR, NULL, 0, NULL, NULL, 0); }
		| T_UCHAR
                { $$ = create_type (YDR_TUCHAR, NULL, 0, NULL, NULL, 0); }
		| T_UNSIGNED T_CHAR
                { $$ = create_type (YDR_TUCHAR, NULL, 0, NULL, NULL, 0); }
		| T_SHORT
                { $$ = create_type (YDR_TSHORT, NULL, 0, NULL, NULL, 0); }
		| T_USHORT
                { $$ = create_type (YDR_TUSHORT, NULL, 0, NULL, NULL, 0); }
		| T_UNSIGNED T_SHORT
                { $$ = create_type (YDR_TUSHORT, NULL, 0, NULL, NULL, 0); }
		| T_STRING
                { $$ = create_type (YDR_TSTRING, NULL, 0, NULL, NULL, 0); }
		| T_OPAQUE
                { $$ = create_type (YDR_TOPAQUE, NULL, 0, NULL, NULL, 0); }
		| type '*'
                { $$ = create_type (YDR_TPOINTER, NULL, 0, $1, NULL, 0); }
		| T_IDTYPE
                { $$ = create_type (YDR_TUSERDEF, $1, 0, NULL, NULL, 0);
		  if ($$->symbol->type != YDR_TSTRUCT 
		      && $$->symbol->type != YDR_TENUM
		      && $$->symbol->type != YDR_TTYPEDEF)
		       error_message (1, "%s used as a type\n",
				      $$->symbol->name);
	        }
		| T_STRUCT type
		{
		    $$ = $2;
		    if ($$->symbol && $$->symbol->type != YDR_TSTRUCT)
			error_message (1, "%s is not a struct\n",
				       $$->symbol->name);
		}
		| T_STRUCT T_IDENTIFIER
                {   $$ = create_type (YDR_TUSERDEF, define_struct($2), 0, NULL,
				      NULL, 0); 
                }
		;

long_or_int:	T_LONG
		| T_INT
		;

memberdecls:	{ $$ = listnew(); }
		| memberdecls memberdecl ';'
		{ listaddtail ($1, $2); $$ = $1; }
		;

structbody:	'{' memberdecls '}' { $$ = $2; }
		;

opt_constant:	  { $$ = 0; }
		| constant { $$ = $1; }
		;

constant:	T_CONSTANT { $$ = $1; }
		| T_IDCONST
		{ Symbol *s = $1;
		  if (s->type != YDR_TCONST) {
		       error_message (1, "%s not a constant\n", s->name);
		  } else
		       $$ = s->u.val;
	        }
		;
		  
attrs:		{ $$ = listnew(); }
		| attr { $$ = listnew(); listaddhead ($$, $1); }
		| attrs attr
		{ listaddtail ($1, $2); $$ = $1; }
		;

attr:		T_IDENTIFIER '(' '(' T_IDENTIFIER ')' ')'
		{ $$ = $4; 
		  if (strcmp($1, "__attribute__") != 0)
		     error_message(1, "%s isn't __attribute__"); }
		;
%%

void
yyerror (char *s)
{
     error_message (1, "%s\n", s);
}
