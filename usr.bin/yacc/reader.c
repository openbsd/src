/* $OpenBSD: reader.c,v 1.30 2014/12/02 15:56:22 millert Exp $	 */
/* $NetBSD: reader.c,v 1.5 1996/03/19 03:21:43 jtc Exp $	 */

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

#include "defs.h"

/* The line size must be a positive integer.  One hundred was chosen	 */
/* because few lines in Yacc input grammars exceed 100 characters.	 */
/* Note that if a line exceeds LINESIZE characters, the line buffer	 */
/* will be expanded to accommodate it.					 */

#define LINESIZE 100

char *cache;
int cinc, cache_size;

int ntags, tagmax;
char **tag_table;

char saw_eof, unionized;
char *cptr, *line;
int linesize;

bucket *goal;
int prec;
int gensym;
char last_was_action;

int maxitems;
bucket **pitem;

int maxrules;
bucket **plhs;

int name_pool_size;
char *name_pool;

void cachec(int);
void get_line(void);
char *dup_line(void);
void skip_comment(void);
int nextc(void);
int keyword(void);
void copy_ident(void);
void copy_text(void);
void copy_union(void);
bucket *get_literal(void);
int is_reserved(char *);
bucket *get_name(void);
int get_number(void);
char *get_tag(void);
void declare_tokens(int);
void declare_types(void);
void declare_start(void);
void handle_expect(void);
void read_declarations(void);
void initialize_grammar(void);
void expand_items(void);
void expand_rules(void);
void advance_to_start(void);
void start_rule(bucket *, int);
void end_rule(void);
void insert_empty_rule(void);
void add_symbol(void);
void copy_action(void);
int mark_symbol(void);
void read_grammar(void);
void free_tags(void);
void pack_names(void);
void check_symbols(void);
void pack_symbols(void);
void pack_grammar(void);
void print_grammar(void);

char line_format[] = "#line %d \"%s\"\n";

void
cachec(int c)
{
	assert(cinc >= 0);
	if (cinc >= cache_size) {
		cache_size += 256;
		cache = realloc(cache, cache_size);
		if (cache == NULL)
			no_space();
	}
	cache[cinc] = c;
	++cinc;
}


void
get_line(void)
{
	FILE *f = input_file;
	int c, i;

	if (saw_eof || (c = getc(f)) == EOF) {
		if (line) {
			free(line);
			line = 0;
		}
		cptr = 0;
		saw_eof = 1;
		return;
	}
	if (line == NULL || linesize != (LINESIZE + 1)) {
		if (line)
			free(line);
		linesize = LINESIZE + 1;
		line = malloc(linesize);
		if (line == NULL)
			no_space();
	}
	i = 0;
	++lineno;
	for (;;) {
		line[i] = c;
		if (c == '\n') {
			cptr = line;
			return;
		}
		if (++i >= linesize) {
			linesize += LINESIZE;
			line = realloc(line, linesize);
			if (line == NULL)
				no_space();
		}
		c = getc(f);
		if (c == EOF) {
			line[i] = '\n';
			saw_eof = 1;
			cptr = line;
			return;
		}
	}
}


char *
dup_line(void)
{
	char *p, *s, *t;

	if (line == NULL)
		return (0);
	s = line;
	while (*s != '\n')
		++s;
	p = malloc(s - line + 1);
	if (p == NULL)
		no_space();

	s = line;
	t = p;
	while ((*t++ = *s++) != '\n')
		continue;
	return (p);
}


void
skip_comment(void)
{
	char *s;
	int st_lineno = lineno;
	char *st_line = dup_line();
	char *st_cptr = st_line + (cptr - line);

	s = cptr + 2;
	for (;;) {
		if (*s == '*' && s[1] == '/') {
			cptr = s + 2;
			free(st_line);
			return;
		}
		if (*s == '\n') {
			get_line();
			if (line == NULL)
				unterminated_comment(st_lineno, st_line, st_cptr);
			s = cptr;
		} else
			++s;
	}
}


int
nextc(void)
{
	char *s;

	if (line == NULL) {
		get_line();
		if (line == NULL)
			return (EOF);
	}
	s = cptr;
	for (;;) {
		switch (*s) {
		case '\n':
			get_line();
			if (line == NULL)
				return (EOF);
			s = cptr;
			break;

		case ' ':
		case '\t':
		case '\f':
		case '\r':
		case '\v':
		case ',':
		case ';':
			++s;
			break;

		case '\\':
			cptr = s;
			return ('%');

		case '/':
			if (s[1] == '*') {
				cptr = s;
				skip_comment();
				s = cptr;
				break;
			} else if (s[1] == '/') {
				get_line();
				if (line == NULL)
					return (EOF);
				s = cptr;
				break;
			}
			/* fall through */

		default:
			cptr = s;
			return ((unsigned char) *s);
		}
	}
}


int
keyword(void)
{
	int c;
	char *t_cptr = cptr;

	c = (unsigned char) *++cptr;
	if (isalpha(c)) {
		cinc = 0;
		for (;;) {
			if (isalpha(c)) {
				if (isupper(c))
					c = tolower(c);
				cachec(c);
			} else if (isdigit(c) || c == '_' || c == '.' || c == '$')
				cachec(c);
			else
				break;
			c = (unsigned char) *++cptr;
		}
		cachec(NUL);

		if (strcmp(cache, "token") == 0 || strcmp(cache, "term") == 0)
			return (TOKEN);
		if (strcmp(cache, "type") == 0)
			return (TYPE);
		if (strcmp(cache, "left") == 0)
			return (LEFT);
		if (strcmp(cache, "right") == 0)
			return (RIGHT);
		if (strcmp(cache, "nonassoc") == 0 || strcmp(cache, "binary") == 0)
			return (NONASSOC);
		if (strcmp(cache, "start") == 0)
			return (START);
		if (strcmp(cache, "union") == 0)
			return (UNION);
		if (strcmp(cache, "ident") == 0)
			return (IDENT);
		if (strcmp(cache, "expect") == 0)
			return (EXPECT);
	} else {
		++cptr;
		if (c == '{')
			return (TEXT);
		if (c == '%' || c == '\\')
			return (MARK);
		if (c == '<')
			return (LEFT);
		if (c == '>')
			return (RIGHT);
		if (c == '0')
			return (TOKEN);
		if (c == '2')
			return (NONASSOC);
	}
	syntax_error(lineno, line, t_cptr);
	/* NOTREACHED */
	return (0);
}


void
copy_ident(void)
{
	int c;
	FILE *f = output_file;

	c = nextc();
	if (c == EOF)
		unexpected_EOF();
	if (c != '"')
		syntax_error(lineno, line, cptr);
	++outline;
	fprintf(f, "#ident \"");
	for (;;) {
		c = (unsigned char) *++cptr;
		if (c == '\n') {
			fprintf(f, "\"\n");
			return;
		}
		putc(c, f);
		if (c == '"') {
			putc('\n', f);
			++cptr;
			return;
		}
	}
}


void
copy_text(void)
{
	int c;
	int quote;
	FILE *f = text_file;
	int need_newline = 0;
	int t_lineno = lineno;
	char *t_line = dup_line();
	char *t_cptr = t_line + (cptr - line - 2);

	if (*cptr == '\n') {
		get_line();
		if (line == NULL)
			unterminated_text(t_lineno, t_line, t_cptr);
	}
	if (!lflag)
		fprintf(f, line_format, lineno, input_file_name);

loop:
	c = (unsigned char) *cptr++;
	switch (c) {
	case '\n':
next_line:
		putc('\n', f);
		need_newline = 0;
		get_line();
		if (line)
			goto loop;
		unterminated_text(t_lineno, t_line, t_cptr);

	case '\'':
	case '"': {
		int s_lineno = lineno;
		char *s_line = dup_line();
		char *s_cptr = s_line + (cptr - line - 1);

		quote = c;
		putc(c, f);
		for (;;) {
			c = (unsigned char) *cptr++;
			putc(c, f);
			if (c == quote) {
				need_newline = 1;
				free(s_line);
				goto loop;
			}
			if (c == '\n')
				unterminated_string(s_lineno, s_line, s_cptr);
			if (c == '\\') {
				c = (unsigned char) *cptr++;
				putc(c, f);
				if (c == '\n') {
					get_line();
					if (line == NULL)
						unterminated_string(s_lineno, s_line, s_cptr);
				}
			}
		}
	}

	case '/':
		putc(c, f);
		need_newline = 1;
		c = (unsigned char) *cptr;
		if (c == '/') {
			putc('*', f);
			while ((c = (unsigned char) *++cptr) != '\n') {
				if (c == '*' && cptr[1] == '/')
					fprintf(f, "* ");
				else
					putc(c, f);
			}
			fprintf(f, "*/");
			goto next_line;
		}
		if (c == '*') {
			int c_lineno = lineno;
			char *c_line = dup_line();
			char *c_cptr = c_line + (cptr - line - 1);

			putc('*', f);
			++cptr;
			for (;;) {
				c = (unsigned char) *cptr++;
				putc(c, f);
				if (c == '*' && *cptr == '/') {
					putc('/', f);
					++cptr;
					free(c_line);
					goto loop;
				}
				if (c == '\n') {
					get_line();
					if (line == NULL)
						unterminated_comment(c_lineno, c_line, c_cptr);
				}
			}
		}
		need_newline = 1;
		goto loop;

	case '%':
	case '\\':
		if (*cptr == '}') {
			if (need_newline)
				putc('\n', f);
			++cptr;
			free(t_line);
			return;
		}
		/* fall through */

	default:
		putc(c, f);
		need_newline = 1;
		goto loop;
	}
}


void
copy_union(void)
{
	int c, quote, depth;
	int u_lineno = lineno;
	char *u_line = dup_line();
	char *u_cptr = u_line + (cptr - line - 6);

	if (unionized)
		over_unionized(cptr - 6);
	unionized = 1;

	if (!lflag)
		fprintf(text_file, line_format, lineno, input_file_name);

	fprintf(text_file, "#ifndef YYSTYPE_DEFINED\n");
	fprintf(text_file, "#define YYSTYPE_DEFINED\n");
	fprintf(text_file, "typedef union");
	if (dflag)
		fprintf(union_file, "#ifndef YYSTYPE_DEFINED\n");
	if (dflag)
		fprintf(union_file, "#define YYSTYPE_DEFINED\n");
	if (dflag)
		fprintf(union_file, "typedef union");

	depth = 0;
loop:
	c = (unsigned char) *cptr++;
	putc(c, text_file);
	if (dflag)
		putc(c, union_file);
	switch (c) {
	case '\n':
next_line:
		get_line();
		if (line == NULL)
			unterminated_union(u_lineno, u_line, u_cptr);
		goto loop;

	case '{':
		++depth;
		goto loop;

	case '}':
		if (--depth == 0) {
			fprintf(text_file, " YYSTYPE;\n");
			fprintf(text_file, "#endif /* YYSTYPE_DEFINED */\n");
			free(u_line);
			return;
		}
		goto loop;

	case '\'':
	case '"': {
		int s_lineno = lineno;
		char *s_line = dup_line();
		char *s_cptr = s_line + (cptr - line - 1);

		quote = c;
		for (;;) {
			c = (unsigned char) *cptr++;
			putc(c, text_file);
			if (dflag)
				putc(c, union_file);
			if (c == quote) {
				free(s_line);
				goto loop;
			}
			if (c == '\n')
				unterminated_string(s_lineno, s_line, s_cptr);
			if (c == '\\') {
				c = (unsigned char) *cptr++;
				putc(c, text_file);
				if (dflag)
					putc(c, union_file);
				if (c == '\n') {
					get_line();
					if (line == NULL)
						unterminated_string(s_lineno,
						    s_line, s_cptr);
				}
			}
		}
	}

	case '/':
		c = (unsigned char) *cptr;
		if (c == '/') {
			putc('*', text_file);
			if (dflag)
				putc('*', union_file);
			while ((c = (unsigned char) *++cptr) != '\n') {
				if (c == '*' && cptr[1] == '/') {
					fprintf(text_file, "* ");
					if (dflag)
						fprintf(union_file, "* ");
				} else {
					putc(c, text_file);
					if (dflag)
						putc(c, union_file);
				}
			}
			fprintf(text_file, "*/\n");
			if (dflag)
				fprintf(union_file, "*/\n");
			goto next_line;
		}
		if (c == '*') {
			int c_lineno = lineno;
			char *c_line = dup_line();
			char *c_cptr = c_line + (cptr - line - 1);

			putc('*', text_file);
			if (dflag)
				putc('*', union_file);
			++cptr;
			for (;;) {
				c = (unsigned char) *cptr++;
				putc(c, text_file);
				if (dflag)
					putc(c, union_file);
				if (c == '*' && *cptr == '/') {
					putc('/', text_file);
					if (dflag)
						putc('/', union_file);
					++cptr;
					free(c_line);
					goto loop;
				}
				if (c == '\n') {
					get_line();
					if (line == NULL)
						unterminated_comment(c_lineno,
						    c_line, c_cptr);
				}
			}
		}
		goto loop;

	default:
		goto loop;
	}
}


bucket *
get_literal(void)
{
	int c, quote, i, n;
	char *s;
	bucket *bp;
	int s_lineno = lineno;
	char *s_line = dup_line();
	char *s_cptr = s_line + (cptr - line);

	quote = (unsigned char) *cptr++;
	cinc = 0;
	for (;;) {
		c = (unsigned char) *cptr++;
		if (c == quote)
			break;
		if (c == '\n')
			unterminated_string(s_lineno, s_line, s_cptr);
		if (c == '\\') {
			char *c_cptr = cptr - 1;
			unsigned long ulval;

			c = (unsigned char) *cptr++;
			switch (c) {
			case '\n':
				get_line();
				if (line == NULL)
					unterminated_string(s_lineno, s_line,
					    s_cptr);
				continue;

			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
				ulval = strtoul(cptr - 1, &s, 8);
				if (s == cptr - 1 || ulval > MAXCHAR)
					illegal_character(c_cptr);
				c = (int) ulval;
				cptr = s;
				break;

			case 'x':
				ulval = strtoul(cptr, &s, 16);
				if (s == cptr || ulval > MAXCHAR)
					illegal_character(c_cptr);
				c = (int) ulval;
				cptr = s;
				break;

			case 'a':
				c = 7;
				break;
			case 'b':
				c = '\b';
				break;
			case 'f':
				c = '\f';
				break;
			case 'n':
				c = '\n';
				break;
			case 'r':
				c = '\r';
				break;
			case 't':
				c = '\t';
				break;
			case 'v':
				c = '\v';
				break;
			}
		}
		cachec(c);
	}
	free(s_line);

	n = cinc;
	s = malloc(n);
	if (s == NULL)
		no_space();

	memcpy(s, cache, n);

	cinc = 0;
	if (n == 1)
		cachec('\'');
	else
		cachec('"');

	for (i = 0; i < n; ++i) {
		c = ((unsigned char *) s)[i];
		if (c == '\\' || c == cache[0]) {
			cachec('\\');
			cachec(c);
		} else if (isprint(c))
			cachec(c);
		else {
			cachec('\\');
			switch (c) {
			case 7:
				cachec('a');
				break;
			case '\b':
				cachec('b');
				break;
			case '\f':
				cachec('f');
				break;
			case '\n':
				cachec('n');
				break;
			case '\r':
				cachec('r');
				break;
			case '\t':
				cachec('t');
				break;
			case '\v':
				cachec('v');
				break;
			default:
				cachec(((c >> 6) & 7) + '0');
				cachec(((c >> 3) & 7) + '0');
				cachec((c & 7) + '0');
				break;
			}
		}
	}

	if (n == 1)
		cachec('\'');
	else
		cachec('"');

	cachec(NUL);
	bp = lookup(cache);
	bp->class = TERM;
	if (n == 1 && bp->value == UNDEFINED)
		bp->value = *(unsigned char *) s;
	free(s);

	return (bp);
}


int
is_reserved(char *name)
{
	char *s;

	if (strcmp(name, ".") == 0 ||
	    strcmp(name, "$accept") == 0 ||
	    strcmp(name, "$end") == 0)
		return (1);

	if (name[0] == '$' && name[1] == '$' && isdigit((unsigned char) name[2])) {
		s = name + 3;
		while (isdigit((unsigned char) *s))
			++s;
		if (*s == NUL)
			return (1);
	}
	return (0);
}


bucket *
get_name(void)
{
	int c;

	cinc = 0;
	for (c = (unsigned char) *cptr; IS_IDENT(c); c = (unsigned char) *++cptr)
		cachec(c);
	cachec(NUL);

	if (is_reserved(cache))
		used_reserved(cache);

	return (lookup(cache));
}


int
get_number(void)
{
	int c, n;

	n = 0;
	for (c = (unsigned char) *cptr; isdigit(c); c = (unsigned char) *++cptr)
		n = 10 * n + (c - '0');

	return (n);
}


char *
get_tag(void)
{
	int c, i;
	char *s;
	int t_lineno = lineno;
	char *t_line = dup_line();
	char *t_cptr = t_line + (cptr - line);

	++cptr;
	c = nextc();
	if (c == EOF)
		unexpected_EOF();
	if (!isalpha(c) && c != '_' && c != '$')
		illegal_tag(t_lineno, t_line, t_cptr);

	cinc = 0;
	do {
		cachec(c);
		c = (unsigned char) *++cptr;
	} while (IS_IDENT(c));
	cachec(NUL);

	c = nextc();
	if (c == EOF)
		unexpected_EOF();
	if (c != '>')
		illegal_tag(t_lineno, t_line, t_cptr);
	free(t_line);
	++cptr;

	for (i = 0; i < ntags; ++i) {
		if (strcmp(cache, tag_table[i]) == 0)
			return (tag_table[i]);
	}

	if (ntags >= tagmax) {
		tagmax += 16;
		tag_table = reallocarray(tag_table, tagmax, sizeof(char *));
		if (tag_table == NULL)
			no_space();
	}
	s = malloc(cinc);
	if (s == NULL)
		no_space();
	strlcpy(s, cache, cinc);
	tag_table[ntags] = s;
	++ntags;
	return (s);
}


void
declare_tokens(int assoc)
{
	int c;
	bucket *bp;
	int value;
	char *tag = 0;

	if (assoc != TOKEN)
		++prec;

	c = nextc();
	if (c == EOF)
		unexpected_EOF();
	if (c == '<') {
		tag = get_tag();
		c = nextc();
		if (c == EOF)
			unexpected_EOF();
	}
	for (;;) {
		if (isalpha(c) || c == '_' || c == '.' || c == '$')
			bp = get_name();
		else if (c == '\'' || c == '"')
			bp = get_literal();
		else
			return;

		if (bp == goal)
			tokenized_start(bp->name);
		bp->class = TERM;

		if (tag) {
			if (bp->tag && tag != bp->tag)
				retyped_warning(bp->name);
			bp->tag = tag;
		}
		if (assoc != TOKEN) {
			if (bp->prec && prec != bp->prec)
				reprec_warning(bp->name);
			bp->assoc = assoc;
			bp->prec = prec;
		}
		c = nextc();
		if (c == EOF)
			unexpected_EOF();
		if (isdigit(c)) {
			value = get_number();
			if (bp->value != UNDEFINED && value != bp->value)
				revalued_warning(bp->name);
			bp->value = value;
			c = nextc();
			if (c == EOF)
				unexpected_EOF();
		}
	}
}


/*
 * %expect requires special handling as it really isn't part of the yacc
 * grammar only a flag for yacc proper.
 */
void
declare_expect(int assoc)
{
	int c;

	if (assoc != EXPECT)
		++prec;

	/*
         * Stay away from nextc - doesn't detect EOL and will read to EOF.
         */
	c = (unsigned char) *++cptr;
	if (c == EOF)
		unexpected_EOF();

	for (;;) {
		if (isdigit(c)) {
			SRexpect = get_number();
			break;
		}
		/*
	         * Looking for number before EOL.
	         * Spaces, tabs, and numbers are ok.
	         * Words, punc., etc. are syntax errors.
	         */
		else if (c == '\n' || isalpha(c) || !isspace(c)) {
			syntax_error(lineno, line, cptr);
		} else {
			c = (unsigned char) *++cptr;
			if (c == EOF)
				unexpected_EOF();
		}
	}
}


void
declare_types(void)
{
	int c;
	bucket *bp;
	char *tag;

	c = nextc();
	if (c == EOF)
		unexpected_EOF();
	if (c != '<')
		syntax_error(lineno, line, cptr);
	tag = get_tag();

	for (;;) {
		c = nextc();
		if (isalpha(c) || c == '_' || c == '.' || c == '$')
			bp = get_name();
		else if (c == '\'' || c == '"')
			bp = get_literal();
		else
			return;

		if (bp->tag && tag != bp->tag)
			retyped_warning(bp->name);
		bp->tag = tag;
	}
}


void
declare_start(void)
{
	int c;
	bucket *bp;

	c = nextc();
	if (c == EOF)
		unexpected_EOF();
	if (!isalpha(c) && c != '_' && c != '.' && c != '$')
		syntax_error(lineno, line, cptr);
	bp = get_name();
	if (bp->class == TERM)
		terminal_start(bp->name);
	if (goal && goal != bp)
		restarted_warning();
	goal = bp;
}


void
read_declarations(void)
{
	int c, k;

	cache_size = 256;
	cache = malloc(cache_size);
	if (cache == NULL)
		no_space();

	for (;;) {
		c = nextc();
		if (c == EOF)
			unexpected_EOF();
		if (c != '%')
			syntax_error(lineno, line, cptr);
		switch (k = keyword()) {
		case MARK:
			return;

		case IDENT:
			copy_ident();
			break;

		case TEXT:
			copy_text();
			break;

		case UNION:
			copy_union();
			break;

		case TOKEN:
		case LEFT:
		case RIGHT:
		case NONASSOC:
			declare_tokens(k);
			break;

		case EXPECT:
			declare_expect(k);
			break;

		case TYPE:
			declare_types();
			break;

		case START:
			declare_start();
			break;
		}
	}
}


void
initialize_grammar(void)
{
	nitems = 4;
	maxitems = 300;
	pitem = calloc(maxitems, sizeof(bucket *));
	if (pitem == NULL)
		no_space();

	nrules = 3;
	maxrules = 100;
	plhs = reallocarray(NULL, maxrules, sizeof(bucket *));
	if (plhs == NULL)
		no_space();
	plhs[0] = 0;
	plhs[1] = 0;
	plhs[2] = 0;
	rprec = reallocarray(NULL, maxrules, sizeof(short));
	if (rprec == NULL)
		no_space();
	rprec[0] = 0;
	rprec[1] = 0;
	rprec[2] = 0;
	rassoc = reallocarray(NULL, maxrules, sizeof(char));
	if (rassoc == NULL)
		no_space();
	rassoc[0] = TOKEN;
	rassoc[1] = TOKEN;
	rassoc[2] = TOKEN;
}


void
expand_items(void)
{
	int olditems = maxitems;

	maxitems += 300;
	pitem = reallocarray(pitem, maxitems, sizeof(bucket *));
	if (pitem == NULL)
		no_space();
	memset(pitem + olditems, 0, (maxitems - olditems) * sizeof(bucket *));
}


void
expand_rules(void)
{
	maxrules += 100;
	plhs = reallocarray(plhs, maxrules, sizeof(bucket *));
	if (plhs == NULL)
		no_space();
	rprec = reallocarray(rprec, maxrules, sizeof(short));
	if (rprec == NULL)
		no_space();
	rassoc = reallocarray(rassoc, maxrules, sizeof(char));
	if (rassoc == NULL)
		no_space();
}


void
advance_to_start(void)
{
	int c;
	bucket *bp;
	char *s_cptr;
	int s_lineno;

	for (;;) {
		c = nextc();
		if (c != '%')
			break;
		s_cptr = cptr;
		switch (keyword()) {
		case MARK:
			no_grammar();

		case TEXT:
			copy_text();
			break;

		case START:
			declare_start();
			break;

		default:
			syntax_error(lineno, line, s_cptr);
		}
	}

	c = nextc();
	if (!isalpha(c) && c != '_' && c != '.' && c != '_')
		syntax_error(lineno, line, cptr);
	bp = get_name();
	if (goal == NULL) {
		if (bp->class == TERM)
			terminal_start(bp->name);
		goal = bp;
	}
	s_lineno = lineno;
	c = nextc();
	if (c == EOF)
		unexpected_EOF();
	if (c != ':')
		syntax_error(lineno, line, cptr);
	start_rule(bp, s_lineno);
	++cptr;
}


void
start_rule(bucket * bp, int s_lineno)
{
	if (bp->class == TERM)
		terminal_lhs(s_lineno);
	bp->class = NONTERM;
	if (nrules >= maxrules)
		expand_rules();
	plhs[nrules] = bp;
	rprec[nrules] = UNDEFINED;
	rassoc[nrules] = TOKEN;
}


void
end_rule(void)
{
	int i;

	if (!last_was_action && plhs[nrules]->tag) {
		for (i = nitems - 1; pitem[i]; --i)
			continue;
		if (i == maxitems - 1 || pitem[i + 1] == 0 ||
		    pitem[i + 1]->tag != plhs[nrules]->tag)
			default_action_warning();
	}
	last_was_action = 0;
	if (nitems >= maxitems)
		expand_items();
	pitem[nitems] = 0;
	++nitems;
	++nrules;
}


void
insert_empty_rule(void)
{
	bucket *bp, **bpp;

	assert(cache);
	snprintf(cache, cache_size, "$$%d", ++gensym);
	bp = make_bucket(cache);
	last_symbol->next = bp;
	last_symbol = bp;
	bp->tag = plhs[nrules]->tag;
	bp->class = NONTERM;

	if ((nitems += 2) > maxitems)
		expand_items();
	bpp = pitem + nitems - 1;
	*bpp-- = bp;
	while ((bpp[0] = bpp[-1]))
		--bpp;

	if (++nrules >= maxrules)
		expand_rules();
	plhs[nrules] = plhs[nrules - 1];
	plhs[nrules - 1] = bp;
	rprec[nrules] = rprec[nrules - 1];
	rprec[nrules - 1] = 0;
	rassoc[nrules] = rassoc[nrules - 1];
	rassoc[nrules - 1] = TOKEN;
}


void
add_symbol(void)
{
	int c;
	bucket *bp;
	int s_lineno = lineno;

	c = (unsigned char) *cptr;
	if (c == '\'' || c == '"')
		bp = get_literal();
	else
		bp = get_name();

	c = nextc();
	if (c == ':') {
		end_rule();
		start_rule(bp, s_lineno);
		++cptr;
		return;
	}
	if (last_was_action)
		insert_empty_rule();
	last_was_action = 0;

	if (++nitems > maxitems)
		expand_items();
	pitem[nitems - 1] = bp;
}


void
copy_action(void)
{
	int c, i, n, depth, quote;
	char *tag;
	FILE *f = action_file;
	int a_lineno = lineno;
	char *a_line = dup_line();
	char *a_cptr = a_line + (cptr - line);

	if (last_was_action)
		insert_empty_rule();
	last_was_action = 1;

	fprintf(f, "case %d:\n", nrules - 2);
	if (!lflag)
		fprintf(f, line_format, lineno, input_file_name);
	if (*cptr == '=')
		++cptr;

	n = 0;
	for (i = nitems - 1; pitem[i]; --i)
		++n;

	depth = 0;
loop:
	c = (unsigned char) *cptr;
	if (c == '$') {
		if (cptr[1] == '<') {
			int d_lineno = lineno;
			char *d_line = dup_line();
			char *d_cptr = d_line + (cptr - line);

			++cptr;
			tag = get_tag();
			c = (unsigned char) *cptr;
			if (c == '$') {
				fprintf(f, "yyval.%s", tag);
				++cptr;
				free(d_line);
				goto loop;
			} else if (isdigit(c)) {
				i = get_number();
				if (i > n)
					dollar_warning(d_lineno, i);
				fprintf(f, "yyvsp[%d].%s", i - n, tag);
				free(d_line);
				goto loop;
			} else if (c == '-' && isdigit((unsigned char) cptr[1])) {
				++cptr;
				i = -get_number() - n;
				fprintf(f, "yyvsp[%d].%s", i, tag);
				free(d_line);
				goto loop;
			} else
				dollar_error(d_lineno, d_line, d_cptr);
		} else if (cptr[1] == '$') {
			if (ntags) {
				tag = plhs[nrules]->tag;
				if (tag == NULL)
					untyped_lhs();
				fprintf(f, "yyval.%s", tag);
			} else
				fprintf(f, "yyval");
			cptr += 2;
			goto loop;
		} else if (isdigit((unsigned char) cptr[1])) {
			++cptr;
			i = get_number();
			if (ntags) {
				if (i <= 0 || i > n)
					unknown_rhs(i);
				tag = pitem[nitems + i - n - 1]->tag;
				if (tag == NULL)
					untyped_rhs(i, pitem[nitems + i - n - 1]->name);
				fprintf(f, "yyvsp[%d].%s", i - n, tag);
			} else {
				if (i > n)
					dollar_warning(lineno, i);
				fprintf(f, "yyvsp[%d]", i - n);
			}
			goto loop;
		} else if (cptr[1] == '-') {
			cptr += 2;
			i = get_number();
			if (ntags)
				unknown_rhs(-i);
			fprintf(f, "yyvsp[%d]", -i - n);
			goto loop;
		}
	}
	if (isalpha(c) || c == '_' || c == '$') {
		do {
			putc(c, f);
			c = (unsigned char) *++cptr;
		} while (isalnum(c) || c == '_' || c == '$');
		goto loop;
	}
	putc(c, f);
	++cptr;
	switch (c) {
	case '\n':
next_line:
		get_line();
		if (line)
			goto loop;
		unterminated_action(a_lineno, a_line, a_cptr);

	case ';':
		if (depth > 0)
			goto loop;
		fprintf(f, "\nbreak;\n");
		free(a_line);
		return;

	case '{':
		++depth;
		goto loop;

	case '}':
		if (--depth > 0)
			goto loop;
		fprintf(f, "\nbreak;\n");
		free(a_line);
		return;

	case '\'':
	case '"': {
		int s_lineno = lineno;
		char *s_line = dup_line();
		char *s_cptr = s_line + (cptr - line - 1);

		quote = c;
		for (;;) {
			c = (unsigned char) *cptr++;
			putc(c, f);
			if (c == quote) {
				free(s_line);
				goto loop;
			}
			if (c == '\n')
				unterminated_string(s_lineno, s_line, s_cptr);
			if (c == '\\') {
				c = (unsigned char) *cptr++;
				putc(c, f);
				if (c == '\n') {
					get_line();
					if (line == NULL)
						unterminated_string(s_lineno, s_line, s_cptr);
				}
			}
		}
	}

	case '/':
		c = (unsigned char) *cptr;
		if (c == '/') {
			putc('*', f);
			while ((c = (unsigned char) *++cptr) != '\n') {
				if (c == '*' && cptr[1] == '/')
					fprintf(f, "* ");
				else
					putc(c, f);
			}
			fprintf(f, "*/\n");
			goto next_line;
		}
		if (c == '*') {
			int c_lineno = lineno;
			char *c_line = dup_line();
			char *c_cptr = c_line + (cptr - line - 1);

			putc('*', f);
			++cptr;
			for (;;) {
				c = (unsigned char) *cptr++;
				putc(c, f);
				if (c == '*' && *cptr == '/') {
					putc('/', f);
					++cptr;
					free(c_line);
					goto loop;
				}
				if (c == '\n') {
					get_line();
					if (line == NULL)
						unterminated_comment(c_lineno, c_line, c_cptr);
				}
			}
		}
		goto loop;

	default:
		goto loop;
	}
}


int
mark_symbol(void)
{
	int c;
	bucket *bp = NULL;

	c = (unsigned char) cptr[1];
	if (c == '%' || c == '\\') {
		cptr += 2;
		return (1);
	}
	if (c == '=')
		cptr += 2;
	else if ((c == 'p' || c == 'P') &&
	    ((c = cptr[2]) == 'r' || c == 'R') &&
	    ((c = cptr[3]) == 'e' || c == 'E') &&
	    ((c = cptr[4]) == 'c' || c == 'C') &&
	    ((c = (unsigned char) cptr[5], !IS_IDENT(c))))
		cptr += 5;
	else
		syntax_error(lineno, line, cptr);

	c = nextc();
	if (isalpha(c) || c == '_' || c == '.' || c == '$')
		bp = get_name();
	else if (c == '\'' || c == '"')
		bp = get_literal();
	else {
		syntax_error(lineno, line, cptr);
		/* NOTREACHED */
	}

	if (rprec[nrules] != UNDEFINED && bp->prec != rprec[nrules])
		prec_redeclared();

	rprec[nrules] = bp->prec;
	rassoc[nrules] = bp->assoc;
	return (0);
}


void
read_grammar(void)
{
	int c;

	initialize_grammar();
	advance_to_start();

	for (;;) {
		c = nextc();
		if (c == EOF)
			break;
		if (isalpha(c) || c == '_' || c == '.' || c == '$' || c == '\'' ||
		    c == '"')
			add_symbol();
		else if (c == '{' || c == '=')
			copy_action();
		else if (c == '|') {
			end_rule();
			start_rule(plhs[nrules - 1], 0);
			++cptr;
		} else if (c == '%') {
			if (mark_symbol())
				break;
		} else
			syntax_error(lineno, line, cptr);
	}
	end_rule();
}


void
free_tags(void)
{
	int i;

	if (tag_table == NULL)
		return;

	for (i = 0; i < ntags; ++i) {
		assert(tag_table[i]);
		free(tag_table[i]);
	}
	free(tag_table);
}


void
pack_names(void)
{
	bucket *bp;
	char *p, *s, *t;

	name_pool_size = 13;	/* 13 == sizeof("$end") + sizeof("$accept") */
	for (bp = first_symbol; bp; bp = bp->next)
		name_pool_size += strlen(bp->name) + 1;
	name_pool = malloc(name_pool_size);
	if (name_pool == NULL)
		no_space();

	strlcpy(name_pool, "$accept", name_pool_size);
	strlcpy(name_pool + 8, "$end", name_pool_size - 8);
	t = name_pool + 13;
	for (bp = first_symbol; bp; bp = bp->next) {
		p = t;
		s = bp->name;
		while ((*t++ = *s++))
			continue;
		free(bp->name);
		bp->name = p;
	}
}


void
check_symbols(void)
{
	bucket *bp;

	if (goal->class == UNKNOWN)
		undefined_goal(goal->name);

	for (bp = first_symbol; bp; bp = bp->next) {
		if (bp->class == UNKNOWN) {
			undefined_symbol_warning(bp->name);
			bp->class = TERM;
		}
	}
}


void
pack_symbols(void)
{
	bucket *bp;
	bucket **v;
	int i, j, k, n;

	nsyms = 2;
	ntokens = 1;
	for (bp = first_symbol; bp; bp = bp->next) {
		++nsyms;
		if (bp->class == TERM)
			++ntokens;
	}
	start_symbol = ntokens;
	nvars = nsyms - ntokens;

	symbol_name = reallocarray(NULL, nsyms, sizeof(char *));
	if (symbol_name == NULL)
		no_space();
	symbol_value = reallocarray(NULL, nsyms, sizeof(short));
	if (symbol_value == NULL)
		no_space();
	symbol_prec = reallocarray(NULL, nsyms, sizeof(short));
	if (symbol_prec == NULL)
		no_space();
	symbol_assoc = malloc(nsyms);
	if (symbol_assoc == NULL)
		no_space();

	v = reallocarray(NULL, nsyms, sizeof(bucket *));
	if (v == NULL)
		no_space();

	v[0] = 0;
	v[start_symbol] = 0;

	i = 1;
	j = start_symbol + 1;
	for (bp = first_symbol; bp; bp = bp->next) {
		if (bp->class == TERM)
			v[i++] = bp;
		else
			v[j++] = bp;
	}
	assert(i == ntokens && j == nsyms);

	for (i = 1; i < ntokens; ++i)
		v[i]->index = i;

	goal->index = start_symbol + 1;
	k = start_symbol + 2;
	while (++i < nsyms)
		if (v[i] != goal) {
			v[i]->index = k;
			++k;
		}
	goal->value = 0;
	k = 1;
	for (i = start_symbol + 1; i < nsyms; ++i) {
		if (v[i] != goal) {
			v[i]->value = k;
			++k;
		}
	}

	k = 0;
	for (i = 1; i < ntokens; ++i) {
		n = v[i]->value;
		if (n > 256) {
			for (j = k++; j > 0 && symbol_value[j - 1] > n; --j)
				symbol_value[j] = symbol_value[j - 1];
			symbol_value[j] = n;
		}
	}

	if (v[1]->value == UNDEFINED)
		v[1]->value = 256;

	j = 0;
	n = 257;
	for (i = 2; i < ntokens; ++i) {
		if (v[i]->value == UNDEFINED) {
			while (j < k && n == symbol_value[j]) {
				while (++j < k && n == symbol_value[j])
					continue;
				++n;
			}
			v[i]->value = n;
			++n;
		}
	}

	symbol_name[0] = name_pool + 8;
	symbol_value[0] = 0;
	symbol_prec[0] = 0;
	symbol_assoc[0] = TOKEN;
	for (i = 1; i < ntokens; ++i) {
		symbol_name[i] = v[i]->name;
		symbol_value[i] = v[i]->value;
		symbol_prec[i] = v[i]->prec;
		symbol_assoc[i] = v[i]->assoc;
	}
	symbol_name[start_symbol] = name_pool;
	symbol_value[start_symbol] = -1;
	symbol_prec[start_symbol] = 0;
	symbol_assoc[start_symbol] = TOKEN;
	for (++i; i < nsyms; ++i) {
		k = v[i]->index;
		symbol_name[k] = v[i]->name;
		symbol_value[k] = v[i]->value;
		symbol_prec[k] = v[i]->prec;
		symbol_assoc[k] = v[i]->assoc;
	}

	free(v);
}


void
pack_grammar(void)
{
	int i, j;
	int assoc, prec;

	ritem = reallocarray(NULL, nitems, sizeof(short));
	if (ritem == NULL)
		no_space();
	rlhs = reallocarray(NULL, nrules, sizeof(short));
	if (rlhs == NULL)
		no_space();
	rrhs = reallocarray(NULL, nrules + 1, sizeof(short));
	if (rrhs == NULL)
		no_space();
	rprec = reallocarray(rprec, nrules, sizeof(short));
	if (rprec == NULL)
		no_space();
	rassoc = realloc(rassoc, nrules);
	if (rassoc == NULL)
		no_space();

	ritem[0] = -1;
	ritem[1] = goal->index;
	ritem[2] = 0;
	ritem[3] = -2;
	rlhs[0] = 0;
	rlhs[1] = 0;
	rlhs[2] = start_symbol;
	rrhs[0] = 0;
	rrhs[1] = 0;
	rrhs[2] = 1;

	j = 4;
	for (i = 3; i < nrules; ++i) {
		rlhs[i] = plhs[i]->index;
		rrhs[i] = j;
		assoc = TOKEN;
		prec = 0;
		while (pitem[j]) {
			ritem[j] = pitem[j]->index;
			if (pitem[j]->class == TERM) {
				prec = pitem[j]->prec;
				assoc = pitem[j]->assoc;
			}
			++j;
		}
		ritem[j] = -i;
		++j;
		if (rprec[i] == UNDEFINED) {
			rprec[i] = prec;
			rassoc[i] = assoc;
		}
	}
	rrhs[i] = j;

	free(plhs);
	free(pitem);
}


void
print_grammar(void)
{
	int i, j, k;
	int spacing = 0;
	FILE *f = verbose_file;

	if (!vflag)
		return;

	k = 1;
	for (i = 2; i < nrules; ++i) {
		if (rlhs[i] != rlhs[i - 1]) {
			if (i != 2)
				fprintf(f, "\n");
			fprintf(f, "%4d  %s :", i - 2, symbol_name[rlhs[i]]);
			spacing = strlen(symbol_name[rlhs[i]]) + 1;
		} else {
			fprintf(f, "%4d  ", i - 2);
			j = spacing;
			while (--j >= 0)
				putc(' ', f);
			putc('|', f);
		}

		while (ritem[k] >= 0) {
			fprintf(f, " %s", symbol_name[ritem[k]]);
			++k;
		}
		++k;
		putc('\n', f);
	}
}


void
reader(void)
{
	write_section(banner);
	create_symbol_table();
	read_declarations();
	read_grammar();
	free_symbol_table();
	free_tags();
	pack_names();
	check_symbols();
	pack_symbols();
	pack_grammar();
	free_symbols();
	print_grammar();
}
