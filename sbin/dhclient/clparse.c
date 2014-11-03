/*	$OpenBSD: clparse.c,v 1.90 2014/11/03 22:06:39 krw Exp $	*/

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

#include "dhcpd.h"
#include "dhctoken.h"

void parse_client_statement(FILE *);
int parse_X(FILE *, u_int8_t *, int);
int parse_option_list(FILE *, u_int8_t *, size_t);
void parse_interface_declaration(FILE *);
void parse_client_lease_statement(FILE *, int);
void parse_client_lease_declaration(FILE *, struct client_lease *);
int parse_option_decl(FILE *, struct option_data *);
void parse_reject_statement(FILE *);

/*
 * client-conf-file :== client-declarations EOF
 * client-declarations :== <nil>
 *			 | client-declaration
 *			 | client-declarations client-declaration
 */
void
read_client_conf(void)
{
	FILE *cfile;
	int token;

	new_parse(path_dhclient_conf);

	/* Set some defaults. */
	config->link_timeout = 10;
	config->timeout = 60;
	config->select_interval = 0;
	config->reboot_timeout = 10;
	config->retry_interval = 300;
	config->backoff_cutoff = 15;
	config->initial_interval = 3;
	config->bootp_policy = ACCEPT;
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
	    [config->requested_option_count++] = DHO_DOMAIN_NAME_SERVERS;
	config->requested_options
	    [config->requested_option_count++] = DHO_HOST_NAME;

	if ((cfile = fopen(path_dhclient_conf, "r")) != NULL) {
		do {
			token = peek_token(NULL, cfile);
			if (token == EOF)
				break;
			parse_client_statement(cfile);
		} while (1);
		fclose(cfile);
	}
}

/*
 * lease-file :== client-lease-statements EOF
 * client-lease-statements :== <nil>
 *		     | client-lease-statements LEASE client-lease-statement
 */
void
read_client_leases(void)
{
	FILE	*cfile;
	int	 token;

	new_parse(path_dhclient_db);

	/* Open the lease file.   If we can't open it, just return -
	   we can safely trust the server to remember our state. */
	if ((cfile = fopen(path_dhclient_db, "r")) == NULL)
		return;
	do {
		token = next_token(NULL, cfile);
		if (token == EOF)
			break;
		if (token != TOK_LEASE) {
			warning("Corrupt lease file - possible data loss!");
			break;
		}
		parse_client_lease_statement(cfile, 0);
	} while (1);
	fclose(cfile);
}

/*
 * client-declaration :==
 *	TOK_SEND option-decl |
 *	TOK_DEFAULT option-decl |
 *	TOK_SUPERSEDE option-decl |
 *	TOK_APPEND option-decl |
 *	TOK_PREPEND option-decl |
 *	TOK_MEDIA string-list |
 *	hardware-declaration |
 *	TOK_REQUEST option-list |
 *	TOK_REQUIRE option-list |
 *	TOK_IGNORE option-list |
 *	TOK_TIMEOUT number |
 *	TOK_RETRY number |
 *	TOK_SELECT_TIMEOUT number |
 *	TOK_REBOOT number |
 *	TOK_BACKOFF_CUTOFF number |
 *	TOK_INITIAL_INTERVAL number |
 *	interface-declaration |
 *	TOK_LEASE client-lease-statement |
 *	TOK_ALIAS client-lease-statement |
 *	TOK_REJECT reject-statement
 */
void
parse_client_statement(FILE *cfile)
{
	u_int8_t optlist[256];
	char *string;
	int code, count, token;

	token = next_token(NULL, cfile);

	switch (token) {
	case TOK_SEND:
		parse_option_decl(cfile, &config->send_options[0]);
		break;
	case TOK_DEFAULT:
		code = parse_option_decl(cfile, &config->defaults[0]);
		if (code != -1)
			config->default_actions[code] = ACTION_DEFAULT;
		break;
	case TOK_SUPERSEDE:
		code = parse_option_decl(cfile, &config->defaults[0]);
		if (code != -1)
			config->default_actions[code] = ACTION_SUPERSEDE;
		break;
	case TOK_APPEND:
		code = parse_option_decl(cfile, &config->defaults[0]);
		if (code != -1)
			config->default_actions[code] = ACTION_APPEND;
		break;
	case TOK_PREPEND:
		code = parse_option_decl(cfile, &config->defaults[0]);
		if (code != -1)
			config->default_actions[code] = ACTION_PREPEND;
		break;
	case TOK_HARDWARE:
		parse_ethernet(cfile, &ifi->hw_address);
		break;
	case TOK_REQUEST:
		count = parse_option_list(cfile, optlist, sizeof(optlist));
		if (count != -1) {
			config->requested_option_count = count;
			memcpy(config->requested_options, optlist,
			    sizeof(config->requested_options));
		}
		break;
	case TOK_REQUIRE:
		count = parse_option_list(cfile, optlist, sizeof(optlist));
		if (count != -1) {
			config->required_option_count = count;
			memcpy(config->required_options, optlist,
			    sizeof(config->required_options));
		}
		break;
	case TOK_IGNORE:
		count = parse_option_list(cfile, optlist, sizeof(optlist));
		if (count != -1) {
			config->ignored_option_count = count;
			memcpy(config->ignored_options, optlist,
			    sizeof(config->ignored_options));
		}
		break;
	case TOK_LINK_TIMEOUT:
		parse_lease_time(cfile, &config->link_timeout);
		break;
	case TOK_TIMEOUT:
		parse_lease_time(cfile, &config->timeout);
		break;
	case TOK_RETRY:
		parse_lease_time(cfile, &config->retry_interval);
		break;
	case TOK_SELECT_TIMEOUT:
		parse_lease_time(cfile, &config->select_interval);
		break;
	case TOK_REBOOT:
		parse_lease_time(cfile, &config->reboot_timeout);
		break;
	case TOK_BACKOFF_CUTOFF:
		parse_lease_time(cfile, &config->backoff_cutoff);
		break;
	case TOK_INITIAL_INTERVAL:
		parse_lease_time(cfile, &config->initial_interval);
		break;
	case TOK_INTERFACE:
		parse_interface_declaration(cfile);
		break;
	case TOK_LEASE:
		parse_client_lease_statement(cfile, 1);
		break;
	case TOK_ALIAS:
	case TOK_MEDIA:
		/* Deprecated and ignored. */
		skip_to_semi(cfile);
		break;
	case TOK_REJECT:
		parse_reject_statement(cfile);
		break;
	case TOK_FILENAME:
		string = parse_string(cfile);
		if (config->filename)
			free(config->filename);
		config->filename = string;
		break;
	case TOK_SERVER_NAME:
		string = parse_string(cfile);
		if (config->server_name)
			free(config->server_name);
		config->server_name = string;
		break;
	case TOK_FIXED_ADDR:
		if (parse_ip_addr(cfile, &config->address))
			parse_semi(cfile);
		break;
	case TOK_NEXT_SERVER:
		if (parse_ip_addr(cfile, &config->next_server))
			parse_semi(cfile);
		break;
	default:
		parse_warn("expecting a statement.");
		if (token != ';')
			skip_to_semi(cfile);
		break;
	}
}

int
parse_X(FILE *cfile, u_int8_t *buf, int max)
{
	int	 token;
	char	*val;
	int	 len;

	token = peek_token(&val, cfile);
	if (token == TOK_NUMBER_OR_NAME || token == TOK_NUMBER) {
		len = 0;
		for (token = ':'; token == ':';
		     token = next_token(NULL, cfile)) {
			if (!parse_hex(cfile, &buf[len]))
				break;
			if (++len == max)
				break;
			if (peek_token(NULL, cfile) == ';')
				return (len);
		}
		if (token != ':') {
			parse_warn("expecting ':'.");
			skip_to_semi(cfile);
			return (-1);
		} else {
			parse_warn("expecting hex octet.");
			skip_to_semi(cfile);
			return (-1);
		}
	} else if (token == TOK_STRING) {
		token = next_token(&val, cfile);
		len = strlen(val);
		if (len + 1 > max) {
			parse_warn("string constant too long.");
			skip_to_semi(cfile);
			return (-1);
		}
		memcpy(buf, val, len + 1);
	} else {
		token = next_token(NULL, cfile);
		parse_warn("expecting string or hexadecimal data");
		if (token != ';')
			skip_to_semi(cfile);
		return (-1);
	}
	return (len);
}

/*
 * option-list :== option_name |
 *		   option_list COMMA option_name
 */
int
parse_option_list(FILE *cfile, u_int8_t *list, size_t sz)
{
	int	 ix, i, j;
	int	 token;
	char	*val;

	memset(list, DHO_PAD, sz);
	ix = 0;
	do {
		token = next_token(&val, cfile);
		if (token == ';' && ix == 0) {
			/* Empty list. */
			return (0);
		}
		if (!is_identifier(token)) {
			parse_warn("expecting option name.");
			goto syntaxerror;
		}
		/*
		 * 0 (DHO_PAD) and 255 (DHO_END) are not valid in option
		 * lists.  They are not really options and it makes no sense
		 * to request, require or ignore them.
		 */
		for (i = 1; i < DHO_END; i++)
			if (!strcasecmp(dhcp_options[i].name, val))
				break;

		if (i == DHO_END) {
			parse_warn("expecting option name.");
			goto syntaxerror;
		}
		if (ix == sz) {
			parse_warn("too many options.");
			goto syntaxerror;
		}
		/* Avoid storing duplicate options in the list. */
		for (j = 0; j < ix && list[j] != i; j++)
			;
		if (j == ix)
			list[ix++] = i;
		token = peek_token(NULL, cfile);
		if (token == ',')
			token = next_token(NULL, cfile);
	} while (token == ',');

	if (parse_semi(cfile))
		return (ix);

syntaxerror:
	if (token != ';')
		skip_to_semi(cfile);
	return (-1);
}

/*
 * interface-declaration :==
 *	INTERFACE string LBRACE client-declarations RBRACE
 */
void
parse_interface_declaration(FILE *cfile)
{
	char *val;
	int token;

	token = next_token(&val, cfile);
	if (token != TOK_STRING) {
		parse_warn("expecting interface name (in quotes).");
		if (token != ';')
			skip_to_semi(cfile);
		return;
	}

	if (strcmp(ifi->name, val) != 0) {
		skip_to_semi(cfile);
		return;
	}

	token = next_token(&val, cfile);
	if (token != '{') {
		parse_warn("expecting left brace.");
		if (token != ';')
			skip_to_semi(cfile);
		return;
	}

	do {
		token = peek_token(&val, cfile);
		if (token == EOF) {
			parse_warn("unterminated interface declaration.");
			return;
		}
		if (token == '}')
			break;
		parse_client_statement(cfile);
	} while (1);
	token = next_token(&val, cfile);
}

/*
 * client-lease-statement :==
 *	RBRACE client-lease-declarations LBRACE
 *
 *	client-lease-declarations :==
 *		<nil> |
 *		client-lease-declaration |
 *		client-lease-declarations client-lease-declaration
 */
void
parse_client_lease_statement(FILE *cfile, int is_static)
{
	struct client_lease	*lease, *lp, *pl;
	struct option_data	*opt1, *opt2;
	int			 token;

	token = next_token(NULL, cfile);
	if (token != '{') {
		parse_warn("expecting left brace.");
		if (token != ';')
			skip_to_semi(cfile);
		return;
	}

	lease = calloc(1, sizeof(struct client_lease));
	if (!lease)
		error("no memory for lease.");

	do {
		token = peek_token(NULL, cfile);
		if (token == EOF) {
			parse_warn("unterminated lease declaration.");
			free_client_lease(lease);
			return;
		}
		if (token == '}')
			break;
		parse_client_lease_declaration(cfile, lease);
	} while (1);
	token = next_token(NULL, cfile);

	/*
	 * If the new lease is for an obsolete client-identifier, toss it.
	 */
	opt1 = &lease->options[DHO_DHCP_CLIENT_IDENTIFIER];
	opt2 = &config->send_options[DHO_DHCP_CLIENT_IDENTIFIER];
	if (opt1->len && opt2->len && (opt1->len != opt2->len ||
	    memcmp(opt1->data, opt2->data, opt1->len))) {
		note("Obsolete client identifier (%s) in recorded lease",
		    pretty_print_option( DHO_DHCP_CLIENT_IDENTIFIER, opt1, 0));
		free_client_lease(lease);
		return;
	}

	/*
	 * The new lease will supersede a lease of the same type and for
	 * the same address.
	 */
	TAILQ_FOREACH_SAFE(lp, &client->leases, next, pl) {
		if (lp->address.s_addr == lease->address.s_addr &&
		    lp->is_static == is_static) {
			TAILQ_REMOVE(&client->leases, lp, next);
			lp->is_static = 0;	/* Else it won't be freed. */
			free_client_lease(lp);
		}
	}

	/*
	 * If the lease is marked as static before now it will leak on parse
	 * errors because free_client_lease() ignores attempts to free static
	 * leases.
	 */
	lease->is_static = is_static;
	if (is_static)
		TAILQ_INSERT_TAIL(&client->leases, lease, next);
	else
		TAILQ_INSERT_HEAD(&client->leases, lease,  next);
}

/*
 * client-lease-declaration :==
 *	BOOTP |
 *	INTERFACE string |
 *	FIXED_ADDR ip_address |
 *	FILENAME string |
 *	SERVER_NAME string |
 *	OPTION option-decl |
 *	RENEW time-decl |
 *	REBIND time-decl |
 *	EXPIRE time-decl
 */
void
parse_client_lease_declaration(FILE *cfile, struct client_lease *lease)
{
	char *val;
	int token;

	token = next_token(&val, cfile);

	switch (token) {
	case TOK_BOOTP:
		lease->is_bootp = 1;
		break;
	case TOK_INTERFACE:
		token = next_token(&val, cfile);
		if (token != TOK_STRING) {
			parse_warn("expecting interface name (in quotes).");
			if (token != ';')
				skip_to_semi(cfile);
			return;
		}
		if (strcmp(ifi->name, val) != 0) {
			if (lease->is_static == 0)
				parse_warn("wrong interface name.");
			skip_to_semi(cfile);
			return;
		}
		break;
	case TOK_FIXED_ADDR:
		if (!parse_ip_addr(cfile, &lease->address))
			return;
		break;
	case TOK_NEXT_SERVER:
		if (!parse_ip_addr(cfile, &lease->next_server))
			return;
		break;
	case TOK_MEDIUM:
		skip_to_semi(cfile);
		return;
	case TOK_FILENAME:
		lease->filename = parse_string(cfile);
		return;
	case TOK_SERVER_NAME:
		lease->server_name = parse_string(cfile);
		return;
	case TOK_RENEW:
		lease->renewal = parse_date(cfile);
		return;
	case TOK_REBIND:
		lease->rebind = parse_date(cfile);
		return;
	case TOK_EXPIRE:
		lease->expiry = parse_date(cfile);
		return;
	case TOK_OPTION:
		parse_option_decl(cfile, lease->options);
		return;
	default:
		parse_warn("expecting lease declaration.");
		if (token != ';')
			skip_to_semi(cfile);
		return;
	}

	parse_semi(cfile);
}

int
parse_option_decl(FILE *cfile, struct option_data *options)
{
	char		*val;
	int		 token;
	u_int8_t	 buf[4];
	u_int8_t	 cidr[5];
	u_int8_t	 hunkbuf[1024];
	int		 hunkix = 0;
	char		*fmt;
	struct in_addr	 ip_addr;
	u_int8_t	*dp;
	int		 len, code;
	int		 nul_term = 0;

	token = next_token(&val, cfile);
	if (!is_identifier(token)) {
		parse_warn("expecting identifier after option keyword.");
		if (token != ';')
			skip_to_semi(cfile);
		return (-1);
	}

	/* Look up the actual option info. */
	fmt = NULL;
	for (code = 0; code < 256; code++)
		if (strcmp(dhcp_options[code].name, val) == 0)
			break;

	if (code > 255) {
		parse_warn("unknown option name.");
		skip_to_semi(cfile);
		return (-1);
	}

	/* Parse the option data. */
	do {
		for (fmt = dhcp_options[code].format; *fmt; fmt++) {
			if (*fmt == 'A')
				break;
			switch (*fmt) {
			case 'X':
				len = parse_X(cfile, &hunkbuf[hunkix],
				    sizeof(hunkbuf) - hunkix);
				if (len == -1)
					return (-1);
				hunkix += len;
				break;
			case 't': /* Text string. */
				token = next_token(&val, cfile);
				if (token != TOK_STRING) {
					parse_warn("expecting string.");
					if (token != ';')
						skip_to_semi(cfile);
					return (-1);
				}
				len = strlen(val);
				if (hunkix + len + 1 > sizeof(hunkbuf)) {
					parse_warn("option data buffer "
					    "overflow");
					skip_to_semi(cfile);
					return (-1);
				}
				memcpy(&hunkbuf[hunkix], val, len + 1);
				nul_term = 1;
				hunkix += len;
				break;
			case 'I': /* IP address. */
				if (!parse_ip_addr(cfile, &ip_addr))
					return (-1);
				len = sizeof(ip_addr);
				dp = (u_int8_t *)&ip_addr;
alloc:
				if (hunkix + len > sizeof(hunkbuf)) {
					parse_warn("option data buffer "
					    "overflow");
					skip_to_semi(cfile);
					return (-1);
				}
				memcpy(&hunkbuf[hunkix], dp, len);
				hunkix += len;
				break;
			case 'l':	/* Signed 32-bit integer. */
				if (!parse_decimal(cfile, buf, *fmt)) {
					parse_warn("expecting signed 32-bit "
					    "integer.");
					skip_to_semi(cfile);
					return (-1);
				}
				len = 4;
				dp = buf;
				goto alloc;
			case 'L':	/* Unsigned 32-bit integer. */
				if (!parse_decimal(cfile, buf, *fmt)) {
					parse_warn("expecting unsigned 32-bit "
					    "integer.");
					skip_to_semi(cfile);
					return (-1);
				}
				len = 4;
				dp = buf;
				goto alloc;
			case 'S':	/* Unsigned 16-bit integer. */
				if (!parse_decimal(cfile, buf, *fmt)) {
					parse_warn("expecting unsigned 16-bit "
					    "integer.");
					skip_to_semi(cfile);
					return (-1);
				}
				len = 2;
				dp = buf;
				goto alloc;
			case 'B':	/* Unsigned 8-bit integer. */
				if (!parse_decimal(cfile, buf, *fmt)) {
					parse_warn("expecting unsigned 8-bit "
					    "integer.");
					skip_to_semi(cfile);
					return (-1);
				}
				len = 1;
				dp = buf;
				goto alloc;
			case 'f': /* Boolean flag. */
				token = next_token(&val, cfile);
				if (!is_identifier(token)) {
					parse_warn("expecting identifier.");
bad_flag:
					if (token != ';')
						skip_to_semi(cfile);
					return (-1);
				}
				if (!strcasecmp(val, "true") ||
				    !strcasecmp(val, "on"))
					buf[0] = 1;
				else if (!strcasecmp(val, "false") ||
				    !strcasecmp(val, "off"))
					buf[0] = 0;
				else {
					parse_warn("expecting boolean.");
					goto bad_flag;
				}
				len = 1;
				dp = buf;
				goto alloc;
			case 'C':
				if (!parse_cidr(cfile, cidr))
					return (-1);
				len = 1 + (cidr[0] + 7) / 8;
				dp = cidr;
				goto alloc;
			default:
				warning("Bad format %c in parse_option_param.",
				    *fmt);
				skip_to_semi(cfile);
				return (-1);
			}
		}
		token = peek_token(NULL, cfile);
		if (*fmt == 'A' && token == ',')
			token = next_token(NULL, cfile);
	} while (*fmt == 'A' && token == ',');

	if (!parse_semi(cfile))
		return (-1);

	options[code].data = malloc(hunkix + nul_term);
	if (!options[code].data)
		error("out of memory allocating option data.");
	memcpy(options[code].data, hunkbuf, hunkix + nul_term);
	options[code].len = hunkix;
	return (code);
}

void
parse_reject_statement(FILE *cfile)
{
	struct reject_elem *elem;
	struct in_addr addr;
	int token;

	do {
		if (!parse_ip_addr(cfile, &addr))
			return;

		elem = malloc(sizeof(struct reject_elem));
		if (!elem)
			error("no memory for reject address!");

		elem->addr = addr;
		TAILQ_INSERT_TAIL(&config->reject_list, elem, next);

		token = peek_token(NULL, cfile);
		if (token == ',')
			token = next_token(NULL, cfile);
	} while (token == ',');

	parse_semi(cfile);
}
