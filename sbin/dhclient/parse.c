/*	$OpenBSD: parse.c,v 1.39 2015/05/18 17:51:21 krw Exp $	*/

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

#include <stdint.h>

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
				if (!--brace_count) {
					token = next_token(NULL, cfile);
					return;
				}
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
		parse_warn("expecting semicolon.");
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
		if (token != ';')
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

/* cidr :== ip-address "/" bit-count
 * ip-address :== NUMBER [ DOT NUMBER [ DOT NUMBER [ DOT NUMBER ] ] ]
 * bit-count :== 0..32
 */
int
parse_cidr(FILE *cfile, unsigned char *cidr)
{
	struct in_addr addr;
	int token;
	int len;

	token = '.';
	len = 0;
	for (token = '.'; token == '.'; token = next_token(NULL, cfile)) {
		if (!parse_decimal(cfile, cidr + 1 + len, 'B'))
			break;
		if (++len == sizeof(addr)) {
			token = next_token(NULL, cfile);
			break;
		}
	}

	if (!len) {
		parse_warn("expecting CIDR subnet.");
		skip_to_semi(cfile);
		return (0);
	} else if (token != '/') {
		parse_warn("expecting '/'.");
		skip_to_semi(cfile);
		return (0);
	} else if (!parse_decimal(cfile, cidr, 'B') || *cidr > 32) {
		parse_warn("Expecting CIDR prefix length.");
		skip_to_semi(cfile);
		return (0);
	}

	return (1);
}

int
parse_ip_addr(FILE *cfile, struct in_addr *addr)
{
	struct in_addr buf;
	int len, token;

	token = '.';
	len = 0;
	for (token = '.'; token == '.'; token = next_token(NULL, cfile)) {
		if (!parse_decimal(cfile, (unsigned char *)&buf + len, 'B'))
			break;
		if (++len == sizeof(buf))
			break;
	}

	if (len == 4) {
		memcpy(addr, &buf, sizeof(*addr));
		return (1);
	} else if (token != '.') {
		parse_warn("expecting '.'.");
		skip_to_semi(cfile);
		return (0);
	} else {
		parse_warn("expecting decimal octet.");
		skip_to_semi(cfile);
		return (0);
	}
}

/*
 * ETHERNET :== 'ethernet' NUMBER:NUMBER:NUMBER:NUMBER:NUMBER:NUMBER
 */
void
parse_ethernet(FILE *cfile, struct ether_addr *hardware)
{
	struct ether_addr buf;
	int len, token;

	token = next_token(NULL, cfile);
	if (token != TOK_ETHERNET) {
		parse_warn("expecting 'ethernet'.");
		if (token != ';')
			skip_to_semi(cfile);
		return;
	}

	len = 0;
	for (token = ':'; token == ':'; token = next_token(NULL, cfile)) {
		if (!parse_hex(cfile, &buf.ether_addr_octet[len]))
			break;
		if (++len == sizeof(buf.ether_addr_octet))
			break;
	}

	if (len == 6) {
		if (parse_semi(cfile))
		    memcpy(hardware, &buf, sizeof(*hardware));
	} else if (token != ':') {
		parse_warn("expecting ':'.");
		skip_to_semi(cfile);
	} else {
		parse_warn("expecting hex octet.");
		skip_to_semi(cfile);
	}
}

/*
 * lease-time :== NUMBER SEMI
 */
void
parse_lease_time(FILE *cfile, time_t *timep)
{
	u_int32_t value;

	if (!parse_decimal(cfile, (char *)&value, 'L')) {
		parse_warn("expecting unsigned 32-bit decimal value.");
		skip_to_semi(cfile);
		return;
	}

	*timep = betoh32(value);

	parse_semi(cfile);
}

int
parse_decimal(FILE *cfile, unsigned char *buf, char fmt)
{
	char *val;
	const char *errstr;
	int bytes, token;
	long long numval, low, high;

	token = next_token(&val, cfile);

	switch (fmt) {
	case 'l':	/* Signed 32-bit integer. */
		low = INT32_MIN;
		high = INT32_MAX;
		bytes = 4;
		break;
	case 'L':	/* Unsigned 32-bit integer. */
		low = 0;
		high = UINT32_MAX;
		bytes = 4;
		break;
	case 'S':	/* Unsigned 16-bit integer. */
		low = 0;
		high = UINT16_MAX;
		bytes = 2;
		break;
	case 'B':	/* Unsigned 8-bit integer. */
		low = 0;
		high = UINT8_MAX;
		bytes = 1;
		break;
	default:
		return (0);
	}

	numval = strtonum(val, low, high, &errstr);
	if (errstr)
		return (0);

	numval = htobe64(numval);
	memcpy(buf, (char *)&numval + (sizeof(numval) - bytes), bytes);

	return (1);
}

int
parse_hex(FILE *cfile, unsigned char *buf)
{
	char *val, *ep;
	int token;
	unsigned long ulval;

	token = next_token(&val, cfile);

	errno = 0;
	ulval = strtoul(val, &ep, 16);
	if ((val[0] == '\0' || *ep != '\0') ||
	    (errno == ERANGE && ulval == ULONG_MAX) ||
	    (ulval > UINT8_MAX))
		return (0);

	buf[0] = ulval;

	return (1);
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
		case TOK_NUMBER_OR_NAME:
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
