/*	$OpenBSD: for.c,v 1.15 2000/03/26 16:21:32 espie Exp $	*/
/*	$NetBSD: for.c,v 1.4 1996/11/06 17:59:05 christos Exp $	*/

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

/*-
 * for.c --
 *	Functions to handle loops in a makefile.
 *
 * Interface:
 *	For_Eval 	Evaluate the .for in the passed line
 *	For_Accumulate	Add lines to an accumulating loop
 *	For_Run		Run accumulated loop
 *
 */

#include    <ctype.h>
#include    <assert.h>
#include    <stddef.h>
#include    "make.h"
#include    "buf.h"

#ifndef lint
#if 0
static char sccsid[] = "@(#)for.c	8.1 (Berkeley) 6/6/93";
#else
static char rcsid[] = "$OpenBSD: for.c,v 1.15 2000/03/26 16:21:32 espie Exp $";
#endif
#endif /* not lint */

/*
 * For statements are of the form:
 *
 * .for <variable> in <varlist>
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
    char		*text;		/* unexpanded text       	*/
    char		*var;		/* Index name		 	*/
    Lst  		lst;		/* List of items	 	*/
    size_t		guess;		/* Estimated expansion size	*/
    BUFFER		buf;		/* Accumulating text	 	*/
    unsigned long	lineno;		/* Line number at start of loop */
    unsigned long	level;		/* Nesting level		*/
};

static int ForExec __P((ClientData, ClientData));
static void build_words_list __P((Lst, const char *));

/* Cut a string into words, stuff that into list.  */
static void
build_words_list(lst, s)
    Lst lst;
    const char *s;
{
    const char *wrd;

    for (;;) {
	for (; *s != '\0' && isspace(*s); s++)
	    continue;
	if (*s == '\0')
	    break;
	for (wrd = s; *s != '\0' && !isspace(*s); s++)
	    continue;
    	/* note that we fill the list backward, since 
     	 * Parse_FromString stacks strings.  */
	Lst_AtFront(lst, interval_dup(wrd, s));
    }
}

/*
 *-----------------------------------------------------------------------
 * For_Eval --
 *	Evaluate the for loop in the passed line. The line
 *	looks like this:
 *	    .for <variable> in <varlist>
 *
 * Results:
 *	Loop structure, to accumulate further lines.
 *	NULL if this was not a for loop after all.
 *-----------------------------------------------------------------------
 */

For *
For_Eval(line)
    char    	    *line;    /* Line to parse */
{
    char 	*ptr = line;
    char	*wrd;
    char 	*sub;
    char	*endVar;
    For 	*arg;

    for (ptr++; *ptr && isspace(*ptr); ptr++)
	continue;
    /* If we are not in a for loop quickly determine if the statement is
     * a for.  */
    if (ptr[0] != 'f' || ptr[1] != 'o' || ptr[2] != 'r' ||
	!isspace(ptr[3]))
	return NULL;
    ptr += 4;

    while (*ptr && isspace(*ptr))
	ptr++;

    /* We found a for loop, and now we are going to parse it.  */

    /* Grab the variable.  */
    for (wrd = ptr; *ptr && !isspace(*ptr); ptr++)
	continue;
    if (ptr - wrd == 0) {
	Parse_Error(PARSE_FATAL, "missing variable in for");
	return 0;
    }
    endVar = ptr++;

    while (*ptr && isspace(*ptr))
	ptr++;

    /* Grab the `in'.  */
    if (ptr[0] != 'i' || ptr[1] != 'n' ||
	!isspace(ptr[2])) {
	Parse_Error(PARSE_FATAL, "missing `in' in for");
	printf("%s\n", ptr);
	return NULL;
    }
    ptr += 3;

    /* .for loop is go, collate what we need.  */
    arg = emalloc(sizeof(*arg));
    arg->var = interval_dup(wrd, endVar);

    /* Make a list with the remaining words.  */
    sub = Var_Subst(ptr, VAR_GLOBAL, FALSE);
    if (DEBUG(FOR))
	(void)fprintf(stderr, "For: Iterator %s List %s\n", arg->var, sub);

    arg->lst = Lst_Init();
    build_words_list(arg->lst, sub);
    free(sub);
    arg->lineno = Parse_Getlineno();
    arg->level = 1;
    Buf_Init(&arg->buf, 0);

    return arg;
}


/*-
 *-----------------------------------------------------------------------
 * For_Accumulate --
 *	Accumulate lines in a for loop, until we find the matching endfor.
 *
 * Results:
 *	TRUE: keep accumulating lines.
 *	FALSE: We found the matching .endfor
 *
 * Side Effects:
 *	Accumulate lines in arg.
 *-----------------------------------------------------------------------
 */
Boolean
For_Accumulate(arg, line)
    For		    *arg;
    const char      *line;    /* Line to parse */
{
    const char	    *ptr = line;

    assert(arg->level > 0);

    if (*ptr == '.') {

	for (ptr++; *ptr && isspace(*ptr); ptr++)
	    continue;

	if (strncmp(ptr, "endfor", 6) == 0 &&
	    (isspace(ptr[6]) || !ptr[6])) {
	    if (DEBUG(FOR))
		(void)fprintf(stderr, "For: end for %lu\n", arg->level);
	    /* If matching endfor, don't add line to buffer.  */
	    if (--arg->level == 0)
		return FALSE;
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
    return TRUE;
}


#define GUESS_EXPANSION 32
/*-
 *-----------------------------------------------------------------------
 * ForExec --
 *	Expand the for loop for this index and push it in the Makefile
 *-----------------------------------------------------------------------
 */
static int
ForExec(namep, argp)
    ClientData namep;
    ClientData argp;
{
    char *name = (char *)namep;
    For *arg = (For *)argp;

    Buf_Init(&arg->buf, arg->guess);
    Var_Set(arg->var, name, VAR_GLOBAL);
    if (DEBUG(FOR))
	(void)fprintf(stderr, "--- %s = %s\n", arg->var, name);
    Var_SubstVar(&arg->buf, arg->text, arg->var, VAR_GLOBAL);
    if (Buf_Size(&arg->buf) >= arg->guess)
    	arg->guess = Buf_Size(&arg->buf) + GUESS_EXPANSION;
    
    Parse_FromString(Buf_Retrieve(&arg->buf), arg->lineno);
    Var_Delete(arg->var, VAR_GLOBAL);
    return 0;
}


/*-
 *-----------------------------------------------------------------------
 * For_Run --
 *	Run the for loop, pushing expanded lines for reparse
 *-----------------------------------------------------------------------
 */

void
For_Run(arg)
    For *arg;
{
    arg->text = Buf_Retrieve(&arg->buf);
    arg->guess = Buf_Size(&arg->buf) + GUESS_EXPANSION;

    Lst_ForEach(arg->lst, ForExec, arg);
    free(arg->var);
    free(arg->text);
    Lst_Destroy(arg->lst, (void (*) __P((ClientData)))free);
    free(arg);
}
