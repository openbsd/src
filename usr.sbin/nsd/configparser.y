/*
 * configparser.y -- yacc grammar for NSD configuration files
 *
 * Copyright (c) 2001-2006, NLnet Labs. All rights reserved.
 *
 * See LICENSE for the license.
 *
 */

%{
#include <config.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "options.h"
#include "util.h"
#include "configyyrename.h"
int c_lex(void);
void c_error(const char *message);

#ifdef __cplusplus
extern "C"
#endif /* __cplusplus */

/* these need to be global, otherwise they cannot be used inside yacc */
extern config_parser_state_t* cfg_parser;
static int server_settings_seen = 0;

#if 0
#define OUTYY(s)  printf s /* used ONLY when debugging */
#else
#define OUTYY(s)
#endif

%}
%union {
	char*	str;
}

%token SPACE LETTER NEWLINE COMMENT COLON ANY ZONESTR
%token <str> STRING
%token VAR_SERVER VAR_NAME VAR_IP_ADDRESS VAR_DEBUG_MODE
%token VAR_IP4_ONLY VAR_IP6_ONLY VAR_DATABASE VAR_IDENTITY VAR_NSID VAR_LOGFILE
%token VAR_SERVER_COUNT VAR_TCP_COUNT VAR_PIDFILE VAR_PORT VAR_STATISTICS
%token VAR_CHROOT VAR_USERNAME VAR_ZONESDIR VAR_XFRDFILE VAR_DIFFFILE
%token VAR_XFRD_RELOAD_TIMEOUT VAR_TCP_QUERY_COUNT VAR_TCP_TIMEOUT
%token VAR_IPV4_EDNS_SIZE VAR_IPV6_EDNS_SIZE
%token VAR_ZONEFILE 
%token VAR_ZONE
%token VAR_ALLOW_NOTIFY VAR_REQUEST_XFR VAR_NOTIFY VAR_PROVIDE_XFR 
%token VAR_NOTIFY_RETRY VAR_OUTGOING_INTERFACE VAR_ALLOW_AXFR_FALLBACK
%token VAR_KEY
%token VAR_ALGORITHM VAR_SECRET
%token VAR_AXFR VAR_UDP
%token VAR_VERBOSITY VAR_HIDE_VERSION

%%
toplevelvars: /* empty */ | toplevelvars toplevelvar ;
toplevelvar: serverstart contents_server | zonestart contents_zone | 
	keystart contents_key;

/* server: declaration */
serverstart: VAR_SERVER
	{ OUTYY(("\nP(server:)\n")); 
		if(server_settings_seen) {
			yyerror("duplicate server: element.");
		}
		server_settings_seen = 1;
	}
	;
contents_server: contents_server content_server | ;
content_server: server_ip_address | server_debug_mode | server_ip4_only | 
	server_ip6_only | server_database | server_identity | server_nsid | server_logfile | 
	server_server_count | server_tcp_count | server_pidfile | server_port | 
	server_statistics | server_chroot | server_username | server_zonesdir |
	server_difffile | server_xfrdfile | server_xfrd_reload_timeout |
	server_tcp_query_count | server_tcp_timeout | server_ipv4_edns_size |
	server_ipv6_edns_size | server_verbosity | server_hide_version;
server_ip_address: VAR_IP_ADDRESS STRING 
	{ 
		OUTYY(("P(server_ip_address:%s)\n", $2)); 
		if(cfg_parser->current_ip_address_option) {
			cfg_parser->current_ip_address_option->next = 
				(ip_address_option_t*)region_alloc(
				cfg_parser->opt->region, sizeof(ip_address_option_t));
			cfg_parser->current_ip_address_option = 
				cfg_parser->current_ip_address_option->next;
			cfg_parser->current_ip_address_option->next=0;
		} else {
			cfg_parser->current_ip_address_option = 
				(ip_address_option_t*)region_alloc(
				cfg_parser->opt->region, sizeof(ip_address_option_t));
			cfg_parser->current_ip_address_option->next=0;
			cfg_parser->opt->ip_addresses = cfg_parser->current_ip_address_option;
		}

		cfg_parser->current_ip_address_option->address = 
			region_strdup(cfg_parser->opt->region, $2);
	}
	;
server_debug_mode: VAR_DEBUG_MODE STRING 
	{ 
		OUTYY(("P(server_debug_mode:%s)\n", $2)); 
		if(strcmp($2, "yes") != 0 && strcmp($2, "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->opt->debug_mode = (strcmp($2, "yes")==0);
	}
	;
server_verbosity: VAR_VERBOSITY STRING 
	{ 
		OUTYY(("P(server_verbosity:%s)\n", $2)); 
		if(atoi($2) == 0 && strcmp($2, "0") != 0)
			yyerror("number expected");
		else cfg_parser->opt->verbosity = atoi($2);
	}
	;
server_hide_version: VAR_HIDE_VERSION STRING 
	{ 
		OUTYY(("P(server_hide_version:%s)\n", $2)); 
		if(strcmp($2, "yes") != 0 && strcmp($2, "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->opt->hide_version = (strcmp($2, "yes")==0);
	}
	;
server_ip4_only: VAR_IP4_ONLY STRING 
	{ 
		OUTYY(("P(server_ip4_only:%s)\n", $2)); 
		if(strcmp($2, "yes") != 0 && strcmp($2, "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->opt->ip4_only = (strcmp($2, "yes")==0);
	}
	;
server_ip6_only: VAR_IP6_ONLY STRING 
	{ 
		OUTYY(("P(server_ip6_only:%s)\n", $2)); 
		if(strcmp($2, "yes") != 0 && strcmp($2, "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->opt->ip6_only = (strcmp($2, "yes")==0);
	}
	;
server_database: VAR_DATABASE STRING
	{ 
		OUTYY(("P(server_database:%s)\n", $2)); 
		cfg_parser->opt->database = region_strdup(cfg_parser->opt->region, $2);
	}
	;
server_identity: VAR_IDENTITY STRING
	{ 
		OUTYY(("P(server_identity:%s)\n", $2)); 
		cfg_parser->opt->identity = region_strdup(cfg_parser->opt->region, $2);
	}
	;
server_nsid: VAR_NSID STRING
	{ 
		unsigned char* nsid = 0;
		uint16_t nsid_len = 0;

		OUTYY(("P(server_nsid:%s)\n", $2));

                if (strlen($2) % 2 != 0) {
			yyerror("the NSID must be a hex string of an even length.");
		} else {
			nsid_len = strlen($2) / 2;
			nsid = xalloc(nsid_len);
			if (hex_pton($2, nsid, nsid_len) == -1)
				yyerror("hex string cannot be parsed in NSID.");
			else
				cfg_parser->opt->nsid = region_strdup(cfg_parser->opt->region, $2);
			free(nsid);
		}
	}
	;
server_logfile: VAR_LOGFILE STRING
	{ 
		OUTYY(("P(server_logfile:%s)\n", $2)); 
		cfg_parser->opt->logfile = region_strdup(cfg_parser->opt->region, $2);
	}
	;
server_server_count: VAR_SERVER_COUNT STRING
	{ 
		OUTYY(("P(server_server_count:%s)\n", $2)); 
		if(atoi($2) <= 0)
			yyerror("number greater than zero expected");
		else cfg_parser->opt->server_count = atoi($2);
	}
	;
server_tcp_count: VAR_TCP_COUNT STRING
	{ 
		OUTYY(("P(server_tcp_count:%s)\n", $2)); 
		if(atoi($2) <= 0)
			yyerror("number greater than zero expected");
		else cfg_parser->opt->tcp_count = atoi($2);
	}
	;
server_pidfile: VAR_PIDFILE STRING
	{ 
		OUTYY(("P(server_pidfile:%s)\n", $2)); 
		cfg_parser->opt->pidfile = region_strdup(cfg_parser->opt->region, $2);
	}
	;
server_port: VAR_PORT STRING
	{ 
		OUTYY(("P(server_port:%s)\n", $2)); 
		cfg_parser->opt->port = region_strdup(cfg_parser->opt->region, $2);
	}
	;
server_statistics: VAR_STATISTICS STRING
	{ 
		OUTYY(("P(server_statistics:%s)\n", $2)); 
		if(atoi($2) == 0 && strcmp($2, "0") != 0)
			yyerror("number expected");
		else cfg_parser->opt->statistics = atoi($2);
	}
	;
server_chroot: VAR_CHROOT STRING
	{ 
		OUTYY(("P(server_chroot:%s)\n", $2)); 
		cfg_parser->opt->chroot = region_strdup(cfg_parser->opt->region, $2);
	}
	;
server_username: VAR_USERNAME STRING
	{ 
		OUTYY(("P(server_username:%s)\n", $2)); 
		cfg_parser->opt->username = region_strdup(cfg_parser->opt->region, $2);
	}
	;
server_zonesdir: VAR_ZONESDIR STRING
	{ 
		OUTYY(("P(server_zonesdir:%s)\n", $2)); 
		cfg_parser->opt->zonesdir = region_strdup(cfg_parser->opt->region, $2);
	}
	;
server_difffile: VAR_DIFFFILE STRING
	{ 
		OUTYY(("P(server_difffile:%s)\n", $2)); 
		cfg_parser->opt->difffile = region_strdup(cfg_parser->opt->region, $2);
	}
	;
server_xfrdfile: VAR_XFRDFILE STRING
	{ 
		OUTYY(("P(server_xfrdfile:%s)\n", $2)); 
		cfg_parser->opt->xfrdfile = region_strdup(cfg_parser->opt->region, $2);
	}
	;
server_xfrd_reload_timeout: VAR_XFRD_RELOAD_TIMEOUT STRING
	{ 
		OUTYY(("P(server_xfrd_reload_timeout:%s)\n", $2)); 
		if(atoi($2) == 0 && strcmp($2, "0") != 0)
			yyerror("number expected");
		cfg_parser->opt->xfrd_reload_timeout = atoi($2);
	}
	;
server_tcp_query_count: VAR_TCP_QUERY_COUNT STRING
	{ 
		OUTYY(("P(server_tcp_query_count:%s)\n", $2)); 
		if(atoi($2) == 0 && strcmp($2, "0") != 0)
			yyerror("number expected");
		cfg_parser->opt->tcp_query_count = atoi($2);
	}
	;
server_tcp_timeout: VAR_TCP_TIMEOUT STRING
	{ 
		OUTYY(("P(server_tcp_timeout:%s)\n", $2)); 
		if(atoi($2) == 0 && strcmp($2, "0") != 0)
			yyerror("number expected");
		cfg_parser->opt->tcp_timeout = atoi($2);
	}
	;
server_ipv4_edns_size: VAR_IPV4_EDNS_SIZE STRING
	{ 
		OUTYY(("P(server_ipv4_edns_size:%s)\n", $2)); 
		if(atoi($2) == 0 && strcmp($2, "0") != 0)
			yyerror("number expected");
		cfg_parser->opt->ipv4_edns_size = atoi($2);
	}
	;
server_ipv6_edns_size: VAR_IPV6_EDNS_SIZE STRING
	{ 
		OUTYY(("P(server_ipv6_edns_size:%s)\n", $2)); 
		if(atoi($2) == 0 && strcmp($2, "0") != 0)
			yyerror("number expected");
		cfg_parser->opt->ipv6_edns_size = atoi($2);
	}
	;

/* zone: declaration */
zonestart: VAR_ZONE
	{ 
		OUTYY(("\nP(zone:)\n")); 
		if(cfg_parser->current_zone) {
			if(!cfg_parser->current_zone->name) 
				c_error("previous zone has no name");
			else {
				if(!nsd_options_insert_zone(cfg_parser->opt, 
					cfg_parser->current_zone))
					c_error("duplicate zone");
			}
			if(!cfg_parser->current_zone->zonefile) 
				c_error("previous zone has no zonefile");
		}
		cfg_parser->current_zone = zone_options_create(cfg_parser->opt->region);
		cfg_parser->current_allow_notify = 0;
		cfg_parser->current_request_xfr = 0;
		cfg_parser->current_notify = 0;
		cfg_parser->current_provide_xfr = 0;
		cfg_parser->current_outgoing_interface = 0;
	}
	;
contents_zone: contents_zone content_zone | content_zone;
content_zone: zone_name | zone_zonefile | zone_allow_notify | 
	zone_request_xfr | zone_notify | zone_notify_retry | zone_provide_xfr | 
	zone_outgoing_interface | zone_allow_axfr_fallback;
zone_name: VAR_NAME STRING
	{ 
		OUTYY(("P(zone_name:%s)\n", $2)); 
#ifndef NDEBUG
		assert(cfg_parser->current_zone);
#endif
		cfg_parser->current_zone->name = region_strdup(cfg_parser->opt->region, $2);
	}
	;
zone_zonefile: VAR_ZONEFILE STRING
	{ 
		OUTYY(("P(zone_zonefile:%s)\n", $2)); 
#ifndef NDEBUG
		assert(cfg_parser->current_zone);
#endif
		cfg_parser->current_zone->zonefile = region_strdup(cfg_parser->opt->region, $2);
	}
	;
zone_allow_notify: VAR_ALLOW_NOTIFY STRING STRING
	{ 
		acl_options_t* acl = parse_acl_info(cfg_parser->opt->region, $2, $3);
		OUTYY(("P(zone_allow_notify:%s %s)\n", $2, $3)); 
		if(cfg_parser->current_allow_notify)
			cfg_parser->current_allow_notify->next = acl;
		else
			cfg_parser->current_zone->allow_notify = acl;
		cfg_parser->current_allow_notify = acl;
	}
	;
zone_request_xfr: VAR_REQUEST_XFR zone_request_xfr_data
	{
	}
	;
zone_request_xfr_data: STRING STRING
	{ 
		acl_options_t* acl = parse_acl_info(cfg_parser->opt->region, $1, $2);
		OUTYY(("P(zone_request_xfr:%s %s)\n", $1, $2)); 
		if(acl->blocked) c_error("blocked address used for request-xfr");
		if(acl->rangetype!=acl_range_single) c_error("address range used for request-xfr");
		if(cfg_parser->current_request_xfr)
			cfg_parser->current_request_xfr->next = acl;
		else
			cfg_parser->current_zone->request_xfr = acl;
		cfg_parser->current_request_xfr = acl;
	}
	| VAR_AXFR STRING STRING
	{ 
		acl_options_t* acl = parse_acl_info(cfg_parser->opt->region, $2, $3);
		acl->use_axfr_only = 1;
		OUTYY(("P(zone_request_xfr:%s %s)\n", $2, $3)); 
		if(acl->blocked) c_error("blocked address used for request-xfr");
		if(acl->rangetype!=acl_range_single) c_error("address range used for request-xfr");
		if(cfg_parser->current_request_xfr)
			cfg_parser->current_request_xfr->next = acl;
		else
			cfg_parser->current_zone->request_xfr = acl;
		cfg_parser->current_request_xfr = acl;
	}
	| VAR_UDP STRING STRING
	{ 
		acl_options_t* acl = parse_acl_info(cfg_parser->opt->region, $2, $3);
		acl->allow_udp = 1;
		OUTYY(("P(zone_request_xfr:%s %s)\n", $2, $3)); 
		if(acl->blocked) c_error("blocked address used for request-xfr");
		if(acl->rangetype!=acl_range_single) c_error("address range used for request-xfr");
		if(cfg_parser->current_request_xfr)
			cfg_parser->current_request_xfr->next = acl;
		else
			cfg_parser->current_zone->request_xfr = acl;
		cfg_parser->current_request_xfr = acl;
	}
	;
zone_notify: VAR_NOTIFY STRING STRING
	{ 
		acl_options_t* acl = parse_acl_info(cfg_parser->opt->region, $2, $3);
		OUTYY(("P(zone_notify:%s %s)\n", $2, $3)); 
		if(acl->blocked) c_error("blocked address used for notify");
		if(acl->rangetype!=acl_range_single) c_error("address range used for notify");
		if(cfg_parser->current_notify)
			cfg_parser->current_notify->next = acl;
		else
			cfg_parser->current_zone->notify = acl;
		cfg_parser->current_notify = acl;
	}
	;
zone_notify_retry: VAR_NOTIFY_RETRY STRING
	{ 
		OUTYY(("P(zone_notify_retry:%s)\n", $2)); 
		if(atoi($2) == 0 && strcmp($2, "0") != 0)
			yyerror("number expected");
		else cfg_parser->current_zone->notify_retry = atoi($2);
	}
	;
zone_provide_xfr: VAR_PROVIDE_XFR STRING STRING
	{ 
		acl_options_t* acl = parse_acl_info(cfg_parser->opt->region, $2, $3);
		OUTYY(("P(zone_provide_xfr:%s %s)\n", $2, $3)); 
		if(cfg_parser->current_provide_xfr)
			cfg_parser->current_provide_xfr->next = acl;
		else
			cfg_parser->current_zone->provide_xfr = acl;
		cfg_parser->current_provide_xfr = acl;
	}
	;
zone_outgoing_interface: VAR_OUTGOING_INTERFACE STRING
	{ 
		acl_options_t* acl = parse_acl_info(cfg_parser->opt->region, $2, "NOKEY");
		OUTYY(("P(zone_outgoing_interface:%s)\n", $2)); 

		if(cfg_parser->current_outgoing_interface)
			cfg_parser->current_outgoing_interface->next = acl;
		else
			cfg_parser->current_zone->outgoing_interface = acl;
		cfg_parser->current_outgoing_interface = acl;
	}
	;
zone_allow_axfr_fallback: VAR_ALLOW_AXFR_FALLBACK STRING 
	{ 
		OUTYY(("P(zone_allow_axfr_fallback:%s)\n", $2)); 
		if(strcmp($2, "yes") != 0 && strcmp($2, "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->current_zone->allow_axfr_fallback = (strcmp($2, "yes")==0);
	}
	;

/* key: declaration */
keystart: VAR_KEY
	{ 
		OUTYY(("\nP(key:)\n")); 
		if(cfg_parser->current_key) {
			if(!cfg_parser->current_key->name) c_error("previous key has no name");
			if(!cfg_parser->current_key->algorithm) c_error("previous key has no algorithm");
			if(!cfg_parser->current_key->secret) c_error("previous key has no secret blob");
			cfg_parser->current_key->next = key_options_create(cfg_parser->opt->region);
			cfg_parser->current_key = cfg_parser->current_key->next;
		} else {
			cfg_parser->current_key = key_options_create(cfg_parser->opt->region);
                	cfg_parser->opt->keys = cfg_parser->current_key;
		}
		cfg_parser->opt->numkeys++;
	}
	;
contents_key: contents_key content_key | content_key;
content_key: key_name | key_algorithm | key_secret;
key_name: VAR_NAME STRING
	{ 
		OUTYY(("P(key_name:%s)\n", $2)); 
#ifndef NDEBUG
		assert(cfg_parser->current_key);
#endif
		cfg_parser->current_key->name = region_strdup(cfg_parser->opt->region, $2);
	}
	;
key_algorithm: VAR_ALGORITHM STRING
	{ 
		OUTYY(("P(key_algorithm:%s)\n", $2)); 
#ifndef NDEBUG
		assert(cfg_parser->current_key);
#endif
		cfg_parser->current_key->algorithm = region_strdup(cfg_parser->opt->region, $2);
	}
	;
key_secret: VAR_SECRET STRING
	{ 
		OUTYY(("key_secret:%s)\n", $2)); 
#ifndef NDEBUG
		assert(cfg_parser->current_key);
#endif
		cfg_parser->current_key->secret = region_strdup(cfg_parser->opt->region, $2);
	}
	;

%%

/* parse helper routines could be here */
