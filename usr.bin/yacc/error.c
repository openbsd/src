/*	$OpenBSD: error.c,v 1.7 2002/06/19 03:24:56 deraadt Exp $	*/
/*	$NetBSD: error.c,v 1.4 1996/03/19 03:21:32 jtc Exp $	*/

/*
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Robert Paul Corbett.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
#if 0
static char sccsid[] = "@(#)error.c	5.3 (Berkeley) 6/1/90";
#else
static char rcsid[] = "$OpenBSD: error.c,v 1.7 2002/06/19 03:24:56 deraadt Exp $";
#endif
#endif /* not lint */

/* routines for printing error messages  */

#include "defs.h"


void
fatal(msg)
char *msg;
{
    fprintf(stderr, "%s: %s\n", input_file_name, msg);
    done(2);
}


void
no_space()
{
    fprintf(stderr, "%s: yacc is out of space\n", input_file_name);
    done(2);
}


void
open_error(filename)
char *filename;
{
    fprintf(stderr, "%s: cannot open source file\n", input_file_name);
    done(2);
}


void
unexpected_EOF()
{
    fprintf(stderr, "%s:%d: unexpected end-of-file\n",
	    input_file_name, lineno);
    done(1);
}


void
print_pos(st_line, st_cptr)
char *st_line;
char *st_cptr;
{
    char *s;

    if (st_line == 0) return;
    for (s = st_line; *s != '\n'; ++s)
    {
	if (isprint(*s) || *s == '\t')
	    putc(*s, stderr);
	else
	    putc('?', stderr);
    }
    putc('\n', stderr);
    for (s = st_line; s < st_cptr; ++s)
    {
	if (*s == '\t')
	    putc('\t', stderr);
	else
	    putc(' ', stderr);
    }
    putc('^', stderr);
    putc('\n', stderr);
}

void
syntax_error(st_lineno, st_line, st_cptr)
int st_lineno;
char *st_line;
char *st_cptr;
{
    fprintf(stderr, "%s:%d: syntax error\n",
	    input_file_name, st_lineno);
    print_pos(st_line, st_cptr);
    done(1);
}

void
unterminated_comment(c_lineno, c_line, c_cptr)
int c_lineno;
char *c_line;
char *c_cptr;
{
    fprintf(stderr, "%s:%d: unmatched /*\n",
	    input_file_name, c_lineno);
    print_pos(c_line, c_cptr);
    done(1);
}

void
unterminated_string(s_lineno, s_line, s_cptr)
int s_lineno;
char *s_line;
char *s_cptr;
{
    fprintf(stderr, "%s:%d:, unterminated string\n",
	    input_file_name, s_lineno);
    print_pos(s_line, s_cptr);
    done(1);
}

void
unterminated_text(t_lineno, t_line, t_cptr)
int t_lineno;
char *t_line;
char *t_cptr;
{
    fprintf(stderr, "%s:%d: unmatched %%{\n",
	    input_file_name, t_lineno);
    print_pos(t_line, t_cptr);
    done(1);
}

void
unterminated_union(u_lineno, u_line, u_cptr)
int u_lineno;
char *u_line;
char *u_cptr;
{
    fprintf(stderr, "%s:%d: unterminated %%union declaration\n",
	    input_file_name, u_lineno);
    print_pos(u_line, u_cptr);
    done(1);
}

void
over_unionized(u_cptr)
char *u_cptr;
{
    fprintf(stderr, "%s:%d: too many %%union declarations\n",
	    input_file_name, lineno);
    print_pos(line, u_cptr);
    done(1);
}

void
illegal_tag(t_lineno, t_line, t_cptr)
int t_lineno;
char *t_line;
char *t_cptr;
{
    fprintf(stderr, "%s:%d: illegal tag\n",
	    input_file_name, t_lineno);
    print_pos(t_line, t_cptr);
    done(1);
}


void
illegal_character(c_cptr)
char *c_cptr;
{
    fprintf(stderr, "%s:%d: illegal character\n",
	    input_file_name, lineno);
    print_pos(line, c_cptr);
    done(1);
}


void
used_reserved(s)
char *s;
{
    fprintf(stderr, "%s:%d: illegal use of reserved symbol %s\n",
	    input_file_name, lineno, s);
    done(1);
}

void
tokenized_start(s)
char *s;
{
     fprintf(stderr, "%s:%d: the start symbol %s cannot be declared to be a token\n",
	    input_file_name, lineno, s);
     done(1);
}

void
retyped_warning(s)
char *s;
{
    fprintf(stderr, "%s:%d: the type of %s has been redeclared\n",
	    input_file_name, lineno, s);
}

void
reprec_warning(s)
char *s;
{
    fprintf(stderr, "%s:%d: the precedence of %s has been redeclared\n",
	    input_file_name, lineno, s);
}

void
revalued_warning(s)
char *s;
{
    fprintf(stderr, "%s:%d: the value of %s has been redeclared\n",
	    input_file_name, lineno, s);
}

void
terminal_start(s)
char *s;
{
    fprintf(stderr, "%s:%d: the start symbol %s is a token\n",
	    input_file_name, lineno, s);
    done(1);
}

void
restarted_warning()
{
    fprintf(stderr, "%s:%d: the start symbol has been redeclared\n",
	     input_file_name, lineno);
}

void
no_grammar()
{
    fprintf(stderr, "%s:%d: no grammar has been specified\n",
	    input_file_name, lineno);
    done(1);
}

void
terminal_lhs(s_lineno)
int s_lineno;
{
    fprintf(stderr, "%s:%d: a token appears on the lhs of a production\n",
	    input_file_name, s_lineno);
    done(1);
}

void
prec_redeclared()
{
    fprintf(stderr, "%s:%d: conflicting %%prec specifiers\n",
	    input_file_name, lineno);
}

void
unterminated_action(a_lineno, a_line, a_cptr)
int a_lineno;
char *a_line;
char *a_cptr;
{
    fprintf(stderr, "%s:%d: unterminated action\n",
	    input_file_name, a_lineno);
    print_pos(a_line, a_cptr);
    done(1);
}

void
dollar_warning(a_lineno, i)
int a_lineno;
int i;
{
    fprintf(stderr, "%s:%d: $%d references beyond the end of the current rule\n",
	    input_file_name, a_lineno, i);
}

void
dollar_error(a_lineno, a_line, a_cptr)
int a_lineno;
char *a_line;
char *a_cptr;
{
    fprintf(stderr, "%s:%d: illegal $-name\n",
	    input_file_name, a_lineno);
    print_pos(a_line, a_cptr);
    done(1);
}


void
untyped_lhs()
{
    fprintf(stderr, "%s:%d: $$ is untyped\n",
	    input_file_name, lineno);
    done(1);
}

void
untyped_rhs(i, s)
int i;
char *s;
{
    fprintf(stderr, "%s:%d: $%d (%s) is untyped\n",
	    input_file_name, lineno, i, s);
    done(1);
}

void
unknown_rhs(i)
int i;
{
    fprintf(stderr, "%s:%d: $%d is untyped\n",
	    input_file_name, lineno, i);
    done(1);
}

void
default_action_warning()
{
    fprintf(stderr, "%s:%d: the default action assigns an undefined value to $$\n",
	    input_file_name, lineno);
}

void
undefined_goal(s)
char *s;
{
    fprintf(stderr, "%s: the start symbol %s is undefined\n", input_file_name, s);
    done(1);
}

void
undefined_symbol_warning(s)
char *s;
{
    fprintf(stderr, "%s: the symbol %s is undefined\n", input_file_name, s);
}
