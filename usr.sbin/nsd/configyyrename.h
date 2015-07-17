/*
 * configyyrename.h -- renames for config file yy values to avoid conflicts.
 *
 * Copyright (c) 2001-2006, NLnet Labs. All rights reserved.
 *
 * See LICENSE for the license.
 *
 */

#ifndef CONFIGYYRENAME_H
#define CONFIGYYRENAME_H
#include "config.h"

/* defines to change symbols so that no yacc/lex symbols clash */
#define yymaxdepth c_maxdepth
#define yyparse c_parse
#define yylex   c_lex
#define yyerror c_error
#define yylval  c_lval
#define yychar  c_char
#define yydebug c_debug
#define yypact  c_pact
#define yyr1    c_r1
#define yyr2    c_r2
#define yydef   c_def
#define yychk   c_chk
#define yypgo   c_pgo
#define yyact   c_act
#define yyexca  c_exca
#define yyerrflag c_errflag
#define yynerrs c_nerrs
#define yyps    c_ps
#define yypv    c_pv
#define yys     c_s
#define yyss    c_ss
#define yy_yys  c_yys
#define yystate c_state
#define yytmp   c_tmp
#define yyv     c_v
#define yy_yyv  c_yyv
#define yyval   c_val
#define yylloc  c_lloc
#define yyreds  c_reds
#define yytoks  c_toks
#define yylhs   c_yylhs
#define yylen   c_yylen
#define yydefred c_yydefred
#define yydgoto c_yydgoto
#define yysindex c_yysindex
#define yyrindex c_yyrindex
#define yygindex c_yygindex
#define yytable  c_yytable
#define yycheck  c_yycheck
#define yyname   c_yyname
#define yyrule   c_yyrule
#define yyin    c_in
#define yyout   c_out
#define yywrap  c_wrap
#define yy_load_buffer_state c_load_buffer_state
#define yy_switch_to_buffer c_switch_to_buffer
#define yy_flush_buffer c_flush_buffer
#define yy_init_buffer c_init_buffer
#define yy_scan_buffer c_scan_buffer
#define yy_scan_bytes c_scan_bytes
#define yy_scan_string c_scan_string
#define yy_create_buffer c_create_buffer
#define yyrestart c_restart
#define yy_delete_buffer c_delete_buffer
#define yypop_buffer_state c_pop_buffer_state
#define yypush_buffer_state c_push_buffer_state
#define yyunput c_unput
#define yyset_in c_set_in
#define yyget_in c_get_in
#define yyset_out c_set_out
#define yyget_out c_get_out
#define yyget_lineno c_get_lineno
#define yyset_lineno c_set_lineno
#define yyset_debug c_set_debug
#define yyget_debug c_get_debug
#define yy_flex_debug c_flex_debug
#define yylex_destroy c_lex_destroy
#define yyfree c_free
#define yyrealloc c_realloc
#define yyalloc c_alloc
#define yymalloc c_malloc
#define yyget_leng c_get_leng
#define yylineno c_lineno
#define yyget_text c_get_text
#define yyvsp c_vsp
#define yyvs c_vs
#define yytext c_text
#define yyleng c_leng
#define yy_meta c__meta
#define yy_start c__start
#define yy_nxt c__nxt
#define yy_n_chars c__n_chars
#define yy_more_flag c__more_flag
#define yy_more_len c__more_len
#define yy_try_NUL_trans c__try_NUL_trans
#define yy_last_accepting_cpos c__last_accepting_cpos
#define yy_last_accepting_state c__last_accepting_state
#define yy_init c__init
#define yy_base c__base
#define yy_accept c__accept
#define yy_c_buf_p c__c_buf_p
#define yy_chk c__chk
#ifndef LEX_DEFINES_YY_CURRENT_BUFFER
#  define yy_current_buffer c__current_buffer
#endif
#define yy_def c__def
#define yy_did_buffer_switch_on_eof c__did_buffer_switch_on_eof
#define yy_ec c__ec
#define yy_fatal_error c__fatal_error
#define yy_flex_alloc c__flex_alloc
#define yy_flex_free c__flex_free
#define yy_flex_realloc c__flex_realloc
#define yy_get_next_buffer c__get_next_buffer
#define yy_get_previous_state c__get_previous_state
#define yy_hold_char c__hold_char

#endif /* CONFIGYYRENAME_H */
