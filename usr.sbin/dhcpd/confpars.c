/*	$OpenBSD: confpars.c,v 1.11 2004/09/15 18:15:50 henning Exp $ */

/*
 * Copyright (c) 1995, 1996, 1997 The Internet Software Consortium.
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

static time_t parsed_time;

/* conf-file :== parameters declarations EOF
   parameters :== <nil> | parameter | parameters parameter
   declarations :== <nil> | declaration | declarations declaration */

int
readconf(void)
{
	FILE *cfile;
	char *val;
	int token;
	int declaration = 0;

	new_parse(path_dhcpd_conf);

	/* Set up the initial dhcp option universe. */
	initialize_universes();

	/* Set up the global defaults... */
	root_group.default_lease_time = 43200; /* 12 hours. */
	root_group.max_lease_time = 86400; /* 24 hours. */
	root_group.bootp_lease_cutoff = MAX_TIME;
	root_group.boot_unknown_clients = 1;
	root_group.allow_bootp = 1;
	root_group.allow_booting = 1;
	root_group.authoritative = 1;

	if ((cfile = fopen(path_dhcpd_conf, "r")) == NULL)
		error("Can't open %s: %m", path_dhcpd_conf);

	do {
		token = peek_token(&val, cfile);
		if (token == EOF)
			break;
		declaration = parse_statement(cfile, &root_group,
						 ROOT_GROUP,
						 NULL,
						 declaration);
	} while (1);
	token = next_token(&val, cfile); /* Clear the peek buffer */
	fclose(cfile);

	return !warnings_occurred;
}

/* lease-file :== lease-declarations EOF
   lease-statments :== <nil>
		   | lease-declaration
		   | lease-declarations lease-declaration
 */
void
read_leases(void)
{
	FILE *cfile;
	char *val;
	int token;

	new_parse(path_dhcpd_db);

	/* Open the lease file.   If we can't open it, fail.   The reason
	   for this is that although on initial startup, the absence of
	   a lease file is perfectly benign, if dhcpd has been running
	   and this file is absent, it means that dhcpd tried and failed
	   to rewrite the lease database.   If we proceed and the
	   problem which caused the rewrite to fail has been fixed, but no
	   human has corrected the database problem, then we are left
	   thinking that no leases have been assigned to anybody, which
	   could create severe network chaos. */
	if ((cfile = fopen(path_dhcpd_db, "r")) == NULL) {
		warn("Can't open lease database %s: %m -- %s",
		    path_dhcpd_db,
		    "check for failed database rewrite attempt!");
		warn("Please read the dhcpd.leases manual page if you.");
		error("don't know what to do about this.");
	}

	do {
		token = next_token(&val, cfile);
		if (token == EOF)
			break;
		if (token != LEASE) {
			warn("Corrupt lease file - possible data loss!");
			skip_to_semi(cfile);
		} else {
			struct lease *lease;
			lease = parse_lease_declaration(cfile);
			if (lease)
				enter_lease(lease);
			else
				parse_warn("possibly corrupt lease file");
		}

	} while (1);
	fclose(cfile);
}

/* statement :== parameter | declaration

   parameter :== timestamp
	     | DEFAULT_LEASE_TIME lease_time
	     | MAX_LEASE_TIME lease_time
	     | DYNAMIC_BOOTP_LEASE_CUTOFF date
	     | DYNAMIC_BOOTP_LEASE_LENGTH lease_time
	     | BOOT_UNKNOWN_CLIENTS boolean
	     | ONE_LEASE_PER_CLIENT boolean
	     | GET_LEASE_HOSTNAMES boolean
	     | USE_HOST_DECL_NAME boolean
	     | NEXT_SERVER ip-addr-or-hostname SEMI
	     | option_parameter
	     | SERVER-IDENTIFIER ip-addr-or-hostname SEMI
	     | FILENAME string-parameter
	     | SERVER_NAME string-parameter
	     | hardware-parameter
	     | fixed-address-parameter
	     | ALLOW allow-deny-keyword
	     | DENY allow-deny-keyword
	     | USE_LEASE_ADDR_FOR_DEFAULT_ROUTE boolean

   declaration :== host-declaration
		 | group-declaration
		 | shared-network-declaration
		 | subnet-declaration
		 | VENDOR_CLASS class-declaration
		 | USER_CLASS class-declaration
		 | RANGE address-range-declaration */

int parse_statement(cfile, group, type, host_decl, declaration)
	FILE *cfile;
	struct group *group;
	int type;
	struct host_decl *host_decl;
	int declaration;
{
	int token;
	char *val;
	struct shared_network *share;
	char *t, *n;
	struct tree *tree;
	struct tree_cache *cache;
	struct hardware hardware;

	switch (next_token(&val, cfile)) {
	case HOST:
		if (type != HOST_DECL)
			parse_host_declaration(cfile, group);
		else {
			parse_warn("host declarations not allowed here.");
			skip_to_semi(cfile);
		}
		return 1;

	case GROUP:
		if (type != HOST_DECL)
			parse_group_declaration(cfile, group);
		else {
			parse_warn("host declarations not allowed here.");
			skip_to_semi(cfile);
		}
		return 1;

	case TIMESTAMP:
		parsed_time = parse_timestamp(cfile);
		break;

	case SHARED_NETWORK:
		if (type == SHARED_NET_DECL ||
		    type == HOST_DECL ||
		    type == SUBNET_DECL) {
			parse_warn("shared-network parameters not %s.",
				    "allowed here");
			skip_to_semi(cfile);
			break;
		}

		parse_shared_net_declaration(cfile, group);
		return 1;

	case SUBNET:
		if (type == HOST_DECL || type == SUBNET_DECL) {
			parse_warn("subnet declarations not allowed here.");
			skip_to_semi(cfile);
			return 1;
		}

		/* If we're in a subnet declaration, just do the parse. */
		if (group->shared_network) {
			parse_subnet_declaration(cfile,
			    group->shared_network);
			break;
		}

		/* Otherwise, cons up a fake shared network structure
		   and populate it with the lone subnet... */

		share = new_shared_network("parse_statement");
		if (!share)
			error("No memory for shared subnet");
		share->group = clone_group(group, "parse_statement:subnet");
		share->group->shared_network = share;

		parse_subnet_declaration(cfile, share);

		/* share->subnets is the subnet we just parsed. */
		if (share->subnets) {
			share->interface =
				share->subnets->interface;

			/* Make the shared network name from network number. */
			n = piaddr(share->subnets->net);
			t = malloc(strlen(n) + 1);
			if (!t)
				error("no memory for subnet name");
			strlcpy(t, n, (strlen(n) + 1));
			share->name = t;

			/* Copy the authoritative parameter from the subnet,
			   since there is no opportunity to declare it here. */
			share->group->authoritative =
				share->subnets->group->authoritative;
			enter_shared_network(share);
		}
		return 1;

	case VENDOR_CLASS:
		parse_class_declaration(cfile, group, 0);
		return 1;

	case USER_CLASS:
		parse_class_declaration(cfile, group, 1);
		return 1;

	case DEFAULT_LEASE_TIME:
		parse_lease_time(cfile, &group->default_lease_time);
		break;

	case MAX_LEASE_TIME:
		parse_lease_time(cfile, &group->max_lease_time);
		break;

	case DYNAMIC_BOOTP_LEASE_CUTOFF:
		group->bootp_lease_cutoff = parse_date(cfile);
		break;

	case DYNAMIC_BOOTP_LEASE_LENGTH:
		parse_lease_time(cfile, &group->bootp_lease_length);
		break;

	case BOOT_UNKNOWN_CLIENTS:
		if (type == HOST_DECL)
			parse_warn("boot-unknown-clients not allowed here.");
		group->boot_unknown_clients = parse_boolean(cfile);
		break;

	case ONE_LEASE_PER_CLIENT:
		if (type == HOST_DECL)
			parse_warn("one-lease-per-client not allowed here.");
		group->one_lease_per_client = parse_boolean(cfile);
		break;

	case GET_LEASE_HOSTNAMES:
		if (type == HOST_DECL)
			parse_warn("get-lease-hostnames not allowed here.");
		group->get_lease_hostnames = parse_boolean(cfile);
		break;

	case ALWAYS_REPLY_RFC1048:
		group->always_reply_rfc1048 = parse_boolean(cfile);
		break;

	case USE_HOST_DECL_NAMES:
		if (type == HOST_DECL)
			parse_warn("use-host-decl-names not allowed here.");
		group->use_host_decl_names = parse_boolean(cfile);
		break;

	case USE_LEASE_ADDR_FOR_DEFAULT_ROUTE:
		group->use_lease_addr_for_default_route =
			parse_boolean(cfile);
		break;

	case TOKEN_NOT:
		token = next_token(&val, cfile);
		switch (token) {
		case AUTHORITATIVE:
			if (type == HOST_DECL)
			    parse_warn("authority makes no sense here.");
			group->authoritative = 0;
			parse_semi(cfile);
			break;
		default:
			parse_warn("expecting assertion");
			skip_to_semi(cfile);
			break;
		}
		break;

	case AUTHORITATIVE:
		if (type == HOST_DECL)
		    parse_warn("authority makes no sense here.");
		group->authoritative = 1;
		parse_semi(cfile);
		break;

	case NEXT_SERVER:
		tree = parse_ip_addr_or_hostname(cfile, 0);
		if (!tree)
			break;
		cache = tree_cache(tree);
		if (!tree_evaluate (cache))
			error("next-server is not known");
		group->next_server.len = 4;
		memcpy(group->next_server.iabuf,
			cache->value, group->next_server.len);
		parse_semi(cfile);
		break;

	case OPTION:
		parse_option_param(cfile, group);
		break;

	case SERVER_IDENTIFIER:
		tree = parse_ip_addr_or_hostname(cfile, 0);
		if (!tree)
			return declaration;
		group->options[DHO_DHCP_SERVER_IDENTIFIER] = tree_cache(tree);
		token = next_token(&val, cfile);
		break;

	case FILENAME:
		group->filename = parse_string(cfile);
		break;

	case SERVER_NAME:
		group->server_name = parse_string(cfile);
		break;

	case HARDWARE:
		parse_hardware_param(cfile, &hardware);
		if (host_decl)
			host_decl->interface = hardware;
		else
			parse_warn("hardware address parameter %s",
				    "not allowed here.");
		break;

	case FIXED_ADDR:
		cache = parse_fixed_addr_param(cfile);
		if (host_decl)
			host_decl->fixed_addr = cache;
		else
			parse_warn("fixed-address parameter not %s",
				    "allowed here.");
		break;

	case RANGE:
		if (type != SUBNET_DECL || !group->subnet) {
			parse_warn("range declaration not allowed here.");
			skip_to_semi(cfile);
			return declaration;
		}
		parse_address_range(cfile, group->subnet);
		return declaration;

	case ALLOW:
		parse_allow_deny(cfile, group, 1);
		break;

	case DENY:
		parse_allow_deny(cfile, group, 0);
		break;

	default:
		if (declaration)
			parse_warn("expecting a declaration.");
		else
			parse_warn("expecting a parameter or declaration.");
		skip_to_semi(cfile);
		return declaration;
	}

	if (declaration) {
		parse_warn("parameters not allowed after first declaration.");
		return 1;
	}

	return 0;
}

/* allow-deny-keyword :== BOOTP
			| BOOTING
			| DYNAMIC_BOOTP
			| UNKNOWN_CLIENTS */

void parse_allow_deny(cfile, group, flag)
	FILE *cfile;
	struct group *group;
	int flag;
{
	int token;
	char *val;

	token = next_token(&val, cfile);
	switch (token) {
	case BOOTP:
		group->allow_bootp = flag;
		break;

	case BOOTING:
		group->allow_booting = flag;
		break;

	case DYNAMIC_BOOTP:
		group->dynamic_bootp = flag;
		break;

	case UNKNOWN_CLIENTS:
		group->boot_unknown_clients = flag;
		break;

	default:
		parse_warn("expecting allow/deny key");
		skip_to_semi(cfile);
		return;
	}
	parse_semi(cfile);
}

/* boolean :== ON SEMI | OFF SEMI | TRUE SEMI | FALSE SEMI */

int parse_boolean(cfile)
	FILE *cfile;
{
	int token;
	char *val;
	int rv;

	token = next_token(&val, cfile);
	if (!strcasecmp (val, "true")
	    || !strcasecmp (val, "on"))
		rv = 1;
	else if (!strcasecmp (val, "false")
		 || !strcasecmp (val, "off"))
		rv = 0;
	else {
		parse_warn("boolean value (true/false/on/off) expected");
		skip_to_semi(cfile);
		return 0;
	}
	parse_semi(cfile);
	return rv;
}

/* Expect a left brace; if there isn't one, skip over the rest of the
   statement and return zero; otherwise, return 1. */

int
parse_lbrace(FILE *cfile)
{
	int token;
	char *val;

	token = next_token(&val, cfile);
	if (token != LBRACE) {
		parse_warn("expecting left brace.");
		skip_to_semi(cfile);
		return 0;
	}
	return 1;
}


/* host-declaration :== hostname RBRACE parameters declarations LBRACE */

void parse_host_declaration(cfile, group)
	FILE *cfile;
	struct group *group;
{
	char *val;
	int token;
	struct host_decl *host;
	char *name = parse_host_name(cfile);
	int declaration = 0;

	if (!name)
		return;

	host = (struct host_decl *)dmalloc(sizeof (struct host_decl),
	    "parse_host_declaration");
	if (!host)
		error("can't allocate host decl struct %s.", name);

	host->name = name;
	host->group = clone_group(group, "parse_host_declaration");

	if (!parse_lbrace(cfile))
		return;

	do {
		token = peek_token(&val, cfile);
		if (token == RBRACE) {
			token = next_token(&val, cfile);
			break;
		}
		if (token == EOF) {
			token = next_token(&val, cfile);
			parse_warn("unexpected end of file");
			break;
		}
		declaration = parse_statement(cfile, host->group,
		    HOST_DECL, host, declaration);
	} while (1);

	if (!host->group->options[DHO_HOST_NAME] &&
	    host->group->use_host_decl_names) {
		host->group->options[DHO_HOST_NAME] =
		    new_tree_cache("parse_host_declaration");
		if (!host->group->options[DHO_HOST_NAME])
			error("can't allocate a tree cache for hostname.");
		host->group->options[DHO_HOST_NAME]->len =
			strlen(name);
		host->group->options[DHO_HOST_NAME]->value =
			(unsigned char *)name;
		host->group->options[DHO_HOST_NAME]->buf_size =
			host->group->options[DHO_HOST_NAME]->len;
		host->group->options[DHO_HOST_NAME]->timeout =
			0xFFFFFFFF;
		host->group->options[DHO_HOST_NAME]->tree =
			NULL;
	}

	enter_host(host);
}

/* class-declaration :== STRING LBRACE parameters declarations RBRACE
*/

void parse_class_declaration(cfile, group, type)
	FILE *cfile;
	struct group *group;
	int type;
{
	char *val;
	int token;
	struct class *class;
	int declaration = 0;

	token = next_token(&val, cfile);
	if (token != STRING) {
		parse_warn("Expecting class name");
		skip_to_semi(cfile);
		return;
	}

	class = add_class (type, val);
	if (!class)
		error("No memory for class %s.", val);
	class->group = clone_group(group, "parse_class_declaration");

	if (!parse_lbrace(cfile))
		return;

	do {
		token = peek_token(&val, cfile);
		if (token == RBRACE) {
			token = next_token(&val, cfile);
			break;
		} else if (token == EOF) {
			token = next_token(&val, cfile);
			parse_warn("unexpected end of file");
			break;
		} else {
			declaration = parse_statement(cfile, class->group,
			    CLASS_DECL, NULL, declaration);
		}
	} while (1);
}

/* shared-network-declaration :==
			hostname LBRACE declarations parameters RBRACE */

void parse_shared_net_declaration(cfile, group)
	FILE *cfile;
	struct group *group;
{
	char *val;
	int token;
	struct shared_network *share;
	char *name;
	int declaration = 0;

	share = new_shared_network("parse_shared_net_declaration");
	if (!share)
		error("No memory for shared subnet");
	share->leases = NULL;
	share->last_lease = NULL;
	share->insertion_point = NULL;
	share->next = NULL;
	share->interface = NULL;
	share->group = clone_group(group, "parse_shared_net_declaration");
	share->group->shared_network = share;

	/* Get the name of the shared network... */
	token = peek_token(&val, cfile);
	if (token == STRING) {
		token = next_token(&val, cfile);

		if (val[0] == 0) {
			parse_warn("zero-length shared network name");
			val = "<no-name-given>";
		}
		name = malloc(strlen(val) + 1);
		if (!name)
			error("no memory for shared network name");
		strlcpy(name, val, strlen(val) + 1);
	} else {
		name = parse_host_name(cfile);
		if (!name)
			return;
	}
	share->name = name;

	if (!parse_lbrace(cfile))
		return;

	do {
		token = peek_token(&val, cfile);
		if (token == RBRACE) {
			token = next_token(&val, cfile);
			if (!share->subnets) {
				parse_warn("empty shared-network decl");
				return;
			}
			enter_shared_network(share);
			return;
		} else if (token == EOF) {
			token = next_token(&val, cfile);
			parse_warn("unexpected end of file");
			break;
		}

		declaration = parse_statement(cfile, share->group,
		    SHARED_NET_DECL, NULL, declaration);
	} while (1);
}

/* subnet-declaration :==
	net NETMASK netmask RBRACE parameters declarations LBRACE */

void parse_subnet_declaration(cfile, share)
	FILE *cfile;
	struct shared_network *share;
{
	char *val;
	int token;
	struct subnet *subnet, *t, *u;
	struct iaddr iaddr;
	unsigned char addr[4];
	int len = sizeof addr;
	int declaration = 0;

	subnet = new_subnet("parse_subnet_declaration");
	if (!subnet)
		error("No memory for new subnet");
	subnet->shared_network = share;
	subnet->group = clone_group(share->group, "parse_subnet_declaration");
	subnet->group->subnet = subnet;

	/* Get the network number... */
	if (!parse_numeric_aggregate(cfile, addr, &len, DOT, 10, 8))
		return;
	memcpy(iaddr.iabuf, addr, len);
	iaddr.len = len;
	subnet->net = iaddr;

	token = next_token(&val, cfile);
	if (token != NETMASK) {
		parse_warn("Expecting netmask");
		skip_to_semi(cfile);
		return;
	}

	/* Get the netmask... */
	if (!parse_numeric_aggregate(cfile, addr, &len, DOT, 10, 8))
		return;
	memcpy(iaddr.iabuf, addr, len);
	iaddr.len = len;
	subnet->netmask = iaddr;

	enter_subnet(subnet);

	if (!parse_lbrace(cfile))
		return;

	do {
		token = peek_token(&val, cfile);
		if (token == RBRACE) {
			token = next_token(&val, cfile);
			break;
		} else if (token == EOF) {
			token = next_token(&val, cfile);
			parse_warn("unexpected end of file");
			break;
		}
		declaration = parse_statement(cfile, subnet->group,
		    SUBNET_DECL, NULL, declaration);
	} while (1);

	/* If this subnet supports dynamic bootp, flag it so in the
	   shared_network containing it. */
	if (subnet->group->dynamic_bootp)
		share->group->dynamic_bootp = 1;
	if (subnet->group->one_lease_per_client)
		share->group->one_lease_per_client = 1;

	/* Add the subnet to the list of subnets in this shared net. */
	if (!share->subnets)
		share->subnets = subnet;
	else {
		u = NULL;
		for (t = share->subnets; t; t = t->next_sibling) {
			if (subnet_inner_than(subnet, t, 0)) {
				if (u)
					u->next_sibling = subnet;
				else
					share->subnets = subnet;
				subnet->next_sibling = t;
				return;
			}
			u = t;
		}
		u->next_sibling = subnet;
	}
}

/* group-declaration :== RBRACE parameters declarations LBRACE */

void parse_group_declaration(cfile, group)
	FILE *cfile;
	struct group *group;
{
	char *val;
	int token;
	struct group *g;
	int declaration = 0;

	g = clone_group(group, "parse_group_declaration");

	if (!parse_lbrace(cfile))
		return;

	do {
		token = peek_token(&val, cfile);
		if (token == RBRACE) {
			token = next_token(&val, cfile);
			break;
		} else if (token == EOF) {
			token = next_token(&val, cfile);
			parse_warn("unexpected end of file");
			break;
		}
		declaration = parse_statement(cfile, g, GROUP_DECL, NULL,
		    declaration);
	} while (1);
}

/* ip-addr-or-hostname :== ip-address | hostname
   ip-address :== NUMBER DOT NUMBER DOT NUMBER DOT NUMBER

   Parse an ip address or a hostname.   If uniform is zero, put in
   a TREE_LIMIT node to catch hostnames that evaluate to more than
   one IP address. */

struct tree *parse_ip_addr_or_hostname(cfile, uniform)
	FILE *cfile;
	int uniform;
{
	char *val;
	int token;
	unsigned char addr[4];
	int len = sizeof addr;
	char *name;
	struct tree *rv;
	struct hostent *h;

	token = peek_token(&val, cfile);
	if (is_identifier(token)) {
		name = parse_host_name(cfile);
		if (!name)
			return NULL;
		h = gethostbyname(name);
		if (h == NULL) {
			parse_warn("%s (%d): could not resolve hostname",
			    val, token);
			return NULL;
		}
		rv = tree_const(h->h_addr_list[0], h->h_length);
		if (!uniform)
			rv = tree_limit(rv, 4);
	} else if (token == NUMBER) {
		if (!parse_numeric_aggregate(cfile, addr, &len, DOT, 10, 8))
			return NULL;
		rv = tree_const(addr, len);
	} else {
		if (token != RBRACE && token != LBRACE)
			token = next_token(&val, cfile);
		parse_warn("%s (%d): expecting IP address or hostname",
			    val, token);
		if (token != SEMI)
			skip_to_semi(cfile);
		return NULL;
	}

	return rv;
}


/* fixed-addr-parameter :== ip-addrs-or-hostnames SEMI
   ip-addrs-or-hostnames :== ip-addr-or-hostname
			   | ip-addrs-or-hostnames ip-addr-or-hostname */

struct tree_cache *parse_fixed_addr_param(cfile)
	FILE *cfile;
{
	char *val;
	int token;
	struct tree *tree = NULL;
	struct tree *tmp;

	do {
		tmp = parse_ip_addr_or_hostname(cfile, 0);
		if (tree)
			tree = tree_concat(tree, tmp);
		else
			tree = tmp;
		token = peek_token(&val, cfile);
		if (token == COMMA)
			token = next_token(&val, cfile);
	} while (token == COMMA);

	if (!parse_semi(cfile))
		return NULL;
	return tree_cache(tree);
}

/* option_parameter :== identifier DOT identifier <syntax> SEMI
		      | identifier <syntax> SEMI

   Option syntax is handled specially through format strings, so it
   would be painful to come up with BNF for it.   However, it always
   starts as above and ends in a SEMI. */

void parse_option_param(cfile, group)
	FILE *cfile;
	struct group *group;
{
	char *val;
	int token;
	unsigned char buf[4];
	char *vendor;
	char *fmt;
	struct universe *universe;
	struct option *option;
	struct tree *tree = NULL;
	struct tree *t;

	token = next_token(&val, cfile);
	if (!is_identifier(token)) {
		parse_warn("expecting identifier after option keyword.");
		if (token != SEMI)
			skip_to_semi(cfile);
		return;
	}
	vendor = malloc(strlen(val) + 1);
	if (!vendor)
		error("no memory for vendor token.");
	strlcpy(vendor, val, strlen(val) + 1);
	token = peek_token(&val, cfile);
	if (token == DOT) {
		/* Go ahead and take the DOT token... */
		token = next_token(&val, cfile);

		/* The next token should be an identifier... */
		token = next_token(&val, cfile);
		if (!is_identifier(token)) {
			parse_warn("expecting identifier after '.'");
			if (token != SEMI)
				skip_to_semi(cfile);
			return;
		}

		/* Look up the option name hash table for the specified
		   vendor. */
		universe = ((struct universe *)hash_lookup(&universe_hash,
		    (unsigned char *)vendor, 0));
		/* If it's not there, we can't parse the rest of the
		   declaration. */
		if (!universe) {
			parse_warn("no vendor named %s.", vendor);
			skip_to_semi(cfile);
			return;
		}
	} else {
		/* Use the default hash table, which contains all the
		   standard dhcp option names. */
		val = vendor;
		universe = &dhcp_universe;
	}

	/* Look up the actual option info... */
	option = (struct option *)hash_lookup(universe->hash,
	    (unsigned char *)val, 0);

	/* If we didn't get an option structure, it's an undefined option. */
	if (!option) {
		if (val == vendor)
			parse_warn("no option named %s", val);
		else
			parse_warn("no option named %s for vendor %s",
				    val, vendor);
		skip_to_semi(cfile);
		return;
	}

	/* Free the initial identifier token. */
	free(vendor);

	/* Parse the option data... */
	do {
		/* Set a flag if this is an array of a simple type (i.e.,
		   not an array of pairs of IP addresses, or something
		   like that. */
		int uniform = option->format[1] == 'A';

		for (fmt = option->format; *fmt; fmt++) {
			if (*fmt == 'A')
				break;
			switch (*fmt) {
			case 'X':
				token = peek_token(&val, cfile);
				if (token == NUMBER_OR_NAME ||
				    token == NUMBER) {
					do {
						token = next_token
							(&val, cfile);
						if (token != NUMBER &&
						    token != NUMBER_OR_NAME) {
							parse_warn("expecting "
							    "number.");
							if (token != SEMI)
								skip_to_semi(
								    cfile);
							return;
						}
						convert_num(buf, val, 16, 8);
						tree = tree_concat(tree,
						    tree_const(buf, 1));
						token = peek_token(&val, cfile);
						if (token == COLON)
							token = next_token(&val,
							    cfile);
					} while (token == COLON);
				} else if (token == STRING) {
					token = next_token(&val, cfile);
					tree = tree_concat(tree,
					    tree_const((unsigned char *)val,
					    strlen(val)));
				} else {
					parse_warn("expecting string %s.",
					    "or hexadecimal data");
					skip_to_semi(cfile);
					return;
				}
				break;

			case 't': /* Text string... */
				token = next_token(&val, cfile);
				if (token != STRING
				    && !is_identifier(token)) {
					parse_warn("expecting string.");
					if (token != SEMI)
						skip_to_semi(cfile);
					return;
				}
				tree = tree_concat(tree,
				    tree_const((unsigned char *)val,
				    strlen(val)));
				break;

			case 'I': /* IP address or hostname. */
				t = parse_ip_addr_or_hostname(cfile, uniform);
				if (!t)
					return;
				tree = tree_concat(tree, t);
				break;

			case 'L': /* Unsigned 32-bit integer... */
			case 'l':	/* Signed 32-bit integer... */
				token = next_token(&val, cfile);
				if (token != NUMBER) {
					parse_warn("expecting number.");
					if (token != SEMI)
						skip_to_semi(cfile);
					return;
				}
				convert_num(buf, val, 0, 32);
				tree = tree_concat(tree, tree_const(buf, 4));
				break;
			case 's':	/* Signed 16-bit integer. */
			case 'S':	/* Unsigned 16-bit integer. */
				token = next_token(&val, cfile);
				if (token != NUMBER) {
					parse_warn("expecting number.");
					if (token != SEMI)
						skip_to_semi(cfile);
					return;
				}
				convert_num(buf, val, 0, 16);
				tree = tree_concat(tree, tree_const(buf, 2));
				break;
			case 'b':	/* Signed 8-bit integer. */
			case 'B':	/* Unsigned 8-bit integer. */
				token = next_token(&val, cfile);
				if (token != NUMBER) {
					parse_warn("expecting number.");
					if (token != SEMI)
						skip_to_semi(cfile);
					return;
				}
				convert_num(buf, val, 0, 8);
				tree = tree_concat(tree, tree_const(buf, 1));
				break;
			case 'f': /* Boolean flag. */
				token = next_token(&val, cfile);
				if (!is_identifier(token)) {
					parse_warn("expecting identifier.");
					if (token != SEMI)
						skip_to_semi(cfile);
					return;
				}
				if (!strcasecmp(val, "true")
				    || !strcasecmp(val, "on"))
					buf[0] = 1;
				else if (!strcasecmp(val, "false")
					 || !strcasecmp(val, "off"))
					buf[0] = 0;
				else {
					parse_warn("expecting boolean.");
					if (token != SEMI)
						skip_to_semi(cfile);
					return;
				}
				tree = tree_concat(tree, tree_const(buf, 1));
				break;
			default:
				warn("Bad format %c in parse_option_param.",
				    *fmt);
				skip_to_semi(cfile);
				return;
			}
		}
		if (*fmt == 'A') {
			token = peek_token(&val, cfile);
			if (token == COMMA) {
				token = next_token(&val, cfile);
				continue;
			}
			break;
		}
	} while (*fmt == 'A');

	token = next_token(&val, cfile);
	if (token != SEMI) {
		parse_warn("semicolon expected.");
		skip_to_semi(cfile);
		return;
	}
	group->options[option->code] = tree_cache(tree);
}

/* timestamp :== date

   Timestamps are actually not used in dhcpd.conf, which is a static file,
   but rather in the database file and the journal file.  (Okay, actually
   they're not even used there yet). */

time_t parse_timestamp(cfile)
	FILE *cfile;
{
	time_t rv;

	rv = parse_date(cfile);
	return rv;
}

/* lease_declaration :== LEASE ip_address LBRACE lease_parameters RBRACE

   lease_parameters :== <nil>
		      | lease_parameter
		      | lease_parameters lease_parameter

   lease_parameter :== STARTS date
		     | ENDS date
		     | TIMESTAMP date
		     | HARDWARE hardware-parameter
		     | UID hex_numbers SEMI
		     | HOSTNAME hostname SEMI
		     | CLIENT_HOSTNAME hostname SEMI
		     | CLASS identifier SEMI
		     | DYNAMIC_BOOTP SEMI */

struct lease *parse_lease_declaration(cfile)
	FILE *cfile;
{
	char *val;
	int token;
	unsigned char addr[4];
	int len = sizeof addr;
	int seenmask = 0;
	int seenbit;
	char tbuf[32];
	static struct lease lease;

	/* Zap the lease structure... */
	memset(&lease, 0, sizeof lease);

	/* Get the address for which the lease has been issued. */
	if (!parse_numeric_aggregate(cfile, addr, &len, DOT, 10, 8))
		return NULL;
	memcpy(lease.ip_addr.iabuf, addr, len);
	lease.ip_addr.len = len;

	if (!parse_lbrace(cfile))
		return NULL;

	do {
		token = next_token(&val, cfile);
		if (token == RBRACE)
			break;
		else if (token == EOF) {
			parse_warn("unexpected end of file");
			break;
		}
		strlcpy(tbuf, val, sizeof tbuf);

		/* Parse any of the times associated with the lease. */
		if (token == STARTS || token == ENDS || token == TIMESTAMP) {
			time_t t;
			t = parse_date(cfile);
			switch (token) {
			case STARTS:
				seenbit = 1;
				lease.starts = t;
				break;

			case ENDS:
				seenbit = 2;
				lease.ends = t;
				break;

			case TIMESTAMP:
				seenbit = 4;
				lease.timestamp = t;
				break;

			default:
				/*NOTREACHED*/
				seenbit = 0;
				break;
			}
		} else {
			switch (token) {
				/* Colon-separated hexadecimal octets... */
			case UID:
				seenbit = 8;
				token = peek_token(&val, cfile);
				if (token == STRING) {
					token = next_token(&val, cfile);
					lease.uid_len = strlen(val);
					lease.uid = (unsigned char *)
						malloc(lease.uid_len);
					if (!lease.uid) {
						warn("no space for uid");
						return NULL;
					}
					memcpy(lease.uid, val, lease.uid_len);
					parse_semi(cfile);
				} else {
					lease.uid_len = 0;
					lease.uid =
					    parse_numeric_aggregate(cfile,
					    NULL, &lease.uid_len, ':', 16, 8);
					if (!lease.uid) {
						warn("no space for uid");
						return NULL;
					}
					if (lease.uid_len == 0) {
						lease.uid = NULL;
						parse_warn("zero-length uid");
						seenbit = 0;
						break;
					}
				}
				if (!lease.uid)
					error("No memory for lease uid");
				break;

			case CLASS:
				seenbit = 32;
				token = next_token(&val, cfile);
				if (!is_identifier(token)) {
					if (token != SEMI)
						skip_to_semi(cfile);
					return NULL;
				}
				/* for now, we aren't using this. */
				break;

			case HARDWARE:
				seenbit = 64;
				parse_hardware_param(cfile,
				    &lease.hardware_addr);
				break;

			case DYNAMIC_BOOTP:
				seenbit = 128;
				lease.flags |= BOOTP_LEASE;
				break;

			case ABANDONED:
				seenbit = 256;
				lease.flags |= ABANDONED_LEASE;
				break;

			case HOSTNAME:
				seenbit = 512;
				token = peek_token(&val, cfile);
				if (token == STRING)
					lease.hostname = parse_string(cfile);
				else
					lease.hostname =
					    parse_host_name(cfile);
				if (!lease.hostname) {
					seenbit = 0;
					return NULL;
				}
				break;

			case CLIENT_HOSTNAME:
				seenbit = 1024;
				token = peek_token(&val, cfile);
				if (token == STRING)
					lease.client_hostname =
					    parse_string(cfile);
				else
					lease.client_hostname =
					    parse_host_name(cfile);
				break;

			default:
				skip_to_semi(cfile);
				seenbit = 0;
				return NULL;
			}

			if (token != HARDWARE && token != STRING) {
				token = next_token(&val, cfile);
				if (token != SEMI) {
					parse_warn("semicolon expected.");
					skip_to_semi(cfile);
					return NULL;
				}
			}
		}
		if (seenmask & seenbit) {
			parse_warn("Too many %s parameters in lease %s\n",
			    tbuf, piaddr(lease.ip_addr));
		} else
			seenmask |= seenbit;

	} while (1);
	return &lease;
}

/*
 * address-range-declaration :== ip-address ip-address SEMI
 *			       | DYNAMIC_BOOTP ip-address ip-address SEMI
 */
void
parse_address_range(FILE *cfile, struct subnet *subnet)
{
	struct iaddr low, high;
	unsigned char addr[4];
	int len = sizeof addr, token, dynamic = 0;
	char *val;

	if ((token = peek_token(&val, cfile)) == DYNAMIC_BOOTP) {
		token = next_token(&val, cfile);
		subnet->group->dynamic_bootp = dynamic = 1;
	}

	/* Get the bottom address in the range... */
	if (!parse_numeric_aggregate(cfile, addr, &len, DOT, 10, 8))
		return;
	memcpy(low.iabuf, addr, len);
	low.len = len;

	/* Only one address? */
	token = peek_token(&val, cfile);
	if (token == SEMI)
		high = low;
	else {
		/* Get the top address in the range... */
		if (!parse_numeric_aggregate(cfile, addr, &len, DOT, 10, 8))
			return;
		memcpy(high.iabuf, addr, len);
		high.len = len;
	}

	token = next_token(&val, cfile);
	if (token != SEMI) {
		parse_warn("semicolon expected.");
		skip_to_semi(cfile);
		return;
	}

	/* Create the new address range... */
	new_address_range(low, high, subnet, dynamic);
}


