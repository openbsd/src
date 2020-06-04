/* $OpenBSD: main.c,v 1.34 2020/05/24 17:31:54 espie Exp $	 */
/* $NetBSD: main.c,v 1.5 1996/03/19 03:21:38 jtc Exp $	 */

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

#include <sys/types.h>
#include <fcntl.h>
#include <paths.h>
#include <stdlib.h>
#include <unistd.h>
#include "defs.h"

char dflag;
char lflag;
char rflag;
char tflag;
char vflag;

char *symbol_prefix;
char *file_prefix = "y";

int lineno;
int outline;

int explicit_file_name;

char *code_file_name;
char *defines_file_name;
char *input_file_name = "";
char *output_file_name;
char *verbose_file_name;

FILE *action_file;	/* a temp file, used to save actions associated    */
			/* with rules until the parser is written	   */
FILE *code_file;	/* y.code.c (used when the -r option is specified) */
FILE *defines_file;	/* y.tab.h					   */
FILE *input_file;	/* the input file				   */
FILE *output_file;	/* y.tab.c					   */
FILE *text_file;	/* a temp file, used to save text until all	   */
			/* symbols have been defined			   */
FILE *union_file;	/* a temp file, used to save the union		   */
			/* definition until all symbol have been	   */
			/* defined					   */
FILE *verbose_file;	/* y.output					   */

int nitems;
int nrules;
int nsyms;
int ntokens;
int nvars;

int start_symbol;
char **symbol_name;
short *symbol_value;
short *symbol_prec;
char *symbol_assoc;

short *ritem;
short *rlhs;
short *rrhs;
short *rprec;
char *rassoc;
short **derives;
char *nullable;

void usage(void);
void getargs(int, char *[]);
void create_file_names(void);
void open_files(void);

void
usage(void)
{
	fprintf(stderr, "usage: %s [-dlrtv] [-b file_prefix] [-o output_file] [-p symbol_prefix] file\n", __progname);
	exit(1);
}


void
getargs(int argc, char *argv[])
{
	int ch;

	while ((ch = getopt(argc, argv, "b:dlo:p:rtv")) != -1) {
		switch (ch) {
		case 'b':
			file_prefix = optarg;
			break;

		case 'd':
			dflag = 1;
			break;

		case 'l':
			lflag = 1;
			break;

		case 'o':
			output_file_name = optarg;
			explicit_file_name = 1;
			break;

		case 'p':
			symbol_prefix = optarg;
			break;

		case 'r':
			rflag = 1;
			break;

		case 't':
			tflag = 1;
			break;

		case 'v':
			vflag = 1;
			break;

		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 1)
		usage();
	if (strcmp(*argv, "-") == 0)
		input_file = stdin;
	else
		input_file_name = *argv;
}


void *
allocate(size_t n)
{
	void *v;

	v = NULL;
	if (n) {
		v = calloc(1, n);
		if (!v)
			no_space();
	}
	return (v);
}

void
create_file_names(void)
{
	if (output_file_name == NULL) {
		if (asprintf(&output_file_name, "%s%s", file_prefix, OUTPUT_SUFFIX)
		    == -1)
			no_space();
	}
	if (rflag) {
		if (asprintf(&code_file_name, "%s%s", file_prefix, CODE_SUFFIX) == -1)
			no_space();
	} else
		code_file_name = output_file_name;

	if (dflag) {
		if (explicit_file_name) {
			char *suffix;

			defines_file_name = strdup(output_file_name);
			if (defines_file_name == 0)
				no_space();

			/* does the output_file_name have a known suffix */
			if ((suffix = strrchr(output_file_name, '.')) != 0 &&
			    (!strcmp(suffix, ".c") ||	/* good, old-fashioned C */
			     !strcmp(suffix, ".C") ||	/* C++, or C on Windows */
			     !strcmp(suffix, ".cc") ||	/* C++ */
			     !strcmp(suffix, ".cxx") ||	/* C++ */
			     !strcmp(suffix, ".cpp"))) {/* C++ (Windows) */
				strncpy(defines_file_name, output_file_name,
					suffix - output_file_name + 1);
				defines_file_name[suffix - output_file_name + 1] = 'h';
				defines_file_name[suffix - output_file_name + 2] = '\0';
			} else {
				fprintf(stderr, "%s: suffix of output file name %s"
				 " not recognized, no -d file generated.\n",
					__progname, output_file_name);
				dflag = 0;
				free(defines_file_name);
				defines_file_name = 0;
			}
		} else {
			if (asprintf(&defines_file_name, "%s%s", file_prefix,
				     DEFINES_SUFFIX) == -1)
				no_space();
		}
	}
	if (vflag) {
		if (asprintf(&verbose_file_name, "%s%s", file_prefix,
			     VERBOSE_SUFFIX) == -1)
			no_space();
	}
}


FILE *
create_temp(void)
{
	FILE *f;

	f = tmpfile();
	if (f == NULL)
		tempfile_error();
	return f;
}

void
open_files(void)
{
	create_file_names();

	if (input_file == NULL) {
		input_file = fopen(input_file_name, "r");
		if (input_file == NULL)
			open_error(input_file_name);
	}
	action_file = create_temp();

	text_file = create_temp();

	if (vflag) {
		verbose_file = fopen(verbose_file_name, "w");
		if (verbose_file == NULL)
			open_error(verbose_file_name);
	}
	if (dflag) {
		defines_file = fopen(defines_file_name, "w");
		if (defines_file == NULL)
			open_write_error(defines_file_name);
		union_file = create_temp();
	}
	output_file = fopen(output_file_name, "w");
	if (output_file == NULL)
		open_error(output_file_name);

	if (rflag) {
		code_file = fopen(code_file_name, "w");
		if (code_file == NULL)
			open_error(code_file_name);
	} else
		code_file = output_file;
}


int
main(int argc, char *argv[])
{
	if (pledge("stdio rpath wpath cpath", NULL) == -1)
		fatal("pledge: invalid arguments");

	getargs(argc, argv);
	open_files();
	reader();
	lr0();
	lalr();
	make_parser();
	verbose();
	output();
	return (0);
}
