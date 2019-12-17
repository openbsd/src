/*
 * Copyright (C) Internet Systems Consortium, Inc. ("ISC")
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: ssu.h,v 1.3 2019/12/17 01:46:32 sthen Exp $ */

#ifndef DNS_SSU_H
#define DNS_SSU_H 1

/*! \file dns/ssu.h */

#include <isc/lang.h>

#include <dns/acl.h>
#include <dns/types.h>
#include <dst/dst.h>

ISC_LANG_BEGINDECLS

typedef enum {
	dns_ssumatchtype_name = 0,
	dns_ssumatchtype_subdomain = 1,
	dns_ssumatchtype_wildcard = 2,
	dns_ssumatchtype_self    = 3,
	dns_ssumatchtype_selfsub = 4,
	dns_ssumatchtype_selfwild = 5,
	dns_ssumatchtype_selfkrb5 = 6,
	dns_ssumatchtype_selfms  = 7,
	dns_ssumatchtype_subdomainms = 8,
	dns_ssumatchtype_subdomainkrb5 = 9,
	dns_ssumatchtype_tcpself = 10,
	dns_ssumatchtype_6to4self = 11,
	dns_ssumatchtype_external = 12,
	dns_ssumatchtype_local = 13,
	dns_ssumatchtype_max = 13,      /* max value */

	dns_ssumatchtype_dlz = 14       /* intentionally higher than _max */
} dns_ssumatchtype_t;

#define DNS_SSUMATCHTYPE_NAME		dns_ssumatchtype_name
#define DNS_SSUMATCHTYPE_SUBDOMAIN	dns_ssumatchtype_subdomain
#define DNS_SSUMATCHTYPE_WILDCARD	dns_ssumatchtype_wildcard
#define DNS_SSUMATCHTYPE_SELF		dns_ssumatchtype_self
#define DNS_SSUMATCHTYPE_SELFSUB	dns_ssumatchtype_selfsub
#define DNS_SSUMATCHTYPE_SELFWILD	dns_ssumatchtype_selfwild
#define DNS_SSUMATCHTYPE_SELFKRB5	dns_ssumatchtype_selfkrb5
#define DNS_SSUMATCHTYPE_SELFMS		dns_ssumatchtype_selfms
#define DNS_SSUMATCHTYPE_SUBDOMAINMS	dns_ssumatchtype_subdomainms
#define DNS_SSUMATCHTYPE_SUBDOMAINKRB5	dns_ssumatchtype_subdomainkrb5
#define DNS_SSUMATCHTYPE_TCPSELF	dns_ssumatchtype_tcpself
#define DNS_SSUMATCHTYPE_6TO4SELF	dns_ssumatchtype_6to4self
#define DNS_SSUMATCHTYPE_EXTERNAL	dns_ssumatchtype_external
#define DNS_SSUMATCHTYPE_LOCAL		dns_ssumatchtype_local
#define DNS_SSUMATCHTYPE_MAX 		dns_ssumatchtype_max  /* max value */

#define DNS_SSUMATCHTYPE_DLZ		dns_ssumatchtype_dlz  /* intentionally higher than _MAX */

isc_result_t
dns_ssutable_create(isc_mem_t *mctx, dns_ssutable_t **table);
/*%<
 *	Creates a table that will be used to store simple-secure-update rules.
 *	Note: all locking must be provided by the client.
 *
 *	Requires:
 *\li		'mctx' is a valid memory context
 *\li		'table' is not NULL, and '*table' is NULL
 *
 *	Returns:
 *\li		ISC_R_SUCCESS
 *\li		ISC_R_NOMEMORY
 */

isc_result_t
dns_ssutable_createdlz(isc_mem_t *mctx, dns_ssutable_t **tablep,
		       dns_dlzdb_t *dlzdatabase);
/*%<
 * Create an SSU table that contains a dlzdatabase pointer, and a
 * single rule with matchtype DNS_SSUMATCHTYPE_DLZ. This type of SSU
 * table is used by writeable DLZ drivers to offload authorization for
 * updates to the driver.
 */

void
dns_ssutable_attach(dns_ssutable_t *source, dns_ssutable_t **targetp);
/*%<
 *	Attach '*targetp' to 'source'.
 *
 *	Requires:
 *\li		'source' is a valid SSU table
 *\li		'targetp' points to a NULL dns_ssutable_t *.
 *
 *	Ensures:
 *\li		*targetp is attached to source.
 */

void
dns_ssutable_detach(dns_ssutable_t **tablep);
/*%<
 *	Detach '*tablep' from its simple-secure-update rule table.
 *
 *	Requires:
 *\li		'tablep' points to a valid dns_ssutable_t
 *
 *	Ensures:
 *\li		*tablep is NULL
 *\li		If '*tablep' is the last reference to the SSU table, all
 *			resources used by the table will be freed.
 */

isc_result_t
dns_ssutable_addrule(dns_ssutable_t *table, isc_boolean_t grant,
		     dns_name_t *identity, unsigned int matchtype,
		     dns_name_t *name, unsigned int ntypes,
		     dns_rdatatype_t *types);
/*%<
 *	Adds a new rule to a simple-secure-update rule table.  The rule
 *	either grants or denies update privileges of an identity (or set of
 *	identities) to modify a name (or set of names) or certain types present
 *	at that name.
 *
 *	Notes:
 *\li		If 'matchtype' is of SELF type, this rule only matches if the
 *              name to be updated matches the signing identity.
 *
 *\li		If 'ntypes' is 0, this rule applies to all types except
 *		NS, SOA, RRSIG, and NSEC.
 *
 *\li		If 'types' includes ANY, this rule applies to all types
 *		except NSEC.
 *
 *	Requires:
 *\li		'table' is a valid SSU table
 *\li		'identity' is a valid absolute name
 *\li		'matchtype' must be one of the defined constants.
 *\li		'name' is a valid absolute name
 *\li		If 'ntypes' > 0, 'types' must not be NULL
 *
 *	Returns:
 *\li		ISC_R_SUCCESS
 *\li		ISC_R_NOMEMORY
 */

isc_boolean_t
dns_ssutable_checkrules(dns_ssutable_t *table, dns_name_t *signer,
			dns_name_t *name, isc_netaddr_t *addr,
			dns_rdatatype_t type, const dst_key_t *key);
isc_boolean_t
dns_ssutable_checkrules2(dns_ssutable_t *table, dns_name_t *signer,
			dns_name_t *name, isc_netaddr_t *addr,
			isc_boolean_t tcp, const dns_aclenv_t *env,
			dns_rdatatype_t type, const dst_key_t *key);
/*%<
 *	Checks that the attempted update of (name, type) is allowed according
 *	to the rules specified in the simple-secure-update rule table.  If
 *	no rules are matched, access is denied.
 *
 *	Notes:
 *		In dns_ssutable_checkrules(), 'addr' should only be
 *		set if the request received via TCP.  This provides a
 *		weak assurance that the request was not spoofed.
 *		'addr' is to to validate DNS_SSUMATCHTYPE_TCPSELF
 *		and DNS_SSUMATCHTYPE_6TO4SELF rules.
 *
 *		In dns_ssutable_checkrules2(), 'addr' can also be passed for
 *		UDP requests and TCP is specified via the 'tcp' parameter.
 *		In addition to DNS_SSUMATCHTYPE_TCPSELF and
 *		tcp_ssumatchtype_6to4self  rules, the address
 *		also be used to check DNS_SSUMATCHTYPE_LOCAL rules.
 *		If 'addr' is set then 'env' must also be set so that
 *		requests from non-localhost addresses can be rejected.
 *
 *		For DNS_SSUMATCHTYPE_TCPSELF the addresses are mapped to
 *		the standard reverse names under IN-ADDR.ARPA and IP6.ARPA.
 *		RFC 1035, Section 3.5, "IN-ADDR.ARPA domain" and RFC 3596,
 *		Section 2.5, "IP6.ARPA Domain".
 *
 *		For DNS_SSUMATCHTYPE_6TO4SELF, IPv4 address are converted
 *		to a 6to4 prefix (48 bits) per the rules in RFC 3056.  Only
 *		the top	48 bits of the IPv6 address are mapped to the reverse
 *		name. This is independent of whether the most significant 16
 *		bits match 2002::/16, assigned for 6to4 prefixes, or not.
 *
 *	Requires:
 *\li		'table' is a valid SSU table
 *\li		'signer' is NULL or a valid absolute name
 *\li		'addr' is NULL or a valid network address.
 *\li		'aclenv' is NULL or a valid ACL environment.
 *\li		'name' is a valid absolute name
 *\li		if 'addr' is not NULL, 'env' is not NULL.
 */


/*% Accessor functions to extract rule components */
isc_boolean_t	dns_ssurule_isgrant(const dns_ssurule_t *rule);
/*% Accessor functions to extract rule components */
dns_name_t *	dns_ssurule_identity(const dns_ssurule_t *rule);
/*% Accessor functions to extract rule components */
unsigned int	dns_ssurule_matchtype(const dns_ssurule_t *rule);
/*% Accessor functions to extract rule components */
dns_name_t *	dns_ssurule_name(const dns_ssurule_t *rule);
/*% Accessor functions to extract rule components */
unsigned int	dns_ssurule_types(const dns_ssurule_t *rule,
				  dns_rdatatype_t **types);

isc_result_t	dns_ssutable_firstrule(const dns_ssutable_t *table,
				       dns_ssurule_t **rule);
/*%<
 * Initiates a rule iterator.  There is no need to maintain any state.
 *
 * Returns:
 *\li	#ISC_R_SUCCESS
 *\li	#ISC_R_NOMORE
 */

isc_result_t	dns_ssutable_nextrule(dns_ssurule_t *rule,
				      dns_ssurule_t **nextrule);
/*%<
 * Returns the next rule in the table.
 *
 * Returns:
 *\li	#ISC_R_SUCCESS
 *\li	#ISC_R_NOMORE
 */

isc_boolean_t
dns_ssu_external_match(dns_name_t *identity, dns_name_t *signer,
		       dns_name_t *name, isc_netaddr_t *tcpaddr,
		       dns_rdatatype_t type, const dst_key_t *key,
		       isc_mem_t *mctx);
/*%<
 * Check a policy rule via an external application
 */

isc_result_t
dns_ssu_mtypefromstring(const char *str, dns_ssumatchtype_t *mtype);
/*%<
 * Set 'mtype' from 'str'
 *
 * Requires:
 *\li		'str' is not NULL.
 *\li		'mtype' is not NULL,
 *
 * Returns:
 *\li	#ISC_R_SUCCESS
 *\li	#ISC_R_NOTFOUND
 */

ISC_LANG_ENDDECLS

#endif /* DNS_SSU_H */
