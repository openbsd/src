/*	$OpenBSD: for.c,v 1.7 1999/12/06 22:24:31 espie Exp $	*/
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

#ifndef lint
#if 0
static char sccsid[] = "@(#)for.c	8.1 (Berkeley) 6/6/93";
#else
static char rcsid[] = "$OpenBSD: for.c,v 1.7 1999/12/06 22:24:31 espie Exp $";
#endif
#endif /* not lint */

/*-
 * for.c --
 *	Functions to handle loops in a makefile.
 *
 * Interface:
 *	For_Eval 	Evaluate the loop in the passed line.
 *	For_Run		Run accumulated loop
 *
 */

#include    <ctype.h>
#include    "make.h"
#include    "hash.h"
#include    "dir.h"
#include    "buf.h"

/*
 * For statements are of the form:
 *
 * .for <variable> in <varlist>
 * ...
 * .endfor
 *
 * The trick is to look for the matching end inside for for loop
 * To do that, we count the current nesting level of the for loops.
 * and the .endfor statements, accumulating all the statements between
 * the initial .for loop and the matching .endfor;
 * then we evaluate the for loop for each variable in the varlist.
 */

static int  	  forLevel = 0;  	/* Nesting level	     */
static char	 *forVar;		/* Iteration variable	     */
static Buffer	  forBuf;		/* Commands in loop	     */
static Lst	  forLst;		/* List of items	     */
static unsigned long forLineno;		/* Line at beginning of loop */

/*
 * State of a for loop.
 */
typedef struct _For {
    Buffer	  buf;			/* Unexpanded buffer	     */
    char*	  var;			/* Index name		     */
    Lst  	  lst;			/* List of variables	     */
    unsigned long lineno;
} For;

static int ForExec	__P((ClientData, ClientData));
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
	Lst_AtFront(lst, (ClientData)interval_dup(wrd, s));
    }
}


/*-
 *-----------------------------------------------------------------------
 * For_Eval --
 *	Evaluate the for loop in the passed line. The line
 *	looks like this:
 *	    .for <variable> in <varlist>
 *
 * Results:
 *	TRUE: We found a for loop, or we are inside a for loop
 *	FALSE: We did not find a for loop, or we found the end of the for
 *	       for loop.
 *
 * Side Effects:
 *	None.
 *
 *-----------------------------------------------------------------------
 */
int
For_Eval (line)
    char    	    *line;    /* Line to parse */
{
    char	    *ptr = line;
    int	    	    level;  	/* Level at which to report errors. */

    level = PARSE_FATAL;


    if (forLevel == 0) {
	char	    *endVar;
	char 	    *sub;
	char	    *wrd;

	for (ptr++; *ptr && isspace((unsigned char) *ptr); ptr++)
	    continue;
	/* If we are not in a for loop quickly determine if the statement is
	 * a for.  */
	if (ptr[0] != 'f' || ptr[1] != 'o' || ptr[2] != 'r' ||
	    !isspace((unsigned char) ptr[3]))
	    return FALSE;
	ptr += 3;

	/* We found a for loop, and now we are going to parse it.  */
	while (*ptr && isspace((unsigned char) *ptr))
	    ptr++;

	/* Grab the variable.  */
	for (wrd = ptr; *ptr && !isspace((unsigned char) *ptr); ptr++)
	    continue;

	if (ptr - wrd == 0) {
	    Parse_Error(level, "missing variable in for");
	    return 0;
	}
	endVar = ptr++;

	while (*ptr && isspace((unsigned char) *ptr))
	    ptr++;

	/* Grab the `in'.  */
	if (ptr[0] != 'i' || ptr[1] != 'n' ||
	    !isspace((unsigned char) ptr[2])) {
	    Parse_Error(level, "missing `in' in for");
	    printf("%s\n", ptr);
	    return 0;
	}
	ptr += 3;

	/* .for loop is go, collate what we need.  */
	forVar = interval_dup(wrd, endVar);

	/* Make a list with the remaining words.  */
	sub = Var_Subst(NULL, ptr, VAR_GLOBAL, FALSE);
	if (DEBUG(FOR))
	    (void)fprintf(stderr, "For: Iterator %s List %s\n", forVar, sub);

	forLst = Lst_Init(FALSE);
	build_words_list(forLst, sub);
	free(sub);
	forLineno = Parse_Getlineno();
	forBuf = Buf_Init(0);
	forLevel++;
	return 1;
    }
    else if (*ptr == '.') {

	for (ptr++; *ptr && isspace((unsigned char) *ptr); ptr++)
	    continue;

	if (strncmp(ptr, "endfor", 6) == 0 &&
	    (isspace((unsigned char) ptr[6]) || !ptr[6])) {
	    if (DEBUG(FOR))
		(void) fprintf(stderr, "For: end for %d\n", forLevel);
	    if (--forLevel < 0) {
		Parse_Error(level, "for-less endfor");
		return 0;
	    }
	}
	else if (strncmp(ptr, "for", 3) == 0 &&
		 isspace((unsigned char) ptr[3])) {
	    forLevel++;
	    if (DEBUG(FOR))
		(void) fprintf(stderr, "For: new loop %d\n", forLevel);
	}
    }

    if (forLevel != 0) {
	Buf_AddChars(forBuf, strlen(line), line);
	Buf_AddChar(forBuf, '\n');
	return 1;
    }
    else {
	return 0;
    }
}

/*-
 *-----------------------------------------------------------------------
 * ForExec --
 *	Expand the for loop for this index and push it in the Makefile
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	None.
 *
 *-----------------------------------------------------------------------
 */
static int
ForExec(namep, argp)
    ClientData namep;
    ClientData argp;
{
    char *name = (char *) namep;
    For *arg = (For *) argp;
    Var_Set(arg->var, name, VAR_GLOBAL);
    if (DEBUG(FOR))
	(void) fprintf(stderr, "--- %s = %s\n", arg->var, name);
    Parse_FromString(Var_Subst(arg->var, Buf_GetAll(arg->buf, NULL),
			       VAR_GLOBAL, FALSE), arg->lineno);
    Var_Delete(arg->var, VAR_GLOBAL);

    return 0;
}


/*-
 *-----------------------------------------------------------------------
 * For_Run --
 *	Run the for loop, immitating the actions of an include file
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	None.
 *
 *-----------------------------------------------------------------------
 */
void
For_Run()
{
    For arg;

    if (forVar == NULL || forBuf == NULL || forLst == NULL)
	return;
    arg.var = forVar;
    arg.buf = forBuf;
    arg.lst = forLst;
    arg.lineno = forLineno;
    forVar = NULL;
    forBuf = NULL;
    forLst = NULL;

    Lst_ForEach(arg.lst, ForExec, (ClientData) &arg);

    free((Address)arg.var);
    Lst_Destroy(arg.lst, (void (*) __P((ClientData))) free);
    Buf_Destroy(arg.buf, TRUE);
}
