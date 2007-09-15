/*	$OpenBSD: main.c,v 1.2 2007/09/15 22:04:38 ray Exp $	*/

/*
 * Copyright (c) 2002 Anders Magnusson. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
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

#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>

#include "pass1.h"
#include "pass2.h"

int sflag, nflag, oflag, kflag;
int lflag, odebug, rdebug, radebug, vdebug, s2debug, udebug, x2debug;
#if !defined(MULTIPASS) || defined(PASST)
int iTflag, oTflag;
#endif
int xdebug, mdebug, sdebug, gflag, c2debug, pdebug;
int Wstrict_prototypes, Wmissing_prototypes, Wimplicit_int,
	Wimplicit_function_declaration;
int xssaflag, xtailcallflag, xtemps, xdeljumps;

int e2debug, t2debug, f2debug, b2debug;

struct suedef btdims[24];
char *prgname;

static void prtstats(void);

static struct {
	char *n; int *f;
} flagstr[] = {
	{ "strict-prototypes", &Wstrict_prototypes, },
	{ "missing-prototypes", &Wmissing_prototypes, },
	{ "implicit-int", &Wimplicit_int, },
	{ "implicit-function-declaration", &Wimplicit_function_declaration, },
	{ NULL, NULL, },
};

static void
usage(void)
{
	(void)fprintf(stderr, "usage: %s [option] [infile] [outfile]...\n",
	    prgname);
	exit(1);
}

static void
segvcatch(int a)
{
	fprintf(stderr, "%sinternal compiler error: %s, line %d\n",
	    nerrors ? "" : "major ", ftitle, lineno);
	fflush(stderr);
	exit(1);
}

/*
 * "emulate" the gcc warning flags.
 */
static void
Wflags(char *str)
{
	int i, found = 0, all;

	if (strcmp(str, "implicit") == 0) {
		Wimplicit_int = Wimplicit_function_declaration = 1;
		return;
	}
	all = strcmp(str, "W") == 0;
	for (i = 0; flagstr[i].n; i++)
		if (all || strcmp(flagstr[i].n, str) == 0) {
			*flagstr[i].f = 1;
			found++;
		}
	if (found == 0)
		usage();
}


/* control multiple files */
int
main(int argc, char *argv[])
{

	int ch;

	prgname = argv[0];

	while ((ch = getopt(argc, argv, "VlwX:Z:W:sOT:gx:k")) != -1)
		switch (ch) {
#if !defined(MULTIPASS) || defined(PASS1)
		case 'X':
			while (*optarg)
				switch (*optarg++) {
				case 'd': ++ddebug; break; /* declarations */
				case 'i': ++idebug; break; /* initializations */
				case 'b': ++bdebug; break;
				case 't': ++tdebug; break;
				case 'e': ++edebug; break; /* pass1 exit */
				case 'x': ++xdebug; break; /* MD code */
				case 's': ++sdebug; break;
				case 'n': ++nflag; break;
				case 'o': ++oflag; break;
				case 'p': ++pdebug; break; /* prototype */
				default:
					fprintf(stderr, "unknown X flag '%c'\n",
					    optarg[-1]);
					exit(1);
				}
#endif
			break;
#if !defined(MULTIPASS) || defined(PASST)
		case 'T':
			while (*optarg)
				switch (*optarg++) {
				case 'i': ++iTflag; break;
				case 'o': ++oTflag; break;
				case 'n': ++nflag; break;
				default:
					fprintf(stderr, "unknown T flag '%c'\n",
					    optarg[-1]);
					exit(1);
				}
#endif
			break;
#if !defined(MULTIPASS) || defined(PASS2)
		case 'Z':
			while (*optarg)
				switch (*optarg++) {
				case 'f': /* instruction matching */
					++f2debug;
					break;
				case 'e': /* print tree upon pass2 enter */
					++e2debug;
					break;
				case 'o': ++odebug; break;
				case 'r': /* register alloc/graph coloring */
					++rdebug;
					break;
				case 'a': ++radebug; break;
				case 'b': /* basic block and SSA building */
					++b2debug;
					break;
				case 'c': /* code printout */
					++c2debug;
					break;
				case 'm': ++mdebug; break;
				case 'v': ++vdebug; break;
				case 't': ++t2debug; break;
				case 's': /* shape matching */
					++s2debug;
					break;
				case 'u': /* Sethi-Ullman debugging */
					++udebug;
					break;
				case 'x': ++x2debug; break;
				case 'n': ++nflag; break;
				default:
					fprintf(stderr, "unknown Z flag '%c'\n",
					    optarg[-1]);
					exit(1);
				}
#endif
			break;

		case 'k': /* PIC code */
			++kflag;
			break;

		case 'l': /* linenos */
			++lflag;
			break;

		case 'g': /* Debugging */
			gflag = 1;
			break;

		case 's': /* Statistics */
			++sflag;
			break;

		case 'W': /* Enable different warnings */
			Wflags(optarg);
			break;

		case 'x': /* Different optimizations */
			if (strcmp(optarg, "ssa") == 0)
				xssaflag++;
			else if (strcmp(optarg, "tailcall") == 0)
				xtailcallflag++;
			else if (strcmp(optarg, "temps") == 0)
				xtemps++;
			else if (strcmp(optarg, "deljumps") == 0)
				xdeljumps++;
			else
				usage();
			break;

		case '?':
		default:
			usage();
		}
		argc -= optind;
		argv += optind;

		if (argc != 0) {
			if (freopen(argv[0], "r", stdin) == NULL) {
				fprintf(stderr, "open input file '%s':",
				    argv[0]);
				perror(NULL);
				exit(1);
			}
			if (argc != 1)
				if (freopen(argv[1], "w", stdout) == NULL) {
					fprintf(stderr, "open output file '%s':",
					    argv[1]);
					perror(NULL);
					exit(1);
				}
		}

	mkdope();
	signal(SIGSEGV, segvcatch);
	fregs = FREGS;	/* number of free registers */
	lineno = 1;
#ifdef GCC_COMPAT
	gcc_init();
#endif

	/* dimension table initialization */

	btdims[VOID].suesize = 0;
	btdims[BOOL].suesize = SZBOOL;
	btdims[CHAR].suesize = SZCHAR;
	btdims[INT].suesize = SZINT;
	btdims[FLOAT].suesize = SZFLOAT;
	btdims[DOUBLE].suesize = SZDOUBLE;
	btdims[LDOUBLE].suesize = SZLDOUBLE;
	btdims[LONG].suesize = SZLONG;
	btdims[LONGLONG].suesize = SZLONGLONG;
	btdims[SHORT].suesize = SZSHORT;
	btdims[UCHAR].suesize = SZCHAR;
	btdims[USHORT].suesize = SZSHORT;
	btdims[UNSIGNED].suesize = SZINT;
	btdims[ULONG].suesize = SZLONG;
	btdims[ULONGLONG].suesize = SZLONGLONG;
	/* starts past any of the above */
	reached = 1;

	bjobcode();

#ifdef STABS
	if (gflag) {
		stabs_file(argc ? argv[0] : "");
		stabs_init();
	}
#endif

	(void) yyparse();
	yyaccpt();

	ejobcode( nerrors ? 1 : 0 );
	lcommprint();

	if (sflag)
		prtstats();
	return(nerrors?1:0);

}

void
prtstats(void)
{
	extern int nametabs, namestrlen, tmpallocsize, permallocsize;
	extern int lostmem, arglistcnt, dimfuncnt, inlnodecnt, inlstatcnt;
	extern int symtabcnt, suedefcnt;

	fprintf(stderr, "Name table entries:		%d pcs\n", nametabs);
	fprintf(stderr, "Name string size:		%d B\n", namestrlen);
	fprintf(stderr, "Permanent allocated memory:	%d B\n", permallocsize);
	fprintf(stderr, "Temporary allocated memory:	%d B\n", tmpallocsize);
	fprintf(stderr, "Lost memory:			%d B\n", lostmem);
	fprintf(stderr, "Argument list unions:		%d pcs\n", arglistcnt);
	fprintf(stderr, "Dimension/function unions:	%d pcs\n", dimfuncnt);
	fprintf(stderr, "Struct/union/enum blocks:	%d pcs\n", suedefcnt);
	fprintf(stderr, "Inline node count:		%d pcs\n", inlnodecnt);
	fprintf(stderr, "Inline control blocks:		%d pcs\n", inlstatcnt);
	fprintf(stderr, "Permanent symtab entries:	%d pcs\n", symtabcnt);
}
