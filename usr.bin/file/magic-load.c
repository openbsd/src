/* $OpenBSD: magic-load.c,v 1.11 2015/08/11 22:12:48 nicm Exp $ */

/*
 * Copyright (c) 2015 Nicholas Marriott <nicm@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <regex.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "magic.h"
#include "xmalloc.h"

static int
magic_odigit(u_char c)
{
	if (c >= '0' && c <= '7')
		return (c - '0');
	return (-1);
}

static int
magic_xdigit(u_char c)
{
	if (c >= '0' && c <= '9')
		return (c - '0');
	if (c >= 'a' && c <= 'f')
		return (10 + c - 'a');
	if (c >= 'A' && c <= 'F')
		return (10 + c - 'A');
	return (-1);
}

static void
magic_mark_text(struct magic_line *ml, int text)
{
	do {
		ml->text = text;
		ml = ml->parent;
	} while (ml != NULL);
}

static int
magic_make_pattern(struct magic_line *ml, const char *name, regex_t *re,
    const char *p)
{
	int	error;
	char	errbuf[256];

	error = regcomp(re, p, REG_EXTENDED|REG_NOSUB);
	if (error != 0) {
		regerror(error, re, errbuf, sizeof errbuf);
		magic_warn(ml, "bad %s pattern: %s", name, errbuf);
		return (-1);
	}
	return (0);
}

static int
magic_set_result(struct magic_line *ml, const char *s)
{
	const char	*fmt;
	const char	*endfmt;
	const char	*cp;
	regex_t		*re = NULL;
	regmatch_t	 pmatch;
	size_t		 fmtlen;

	while (isspace((u_char)*s))
		s++;
	if (*s == '\0') {
		ml->result = NULL;
		return (0);
	}
	ml->result = xstrdup(s);

	fmt = NULL;
	for (cp = s; *cp != '\0'; cp++) {
		if (cp[0] == '%' && cp[1] != '%') {
			if (fmt != NULL) {
				magic_warn(ml, "multiple formats");
				return (-1);
			}
			fmt = cp;
		}
	}
	if (fmt == NULL)
		return (0);
	fmt++;

	for (endfmt = fmt; *endfmt != '\0'; endfmt++) {
		if (strchr("diouxXeEfFgGsc", *endfmt) != NULL)
			break;
	}
	if (*endfmt == '\0') {
		magic_warn(ml, "unterminated format");
		return (-1);
	}
	fmtlen = endfmt + 1 - fmt;
	if (fmtlen > 32) {
		magic_warn(ml, "format too long");
		return (-1);
	}

	if (*endfmt == 's') {
		switch (ml->type) {
		case MAGIC_TYPE_DATE:
		case MAGIC_TYPE_LDATE:
		case MAGIC_TYPE_UDATE:
		case MAGIC_TYPE_ULDATE:
		case MAGIC_TYPE_BEDATE:
		case MAGIC_TYPE_BELDATE:
		case MAGIC_TYPE_UBEDATE:
		case MAGIC_TYPE_UBELDATE:
		case MAGIC_TYPE_QDATE:
		case MAGIC_TYPE_QLDATE:
		case MAGIC_TYPE_UQDATE:
		case MAGIC_TYPE_UQLDATE:
		case MAGIC_TYPE_BEQDATE:
		case MAGIC_TYPE_BEQLDATE:
		case MAGIC_TYPE_UBEQDATE:
		case MAGIC_TYPE_UBEQLDATE:
		case MAGIC_TYPE_LEQDATE:
		case MAGIC_TYPE_LEQLDATE:
		case MAGIC_TYPE_ULEQDATE:
		case MAGIC_TYPE_ULEQLDATE:
		case MAGIC_TYPE_LEDATE:
		case MAGIC_TYPE_LELDATE:
		case MAGIC_TYPE_ULEDATE:
		case MAGIC_TYPE_ULELDATE:
		case MAGIC_TYPE_MEDATE:
		case MAGIC_TYPE_MELDATE:
		case MAGIC_TYPE_STRING:
		case MAGIC_TYPE_PSTRING:
		case MAGIC_TYPE_BESTRING16:
		case MAGIC_TYPE_LESTRING16:
		case MAGIC_TYPE_REGEX:
		case MAGIC_TYPE_SEARCH:
			break;
		default:
			ml->stringify = 1;
			break;
		}
	}

	if (!ml->root->compiled) {
		/*
		 * XXX %ld (and %lu and so on) is invalid on 64-bit platforms
		 * with byte, short, long. We get lucky because our first and
		 * only argument ends up in a register. Accept it for now.
		 */
		if (magic_make_pattern(ml, "short", &ml->root->format_short,
		    "^-?[0-9]*(\\.[0-9]*)?(c|(l|h|hh)?[iduxX])$") != 0)
			return (-1);
		if (magic_make_pattern(ml, "long", &ml->root->format_long,
		    "^-?[0-9]*(\\.[0-9]*)?(c|(l|h|hh)?[iduxX])$") != 0)
			return (-1);
		if (magic_make_pattern(ml, "quad", &ml->root->format_quad,
		    "^-?[0-9]*(\\.[0-9]*)?ll[iduxX]$") != 0)
			return (-1);
		if (magic_make_pattern(ml, "float", &ml->root->format_float,
		    "^-?[0-9]*(\\.[0-9]*)?[eEfFgG]$") != 0)
			return (-1);
		if (magic_make_pattern(ml, "string", &ml->root->format_string,
		    "^-?[0-9]*(\\.[0-9]*)?s$") != 0)
			return (-1);
		ml->root->compiled = 1;
	}

	if (ml->stringify)
		re = &ml->root->format_string;
	else {
		switch (ml->type) {
		case MAGIC_TYPE_NONE:
		case MAGIC_TYPE_DEFAULT:
			return (0); /* don't use result */
		case MAGIC_TYPE_BYTE:
		case MAGIC_TYPE_UBYTE:
		case MAGIC_TYPE_SHORT:
		case MAGIC_TYPE_USHORT:
		case MAGIC_TYPE_BESHORT:
		case MAGIC_TYPE_UBESHORT:
		case MAGIC_TYPE_LESHORT:
		case MAGIC_TYPE_ULESHORT:
			re = &ml->root->format_short;
			break;
		case MAGIC_TYPE_LONG:
		case MAGIC_TYPE_ULONG:
		case MAGIC_TYPE_BELONG:
		case MAGIC_TYPE_UBELONG:
		case MAGIC_TYPE_LELONG:
		case MAGIC_TYPE_ULELONG:
		case MAGIC_TYPE_MELONG:
			re = &ml->root->format_long;
			break;
		case MAGIC_TYPE_QUAD:
		case MAGIC_TYPE_UQUAD:
		case MAGIC_TYPE_BEQUAD:
		case MAGIC_TYPE_UBEQUAD:
		case MAGIC_TYPE_LEQUAD:
		case MAGIC_TYPE_ULEQUAD:
			re = &ml->root->format_quad;
			break;
		case MAGIC_TYPE_FLOAT:
		case MAGIC_TYPE_BEFLOAT:
		case MAGIC_TYPE_LEFLOAT:
		case MAGIC_TYPE_DOUBLE:
		case MAGIC_TYPE_BEDOUBLE:
		case MAGIC_TYPE_LEDOUBLE:
			re = &ml->root->format_float;
			break;
		case MAGIC_TYPE_DATE:
		case MAGIC_TYPE_LDATE:
		case MAGIC_TYPE_UDATE:
		case MAGIC_TYPE_ULDATE:
		case MAGIC_TYPE_BEDATE:
		case MAGIC_TYPE_BELDATE:
		case MAGIC_TYPE_UBEDATE:
		case MAGIC_TYPE_UBELDATE:
		case MAGIC_TYPE_QDATE:
		case MAGIC_TYPE_QLDATE:
		case MAGIC_TYPE_UQDATE:
		case MAGIC_TYPE_UQLDATE:
		case MAGIC_TYPE_BEQDATE:
		case MAGIC_TYPE_BEQLDATE:
		case MAGIC_TYPE_UBEQDATE:
		case MAGIC_TYPE_UBEQLDATE:
		case MAGIC_TYPE_LEQDATE:
		case MAGIC_TYPE_LEQLDATE:
		case MAGIC_TYPE_ULEQDATE:
		case MAGIC_TYPE_ULEQLDATE:
		case MAGIC_TYPE_LEDATE:
		case MAGIC_TYPE_LELDATE:
		case MAGIC_TYPE_ULEDATE:
		case MAGIC_TYPE_ULELDATE:
		case MAGIC_TYPE_MEDATE:
		case MAGIC_TYPE_MELDATE:
		case MAGIC_TYPE_STRING:
		case MAGIC_TYPE_PSTRING:
		case MAGIC_TYPE_REGEX:
		case MAGIC_TYPE_SEARCH:
			re = &ml->root->format_string;
			break;
		case MAGIC_TYPE_BESTRING16:
		case MAGIC_TYPE_LESTRING16:
			magic_warn(ml, "unsupported type: %s", ml->type_string);
			return (-1);
		}
	}

	pmatch.rm_so = 0;
	pmatch.rm_eo = fmtlen;
	if (regexec(re, fmt, 1, &pmatch, REG_STARTEND) != 0) {
		magic_warn(ml, "bad format for %s: %%%.*s", ml->type_string,
		    (int)fmtlen, fmt);
		return (-1);
	}

	return (0);
}

static u_int
magic_get_strength(struct magic_line *ml)
{
	int	n;
	size_t	size;

	if (ml->test_not || ml->test_operator == 'x')
		return (1);

	n = 2 * MAGIC_STRENGTH_MULTIPLIER;
	switch (ml->type) {
	case MAGIC_TYPE_NONE:
	case MAGIC_TYPE_DEFAULT:
		return (0);
	case MAGIC_TYPE_BYTE:
	case MAGIC_TYPE_UBYTE:
		n += 1 * MAGIC_STRENGTH_MULTIPLIER;
		break;
	case MAGIC_TYPE_SHORT:
	case MAGIC_TYPE_USHORT:
	case MAGIC_TYPE_BESHORT:
	case MAGIC_TYPE_UBESHORT:
	case MAGIC_TYPE_LESHORT:
	case MAGIC_TYPE_ULESHORT:
		n += 2 * MAGIC_STRENGTH_MULTIPLIER;
		break;
	case MAGIC_TYPE_LONG:
	case MAGIC_TYPE_ULONG:
	case MAGIC_TYPE_FLOAT:
	case MAGIC_TYPE_DATE:
	case MAGIC_TYPE_LDATE:
	case MAGIC_TYPE_UDATE:
	case MAGIC_TYPE_ULDATE:
	case MAGIC_TYPE_BELONG:
	case MAGIC_TYPE_UBELONG:
	case MAGIC_TYPE_BEFLOAT:
	case MAGIC_TYPE_BEDATE:
	case MAGIC_TYPE_BELDATE:
	case MAGIC_TYPE_UBEDATE:
	case MAGIC_TYPE_UBELDATE:
		n += 4 * MAGIC_STRENGTH_MULTIPLIER;
		break;
	case MAGIC_TYPE_QUAD:
	case MAGIC_TYPE_UQUAD:
	case MAGIC_TYPE_DOUBLE:
	case MAGIC_TYPE_QDATE:
	case MAGIC_TYPE_QLDATE:
	case MAGIC_TYPE_UQDATE:
	case MAGIC_TYPE_UQLDATE:
	case MAGIC_TYPE_BEQUAD:
	case MAGIC_TYPE_UBEQUAD:
	case MAGIC_TYPE_BEDOUBLE:
	case MAGIC_TYPE_BEQDATE:
	case MAGIC_TYPE_BEQLDATE:
	case MAGIC_TYPE_UBEQDATE:
	case MAGIC_TYPE_UBEQLDATE:
	case MAGIC_TYPE_LEQUAD:
	case MAGIC_TYPE_ULEQUAD:
	case MAGIC_TYPE_LEDOUBLE:
	case MAGIC_TYPE_LEQDATE:
	case MAGIC_TYPE_LEQLDATE:
	case MAGIC_TYPE_ULEQDATE:
	case MAGIC_TYPE_ULEQLDATE:
	case MAGIC_TYPE_LELONG:
	case MAGIC_TYPE_ULELONG:
	case MAGIC_TYPE_LEFLOAT:
	case MAGIC_TYPE_LEDATE:
	case MAGIC_TYPE_LELDATE:
	case MAGIC_TYPE_ULEDATE:
	case MAGIC_TYPE_ULELDATE:
	case MAGIC_TYPE_MELONG:
	case MAGIC_TYPE_MEDATE:
	case MAGIC_TYPE_MELDATE:
		n += 8 * MAGIC_STRENGTH_MULTIPLIER;
		break;
	case MAGIC_TYPE_STRING:
	case MAGIC_TYPE_PSTRING:
		n += ml->test_string_size * MAGIC_STRENGTH_MULTIPLIER;
		break;
	case MAGIC_TYPE_BESTRING16:
	case MAGIC_TYPE_LESTRING16:
		n += ml->test_string_size * MAGIC_STRENGTH_MULTIPLIER / 2;
		break;
	case MAGIC_TYPE_REGEX:
	case MAGIC_TYPE_SEARCH:
		size = MAGIC_STRENGTH_MULTIPLIER / ml->test_string_size;
		if (size < 1)
			size = 1;
		n += ml->test_string_size * size;
		break;
	}
	switch (ml->test_operator) {
	case '=':
		n += MAGIC_STRENGTH_MULTIPLIER;
		break;
	case '<':
	case '>':
	case '[':
	case ']':
		n -= 2 * MAGIC_STRENGTH_MULTIPLIER;
		break;
	case '^':
	case '&':
		n -= MAGIC_STRENGTH_MULTIPLIER;
		break;
	}
	return (n <= 0 ? 1 : n);
}

static int
magic_get_string(char **line, char *out, size_t *outlen)
{
	char	*start, *cp, c;
	int	 d0, d1, d2;

	start = out;
	for (cp = *line; *cp != '\0' && !isspace((u_char)*cp); cp++) {
		if (*cp != '\\') {
			*out++ = *cp;
			continue;
		}

		switch (c = *++cp) {
		case '\0': /* end of line */
			return (-1);
		case ' ':
			*out++ = ' ';
			break;
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
			d0 = magic_odigit(cp[0]);
			if (cp[0] != '\0')
				d1 = magic_odigit(cp[1]);
			else
				d1 = -1;
			if (cp[0] != '\0' && cp[1] != '\0')
				d2 = magic_odigit(cp[2]);
			else
				d2 = -1;

			if (d0 != -1 && d1 != -1 && d2 != -1) {
				*out = d2 | (d1 << 3) | (d0 << 6);
				cp += 2;
			} else if (d0 != -1 && d1 != -1) {
				*out = d1 | (d0 << 3);
				cp++;
			} else if (d0 != -1)
				*out = d0;
			else
				return (-1);
			out++;
			break;
		case 'x':
			d0 = magic_xdigit(cp[1]);
			if (cp[1] != '\0')
				d1 = magic_xdigit(cp[2]);
			else
				d1 = -1;

			if (d0 != -1 && d1 != -1) {
				*out = d1 | (d0 << 4);
				cp += 2;
			} else if (d0 != -1) {
				*out = d0;
				cp++;
			} else
				return (-1);
			out++;

			break;
		case 'a':
			*out++ = '\a';
			break;
		case 'b':
			*out++ = '\b';
			break;
		case 't':
			*out++ = '\t';
			break;
		case 'f':
			*out++ = '\f';
			break;
		case 'n':
			*out++ = '\n';
			break;
		case 'r':
			*out++ = '\r';
			break;
		case '\\':
			*out++ = '\\';
			break;
		case '\'':
			*out++ = '\'';
			break;
		case '\"':
			*out++ = '\"';
			break;
		default:
			*out++ = c;
			break;
		}
	}
	*out = '\0';
	*outlen = out - start;

	*line = cp;
	return (0);
}

static int
magic_parse_offset(struct magic_line *ml, char **line)
{
	char	*copy, *s, *cp, *endptr;

	while (isspace((u_char)**line))
		(*line)++;
	copy = s = cp = xmalloc(strlen(*line) + 1);
	while (**line != '\0' && !isspace((u_char)**line))
		*cp++ = *(*line)++;
	*cp = '\0';

	ml->offset = 0;
	ml->offset_relative = 0;

	ml->indirect_type = ' ';
	ml->indirect_relative = 0;
	ml->indirect_offset = 0;
	ml->indirect_operator = ' ';
	ml->indirect_operand = 0;

	if (*s == '&') {
		ml->offset_relative = 1;
		s++;
	}

	if (*s != '(') {
		endptr = magic_strtoll(s, &ml->offset);
		if (endptr == NULL || *endptr != '\0') {
			magic_warn(ml, "missing closing bracket");
			goto fail;
		}
		if (ml->offset < 0 && !ml->offset_relative) {
			magic_warn(ml, "negative absolute offset");
			goto fail;
		}
		goto done;
	}
	s++;

	if (*s == '&') {
		ml->indirect_relative = 1;
		s++;
	}

	endptr = magic_strtoll(s, &ml->indirect_offset);
	if (endptr == NULL) {
		magic_warn(ml, "can't parse offset: %s", s);
		goto fail;
	}
	s = endptr;
	if (*s == ')')
		goto done;

	if (*s == '.') {
		s++;
		if (*s == '\0' || strchr("bslBSL", *s) == NULL) {
			magic_warn(ml, "unknown offset type: %c", *s);
			goto fail;
		}
		ml->indirect_type = *s;
		s++;
		if (*s == ')')
			goto done;
	}

	if (*s == '\0' || strchr("+-*", *s) == NULL) {
		magic_warn(ml, "unknown offset operator: %c", *s);
		goto fail;
	}
	ml->indirect_operator = *s;
	s++;
	if (*s == ')')
		goto done;

	if (*s == '(') {
		s++;
		endptr = magic_strtoll(s, &ml->indirect_operand);
		if (endptr == NULL || *endptr != ')') {
			magic_warn(ml, "missing closing bracket");
			goto fail;
		}
		if (*++endptr != ')') {
			magic_warn(ml, "missing closing bracket");
			goto fail;
		}
	} else {
		endptr = magic_strtoll(s, &ml->indirect_operand);
		if (endptr == NULL || *endptr != ')') {
			magic_warn(ml, "missing closing bracket");
			goto fail;
		}
	}

done:
	free(copy);
	return (0);

fail:
	free(copy);
	return (-1);
}

static int
magic_parse_type(struct magic_line *ml, char **line)
{
	char	*copy, *s, *cp, *endptr;

	while (isspace((u_char)**line))
		(*line)++;
	copy = s = cp = xmalloc(strlen(*line) + 1);
	while (**line != '\0' && !isspace((u_char)**line))
		*cp++ = *(*line)++;
	*cp = '\0';

	ml->type = MAGIC_TYPE_NONE;
	ml->type_string = xstrdup(s);

	ml->type_operator = ' ';
	ml->type_operand = 0;

	if (strncmp(s, "string", (sizeof "string") - 1) == 0) {
		ml->type = MAGIC_TYPE_STRING;
		magic_mark_text(ml, 0);
		goto done;
	}
	if (strncmp(s, "search", (sizeof "search") - 1) == 0) {
		ml->type = MAGIC_TYPE_SEARCH;
		goto done;
	}
	if (strncmp(s, "regex", (sizeof "regex") - 1) == 0) {
		ml->type = MAGIC_TYPE_REGEX;
		goto done;
	}

	cp = &s[strcspn(s, "-&")];
	if (*cp != '\0') {
		ml->type_operator = *cp;
		endptr = magic_strtoull(cp + 1, &ml->type_operand);
		if (endptr == NULL || *endptr != '\0') {
			magic_warn(ml, "can't parse operand: %s", cp + 1);
			goto fail;
		}
		*cp = '\0';
	}

	if (strcmp(s, "byte") == 0)
		ml->type = MAGIC_TYPE_BYTE;
	else if (strcmp(s, "short") == 0)
		ml->type = MAGIC_TYPE_SHORT;
	else if (strcmp(s, "long") == 0)
		ml->type = MAGIC_TYPE_LONG;
	else if (strcmp(s, "quad") == 0)
		ml->type = MAGIC_TYPE_QUAD;
	else if (strcmp(s, "ubyte") == 0)
		ml->type = MAGIC_TYPE_UBYTE;
	else if (strcmp(s, "ushort") == 0)
		ml->type = MAGIC_TYPE_USHORT;
	else if (strcmp(s, "ulong") == 0)
		ml->type = MAGIC_TYPE_ULONG;
	else if (strcmp(s, "uquad") == 0)
		ml->type = MAGIC_TYPE_UQUAD;
	else if (strcmp(s, "float") == 0)
		ml->type = MAGIC_TYPE_FLOAT;
	else if (strcmp(s, "double") == 0)
		ml->type = MAGIC_TYPE_DOUBLE;
	else if (strcmp(s, "pstring") == 0)
		ml->type = MAGIC_TYPE_PSTRING;
	else if (strcmp(s, "date") == 0)
		ml->type = MAGIC_TYPE_DATE;
	else if (strcmp(s, "qdate") == 0)
		ml->type = MAGIC_TYPE_QDATE;
	else if (strcmp(s, "ldate") == 0)
		ml->type = MAGIC_TYPE_LDATE;
	else if (strcmp(s, "qldate") == 0)
		ml->type = MAGIC_TYPE_QLDATE;
	else if (strcmp(s, "udate") == 0)
		ml->type = MAGIC_TYPE_UDATE;
	else if (strcmp(s, "uqdate") == 0)
		ml->type = MAGIC_TYPE_UQDATE;
	else if (strcmp(s, "uldate") == 0)
		ml->type = MAGIC_TYPE_ULDATE;
	else if (strcmp(s, "uqldate") == 0)
		ml->type = MAGIC_TYPE_UQLDATE;
	else if (strcmp(s, "beshort") == 0)
		ml->type = MAGIC_TYPE_BESHORT;
	else if (strcmp(s, "belong") == 0)
		ml->type = MAGIC_TYPE_BELONG;
	else if (strcmp(s, "bequad") == 0)
		ml->type = MAGIC_TYPE_BEQUAD;
	else if (strcmp(s, "ubeshort") == 0)
		ml->type = MAGIC_TYPE_UBESHORT;
	else if (strcmp(s, "ubelong") == 0)
		ml->type = MAGIC_TYPE_UBELONG;
	else if (strcmp(s, "ubequad") == 0)
		ml->type = MAGIC_TYPE_UBEQUAD;
	else if (strcmp(s, "befloat") == 0)
		ml->type = MAGIC_TYPE_BEFLOAT;
	else if (strcmp(s, "bedouble") == 0)
		ml->type = MAGIC_TYPE_BEDOUBLE;
	else if (strcmp(s, "bedate") == 0)
		ml->type = MAGIC_TYPE_BEDATE;
	else if (strcmp(s, "beqdate") == 0)
		ml->type = MAGIC_TYPE_BEQDATE;
	else if (strcmp(s, "beldate") == 0)
		ml->type = MAGIC_TYPE_BELDATE;
	else if (strcmp(s, "beqldate") == 0)
		ml->type = MAGIC_TYPE_BEQLDATE;
	else if (strcmp(s, "ubedate") == 0)
		ml->type = MAGIC_TYPE_UBEDATE;
	else if (strcmp(s, "ubeqdate") == 0)
		ml->type = MAGIC_TYPE_UBEQDATE;
	else if (strcmp(s, "ubeldate") == 0)
		ml->type = MAGIC_TYPE_UBELDATE;
	else if (strcmp(s, "ubeqldate") == 0)
		ml->type = MAGIC_TYPE_UBEQLDATE;
	else if (strcmp(s, "bestring16") == 0)
		ml->type = MAGIC_TYPE_BESTRING16;
	else if (strcmp(s, "leshort") == 0)
		ml->type = MAGIC_TYPE_LESHORT;
	else if (strcmp(s, "lelong") == 0)
		ml->type = MAGIC_TYPE_LELONG;
	else if (strcmp(s, "lequad") == 0)
		ml->type = MAGIC_TYPE_LEQUAD;
	else if (strcmp(s, "uleshort") == 0)
		ml->type = MAGIC_TYPE_ULESHORT;
	else if (strcmp(s, "ulelong") == 0)
		ml->type = MAGIC_TYPE_ULELONG;
	else if (strcmp(s, "ulequad") == 0)
		ml->type = MAGIC_TYPE_ULEQUAD;
	else if (strcmp(s, "lefloat") == 0)
		ml->type = MAGIC_TYPE_LEFLOAT;
	else if (strcmp(s, "ledouble") == 0)
		ml->type = MAGIC_TYPE_LEDOUBLE;
	else if (strcmp(s, "ledate") == 0)
		ml->type = MAGIC_TYPE_LEDATE;
	else if (strcmp(s, "leqdate") == 0)
		ml->type = MAGIC_TYPE_LEQDATE;
	else if (strcmp(s, "leldate") == 0)
		ml->type = MAGIC_TYPE_LELDATE;
	else if (strcmp(s, "leqldate") == 0)
		ml->type = MAGIC_TYPE_LEQLDATE;
	else if (strcmp(s, "uledate") == 0)
		ml->type = MAGIC_TYPE_ULEDATE;
	else if (strcmp(s, "uleqdate") == 0)
		ml->type = MAGIC_TYPE_ULEQDATE;
	else if (strcmp(s, "uleldate") == 0)
		ml->type = MAGIC_TYPE_ULELDATE;
	else if (strcmp(s, "uleqldate") == 0)
		ml->type = MAGIC_TYPE_ULEQLDATE;
	else if (strcmp(s, "lestring16") == 0)
		ml->type = MAGIC_TYPE_LESTRING16;
	else if (strcmp(s, "melong") == 0)
		ml->type = MAGIC_TYPE_MELONG;
	else if (strcmp(s, "medate") == 0)
		ml->type = MAGIC_TYPE_MEDATE;
	else if (strcmp(s, "meldate") == 0)
		ml->type = MAGIC_TYPE_MELDATE;
	else if (strcmp(s, "default") == 0)
		ml->type = MAGIC_TYPE_DEFAULT;
	else {
		magic_warn(ml, "unknown type: %s", s);
		goto fail;
	}
	magic_mark_text(ml, 0);

done:
	free(copy);
	return (0);

fail:
	free(copy);
	return (-1);
}

static int
magic_parse_value(struct magic_line *ml, char **line)
{
	char	*copy, *s, *cp, *endptr;
	size_t	 slen;
	uint64_t u;

	while (isspace((u_char)**line))
		(*line)++;

	ml->test_operator = '=';
	ml->test_not = 0;
	ml->test_string = NULL;
	ml->test_string_size = 0;
	ml->test_unsigned = 0;
	ml->test_signed = 0;

	if (**line == '\0')
		return (0);

	s = *line;
	if (s[0] == 'x' && (s[1] == '\0' || isspace((u_char)s[1]))) {
		(*line)++;
		ml->test_operator = 'x';
		return (0);
	}

	if (**line == '!') {
		ml->test_not = 1;
		(*line)++;
	}

	switch (ml->type) {
	case MAGIC_TYPE_STRING:
	case MAGIC_TYPE_PSTRING:
	case MAGIC_TYPE_SEARCH:
		if (**line == '>' || **line == '<' || **line == '=') {
			ml->test_operator = **line;
			(*line)++;
		}
		/* FALLTHROUGH */
	case MAGIC_TYPE_REGEX:
		copy = s = xmalloc(strlen(*line) + 1);
		if (magic_get_string(line, s, &slen) != 0) {
			magic_warn(ml, "can't parse string");
			goto fail;
		}
		ml->test_string_size = slen;
		ml->test_string = s;
		return (0); /* do not free */
	default:
		break;
	}

	while (isspace((u_char)**line))
		(*line)++;
	if ((*line)[0] == '<' && (*line)[1] == '=') {
		ml->test_operator = '[';
		(*line) += 2;
	} else if ((*line)[0] == '>' && (*line)[1] == '=') {
		ml->test_operator = ']';
		(*line) += 2;
	} else if (strchr("=<>&^", **line) != NULL) {
		ml->test_operator = **line;
		(*line)++;
	}

	while (isspace((u_char)**line))
		(*line)++;
	copy = cp = xmalloc(strlen(*line) + 1);
	while (**line != '\0' && !isspace((u_char)**line))
		*cp++ = *(*line)++;
	*cp = '\0';

	switch (ml->type) {
	case MAGIC_TYPE_FLOAT:
	case MAGIC_TYPE_DOUBLE:
	case MAGIC_TYPE_BEFLOAT:
	case MAGIC_TYPE_BEDOUBLE:
	case MAGIC_TYPE_LEFLOAT:
	case MAGIC_TYPE_LEDOUBLE:
		errno = 0;
		ml->test_double = strtod(copy, &endptr);
		if (errno == ERANGE)
			endptr = NULL;
		break;
	default:
		if (*ml->type_string == 'u')
			endptr = magic_strtoull(copy, &ml->test_unsigned);
		else {
			endptr = magic_strtoll(copy, &ml->test_signed);
			if (endptr == NULL || *endptr != '\0') {
				/*
				 * If we can't parse this as a signed number,
				 * try as unsigned instead.
				 */
				endptr = magic_strtoull(copy, &u);
				if (endptr != NULL && *endptr == '\0')
					ml->test_signed = (int64_t)u;
			}
		}
		break;
	}
	if (endptr == NULL || *endptr != '\0') {
		magic_warn(ml, "can't parse number: %s", copy);
		goto fail;
	}

	free(copy);
	return (0);

fail:
	free(copy);
	return (-1);
}

static void
magic_free_line(struct magic_line *ml)
{
	free((void *)ml->type_string);

	free((void *)ml->mimetype);
	free((void *)ml->result);

	free(ml);
}

int
magic_compare(struct magic_line *ml1, struct magic_line *ml2)
{
	if (ml1->strength < ml2->strength)
		return (1);
	if (ml1->strength > ml2->strength)
		return (-1);

	/*
	 * The original file depends on the (undefined!) qsort(3) behaviour
	 * when the strength is equal. This is impossible to reproduce with an
	 * RB tree so just use the line number and hope for the best.
	 */
	if (ml1->line < ml2->line)
		return (-1);
	if (ml1->line > ml2->line)
		return (1);

	return (0);
}
RB_GENERATE(magic_tree, magic_line, node, magic_compare);

static void
magic_set_mimetype(struct magic *m, u_int at, struct magic_line *ml, char *line)
{
	char	*mimetype, *cp;

	mimetype = line + (sizeof "!:mime") - 1;
	while (isspace((u_char)*mimetype))
		mimetype++;

	cp = strchr(mimetype, '#');
	if (cp != NULL)
		*cp = '\0';

	if (*mimetype != '\0') {
		cp = mimetype + strlen(mimetype) - 1;
		while (cp != mimetype && isspace((u_char)*cp))
			*cp-- = '\0';
	}

	cp = mimetype;
	while (*cp != '\0') {
		if (!isalnum((u_char)*cp) && strchr("/-.+", *cp) == NULL)
			break;
		cp++;
	}
	if (*mimetype == '\0' || *cp != '\0') {
		magic_warnm(m, at, "invalid MIME type: %s", mimetype);
		return;
	}
	if (ml == NULL) {
		magic_warnm(m, at, "stray MIME type: %s", mimetype);
		return;
	}
	ml->mimetype = xstrdup(mimetype);
}

struct magic *
magic_load(FILE *f, const char *path, int warnings)
{
	struct magic		*m;
	struct magic_line	*ml = NULL, *parent, *parent0;
	char			*line, *tmp;
	size_t			 size;
	u_int			 at, level, n, i;

	m = xcalloc(1, sizeof *m);
	m->path = xstrdup(path);
	m->warnings = warnings;
	RB_INIT(&m->tree);

	parent = NULL;
	parent0 = NULL;
	level = 0;

	at = 0;
	tmp = NULL;
	while ((line = fgetln(f, &size))) {
		if (line[size - 1] == '\n')
			line[size - 1] = '\0';
		else {
			tmp = xmalloc(size + 1);
			memcpy(tmp, line, size);
			tmp[size] = '\0';
			line = tmp;
		}
		at++;

		while (isspace((u_char)*line))
		    line++;
		if (*line == '\0' || *line == '#')
			continue;

		if (strncmp (line, "!:mime", (sizeof "!:mime") - 1) == 0) {
			magic_set_mimetype(m, at, ml, line);
			continue;
		}

		n = 0;
		for (; *line == '>'; line++)
			n++;

		ml = xcalloc(1, sizeof *ml);
		ml->root = m;
		ml->line = at;
		ml->type = MAGIC_TYPE_NONE;
		TAILQ_INIT(&ml->children);
		ml->text = 1;

		if (n == level + 1) {
			parent = parent0;
		} else if (n < level) {
			for (i = n; i < level && parent != NULL; i++)
				parent = parent->parent;
		} else if (n != level) {
			magic_warn(ml, "level skipped (%u->%u)", level, n);
			free(ml);
			continue;
		}
		ml->parent = parent;
		level = n;

		if (magic_parse_offset(ml, &line) != 0 ||
		    magic_parse_type(ml, &line) != 0 ||
		    magic_parse_value(ml, &line) != 0 ||
		    magic_set_result(ml, line) != 0) {
			magic_free_line(ml);
			ml = NULL;
			continue;
		}

		ml->strength = magic_get_strength(ml);
		if (ml->parent == NULL)
			RB_INSERT(magic_tree, &m->tree, ml);
		else
			TAILQ_INSERT_TAIL(&ml->parent->children, ml, entry);
		parent0 = ml;
	}
	free(tmp);

	fclose(f);
	return (m);
}
