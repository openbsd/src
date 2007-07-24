/*	$OpenPackages$ */
/*	$OpenBSD: for.c,v 1.33 2007/07/24 18:52:47 espie Exp $	*/
/*	$NetBSD: for.c,v 1.4 1996/11/06 17:59:05 christos Exp $ */

/*
 * Copyright (c) 1999 Marc Espie.
 *
 * Extensive code modifications for the OpenBSD project.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE OPENBSD PROJECT AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OPENBSD
 * PROJECT OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1992, The Regents of the University of California.
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
 * 3. Neither the name of the University nor the names of its contributors
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

#include <assert.h>
#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "defines.h"
#include "buf.h"
#include "for.h"
#include "lst.h"
#include "error.h"
#include "var.h"
#include "lowparse.h"
#include "str.h"
#include "memory.h"

/*
 * For statements are of the form:
 *
 * .for <variable> [variable...] in <varlist>
 * ...
 * .endfor
 *
 * The trick is to look for the matching .end inside .for loops.
 * To do that, we keep track of the nesting level of .for loops
 * and matching .endfor statements, accumulating all statements between
 * the initial .for loop and the matching .endfor,
 * then we evaluate the .for loop for each variable in the varlist.
 */

/* State of a for loop.  */
struct For_ {
    char		*text;		/* Unexpanded text		*/
    LIST		vars;		/* List of variables		*/
    LstNode		var;		/* Current var			*/
    int			nvars;		/* Total number of vars		*/
    LIST		lst;		/* List of items		*/
    size_t		guess;		/* Estimated expansion size	*/
    BUFFER		buf;		/* Accumulating text		*/
    unsigned long	lineno; 	/* Line number at start of loop */
    unsigned long	level;		/* Nesting level		*/
    bool		freeold;
};

/* ForExec(value, handle);
 *	Expands next variable in loop sequence described by handle to value. */
static void ForExec(void *, void *);

/* n = build_words_list(lst, s);
 *	Cuts string into words, pushes words into list, in reverse order,
 *	because Parse_FromString works as a stack.
 *	Returns the number of words.  */
static unsigned long build_words_list(Lst, const char *);

static unsigned long
build_words_list(Lst lst, const char *s)
{
    const char *end, *wrd;
    unsigned long n;

    n = 0;
    end = s;

    while ((wrd = iterate_words(&end)) != NULL) {
	Lst_AtFront(lst, escape_dupi(wrd, end, "\"'"));
	n++;
    }
    return n;
}

For *
For_Eval(const char *line)
{
    const char	*ptr = line;
    const char	*wrd;
    char	*sub;
    const char	*endVar;
    For 	*arg;
    unsigned long n;

    while (isspace(*ptr))
	ptr++;

    /* Parse loop.  */

    arg = emalloc(sizeof(*arg));
    arg->nvars = 0;
    Lst_Init(&arg->vars);

    for (;;) {
	/* Grab the variables.  */
	for (wrd = ptr; *ptr && !isspace(*ptr); ptr++)
	    continue;
	if (ptr - wrd == 0) {
	    Parse_Error(PARSE_FATAL, "Syntax error in for");
	    return 0;
	}
	endVar = ptr++;
	while (isspace(*ptr))
	    ptr++;
	/* End of variable list ? */
	if (endVar - wrd == 2 && wrd[0] == 'i' && wrd[1] == 'n')
	    break;
	Lst_AtEnd(&arg->vars, Var_NewLoopVar(wrd, endVar));
	arg->nvars++;
    }
    if (arg->nvars == 0) {
	Parse_Error(PARSE_FATAL, "Missing variable in for");
	return 0;
    }

    /* Make a list with the remaining words.  */
    sub = Var_Subst(ptr, NULL, false);
    if (DEBUG(FOR)) {
    	LstNode ln;
	(void)fprintf(stderr, "For: Iterator ");
	for (ln = Lst_First(&arg->vars); ln != NULL; ln = Lst_Adv(ln))
		(void)fprintf(stderr, "%s ", (char *)Lst_Datum(ln));
	(void)fprintf(stderr, "List %s\n", sub);
    }

    Lst_Init(&arg->lst);
    n = build_words_list(&arg->lst, sub);
    free(sub);
    if (arg->nvars != 1 && n % arg->nvars != 0) {
	Parse_Error(PARSE_FATAL, "Wrong number of items in for loop");
    	return 0;
    }
    arg->lineno = Parse_Getlineno();
    arg->level = 1;
    Buf_Init(&arg->buf, 0);

    return arg;
}


bool
For_Accumulate(For *arg, const char *line)
{
    const char	    *ptr = line;

    assert(arg->level > 0);

    if (*ptr == '.') {

	for (ptr++; isspace(*ptr); ptr++)
	    continue;

	if (strncmp(ptr, "endfor", 6) == 0 &&
	    (isspace(ptr[6]) || !ptr[6])) {
	    if (DEBUG(FOR))
		(void)fprintf(stderr, "For: end for %lu\n", arg->level);
	    /* If matching endfor, don't add line to buffer.  */
	    if (--arg->level == 0)
		return false;
	}
	else if (strncmp(ptr, "for", 3) == 0 &&
		 isspace(ptr[3])) {
	    arg->level++;
	    if (DEBUG(FOR))
		(void)fprintf(stderr, "For: new loop %lu\n", arg->level);
	}
    }
    Buf_AddString(&arg->buf, line);
    Buf_AddChar(&arg->buf, '\n');
    return true;
}


#define GUESS_EXPANSION 32
static void
ForExec(void *valuep, void *argp)
{
    char *value = (char *)valuep;
    For *arg = (For *)argp;
    BUFFER buf;

    /* Parse_FromString pushes stuff back, so we need to go over vars in
       reverse.  */
    if (arg->var == NULL) {
    	arg->var = Lst_Last(&arg->vars);
	arg->text = Buf_Retrieve(&arg->buf);
	arg->freeold = false;
    }

    if (DEBUG(FOR))
	(void)fprintf(stderr, "--- %s = %s\n", (char *)Lst_Datum(arg->var),
	    value);
    Buf_Init(&buf, arg->guess);
    Var_SubstVar(&buf, arg->text, Lst_Datum(arg->var), value);
    if (arg->freeold)
    	free(arg->text);
    arg->text = Buf_Retrieve(&buf);
    arg->freeold = true;
    arg->var = Lst_Rev(arg->var);
    if (arg->var == NULL)
	Parse_FromString(arg->text, arg->lineno);
}


void
For_Run(For *arg)
{
    arg->text = Buf_Retrieve(&arg->buf);
    arg->guess = Buf_Size(&arg->buf) + GUESS_EXPANSION;

    arg->var = NULL;
    Lst_ForEach(&arg->lst, ForExec, arg);
    Buf_Destroy(&arg->buf);
    Lst_Destroy(&arg->vars, (SimpleProc)Var_DeleteLoopVar);
    Lst_Destroy(&arg->lst, (SimpleProc)free);
    free(arg);
}
