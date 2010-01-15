%{
/*
 * zyparser.y -- yacc grammar for (DNS) zone files
 *
 * Copyright (c) 2001-2006, NLnet Labs. All rights reserved.
 *
 * See LICENSE for the license.
 *
 */

#include <config.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "dname.h"
#include "namedb.h"
#include "zonec.h"

/* these need to be global, otherwise they cannot be used inside yacc */
zparser_type *parser;

#ifdef __cplusplus
extern "C"
#endif /* __cplusplus */
int yywrap(void);

/* this hold the nxt bits */
static uint8_t nxtbits[16];
static int dlv_warn = 1;

/* 256 windows of 256 bits (32 bytes) */
/* still need to reset the bastard somewhere */
static uint8_t nsecbits[NSEC_WINDOW_COUNT][NSEC_WINDOW_BITS_SIZE];

/* hold the highest rcode seen in a NSEC rdata , BUG #106 */
uint16_t nsec_highest_rcode;

void yyerror(const char *message);

#ifdef NSEC3
/* parse nsec3 parameters and add the (first) rdata elements */
static void
nsec3_add_params(const char* hash_algo_str, const char* flag_str,
	const char* iter_str, const char* salt_str, int salt_len);
#endif /* NSEC3 */

%}
%union {
	domain_type	 *domain;
	const dname_type *dname;
	struct lex_data	  data;
	uint32_t	  ttl;
	uint16_t	  klass;
	uint16_t	  type;
	uint16_t	 *unknown;
}

/*
 * Tokens to represent the known RR types of DNS.
 */
%token <type> T_A T_NS T_MX T_TXT T_CNAME T_AAAA T_PTR T_NXT T_KEY T_SOA T_SIG
%token <type> T_SRV T_CERT T_LOC T_MD T_MF T_MB T_MG T_MR T_NULL T_WKS T_HINFO
%token <type> T_MINFO T_RP T_AFSDB T_X25 T_ISDN T_RT T_NSAP T_NSAP_PTR T_PX
%token <type> T_GPOS T_EID T_NIMLOC T_ATMA T_NAPTR T_KX T_A6 T_DNAME T_SINK
%token <type> T_OPT T_APL T_UINFO T_UID T_GID T_UNSPEC T_TKEY T_TSIG T_IXFR
%token <type> T_AXFR T_MAILB T_MAILA T_DS T_DLV T_SSHFP T_RRSIG T_NSEC T_DNSKEY
%token <type> T_SPF T_NSEC3 T_IPSECKEY T_DHCID T_NSEC3PARAM

/* other tokens */
%token	       DOLLAR_TTL DOLLAR_ORIGIN NL SP
%token <data>  STR PREV BITLAB
%token <ttl>   T_TTL
%token <klass> T_RRCLASS

/* unknown RRs */
%token	       URR
%token <type>  T_UTYPE

%type <type>	type_and_rdata
%type <domain>	owner dname abs_dname
%type <dname>	rel_dname label
%type <data>	wire_dname wire_abs_dname wire_rel_dname wire_label
%type <data>	concatenated_str_seq str_sp_seq str_dot_seq dotted_str
%type <data>	nxt_seq nsec_more
%type <unknown> rdata_unknown

%%
lines:	/* empty file */
    |	lines line
    ;

line:	NL
    |	sp NL
    |	PREV NL		{}    /* Lines containing only whitespace.  */
    |	ttl_directive
	{
	    parser->error_occurred = 0;
    }
    |	origin_directive
	{
	    parser->error_occurred = 0;
    }
    |	rr
    {	/* rr should be fully parsed */
	    if (!parser->error_occurred) {
			    parser->current_rr.rdatas
				    = (rdata_atom_type *) region_alloc_init(
					    parser->region,
					    parser->current_rr.rdatas,
					    (parser->current_rr.rdata_count
					     * sizeof(rdata_atom_type)));

			    process_rr();
	    }

	    region_free_all(parser->rr_region);

	    parser->current_rr.type = 0;
	    parser->current_rr.rdata_count = 0;
	    parser->current_rr.rdatas = parser->temporary_rdatas;
	    parser->error_occurred = 0;
    }
    |	error NL
    ;

/* needed to cope with ( and ) in arbitary places */
sp:	SP
    |	sp SP
    ;

trail:	NL
    |	sp NL
    ;

ttl_directive:	DOLLAR_TTL sp STR trail
    {
	    parser->default_ttl = zparser_ttl2int($3.str, &(parser->error_occurred));
	    if (parser->error_occurred == 1) {
		    parser->default_ttl = DEFAULT_TTL;
			parser->error_occurred = 0;
	    }
    }
    ;

origin_directive:	DOLLAR_ORIGIN sp abs_dname trail
    {
	    parser->origin = $3;
    }
    |	DOLLAR_ORIGIN sp rel_dname trail
    {
	    zc_error_prev_line("$ORIGIN directive requires absolute domain name");
    }
    ;

rr:	owner classttl type_and_rdata
    {
	    parser->current_rr.owner = $1;
	    parser->current_rr.type = $3;
    }
    ;

owner:	dname sp
    {
	    parser->prev_dname = $1;
	    $$ = $1;
    }
    |	PREV
    {
	    $$ = parser->prev_dname;
    }
    ;

classttl:	/* empty - fill in the default, def. ttl and IN class */
    {
	    parser->current_rr.ttl = parser->default_ttl;
	    parser->current_rr.klass = parser->default_class;
    }
    |	T_RRCLASS sp		/* no ttl */
    {
	    parser->current_rr.ttl = parser->default_ttl;
	    parser->current_rr.klass = $1;
    }
    |	T_TTL sp		/* no class */
    {
	    parser->current_rr.ttl = $1;
	    parser->current_rr.klass = parser->default_class;
    }
    |	T_TTL sp T_RRCLASS sp	/* the lot */
    {
	    parser->current_rr.ttl = $1;
	    parser->current_rr.klass = $3;
    }
    |	T_RRCLASS sp T_TTL sp	/* the lot - reversed */
    {
	    parser->current_rr.ttl = $3;
	    parser->current_rr.klass = $1;
    }
    ;

dname:	abs_dname
    |	rel_dname
    {
	    if ($1 == error_dname) {
		    $$ = error_domain;
	    } else if ($1->name_size + domain_dname(parser->origin)->name_size - 1 > MAXDOMAINLEN) {
		    zc_error("domain name exceeds %d character limit", MAXDOMAINLEN);
		    $$ = error_domain;
	    } else {
		    $$ = domain_table_insert(
			    parser->db->domains,
			    dname_concatenate(
				    parser->rr_region,
				    $1,
				    domain_dname(parser->origin)));
	    }
    }
    ;

abs_dname:	'.'
    {
	    $$ = parser->db->domains->root;
    }
    |	'@'
    {
	    $$ = parser->origin;
    }
    |	rel_dname '.'
    {
	    if ($1 != error_dname) {
		    $$ = domain_table_insert(parser->db->domains, $1);
	    } else {
		    $$ = error_domain;
	    }
    }
    ;

label:	STR
    {
	    if ($1.len > MAXLABELLEN) {
		    zc_error("label exceeds %d character limit", MAXLABELLEN);
		    $$ = error_dname;
	    } else {
		    $$ = dname_make_from_label(parser->rr_region,
					       (uint8_t *) $1.str,
					       $1.len);
	    }
    }
    |	BITLAB
    {
	    zc_error("bitlabels are not supported. RFC2673 has status experimental.");
	    $$ = error_dname;
    }
    ;

rel_dname:	label
    |	rel_dname '.' label
    {
	    if ($1 == error_dname || $3 == error_dname) {
		    $$ = error_dname;
	    } else if ($1->name_size + $3->name_size - 1 > MAXDOMAINLEN) {
		    zc_error("domain name exceeds %d character limit",
			     MAXDOMAINLEN);
		    $$ = error_dname;
	    } else {
		    $$ = dname_concatenate(parser->rr_region, $1, $3);
	    }
    }
    ;

/*
 * Some dnames in rdata are handled as opaque blobs
 */

wire_dname:	wire_abs_dname
    |	wire_rel_dname
    ;

wire_abs_dname:	'.'
    {
	    char *result = (char *) region_alloc(parser->rr_region, 2);
	    result[0] = 0;
	    result[1] = '\0';
	    $$.str = result;
	    $$.len = 1;
    }
    |	wire_rel_dname '.'
    {
	    char *result = (char *) region_alloc(parser->rr_region,
						 $1.len + 2);
	    memcpy(result, $1.str, $1.len);
	    result[$1.len] = 0;
	    result[$1.len+1] = '\0';
	    $$.str = result;
	    $$.len = $1.len + 1;
    }
    ;

wire_label:	STR
    {
	    char *result = (char *) region_alloc(parser->rr_region,
						 $1.len + 1);

	    if ($1.len > MAXLABELLEN)
		    zc_error("label exceeds %d character limit", MAXLABELLEN);

	    /* make label anyway */
	    result[0] = $1.len;
	    memcpy(result+1, $1.str, $1.len);

	    $$.str = result;
	    $$.len = $1.len + 1;
    }
    ;

wire_rel_dname:	wire_label
    |	wire_rel_dname '.' wire_label
    {
	    if ($1.len + $3.len - 3 > MAXDOMAINLEN)
		    zc_error("domain name exceeds %d character limit",
			     MAXDOMAINLEN);

	    /* make dname anyway */
	    $$.len = $1.len + $3.len;
	    $$.str = (char *) region_alloc(parser->rr_region, $$.len + 1);
	    memcpy($$.str, $1.str, $1.len);
	    memcpy($$.str + $1.len, $3.str, $3.len);
	    $$.str[$$.len] = '\0';
    }
    ;



str_seq:	STR
    {
	    zadd_rdata_wireformat(zparser_conv_text(parser->region, $1.str, $1.len));
    }
    |	str_seq sp STR
    {
	    zadd_rdata_wireformat(zparser_conv_text(parser->region, $3.str, $3.len));
    }
    ;

/*
 * Generate a single string from multiple STR tokens, separated by
 * spaces or dots.
 */
concatenated_str_seq:	STR
    |	'.'
    {
	    $$.len = 1;
	    $$.str = region_strdup(parser->rr_region, ".");
    }
    |	concatenated_str_seq sp STR
    {
	    $$.len = $1.len + $3.len + 1;
	    $$.str = (char *) region_alloc(parser->rr_region, $$.len + 1);
	    memcpy($$.str, $1.str, $1.len);
	    memcpy($$.str + $1.len, " ", 1);
	    memcpy($$.str + $1.len + 1, $3.str, $3.len);
	    $$.str[$$.len] = '\0';
    }
    |	concatenated_str_seq '.' STR
    {
	    $$.len = $1.len + $3.len + 1;
	    $$.str = (char *) region_alloc(parser->rr_region, $$.len + 1);
	    memcpy($$.str, $1.str, $1.len);
	    memcpy($$.str + $1.len, ".", 1);
	    memcpy($$.str + $1.len + 1, $3.str, $3.len);
	    $$.str[$$.len] = '\0';
    }
    ;

/* used to convert a nxt list of types */
nxt_seq:	STR
    {
	    uint16_t type = rrtype_from_string($1.str);
	    if (type != 0 && type < 128) {
		    set_bit(nxtbits, type);
	    } else {
		    zc_error("bad type %d in NXT record", (int) type);
	    }
    }
    |	nxt_seq sp STR
    {
	    uint16_t type = rrtype_from_string($3.str);
	    if (type != 0 && type < 128) {
		    set_bit(nxtbits, type);
	    } else {
		    zc_error("bad type %d in NXT record", (int) type);
	    }
    }
    ;

nsec_more:	SP nsec_more
    {
    }
    |	NL
    {
    }
    |	STR nsec_seq
    {
	    uint16_t type = rrtype_from_string($1.str);
	    if (type != 0) {
                    if (type > nsec_highest_rcode) {
                            nsec_highest_rcode = type;
                    }
		    set_bitnsec(nsecbits, type);
	    } else {
		    zc_error("bad type %d in NSEC record", (int) type);
	    }
    }
    ;

nsec_seq:	NL
	|	SP nsec_more
	;

/*
 * Sequence of STR tokens separated by spaces.	The spaces are not
 * preserved during concatenation.
 */
str_sp_seq:	STR
    |	str_sp_seq sp STR
    {
	    char *result = (char *) region_alloc(parser->rr_region,
						 $1.len + $3.len + 1);
	    memcpy(result, $1.str, $1.len);
	    memcpy(result + $1.len, $3.str, $3.len);
	    $$.str = result;
	    $$.len = $1.len + $3.len;
	    $$.str[$$.len] = '\0';
    }
    ;

/*
 * Sequence of STR tokens separated by dots.  The dots are not
 * preserved during concatenation.
 */
str_dot_seq:	STR
    |	str_dot_seq '.' STR
    {
	    char *result = (char *) region_alloc(parser->rr_region,
						 $1.len + $3.len + 1);
	    memcpy(result, $1.str, $1.len);
	    memcpy(result + $1.len, $3.str, $3.len);
	    $$.str = result;
	    $$.len = $1.len + $3.len;
	    $$.str[$$.len] = '\0';
    }
    ;

/*
 * A string that can contain dots.
 */
dotted_str:	STR
    |	'.'
    {
	$$.str = ".";
	$$.len = 1;
    }
    |	dotted_str '.'
    {
	    char *result = (char *) region_alloc(parser->rr_region,
						 $1.len + 2);
	    memcpy(result, $1.str, $1.len);
	    result[$1.len] = '.';
	    $$.str = result;
	    $$.len = $1.len + 1;
	    $$.str[$$.len] = '\0';
    }
    |	dotted_str '.' STR
    {
	    char *result = (char *) region_alloc(parser->rr_region,
						 $1.len + $3.len + 2);
	    memcpy(result, $1.str, $1.len);
	    result[$1.len] = '.';
	    memcpy(result + $1.len + 1, $3.str, $3.len);
	    $$.str = result;
	    $$.len = $1.len + $3.len + 1;
	    $$.str[$$.len] = '\0';
    }
    ;

/* define what we can parse */
type_and_rdata:
    /*
     * All supported RR types.	We don't support NULL and types marked obsolete.
     */
    	T_A sp rdata_a
    |	T_A sp rdata_unknown { $$ = $1; parse_unknown_rdata($1, $3); }
    |	T_NS sp rdata_domain_name
    |	T_NS sp rdata_unknown { $$ = $1; parse_unknown_rdata($1, $3); }
    |	T_MD sp rdata_domain_name { zc_warning_prev_line("MD is obsolete"); }
    |	T_MD sp rdata_unknown
    {
	    zc_warning_prev_line("MD is obsolete");
	    $$ = $1; parse_unknown_rdata($1, $3);
    }
    |	T_MF sp rdata_domain_name { zc_warning_prev_line("MF is obsolete"); }
    |	T_MF sp rdata_unknown
    {
	    zc_warning_prev_line("MF is obsolete");
	    $$ = $1;
	    parse_unknown_rdata($1, $3);
    }
    |	T_CNAME sp rdata_domain_name
    |	T_CNAME sp rdata_unknown { $$ = $1; parse_unknown_rdata($1, $3); }
    |	T_SOA sp rdata_soa
    |	T_SOA sp rdata_unknown { $$ = $1; parse_unknown_rdata($1, $3); }
    |	T_MB sp rdata_domain_name { zc_warning_prev_line("MB is obsolete"); }
    |	T_MB sp rdata_unknown
    {
	    zc_warning_prev_line("MB is obsolete");
	    $$ = $1;
	    parse_unknown_rdata($1, $3);
    }
    |	T_MG sp rdata_domain_name
    |	T_MG sp rdata_unknown { $$ = $1; parse_unknown_rdata($1, $3); }
    |	T_MR sp rdata_domain_name
    |	T_MR sp rdata_unknown { $$ = $1; parse_unknown_rdata($1, $3); }
      /* NULL */
    |	T_WKS sp rdata_wks
    |	T_WKS sp rdata_unknown { $$ = $1; parse_unknown_rdata($1, $3); }
    |	T_PTR sp rdata_domain_name
    |	T_PTR sp rdata_unknown { $$ = $1; parse_unknown_rdata($1, $3); }
    |	T_HINFO sp rdata_hinfo
    |	T_HINFO sp rdata_unknown { $$ = $1; parse_unknown_rdata($1, $3); }
    |	T_MINFO sp rdata_minfo /* Experimental */
    |	T_MINFO sp rdata_unknown { $$ = $1; parse_unknown_rdata($1, $3); }
    |	T_MX sp rdata_mx
    |	T_MX sp rdata_unknown { $$ = $1; parse_unknown_rdata($1, $3); }
    |	T_TXT sp rdata_txt
    |	T_TXT sp rdata_unknown { $$ = $1; parse_unknown_rdata($1, $3); }
    |	T_SPF sp rdata_txt
    |	T_SPF sp rdata_unknown { $$ = $1; parse_unknown_rdata($1, $3); }
    |	T_RP sp rdata_rp		/* RFC 1183 */
    |	T_RP sp rdata_unknown { $$ = $1; parse_unknown_rdata($1, $3); }
    |	T_AFSDB sp rdata_afsdb	/* RFC 1183 */
    |	T_AFSDB sp rdata_unknown { $$ = $1; parse_unknown_rdata($1, $3); }
    |	T_X25 sp rdata_x25	/* RFC 1183 */
    |	T_X25 sp rdata_unknown { $$ = $1; parse_unknown_rdata($1, $3); }
    |	T_ISDN sp rdata_isdn	/* RFC 1183 */
    |	T_ISDN sp rdata_unknown { $$ = $1; parse_unknown_rdata($1, $3); }
    |	T_IPSECKEY sp rdata_ipseckey	/* RFC 4025 */
    |	T_IPSECKEY sp rdata_unknown { $$ = $1; parse_unknown_rdata($1, $3); }
    |	T_DHCID sp rdata_dhcid
    |	T_DHCID sp rdata_unknown { $$ = $1; parse_unknown_rdata($1, $3); }
    |	T_RT sp rdata_rt		/* RFC 1183 */
    |	T_RT sp rdata_unknown { $$ = $1; parse_unknown_rdata($1, $3); }
    |	T_NSAP sp rdata_nsap	/* RFC 1706 */
    |	T_NSAP sp rdata_unknown { $$ = $1; parse_unknown_rdata($1, $3); }
    |	T_SIG sp rdata_rrsig
    |	T_SIG sp rdata_unknown { $$ = $1; parse_unknown_rdata($1, $3); }
    |	T_KEY sp rdata_dnskey
    |	T_KEY sp rdata_unknown { $$ = $1; parse_unknown_rdata($1, $3); }
    |	T_PX sp rdata_px		/* RFC 2163 */
    |	T_PX sp rdata_unknown { $$ = $1; parse_unknown_rdata($1, $3); }
    |	T_AAAA sp rdata_aaaa
    |	T_AAAA sp rdata_unknown { $$ = $1; parse_unknown_rdata($1, $3); }
    |	T_LOC sp rdata_loc
    |	T_LOC sp rdata_unknown { $$ = $1; parse_unknown_rdata($1, $3); }
    |	T_NXT sp rdata_nxt
    |	T_NXT sp rdata_unknown { $$ = $1; parse_unknown_rdata($1, $3); }
    |	T_SRV sp rdata_srv
    |	T_SRV sp rdata_unknown { $$ = $1; parse_unknown_rdata($1, $3); }
    |	T_NAPTR sp rdata_naptr	/* RFC 2915 */
    |	T_NAPTR sp rdata_unknown { $$ = $1; parse_unknown_rdata($1, $3); }
    |	T_KX sp rdata_kx		/* RFC 2230 */
    |	T_KX sp rdata_unknown { $$ = $1; parse_unknown_rdata($1, $3); }
    |	T_CERT sp rdata_cert	/* RFC 2538 */
    |	T_CERT sp rdata_unknown { $$ = $1; parse_unknown_rdata($1, $3); }
    |	T_DNAME sp rdata_domain_name /* RFC 2672 */
    |	T_DNAME sp rdata_unknown { $$ = $1; parse_unknown_rdata($1, $3); }
    |	T_APL trail		/* RFC 3123 */
    |	T_APL sp rdata_apl	/* RFC 3123 */
    |	T_APL sp rdata_unknown { $$ = $1; parse_unknown_rdata($1, $3); }
    |	T_DS sp rdata_ds
    |	T_DS sp rdata_unknown { $$ = $1; parse_unknown_rdata($1, $3); }
    |	T_DLV sp rdata_dlv { if (dlv_warn) { dlv_warn = 0; zc_warning_prev_line("DLV is experimental"); } }
    |	T_DLV sp rdata_unknown { if (dlv_warn) { dlv_warn = 0; zc_warning_prev_line("DLV is experimental"); } $$ = $1; parse_unknown_rdata($1, $3); }
    |	T_SSHFP sp rdata_sshfp
    |	T_SSHFP sp rdata_unknown { $$ = $1; parse_unknown_rdata($1, $3); }
    |	T_RRSIG sp rdata_rrsig
    |	T_RRSIG sp rdata_unknown { $$ = $1; parse_unknown_rdata($1, $3); }
    |	T_NSEC sp rdata_nsec
    |	T_NSEC sp rdata_unknown { $$ = $1; parse_unknown_rdata($1, $3); }
    |	T_NSEC3 sp rdata_nsec3
    |	T_NSEC3 sp rdata_unknown { $$ = $1; parse_unknown_rdata($1, $3); }
    |	T_NSEC3PARAM sp rdata_nsec3_param
    |	T_NSEC3PARAM sp rdata_unknown { $$ = $1; parse_unknown_rdata($1, $3); }
    |	T_DNSKEY sp rdata_dnskey
    |	T_DNSKEY sp rdata_unknown { $$ = $1; parse_unknown_rdata($1, $3); }
    |	T_UTYPE sp rdata_unknown { $$ = $1; parse_unknown_rdata($1, $3); }
    |	STR error NL
    {
	    zc_error_prev_line("unrecognized RR type '%s'", $1.str);
    }
    ;

/*
 *
 * below are all the definition for all the different rdata
 *
 */

rdata_a:	dotted_str trail
    {
	    zadd_rdata_wireformat(zparser_conv_a(parser->region, $1.str));
    }
    ;

rdata_domain_name:	dname trail
    {
	    /* convert a single dname record */
	    zadd_rdata_domain($1);
    }
    ;

rdata_soa:	dname sp dname sp STR sp STR sp STR sp STR sp STR trail
    {
	    /* convert the soa data */
	    zadd_rdata_domain($1);	/* prim. ns */
	    zadd_rdata_domain($3);	/* email */
	    zadd_rdata_wireformat(zparser_conv_serial(parser->region, $5.str)); /* serial */
	    zadd_rdata_wireformat(zparser_conv_period(parser->region, $7.str)); /* refresh */
	    zadd_rdata_wireformat(zparser_conv_period(parser->region, $9.str)); /* retry */
	    zadd_rdata_wireformat(zparser_conv_period(parser->region, $11.str)); /* expire */
	    zadd_rdata_wireformat(zparser_conv_period(parser->region, $13.str)); /* minimum */
    }
    ;

rdata_wks:	dotted_str sp STR sp concatenated_str_seq trail
    {
	    zadd_rdata_wireformat(zparser_conv_a(parser->region, $1.str)); /* address */
	    zadd_rdata_wireformat(zparser_conv_services(parser->region, $3.str, $5.str)); /* protocol and services */
    }
    ;

rdata_hinfo:	STR sp STR trail
    {
	    zadd_rdata_wireformat(zparser_conv_text(parser->region, $1.str, $1.len)); /* CPU */
	    zadd_rdata_wireformat(zparser_conv_text(parser->region, $3.str, $3.len)); /* OS*/
    }
    ;

rdata_minfo:	dname sp dname trail
    {
	    /* convert a single dname record */
	    zadd_rdata_domain($1);
	    zadd_rdata_domain($3);
    }
    ;

rdata_mx:	STR sp dname trail
    {
	    zadd_rdata_wireformat(zparser_conv_short(parser->region, $1.str));  /* priority */
	    zadd_rdata_domain($3);	/* MX host */
    }
    ;

rdata_txt:	str_seq trail
    ;

/* RFC 1183 */
rdata_rp:	dname sp dname trail
    {
	    zadd_rdata_domain($1); /* mbox d-name */
	    zadd_rdata_domain($3); /* txt d-name */
    }
    ;

/* RFC 1183 */
rdata_afsdb:	STR sp dname trail
    {
	    zadd_rdata_wireformat(zparser_conv_short(parser->region, $1.str)); /* subtype */
	    zadd_rdata_domain($3); /* domain name */
    }
    ;

/* RFC 1183 */
rdata_x25:	STR trail
    {
	    zadd_rdata_wireformat(zparser_conv_text(parser->region, $1.str, $1.len)); /* X.25 address. */
    }
    ;

/* RFC 1183 */
rdata_isdn:	STR trail
    {
	    zadd_rdata_wireformat(zparser_conv_text(parser->region, $1.str, $1.len)); /* address */
    }
    |	STR sp STR trail
    {
	    zadd_rdata_wireformat(zparser_conv_text(parser->region, $1.str, $1.len)); /* address */
	    zadd_rdata_wireformat(zparser_conv_text(parser->region, $3.str, $3.len)); /* sub-address */
    }
    ;

/* RFC 1183 */
rdata_rt:	STR sp dname trail
    {
	    zadd_rdata_wireformat(zparser_conv_short(parser->region, $1.str)); /* preference */
	    zadd_rdata_domain($3); /* intermediate host */
    }
    ;

/* RFC 1706 */
rdata_nsap:	str_dot_seq trail
    {
	    /* String must start with "0x" or "0X".	 */
	    if (strncasecmp($1.str, "0x", 2) != 0) {
		    zc_error_prev_line("NSAP rdata must start with '0x'");
	    } else {
		    zadd_rdata_wireformat(zparser_conv_hex(parser->region, $1.str + 2, $1.len - 2)); /* NSAP */
	    }
    }
    ;

/* RFC 2163 */
rdata_px:	STR sp dname sp dname trail
    {
	    zadd_rdata_wireformat(zparser_conv_short(parser->region, $1.str)); /* preference */
	    zadd_rdata_domain($3); /* MAP822 */
	    zadd_rdata_domain($5); /* MAPX400 */
    }
    ;

rdata_aaaa:	dotted_str trail
    {
	    zadd_rdata_wireformat(zparser_conv_aaaa(parser->region, $1.str));  /* IPv6 address */
    }
    ;

rdata_loc:	concatenated_str_seq trail
    {
	    zadd_rdata_wireformat(zparser_conv_loc(parser->region, $1.str)); /* Location */
    }
    ;

rdata_nxt:	dname sp nxt_seq trail
    {
	    zadd_rdata_domain($1); /* nxt name */
	    zadd_rdata_wireformat(zparser_conv_nxt(parser->region, nxtbits)); /* nxt bitlist */
	    memset(nxtbits, 0, sizeof(nxtbits));
    }
    ;

rdata_srv:	STR sp STR sp STR sp dname trail
    {
	    zadd_rdata_wireformat(zparser_conv_short(parser->region, $1.str)); /* prio */
	    zadd_rdata_wireformat(zparser_conv_short(parser->region, $3.str)); /* weight */
	    zadd_rdata_wireformat(zparser_conv_short(parser->region, $5.str)); /* port */
	    zadd_rdata_domain($7); /* target name */
    }
    ;

/* RFC 2915 */
rdata_naptr:	STR sp STR sp STR sp STR sp STR sp dname trail
    {
	    zadd_rdata_wireformat(zparser_conv_short(parser->region, $1.str)); /* order */
	    zadd_rdata_wireformat(zparser_conv_short(parser->region, $3.str)); /* preference */
	    zadd_rdata_wireformat(zparser_conv_text(parser->region, $5.str, $5.len)); /* flags */
	    zadd_rdata_wireformat(zparser_conv_text(parser->region, $7.str, $7.len)); /* service */
	    zadd_rdata_wireformat(zparser_conv_text(parser->region, $9.str, $9.len)); /* regexp */
	    zadd_rdata_domain($11); /* target name */
    }
    ;

/* RFC 2230 */
rdata_kx:	STR sp dname trail
    {
	    zadd_rdata_wireformat(zparser_conv_short(parser->region, $1.str)); /* preference */
	    zadd_rdata_domain($3); /* exchanger */
    }
    ;

/* RFC 2538 */
rdata_cert:	STR sp STR sp STR sp str_sp_seq trail
    {
	    zadd_rdata_wireformat(zparser_conv_certificate_type(parser->region, $1.str)); /* type */
	    zadd_rdata_wireformat(zparser_conv_short(parser->region, $3.str)); /* key tag */
	    zadd_rdata_wireformat(zparser_conv_algorithm(parser->region, $5.str)); /* algorithm */
	    zadd_rdata_wireformat(zparser_conv_b64(parser->region, $7.str)); /* certificate or CRL */
    }
    ;

/* RFC 3123 */
rdata_apl:	rdata_apl_seq trail
    ;

rdata_apl_seq:	dotted_str
    {
	    zadd_rdata_wireformat(zparser_conv_apl_rdata(parser->region, $1.str));
    }
    |	rdata_apl_seq sp dotted_str
    {
	    zadd_rdata_wireformat(zparser_conv_apl_rdata(parser->region, $3.str));
    }
    ;

rdata_ds:	STR sp STR sp STR sp str_sp_seq trail
    {
	    zadd_rdata_wireformat(zparser_conv_short(parser->region, $1.str)); /* keytag */
	    zadd_rdata_wireformat(zparser_conv_algorithm(parser->region, $3.str)); /* alg */
	    zadd_rdata_wireformat(zparser_conv_byte(parser->region, $5.str)); /* type */
	    zadd_rdata_wireformat(zparser_conv_hex(parser->region, $7.str, $7.len)); /* hash */
    }
    ;

rdata_dlv:	STR sp STR sp STR sp str_sp_seq trail
    {
	    zadd_rdata_wireformat(zparser_conv_short(parser->region, $1.str)); /* keytag */
	    zadd_rdata_wireformat(zparser_conv_algorithm(parser->region, $3.str)); /* alg */
	    zadd_rdata_wireformat(zparser_conv_byte(parser->region, $5.str)); /* type */
	    zadd_rdata_wireformat(zparser_conv_hex(parser->region, $7.str, $7.len)); /* hash */
    }
    ;

rdata_sshfp:	STR sp STR sp str_sp_seq trail
    {
	    zadd_rdata_wireformat(zparser_conv_byte(parser->region, $1.str)); /* alg */
	    zadd_rdata_wireformat(zparser_conv_byte(parser->region, $3.str)); /* fp type */
	    zadd_rdata_wireformat(zparser_conv_hex(parser->region, $5.str, $5.len)); /* hash */
    }
    ;

rdata_dhcid:	str_sp_seq trail
    {
	    zadd_rdata_wireformat(zparser_conv_b64(parser->region, $1.str)); /* data blob */
    }
    ;

rdata_rrsig:	STR sp STR sp STR sp STR sp STR sp STR sp STR sp wire_dname sp str_sp_seq trail
    {
	    zadd_rdata_wireformat(zparser_conv_rrtype(parser->region, $1.str)); /* rr covered */
	    zadd_rdata_wireformat(zparser_conv_algorithm(parser->region, $3.str)); /* alg */
	    zadd_rdata_wireformat(zparser_conv_byte(parser->region, $5.str)); /* # labels */
	    zadd_rdata_wireformat(zparser_conv_period(parser->region, $7.str)); /* # orig TTL */
	    zadd_rdata_wireformat(zparser_conv_time(parser->region, $9.str)); /* sig exp */
	    zadd_rdata_wireformat(zparser_conv_time(parser->region, $11.str)); /* sig inc */
	    zadd_rdata_wireformat(zparser_conv_short(parser->region, $13.str)); /* key id */
	    zadd_rdata_wireformat(zparser_conv_dns_name(parser->region, 
				(const uint8_t*) $15.str,$15.len)); /* sig name */
	    zadd_rdata_wireformat(zparser_conv_b64(parser->region, $17.str)); /* sig data */
    }
    ;

rdata_nsec:	wire_dname nsec_seq
    {
	    zadd_rdata_wireformat(zparser_conv_dns_name(parser->region, 
				(const uint8_t*) $1.str, $1.len)); /* nsec name */
	    zadd_rdata_wireformat(zparser_conv_nsec(parser->region, nsecbits)); /* nsec bitlist */
	    memset(nsecbits, 0, sizeof(nsecbits));
            nsec_highest_rcode = 0;
    }
    ;

rdata_nsec3:   STR sp STR sp STR sp STR sp STR nsec_seq
    {
#ifdef NSEC3
	    nsec3_add_params($1.str, $3.str, $5.str, $7.str, $7.len);

	    zadd_rdata_wireformat(zparser_conv_b32(parser->region, $9.str)); /* next hashed name */
	    zadd_rdata_wireformat(zparser_conv_nsec(parser->region, nsecbits)); /* nsec bitlist */
	    memset(nsecbits, 0, sizeof(nsecbits));
	    nsec_highest_rcode = 0;
#else
	    zc_error_prev_line("nsec3 not supported");
#endif /* NSEC3 */
    }
    ;

rdata_nsec3_param:   STR sp STR sp STR sp STR trail
    {
#ifdef NSEC3
	    nsec3_add_params($1.str, $3.str, $5.str, $7.str, $7.len);
#else
	    zc_error_prev_line("nsec3 not supported");
#endif /* NSEC3 */
    }
    ;

rdata_dnskey:	STR sp STR sp STR sp str_sp_seq trail
    {
	    zadd_rdata_wireformat(zparser_conv_short(parser->region, $1.str)); /* flags */
	    zadd_rdata_wireformat(zparser_conv_byte(parser->region, $3.str)); /* proto */
	    zadd_rdata_wireformat(zparser_conv_algorithm(parser->region, $5.str)); /* alg */
	    zadd_rdata_wireformat(zparser_conv_b64(parser->region, $7.str)); /* hash */
    }
    ;

rdata_ipsec_base: STR sp STR sp STR sp dotted_str
    {
	    const dname_type* name = 0;
	    zadd_rdata_wireformat(zparser_conv_byte(parser->region, $1.str)); /* precedence */
	    zadd_rdata_wireformat(zparser_conv_byte(parser->region, $3.str)); /* gateway type */
	    zadd_rdata_wireformat(zparser_conv_byte(parser->region, $5.str)); /* algorithm */
	    switch(atoi($3.str)) {
		case IPSECKEY_NOGATEWAY: 
			zadd_rdata_wireformat(alloc_rdata_init(parser->region, "", 0));
			break;
		case IPSECKEY_IP4:
			zadd_rdata_wireformat(zparser_conv_a(parser->region, $7.str));
			break;
		case IPSECKEY_IP6:
			zadd_rdata_wireformat(zparser_conv_aaaa(parser->region, $7.str));
			break;
		case IPSECKEY_DNAME:
			/* convert and insert the dname */
			if(strlen($7.str) == 0)
				zc_error_prev_line("IPSECKEY must specify gateway name");
			if(!(name = dname_parse(parser->region, $7.str)))
				zc_error_prev_line("IPSECKEY bad gateway dname %s", $7.str);
			if($7.str[strlen($7.str)-1] != '.')
				name = dname_concatenate(parser->rr_region, name, 
					domain_dname(parser->origin));
			zadd_rdata_wireformat(alloc_rdata_init(parser->region,
				dname_name(name), name->name_size));
			break;
		default:
			zc_error_prev_line("unknown IPSECKEY gateway type");
	    }
    }
    ;

rdata_ipseckey:	rdata_ipsec_base sp str_sp_seq trail
    {
	   zadd_rdata_wireformat(zparser_conv_b64(parser->region, $3.str)); /* public key */
    }
    | rdata_ipsec_base trail
    ;

rdata_unknown:	URR sp STR sp str_sp_seq trail
    {
	    /* $2 is the number of octects, currently ignored */
	    $$ = zparser_conv_hex(parser->region, $5.str, $5.len);

    }
    |	URR sp STR trail
    {
	    $$ = zparser_conv_hex(parser->region, "", 0);
    }
    |	URR error NL
    {
	    $$ = zparser_conv_hex(parser->region, "", 0);
    }
    ;
%%

int
yywrap(void)
{
	return 1;
}

/*
 * Create the parser.
 */
zparser_type *
zparser_create(region_type *region, region_type *rr_region, namedb_type *db)
{
	zparser_type *result;

	result = (zparser_type *) region_alloc(region, sizeof(zparser_type));
	result->region = region;
	result->rr_region = rr_region;
	result->db = db;

	result->filename = NULL;
	result->current_zone = NULL;
	result->origin = NULL;
	result->prev_dname = NULL;
	result->default_apex = NULL;

	result->temporary_rdatas = (rdata_atom_type *) region_alloc(
		result->region, MAXRDATALEN * sizeof(rdata_atom_type));

	return result;
}

/*
 * Initialize the parser for a new zone file.
 */
void
zparser_init(const char *filename, uint32_t ttl, uint16_t klass,
	     const dname_type *origin)
{
	memset(nxtbits, 0, sizeof(nxtbits));
	memset(nsecbits, 0, sizeof(nsecbits));
        nsec_highest_rcode = 0;

	parser->default_ttl = ttl;
	parser->default_class = klass;
	parser->current_zone = NULL;
	parser->origin = domain_table_insert(parser->db->domains, origin);
	parser->prev_dname = parser->origin;
	parser->default_apex = parser->origin;
	parser->error_occurred = 0;
	parser->errors = 0;
	parser->line = 1;
	parser->filename = filename;
	parser->current_rr.rdata_count = 0;
	parser->current_rr.rdatas = parser->temporary_rdatas;
}

void
yyerror(const char *message)
{
	zc_error("%s", message);
}

static void
error_va_list(unsigned line, const char *fmt, va_list args)
{
	if (parser->filename) {
		fprintf(stderr, "%s:%u: ", parser->filename, line);
	}
	fprintf(stderr, "error: ");
	vfprintf(stderr, fmt, args);
	fprintf(stderr, "\n");

	++parser->errors;
	parser->error_occurred = 1;
}

/* the line counting sux, to say the least
 * with this grose hack we try do give sane
 * numbers back */
void
zc_error_prev_line(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	error_va_list(parser->line - 1, fmt, args);
	va_end(args);
}

void
zc_error(const char *fmt, ...)
{
	/* send an error message to stderr */
	va_list args;
	va_start(args, fmt);
	error_va_list(parser->line, fmt, args);
	va_end(args);
}

static void
warning_va_list(unsigned line, const char *fmt, va_list args)
{
	if (parser->filename) {
		fprintf(stderr, "%s:%u: ", parser->filename, line);
	}
	fprintf(stderr, "warning: ");
	vfprintf(stderr, fmt, args);
	fprintf(stderr, "\n");
}

void
zc_warning_prev_line(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	warning_va_list(parser->line - 1, fmt, args);
	va_end(args);
}

void
zc_warning(const char *fmt, ... )
{
	va_list args;
	va_start(args, fmt);
	warning_va_list(parser->line, fmt, args);
	va_end(args);
}

#ifdef NSEC3
static void
nsec3_add_params(const char* hashalgo_str, const char* flag_str,
	const char* iter_str, const char* salt_str, int salt_len)
{
	zadd_rdata_wireformat(zparser_conv_byte(parser->region, hashalgo_str));
	zadd_rdata_wireformat(zparser_conv_byte(parser->region, flag_str));
	zadd_rdata_wireformat(zparser_conv_short(parser->region, iter_str));

	/* salt */
	if(strcmp(salt_str, "-") != 0) 
		zadd_rdata_wireformat(zparser_conv_hex_length(parser->region, 
			salt_str, salt_len)); 
	else 
		zadd_rdata_wireformat(alloc_rdata_init(parser->region, "", 1));
}
#endif /* NSEC3 */
