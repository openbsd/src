/*	$OpenBSD: parse.c,v 1.64 2017/10/08 17:35:56 krw Exp $	*/

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

#include <sys/queue.h>
#include <sys/socket.h>

#include <net/if.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <vis.h>

#include "dhcp.h"
#include "dhcpd.h"
#include "dhctoken.h"
#include "log.h"

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
			if (brace_count > 0) {
				if (--brace_count == 0) {
					token = next_token(NULL, cfile);
					return;
				}
			} else
				return;
		} else if (token == '{') {
			brace_count++;
		} else if (token == ';' && brace_count == 0) {
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
		return 0;
	}
	return 1;
}

char *
parse_string(FILE *cfile, unsigned int *len)
{
	static char	 unvisbuf[1500];
	char		*val, *s;
	int		 i, token;

	token = next_token(&val, cfile);
	if (token != TOK_STRING) {
		parse_warn("expecting string.");
		if (token != ';')
			skip_to_semi(cfile);
		return NULL;
	}

	i = strnunvis(unvisbuf, val, sizeof(unvisbuf));
	if (i == -1) {
		parse_warn("could not unvis string");
		return NULL;
	}
	s = malloc(i+1);
	if (s == NULL)
		fatal("unvis string %s", val);
	memcpy(s, unvisbuf, i+1);	/* Copy the terminating NUL. */
	if (len != NULL)
		*len = i;

	return s;
}

/*
 * cidr :== ip-address "/" bit-count
 * ip-address :== NUMBER [ DOT NUMBER [ DOT NUMBER [ DOT NUMBER ] ] ]
 * bit-count :== 0..32
 */
int
parse_cidr(FILE *cfile, unsigned char *cidr)
{
	struct in_addr	 addr;
	int		 token;
	int		 len;

	token = '.';
	len = 0;
	for (token = '.'; token == '.'; token = next_token(NULL, cfile)) {
		if (parse_decimal(cfile, cidr + 1 + len, 'B') == 0)
			break;
		if (++len == sizeof(addr)) {
			token = next_token(NULL, cfile);
			break;
		}
	}

	if (len == 0) {
		parse_warn("expecting decimal value.");
		skip_to_semi(cfile);
		return 0;
	} else if (token != '/') {
		parse_warn("expecting '/'.");
		skip_to_semi(cfile);
		return 0;
	} else if (parse_decimal(cfile, cidr, 'B') == 0 || *cidr > 32) {
		parse_warn("expecting decimal value <= 32.");
		skip_to_semi(cfile);
		return 0;
	}

	return 1;
}

int
parse_ip_addr(FILE *cfile, struct in_addr *addr)
{
	struct in_addr	 buf;
	int		 len, token;

	token = '.';
	len = 0;
	for (token = '.'; token == '.'; token = next_token(NULL, cfile)) {
		if (parse_decimal(cfile, (unsigned char *)&buf + len, 'B') == 0)
			break;
		if (++len == sizeof(buf))
			break;
	}

	if (len == 4) {
		memcpy(addr, &buf, sizeof(*addr));
		return 1;
	} else if (token != '.') {
		parse_warn("expecting '.'.");
		skip_to_semi(cfile);
		return 0;
	} else {
		parse_warn("expecting decimal value.");
		skip_to_semi(cfile);
		return 0;
	}
}

/*
 * lease-time :== NUMBER SEMI
 */
void
parse_lease_time(FILE *cfile, time_t *timep)
{
	uint32_t	 value;

	if (parse_decimal(cfile, (char *)&value, 'L') == 0) {
		parse_warn("expecting unsigned 32-bit decimal value.");
		skip_to_semi(cfile);
		return;
	}

	*timep = betoh32(value);

	parse_semi(cfile);
}

int
parse_boolean(FILE *cfile, unsigned char *buf)
{
	char	*val;
	int	 token;

	token = next_token(&val, cfile);
	if (is_identifier(token) != 0) {
		if (strcasecmp(val, "true") == 0 ||
		    strcasecmp(val, "on") == 0) {
			buf[0] = 1;
			return 1;
		}
		if (strcasecmp(val, "false") == 0 ||
		    strcasecmp(val, "off") == 0) {
			buf[0] = 0;
			return 1;
		}
	}

	return 0;
}

int
parse_decimal(FILE *cfile, unsigned char *buf, char fmt)
{
	const char	*errstr;
	char		*val;
	int		 bytes, token;
	long long	 numval, low, high;

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
		return 0;
	}

	numval = strtonum(val, low, high, &errstr);
	if (errstr != NULL)
		return 0;

	numval = htobe64(numval);
	memcpy(buf, (char *)&numval + (sizeof(numval) - bytes), bytes);

	return 1;
}

int
parse_hex(FILE *cfile, unsigned char *buf)
{
	char		*val, *ep;
	unsigned long	 ulval;
	int		 token;

	token = next_token(&val, cfile);

	errno = 0;
	ulval = strtoul(val, &ep, 16);
	if ((val[0] == '\0' || *ep != '\0') ||
	    (errno == ERANGE && ulval == ULONG_MAX) ||
	    (ulval > UINT8_MAX))
		return 0;

	buf[0] = ulval;

	return 1;
}

/*
 * date :== NUMBER NUMBER SLASH NUMBER SLASH NUMBER
 *		NUMBER COLON NUMBER COLON NUMBER UTC SEMI
 *
 * Dates are always in UTC; first number is day of week; next is
 * year/month/day; next is hours:minutes:seconds on a 24-hour
 * clock.
 *
 * XXX Will break after year 9999!
 */
time_t
parse_date(FILE *cfile)
{
	char		 timestr[23]; /* "wyyyy/mm/dd hh:mm:ssUTC" */
	struct tm	 tm;
	char		*val, *p;
	size_t		 n;
	time_t		 guess;
	int		 token;

	memset(timestr, 0, sizeof(timestr));

	guess = -1;
	n = 0;
	do {
		token = next_token(&val, cfile);

		switch (token) {
		case EOF:
			n = sizeof(timestr);
			break;
		case';':
			memset(&tm, 0, sizeof(tm));	/* 'cuz strptime ignores tm_isdt. */
			p = strptime(timestr, DB_TIMEFMT, &tm);
			if (p != NULL && *p == '\0')
				guess = timegm(&tm);
			break;
		default:
			n = strlcat(timestr, val, sizeof(timestr));
			break;

		}
	} while (n < sizeof(timestr) && token != ';');

	if (guess == -1) {
		guess = 0;
		parse_warn("expecting UTC time.");
		if (token != ';')
			skip_to_semi(cfile);
	}

	return guess;
}

void
parse_warn(char *msg)
{
	static char	 spaces[81];
	unsigned int	 i;

	log_warnx("%s: %s line %d: %s", log_procname, tlname, lexline, msg);
	log_warnx("%s: %s", log_procname, token_line);
	if ((unsigned int)lexchar < sizeof(spaces)) {
		memset(spaces, 0, sizeof(spaces));
		for (i = 0; (int)i < lexchar - 1; i++) {
			if (token_line[i] == '\t')
				spaces[i] = '\t';
			else
				spaces[i] = ' ';
		}
	}
	log_warnx("%s: %s^", log_procname, spaces);
}
