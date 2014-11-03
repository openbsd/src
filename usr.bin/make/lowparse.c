/*	$OpenBSD: lowparse.c,v 1.33 2014/11/03 12:48:37 espie Exp $ */

/* low-level parsing functions. */

/*
 * Copyright (c) 1999,2000 Marc Espie.
 *
 * Extensive code changes for the OpenBSD project.
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

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "config.h"
#include "defines.h"
#include "buf.h"
#include "lowparse.h"
#include "error.h"
#include "lst.h"
#include "memory.h"
#include "pathnames.h"
#ifndef LOCATION_TYPE
#include "location.h"
#endif
#include "var.h"


#define READ_MAKEFILES "MAKEFILE_LIST"

/* Input stream structure: file or string.
 * Files have str == NULL, F != NULL.
 * Strings have F == NULL, str != NULL.
 */
struct input_stream {
	Location origin;	/* Name of file and line number */
	FILE *F;		/* Open stream, or NULL if pure string. */
	char *str;		/* Input string, if F == NULL. */

	/* Line buffer. */
	char *ptr;		/* Where we are. */
	char *end;		/* Don't overdo it. */
};

static struct input_stream *current;	/* the input_stream being parsed. */

static LIST input_stack;	/* Stack of input_stream waiting to be parsed
				 * (includes and loop reparses) */

/* record gnode location for proper reporting at runtime */
static Location *post_parse = NULL;

/* input_stream ctors.
 *
 * obj = new_input_file(filename, filehandle);
 *	Create input stream from filename, filehandle. */
static struct input_stream *new_input_file(const char *, FILE *);
/* obj = new_input_string(str, origin);
 *	Create input stream from str, origin. */
static struct input_stream *new_input_string(char *, const Location *);
/* free_input_stream(obj);
 *	Discard consumed input stream, closing files, freeing memory.  */
static void free_input_stream(struct input_stream *);


/* Handling basic character reading.
 * c = read_char();
 *	New character c from current input stream, or EOF at end of stream. */
#define read_char()	\
    current->ptr < current->end ? *current->ptr++ : grab_new_line_and_readchar()
/* char = grab_new_line_and_readchar();
 *	Guts for read_char. Grabs a new line off fgetln when we have
 *	consumed the current line and returns the first char, or EOF at end of
 *	stream.  */
static int grab_new_line_and_readchar(void);
/* c = skip_to_end_of_line();
 *	Skips to the end of the current line, returns either '\n' or EOF.  */
static int skip_to_end_of_line(void);


/* Helper functions to handle basic parsing. */
/* read_logical_line(buffer, firstchar);
 *	Grabs logical line into buffer, the first character has already been
 *	read into firstchar.  */
static void read_logical_line(Buffer, int);

/* firstchar = ParseSkipEmptyLines(buffer);
 *	Scans lines, skipping empty lines. May put some characters into
 *	buffer, returns the first character useful to continue parsing
 *	(e.g., not a backslash or a space. */
static int skip_empty_lines_and_read_char(Buffer);

const char *curdir;
size_t curdir_len;

void
Parse_setcurdir(const char *dir)
{
	curdir = dir;
	curdir_len = strlen(dir);
}

static bool
startswith(const char *f, const char *s, size_t len)
{
	return strncmp(f, s, len) == 0 && f[len] == '/';
}

static const char *
simplify(const char *filename)
{
	if (startswith(filename, curdir, curdir_len))
		return filename + curdir_len + 1;
	else if (startswith(filename, _PATH_DEFSYSPATH,
	    sizeof(_PATH_DEFSYSPATH)-1)) {
	    	size_t sz;
		char *buf;
		sz = strlen(filename) - sizeof(_PATH_DEFSYSPATH)+3;
		buf = emalloc(sz);
		snprintf(buf, sz, "<%s>", filename+sizeof(_PATH_DEFSYSPATH));
		return buf;
	} else
		return filename;
}

static struct input_stream *
new_input_file(const char *name, FILE *stream)
{
	struct input_stream *istream;

	istream = emalloc(sizeof(*istream));
	istream->origin.fname = simplify(name);
	Var_Append(READ_MAKEFILES, name);
	istream->str = NULL;
	/* Naturally enough, we start reading at line 0. */
	istream->origin.lineno = 0;
	istream->F = stream;
	istream->ptr = istream->end = NULL;
	return istream;
}

static void
free_input_stream(struct input_stream *istream)
{
	if (istream->F && fileno(istream->F) != STDIN_FILENO)
		(void)fclose(istream->F);
	free(istream->str);
	/* Note we can't free the file names, as they are embedded in GN
	 * for error reports. */
	free(istream);
}

static struct input_stream *
new_input_string(char *str, const Location *origin)
{
	struct input_stream *istream;

	istream = emalloc(sizeof(*istream));
	/* No malloc, name is always taken from an already existing istream
	 * and strings are used in for loops, so we need to reset the line
	 * counter to an appropriate value. */
	istream->origin = *origin;
	istream->F = NULL;
	istream->ptr = istream->str = str;
	istream->end = str + strlen(str);
	return istream;
}


void
Parse_FromString(char *str, unsigned long lineno)
{
	Location origin;
	
	origin.fname = current->origin.fname;
	origin.lineno = lineno;
	if (DEBUG(FOR))
		(void)fprintf(stderr, "%s\n----\n", str);

	Lst_Push(&input_stack, current);
	assert(current != NULL);
	current = new_input_string(str, &origin);
}


void
Parse_FromFile(const char *name, FILE *stream)
{
	if (current != NULL)
		Lst_Push(&input_stack, current);
	current = new_input_file(name, stream);
}

bool
Parse_NextFile(void)
{
	if (current != NULL)
		free_input_stream(current);
	current = (struct input_stream *)Lst_Pop(&input_stack);
	return current != NULL;
}

static int
grab_new_line_and_readchar(void)
{
	size_t len;

	if (current->F) {
		current->ptr = fgetln(current->F, &len);
		if (current->ptr) {
			current->end = current->ptr + len;
			return *current->ptr++;
		} else {
			current->end = NULL;
		}
	}
	return EOF;
}

static int
skip_to_end_of_line(void)
{
	if (current->F) {
		if (current->end - current->ptr > 1)
			current->ptr = current->end - 1;
		if (*current->ptr == '\n')
			return *current->ptr++;
		return EOF;
	} else {
		int c;

		do {
			c = read_char();
		} while (c != '\n' && c != EOF);
		return c;
	}
}


char *
Parse_ReadNextConditionalLine(Buffer linebuf)
{
	int c;

	/* If first char isn't dot, skip to end of line, handling \ */
	while ((c = read_char()) != '.') {
		for (;c != '\n'; c = read_char()) {
			if (c == '\\') {
				c = read_char();
				if (c == '\n')
					current->origin.lineno++;
			}
			if (c == EOF)
				/* Unclosed conditional, reported by cond.c */
				return NULL;
		}
		current->origin.lineno++;
	}

	/* This is the line we need to copy */
	return Parse_ReadUnparsedLine(linebuf, "conditional");
}

static void
read_logical_line(Buffer linebuf, int c)
{
	for (;;) {
		if (c == '\n') {
			current->origin.lineno++;
			break;
		}
		if (c == EOF)
			break;
		Buf_AddChar(linebuf, c);
		c = read_char();
		while (c == '\\') {
			c = read_char();
			if (c == '\n') {
				Buf_AddSpace(linebuf);
				current->origin.lineno++;
				do {
					c = read_char();
				} while (c == ' ' || c == '\t');
			} else {
				Buf_AddChar(linebuf, '\\');
				if (c == '\\') {
					Buf_AddChar(linebuf, '\\');
					c = read_char();
				}
				break;
			}
		}
	}
}

char *
Parse_ReadUnparsedLine(Buffer linebuf, const char *type)
{
	int c;

	Buf_Reset(linebuf);
	c = read_char();
	if (c == EOF) {
		Parse_Error(PARSE_FATAL, "Unclosed %s", type);
		return NULL;
	}

	/* Handle '\' at beginning of line, since \\n needs special treatment */
	while (c == '\\') {
		c = read_char();
		if (c == '\n') {
			current->origin.lineno++;
			do {
				c = read_char();
			} while (c == ' ' || c == '\t');
		} else {
			Buf_AddChar(linebuf, '\\');
			if (c == '\\') {
				Buf_AddChar(linebuf, '\\');
				c = read_char();
			}
			break;
		}
	}
	read_logical_line(linebuf, c);

	return Buf_Retrieve(linebuf);
}

/* This is a fairly complex function, but without it, we could not skip
 * blocks of comments without reading them. */
static int
skip_empty_lines_and_read_char(Buffer linebuf)
{
	int c;		/* the current character */

	for (;;) {
		Buf_Reset(linebuf);
		c = read_char();
		/* Strip leading spaces, fold on '\n' */
		if (c == ' ') {
			do {
				c = read_char();
			} while (c == ' ' || c == '\t');
			while (c == '\\') {
				c = read_char();
				if (c == '\n') {
					current->origin.lineno++;
					do {
						c = read_char();
					} while (c == ' ' || c == '\t');
				} else {
					Buf_AddChar(linebuf, '\\');
					if (c == '\\') {
						Buf_AddChar(linebuf, '\\');
						c = read_char();
					}
					if (c == EOF)
						return '\n';
					else
						return c;
				}
			}
			assert(c != '\t');
		}
		if (c == '#')
			c = skip_to_end_of_line();
		/* Almost identical to spaces, except this occurs after
		 * comments have been taken care of, and we keep the tab
		 * itself.  */
		if (c == '\t') {
			Buf_AddChar(linebuf, '\t');
			do {
				c = read_char();
			} while (c == ' ' || c == '\t');
			while (c == '\\') {
				c = read_char();
				if (c == '\n') {
					current->origin.lineno++;
					do {
						c = read_char();
					} while (c == ' ' || c == '\t');
				} else {
					Buf_AddChar(linebuf, '\\');
					if (c == '\\') {
						Buf_AddChar(linebuf, '\\');
						c = read_char();
					}
					if (c == EOF)
						return '\n';
					else
						return c;
				}
			}
		}
		if (c == '\n')
			current->origin.lineno++;
		else
			return c;
	}
}

/* Parse_ReadNormalLine removes beginning and trailing blanks (but keeps
 * the first tab), handles escaped newlines, and skips over uninteresting
 * lines.
 *
 * The line number is incremented, which implies that continuation
 * lines are numbered with the last line number (we could do better, at a
 * price).
 *
 * Trivial comments are also removed, but we can't do more, as
 * we don't know which lines are shell commands or not.  */
char *
Parse_ReadNormalLine(Buffer linebuf)
{
	int c;		/* the current character */

	c = skip_empty_lines_and_read_char(linebuf);

	if (c == EOF)
		return NULL;
	else {
		read_logical_line(linebuf, c);
		return Buf_Retrieve(linebuf);
	}
}

unsigned long
Parse_Getlineno(void)
{
	return current ? current->origin.lineno : 0;
}

const char *
Parse_Getfilename(void)
{
	return current ? current->origin.fname : NULL;
}

void
Parse_SetLocation(Location *origin)
{
	post_parse = origin;
}

void
Parse_FillLocation(Location *origin)
{
	if (post_parse) {
		*origin = *post_parse;
	} else {
		origin->lineno = Parse_Getlineno();
		origin->fname = Parse_Getfilename();
	}
}

void
Parse_ReportErrors(void)
{
	if (fatal_errors)
		exit(1);
	else
		assert(current == NULL);
}
