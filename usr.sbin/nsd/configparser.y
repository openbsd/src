/*
 * configparser.y -- yacc grammar for NSD configuration files
 *
 * Copyright (c) 2001-2006, NLnet Labs. All rights reserved.
 *
 * See LICENSE for the license.
 *
 */

%{
#include "config.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "options.h"
#include "util.h"
#include "dname.h"
#include "tsig.h"
#include "rrl.h"
#include "configyyrename.h"
int c_lex(void);
void c_error(const char *message);

#ifdef __cplusplus
extern "C"
#endif /* __cplusplus */

/* these need to be global, otherwise they cannot be used inside yacc */
extern config_parser_state_type* cfg_parser;

#if 0
#define OUTYY(s) printf s /* used ONLY when debugging */
#else
#define OUTYY(s)
#endif

%}
%union {
	char*	str;
}

%token SPACE LETTER NEWLINE COMMENT COLON ANY ZONESTR
%token <str> STRING
%token VAR_SERVER VAR_NAME VAR_IP_ADDRESS VAR_IP_TRANSPARENT VAR_DEBUG_MODE
%token VAR_IP4_ONLY VAR_IP6_ONLY VAR_DATABASE VAR_IDENTITY VAR_NSID VAR_LOGFILE
%token VAR_SERVER_COUNT VAR_TCP_COUNT VAR_PIDFILE VAR_PORT VAR_STATISTICS
%token VAR_CHROOT VAR_USERNAME VAR_ZONESDIR VAR_XFRDFILE VAR_DIFFFILE
%token VAR_XFRD_RELOAD_TIMEOUT VAR_TCP_QUERY_COUNT VAR_TCP_TIMEOUT
%token VAR_IPV4_EDNS_SIZE VAR_IPV6_EDNS_SIZE VAR_DO_IP4 VAR_DO_IP6
%token VAR_TCP_MSS VAR_OUTGOING_TCP_MSS VAR_IP_FREEBIND
%token VAR_ZONEFILE 
%token VAR_ZONE
%token VAR_ALLOW_NOTIFY VAR_REQUEST_XFR VAR_NOTIFY VAR_PROVIDE_XFR VAR_SIZE_LIMIT_XFR 
%token VAR_NOTIFY_RETRY VAR_OUTGOING_INTERFACE VAR_ALLOW_AXFR_FALLBACK
%token VAR_KEY
%token VAR_ALGORITHM VAR_SECRET
%token VAR_AXFR VAR_UDP
%token VAR_VERBOSITY VAR_HIDE_VERSION
%token VAR_PATTERN VAR_INCLUDEPATTERN VAR_ZONELISTFILE
%token VAR_REMOTE_CONTROL VAR_CONTROL_ENABLE VAR_CONTROL_INTERFACE
%token VAR_CONTROL_PORT VAR_SERVER_KEY_FILE VAR_SERVER_CERT_FILE
%token VAR_CONTROL_KEY_FILE VAR_CONTROL_CERT_FILE VAR_XFRDIR
%token VAR_RRL_SIZE VAR_RRL_RATELIMIT VAR_RRL_SLIP 
%token VAR_RRL_IPV4_PREFIX_LENGTH VAR_RRL_IPV6_PREFIX_LENGTH
%token VAR_RRL_WHITELIST_RATELIMIT VAR_RRL_WHITELIST
%token VAR_ZONEFILES_CHECK VAR_ZONEFILES_WRITE VAR_LOG_TIME_ASCII
%token VAR_ROUND_ROBIN VAR_ZONESTATS VAR_REUSEPORT VAR_VERSION
%token VAR_MAX_REFRESH_TIME VAR_MIN_REFRESH_TIME
%token VAR_MAX_RETRY_TIME VAR_MIN_RETRY_TIME
%token VAR_MULTI_MASTER_CHECK VAR_MINIMAL_RESPONSES VAR_REFUSE_ANY
%token VAR_USE_SYSTEMD VAR_DNSTAP VAR_DNSTAP_ENABLE VAR_DNSTAP_SOCKET_PATH
%token VAR_DNSTAP_SEND_IDENTITY VAR_DNSTAP_SEND_VERSION VAR_DNSTAP_IDENTITY
%token VAR_DNSTAP_VERSION VAR_DNSTAP_LOG_AUTH_QUERY_MESSAGES
%token VAR_DNSTAP_LOG_AUTH_RESPONSE_MESSAGES

%%
toplevelvars: /* empty */ | toplevelvars toplevelvar ;
toplevelvar: serverstart contents_server | zonestart contents_zone | 
	keystart contents_key | patternstart contents_pattern |
	rcstart contents_rc | dtstart contents_dt;

/* server: declaration */
serverstart: VAR_SERVER
	{ OUTYY(("\nP(server:)\n")); 
		if(cfg_parser->server_settings_seen) {
			yyerror("duplicate server: element.");
		}
		cfg_parser->server_settings_seen = 1;
	}
	;
contents_server: contents_server content_server | ;
content_server: server_ip_address | server_ip_transparent | server_debug_mode | server_ip4_only | 
	server_ip6_only | server_database | server_identity | server_nsid | server_logfile | 
	server_server_count | server_tcp_count | server_pidfile | server_port | 
	server_statistics | server_chroot | server_username | server_zonesdir |
	server_difffile | server_xfrdfile | server_xfrd_reload_timeout |
	server_tcp_query_count | server_tcp_timeout | server_ipv4_edns_size |
	server_ipv6_edns_size | server_verbosity | server_hide_version |
	server_zonelistfile | server_xfrdir |
	server_tcp_mss | server_outgoing_tcp_mss |
	server_rrl_size | server_rrl_ratelimit | server_rrl_slip | 
	server_rrl_ipv4_prefix_length | server_rrl_ipv6_prefix_length | server_rrl_whitelist_ratelimit |
	server_zonefiles_check | server_do_ip4 | server_do_ip6 |
	server_zonefiles_write | server_log_time_ascii | server_round_robin |
	server_reuseport | server_version | server_ip_freebind |
	server_minimal_responses | server_refuse_any | server_use_systemd;
server_ip_address: VAR_IP_ADDRESS STRING 
	{ 
		OUTYY(("P(server_ip_address:%s)\n", $2)); 
		if(cfg_parser->current_ip_address_option) {
			cfg_parser->current_ip_address_option->next = 
				(ip_address_option_type*)region_alloc(
				cfg_parser->opt->region, sizeof(ip_address_option_type));
			cfg_parser->current_ip_address_option = 
				cfg_parser->current_ip_address_option->next;
			cfg_parser->current_ip_address_option->next=0;
		} else {
			cfg_parser->current_ip_address_option = 
				(ip_address_option_type*)region_alloc(
				cfg_parser->opt->region, sizeof(ip_address_option_type));
			cfg_parser->current_ip_address_option->next=0;
			cfg_parser->opt->ip_addresses = cfg_parser->current_ip_address_option;
		}

		cfg_parser->current_ip_address_option->address = 
			region_strdup(cfg_parser->opt->region, $2);
	}
	;
server_ip_transparent: VAR_IP_TRANSPARENT STRING 
	{ 
		OUTYY(("P(server_ip_transparent:%s)\n", $2)); 
		if(strcmp($2, "yes") != 0 && strcmp($2, "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->opt->ip_transparent = (strcmp($2, "yes")==0);
	}
	;
server_ip_freebind: VAR_IP_FREEBIND STRING 
	{ 
		OUTYY(("P(server_ip_freebind:%s)\n", $2)); 
		if(strcmp($2, "yes") != 0 && strcmp($2, "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->opt->ip_freebind = (strcmp($2, "yes")==0);
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
server_use_systemd: VAR_USE_SYSTEMD STRING 
	{ 
		OUTYY(("P(server_use_systemd:%s)\n", $2)); 
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
		/* for backwards compatibility in config file with NSD3 */
		OUTYY(("P(server_ip4_only:%s)\n", $2)); 
		if(strcmp($2, "yes") != 0 && strcmp($2, "no") != 0)
			yyerror("expected yes or no.");
		else if(strcmp($2, "yes")==0) {
			cfg_parser->opt->do_ip4 = 1;
			cfg_parser->opt->do_ip6 = 0;
		}
	}
	;
server_ip6_only: VAR_IP6_ONLY STRING 
	{ 
		/* for backwards compatibility in config file with NSD3 */
		OUTYY(("P(server_ip6_only:%s)\n", $2)); 
		if(strcmp($2, "yes") != 0 && strcmp($2, "no") != 0)
			yyerror("expected yes or no.");
		else if(strcmp($2, "yes")==0) {
			cfg_parser->opt->do_ip6 = 1;
			cfg_parser->opt->do_ip4 = 0;
		}
	}
	;
server_do_ip4: VAR_DO_IP4 STRING 
	{ 
		OUTYY(("P(server_do_ip4:%s)\n", $2)); 
		if(strcmp($2, "yes") != 0 && strcmp($2, "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->opt->do_ip4 = (strcmp($2, "yes")==0);
	}
	;
server_do_ip6: VAR_DO_IP6 STRING 
	{ 
		OUTYY(("P(server_do_ip6:%s)\n", $2)); 
		if(strcmp($2, "yes") != 0 && strcmp($2, "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->opt->do_ip6 = (strcmp($2, "yes")==0);
	}
	;
server_reuseport: VAR_REUSEPORT STRING 
	{ 
		OUTYY(("P(server_reuseport:%s)\n", $2)); 
		if(strcmp($2, "yes") != 0 && strcmp($2, "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->opt->reuseport = (strcmp($2, "yes")==0);
	}
	;
server_database: VAR_DATABASE STRING
	{ 
		OUTYY(("P(server_database:%s)\n", $2)); 
		cfg_parser->opt->database = region_strdup(cfg_parser->opt->region, $2);
		if(cfg_parser->opt->database[0] == 0 &&
			cfg_parser->opt->zonefiles_write == 0)
			cfg_parser->opt->zonefiles_write = ZONEFILES_WRITE_INTERVAL;
	}
	;
server_identity: VAR_IDENTITY STRING
	{ 
		OUTYY(("P(server_identity:%s)\n", $2)); 
		cfg_parser->opt->identity = region_strdup(cfg_parser->opt->region, $2);
	}
	;
server_version: VAR_VERSION STRING
	{ 
		OUTYY(("P(server_version:%s)\n", $2)); 
		cfg_parser->opt->version = region_strdup(cfg_parser->opt->region, $2);
	}
	;
server_nsid: VAR_NSID STRING
	{ 
		unsigned char* nsid = 0;
		size_t nsid_len = 0;

		OUTYY(("P(server_nsid:%s)\n", $2));

		if (strncasecmp($2, "ascii_", 6) == 0) {
			nsid_len = strlen($2+6);
			if(nsid_len < 65535) {
				cfg_parser->opt->nsid = region_alloc(cfg_parser->opt->region, nsid_len*2+1);
				hex_ntop((uint8_t*)$2+6, nsid_len, (char*)cfg_parser->opt->nsid, nsid_len*2+1);
			} else
				yyerror("NSID too long");
		} else if (strlen($2) % 2 != 0) {
			yyerror("the NSID must be a hex string of an even length.");
		} else {
			nsid_len = strlen($2) / 2;
			if(nsid_len < 65535) {
				nsid = xalloc(nsid_len);
				if (hex_pton($2, nsid, nsid_len) == -1)
					yyerror("hex string cannot be parsed in NSID.");
				else
					cfg_parser->opt->nsid = region_strdup(cfg_parser->opt->region, $2);
				free(nsid);
			} else
				yyerror("NSID too long");
		}
	}
	;
server_logfile: VAR_LOGFILE STRING
	{ 
		OUTYY(("P(server_logfile:%s)\n", $2)); 
		cfg_parser->opt->logfile = region_strdup(cfg_parser->opt->region, $2);
	}
	;
server_log_time_ascii: VAR_LOG_TIME_ASCII STRING 
	{ 
		OUTYY(("P(server_log_time_ascii:%s)\n", $2)); 
		if(strcmp($2, "yes") != 0 && strcmp($2, "no") != 0)
			yyerror("expected yes or no.");
		else {
			cfg_parser->opt->log_time_ascii = (strcmp($2, "yes")==0);
			log_time_asc = cfg_parser->opt->log_time_ascii;
		}
	}
	;
server_round_robin: VAR_ROUND_ROBIN STRING 
	{ 
		OUTYY(("P(server_round_robin:%s)\n", $2)); 
		if(strcmp($2, "yes") != 0 && strcmp($2, "no") != 0)
			yyerror("expected yes or no.");
		else {
			cfg_parser->opt->round_robin = (strcmp($2, "yes")==0);
			round_robin = cfg_parser->opt->round_robin;
		}
	}
	;
server_minimal_responses: VAR_MINIMAL_RESPONSES STRING 
	{ 
		OUTYY(("P(server_minimal_responses:%s)\n", $2)); 
		if(strcmp($2, "yes") != 0 && strcmp($2, "no") != 0)
			yyerror("expected yes or no.");
		else {
			cfg_parser->opt->minimal_responses = (strcmp($2, "yes")==0);
			minimal_responses = cfg_parser->opt->minimal_responses;
		}
	}
	;
server_refuse_any: VAR_REFUSE_ANY STRING 
	{ 
		OUTYY(("P(server_refuse_any:%s)\n", $2)); 
		if(strcmp($2, "yes") != 0 && strcmp($2, "no") != 0)
			yyerror("expected yes or no.");
		else {
			cfg_parser->opt->refuse_any = (strcmp($2, "yes")==0);
		}
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
server_zonelistfile: VAR_ZONELISTFILE STRING
	{ 
		OUTYY(("P(server_zonelistfile:%s)\n", $2)); 
		cfg_parser->opt->zonelistfile = region_strdup(cfg_parser->opt->region, $2);
	}
	;
server_xfrdir: VAR_XFRDIR STRING
	{ 
		OUTYY(("P(server_xfrdir:%s)\n", $2)); 
		cfg_parser->opt->xfrdir = region_strdup(cfg_parser->opt->region, $2);
	}
	;
server_difffile: VAR_DIFFFILE STRING
	{ 
		OUTYY(("P(server_difffile:%s)\n", $2)); 
		/* ignore the value for backwards compatibility in config file*/
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
server_tcp_mss: VAR_TCP_MSS STRING
	{
		OUTYY(("P(server_tcp_mss:%s)\n", $2));
		if(atoi($2) == 0 && strcmp($2, "0") != 0)
			yyerror("number expected");
		cfg_parser->opt->tcp_mss = atoi($2);
	}
	;
server_outgoing_tcp_mss: VAR_OUTGOING_TCP_MSS STRING
	{
		OUTYY(("P(server_outgoing_tcp_mss:%s)\n", $2));
		if(atoi($2) == 0 && strcmp($2, "0") != 0)
			yyerror("number expected");
		cfg_parser->opt->outgoing_tcp_mss = atoi($2);
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
server_rrl_size: VAR_RRL_SIZE STRING
	{ 
		OUTYY(("P(server_rrl_size:%s)\n", $2)); 
#ifdef RATELIMIT
		if(atoi($2) <= 0)
			yyerror("number greater than zero expected");
		cfg_parser->opt->rrl_size = atoi($2);
#endif
	}
	;
server_rrl_ratelimit: VAR_RRL_RATELIMIT STRING
	{ 
		OUTYY(("P(server_rrl_ratelimit:%s)\n", $2)); 
#ifdef RATELIMIT
		cfg_parser->opt->rrl_ratelimit = atoi($2);
#endif
	}
	;
server_rrl_slip: VAR_RRL_SLIP STRING
	{ 
		OUTYY(("P(server_rrl_slip:%s)\n", $2)); 
#ifdef RATELIMIT
		if(atoi($2) < 0)
			yyerror("number equal or greater than zero expected");
		cfg_parser->opt->rrl_slip = atoi($2);
#endif
	}
	;
server_rrl_ipv4_prefix_length: VAR_RRL_IPV4_PREFIX_LENGTH STRING
	{
		OUTYY(("P(server_rrl_ipv4_prefix_length:%s)\n", $2)); 
#ifdef RATELIMIT
		if(atoi($2) < 0 || atoi($2) > 32)
			yyerror("invalid IPv4 prefix length");
		cfg_parser->opt->rrl_ipv4_prefix_length = atoi($2);
#endif
	}
	;
server_rrl_ipv6_prefix_length: VAR_RRL_IPV6_PREFIX_LENGTH STRING
	{
		OUTYY(("P(server_rrl_ipv6_prefix_length:%s)\n", $2)); 
#ifdef RATELIMIT
		if(atoi($2) < 0 || atoi($2) > 64)
			yyerror("invalid IPv6 prefix length");
		cfg_parser->opt->rrl_ipv6_prefix_length = atoi($2);
#endif
	}
	;
server_rrl_whitelist_ratelimit: VAR_RRL_WHITELIST_RATELIMIT STRING
	{ 
		OUTYY(("P(server_rrl_whitelist_ratelimit:%s)\n", $2)); 
#ifdef RATELIMIT
		cfg_parser->opt->rrl_whitelist_ratelimit = atoi($2);
#endif
	}
	;
server_zonefiles_check: VAR_ZONEFILES_CHECK STRING 
	{ 
		OUTYY(("P(server_zonefiles_check:%s)\n", $2)); 
		if(strcmp($2, "yes") != 0 && strcmp($2, "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->opt->zonefiles_check = (strcmp($2, "yes")==0);
	}
	;
server_zonefiles_write: VAR_ZONEFILES_WRITE STRING 
	{ 
		OUTYY(("P(server_zonefiles_write:%s)\n", $2)); 
		if(atoi($2) == 0 && strcmp($2, "0") != 0)
			yyerror("number expected");
		else cfg_parser->opt->zonefiles_write = atoi($2);
	}
	;

rcstart: VAR_REMOTE_CONTROL
	{
		OUTYY(("\nP(remote-control:)\n"));
	}
	;
contents_rc: contents_rc content_rc 
	| ;
content_rc: rc_control_enable | rc_control_interface | rc_control_port |
	rc_server_key_file | rc_server_cert_file | rc_control_key_file |
	rc_control_cert_file
	;
rc_control_enable: VAR_CONTROL_ENABLE STRING
	{
		OUTYY(("P(control_enable:%s)\n", $2));
		if(strcmp($2, "yes") != 0 && strcmp($2, "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->opt->control_enable = (strcmp($2, "yes")==0);
	}
	;
rc_control_port: VAR_CONTROL_PORT STRING
	{
		OUTYY(("P(control_port:%s)\n", $2));
		if(atoi($2) == 0)
			yyerror("control port number expected");
		else cfg_parser->opt->control_port = atoi($2);
	}
	;
rc_control_interface: VAR_CONTROL_INTERFACE STRING
	{
		ip_address_option_type* last = NULL;
		ip_address_option_type* o = (ip_address_option_type*)region_alloc(
			cfg_parser->opt->region, sizeof(ip_address_option_type));
		OUTYY(("P(control_interface:%s)\n", $2));
		/* append at end */
		last = cfg_parser->opt->control_interface;
		while(last && last->next)
			last = last->next;
		if(last == NULL)
			cfg_parser->opt->control_interface = o;
		else	last->next = o;
		o->next = NULL;
		o->address = region_strdup(cfg_parser->opt->region, $2);
	}
	;
rc_server_key_file: VAR_SERVER_KEY_FILE STRING
	{
	OUTYY(("P(rc_server_key_file:%s)\n", $2));
	cfg_parser->opt->server_key_file = region_strdup(cfg_parser->opt->region, $2);
	}
	;
rc_server_cert_file: VAR_SERVER_CERT_FILE STRING
	{
	OUTYY(("P(rc_server_cert_file:%s)\n", $2));
	cfg_parser->opt->server_cert_file = region_strdup(cfg_parser->opt->region, $2);
	}
	;
rc_control_key_file: VAR_CONTROL_KEY_FILE STRING
	{
	OUTYY(("P(rc_control_key_file:%s)\n", $2));
	cfg_parser->opt->control_key_file = region_strdup(cfg_parser->opt->region, $2);
	}
	;
rc_control_cert_file: VAR_CONTROL_CERT_FILE STRING
	{
	OUTYY(("P(rc_control_cert_file:%s)\n", $2));
	cfg_parser->opt->control_cert_file = region_strdup(cfg_parser->opt->region, $2);
	}
	;

/* dnstap: declaration */
dtstart: VAR_DNSTAP
	{
		OUTYY(("\nP(dnstap:)\n"));
	}
	;
contents_dt: contents_dt content_dt
	| ;
content_dt: dt_dnstap_enable | dt_dnstap_socket_path |
	dt_dnstap_send_identity | dt_dnstap_send_version |
	dt_dnstap_identity | dt_dnstap_version |
	dt_dnstap_log_auth_query_messages |
	dt_dnstap_log_auth_response_messages
	;
dt_dnstap_enable: VAR_DNSTAP_ENABLE STRING
	{
		OUTYY(("P(dt_dnstap_enable:%s)\n", $2));
		if(strcmp($2, "yes") != 0 && strcmp($2, "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->opt->dnstap_enable = (strcmp($2, "yes")==0);
	}
	;
dt_dnstap_socket_path: VAR_DNSTAP_SOCKET_PATH STRING
	{
		OUTYY(("P(dt_dnstap_socket_path:%s)\n", $2));
		cfg_parser->opt->dnstap_socket_path = region_strdup(cfg_parser->opt->region, $2);
	}
	;
dt_dnstap_send_identity: VAR_DNSTAP_SEND_IDENTITY STRING
	{
		OUTYY(("P(dt_dnstap_send_identity:%s)\n", $2));
		if(strcmp($2, "yes") != 0 && strcmp($2, "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->opt->dnstap_send_identity = (strcmp($2, "yes")==0);
	}
	;
dt_dnstap_send_version: VAR_DNSTAP_SEND_VERSION STRING
	{
		OUTYY(("P(dt_dnstap_send_version:%s)\n", $2));
		if(strcmp($2, "yes") != 0 && strcmp($2, "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->opt->dnstap_send_version = (strcmp($2, "yes")==0);
	}
	;
dt_dnstap_identity: VAR_DNSTAP_IDENTITY STRING
	{
		OUTYY(("P(dt_dnstap_identity:%s)\n", $2));
		cfg_parser->opt->dnstap_identity = region_strdup(cfg_parser->opt->region, $2);
	}
	;
dt_dnstap_version: VAR_DNSTAP_VERSION STRING
	{
		OUTYY(("P(dt_dnstap_version:%s)\n", $2));
		cfg_parser->opt->dnstap_version = region_strdup(cfg_parser->opt->region, $2);
	}
	;
dt_dnstap_log_auth_query_messages: VAR_DNSTAP_LOG_AUTH_QUERY_MESSAGES STRING
	{
		OUTYY(("P(dt_dnstap_log_auth_query_messages:%s)\n", $2));
		if(strcmp($2, "yes") != 0 && strcmp($2, "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->opt->dnstap_log_auth_query_messages = (strcmp($2, "yes")==0);
	}
	;
dt_dnstap_log_auth_response_messages: VAR_DNSTAP_LOG_AUTH_RESPONSE_MESSAGES STRING
	{
		OUTYY(("P(dt_dnstap_log_auth_response_messages:%s)\n", $2));
		if(strcmp($2, "yes") != 0 && strcmp($2, "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->opt->dnstap_log_auth_response_messages = (strcmp($2, "yes")==0);
	}
	;

/* pattern: declaration */
patternstart: VAR_PATTERN
	{ 
		OUTYY(("\nP(pattern:)\n")); 
		if(cfg_parser->current_zone) {
			if(!cfg_parser->current_zone->name) 
				c_error("previous zone has no name");
			else {
				if(!nsd_options_insert_zone(cfg_parser->opt, 
					cfg_parser->current_zone))
					c_error("duplicate zone");
			}
			if(!cfg_parser->current_zone->pattern) 
				c_error("previous zone has no pattern");
			cfg_parser->current_zone = NULL;
		}
		if(cfg_parser->current_pattern) {
			if(!cfg_parser->current_pattern->pname) 
				c_error("previous pattern has no name");
			else {
				if(!nsd_options_insert_pattern(cfg_parser->opt, 
					cfg_parser->current_pattern))
					c_error_msg("duplicate pattern %s",
						cfg_parser->current_pattern->pname);
			}
		}
		cfg_parser->current_pattern = pattern_options_create(
			cfg_parser->opt->region);
		cfg_parser->current_allow_notify = 0;
		cfg_parser->current_request_xfr = 0;
		cfg_parser->current_notify = 0;
		cfg_parser->current_provide_xfr = 0;
		cfg_parser->current_outgoing_interface = 0;
	}
	;
contents_pattern: contents_pattern content_pattern | content_pattern;
content_pattern: pattern_name | zone_config_item;
zone_config_item: zone_zonefile | zone_allow_notify | zone_request_xfr |
	zone_notify | zone_notify_retry | zone_provide_xfr | 
	zone_outgoing_interface | zone_allow_axfr_fallback | include_pattern |
	zone_rrl_whitelist | zone_zonestats | zone_max_refresh_time |
	zone_min_refresh_time | zone_max_retry_time | zone_min_retry_time |
       zone_size_limit_xfr | zone_multi_master_check;
pattern_name: VAR_NAME STRING
	{ 
		OUTYY(("P(pattern_name:%s)\n", $2)); 
#ifndef NDEBUG
		assert(cfg_parser->current_pattern);
#endif
		if(strchr($2, ' '))
			c_error_msg("space is not allowed in pattern name: "
				"'%s'", $2);
		cfg_parser->current_pattern->pname = region_strdup(cfg_parser->opt->region, $2);
	}
	;
include_pattern: VAR_INCLUDEPATTERN STRING
	{
		OUTYY(("P(include-pattern:%s)\n", $2)); 
#ifndef NDEBUG
		assert(cfg_parser->current_pattern);
#endif
		config_apply_pattern($2);
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
			if(!cfg_parser->current_zone->pattern) 
				c_error("previous zone has no pattern");
		}
		if(cfg_parser->current_pattern) {
			if(!cfg_parser->current_pattern->pname) 
				c_error("previous pattern has no name");
			else {
				if(!nsd_options_insert_pattern(cfg_parser->opt, 
					cfg_parser->current_pattern))
					c_error_msg("duplicate pattern %s",
						cfg_parser->current_pattern->pname);
			}
		}
		cfg_parser->current_zone = zone_options_create(cfg_parser->opt->region);
		cfg_parser->current_zone->part_of_config = 1;
		cfg_parser->current_pattern = pattern_options_create(
			cfg_parser->opt->region);
		cfg_parser->current_pattern->implicit = 1;
		cfg_parser->current_zone->pattern = cfg_parser->current_pattern;
		cfg_parser->current_allow_notify = 0;
		cfg_parser->current_request_xfr = 0;
		cfg_parser->current_notify = 0;
		cfg_parser->current_provide_xfr = 0;
		cfg_parser->current_outgoing_interface = 0;
	}
	;
contents_zone: contents_zone content_zone | content_zone;
content_zone: zone_name | zone_config_item;
zone_name: VAR_NAME STRING
	{ 
		char* s;
		OUTYY(("P(zone_name:%s)\n", $2)); 
#ifndef NDEBUG
		assert(cfg_parser->current_zone);
		assert(cfg_parser->current_pattern);
#endif
		cfg_parser->current_zone->name = region_strdup(cfg_parser->opt->region, $2);
		s = (char*)region_alloc(cfg_parser->opt->region,
			strlen($2)+strlen(PATTERN_IMPLICIT_MARKER)+1);
		memmove(s, PATTERN_IMPLICIT_MARKER,
			strlen(PATTERN_IMPLICIT_MARKER));
		memmove(s+strlen(PATTERN_IMPLICIT_MARKER), $2, strlen($2)+1);
		if(pattern_options_find(cfg_parser->opt, s))
			c_error_msg("zone %s cannot be created because "
				"implicit pattern %s already exists", $2, s);
		cfg_parser->current_pattern->pname = s;
	}
	;
zone_zonefile: VAR_ZONEFILE STRING
	{ 
		OUTYY(("P(zonefile:%s)\n", $2)); 
#ifndef NDEBUG
		assert(cfg_parser->current_pattern);
#endif
		cfg_parser->current_pattern->zonefile = region_strdup(cfg_parser->opt->region, $2);
	}
	;
zone_zonestats: VAR_ZONESTATS STRING
	{ 
		OUTYY(("P(zonestats:%s)\n", $2)); 
#ifndef NDEBUG
		assert(cfg_parser->current_pattern);
#endif
		cfg_parser->current_pattern->zonestats = region_strdup(cfg_parser->opt->region, $2);
	}
	;
zone_allow_notify: VAR_ALLOW_NOTIFY STRING STRING
	{ 
		acl_options_type* acl = parse_acl_info(cfg_parser->opt->region, $2, $3);
		OUTYY(("P(allow_notify:%s %s)\n", $2, $3)); 
		if(cfg_parser->current_allow_notify)
			cfg_parser->current_allow_notify->next = acl;
		else
			cfg_parser->current_pattern->allow_notify = acl;
		cfg_parser->current_allow_notify = acl;
	}
	;
zone_request_xfr: VAR_REQUEST_XFR zone_request_xfr_data
	{
	}
	;
zone_size_limit_xfr: VAR_SIZE_LIMIT_XFR STRING
	{ 
		OUTYY(("P(size_limit_xfr:%s)\n", $2)); 
		if(atoll($2) < 0)
			yyerror("number >= 0 expected");
		else cfg_parser->current_pattern->size_limit_xfr = atoll($2);
	}
	;
zone_request_xfr_data: STRING STRING
	{ 
		acl_options_type* acl = parse_acl_info(cfg_parser->opt->region, $1, $2);
		OUTYY(("P(request_xfr:%s %s)\n", $1, $2)); 
		if(acl->blocked) c_error("blocked address used for request-xfr");
		if(acl->rangetype!=acl_range_single) c_error("address range used for request-xfr");
		if(cfg_parser->current_request_xfr)
			cfg_parser->current_request_xfr->next = acl;
		else
			cfg_parser->current_pattern->request_xfr = acl;
		cfg_parser->current_request_xfr = acl;
	}
	| VAR_AXFR STRING STRING
	{ 
		acl_options_type* acl = parse_acl_info(cfg_parser->opt->region, $2, $3);
		acl->use_axfr_only = 1;
		OUTYY(("P(request_xfr:%s %s)\n", $2, $3)); 
		if(acl->blocked) c_error("blocked address used for request-xfr");
		if(acl->rangetype!=acl_range_single) c_error("address range used for request-xfr");
		if(cfg_parser->current_request_xfr)
			cfg_parser->current_request_xfr->next = acl;
		else
			cfg_parser->current_pattern->request_xfr = acl;
		cfg_parser->current_request_xfr = acl;
	}
	| VAR_UDP STRING STRING
	{ 
		acl_options_type* acl = parse_acl_info(cfg_parser->opt->region, $2, $3);
		acl->allow_udp = 1;
		OUTYY(("P(request_xfr:%s %s)\n", $2, $3)); 
		if(acl->blocked) c_error("blocked address used for request-xfr");
		if(acl->rangetype!=acl_range_single) c_error("address range used for request-xfr");
		if(cfg_parser->current_request_xfr)
			cfg_parser->current_request_xfr->next = acl;
		else
			cfg_parser->current_pattern->request_xfr = acl;
		cfg_parser->current_request_xfr = acl;
	}
	;
zone_notify: VAR_NOTIFY STRING STRING
	{ 
		acl_options_type* acl = parse_acl_info(cfg_parser->opt->region, $2, $3);
		OUTYY(("P(notify:%s %s)\n", $2, $3)); 
		if(acl->blocked) c_error("blocked address used for notify");
		if(acl->rangetype!=acl_range_single) c_error("address range used for notify");
		if(cfg_parser->current_notify)
			cfg_parser->current_notify->next = acl;
		else
			cfg_parser->current_pattern->notify = acl;
		cfg_parser->current_notify = acl;
	}
	;
zone_notify_retry: VAR_NOTIFY_RETRY STRING
	{ 
		OUTYY(("P(notify_retry:%s)\n", $2)); 
		if(atoi($2) == 0 && strcmp($2, "0") != 0)
			yyerror("number expected");
		else {
			cfg_parser->current_pattern->notify_retry = atoi($2);
			cfg_parser->current_pattern->notify_retry_is_default=0;
		}
	}
	;
zone_provide_xfr: VAR_PROVIDE_XFR STRING STRING
	{ 
		acl_options_type* acl = parse_acl_info(cfg_parser->opt->region, $2, $3);
		OUTYY(("P(provide_xfr:%s %s)\n", $2, $3)); 
		if(cfg_parser->current_provide_xfr)
			cfg_parser->current_provide_xfr->next = acl;
		else
			cfg_parser->current_pattern->provide_xfr = acl;
		cfg_parser->current_provide_xfr = acl;
	}
	;
zone_outgoing_interface: VAR_OUTGOING_INTERFACE STRING
	{ 
		acl_options_type* acl = parse_acl_info(cfg_parser->opt->region, $2, "NOKEY");
		OUTYY(("P(outgoing_interface:%s)\n", $2)); 
		if(acl->rangetype!=acl_range_single) c_error("address range used for outgoing interface");
		if(cfg_parser->current_outgoing_interface)
			cfg_parser->current_outgoing_interface->next = acl;
		else
			cfg_parser->current_pattern->outgoing_interface = acl;
		cfg_parser->current_outgoing_interface = acl;
	}
	;
zone_allow_axfr_fallback: VAR_ALLOW_AXFR_FALLBACK STRING 
	{ 
		OUTYY(("P(allow_axfr_fallback:%s)\n", $2)); 
		if(strcmp($2, "yes") != 0 && strcmp($2, "no") != 0)
			yyerror("expected yes or no.");
		else {
			cfg_parser->current_pattern->allow_axfr_fallback = (strcmp($2, "yes")==0);
			cfg_parser->current_pattern->allow_axfr_fallback_is_default = 0;
		}
	}
	;
zone_rrl_whitelist: VAR_RRL_WHITELIST STRING
	{ 
		OUTYY(("P(zone_rrl_whitelist:%s)\n", $2)); 
#ifdef RATELIMIT
		cfg_parser->current_pattern->rrl_whitelist |= rrlstr2type($2);
#endif
	}
	;
zone_max_refresh_time: VAR_MAX_REFRESH_TIME STRING
{
	OUTYY(("P(zone_max_refresh_time:%s)\n", $2));
	if(atoi($2) == 0 && strcmp($2, "0") != 0)
		yyerror("number expected");
	else {
		cfg_parser->current_pattern->max_refresh_time = atoi($2);
		cfg_parser->current_pattern->max_refresh_time_is_default = 0;
	}
};
zone_min_refresh_time: VAR_MIN_REFRESH_TIME STRING
{
	OUTYY(("P(zone_min_refresh_time:%s)\n", $2));
	if(atoi($2) == 0 && strcmp($2, "0") != 0)
		yyerror("number expected");
	else {
		cfg_parser->current_pattern->min_refresh_time = atoi($2);
		cfg_parser->current_pattern->min_refresh_time_is_default = 0;
	}
};
zone_max_retry_time: VAR_MAX_RETRY_TIME STRING
{
	OUTYY(("P(zone_max_retry_time:%s)\n", $2));
	if(atoi($2) == 0 && strcmp($2, "0") != 0)
		yyerror("number expected");
	else {
		cfg_parser->current_pattern->max_retry_time = atoi($2);
		cfg_parser->current_pattern->max_retry_time_is_default = 0;
	}
};
zone_min_retry_time: VAR_MIN_RETRY_TIME STRING
{
	OUTYY(("P(zone_min_retry_time:%s)\n", $2));
	if(atoi($2) == 0 && strcmp($2, "0") != 0)
		yyerror("number expected");
	else {
		cfg_parser->current_pattern->min_retry_time = atoi($2);
		cfg_parser->current_pattern->min_retry_time_is_default = 0;
	}
};
zone_multi_master_check: VAR_MULTI_MASTER_CHECK STRING
	{
		OUTYY(("P(zone_multi_master_check:%s)\n", $2));
		if(strcmp($2, "yes") != 0 && strcmp($2, "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->current_pattern->multi_master_check = (strcmp($2, "yes")==0);
	}

/* key: declaration */
keystart: VAR_KEY
	{ 
		OUTYY(("\nP(key:)\n")); 
		if(cfg_parser->current_key) {
			if(!cfg_parser->current_key->name) c_error("previous key has no name");
			if(!cfg_parser->current_key->algorithm) c_error("previous key has no algorithm");
			if(!cfg_parser->current_key->secret) c_error("previous key has no secret blob");
			key_options_insert(cfg_parser->opt, cfg_parser->current_key);
		}
		cfg_parser->current_key = key_options_create(cfg_parser->opt->region);
		cfg_parser->current_key->algorithm = region_strdup(cfg_parser->opt->region, "sha256");
	}
	;
contents_key: contents_key content_key | content_key;
content_key: key_name | key_algorithm | key_secret;
key_name: VAR_NAME STRING
	{ 
		const dname_type* d;
		OUTYY(("P(key_name:%s)\n", $2)); 
#ifndef NDEBUG
		assert(cfg_parser->current_key);
#endif
		cfg_parser->current_key->name = region_strdup(cfg_parser->opt->region, $2);
		d = dname_parse(cfg_parser->opt->region, $2);
		if(!d)	c_error_msg("Failed to parse tsig key name %s", $2);
		else	region_recycle(cfg_parser->opt->region, (void*)d,
				dname_total_size(d));
	}
	;
key_algorithm: VAR_ALGORITHM STRING
	{ 
		OUTYY(("P(key_algorithm:%s)\n", $2)); 
#ifndef NDEBUG
		assert(cfg_parser->current_key);
#endif
		if(cfg_parser->current_key->algorithm)
			region_recycle(cfg_parser->opt->region, cfg_parser->current_key->algorithm, strlen(cfg_parser->current_key->algorithm)+1);
		cfg_parser->current_key->algorithm = region_strdup(cfg_parser->opt->region, $2);
		if(tsig_get_algorithm_by_name($2) == NULL)
			c_error_msg("Bad tsig algorithm %s", $2);
	}
	;
key_secret: VAR_SECRET STRING
	{ 
		uint8_t data[16384];
		int size;
		OUTYY(("key_secret:%s)\n", $2)); 
#ifndef NDEBUG
		assert(cfg_parser->current_key);
#endif
		cfg_parser->current_key->secret = region_strdup(cfg_parser->opt->region, $2);
		size = __b64_pton($2, data, sizeof(data));
		if(size == -1) {
			c_error_msg("Cannot base64 decode tsig secret %s",
				cfg_parser->current_key->name?
				cfg_parser->current_key->name:"");
		} else if(size != 0) {
			memset(data, 0xdd, size); /* wipe secret */
		}
	}
	;

%%

/* parse helper routines could be here */
