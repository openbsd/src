/*	$OpenBSD: conflex.c,v 1.30 2015/05/02 12:37:35 krw Exp $	*/

/* Lexical scanner for dhclient config file. */

/*
 * Copyright (c) 1995, 1996, 1997 The Internet Software Consortium.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of The Internet Software Consortium nor the names
 *    of its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INTERNET SOFTWARE CONSORTIUM AND
 * CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE INTERNET SOFTWARE CONSORTIUM OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This software has been written for the Internet Software Consortium
 * by Ted Lemon <mellon@fugue.com> in cooperation with Vixie
 * Enterprises.  To learn more about the Internet Software Consortium,
 * see ``http://www.vix.com/isc''.  To learn more about Vixie
 * Enterprises, see ``http://www.vix.com''.
 */

#include "dhcpd.h"
#include "dhctoken.h"

#include <vis.h>

int lexline;
int lexchar;
char *token_line;
char *prev_line;
char *cur_line;
char *tlname;

static char line1[81];
static char line2[81];
static int lpos;
static int line;
static int tlpos;
static int tline;
static int token;
static int ugflag;
static char *tval;
static char tokbuf[1500];
static char visbuf[1500];

static int get_char(FILE *);
static int get_token(FILE *);
static void skip_to_eol(FILE *);
static int read_string(FILE *);
static int read_number(int, FILE *);
static int read_num_or_name(int, FILE *);
static int intern(char *, int);

void
new_parse(char *name)
{
	/*
	 * Initialize all parsing state, as we are starting to parse a
	 * new file, 'name'.
	 */

	memset(line1, 0, sizeof(line1));
	memset(line2, 0, sizeof(line2));
	memset(tokbuf, 0, sizeof(tokbuf));

	lpos = line = 1;
	tlpos = tline = token = ugflag = 0;
	tval = NULL;

	lexline = lexchar = 0;
	cur_line = line1;
	prev_line = line2;
	token_line = cur_line;
	tlname = name;

	warnings_occurred = 0;
}

static int
get_char(FILE *cfile)
{
	int c = getc(cfile);
	if (!ugflag) {
		if (c == '\n') {
			if (cur_line == line1) {
				cur_line = line2;
				prev_line = line1;
			} else {
				cur_line = line1;
				prev_line = line2;
			}
			line++;
			lpos = 1;
			cur_line[0] = 0;
		} else if (c != EOF) {
			if (lpos < sizeof(line1)) {
				cur_line[lpos - 1] = c;
				cur_line[lpos] = 0;
			}
			lpos++;
		}
	} else
		ugflag = 0;
	return (c);
}

static int
get_token(FILE *cfile)
{
	int		c, ttok;
	static char	tb[2];
	int		l, p, u;

	u = ugflag;

	do {
		l = line;
		p = lpos - u;
		u = 0;

		c = get_char(cfile);

		if (isascii(c) && isspace(c))
			continue;
		if (c == '#') {
			skip_to_eol(cfile);
			continue;
		}
		if (c == '"') {
			lexline = l;
			lexchar = p;
			ttok = read_string(cfile);
			break;
		}
		if ((isascii(c) && isdigit(c)) || c == '-') {
			lexline = l;
			lexchar = p;
			ttok = read_number(c, cfile);
			break;
		} else if (isascii(c) && isalpha(c)) {
			lexline = l;
			lexchar = p;
			ttok = read_num_or_name(c, cfile);
			break;
		} else {
			lexline = l;
			lexchar = p;
			tb[0] = c;
			tb[1] = 0;
			tval = tb;
			ttok = c;
			break;
		}
	} while (1);
	return (ttok);
}

int
next_token(char **rval, FILE *cfile)
{
	int	rv;

	if (token) {
		if (lexline != tline)
			token_line = cur_line;
		lexchar = tlpos;
		lexline = tline;
		rv = token;
		token = 0;
	} else {
		rv = get_token(cfile);
		token_line = cur_line;
	}
	if (rval)
		*rval = tval;

	return (rv);
}

int
peek_token(char **rval, FILE *cfile)
{
	int	x;

	if (!token) {
		tlpos = lexchar;
		tline = lexline;
		token = get_token(cfile);
		if (lexline != tline)
			token_line = prev_line;
		x = lexchar;
		lexchar = tlpos;
		tlpos = x;
		x = lexline;
		lexline = tline;
		tline = x;
	}
	if (rval)
		*rval = tval;

	return (token);
}

static void
skip_to_eol(FILE *cfile)
{
	int	c;

	do {
		c = get_char(cfile);
		if (c == EOF)
			return;
		if (c == '\n')
			return;
	} while (1);
}

static int
read_string(FILE *cfile)
{
	int i, c, bs;

	/*
	 * Read in characters until an un-escaped '"' is encountered. And
	 * then unvis the data that was read.
	 */
	bs = i = 0;
	memset(visbuf, 0, sizeof(visbuf));
	while ((c = get_char(cfile)) != EOF) {
		if (c == '"' && bs == 0)
			break;

		visbuf[i++] = c;
		if (bs)
			bs = 0;
		else if (c == '\\')
			bs = 1;

		if (i == sizeof(visbuf) - 1)
			break;
	}
	if (bs == 1)
		visbuf[--i] = '\0';
	i = strnunvis(tokbuf, visbuf, sizeof(tokbuf));

	if (c == EOF)
		parse_warn("eof in string constant");
	else if (i == -1 || i >= sizeof(tokbuf))
		parse_warn("string constant too long");

	tval = tokbuf;

	return (TOK_STRING);
}

static int
read_number(int c, FILE *cfile)
{
	int	seenx = 0, i = 0, token = TOK_NUMBER;

	tokbuf[i++] = c;
	for (; i < sizeof(tokbuf); i++) {
		c = get_char(cfile);
		if (!seenx && c == 'x')
			seenx = 1;
		else if (!isascii(c) || !isxdigit(c)) {
			ungetc(c, cfile);
			ugflag = 1;
			break;
		}
		tokbuf[i] = c;
	}
	if (i == sizeof(tokbuf)) {
		parse_warn("numeric token larger than internal buffer");
		i--;
	}
	tokbuf[i] = 0;
	tval = tokbuf;

	return (token);
}

static int
read_num_or_name(int c, FILE *cfile)
{
	int	i = 0;
	int	rv = TOK_NUMBER_OR_NAME;

	tokbuf[i++] = c;
	for (; i < sizeof(tokbuf); i++) {
		c = get_char(cfile);
		if (!isascii(c) || (c != '-' && c != '_' && !isalnum(c))) {
			ungetc(c, cfile);
			ugflag = 1;
			break;
		}
		if (!isxdigit(c))
			rv = TOK_NAME;
		tokbuf[i] = c;
	}
	if (i == sizeof(tokbuf)) {
		parse_warn("token larger than internal buffer");
		i--;
	}
	tokbuf[i] = 0;
	tval = tokbuf;

	return (intern(tval, rv));
}

static const struct keywords {
	const char	*k_name;
	int		k_val;
} keywords[] = {
	{ "alias",				TOK_ALIAS },
	{ "append",				TOK_APPEND },
	{ "backoff-cutoff",			TOK_BACKOFF_CUTOFF },
	{ "bootp",				TOK_BOOTP },
	{ "default",				TOK_DEFAULT },
	{ "deny",				TOK_DENY },
	{ "ethernet",				TOK_ETHERNET },
	{ "expire",				TOK_EXPIRE },
	{ "filename",				TOK_FILENAME },
	{ "fixed-address",			TOK_FIXED_ADDR },
	{ "hardware",				TOK_HARDWARE },
	{ "ignore",				TOK_IGNORE },
	{ "initial-interval",			TOK_INITIAL_INTERVAL },
	{ "interface",				TOK_INTERFACE },
	{ "lease",				TOK_LEASE },
	{ "link-timeout",			TOK_LINK_TIMEOUT },
	{ "media",				TOK_MEDIA },
	{ "medium",				TOK_MEDIUM },
	{ "next-server",			TOK_NEXT_SERVER },
	{ "option",				TOK_OPTION },
	{ "prepend",				TOK_PREPEND },
	{ "rebind",				TOK_REBIND },
	{ "reboot",				TOK_REBOOT },
	{ "reject",				TOK_REJECT },
	{ "renew",				TOK_RENEW },
	{ "request",				TOK_REQUEST },
	{ "require",				TOK_REQUIRE },
	{ "retry",				TOK_RETRY },
	{ "select-timeout",			TOK_SELECT_TIMEOUT },
	{ "send",				TOK_SEND },
	{ "server-name",			TOK_SERVER_NAME },
	{ "supersede",				TOK_SUPERSEDE },
	{ "timeout",				TOK_TIMEOUT }
};

int	kw_cmp(const void *k, const void *e);

int
kw_cmp(const void *k, const void *e)
{
	return (strcasecmp(k, ((const struct keywords *)e)->k_name));
}

static int
intern(char *atom, int dfv)
{
	const struct keywords *p;

	p = bsearch(atom, keywords, sizeof(keywords)/sizeof(keywords[0]),
	    sizeof(keywords[0]), kw_cmp);
	if (p)
		return (p->k_val);
	return (dfv);
}
