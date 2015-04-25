/*	$OpenBSD: irr_parser.c,v 1.13 2015/04/25 13:23:01 phessler Exp $ */

/*
 * Copyright (c) 2007 Henning Brauer <henning@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <ctype.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

#include "irrfilter.h"

#define PARSEBUF_INCREMENT 4096

int	 lineno;
char	*parsebuf = NULL;
size_t	 parsebuflen = 0;

void	 grow_parsebuf(void);
char	*irr_getln(FILE *f);
int	 parse_policy(char *, char *);
int	 policy_additem(char *, struct policy_item *);
int	 parse_asset(char *, char *);
int	 parse_route(char *, char *);

/*
 * parse_response() return values:
 * -1	error
 * 0	object not found
 * >0	number of lines matched plus 1
 */
int
parse_response(FILE *f, enum qtype qtype)
{
	char	*key, *val;
	int	 cnt, n;

	lineno = 1;
	cnt = 1;
	while ((val = irr_getln(f)) != NULL) {
		if (!strncmp(val, "%ERROR:101:", 11))	/* no entries found */
			return (0);

		if (val[0] == '%') {
			warnx("message from whois server: %s", val);
			return (-1);
		}

		key = strsep(&val, ":");
		if (val == NULL) {
			warnx("%u: %s", lineno, key);
			warnx("no \":\" found!");
			return (-1);
		}
		EATWS(val);

		switch (qtype) {
		case QTYPE_OWNAS:
			if ((n = parse_policy(key, val)) == -1)
				return (-1);
			break;
		case QTYPE_ASSET:
			if ((n = parse_asset(key, val)) == -1)
				return (-1);
			break;
		case QTYPE_ROUTE:
		case QTYPE_ROUTE6:
			if ((n = parse_route(key, val)) == -1)
				return (-1);
			break;
		default:
			err(1, "king bula suffers from dementia");
		}
		cnt += n;
	}

	return (cnt);
}

void
grow_parsebuf(void)
{
	char	*p;
	size_t	 newlen;

	newlen = parsebuflen + PARSEBUF_INCREMENT;
	if ((p = realloc(parsebuf, newlen)) == NULL)
		err(1, "grow_parsebuf realloc");
	parsebuf = p;
	parsebuflen = newlen;

	if (0)
		fprintf(stderr, "parsebuf now %lu bytes\n", (ulong)parsebuflen);
}

char *
irr_getln(FILE *f)
{
	int	 c, next, last;
	char	*p;

	if (parsebuf == NULL)
		grow_parsebuf();
	p = parsebuf;
	last = -1;

	do {
		c = getc(f);

		if (p == parsebuf) {	/* beginning of new line */
			if (c == '%') {
				next = getc(f);
				switch (next) {
				case ' ':		/* comment. skip over */
					while ((c = getc(f)) != '\n' &&
					    c != EOF)
						; /* nothing */
					break;
				case '\n':
				case EOF:
					c = next;
					break;
				default:
					ungetc(next, f);
					break;
				}
			}
		}

		if (c == '#') /* skip until \n */
			while ((c = getc(f)) != '\n' && c != EOF)
				; /* nothing */

		if (c == '\n') {
			lineno++;
			next = getc(f);
			if (next == '+')	/* continuation, skip the + */
				c = getc(f);
			else if (ISWS(next))	/* continuation */
				c = next;
			else
				ungetc(next, f);
		}


		if (c == '\n' || c == EOF) {
			if (c == EOF)
				if (ferror(f))
					err(1, "ferror");
			if (p > parsebuf) {
				*p = '\0';
				return (parsebuf);
			}
		} else {
			if (!(ISWS(c) && ISWS(last))) {
				if (p + 1 >= parsebuf + parsebuflen - 1) {
					size_t	offset;

					offset = p - parsebuf;
					grow_parsebuf();
					p = parsebuf + offset;
				}
				if (ISWS(c)) /* equal opportunity whitespace */
					*p++ = ' ';
				else
					*p++ = (char)c;
			}
			last = c;
		}
	} while (c != EOF);

	return (NULL);
}

/*
 * parse the policy from an aut-num object
 */

enum policy_parser_st {
	PO_NONE,
	PO_PEER_KEY,
	PO_PEER_AS,
	PO_PEER_ADDR,
	PO_RTR_KEY,
	PO_RTR_ADDR,
	PO_ACTION_KEY,
	PO_ACTION_SPEC,
	PO_FILTER_KEY,
	PO_FILTER_SPEC
};

int
parse_policy(char *key, char *val)
{
	struct policy_item	*pi;
	enum pdir		 dir;
	enum policy_parser_st	 st = PO_NONE, nextst;
	char			*tok, *router = "", *p;

	if (!strcmp(key, "import"))
		dir = IMPORT;
	else if (!strcmp(key, "export"))
		dir = EXPORT;
	else				/* ignore! */
		return (0);

	if (dir == EXPORT && (irrflags & F_IMPORTONLY))
		return (0);

	if ((pi = calloc(1, sizeof(*pi))) == NULL)
		err(1, "parse_policy calloc");
	pi->dir = dir;

	while ((tok = strsep(&val, " ")) != NULL) {
		nextst = PO_NONE;
		if (dir == IMPORT) {
			if (!strcmp(tok, "from"))
				nextst = PO_PEER_KEY;
			else if (!strcmp(tok, "at"))
				nextst = PO_RTR_KEY;
			else if (!strcmp(tok, "action"))
				nextst = PO_ACTION_KEY;
			else if (!strcmp(tok, "accept"))
				nextst = PO_FILTER_KEY;
		} else if (dir == EXPORT) {
			if (!strcmp(tok, "to"))
				nextst = PO_PEER_KEY;
			else if (!strcmp(tok, "at"))
				nextst = PO_RTR_KEY;
			else if (!strcmp(tok, "action"))
				nextst = PO_ACTION_KEY;
			else if (!strcmp(tok, "announce"))
				nextst = PO_FILTER_KEY;
		}

		if (nextst == PO_FILTER_KEY) /* rest is filter spec */
			if ((pi->filter = strdup(val)) == NULL)
				err(1, NULL);

		if (nextst == PO_ACTION_KEY) {
			/* action list. ends after last ; */
			p = strrchr(val, ';');
			if (p == NULL || !ISWS(*++p))
				errx(1, "syntax error in action spec");
			*p = '\0';
			if ((pi->action = strdup(val)) == NULL)
				err(1, NULL);
			val = ++p;
			while (ISWS(*p))
				p++;
		}

		switch (st) {
		case PO_NONE:
			if (nextst != PO_PEER_KEY)
				goto ppoerr;
			st = nextst;
			break;
		case PO_PEER_KEY:
			if (pi->peer_as == 0) {
				const char	*errstr;

				if (nextst != PO_NONE)
					goto ppoerr;
				if (strlen(tok) < 3 ||
				    strncasecmp(tok, "AS", 2) ||
				    !isdigit((unsigned char)tok[2]))
					errx(1, "peering spec \"%s\": format "
					    "error, AS expected", tok);
				pi->peer_as = strtonum(tok + 2, 1, UINT_MAX,
				    &errstr);
				if (errstr)
					errx(1, "peering spec \"%s\": format "
					    "error: %s", tok, errstr);
			} else {
				switch (nextst) {
				case PO_NONE:
					if (!strcasecmp(tok, "and") ||
					    !strcasecmp(tok, "or") ||
					    !strcasecmp(tok, "not"))
						fprintf(stderr, "compound "
						    "peering statements are "
						    "not supported");
					 else	/* peer address */
						if ((pi->peer_addr =
						    strdup(tok)) == NULL)
							err(1, NULL);
					break;
				case PO_RTR_KEY:
				case PO_ACTION_KEY:
				case PO_FILTER_KEY:
					st = nextst;
					break;
				default:
					goto ppoerr;
				}
			}
			break;
		case PO_PEER_AS:
		case PO_PEER_ADDR:
			err(1, "state error");
			break;
		case PO_RTR_KEY:
			if (nextst != PO_NONE)
				goto ppoerr;
			/* rtr address */
			if ((router = strdup(tok)) == NULL)
				err(1, NULL);
			st = PO_RTR_ADDR;
			break;
		case PO_RTR_ADDR:
			if (nextst != PO_ACTION_KEY &&
			    nextst != PO_FILTER_KEY)
				goto ppoerr;
			st = nextst;
			break;
		case PO_ACTION_KEY:
			/* already handled, next must be FILTER_KEY */
			if (nextst != PO_FILTER_KEY)
				goto ppoerr;
			st = nextst;
			break;
		case PO_FILTER_KEY:
			/* already handled */
			break;
		case PO_ACTION_SPEC:
		case PO_FILTER_SPEC:
			err(1, "state error");
			break;
		}
	}

	if (st != PO_FILTER_KEY)
		err(1, "state error");

	if (policy_additem(router, pi) == -1)
		return (-1);

	return (1);

ppoerr:
	free(pi);
	fprintf(stderr, "%u: parse error\n", lineno);
	return (-1);
}

int
policy_additem(char *router, struct policy_item *pi)
{
	struct router	*r;

	for (r = TAILQ_FIRST(&router_head); r != NULL &&
	    strcmp(r->address, router); r = TAILQ_NEXT(r, entry))
		; /* nothing */

	if (r == NULL) {
		if ((r = calloc(1, sizeof(*r))) == NULL ||
		    (r->address = strdup(router)) == NULL)
			err(1, NULL);
		TAILQ_INIT(&r->policy_h);
		TAILQ_INSERT_TAIL(&router_head, r, entry);
	}

	TAILQ_INSERT_TAIL(&r->policy_h, pi, entry);

	return (0);
}

/*
 * parse as-set: get members
 */

int
parse_asset(char *key, char *val)
{
	char	*tok;

	if (strcmp(key, "members"))	/* ignore everything else */
		return (0);

	while ((tok = strsep(&val, ",")) != NULL) {
		EATWS(tok);
		if (tok[0] != '\0')
			asset_addmember(tok);
	}

	return (1);
}

/*
 * parse route obj: just get the prefix
 */
int
parse_route(char *key, char *val)
{
	if (strcmp(key, "route") && strcmp(key, "route6"))
		/* ignore everything else */
		return (0);

	/* route is single-value, but seen trailing , and \r in the wild */
	if (strlen(val) > 0 && (val[strlen(val) - 1] == ',' ||
	    val[strlen(val) - 1] == '\r'))
		val[strlen(val) - 1] = '\0';

	return (prefixset_addmember(val));
}
