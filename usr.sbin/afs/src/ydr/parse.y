/*
 * Copyright (c) 1995, 1996, 1997, 1998 Kungliga Tekniska Högskolan
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the Kungliga Tekniska
 *      Högskolan and its contributors.
 * 
 * 4. Neither the name of the Institute nor the names of its contributors
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
RCSID("$Id: parse.y,v 1.1.1.1 1998/09/14 21:53:27 art Exp $");
#endif

#include <stdio.h>
#include <list.h>
#include <bool.h>
#include <mem.h>
#include "sym.h"
#include "types.h"
#include "output.h"
#include "lex.h"

void yyerror (char *);

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
%token T_SHORT T_USHORT T_UCHAR T_ASIS
%token <name> T_IDENTIFIER T_VERBATIM T_PACKAGE
%token <constant> T_CONSTANT
%token <sym> T_IDCONST T_IDTYPE

%type <constant> constant opt_constant
%type <constant> param_type
%type <sym> enumentry type_decl proc_decl
%type <list> enumentries enumbody structbody memberdecls params
%type <type> type
%type <sentry> memberdecl memberdecl2
%type <arg> param
%type <flags> flags

%start specification

%%

specification:	
		| specification declaration
		| specification directive
		| error ';'
		;

declaration:	type_decl { 
     		     generate_header ($1, headerfile);
		     generate_sizeof ($1, headerfile);
		     generate_function ($1, ydrfile, TRUE);
		     generate_function_prototype ($1, headerfile, TRUE);
		     generate_function ($1, ydrfile, FALSE);
		     generate_function_prototype ($1, headerfile, FALSE);
		}
		| proc_decl { 
		     generate_client_stub ($1, clientfile, clienthdrfile);
		     generate_server_stub ($1, serverfile, serverhdrfile);
		}
		;

type_decl	: T_ENUM T_IDENTIFIER enumbody ';'
		{ $$ = define_enum ($2, $3); }
		| T_STRUCT T_IDENTIFIER { define_struct($2); } structbody ';'
		{ $$ = set_struct_body ($2, $4); }
		| T_TYPEDEF memberdecl ';'
		{ $$ = define_typedef ($2); }
		| T_CONST T_IDENTIFIER '=' constant ';'
		{ $$ = define_const ($2, $4);  }
		;

flags:		{ $$ = TSIMPLE; }
		| T_SPLIT { $$ = TSPLIT; }
		| T_MULTI { $$ = TSPLIT | TSIMPLE; }
		;

proc_decl:	T_IDENTIFIER '(' params ')' flags '=' constant ';'
		{ $$ = (Symbol *)emalloc(sizeof(Symbol));
	          $$->type = TPROC;
	          $$->name = $1;
		  $$->u.proc.arguments = $3;
		  $$->u.proc.id = $7;
		  $$->u.proc.flags = $5; 
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
		;

directive:	T_PACKAGE T_IDENTIFIER
		{ package = $2; }
		| T_VERBATIM
		{ fprintf (headerfile, "%s\n", $1); }
		;

enumbody:	'{' enumentries '}' { $$ = $2; }
		;

enumentries:	{ $$ = listnew (); }
		| enumentry
		{ $$ = listnew(); listaddhead ($$, $1); }
		| enumentries ',' enumentry
		{ listaddtail ($1, $3); $$ = $1;}
		;

enumentry:	T_IDENTIFIER '=' constant
		{ $$ = createenumentry ($1, $3); }
		;

memberdecl:	T_ASIS memberdecl2
		{ $$ = $2; $$->type->flags |= TASIS; }  
		| memberdecl2
		{ $$ = $1; }
		;  

memberdecl2:	type T_IDENTIFIER
		{ $$ = createstructentry ($2, $1); }
		| T_STRUCT type T_IDENTIFIER
		{
		    $$ = createstructentry ($3, $2);
		}
		| T_STRING T_IDENTIFIER '<' opt_constant '>'
		{ Type *t  = (Type *)emalloc (sizeof(Type));
		  t->type  = TSTRING;
		  t->size  = $4;
		  t->flags = 0;
		  $$ = createstructentry ($2, t);
		}
		| type T_IDENTIFIER '[' opt_constant ']' 
		{ Type *t    = (Type *)emalloc(sizeof(Type));
		  t->type    = TARRAY;
		  t->size    = $4;
		  t->subtype = $1;
		  t->flags   = 0;
		  $$ = createstructentry ($2, t); }
		| type T_IDENTIFIER '<' opt_constant '>'
		{ Type *t      = (Type *)emalloc(sizeof(Type));
		  t->type      = TVARRAY;
		  t->size      = $4;
		  t->subtype   = $1;
		  t->indextype = NULL;
		  t->flags     = 0;
		  $$ = createstructentry ($2, t); }
		| type T_IDENTIFIER '<' type '>'
		{ Type *t      = (Type *)emalloc(sizeof(Type));
		  t->type      = TVARRAY;
		  t->size      = 0;
		  t->subtype   = $1;
		  t->indextype = $4;
		  t->flags     = 0;
		  $$ = createstructentry ($2, t); }
		;

type:		long_or_int
		{ $$ = (Type *)emalloc(sizeof(Type)); 
		  $$->type  = TLONG;
		  $$->flags = 0; }
		| T_UNSIGNED
		{ $$ = (Type *)emalloc(sizeof(Type)); 
		  $$->type  = TULONG;
		  $$->flags = 0; }
		| T_ULONG
		{ $$ = (Type *)emalloc(sizeof(Type)); 
		  $$->type  = TULONG;
		  $$->flags = 0; }
		| T_UNSIGNED T_LONG
		{ $$ = (Type *)emalloc(sizeof(Type)); 
		  $$->type  = TULONG;
		  $$->flags = 0; }
		| T_CHAR
		{ $$ = (Type *)emalloc(sizeof(Type)); 
		  $$->type  = TCHAR;
		  $$->flags = 0; }
		| T_UCHAR
		{ $$ = (Type *)emalloc(sizeof(Type)); 
		  $$->type  = TUCHAR;
		  $$->flags = 0; }
		| T_UNSIGNED T_CHAR
		{ $$ = (Type *)emalloc(sizeof(Type)); 
		  $$->type  = TUCHAR;
		  $$->flags = 0; }
		| T_SHORT
		{ $$ = (Type *)emalloc(sizeof(Type)); 
		  $$->type  = TSHORT;
		  $$->flags = 0; }
		| T_USHORT
		{ $$ = (Type *)emalloc(sizeof(Type)); 
		  $$->type  = TUSHORT;
		  $$->flags = 0; }
		| T_UNSIGNED T_SHORT
		{ $$ = (Type *)emalloc(sizeof(Type)); 
		  $$->type  = TUSHORT;
		  $$->flags = 0; }
		| T_STRING
		{ $$ = (Type *)emalloc(sizeof(Type)); 
		  $$->type  = TSTRING;
		  $$->flags = 0; }
		| T_OPAQUE
		{ $$ = (Type *)emalloc(sizeof(Type)); 
		  $$->type  = TOPAQUE;
		  $$->flags = 0; }
		| type '*'
		{ $$ = (Type *)emalloc(sizeof(Type)); 
		  $$->type = TPOINTER; 
		  $$->subtype = $1;
		  $$->flags = 0; }
		| T_IDTYPE
		{ $$ = (Type *)emalloc(sizeof(Type)); 
		  $$->type   = TUSERDEF;
		  $$->symbol = $1; 
		  $$->flags  = 0;
		  if ($$->symbol->type != TSTRUCT 
		      && $$->symbol->type != TENUM
		      && $$->symbol->type != TTYPEDEF)
		       error_message ("%s used as a type\n", $$->symbol->name);
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
		  if (s->type != TCONST) {
		       error_message ("%s not a constant\n", s->name);
		  } else
		       $$ = s->u.val;
	     }
		  
%%

void
yyerror (char *s)
{
     error_message ("%s\n", s);
}
