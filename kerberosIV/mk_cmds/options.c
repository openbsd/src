/*	$Id: options.c,v 1.1.1.1 1995/12/14 06:52:47 tholo Exp $	*/

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
#define NO_SS_ERR_H
#include <ss/ss.h>

struct option {
     char *text;
     long value;
};

static struct option options[] = {
     { "dont_list", SS_OPT_DONT_LIST },
     { "^list", SS_OPT_DONT_LIST },
     { "dont_summarize", SS_OPT_DONT_SUMMARIZE },
     { "^summarize", SS_OPT_DONT_SUMMARIZE },
     { (char *)NULL, 0 }
};

long
flag_val(string)
     register char *string;
{
     register struct option *opt;
     for (opt = options; opt->text; opt++)
	  if (!strcmp(opt->text, string))
	       return(opt->value);
     return(0);
}
