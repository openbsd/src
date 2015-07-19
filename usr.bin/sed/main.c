/*	$OpenBSD: main.c,v 1.23 2015/07/19 06:14:23 sthen Exp $	*/

/*-
 * Copyright (c) 1992 Diomidis Spinellis.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Diomidis Spinellis of Imperial College, University of London.
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
#include <sys/stat.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <regex.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>

#include "defs.h"
#include "extern.h"

/*
 * Linked list of units (strings and files) to be compiled
 */
struct s_compunit {
	struct s_compunit *next;
	enum e_cut {CU_FILE, CU_STRING} type;
	char *s;			/* Pointer to string or fname */
};

/*
 * Linked list pointer to compilation units and pointer to current
 * next pointer.
 */
static struct s_compunit *script, **cu_nextp = &script;

/*
 * Linked list of files to be processed
 */
struct s_flist {
	char *fname;
	struct s_flist *next;
};

/*
 * Linked list pointer to files and pointer to current
 * next pointer.
 */
static struct s_flist *files, **fl_nextp = &files;

FILE *infile;			/* Current input file */
FILE *outfile;			/* Current output file */

int Eflag, aflag, eflag, nflag;
static int rval;	/* Exit status */

/*
 * Current file and line number; line numbers restart across compilation
 * units, but span across input files.  The latter is optional if editing
 * in place.
 */
const char *fname;		/* File name. */
const char *outfname;		/* Output file name */
static char oldfname[PATH_MAX];	/* Old file name (for in-place editing) */
static char tmpfname[PATH_MAX];	/* Temporary file name (for in-place editing) */
char *inplace;			/* Inplace edit file extension */
u_long linenum;

static void add_compunit(enum e_cut, char *);
static void add_file(char *);
static int next_files_have_lines(void);

int
main(int argc, char *argv[])
{
	int c, fflag;

	fflag = 0;
	inplace = NULL;
	while ((c = getopt(argc, argv, "Eae:f:i::nru")) != -1)
		switch (c) {
		case 'E':
		case 'r':
			Eflag = 1;
			break;
		case 'a':
			aflag = 1;
			break;
		case 'e':
			eflag = 1;
			add_compunit(CU_STRING, optarg);
			break;
		case 'f':
			fflag = 1;
			add_compunit(CU_FILE, optarg);
			break;
		case 'i':
			inplace = optarg ? optarg : "";
			break;
		case 'n':
			nflag = 1;
			break;
		case 'u':
			setvbuf(stdout, NULL, _IOLBF, 0);
			break;
		default:
		case '?':
			(void)fprintf(stderr,
			    "usage: sed [-aEnru] [-i[extension]] command [file ...]\n"
			    "       sed [-aEnru] [-e command] [-f command_file] [-i[extension]] [file ...]\n");
			exit(1);
		}
	argc -= optind;
	argv += optind;

	/* First usage case; script is the first arg */
	if (!eflag && !fflag && *argv) {
		add_compunit(CU_STRING, *argv);
		argv++;
	}

	compile();

	/* Continue with first and start second usage */
	if (*argv)
		for (; *argv; argv++)
			add_file(*argv);
	else
		add_file(NULL);
	process();
	cfclose(prog, NULL);
	if (fclose(stdout))
		err(FATAL, "stdout: %s", strerror(errno));
	exit (rval);
}

/*
 * Like fgets, but go through the chain of compilation units chaining them
 * together.  Empty strings and files are ignored.
 */
char *
cu_fgets(char **outbuf, size_t *outsize)
{
	static enum {ST_EOF, ST_FILE, ST_STRING} state = ST_EOF;
	static FILE *f;		/* Current open file */
	static char *s;		/* Current pointer inside string */
	static char string_ident[30];
	size_t len;
	char *p;

	if (*outbuf == NULL)
		*outsize = 0;

again:
	switch (state) {
	case ST_EOF:
		if (script == NULL)
			return (NULL);
		linenum = 0;
		switch (script->type) {
		case CU_FILE:
			if ((f = fopen(script->s, "r")) == NULL)
				err(FATAL,
				    "%s: %s", script->s, strerror(errno));
			fname = script->s;
			state = ST_FILE;
			goto again;
		case CU_STRING:
			if ((snprintf(string_ident,
			    sizeof(string_ident), "\"%s\"", script->s)) >=
			    sizeof(string_ident))
				strlcpy(string_ident +
				    sizeof(string_ident) - 6, " ...\"", 5);
			fname = string_ident;
			s = script->s;
			state = ST_STRING;
			goto again;
		}
	case ST_FILE:
		if ((p = fgetln(f, &len)) != NULL) {
			linenum++;
			if (len >= *outsize) {
				free(*outbuf);
				*outsize = ROUNDLEN(len + 1);
				*outbuf = xmalloc(*outsize);
			}
			memcpy(*outbuf, p, len);
			(*outbuf)[len] = '\0';
			if (linenum == 1 && p[0] == '#' && p[1] == 'n')
				nflag = 1;
			return (*outbuf);
		}
		script = script->next;
		(void)fclose(f);
		state = ST_EOF;
		goto again;
	case ST_STRING:
		if (linenum == 0 && s[0] == '#' && s[1] == 'n')
			nflag = 1;
		p = *outbuf;
		len = *outsize;
		for (;;) {
			if (len <= 1) {
				*outbuf = xrealloc(*outbuf,
				    *outsize + _POSIX2_LINE_MAX);
				p = *outbuf + *outsize - len;
				len += _POSIX2_LINE_MAX;
				*outsize += _POSIX2_LINE_MAX;
			}
			switch (*s) {
			case '\0':
				state = ST_EOF;
				if (s == script->s) {
					script = script->next;
					goto again;
				} else {
					script = script->next;
					*p = '\0';
					linenum++;
					return (*outbuf);
				}
			case '\n':
				*p++ = '\n';
				*p = '\0';
				s++;
				linenum++;
				return (*outbuf);
			default:
				*p++ = *s++;
				len--;
			}
		}
	}
	/* NOTREACHED */
}

/*
 * Like fgets, but go through the list of files chaining them together.
 * Set len to the length of the line.
 */
int
mf_fgets(SPACE *sp, enum e_spflag spflag)
{
	struct stat sb;
	size_t len;
	char *p;
	int c, fd;
	static int firstfile;

	if (infile == NULL) {
		/* stdin? */
		if (files->fname == NULL) {
			if (inplace != NULL)
				err(FATAL, "-i may not be used with stdin");
			infile = stdin;
			fname = "stdin";
			outfile = stdout;
			outfname = "stdout";
		}

		firstfile = 1;
	}

	for (;;) {
		if (infile != NULL && (c = getc(infile)) != EOF) {
			(void)ungetc(c, infile);
			break;
		}
		/* If we are here then either eof or no files are open yet */
		if (infile == stdin) {
			sp->len = 0;
			return (0);
		}
		if (infile != NULL) {
			fclose(infile);
			if (*oldfname != '\0') {
				if (rename(fname, oldfname) != 0) {
					err(WARNING, "rename()");
					unlink(tmpfname);
					exit(1);
				}
				*oldfname = '\0';
			}
			if (*tmpfname != '\0') {
				if (outfile != NULL && outfile != stdout)
					fclose(outfile);
				outfile = NULL;
				rename(tmpfname, fname);
				*tmpfname = '\0';
			}
			outfname = NULL;
		}
		if (firstfile == 0)
			files = files->next;
		else
			firstfile = 0;
		if (files == NULL) {
			sp->len = 0;
			return (0);
		}
		fname = files->fname;
		if (inplace != NULL) {
			if (lstat(fname, &sb) != 0)
				err(1, "%s: %s", fname,
				    strerror(errno ? errno : EIO));
			if (!S_ISREG(sb.st_mode))
				err(FATAL, "%s: %s %s", fname,
				    "in-place editing only",
				    "works for regular files");
			if (*inplace != '\0') {
				strlcpy(oldfname, fname,
				    sizeof(oldfname));
				len = strlcat(oldfname, inplace,
				    sizeof(oldfname));
				if (len > sizeof(oldfname))
					err(FATAL, "%s: name too long", fname);
			}
			len = snprintf(tmpfname, sizeof(tmpfname), "%s/sedXXXXXXXXXX",
			    dirname(fname));
			if (len >= sizeof(tmpfname))
				err(FATAL, "%s: name too long", fname);
			if ((fd = mkstemp(tmpfname)) == -1)
				err(FATAL, "%s", fname);
			if ((outfile = fdopen(fd, "w")) == NULL) {
				unlink(tmpfname);
				err(FATAL, "%s", fname);
			}
			fchown(fileno(outfile), sb.st_uid, sb.st_gid);
			fchmod(fileno(outfile), sb.st_mode & ALLPERMS);
			outfname = tmpfname;
			linenum = 0;
			resetranges();
		} else {
			outfile = stdout;
			outfname = "stdout";
		}
		if ((infile = fopen(fname, "r")) == NULL) {
			err(WARNING, "%s", fname);
			rval = 1;
			continue;
		}
	}

	/*
	 * We are here only when infile is open and we still have something
	 * to read from it.
	 *
	 * Use fgetln so that we can handle essentially infinite input data.
	 * Can't use the pointer into the stdio buffer as the process space
	 * because the ungetc() can cause it to move.
	 */
	p = fgetln(infile, &len);
	if (ferror(infile))
		err(FATAL, "%s: %s", fname, strerror(errno ? errno : EIO));
	if (len != 0 && p[len - 1] == '\n') {
		sp->append_newline = 1;
		len--;
	} else if (!lastline()) {
		sp->append_newline = 1;
	} else {
		sp->append_newline = 0;
	}
	cspace(sp, p, len, spflag);

	linenum++;

	return (1);
}

/*
 * Add a compilation unit to the linked list
 */
static void
add_compunit(enum e_cut type, char *s)
{
	struct s_compunit *cu;

	cu = xmalloc(sizeof(struct s_compunit));
	cu->type = type;
	cu->s = s;
	cu->next = NULL;
	*cu_nextp = cu;
	cu_nextp = &cu->next;
}

/*
 * Add a file to the linked list
 */
static void
add_file(char *s)
{
	struct s_flist *fp;

	fp = xmalloc(sizeof(struct s_flist));
	fp->next = NULL;
	*fl_nextp = fp;
	fp->fname = s;
	fl_nextp = &fp->next;
}


static int
next_files_have_lines()
{
	struct s_flist *file;
	FILE *file_fd;
	int ch;

	file = files;
	while ((file = file->next) != NULL) {
		if ((file_fd = fopen(file->fname, "r")) == NULL)
			continue;

		if ((ch = getc(file_fd)) != EOF) {
			/*
			 * This next file has content, therefore current
			 * file doesn't contains the last line.
			 */
			ungetc(ch, file_fd);
			fclose(file_fd);
			return (1);
		}
		fclose(file_fd);
	}
	return (0);
}

int
lastline(void)
{
	int ch;

	if (feof(infile)) {
		return !(
		    (inplace == NULL) &&
		    next_files_have_lines());
	}
	if ((ch = getc(infile)) == EOF) {
		return !(
		    (inplace == NULL) &&
		    next_files_have_lines());
	}
	ungetc(ch, infile);
	return (0);
}
