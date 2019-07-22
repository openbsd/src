/*	$OpenBSD: parse.c,v 1.83 2019/07/22 17:20:06 krw Exp $	*/

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
		}
		token = next_token(NULL, cfile);
	} while (token != EOF);
}

int
parse_semi(FILE *cfile)
{
	int token;

	token = next_token(NULL, cfile);
	if (token == ';')
		return 1;

	parse_warn("expecting semicolon.");
	skip_to_semi(cfile);

	return 0;
}

int
parse_string(FILE *cfile, char **string)
{
	static char	 unvisbuf[1500];
	char		*val;
	int		 i, token;

	token = next_token(&val, cfile);
	if (token == TOK_STRING) {
		i = strnunvis(unvisbuf, val, sizeof(unvisbuf));
		if (i >= 0) {
			*string = strdup(unvisbuf);
			if (*string == NULL)
				fatal("strdup(unvisbuf)");
			return 1;
		}
	}

	parse_warn("expecting string.");

	if (token != ';')
		skip_to_semi(cfile);

	return 0;
}

/*
 * cidr :== ip-address "/" bit-count
 * ip-address :== NUMBER [ DOT NUMBER [ DOT NUMBER [ DOT NUMBER ] ] ]
 * bit-count :== 0..32
 */
int
parse_cidr(FILE *cfile, unsigned char *cidr)
{
	uint8_t		 buf[5];
	const char	*errstr;
	char		*val;
	long long	 numval;
	unsigned int	 i;
	int		 token;

	memset(buf, 0, sizeof(buf));
	i = 1;	/* Last four octets hold subnet, first octet the # of bits. */
	do {
		token = next_token(&val, cfile);
		if (i == 0)
			numval = strtonum(val, 0, 32, &errstr);
		else
			numval = strtonum(val, 0, UINT8_MAX, &errstr);
		if (errstr != NULL)
			break;
		buf[i++] = numval;
		if (i == 1) {
			memcpy(cidr, buf, sizeof(buf)); /* XXX Need cidr_t */
			return 1;
		}
		token = next_token(NULL, cfile);
		if (token == '/')
			i = 0;
		if (i == sizeof(buf))
			break;
	} while (token == '.' || token == '/');

	parse_warn("expecting IPv4 CIDR block.");

	if (token != ';')
		skip_to_semi(cfile);

	return 0;
}

int
parse_ip_addr(FILE *cfile, struct in_addr *addr)
{
	struct in_addr	 buf;
	const char	*errstr;
	char		*val;
	long long	 numval;
	unsigned int	 i;
	int		 token;

	i = 0;
	do {
		token = next_token(&val, cfile);
		numval = strtonum(val, 0, UINT8_MAX, &errstr);
		if (errstr != NULL)
			break;
		((uint8_t *)&buf)[i++] = numval;
		if (i == sizeof(buf)) {
			memcpy(addr, &buf, sizeof(*addr));
			return 1;
		}
		token = next_token(NULL, cfile);
	} while (token == '.');

	parse_warn("expecting IPv4 address.");

	if (token != ';')
		skip_to_semi(cfile);

	return 0;
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

	parse_warn("expecting boolean.");
	if (token != ';')
		skip_to_semi(cfile);

	return 0;
}

int
parse_number(FILE *cfile, long long *number, long long low, long long high)
{
	const char	*errstr;
	char		*val, *msg;
	int		 rslt, token;
	long long	 numval;

	token = next_token(&val, cfile);

	numval = strtonum(val, low, high, &errstr);
	if (errstr == NULL) {
		*number = numval;
		return 1;
	}

	rslt = asprintf(&msg, "expecting integer between %lld and %lld", low,
	    high);
	if (rslt != -1) {
		parse_warn(msg);
		free(msg);
	}

	if (token != ';')
		skip_to_semi(cfile);

	return 0;
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
		log_warnx("%s: %s^", log_procname, spaces);
	}
}
