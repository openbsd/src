/*	$Id: mk_cmds.c,v 1.1.1.1 1995/12/14 06:52:48 tholo Exp $	*/

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
#include <sys/types.h>
#include <sys/param.h>
#include <sys/file.h>
#ifndef NPOSIX
#include <string.h>
#else
#include <strings.h>
#endif
#include "mk_cmds_defs.h"

static const char copyright[] =
    "Copyright 1987 by MIT Student Information Processing Board";

extern pointer malloc PROTOTYPE((unsigned));
extern char *last_token;
extern FILE *output_file;

extern FILE *yyin, *yyout;
extern unsigned lineno;

main(argc, argv)
    int argc;
    char **argv;
{
    char c_file[MAXPATHLEN];
    int result;
    char *path, *p;

    if (argc != 2) {
	fputs("Usage: ", stderr);
	fputs(argv[0], stderr);
	fputs("cmdtbl.ct\n", stderr);
	exit(1);
    }

    path = malloc(strlen(argv[1])+4); /* extra space to add ".ct" */
    strcpy(path, argv[1]);
    p = strrchr(path, '/');
    if (p == (char *)NULL)
	p = path;
    else
	p++;
    p = strrchr(p, '.');
    if (p == (char *)NULL || strcmp(p, ".ct"))
	strcat(path, ".ct");
    yyin = fopen(path, "r");
    if (!yyin) {
	perror(path);
	exit(1);
    }

    p = strrchr(path, '.');
    *p = '\0';
    strcpy(c_file, path);
    strcat(c_file, ".c");
    *p = '.';

    output_file = fopen(c_file, "w+");
    if (!output_file) {
	perror(c_file);
	exit(1);
    }

    fputs("/* ", output_file);	/* emacs fix -> */
    fputs(c_file, output_file);
    fputs(" - automatically generated from ", output_file);
    fputs(path, output_file);
    fputs(" */\n", output_file);
    fputs("#include <ss/ss.h>\n\n", output_file);
    fputs("#ifndef __STDC__\n#define const\n#endif\n\n", output_file);
    /* parse it */
    result = yyparse();
    /* put file descriptors back where they belong */
    fclose(yyin);		/* bye bye input file */
    fclose(output_file);	/* bye bye output file */

    return result;
}

yyerror(s)
    char *s;
{
    fputs(s, stderr);
    fprintf(stderr, "\nLine %d; last token was '%s'\n",
	    lineno, last_token);
}
