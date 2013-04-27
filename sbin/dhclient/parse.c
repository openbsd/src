/*	$OpenBSD: parse.c,v 1.23 2013/04/27 17:54:24 krw Exp $	*/

/* Common parser code for dhcpd and dhclient. */

/*
 * Copyright (c) 1995, 1996, 1997, 1998 The Internet Software Consortium.
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

/*
 * Skip to the semicolon ending the current statement.   If we encounter
 * braces, the matching closing brace terminates the statement.   If we
 * encounter a right brace but haven't encountered a left brace, return
 * leaving the brace in the token buffer for the caller.   If we see a
 * semicolon and haven't seen a left brace, return.   This lets us skip
 * over:
 *
 *	statement;
 *	statement foo bar { }
 *	statement foo bar { statement { } }
 *	statement}
 *
 *	...et cetera.
 */
void
skip_to_semi(FILE *cfile)
{
	int		 token;
	int		 brace_count = 0;

	do {
		token = peek_token(NULL, cfile);
		if (token == '}') {
			if (brace_count) {
				token = next_token(NULL, cfile);
				if (!--brace_count)
					return;
			} else
				return;
		} else if (token == '{') {
			brace_count++;
		} else if (token == ';' && !brace_count) {
			token = next_token(NULL, cfile);
			return;
		} else if (token == '\n') {
			/*
			 * EOL only happens when parsing
			 * /etc/resolv.conf, and we treat it like a
			 * semicolon because the resolv.conf file is
			 * line-oriented.
			 */
			token = next_token(NULL, cfile);
			return;
		}
		token = next_token(NULL, cfile);
	} while (token != EOF);
}

int
parse_semi(FILE *cfile)
{
	int token;

	token = next_token(NULL, cfile);
	if (token != ';') {
		parse_warn("semicolon expected.");
		skip_to_semi(cfile);
		return (0);
	}
	return (1);
}

/*
 * string-parameter :== STRING SEMI
 */
char *
parse_string(FILE *cfile)
{
	char *val, *s;
	int token;

	token = next_token(&val, cfile);
	if (token != TOK_STRING) {
		parse_warn("filename must be a string");
		skip_to_semi(cfile);
		return (NULL);
	}
	s = strdup(val);
	if (!s)
		error("no memory for string %s.", val);

	if (!parse_semi(cfile)) {
		free(s);
		return (NULL);
	}
	return (s);
}

int
parse_ip_addr(FILE *cfile, struct in_addr *addr)
{
	return (parse_numeric_aggregate(cfile, (char *)addr, 4, '.', 10));
}

/*
 * hardware-parameter :== HARDWARE ETHERNET csns SEMI
 * csns :== NUMBER | csns COLON NUMBER
 */
void
parse_hardware_param(FILE *cfile, struct hardware *hardware)
{
	int token;

	token = next_token(NULL, cfile);
	switch (token) {
	case TOK_ETHERNET:
		hardware->htype = HTYPE_ETHER;
		hardware->hlen = 6;
		break;
	case TOK_TOKEN_RING:
		hardware->htype = HTYPE_IEEE802;
		hardware->hlen = 6;
		break;
	case TOK_FDDI:
		hardware->htype = HTYPE_FDDI;
		hardware->hlen = 6;
		break;
	default:
		parse_warn("expecting a network hardware type");
		skip_to_semi(cfile);
		return;
	}

	if (parse_numeric_aggregate(cfile, hardware->haddr, hardware->hlen,
	    ':', 16) == 0)
		return;

	token = next_token(NULL, cfile);
	if (token != ';') {
		parse_warn("expecting semicolon.");
		skip_to_semi(cfile);
	}
}

/*
 * lease-time :== NUMBER SEMI
 */
void
parse_lease_time(FILE *cfile, time_t *timep)
{
	char *val;
	uint32_t value;
	int token;

	token = next_token(&val, cfile);
	if (token != TOK_NUMBER) {
		parse_warn("Expecting numeric lease time");
		skip_to_semi(cfile);
		return;
	}
	convert_num((unsigned char *)&value, val, 10, 32);
	/* Unswap the number - convert_num returns stuff in NBO. */
	*timep = ntohl(value);

	parse_semi(cfile);
}

/*
 * Parse a sequence of numbers separated by the token specified in separator.
 * Exactly max numbers are expected.
 */
int
parse_numeric_aggregate(FILE *cfile, unsigned char *buf, int max, int separator,
    int base)
{
	char *val;
	int token, count;

	if (buf == NULL || max == 0)
		error("no space for numeric aggregate");

	for (count = 0; count < max; count++, buf++) {
		if (count && (peek_token(&val, cfile) == separator))
			token = next_token(&val, cfile);

		token = next_token(&val, cfile);

		if (token == TOK_NUMBER || (base == 16 && token == TOK_NUMBER_OR_NAME))
			/* XXX Need to check if conversion was successful. */
			convert_num(buf, val, base, 8);
		else
			break;
	}

	if (count < max) {
		parse_warn("numeric aggregate too short.");
		return (0);
	}

	return (1);
}

void
convert_num(unsigned char *buf, char *str, int base, int size)
{
	int negative = 0, tval, max;
	u_int32_t val = 0;
	char *ptr = str;

	if (*ptr == '-') {
		negative = 1;
		ptr++;
	}

	/* If base wasn't specified, figure it out from the data. */
	if (!base) {
		if (ptr[0] == '0') {
			if (ptr[1] == 'x') {
				base = 16;
				ptr += 2;
			} else if (isascii(ptr[1]) && isdigit(ptr[1])) {
				base = 8;
				ptr += 1;
			} else
				base = 10;
		} else
			base = 10;
	}

	do {
		tval = *ptr++;
		/* XXX assumes ASCII... */
		if (tval >= 'a')
			tval = tval - 'a' + 10;
		else if (tval >= 'A')
			tval = tval - 'A' + 10;
		else if (tval >= '0')
			tval -= '0';
		else {
			warning("Bogus number: %s.", str);
			break;
		}
		if (tval >= base) {
			warning("Bogus number: %s: digit %d not in base %d",
			    str, tval, base);
			break;
		}
		val = val * base + tval;
	} while (*ptr);

	if (negative)
		max = (1 << (size - 1));
	else
		max = (1 << (size - 1)) + ((1 << (size - 1)) - 1);
	if (val > max) {
		switch (base) {
		case 8:
			warning("value %s%o exceeds max (%d) for precision.",
			    negative ? "-" : "", val, max);
			break;
		case 16:
			warning("value %s%x exceeds max (%d) for precision.",
			    negative ? "-" : "", val, max);
			break;
		default:
			warning("value %s%u exceeds max (%d) for precision.",
			    negative ? "-" : "", val, max);
			break;
		}
	}

	if (negative)
		switch (size) {
		case 8:
			*buf = -(unsigned long)val;
			break;
		case 16:
			putShort(buf, -(unsigned long)val);
			break;
		case 32:
			putLong(buf, -(unsigned long)val);
			break;
		default:
			warning("Unexpected integer size: %d", size);
			break;
		}
	else
		switch (size) {
		case 8:
			*buf = (u_int8_t)val;
			break;
		case 16:
			putUShort(buf, (u_int16_t)val);
			break;
		case 32:
			putULong(buf, val);
			break;
		default:
			warning("Unexpected integer size: %d", size);
			break;
		}
}

/*
 * date :== NUMBER NUMBER SLASH NUMBER SLASH NUMBER
 *		NUMBER COLON NUMBER COLON NUMBER UTC SEMI
 *
 * Dates are always in UTC; first number is day of week; next is
 * year/month/day; next is hours:minutes:seconds on a 24-hour
 * clock.
 */
time_t
parse_date(FILE *cfile)
{
	struct tm tm;
	char timestr[26]; /* "w yyyy/mm/dd hh:mm:ss UTC" */
	char *val, *p;
	size_t n;
	time_t guess;
	int token;

	memset(timestr, 0, sizeof(timestr));

	do {
		token = peek_token(NULL, cfile);
		switch (token) {
		case TOK_NAME:
		case TOK_NUMBER:
		case '/':
		case ':':
			token = next_token(&val, cfile);
			n = strlcat(timestr, val, sizeof(timestr));
			if (n >= sizeof(timestr)) {
				/* XXX Will break after year 9999! */
				parse_warn("time string too long");
				skip_to_semi(cfile);
				return (0);
			}
			break;
		case';':
			break;
		default:
			parse_warn("invalid time string");
			skip_to_semi(cfile);
			return (0);
		}
	} while (token != ';');

	parse_semi(cfile);

	memset(&tm, 0, sizeof(tm));	/* 'cuz strptime ignores tm_isdt. */
	p = strptime(timestr, DB_TIMEFMT, &tm);
	if (p == NULL || *p != '\0') {
		p = strptime(timestr, OLD_DB_TIMEFMT, &tm);
		if (p == NULL || *p != '\0') {
			p = strptime(timestr, BAD_DB_TIMEFMT, &tm);
			if (p == NULL || *p != '\0') {
				parse_warn("unparseable time string");
				return (0);
			}
		}
	}

	guess = timegm(&tm);
	if (guess == -1) {
		parse_warn("time could not be represented");
		return (0);
	}

	return (guess);
}
