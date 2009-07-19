/*	$OpenBSD: clparse.c,v 1.36 2009/07/19 00:18:02 stevesk Exp $	*/

/* Parser for dhclient config and lease files... */

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

/*
 * client-conf-file :== client-declarations EOF
 * client-declarations :== <nil>
 *			 | client-declaration
 *			 | client-declarations client-declaration
 */
int
read_client_conf(void)
{
	FILE *cfile;
	char *val;
	int token;

	new_parse(path_dhclient_conf);

	/* Set some defaults... */
	config->link_timeout = 10;
	config->timeout = 60;
	config->select_interval = 0;
	config->reboot_timeout = 10;
	config->retry_interval = 300;
	config->backoff_cutoff = 15;
	config->initial_interval = 3;
	config->bootp_policy = ACCEPT;
	config->script_name = _PATH_DHCLIENT_SCRIPT;
	config->requested_options
	    [config->requested_option_count++] = DHO_SUBNET_MASK;
	config->requested_options
	    [config->requested_option_count++] = DHO_BROADCAST_ADDRESS;
	config->requested_options
	    [config->requested_option_count++] = DHO_TIME_OFFSET;
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
			token = peek_token(&val, cfile);
			if (token == EOF)
				break;
			parse_client_statement(cfile);
		} while (1);
		token = next_token(&val, cfile); /* Clear the peek buffer */
		fclose(cfile);
	}

	return (!warnings_occurred);
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
	char	*val;
	int	 token;

	new_parse(path_dhclient_db);

	/* Open the lease file.   If we can't open it, just return -
	   we can safely trust the server to remember our state. */
	if ((cfile = fopen(path_dhclient_db, "r")) == NULL)
		return;
	do {
		token = next_token(&val, cfile);
		if (token == EOF)
			break;
		if (token != TOK_LEASE) {
			warning("Corrupt lease file - possible data loss!");
			skip_to_semi(cfile);
			break;
		} else
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
 *	TOK_TIMEOUT number |
 *	TOK_RETRY number |
 *	TOK_SELECT_TIMEOUT number |
 *	TOK_REBOOT number |
 *	TOK_BACKOFF_CUTOFF number |
 *	TOK_INITIAL_INTERVAL number |
 *	TOK_SCRIPT string |
 *	interface-declaration |
 *	TOK_LEASE client-lease-statement |
 *	TOK_ALIAS client-lease-statement |
 *	TOK_REJECT reject-statement
 */
void
parse_client_statement(FILE *cfile)
{
	char *val;
	int token, code;

	switch (next_token(&val, cfile)) {
	case TOK_SEND:
		parse_option_decl(cfile, &config->send_options[0]);
		return;
	case TOK_DEFAULT:
		code = parse_option_decl(cfile, &config->defaults[0]);
		if (code != -1)
			config->default_actions[code] = ACTION_DEFAULT;
		return;
	case TOK_SUPERSEDE:
		code = parse_option_decl(cfile, &config->defaults[0]);
		if (code != -1)
			config->default_actions[code] = ACTION_SUPERSEDE;
		return;
	case TOK_APPEND:
		code = parse_option_decl(cfile, &config->defaults[0]);
		if (code != -1)
			config->default_actions[code] = ACTION_APPEND;
		return;
	case TOK_PREPEND:
		code = parse_option_decl(cfile, &config->defaults[0]);
		if (code != -1)
			config->default_actions[code] = ACTION_PREPEND;
		return;
	case TOK_MEDIA:
		parse_string_list(cfile, &config->media, 1);
		return;
	case TOK_HARDWARE:
		parse_hardware_param(cfile, &ifi->hw_address);
		return;
	case TOK_REQUEST:
		config->requested_option_count =
			parse_option_list(cfile, config->requested_options);
		return;
	case TOK_REQUIRE:
		memset(config->required_options, 0,
		    sizeof(config->required_options));
		parse_option_list(cfile, config->required_options);
		return;
	case TOK_LINK_TIMEOUT:
		parse_lease_time(cfile, &config->link_timeout);
		return;
	case TOK_TIMEOUT:
		parse_lease_time(cfile, &config->timeout);
		return;
	case TOK_RETRY:
		parse_lease_time(cfile, &config->retry_interval);
		return;
	case TOK_SELECT_TIMEOUT:
		parse_lease_time(cfile, &config->select_interval);
		return;
	case TOK_REBOOT:
		parse_lease_time(cfile, &config->reboot_timeout);
		return;
	case TOK_BACKOFF_CUTOFF:
		parse_lease_time(cfile, &config->backoff_cutoff);
		return;
	case TOK_INITIAL_INTERVAL:
		parse_lease_time(cfile, &config->initial_interval);
		return;
	case TOK_SCRIPT:
		config->script_name = parse_string(cfile);
		return;
	case TOK_INTERFACE:
		parse_interface_declaration(cfile);
		return;
	case TOK_LEASE:
		parse_client_lease_statement(cfile, 1);
		return;
	case TOK_ALIAS:
		parse_client_lease_statement(cfile, 2);
		return;
	case TOK_REJECT:
		parse_reject_statement(cfile);
		return;
	default:
		parse_warn("expecting a statement.");
		skip_to_semi(cfile);
		break;
	}
	token = next_token(&val, cfile);
	if (token != ';') {
		parse_warn("semicolon expected.");
		skip_to_semi(cfile);
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
		do {
			token = next_token(&val, cfile);
			if (token != TOK_NUMBER && token != TOK_NUMBER_OR_NAME) {
				parse_warn("expecting hexadecimal constant.");
				skip_to_semi(cfile);
				return (0);
			}
			convert_num(&buf[len], val, 16, 8);
			if (len++ > max) {
				parse_warn("hexadecimal constant too long.");
				skip_to_semi(cfile);
				return (0);
			}
			token = peek_token(&val, cfile);
			if (token == ':')
				token = next_token(&val, cfile);
		} while (token == ':');
		val = (char *)buf;
	} else if (token == TOK_STRING) {
		token = next_token(&val, cfile);
		len = strlen(val);
		if (len + 1 > max) {
			parse_warn("string constant too long.");
			skip_to_semi(cfile);
			return (0);
		}
		memcpy(buf, val, len + 1);
	} else {
		parse_warn("expecting string or hexadecimal data");
		skip_to_semi(cfile);
		return (0);
	}
	return (len);
}

/*
 * option-list :== option_name |
 *		   option_list COMMA option_name
 */
int
parse_option_list(FILE *cfile, u_int8_t *list)
{
	int	 ix, i;
	int	 token;
	char	*val;

	ix = 0;
	do {
		token = next_token(&val, cfile);
		if (!is_identifier(token)) {
			parse_warn("expected option name.");
			skip_to_semi(cfile);
			return (0);
		}
		for (i = 0; i < 256; i++)
			if (!strcasecmp(dhcp_options[i].name, val))
				break;

		if (i == 256) {
			parse_warn("%s: unexpected option name.", val);
			skip_to_semi(cfile);
			return (0);
		}
		list[ix++] = i;
		if (ix == 256) {
			parse_warn("%s: too many options.", val);
			skip_to_semi(cfile);
			return (0);
		}
		token = next_token(&val, cfile);
	} while (token == ',');
	if (token != ';') {
		parse_warn("expecting semicolon.");
		skip_to_semi(cfile);
		return (0);
	}
	return (ix);
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
	int			 token;
	char			*val;

	token = next_token(&val, cfile);
	if (token != '{') {
		parse_warn("expecting left brace.");
		skip_to_semi(cfile);
		return;
	}

	lease = malloc(sizeof(struct client_lease));
	if (!lease)
		error("no memory for lease.");
	memset(lease, 0, sizeof(*lease));
	lease->is_static = is_static;

	do {
		token = peek_token(&val, cfile);
		if (token == EOF) {
			parse_warn("unterminated lease declaration.");
			return;
		}
		if (token == '}')
			break;
		parse_client_lease_declaration(cfile, lease);
	} while (1);
	token = next_token(&val, cfile);

	/* If the lease declaration didn't include an interface
	 * declaration that we recognized, it's of no use to us.
	 */
	if (!ifi) {
		free_client_lease(lease);
		return;
	}

	/* If this is an alias lease, it doesn't need to be sorted in. */
	if (is_static == 2) {
		client->alias = lease;
		return;
	}

	/*
	 * The new lease may supersede a lease that's not the active
	 * lease but is still on the lease list, so scan the lease list
	 * looking for a lease with the same address, and if we find it,
	 * toss it.
	 */
	pl = NULL;
	for (lp = client->leases; lp; lp = lp->next) {
		if (addr_eq(lp->address, lease->address)) {
			if (pl)
				pl->next = lp->next;
			else
				client->leases = lp->next;
			free_client_lease(lp);
			break;
		} else
			pl = lp;
	}

	/*
	 * If this is a preloaded lease, just put it on the list of
	 * recorded leases - don't make it the active lease.
	 */
	if (is_static) {
		lease->next = client->leases;
		client->leases = lease;
		return;
	}

	/*
	 * The last lease in the lease file on a particular interface is
	 * the active lease for that interface.    Of course, we don't
	 * know what the last lease in the file is until we've parsed
	 * the whole file, so at this point, we assume that the lease we
	 * just parsed is the active lease for its interface.   If
	 * there's already an active lease for the interface, and this
	 * lease is for the same ip address, then we just toss the old
	 * active lease and replace it with this one.   If this lease is
	 * for a different address, then if the old active lease has
	 * expired, we dump it; if not, we put it on the list of leases
	 * for this interface which are still valid but no longer
	 * active.
	 */
	if (client->active) {
		if (client->active->expiry < cur_time)
			free_client_lease(client->active);
		else if (addr_eq(client->active->address, lease->address))
			free_client_lease(client->active);
		else {
			client->active->next = client->leases;
			client->leases = client->active;
		}
	}
	client->active = lease;

	/* Phew. */
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

	switch (next_token(&val, cfile)) {
	case TOK_BOOTP:
		lease->is_bootp = 1;
		break;
	case TOK_INTERFACE:
		token = next_token(&val, cfile);
		if (token != TOK_STRING) {
			parse_warn("expecting interface name (in quotes).");
			skip_to_semi(cfile);
			break;
		}
		if (strcmp(ifi->name, val) != 0) {
			parse_warn("wrong interface name. Expecting '%s'.",
			   ifi->name);
			skip_to_semi(cfile);
			break;
		}
		break;
	case TOK_FIXED_ADDR:
		if (!parse_ip_addr(cfile, &lease->address))
			return;
		break;
	case TOK_MEDIUM:
		parse_string_list(cfile, &lease->medium, 0);
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
		skip_to_semi(cfile);
		break;
	}
	token = next_token(&val, cfile);
	if (token != ';') {
		parse_warn("expecting semicolon.");
		skip_to_semi(cfile);
	}
}

int
parse_option_decl(FILE *cfile, struct option_data *options)
{
	char		*val;
	int		 token;
	u_int8_t	 buf[4];
	u_int8_t	 hunkbuf[1024];
	int		 hunkix = 0;
	char		*fmt;
	struct iaddr	 ip_addr;
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
		parse_warn("no option named %s", val);
		skip_to_semi(cfile);
		return (-1);
	}

	/* Parse the option data... */
	do {
		for (fmt = dhcp_options[code].format; *fmt; fmt++) {
			if (*fmt == 'A')
				break;
			switch (*fmt) {
			case 'X':
				len = parse_X(cfile, &hunkbuf[hunkix],
				    sizeof(hunkbuf) - hunkix);
				hunkix += len;
				break;
			case 't': /* Text string... */
				token = next_token(&val, cfile);
				if (token != TOK_STRING) {
					parse_warn("expecting string.");
					skip_to_semi(cfile);
					return (-1);
				}
				len = strlen(val);
				if (hunkix + len + 1 > sizeof(hunkbuf)) {
					parse_warn("option data buffer %s",
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
				len = ip_addr.len;
				dp = ip_addr.iabuf;
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
			case 'L':	/* Unsigned 32-bit integer... */
			case 'l':	/* Signed 32-bit integer... */
				token = next_token(&val, cfile);
				if (token != TOK_NUMBER) {
need_number:
					parse_warn("expecting number.");
					if (token != ';')
						skip_to_semi(cfile);
					return (-1);
				}
				convert_num(buf, val, 0, 32);
				len = 4;
				dp = buf;
				goto alloc;
			case 's':	/* Signed 16-bit integer. */
			case 'S':	/* Unsigned 16-bit integer. */
				token = next_token(&val, cfile);
				if (token != TOK_NUMBER)
					goto need_number;
				convert_num(buf, val, 0, 16);
				len = 2;
				dp = buf;
				goto alloc;
			case 'b':	/* Signed 8-bit integer. */
			case 'B':	/* Unsigned 8-bit integer. */
				token = next_token(&val, cfile);
				if (token != TOK_NUMBER)
					goto need_number;
				convert_num(buf, val, 0, 8);
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
			default:
				warning("Bad format %c in parse_option_param.",
				    *fmt);
				skip_to_semi(cfile);
				return (-1);
			}
		}
		token = next_token(&val, cfile);
	} while (*fmt == 'A' && token == ',');

	if (token != ';') {
		parse_warn("semicolon expected.");
		skip_to_semi(cfile);
		return (-1);
	}

	options[code].data = malloc(hunkix + nul_term);
	if (!options[code].data)
		error("out of memory allocating option data.");
	memcpy(options[code].data, hunkbuf, hunkix + nul_term);
	options[code].len = hunkix;
	return (code);
}

void
parse_string_list(FILE *cfile, struct string_list **lp, int multiple)
{
	int			 token;
	char			*val;
	struct string_list	*cur, *tmp;

	/* Find the last medium in the media list. */
	if (*lp)
		for (cur = *lp; cur->next; cur = cur->next)
			;	/* nothing */
	else
		cur = NULL;

	do {
		token = next_token(&val, cfile);
		if (token != TOK_STRING) {
			parse_warn("Expecting media options.");
			skip_to_semi(cfile);
			return;
		}

		tmp = malloc(sizeof(struct string_list) + strlen(val));
		if (tmp == NULL)
			error("no memory for string list entry.");
		strlcpy(tmp->string, val, strlen(val) + 1);
		tmp->next = NULL;

		/* Store this medium at the end of the media list. */
		if (cur)
			cur->next = tmp;
		else
			*lp = tmp;
		cur = tmp;

		token = next_token(&val, cfile);
	} while (multiple && token == ',');

	if (token != ';') {
		parse_warn("expecting semicolon.");
		skip_to_semi(cfile);
	}
}

void
parse_reject_statement(FILE *cfile)
{
	struct iaddrlist *list;
	struct iaddr addr;
	char *val;
	int token;

	do {
		if (!parse_ip_addr(cfile, &addr)) {
			parse_warn("expecting IP address.");
			skip_to_semi(cfile);
			return;
		}

		list = malloc(sizeof(struct iaddrlist));
		if (!list)
			error("no memory for reject list!");

		list->addr = addr;
		list->next = config->reject_list;
		config->reject_list = list;

		token = next_token(&val, cfile);
	} while (token == ',');

	if (token != ';') {
		parse_warn("expecting semicolon.");
		skip_to_semi(cfile);
	}
}
