/*	$OpenBSD: clparse.c,v 1.169 2018/11/04 16:32:11 krw Exp $	*/

/* Parser for dhclient config and lease files. */

/*
 * Copyright (c) 1997 The Internet Software Consortium.
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
#include <sys/types.h>

#include <net/if.h>
#include <net/if_arp.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <err.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "dhcp.h"
#include "dhcpd.h"
#include "dhctoken.h"
#include "log.h"

void	parse_conf_decl(FILE *, char *);
int	parse_hex_octets(FILE *, unsigned int *, uint8_t **);
int	parse_option_list(FILE *, int *, uint8_t *);
int	parse_interface(FILE *, char *);
int	parse_lease(FILE *, char *, struct client_lease **);
void	parse_lease_decl(FILE *, struct client_lease *, char *);
int	parse_option(FILE *, int *, struct option_data *);
int	parse_reject_statement(FILE *);

/*
 * conf-decls :==
 *	  <nil>
 *	| conf-decl
 *	| conf-decls conf-decl
 */
void
read_conf(char *name)
{
	struct option_data	*option;
	FILE			*cfile;
	int			 token;
	uint32_t		 expiry;

	new_parse(path_dhclient_conf);

	TAILQ_INIT(&config->reject_list);

	/* Set some defaults. */
	config->link_timeout = 10;	/* secs before going daemon w/o lease */
	config->timeout = 30;		/* secs to wait for an OFFER */
	config->select_interval = 0;	/* secs to wait for other OFFERs */
	config->reboot_timeout = 1;	/* secs before giving up on reboot */
	config->retry_interval = 1;	/* secs before asking for OFFER */
	config->backoff_cutoff = 10;	/* max secs between packet retries */
	config->initial_interval = 1;	/* secs before 1st retry */

	/*
	 * Set default lease length, which will determine default renewal
	 * and rebind times.
	 *
	 * XXX Thus applies to both BOOTP and DHCP leases.
	 *
	 * DHO_DHCP_LEASE_TIME (12 hours == 43200 seconds),
	 */
	option = &config->defaults[DHO_DHCP_LEASE_TIME];
	option->data = malloc(4);
	if (option->data == NULL)
		fatal("default lease length");

	config->default_actions[DHO_DHCP_LEASE_TIME] = ACTION_DEFAULT;
	option->len = 4;
	expiry = htonl(43200);
	memcpy(option->data, &expiry, 4);

	config->requested_options
	    [config->requested_option_count++] = DHO_SUBNET_MASK;
	config->requested_options
	    [config->requested_option_count++] = DHO_BROADCAST_ADDRESS;
	config->requested_options
	    [config->requested_option_count++] = DHO_TIME_OFFSET;
	/* RFC 3442 says CLASSLESS_STATIC_ROUTES must be before ROUTERS! */
	config->requested_options
	    [config->requested_option_count++] = DHO_CLASSLESS_STATIC_ROUTES;
	config->requested_options
	    [config->requested_option_count++] = DHO_ROUTERS;
	config->requested_options
	    [config->requested_option_count++] = DHO_DOMAIN_NAME;
	config->requested_options
	    [config->requested_option_count++] = DHO_DOMAIN_SEARCH;
	config->requested_options
	    [config->requested_option_count++] = DHO_DOMAIN_NAME_SERVERS;
	config->requested_options
	    [config->requested_option_count++] = DHO_HOST_NAME;
	config->requested_options
	    [config->requested_option_count++] = DHO_BOOTFILE_NAME;
	config->requested_options
	    [config->requested_option_count++] = DHO_TFTP_SERVER;

	if ((cfile = fopen(path_dhclient_conf, "r")) != NULL) {
		for (;;) {
			token = peek_token(NULL, cfile);
			if (token == EOF)
				break;
			parse_conf_decl(cfile, name);
		}
		fclose(cfile);
	}
}

/*
 * leases :==
 *	  <nil>
 *	| lease
 *	| leases lease
 */
void
read_lease_db(char *name, struct client_lease_tq *tq)
{
	struct client_lease	*lease, *lp, *nlp;
	FILE			*cfile;

	TAILQ_INIT(tq);

	if ((cfile = fopen(path_lease_db, "r")) == NULL)
		return;

	new_parse(path_lease_db);

	for (;;) {
		if (parse_lease(cfile, name, &lease) == 1) {
			/*
			 * The new lease will supersede a lease with the same ssid
			 * AND the same Client Identifier AND the same
			 * IP address.
			 */
			TAILQ_FOREACH_SAFE(lp, tq, next, nlp) {
				if (lp->ssid_len != lease->ssid_len)
					continue;
				if (memcmp(lp->ssid, lease->ssid, lp->ssid_len) != 0)
					continue;
				if ((lease->options[DHO_DHCP_CLIENT_IDENTIFIER].len != 0) &&
				    ((lp->options[DHO_DHCP_CLIENT_IDENTIFIER].len !=
				    lease->options[DHO_DHCP_CLIENT_IDENTIFIER].len) ||
				    memcmp(lp->options[DHO_DHCP_CLIENT_IDENTIFIER].data,
				    lease->options[DHO_DHCP_CLIENT_IDENTIFIER].data,
				    lp->options[DHO_DHCP_CLIENT_IDENTIFIER].len)))
					continue;
				if (lp->address.s_addr != lease->address.s_addr)
					continue;

				TAILQ_REMOVE(tq, lp, next);
				free_client_lease(lp);
			}

			TAILQ_INSERT_TAIL(tq, lease, next);
		}
		if (feof(cfile) != 0)
			break;
	}

	fclose(cfile);
}

/*
 * conf-decl :==
 *	  APPEND		option SEMI
 *	| BACKOFF_CUTOFF	number SEMI
 *	| DEFAULT		option SEMI
 *	| FILENAME		string SEMI
 *	| FIXED_ADDR		ip-address SEMI
 *	| IGNORE		option-name-list SEMI
 *	| INITIAL_INTERVAL	number SEMI
 *	| INTERFACE		interface
 *	| LINK_TIMEOUT		number SEMI
 *	| NEXT_SERVER		string SEMI
 *	| PREPEND		option SEMI
 *	| REBOOT		number SEMI
 *	| REJECT		ip-address SEMI
 *	| REQUEST		option-name-list SEMI
 *	| REQUIRE		option-name-list SEMI
 *	| RETRY			number SEMI
 *	| SELECT_TIMEOUT	number SEMI
 *	| SEND			option SEMI
 *	| SERVER_NAME		string SEMI
 *	| SUPERSEDE		option SEMI
 *	| TIMEOUT		number SEMI
 */
void
parse_conf_decl(FILE *cfile, char *name)
{
	uint8_t			 list[DHO_COUNT];
	char			*val;
	int			 i, count, token;
	uint32_t		 t;

	token = next_token(NULL, cfile);

	switch (token) {
	case TOK_APPEND:
		if (parse_option(cfile, &i, config->defaults) == 1) {
			config->default_actions[i] = ACTION_APPEND;
			parse_semi(cfile);
		}
		break;
	case TOK_BACKOFF_CUTOFF:
		if (parse_number(cfile, (unsigned char *)&t, 'L') == 1) {
			config->backoff_cutoff = ntohl(t);
			parse_semi(cfile);
		}
		break;
	case TOK_DEFAULT:
		if (parse_option(cfile, &i, config->defaults) == 1) {
			config->default_actions[i] = ACTION_DEFAULT;
			parse_semi(cfile);
		}
		break;
	case TOK_FILENAME:
		if (parse_string(cfile, NULL, &val) == 1) {
			free(config->filename);
			config->filename = val;
			parse_semi(cfile);
		}
		break;
	case TOK_FIXED_ADDR:
		if (parse_ip_addr(cfile, &config->address) == 1)
			parse_semi(cfile);
		break;
	case TOK_IGNORE:
		memset(list, 0, sizeof(list));
		count = 0;
		if (parse_option_list(cfile, &count, list) == 1) {
			enum actions *p = config->default_actions;
			if (count == 0) {
				for (i = 0; i < DHO_COUNT; i++)
					if (p[i] == ACTION_IGNORE)
						p[i] = ACTION_NONE;
			} else {
				for (i = 0; i < count; i++)
					p[list[i]] = ACTION_IGNORE;
			}
			parse_semi(cfile);
		}
		break;
	case TOK_INITIAL_INTERVAL:
		if (parse_number(cfile, (unsigned char *)&t, 'L') == 1) {
			config->initial_interval = ntohl(t);
			parse_semi(cfile);
		}
		break;
	case TOK_INTERFACE:
		if (parse_interface(cfile, name) == 1)
			;
		break;
	case TOK_LEASE:
		skip_to_semi(cfile);
		break;
	case TOK_LINK_TIMEOUT:
		if (parse_number(cfile, (unsigned char *)&t, 'L') == 1) {
			config->link_timeout = ntohl(t);
			parse_semi(cfile);
		}
		break;
	case TOK_NEXT_SERVER:
		if (parse_ip_addr(cfile, &config->next_server) == 1)
			parse_semi(cfile);
		break;
	case TOK_PREPEND:
		if (parse_option(cfile, &i, config->defaults) == 1) {
			config->default_actions[i] = ACTION_PREPEND;
			parse_semi(cfile);
		}
		break;
	case TOK_REBOOT:
		if (parse_number(cfile, (unsigned char *)&t, 'L') == 1) {
			config->reboot_timeout = ntohl(t);
			parse_semi(cfile);
		}
		break;
	case TOK_REJECT:
		if (parse_reject_statement(cfile) == 1)
			parse_semi(cfile);
		break;
	case TOK_REQUEST:
		if (parse_option_list(cfile, &config->requested_option_count,
		    config->requested_options) == 1)
			parse_semi(cfile);
		break;
	case TOK_REQUIRE:
		if (parse_option_list(cfile, &config->required_option_count,
		    config->required_options) == 1)
			parse_semi(cfile);
		break;
	case TOK_RETRY:
		if (parse_number(cfile, (unsigned char *)&t, 'L') == 1) {
			config->retry_interval = ntohl(t);
			parse_semi(cfile);
		}
		break;
	case TOK_SELECT_TIMEOUT:
		if (parse_number(cfile, (unsigned char *)&t, 'L') == 1) {
			config->select_interval = ntohl(t);
			parse_semi(cfile);
		}
		break;
	case TOK_SEND:
		if (parse_option(cfile, &i, config->send_options) == 1)
			parse_semi(cfile);
		break;
	case TOK_SERVER_NAME:
		if (parse_string(cfile, NULL, &val) == 1) {
			free(config->server_name);
			config->server_name = val;
			parse_semi(cfile);
		}
		break;
	case TOK_SUPERSEDE:
		if (parse_option(cfile, &i, config->defaults) == 1) {
			config->default_actions[i] = ACTION_SUPERSEDE;
			parse_semi(cfile);
		}
		break;
	case TOK_TIMEOUT:
		if (parse_number(cfile, (unsigned char *)&t, 'L') == 1) {
			config->timeout = ntohl(t);
			parse_semi(cfile);
		}
		break;
	default:
		parse_warn("expecting statement.");
		if (token != ';')
			skip_to_semi(cfile);
		break;
	}
}

int
parse_hex_octets(FILE *cfile, unsigned int *len, uint8_t **buf)
{
	static uint8_t	 	 octets[1500];
	char			*val, *ep;
	unsigned long		 ulval;
	unsigned int		 i;
	int			 token;

	i = 0;
	do {
		token = next_token(&val, cfile);

		errno = 0;
		ulval = strtoul(val, &ep, 16);
		if ((val[0] == '\0' || *ep != '\0') ||
		    (errno == ERANGE && ulval == ULONG_MAX) ||
		    (ulval > UINT8_MAX))
			break;
		octets[i++] = ulval;

		if (peek_token(NULL, cfile) == ';') {
			*buf = malloc(i);
			if (*buf == NULL)
				break;
			memcpy(*buf, octets, i);
			*len = i;
			return 1;
		}
		if (i == sizeof(octets))
			break;
		token = next_token(NULL, cfile);
	} while (token == ':');

	parse_warn("expecting colon delimited list of hex octets.");

	if (token != ';')
		skip_to_semi(cfile);

	return 0;
}

/*
 * option-list :==
 *	  <nil>
 *	| option-name
 *	| option-list COMMA option-name
 */
int
parse_option_list(FILE *cfile, int *count, uint8_t *optlist)
{
	uint8_t		 list[DHO_COUNT];
	unsigned int	 ix, j;
	int		 i;
	int		 token;
	char		*val;

	/* Empty list of option names is allowed, to re-init optlist. */
	if (peek_token(NULL, cfile) == ';') {
		memset(optlist, DHO_PAD, sizeof(list));
		*count = 0;
		return 1;
	}

	memset(list, 0, sizeof(list));
	memcpy(list, optlist, *count);
	ix = *count;
	do {
		/* Next token must be an option name. */
		token = next_token(&val, cfile);
		i = name_to_code(val);
		if (i == DHO_END)
			break;

		/* Avoid storing duplicate options in the list. */
		for (j = 0; j < ix && list[j] != i; j++)
			;
		if (j == ix)
			list[ix++] = i;

		if (peek_token(NULL, cfile) == ';') {
			memcpy(optlist, list, sizeof(list));
			*count = ix;
			return 1;
		}
		token = next_token(NULL, cfile);
	} while (token == ',');

	parse_warn("expecting comma delimited list of option names.");

	if (token != ';')
		skip_to_semi(cfile);

	return 0;
}

/*
 * interface :==
 *	string LBRACE conf-decls RBRACE
 */
int
parse_interface(FILE *cfile, char *name)
{
	char	*val;
	int	 token;

	token = next_token(&val, cfile);
	if (token != TOK_STRING) {
		parse_warn("expecting string.");
		if (token != ';')
			skip_to_semi(cfile);
		return 0;
	}

	if (strcmp(name, val) != 0) {
		skip_to_semi(cfile);
		return 1;
	}

	token = next_token(&val, cfile);
	if (token != '{') {
		parse_warn("expecting '{'.");
		if (token != ';')
			skip_to_semi(cfile);
		return 0;
	}

	for (;;) {
		token = peek_token(&val, cfile);
		if (token == EOF) {
			parse_warn("unterminated interface declaration.");
			return 0;
		}
		if (token == '}') {
			token = next_token(NULL, cfile);
			return 1;
		}
		parse_conf_decl(cfile, name);
	}

	return 0;
}

/*
 * lease :== LEASE RBRACE lease-decls LBRACE
 *
 * lease-decls :==
 *	  <nil>
 *	| lease-decl
 *	| lease-decls lease-decl
 */
int
parse_lease(FILE *cfile, char *name,
    struct client_lease **lp)
{
	struct client_lease	*lease;
	int			 token;

	token = next_token(NULL, cfile);
	if (token == EOF)
		return 0;
	if (token != TOK_LEASE) {
		parse_warn("expecting lease");
		if (token != ';')
			skip_to_semi(cfile);
		return 0;
	}

	token = next_token(NULL, cfile);
	if (token != '{') {
		parse_warn("expecting '{'.");
		if (token != ';')
			skip_to_semi(cfile);
		return 0;
	}

	lease = calloc(1, sizeof(*lease));
	if (lease == NULL)
		fatal("lease");

	for (;;) {
		token = peek_token(NULL, cfile);
		if (token == EOF) {
			parse_warn("unterminated lease.");
			free_client_lease(lease);
			break;
		}
		if (token == '}') {
			token = next_token(NULL, cfile);
			*lp = lease;
			return 1;
		}
		parse_lease_decl(cfile, lease, name);
	}

	return 0;
}

/*
 * lease-decl :==
 *	  BOOTP		SEMI
 *	| EPOCH		number SEMI
 *	| EXPIRE	<skip to semi> SEMI
 *	| FILENAME	string SEMI
 *	| FIXED_ADDR	ip_address SEMI
 *	| INTERFACE	string SEMI
 *	| NEXT_SERVER	string SEMI
 *	| OPTION	option SEMI
 *	| REBIND	<skip to semi> SEMI
 *	| RENEW		<skip to semi> SEMI
 *	| SERVER_NAME	string SEMI
 *	| SSID		string SEMI
 */
void
parse_lease_decl(FILE *cfile, struct client_lease *lease, char *name)
{
	char		*val;
	unsigned int	 len;
	int		 i, token;

	token = next_token(&val, cfile);

	switch (token) {
	case TOK_BOOTP:
		/* 'bootp' is just a comment. See BOOTP_LEASE(). */
		break;
	case TOK_EPOCH:
		if (parse_number(cfile, (unsigned char *)&lease->epoch, 't')
		    == 0)
			return;
		lease->epoch = betoh64(lease->epoch);
		break;
	case TOK_EXPIRE:
		/* 'expire' is just a comment. See 'epoch'. */
		skip_to_semi(cfile);
		return;
	case TOK_FILENAME:
		if (parse_string(cfile, NULL, &val) == 0)
			return;
		free(lease->filename);
		lease->filename = val;
		break;
	case TOK_FIXED_ADDR:
		if (parse_ip_addr(cfile, &lease->address) == 0)
			return;
		break;
	case TOK_INTERFACE:
		/* 'interface' is just a comment. */
		skip_to_semi(cfile);
		return;
	case TOK_NEXT_SERVER:
		if (parse_ip_addr(cfile, &lease->next_server) == 0)
			return;
		break;
	case TOK_OPTION:
		if (parse_option(cfile, &i, lease->options) == 0)
			return;
		break;
	case TOK_REBIND:
	case TOK_RENEW:
		/* 'rebind' & 'renew' are just comments. See 'epoch'. */
		skip_to_semi(cfile);
		return;
	case TOK_SERVER_NAME:
		if (parse_string(cfile, NULL, &val) == 0)
			return;
		free(lease->server_name);
		lease->server_name = val;
		break;
	case TOK_SSID:
		if (parse_string(cfile, &len, &val) == 0)
			return;
		if (len > sizeof(lease->ssid)) {
			free(val);
			parse_warn("ssid > 32 bytes");
			skip_to_semi(cfile);
			return;
		}
		memset(lease->ssid, 0, sizeof(lease->ssid));
		memcpy(lease->ssid, val, len);
		free(val);
		lease->ssid_len = len;
		break;
	default:
		parse_warn("expecting lease declaration.");
		if (token != ';')
			skip_to_semi(cfile);
		return;
	}

	parse_semi(cfile);
 }

/*
 * option :==
 *	option-name option-value
 *
 * option-value :==
 *	  text
 *	| hex-octets
 *	| signed-32
 *	| unsigned-32
 *	| unsigned-16
 *	| unsigned-8
 *	| flag
 *	| ip-address
 *	| ip-address-array
 *	| ip-address-pair-array
 *	| uint16-array
 *	| cidr-ip-address-array
 */
int
parse_option(FILE *cfile, int *code, struct option_data *options)
{
	uint8_t			 hunkbuf[1024], cidr[5], buf[4];
	struct in_addr		 ip_addr;
	uint8_t			*dp;
	char			*fmt, *val;
	unsigned int		 hunkix = 0;
	int			 i, freedp, len, token;
	int			 nul_term = 0;

	token = next_token(&val, cfile);
	i = name_to_code(val);
	if (i == DHO_END) {
		parse_warn("expecting option name.");
		skip_to_semi(cfile);
		return 0;
	}

	/* Parse the option data. */
	do {
		for (fmt = code_to_format(i); *fmt != '\0'; fmt++) {
			if (*fmt == 'A')
				break;
			freedp = 0;
			switch (*fmt) {
			case 'X':
				if (peek_token(NULL, cfile) == TOK_STRING) {
					if (parse_string(cfile, &len,
					    (char **)&dp) == 0)
						return 0;
				} else if (parse_hex_octets(cfile, &len, &dp)
				    == 0)
					return 0;
				freedp = 1;
				break;
			case 't': /* Text string. */
				if (parse_string(cfile, &len, (char **)&dp)
				    == 0)
					return 0;
				freedp = 1;
				break;
			case 'I': /* IP address. */
				if (parse_ip_addr(cfile, &ip_addr) == 0)
					return 0;
				len = sizeof(ip_addr);
				dp = (uint8_t *)&ip_addr;
				break;
			case 'l':	/* Signed 32-bit integer. */
				if (parse_number(cfile, buf, 'l') == 0)
					return 0;
				len = 4;
				dp = buf;
				break;
			case 'L':	/* Unsigned 32-bit integer. */
				if (parse_number(cfile, buf, 'L') == 0)
					return 0;
				len = 4;
				dp = buf;
				break;
			case 'S':	/* Unsigned 16-bit integer. */
				if (parse_number(cfile, buf, 'S') == 0)
					return 0;
				len = 2;
				dp = buf;
				break;
			case 'B':	/* Unsigned 8-bit integer. */
				if (parse_number(cfile, buf, 'B') == 0)
					return 0;
				len = 1;
				dp = buf;
				break;
			case 'f': /* Boolean flag. */
				if (parse_boolean(cfile, buf) == 0)
					return 0;
				len = 1;
				dp = buf;
				break;
			case 'C':
				if (parse_cidr(cfile, cidr) == 0)
					return 0;
				len = 1 + (cidr[0] + 7) / 8;
				dp = cidr;
				break;
			default:
				log_warnx("%s: bad format %c in "
				    "parse_option_param", log_procname, *fmt);
				skip_to_semi(cfile);
				return 0;
			}
			if (dp != NULL && len > 0) {
				if (hunkix + len > sizeof(hunkbuf)) {
					if (freedp == 1)
						free(dp);
					parse_warn("option data buffer "
					    "overflow");
					skip_to_semi(cfile);
					return 0;
				}
				memcpy(&hunkbuf[hunkix], dp, len);
				hunkix += len;
				if (freedp == 1)
					free(dp);
			}
		}
		token = peek_token(NULL, cfile);
		if (*fmt == 'A' && token == ',')
			token = next_token(NULL, cfile);
	} while (*fmt == 'A' && token == ',');

	free(options[i].data);
	options[i].data = malloc(hunkix + nul_term);
	if (options[i].data == NULL)
		fatal("option data");
	memcpy(options[i].data, hunkbuf, hunkix + nul_term);
	options[i].len = hunkix;

	*code = i;

	return 1;
}

int
parse_reject_statement(FILE *cfile)
{
	struct in_addr		 addr;
	struct reject_elem	*elem;

	if (parse_ip_addr(cfile, &addr) == 0)
		return 0;

	TAILQ_FOREACH(elem, &config->reject_list, next) {
		if (elem->addr.s_addr == addr.s_addr)
			return 1;
	}

	elem = malloc(sizeof(*elem));
	if (elem == NULL)
		fatal("reject address");
	elem->addr = addr;
	TAILQ_INSERT_TAIL(&config->reject_list, elem, next);

	return 1;
}
