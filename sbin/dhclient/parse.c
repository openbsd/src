/*	$OpenBSD: parse.c,v 1.19 2010/06/26 21:14:10 krw Exp $	*/

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
	char		*val;
	int		 brace_count = 0;

	do {
		token = peek_token(&val, cfile);
		if (token == '}') {
			if (brace_count) {
				token = next_token(&val, cfile);
				if (!--brace_count)
					return;
			} else
				return;
		} else if (token == '{') {
			brace_count++;
		} else if (token == ';' && !brace_count) {
			token = next_token(&val, cfile);
			return;
		} else if (token == '\n') {
			/*
			 * EOL only happens when parsing
			 * /etc/resolv.conf, and we treat it like a
			 * semicolon because the resolv.conf file is
			 * line-oriented.
			 */
			token = next_token(&val, cfile);
			return;
		}
		token = next_token(&val, cfile);
	} while (token != EOF);
}

int
parse_semi(FILE *cfile)
{
	int token;
	char *val;

	token = next_token(&val, cfile);
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
parse_ip_addr(FILE *cfile, struct iaddr *addr)
{
	addr->len = 4;
	return (parse_numeric_aggregate(cfile, addr->iabuf, addr->len, '.',
	    10));
}

/*
 * hardware-parameter :== HARDWARE ETHERNET csns SEMI
 * csns :== NUMBER | csns COLON NUMBER
 */
void
parse_hardware_param(FILE *cfile, struct hardware *hardware)
{
	int token;
	char *val;

	token = next_token(&val, cfile);
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

	token = next_token(&val, cfile);
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
	int token;

	token = next_token(&val, cfile);
	if (token != TOK_NUMBER) {
		parse_warn("Expecting numeric lease time");
		skip_to_semi(cfile);
		return;
	}
	convert_num((unsigned char *)timep, val, 10, 32);
	/* Unswap the number - convert_num returns stuff in NBO. */
	*timep = ntohl(*timep);	/* XXX */

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
 *		NUMBER COLON NUMBER COLON NUMBER SEMI
 *
 * Dates are always in GMT; first number is day of week; next is
 * year/month/day; next is hours:minutes:seconds on a 24-hour
 * clock.
 */
time_t
parse_date(FILE *cfile)
{
	static int months[11] = { 31, 59, 90, 120, 151, 181,
	    212, 243, 273, 304, 334 };
	int guess, token;
	struct tm tm;
	char *val;

	/* Day of week... */
	token = next_token(&val, cfile);
	if (token != TOK_NUMBER) {
		parse_warn("numeric day of week expected.");
		if (token != ';')
			skip_to_semi(cfile);
		return (0);
	}
	tm.tm_wday = atoi(val);

	/* Year... */
	token = next_token(&val, cfile);
	if (token != TOK_NUMBER) {
		parse_warn("numeric year expected.");
		if (token != ';')
			skip_to_semi(cfile);
		return (0);
	}
	tm.tm_year = atoi(val);
	if (tm.tm_year > 1900)
		tm.tm_year -= 1900;

	/* Slash separating year from month... */
	token = next_token(&val, cfile);
	if (token != '/') {
		parse_warn("expected slash separating year from month.");
		if (token != ';')
			skip_to_semi(cfile);
		return (0);
	}

	/* Month... */
	token = next_token(&val, cfile);
	if (token != TOK_NUMBER) {
		parse_warn("numeric month expected.");
		if (token != ';')
			skip_to_semi(cfile);
		return (0);
	}
	tm.tm_mon = atoi(val) - 1;

	/* Slash separating month from day... */
	token = next_token(&val, cfile);
	if (token != '/') {
		parse_warn("expected slash separating month from day.");
		if (token != ';')
			skip_to_semi(cfile);
		return (0);
	}

	/* Day... */
	token = next_token(&val, cfile);
	if (token != TOK_NUMBER) {
		parse_warn("numeric day of month expected.");
		if (token != ';')
			skip_to_semi(cfile);
		return (0);
	}
	tm.tm_mday = atoi(val);

	/* Hour... */
	token = next_token(&val, cfile);
	if (token != TOK_NUMBER) {
		parse_warn("numeric hour expected.");
		if (token != ';')
			skip_to_semi(cfile);
		return (0);
	}
	tm.tm_hour = atoi(val);

	/* Colon separating hour from minute... */
	token = next_token(&val, cfile);
	if (token != ':') {
		parse_warn("expected colon separating hour from minute.");
		if (token != ';')
			skip_to_semi(cfile);
		return (0);
	}

	/* Minute... */
	token = next_token(&val, cfile);
	if (token != TOK_NUMBER) {
		parse_warn("numeric minute expected.");
		if (token != ';')
			skip_to_semi(cfile);
		return (0);
	}
	tm.tm_min = atoi(val);

	/* Colon separating minute from second... */
	token = next_token(&val, cfile);
	if (token != ':') {
		parse_warn("expected colon separating minute from second.");
		if (token != ';')
			skip_to_semi(cfile);
		return (0);
	}

	/* Second... */
	token = next_token(&val, cfile);
	if (token != TOK_NUMBER) {
		parse_warn("numeric second expected.");
		if (token != ';')
			skip_to_semi(cfile);
		return (0);
	}
	tm.tm_sec = atoi(val);
	tm.tm_isdst = 0;

	/* XXX: We assume that mktime does not use tm_yday. */
	tm.tm_yday = 0;

	/* Make sure the date ends in a semicolon... */
	token = next_token(&val, cfile);
	if (token != ';') {
		parse_warn("semicolon expected.");
		skip_to_semi(cfile);
		return (0);
	}

	/* Guess the time value... */
	guess = ((((((365 * (tm.tm_year - 70) +	/* Days in years since '70 */
	    (tm.tm_year - 69) / 4 +	/* Leap days since '70 */
	    (tm.tm_mon			/* Days in months this year */
	    ? months[tm.tm_mon - 1] : 0) +
	    (tm.tm_mon > 1 &&		/* Leap day this year */
	    !((tm.tm_year - 72) & 3)) +
	    tm.tm_mday - 1) * 24) +	/* Day of month */
	    tm.tm_hour) * 60) + tm.tm_min) * 60) + tm.tm_sec;

	/*
	 * This guess could be wrong because of leap seconds or other
	 * weirdness we don't know about that the system does.   For
	 * now, we're just going to accept the guess, but at some point
	 * it might be nice to do a successive approximation here to get
	 * an exact value.   Even if the error is small, if the server
	 * is restarted frequently (and thus the lease database is
	 * reread), the error could accumulate into something
	 * significant.
	 */
	return (guess);
}
