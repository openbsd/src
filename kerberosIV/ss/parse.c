/*	$Id: parse.c,v 1.1.1.1 1995/12/14 06:52:47 tholo Exp $	*/

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
#include <errno.h>

#include "ss_internal.h"

#ifndef lint
static char const rcsid[] =
    "$Id: parse.c,v 1.1.1.1 1995/12/14 06:52:47 tholo Exp $";
#endif

enum parse_mode { WHITESPACE, TOKEN, QUOTED_STRING };

/*
 * parse(line_ptr, argc_ptr)
 *
 * Function:
 *      Parses line, dividing at whitespace, into tokens, returns
 *      the "argc" and "argv" values.
 * Arguments:
 *      line_ptr (char *)
 *              Pointer to text string to be parsed.
 *      argc_ptr (int *)
 *              Where to put the "argc" (number of tokens) value.
 * Returns:
 *      argv (char **)
 *              Series of pointers to parsed tokens.
 */

#define NEW_ARGV(old,n) (char **)realloc((char *)old,\
					 (unsigned)(n+2)*sizeof(char*))

char **
ss_parse (sci_idx, line_ptr, argc_ptr)
    int sci_idx;
    register char *line_ptr;
    int *argc_ptr;
{
    register char **argv, *cp;
    register int argc;
    register enum parse_mode parse_mode;

    *argc_ptr = 0;	/* In case of error return something sane */
    argv = (char **) malloc (sizeof(char *));
    if (argv == (char **)NULL) {
	ss_error(sci_idx, errno, "Can't allocate storage");
	return(argv);
    }
    *argv = (char *)NULL;

    argc = 0;

    parse_mode = WHITESPACE;	/* flushing whitespace */
    cp = line_ptr;		/* cp is for output */
    while (1) {
#ifdef DEBUG
	{
	    printf ("character `%c', mode %d\n", *line_ptr, parse_mode);
	}
#endif
	while (parse_mode == WHITESPACE) {
	    if (*line_ptr == '\0')
		goto end_of_line;
	    if (*line_ptr == ' ' || *line_ptr == '\t') {
		line_ptr++;
		continue;
	    }
	    if (*line_ptr == '"') {
		/* go to quoted-string mode */
		parse_mode = QUOTED_STRING;
		cp = line_ptr++;
		argv = NEW_ARGV (argv, argc);
		argv[argc++] = cp;
		argv[argc] = NULL;
	    }
	    else {
		/* random-token mode */
		parse_mode = TOKEN;
		cp = line_ptr;
		argv = NEW_ARGV (argv, argc);
		argv[argc++] = line_ptr;
		argv[argc] = NULL;
	    }
	}
	while (parse_mode == TOKEN) {
	    if (*line_ptr == '\0') {
		*cp++ = '\0';
		goto end_of_line;
	    }
	    else if (*line_ptr == ' ' || *line_ptr == '\t') {
		*cp++ = '\0';
		line_ptr++;
		parse_mode = WHITESPACE;
	    }
	    else if (*line_ptr == '"') {
		line_ptr++;
		parse_mode = QUOTED_STRING;
	    }
	    else {
		*cp++ = *line_ptr++;
	    }
	}
	while (parse_mode == QUOTED_STRING) {
	    if (*line_ptr == '\0') {
		ss_error (sci_idx, 0,
			  "Unbalanced quotes in command line");
		free (argv);
		return NULL;
	    }
	    else if (*line_ptr == '"') {
		if (*++line_ptr == '"') {
		    *cp++ = '"';
		    line_ptr++;
		}
		else {
		    parse_mode = TOKEN;
		}
	    }
	    else {
		*cp++ = *line_ptr++;
	    }
	}
    }
end_of_line:
    *argc_ptr = argc;
#ifdef DEBUG
    {
	int i;
	printf ("argc = %d\n", argc);
	for (i = 0; i <= argc; i++)
	    printf ("\targv[%2d] = `%s'\n", i,
		    argv[i] ? argv[i] : "<NULL>");
    }
#endif
    return(argv);
}
