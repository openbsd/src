%{
/*
 * configlexer.lex - lexical analyzer for NSD config file
 *
 * Copyright (c) 2001-2006, NLnet Labs. All rights reserved
 *
 * See LICENSE for the license.
 *
 */

#include "config.h"

#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <strings.h>

#include "options.h"
#include "configyyrename.h"
#include "configparser.h"
void c_error(const char *message);

#if 0
#define LEXOUT(s)  printf s /* used ONLY when debugging */
#else
#define LEXOUT(s)
#endif

struct inc_state {
	const char* filename;
	int line;
};
static struct inc_state parse_stack[MAXINCLUDES];
static YY_BUFFER_STATE include_stack[MAXINCLUDES];
static int config_include_stack_ptr = 0;

static void config_start_include(const char* filename)
{
	FILE *input;
	if(strlen(filename) == 0) {
		c_error_msg("empty include file name");
		return;
	}
	if(config_include_stack_ptr >= MAXINCLUDES) {
		c_error_msg("includes nested too deeply, skipped (>%d)", MAXINCLUDES);
		return;
	}
	if (cfg_parser->chroot) {
		int l = strlen(cfg_parser->chroot); /* chroot has trailing slash */
		if (strncmp(cfg_parser->chroot, filename, l) != 0) {
			c_error_msg("include file '%s' is not relative to chroot '%s'",
				filename, cfg_parser->chroot);
			return;
		}
		filename += l - 1; /* strip chroot without trailing slash */
	}
	input = fopen(filename, "r");
	if(!input) {
		c_error_msg("cannot open include file '%s': %s",
			filename, strerror(errno));
		return;
	}
	LEXOUT(("switch_to_include_file(%s) ", filename));
	parse_stack[config_include_stack_ptr].filename = cfg_parser->filename;
	parse_stack[config_include_stack_ptr].line = cfg_parser->line;
	include_stack[config_include_stack_ptr] = YY_CURRENT_BUFFER;
	cfg_parser->filename = region_strdup(cfg_parser->opt->region, filename);
	cfg_parser->line = 1;
	yy_switch_to_buffer(yy_create_buffer(input, YY_BUF_SIZE));
	++config_include_stack_ptr;
}

static void config_end_include(void)
{
	--config_include_stack_ptr;
	cfg_parser->filename = parse_stack[config_include_stack_ptr].filename;
	cfg_parser->line = parse_stack[config_include_stack_ptr].line;
	yy_delete_buffer(YY_CURRENT_BUFFER);
	yy_switch_to_buffer(include_stack[config_include_stack_ptr]);
}

#ifndef yy_set_bol /* compat definition, for flex 2.4.6 */
#define yy_set_bol(at_bol) \
        { \
	        if ( ! yy_current_buffer ) \
	                yy_current_buffer = yy_create_buffer( yyin, YY_BUF_SIZE ); \
	        yy_current_buffer->yy_ch_buf[0] = ((at_bol)?'\n':' '); \
        }
#endif

%}
%option noinput
%option nounput
%{
#ifndef YY_NO_UNPUT
#define YY_NO_UNPUT 1
#endif
#ifndef YY_NO_INPUT
#define YY_NO_INPUT 1
#endif
%}

SPACE   [ \t]
LETTER  [a-zA-Z]
UNQUOTEDLETTER [^\"\n\r \t\\]|\\.
NEWLINE [\r\n]
COMMENT \#
COLON 	\:
ANY     [^\"\n\r\\]|\\.

%x	quotedstring include include_quoted

%%
{SPACE}* 		{ LEXOUT(("SP ")); /* ignore */ }
{SPACE}*{COMMENT}.* 	{ LEXOUT(("comment(%s) ", yytext)); /* ignore */ }
server{COLON}		{ LEXOUT(("v(%s) ", yytext)); return VAR_SERVER;}
name{COLON}		{ LEXOUT(("v(%s) ", yytext)); return VAR_NAME;}
ip-address{COLON}	{ LEXOUT(("v(%s) ", yytext)); return VAR_IP_ADDRESS;}
interface{COLON}	{ LEXOUT(("v(%s) ", yytext)); return VAR_IP_ADDRESS;}
ip-transparent{COLON}	{ LEXOUT(("v(%s) ", yytext)); return VAR_IP_TRANSPARENT;}
debug-mode{COLON}	{ LEXOUT(("v(%s) ", yytext)); return VAR_DEBUG_MODE;}
hide-version{COLON}	{ LEXOUT(("v(%s) ", yytext)); return VAR_HIDE_VERSION;}
ip4-only{COLON}		{ LEXOUT(("v(%s) ", yytext)); return VAR_IP4_ONLY;}
ip6-only{COLON}		{ LEXOUT(("v(%s) ", yytext)); return VAR_IP6_ONLY;}
do-ip4{COLON}		{ LEXOUT(("v(%s) ", yytext)); return VAR_DO_IP4;}
do-ip6{COLON}		{ LEXOUT(("v(%s) ", yytext)); return VAR_DO_IP6;}
database{COLON}		{ LEXOUT(("v(%s) ", yytext)); return VAR_DATABASE;}
identity{COLON}		{ LEXOUT(("v(%s) ", yytext)); return VAR_IDENTITY;}
nsid{COLON}		{ LEXOUT(("v(%s) ", yytext)); return VAR_NSID;}
logfile{COLON}		{ LEXOUT(("v(%s) ", yytext)); return VAR_LOGFILE;}
server-count{COLON}	{ LEXOUT(("v(%s) ", yytext)); return VAR_SERVER_COUNT;}
tcp-count{COLON}	{ LEXOUT(("v(%s) ", yytext)); return VAR_TCP_COUNT;}
tcp-query-count{COLON}	{ LEXOUT(("v(%s) ", yytext)); return VAR_TCP_QUERY_COUNT;}
tcp-timeout{COLON}	{ LEXOUT(("v(%s) ", yytext)); return VAR_TCP_TIMEOUT;}
ipv4-edns-size{COLON}	{ LEXOUT(("v(%s) ", yytext)); return VAR_IPV4_EDNS_SIZE;}
ipv6-edns-size{COLON}	{ LEXOUT(("v(%s) ", yytext)); return VAR_IPV6_EDNS_SIZE;}
pidfile{COLON}		{ LEXOUT(("v(%s) ", yytext)); return VAR_PIDFILE;}
port{COLON}		{ LEXOUT(("v(%s) ", yytext)); return VAR_PORT;}
statistics{COLON}	{ LEXOUT(("v(%s) ", yytext)); return VAR_STATISTICS;}
chroot{COLON}		{ LEXOUT(("v(%s) ", yytext)); return VAR_CHROOT;}
username{COLON}		{ LEXOUT(("v(%s) ", yytext)); return VAR_USERNAME;}
zonesdir{COLON}		{ LEXOUT(("v(%s) ", yytext)); return VAR_ZONESDIR;}
zonelistfile{COLON}	{ LEXOUT(("v(%s) ", yytext)); return VAR_ZONELISTFILE;}
difffile{COLON}		{ LEXOUT(("v(%s) ", yytext)); return VAR_DIFFFILE;}
xfrdfile{COLON}		{ LEXOUT(("v(%s) ", yytext)); return VAR_XFRDFILE;}
xfrdir{COLON}		{ LEXOUT(("v(%s) ", yytext)); return VAR_XFRDIR;}
xfrd-reload-timeout{COLON}	{ LEXOUT(("v(%s) ", yytext)); return VAR_XFRD_RELOAD_TIMEOUT;}
verbosity{COLON}	{ LEXOUT(("v(%s) ", yytext)); return VAR_VERBOSITY;}
zone{COLON}		{ LEXOUT(("v(%s) ", yytext)); return VAR_ZONE;}
zonefile{COLON}		{ LEXOUT(("v(%s) ", yytext)); return VAR_ZONEFILE;}
allow-notify{COLON}	{ LEXOUT(("v(%s) ", yytext)); return VAR_ALLOW_NOTIFY;}
request-xfr{COLON}	{ LEXOUT(("v(%s) ", yytext)); return VAR_REQUEST_XFR;}
notify{COLON}		{ LEXOUT(("v(%s) ", yytext)); return VAR_NOTIFY;}
notify-retry{COLON}	{ LEXOUT(("v(%s) ", yytext)); return VAR_NOTIFY_RETRY;}
provide-xfr{COLON}	{ LEXOUT(("v(%s) ", yytext)); return VAR_PROVIDE_XFR;}
outgoing-interface{COLON}	{ LEXOUT(("v(%s) ", yytext)); return VAR_OUTGOING_INTERFACE;}
allow-axfr-fallback{COLON}	{ LEXOUT(("v(%s) ", yytext)); return VAR_ALLOW_AXFR_FALLBACK;}
key{COLON}		{ LEXOUT(("v(%s) ", yytext)); return VAR_KEY;}
algorithm{COLON}	{ LEXOUT(("v(%s) ", yytext)); return VAR_ALGORITHM;}
secret{COLON}		{ LEXOUT(("v(%s) ", yytext)); return VAR_SECRET;}
pattern{COLON}		{ LEXOUT(("v(%s) ", yytext)); return VAR_PATTERN;}
include-pattern{COLON}	{ LEXOUT(("v(%s) ", yytext)); return VAR_INCLUDEPATTERN;}
remote-control{COLON}	{ LEXOUT(("v(%s) ", yytext)); return VAR_REMOTE_CONTROL;}
control-enable{COLON}	{ LEXOUT(("v(%s) ", yytext)); return VAR_CONTROL_ENABLE;}
control-interface{COLON}	{ LEXOUT(("v(%s) ", yytext)); return VAR_CONTROL_INTERFACE;}
control-port{COLON}	{ LEXOUT(("v(%s) ", yytext)); return VAR_CONTROL_PORT;}
server-key-file{COLON}	{ LEXOUT(("v(%s) ", yytext)); return VAR_SERVER_KEY_FILE;}
server-cert-file{COLON}	{ LEXOUT(("v(%s) ", yytext)); return VAR_SERVER_CERT_FILE;}
control-key-file{COLON}	{ LEXOUT(("v(%s) ", yytext)); return VAR_CONTROL_KEY_FILE;}
control-cert-file{COLON}	{ LEXOUT(("v(%s) ", yytext)); return VAR_CONTROL_CERT_FILE;}
AXFR			{ LEXOUT(("v(%s) ", yytext)); return VAR_AXFR;}
UDP			{ LEXOUT(("v(%s) ", yytext)); return VAR_UDP;}
rrl-size{COLON}		{ LEXOUT(("v(%s) ", yytext)); return VAR_RRL_SIZE;}
rrl-ratelimit{COLON}	{ LEXOUT(("v(%s) ", yytext)); return VAR_RRL_RATELIMIT;}
rrl-slip{COLON}		{ LEXOUT(("v(%s) ", yytext)); return VAR_RRL_SLIP;}
rrl-ipv4-prefix-length{COLON}	{ LEXOUT(("v(%s) ", yytext)); return VAR_RRL_IPV4_PREFIX_LENGTH;}
rrl-ipv6-prefix-length{COLON}	{ LEXOUT(("v(%s) ", yytext)); return VAR_RRL_IPV6_PREFIX_LENGTH;}
rrl-whitelist-ratelimit{COLON}	{ LEXOUT(("v(%s) ", yytext)); return VAR_RRL_WHITELIST_RATELIMIT;}
rrl-whitelist{COLON}	{ LEXOUT(("v(%s) ", yytext)); return VAR_RRL_WHITELIST;}
zonefiles-check{COLON}	{ LEXOUT(("v(%s) ", yytext)); return VAR_ZONEFILES_CHECK;}
{NEWLINE}		{ LEXOUT(("NL\n")); cfg_parser->line++;}

	/* Quoted strings. Strip leading and ending quotes */
\"			{ BEGIN(quotedstring); LEXOUT(("QS ")); }
<quotedstring><<EOF>>   {
        yyerror("EOF inside quoted string");
        BEGIN(INITIAL);
}
<quotedstring>{ANY}*    { LEXOUT(("STR(%s) ", yytext)); yymore(); }
<quotedstring>\n        { cfg_parser->line++; yymore(); }
<quotedstring>\" {
        LEXOUT(("QE "));
        BEGIN(INITIAL);
        yytext[yyleng - 1] = '\0';
	yylval.str = region_strdup(cfg_parser->opt->region, yytext);
        return STRING;
}

	/* include: directive */
include{COLON}		{ LEXOUT(("v(%s) ", yytext)); BEGIN(include); }
<include><<EOF>>	{
        yyerror("EOF inside include directive");
        BEGIN(INITIAL);
}
<include>{SPACE}*	{ LEXOUT(("ISP ")); /* ignore */ }
<include>{NEWLINE}	{ LEXOUT(("NL\n")); cfg_parser->line++;}
<include>\"		{ LEXOUT(("IQS ")); BEGIN(include_quoted); }
<include>{UNQUOTEDLETTER}*	{
	LEXOUT(("Iunquotedstr(%s) ", yytext));
	config_start_include(yytext);
	BEGIN(INITIAL);
}
<include_quoted><<EOF>>	{
        yyerror("EOF inside quoted string");
        BEGIN(INITIAL);
}
<include_quoted>{ANY}*	{ LEXOUT(("ISTR(%s) ", yytext)); yymore(); }
<include_quoted>{NEWLINE}	{ cfg_parser->line++; yymore(); }
<include_quoted>\"	{
	LEXOUT(("IQE "));
	yytext[yyleng - 1] = '\0';
	config_start_include(yytext);
	BEGIN(INITIAL);
}
<INITIAL><<EOF>>	{
	yy_set_bol(1); /* Set beginning of line, so "^" rules match.  */
	if (config_include_stack_ptr == 0) {
		yyterminate();
	} else {
		fclose(yyin);
		config_end_include();
	}
}

{UNQUOTEDLETTER}*	{ LEXOUT(("unquotedstr(%s) ", yytext)); 
			yylval.str = region_strdup(cfg_parser->opt->region, yytext); return STRING; }

%%
