/*-
 * Copyright (c) 2010, 2013 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by David A. Holland.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "utils.h"
#include "mode.h"
#include "place.h"
#include "files.h"
#include "directive.h"
#include "macro.h"
#include "eval.h"
#include "output.h"

struct ifstate {
	struct ifstate *prev;
	struct place startplace;
	bool curtrue;
	bool evertrue;
	bool seenelse;
};

static struct ifstate *ifstate;

////////////////////////////////////////////////////////////
// common parsing bits

static
void
uncomment(char *buf)
{
	char *s, *t, *u = NULL;
	bool incomment = false;
	bool inesc = false;
	bool inquote = false;
	char quote = '\0';

	for (s = t = buf; *s; s++) {
		if (incomment) {
			if (s[0] == '*' && s[1] == '/') {
				s++;
				incomment = false;
			}
		} else {
			if (!inquote && s[0] == '/' && s[1] == '*') {
				incomment = true;
			} else {
				if (inesc) {
					inesc = false;
				} else if (s[0] == '\\') {
					inesc = true;
				} else if (!inquote &&
					   (s[0] == '"' || s[0] == '\'')) {
					inquote = true;
					quote = s[0];
				} else if (inquote && s[0] == quote) {
					inquote = false;
				}

				if (t != s) {
					*t = *s;
				}
				if (!strchr(ws, *t)) {
					u = t;
				}
				t++;
			}
		}
	}
	if (u) {
		/* end string after last non-whitespace char */
		u[1] = '\0';
	} else {
		*t = '\0';
	}
}

static
void
oneword(const char *what, struct place *p2, char *line)
{
	size_t pos;

	pos = strcspn(line, ws);
	if (line[pos] != '\0') {
		p2->column += pos;
		complain(p2, "Garbage after %s argument", what);
		complain_fail();
		line[pos] = '\0';
	}
}

////////////////////////////////////////////////////////////
// if handling

static
struct ifstate *
ifstate_create(struct ifstate *prev, struct place *p, bool startstate)
{
	struct ifstate *is;

	is = domalloc(sizeof(*is));
	is->prev = prev;
	if (p != NULL) {
		is->startplace = *p;
	} else {
		place_setbuiltin(&is->startplace, 1);
	}
	is->curtrue = startstate;
	is->evertrue = is->curtrue;
	is->seenelse = false;
	return is;
}

static
void
ifstate_destroy(struct ifstate *is)
{
	dofree(is, sizeof(*is));
}

static
void
ifstate_push(struct place *p, bool startstate)
{
	struct ifstate *newstate;

	newstate = ifstate_create(ifstate, p, startstate);
	if (!ifstate->curtrue) {
		newstate->curtrue = false;
		newstate->evertrue = true;
	}
	ifstate = newstate;
}

static
void
ifstate_pop(void)
{
	struct ifstate *is;

	is = ifstate;
	ifstate = ifstate->prev;
	ifstate_destroy(is);
}

static
void
d_if(struct place *p, struct place *p2, char *line)
{
	char *expr;
	bool val;
	struct place p3 = *p2;
	size_t oldlen;

	expr = macroexpand(p2, line, strlen(line), true);

	oldlen = strlen(expr);
	uncomment(expr);
	/* trim to fit, so the malloc debugging won't complain */
	expr = dorealloc(expr, oldlen + 1, strlen(expr) + 1);

	if (ifstate->curtrue) {
		val = eval(&p3, expr);
	} else {
		val = 0;
	}
	ifstate_push(p, val);
	dostrfree(expr);
}

static
void
d_ifdef(struct place *p, struct place *p2, char *line)
{
	uncomment(line);
	oneword("#ifdef", p2, line);
	ifstate_push(p, macro_isdefined(line));
}

static
void
d_ifndef(struct place *p, struct place *p2, char *line)
{
	uncomment(line);
	oneword("#ifndef", p2, line);
	ifstate_push(p, !macro_isdefined(line));
}

static
void
d_elif(struct place *p, struct place *p2, char *line)
{
	char *expr;
	struct place p3 = *p2;
	size_t oldlen;

	if (ifstate->seenelse) {
		complain(p, "#elif after #else");
		complain_fail();
	}

	if (ifstate->evertrue) {
		ifstate->curtrue = false;
	} else {
		expr = macroexpand(p2, line, strlen(line), true);

		oldlen = strlen(expr);
		uncomment(expr);
		/* trim to fit, so the malloc debugging won't complain */
		expr = dorealloc(expr, oldlen + 1, strlen(expr) + 1);

		ifstate->curtrue = eval(&p3, expr);
		ifstate->evertrue = ifstate->curtrue;
		dostrfree(expr);
	}
}

static
void
d_else(struct place *p, struct place *p2, char *line)
{
	(void)p2;
	(void)line;

	if (ifstate->seenelse) {
		complain(p, "Multiple #else directives in one conditional");
		complain_fail();
	}

	ifstate->curtrue = !ifstate->evertrue;
	ifstate->evertrue = true;
	ifstate->seenelse = true;
}

static
void
d_endif(struct place *p, struct place *p2, char *line)
{
	(void)p2;
	(void)line;

	if (ifstate->prev == NULL) {
		complain(p, "Unmatched #endif");
		complain_fail();
	} else {
		ifstate_pop();
	}
}

////////////////////////////////////////////////////////////
// macros

static
void
d_define(struct place *p, struct place *p2, char *line)
{
	size_t pos, argpos;
	struct place p3, p4;

	(void)p;

	/*
	 * line may be:
	 *    macro expansion
	 *    macro(arg, arg, ...) expansion
	 */

	pos = strcspn(line, " \t\f\v(");
	if (line[pos] == '(') {
		line[pos++] = '\0';
		argpos = pos;
		pos = pos + strcspn(line+pos, "()");
		if (line[pos] == '(') {
			p2->column += pos;
			complain(p2, "Left parenthesis in macro parameters");
			complain_fail();
			return;
		}
		if (line[pos] != ')') {
			p2->column += pos;
			complain(p2, "Unclosed macro parameter list");
			complain_fail();
			return;
		}
		line[pos++] = '\0';
#if 0
		if (!strchr(ws, line[pos])) {
			p2->column += pos;
			complain(p2, "Trash after macro parameter list");
			complain_fail();
			return;
		}
#endif
	} else if (line[pos] == '\0') {
		argpos = 0;
	} else {
		line[pos++] = '\0';
		argpos = 0;
	}

	pos += strspn(line+pos, ws);

	p3 = *p2;
	p3.column += argpos;

	p4 = *p2;
	p4.column += pos;

	if (argpos) {
		macro_define_params(p2, line, &p3,
				    line + argpos, &p4,
				    line + pos);
	} else {
		macro_define_plain(p2, line, &p4, line + pos);
	}
}

static
void
d_undef(struct place *p, struct place *p2, char *line)
{
	(void)p;

	uncomment(line);
	oneword("#undef", p2, line);
	macro_undef(line);
}

////////////////////////////////////////////////////////////
// includes

static
bool
tryinclude(struct place *p, char *line)
{
	size_t len;

	len = strlen(line);
	if (len > 2 && line[0] == '"' && line[len-1] == '"') {
		line[len-1] = '\0';
		file_readquote(p, line+1);
		line[len-1] = '"';
		return true;
	}
	if (len > 2 && line[0] == '<' && line[len-1] == '>') {
		line[len-1] = '\0';
		file_readbracket(p, line+1);
		line[len-1] = '>';
		return true;
	}
	return false;
}

static
void
d_include(struct place *p, struct place *p2, char *line)
{
	char *text;
	size_t oldlen;

	uncomment(line);
	if (tryinclude(p, line)) {
		return;
	}
	text = macroexpand(p2, line, strlen(line), false);

	oldlen = strlen(text);
	uncomment(text);
	/* trim to fit, so the malloc debugging won't complain */
	text = dorealloc(text, oldlen + 1, strlen(text) + 1);

	if (tryinclude(p, text)) {
		dostrfree(text);
		return;
	}
	complain(p, "Illegal #include directive");
	complain(p, "Before macro expansion: #include %s", line);
	complain(p, "After macro expansion: #include %s", text);
	dostrfree(text);
	complain_fail();
}

static
void
d_line(struct place *p, struct place *p2, char *line)
{
	(void)p2;
	(void)line;

	/* XXX */
	complain(p, "Sorry, no #line yet");
}

////////////////////////////////////////////////////////////
// messages

static
void
d_warning(struct place *p, struct place *p2, char *line)
{
	char *msg;

	msg = macroexpand(p2, line, strlen(line), false);
	complain(p, "#warning: %s", msg);
	if (mode.werror) {
		complain_fail();
	}
	dostrfree(msg);
}

static
void
d_error(struct place *p, struct place *p2, char *line)
{
	char *msg;

	msg = macroexpand(p2, line, strlen(line), false);
	complain(p, "#error: %s", msg);
	complain_fail();
	dostrfree(msg);
}

////////////////////////////////////////////////////////////
// other

static
void
d_pragma(struct place *p, struct place *p2, char *line)
{
	(void)p2;

	complain(p, "#pragma %s", line);
	complain_fail();
}

////////////////////////////////////////////////////////////
// directive table

static const struct {
	const char *name;
	bool ifskip;
	void (*func)(struct place *, struct place *, char *line);
} directives[] = {
	{ "define",  true,  d_define },
	{ "elif",    false, d_elif },
	{ "else",    false, d_else },
	{ "endif",   false, d_endif },
	{ "error",   true,  d_error },
	{ "if",      false, d_if },
	{ "ifdef",   false, d_ifdef },
	{ "ifndef",  false, d_ifndef },
	{ "include", true,  d_include },
	{ "line",    true,  d_line },
	{ "pragma",  true,  d_pragma },
	{ "undef",   true,  d_undef },
	{ "warning", true,  d_warning },
};
static const unsigned numdirectives = HOWMANY(directives);

static
void
directive_gotdirective(struct place *p, char *line)
{
	struct place p2;
	size_t len, skip;
	unsigned i;

	p2 = *p;
	for (i=0; i<numdirectives; i++) {
		len = strlen(directives[i].name);
		if (!strncmp(line, directives[i].name, len) &&
		    strchr(ws, line[len])) {
			if (directives[i].ifskip && !ifstate->curtrue) {
				return;
			}
			skip = len + strspn(line+len, ws);
			p2.column += skip;
			line += skip;

			len = strlen(line);
			len = notrailingws(line, len);
			if (len < strlen(line)) {
				line[len] = '\0';
			}
			directives[i].func(p, &p2, line);
			return;
		}
	}
	/* ugh. allow # by itself, including with a comment after it */
	uncomment(line);
	if (line[0] == '\0') {
		return;
	}

	skip = strcspn(line, ws);
	complain(p, "Unknown directive #%.*s", (int)skip, line);
	complain_fail();
}

/*
 * Check for nested comment delimiters in LINE.
 */
static
size_t
directive_scancomments(const struct place *p, char *line, size_t len)
{
	size_t pos;
	bool incomment;
	struct place p2;

	p2 = *p;
	incomment = 0;
	for (pos = 0; pos+1 < len; pos++) {
		if (line[pos] == '/' && line[pos+1] == '*') {
			if (incomment) {
				complain(&p2, "Warning: %c%c within comment",
					 '/', '*');
				if (mode.werror) {
					complain_failed();
				}
			} else {
				incomment = true;
			}
			pos++;
		} else if (line[pos] == '*' && line[pos+1] == '/') {
			if (incomment) {
				incomment = false;
			} else {
				/* stray end-comment; should we care? */
			}
			pos++;
		}
		if (line[pos] == '\n') {
			p2.line++;
			p2.column = 0;
		} else {
			p2.column++;
		}
	}

	/* multiline comments are supposed to arrive in a single buffer */
	assert(!incomment);
	return len;
}

void
directive_gotline(struct place *p, char *line, size_t len)
{
	size_t skip;

	if (warns.nestcomment) {
		directive_scancomments(p, line, len);
	}

	/* check if we have a directive line (# exactly in column 0) */
	if (line[0] == '#') {
		skip = 1 + strspn(line + 1, ws);
		assert(skip <= len);
		p->column += skip;
		assert(line[len] == '\0');
		directive_gotdirective(p, line+skip /*, length = len-skip */);
		p->column += len-skip;
	} else if (ifstate->curtrue) {
		macro_sendline(p, line, len);
		p->column += len;
	}
}

void
directive_goteof(struct place *p)
{
	while (ifstate->prev != NULL) {
		complain(p, "Missing #endif");
		complain(&ifstate->startplace, "...opened at this point");
		complain_failed();
		ifstate_pop();
	}
	macro_sendeof(p);
}

////////////////////////////////////////////////////////////
// module initialization

void
directive_init(void)
{
	ifstate = ifstate_create(NULL, NULL, true);
}

void
directive_cleanup(void)
{
	assert(ifstate->prev == NULL);
	ifstate_destroy(ifstate);
	ifstate = NULL;
}
