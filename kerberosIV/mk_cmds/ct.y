%{
/*	$Id: ct.y,v 1.1.1.1 1995/12/14 06:52:48 tholo Exp $	*/

/*-
 * Copyright 1987, 1988 by the Student Information Processing Board
 *	of the Massachusetts Institute of Technology
 *
 * Permission to use, copy, modify, and distribute this software
 * and its documentation for any purpose and without fee is
 * hereby granted, provided that the above copyright notice
 * appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation,
 * and that the names of M.I.T. and the M.I.T. S.I.P.B. not be
 * used in advertising or publicity pertaining to distribution
 * of the software without specific, written prior permission.
 * M.I.T. and the M.I.T. S.I.P.B. make no representations about
 * the suitability of this software for any purpose.  It is
 * provided "as is" without express or implied warranty.
 */

#include <stdio.h>
#include <stdlib.h>

char *str_concat3(), *ds(), *generate_rqte(), *quote();
long flag_value();
char *last_token = (char *)NULL;
FILE *output_file;
long gensym_n = 0;
%}

%union {
	char *dynstr;
	long flags;
}

%token COMMAND_TABLE REQUEST UNKNOWN UNIMPLEMENTED END
%token <dynstr> STRING
%token <dynstr> FLAGNAME
%type <dynstr> namelist header request_list
%type <dynstr> request_entry
%type <flags> flag_list options
%left OPTIONS
%{
#define NO_SS_ERR_H
#include <ss/ss.h>
%}
%start command_table
%%
command_table :	header request_list END ';'
		{ write_ct($1, $2); }
	;

header	:	COMMAND_TABLE STRING ';'
		{ $$ = $2; }
	;

request_list :	request_list request_entry
		{ $$ = str_concat3($1, $2, ""); }
	|
		{ $$ = ""; }
	;

request_entry :	REQUEST STRING ',' STRING ',' namelist ',' options ';'
		{ $$ = generate_rqte($2, quote($4), $6, $8); }
	|	REQUEST STRING ',' STRING ',' namelist ';'
		{ $$ = generate_rqte($2, quote($4), $6, 0); }
	|	UNKNOWN namelist ';'
		{ $$ = generate_rqte("ss_unknown_request",
					(char *)NULL, $2, 0); }
	|	UNIMPLEMENTED STRING ',' STRING ',' namelist ';'
		{ $$ = generate_rqte("ss_unimplemented", quote($4), $6, 3); }
	;

options	:	'(' flag_list ')'
		{ $$ = $2; }
	|	'(' ')'
		{ $$ = 0; }
	;

flag_list :	flag_list ',' STRING
		{ $$ = $1 | flag_val($3); }
	|	STRING
		{ $$ = flag_val($1); }
	;

namelist: 	STRING
		{ $$ = quote(ds($1)); }
	|	namelist ',' STRING
		{ $$ = str_concat3($1, quote($3), ",\n    "); }
	;

%%
