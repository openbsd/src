/*
 * GSP assembler main program
 *
 * Copyright (c) 1993 Paul Mackerras.
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
 *      This product includes software developed by Paul Mackerras.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/param.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <setjmp.h>
#include <string.h>
#include <errno.h>
#include <err.h>
#include <unistd.h>
#include "gsp_ass.h"
#include "y.tab.h"

#define YYDEBUG_VALUE	0

extern	YYSTYPE yylval;
int	err_count;

char	line[MAXLINE];
int	lineno;

extern	int yydebug;
short	pass2;

unsigned	int pc;
unsigned	int highest_pc;
unsigned	int line_pc;

FILE	*infile;
FILE	*current_infile;
FILE	*objfile;
FILE	*listfile;

char	in_name[PATH_MAX];

struct	input {
	FILE	*fp;
	struct input *next;
	int	lineno;
	char	name[128];
} *pending_input;

jmp_buf	synerrjmp;

main(int argc, char **argv)
{
	char	*hex_name, *list_name;
	short	in_given, hex_given, list_given;
	int	i, v;
	int	c;
	char	*p;

	hex_name = list_name = 0;

	/* parse options */
	while ((c = getopt(argc, argv, "o:l:")) != -1) {
		switch (c) {
		case 'o':
			if (hex_name)
				usage();
			hex_name = optarg;
			break;
		case 'l':
			if (list_name)
				usage();
			list_name = optarg;
			break;
		default:
			usage();
		}
	}

	/* get source file */
	argc -= optind;
	argv += optind;
	if (argc == 0) {
		infile = stdin;
		strcpy(in_name, "<stdin>");
	} else if (argc == 1) {
		strlcpy(in_name, *argv, sizeof(in_name));
		if ((infile = fopen(in_name, "r")) == NULL)
			err(1, "fopen");
	} else 
		usage();

	/* Pass 1 */
	pending_input = NULL;
	current_infile = infile;

	yydebug = YYDEBUG_VALUE;
	pass2 = 0;
	pc = 0;
	lineno = 0;
	while (get_line(line, MAXLINE)) {
		if( !setjmp(synerrjmp)) {
			lex_init(line);
			yyparse();
		}
	}
	if (err_count > 0)
		exit(1);

	/* Open output files */
	if (hex_name == 0)
		objfile = stdout;
	else if ((objfile = fopen(hex_name, "w")) == NULL)
		err(1, "fopen");
	if (list_name)
		if ((listfile = fopen(list_name, "w")) == NULL)
			err(1, "fopen");

	/* Pass 2 */
	pass2 = 1;
	rewind(infile);
	current_infile = infile;
	pc = 0;
	lineno = 0;
	reset_numeric_labels();
	while (get_line(line, MAXLINE)) {
		line_pc = pc;
		if (!setjmp(synerrjmp)) {
			lex_init(line);
			yyparse();
		}
		listing();
	}

	exit(err_count != 0);
}

setext(char *out, const char *in, const char *ext)
{
	const	char *p;

	p = strrchr(in, '.');
	if (p != NULL) {
		memcpy(out, in, p - in);
		strcpy(out + (p - in), ext);
	} else {
		strcpy(out, in);
		strcat(out, ext);
	}
}

push_input(char *fn)
{
	FILE *f;
	struct input *p;

	f = fopen(fn, "r");
	if (f == NULL) {
		p1err("Can't open input file %s", fn);
		return;
	}
	ALLOC(p, struct input *);
	p->fp = current_infile;
	p->lineno = lineno;
	strcpy(p->name, in_name);
	p->next = pending_input;
	current_infile = f;
	lineno = 1;
	strcpy(in_name, fn);
	pending_input = p;
}

int
get_line(char *lp, int maxlen)
{
	struct	input *p;

	while (fgets(lp, maxlen, current_infile) == NULL) {
		if ((p = pending_input) == NULL)
			return 0;
		/* pop the input stack */
		fclose(current_infile);
		current_infile = p->fp;
		strcpy(in_name, p->name);
		lineno = p->lineno;
		pending_input = p->next;
		free(p);
	}
	++lineno;
	return 1;
}

void
perr(char *fmt, ...)
{
	va_list	ap;
	char	error_string[256];

	if (!pass2)
		return;
	fprintf(stderr, "Error in line %d: ", lineno);
	va_start(ap, fmt);
	vsprintf(error_string, fmt, ap);
	va_end(ap);
	fprintf(stderr, "%s\n", error_string);
	list_error(error_string);
	++err_count;
}

void
p1err(char *fmt, ...)
{
	va_list ap;

	fprintf(stderr, "Pass 1 error in line %d: ", lineno);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");
	++err_count;
}

void
yyerror(char *err)
{
	extern	char *lineptr;

	perr("%s", err);
	longjmp(synerrjmp, 1);
}

char *
alloc(size_t nbytes)
{
	char	*p;

	if ((p = malloc(nbytes)) == NULL) {
		fprintf(stderr, "Insufficient memory at line %d\n", lineno);
		exit(1);
	}
	return p;
}

int
usage()
{
	fprintf(stderr,
		"Usage: gspa [infile] [+o|-o hex_file] [+l|-l list_file]\n");
	exit(1);
}
