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

/*! \file */

#include <config.h>

#include <stdlib.h>

#include <isc/base64.h>
#include <isc/buffer.h>
#include <isc/file.h>
#include <isc/hex.h>
#include <isc/log.h>
#include <isc/mem.h>
#include <isc/netaddr.h>
#include <isc/parseint.h>
#include <isc/platform.h>
#include <isc/region.h>
#include <isc/result.h>
#include <isc/sockaddr.h>
#include <isc/string.h>
#include <isc/symtab.h>
#include <isc/util.h>

#ifdef ISC_PLATFORM_USESIT
#ifdef AES_SIT
#include <isc/aes.h>
#endif
#ifdef HMAC_SHA1_SIT
#include <isc/sha1.h>
#endif
#ifdef HMAC_SHA256_SIT
#include <isc/sha2.h>
#endif
#endif

#include <pk11/site.h>

#include <dns/acl.h>
#include <dns/fixedname.h>
#include <dns/rdataclass.h>
#include <dns/rdatatype.h>
#include <dns/rrl.h>
#include <dns/secalg.h>
#include <dns/ssu.h>

#include <dst/dst.h>

#include <isccfg/aclconf.h>
#include <isccfg/cfg.h>

#include <bind9/check.h>

#define INITNAME(A,B) { \
		DNS_NAME_MAGIC, \
		A, sizeof(A), sizeof(B), \
		DNS_NAMEATTR_READONLY | DNS_NAMEATTR_ABSOLUTE, \
		B, NULL, { (void *)-1, (void *)-1}, \
		{NULL, NULL} \
}

static unsigned char dlviscorg_ndata[] = "\003dlv\003isc\003org";
static unsigned char dlviscorg_offsets[] = { 0, 4, 8, 12 };

static const dns_name_t dlviscorg =
	INITNAME(dlviscorg_ndata, dlviscorg_offsets);

static isc_result_t
fileexist(const cfg_obj_t *obj, isc_symtab_t *symtab, isc_boolean_t writeable,
	  isc_log_t *logctxlogc);

static void
freekey(char *key, unsigned int type, isc_symvalue_t value, void *userarg) {
	UNUSED(type);
	UNUSED(value);
	isc_mem_free(userarg, key);
}

static isc_result_t
check_orderent(const cfg_obj_t *ent, isc_log_t *logctx) {
	isc_result_t result = ISC_R_SUCCESS;
	isc_result_t tresult;
	isc_textregion_t r;
	dns_fixedname_t fixed;
	const cfg_obj_t *obj;
	dns_rdataclass_t rdclass;
	dns_rdatatype_t rdtype;
	isc_buffer_t b;
	const char *str;

	dns_fixedname_init(&fixed);
	obj = cfg_tuple_get(ent, "class");
	if (cfg_obj_isstring(obj)) {

		DE_CONST(cfg_obj_asstring(obj), r.base);
		r.length = strlen(r.base);
		tresult = dns_rdataclass_fromtext(&rdclass, &r);
		if (tresult != ISC_R_SUCCESS) {
			cfg_obj_log(obj, logctx, ISC_LOG_ERROR,
				    "rrset-order: invalid class '%s'",
				    r.base);
			result = ISC_R_FAILURE;
		}
	}

	obj = cfg_tuple_get(ent, "type");
	if (cfg_obj_isstring(obj)) {
		DE_CONST(cfg_obj_asstring(obj), r.base);
		r.length = strlen(r.base);
		tresult = dns_rdatatype_fromtext(&rdtype, &r);
		if (tresult != ISC_R_SUCCESS) {
			cfg_obj_log(obj, logctx, ISC_LOG_ERROR,
				    "rrset-order: invalid type '%s'",
				    r.base);
			result = ISC_R_FAILURE;
		}
	}

	obj = cfg_tuple_get(ent, "name");
	if (cfg_obj_isstring(obj)) {
		str = cfg_obj_asstring(obj);
		isc_buffer_constinit(&b, str, strlen(str));
		isc_buffer_add(&b, strlen(str));
		tresult = dns_name_fromtext(dns_fixedname_name(&fixed), &b,
					    dns_rootname, 0, NULL);
		if (tresult != ISC_R_SUCCESS) {
			cfg_obj_log(obj, logctx, ISC_LOG_ERROR,
				    "rrset-order: invalid name '%s'", str);
			result = ISC_R_FAILURE;
		}
	}

	obj = cfg_tuple_get(ent, "order");
	if (!cfg_obj_isstring(obj) ||
	    strcasecmp("order", cfg_obj_asstring(obj)) != 0) {
		cfg_obj_log(ent, logctx, ISC_LOG_ERROR,
			    "rrset-order: keyword 'order' missing");
		result = ISC_R_FAILURE;
	}

	obj = cfg_tuple_get(ent, "ordering");
	if (!cfg_obj_isstring(obj)) {
	    cfg_obj_log(ent, logctx, ISC_LOG_ERROR,
			"rrset-order: missing ordering");
		result = ISC_R_FAILURE;
	} else if (strcasecmp(cfg_obj_asstring(obj), "fixed") == 0) {
#if !DNS_RDATASET_FIXED
		cfg_obj_log(obj, logctx, ISC_LOG_WARNING,
			    "rrset-order: order 'fixed' was disabled at "
			    "compilation time");
#endif
	} else if (strcasecmp(cfg_obj_asstring(obj), "random") != 0 &&
		   strcasecmp(cfg_obj_asstring(obj), "cyclic") != 0) {
		cfg_obj_log(obj, logctx, ISC_LOG_ERROR,
			    "rrset-order: invalid order '%s'",
			    cfg_obj_asstring(obj));
		result = ISC_R_FAILURE;
	}
	return (result);
}

static isc_result_t
check_order(const cfg_obj_t *options, isc_log_t *logctx) {
	isc_result_t result = ISC_R_SUCCESS;
	isc_result_t tresult;
	const cfg_listelt_t *element;
	const cfg_obj_t *obj = NULL;

	if (cfg_map_get(options, "rrset-order", &obj) != ISC_R_SUCCESS)
		return (result);

	for (element = cfg_list_first(obj);
	     element != NULL;
	     element = cfg_list_next(element))
	{
		tresult = check_orderent(cfg_listelt_value(element), logctx);
		if (tresult != ISC_R_SUCCESS)
			result = tresult;
	}
	return (result);
}

static isc_result_t
check_dual_stack(const cfg_obj_t *options, isc_log_t *logctx) {
	const cfg_listelt_t *element;
	const cfg_obj_t *alternates = NULL;
	const cfg_obj_t *value;
	const cfg_obj_t *obj;
	const char *str;
	dns_fixedname_t fixed;
	dns_name_t *name;
	isc_buffer_t buffer;
	isc_result_t result = ISC_R_SUCCESS;
	isc_result_t tresult;

	(void)cfg_map_get(options, "dual-stack-servers", &alternates);

	if (alternates == NULL)
		return (ISC_R_SUCCESS);

	obj = cfg_tuple_get(alternates, "port");
	if (cfg_obj_isuint32(obj)) {
		isc_uint32_t val = cfg_obj_asuint32(obj);
		if (val > ISC_UINT16_MAX) {
			cfg_obj_log(obj, logctx, ISC_LOG_ERROR,
				    "port '%u' out of range", val);
			result = ISC_R_FAILURE;
		}
	}
	obj = cfg_tuple_get(alternates, "addresses");
	for (element = cfg_list_first(obj);
	     element != NULL;
	     element = cfg_list_next(element)) {
		value = cfg_listelt_value(element);
		if (cfg_obj_issockaddr(value))
			continue;
		obj = cfg_tuple_get(value, "name");
		str = cfg_obj_asstring(obj);
		isc_buffer_constinit(&buffer, str, strlen(str));
		isc_buffer_add(&buffer, strlen(str));
		dns_fixedname_init(&fixed);
		name = dns_fixedname_name(&fixed);
		tresult = dns_name_fromtext(name, &buffer, dns_rootname,
					    0, NULL);
		if (tresult != ISC_R_SUCCESS) {
			cfg_obj_log(obj, logctx, ISC_LOG_ERROR,
				    "bad name '%s'", str);
			result = ISC_R_FAILURE;
		}
		obj = cfg_tuple_get(value, "port");
		if (cfg_obj_isuint32(obj)) {
			isc_uint32_t val = cfg_obj_asuint32(obj);
			if (val > ISC_UINT16_MAX) {
				cfg_obj_log(obj, logctx, ISC_LOG_ERROR,
					    "port '%u' out of range", val);
				result = ISC_R_FAILURE;
			}
		}
	}
	return (result);
}

static isc_result_t
check_forward(const cfg_obj_t *options,  const cfg_obj_t *global,
	      isc_log_t *logctx)
{
	const cfg_obj_t *forward = NULL;
	const cfg_obj_t *forwarders = NULL;

	(void)cfg_map_get(options, "forward", &forward);
	(void)cfg_map_get(options, "forwarders", &forwarders);

	if (forwarders != NULL && global != NULL) {
		const char *file = cfg_obj_file(global);
		unsigned int line = cfg_obj_line(global);
		cfg_obj_log(forwarders, logctx, ISC_LOG_ERROR,
			    "forwarders declared in root zone and "
			    "in general configuration: %s:%u",
			    file, line);
		return (ISC_R_FAILURE);
	}
	if (forward != NULL && forwarders == NULL) {
		cfg_obj_log(forward, logctx, ISC_LOG_ERROR,
			    "no matching 'forwarders' statement");
		return (ISC_R_FAILURE);
	}
	return (ISC_R_SUCCESS);
}

static isc_result_t
disabled_algorithms(const cfg_obj_t *disabled, isc_log_t *logctx) {
	isc_result_t result = ISC_R_SUCCESS;
	isc_result_t tresult;
	const cfg_listelt_t *element;
	const char *str;
	isc_buffer_t b;
	dns_fixedname_t fixed;
	dns_name_t *name;
	const cfg_obj_t *obj;

	dns_fixedname_init(&fixed);
	name = dns_fixedname_name(&fixed);
	obj = cfg_tuple_get(disabled, "name");
	str = cfg_obj_asstring(obj);
	isc_buffer_constinit(&b, str, strlen(str));
	isc_buffer_add(&b, strlen(str));
	tresult = dns_name_fromtext(name, &b, dns_rootname, 0, NULL);
	if (tresult != ISC_R_SUCCESS) {
		cfg_obj_log(obj, logctx, ISC_LOG_ERROR,
			    "bad domain name '%s'", str);
		result = tresult;
	}

	obj = cfg_tuple_get(disabled, "algorithms");

	for (element = cfg_list_first(obj);
	     element != NULL;
	     element = cfg_list_next(element))
	{
		isc_textregion_t r;
		dns_secalg_t alg;

		DE_CONST(cfg_obj_asstring(cfg_listelt_value(element)), r.base);
		r.length = strlen(r.base);

		tresult = dns_secalg_fromtext(&alg, &r);
		if (tresult != ISC_R_SUCCESS) {
			cfg_obj_log(cfg_listelt_value(element), logctx,
				    ISC_LOG_ERROR, "invalid algorithm '%s'",
				    r.base);
			result = tresult;
		}
	}
	return (result);
}

static isc_result_t
disabled_ds_digests(const cfg_obj_t *disabled, isc_log_t *logctx) {
	isc_result_t result = ISC_R_SUCCESS;
	isc_result_t tresult;
	const cfg_listelt_t *element;
	const char *str;
	isc_buffer_t b;
	dns_fixedname_t fixed;
	dns_name_t *name;
	const cfg_obj_t *obj;

	dns_fixedname_init(&fixed);
	name = dns_fixedname_name(&fixed);
	obj = cfg_tuple_get(disabled, "name");
	str = cfg_obj_asstring(obj);
	isc_buffer_constinit(&b, str, strlen(str));
	isc_buffer_add(&b, strlen(str));
	tresult = dns_name_fromtext(name, &b, dns_rootname, 0, NULL);
	if (tresult != ISC_R_SUCCESS) {
		cfg_obj_log(obj, logctx, ISC_LOG_ERROR,
			    "bad domain name '%s'", str);
		result = tresult;
	}

	obj = cfg_tuple_get(disabled, "digests");

	for (element = cfg_list_first(obj);
	     element != NULL;
	     element = cfg_list_next(element))
	{
		isc_textregion_t r;
		dns_dsdigest_t digest;

		DE_CONST(cfg_obj_asstring(cfg_listelt_value(element)), r.base);
		r.length = strlen(r.base);

		/* works with a numeric argument too */
		tresult = dns_dsdigest_fromtext(&digest, &r);
		if (tresult != ISC_R_SUCCESS) {
			cfg_obj_log(cfg_listelt_value(element), logctx,
				    ISC_LOG_ERROR, "invalid digest type '%s'",
				    r.base);
			result = tresult;
		}
	}
	return (result);
}

static isc_result_t
nameexist(const cfg_obj_t *obj, const char *name, int value,
	  isc_symtab_t *symtab, const char *fmt, isc_log_t *logctx,
	  isc_mem_t *mctx)
{
	char *key;
	const char *file;
	unsigned int line;
	isc_result_t result;
	isc_symvalue_t symvalue;

	key = isc_mem_strdup(mctx, name);
	if (key == NULL)
		return (ISC_R_NOMEMORY);
	symvalue.as_cpointer = obj;
	result = isc_symtab_define(symtab, key, value, symvalue,
				   isc_symexists_reject);
	if (result == ISC_R_EXISTS) {
		RUNTIME_CHECK(isc_symtab_lookup(symtab, key, value,
						&symvalue) == ISC_R_SUCCESS);
		file = cfg_obj_file(symvalue.as_cpointer);
		line = cfg_obj_line(symvalue.as_cpointer);

		if (file == NULL)
			file = "<unknown file>";
		cfg_obj_log(obj, logctx, ISC_LOG_ERROR, fmt, key, file, line);
		isc_mem_free(mctx, key);
		result = ISC_R_EXISTS;
	} else if (result != ISC_R_SUCCESS) {
		isc_mem_free(mctx, key);
	}
	return (result);
}

static isc_result_t
mustbesecure(const cfg_obj_t *secure, isc_symtab_t *symtab, isc_log_t *logctx,
	     isc_mem_t *mctx)
{
	const cfg_obj_t *obj;
	char namebuf[DNS_NAME_FORMATSIZE];
	const char *str;
	dns_fixedname_t fixed;
	dns_name_t *name;
	isc_buffer_t b;
	isc_result_t result = ISC_R_SUCCESS;

	dns_fixedname_init(&fixed);
	name = dns_fixedname_name(&fixed);
	obj = cfg_tuple_get(secure, "name");
	str = cfg_obj_asstring(obj);
	isc_buffer_constinit(&b, str, strlen(str));
	isc_buffer_add(&b, strlen(str));
	result = dns_name_fromtext(name, &b, dns_rootname, 0, NULL);
	if (result != ISC_R_SUCCESS) {
		cfg_obj_log(obj, logctx, ISC_LOG_ERROR,
			    "bad domain name '%s'", str);
	} else {
		dns_name_format(name, namebuf, sizeof(namebuf));
		result = nameexist(secure, namebuf, 1, symtab,
				   "dnssec-must-be-secure '%s': already "
				   "exists previous definition: %s:%u",
				   logctx, mctx);
	}
	return (result);
}

static isc_result_t
checkacl(const char *aclname, cfg_aclconfctx_t *actx, const cfg_obj_t *zconfig,
	 const cfg_obj_t *voptions, const cfg_obj_t *config,
	 isc_log_t *logctx, isc_mem_t *mctx)
{
	isc_result_t result;
	const cfg_obj_t *aclobj = NULL;
	const cfg_obj_t *options;
	dns_acl_t *acl = NULL;

	if (zconfig != NULL) {
		options = cfg_tuple_get(zconfig, "options");
		cfg_map_get(options, aclname, &aclobj);
	}
	if (voptions != NULL && aclobj == NULL)
		cfg_map_get(voptions, aclname, &aclobj);
	if (config != NULL && aclobj == NULL) {
		options = NULL;
		cfg_map_get(config, "options", &options);
		if (options != NULL)
			cfg_map_get(options, aclname, &aclobj);
	}
	if (aclobj == NULL)
		return (ISC_R_SUCCESS);
	result = cfg_acl_fromconfig(aclobj, config, logctx,
				    actx, mctx, 0, &acl);
	if (acl != NULL)
		dns_acl_detach(&acl);
	return (result);
}

static isc_result_t
check_viewacls(cfg_aclconfctx_t *actx, const cfg_obj_t *voptions,
	       const cfg_obj_t *config, isc_log_t *logctx, isc_mem_t *mctx)
{
	isc_result_t result = ISC_R_SUCCESS, tresult;
	int i = 0;

	static const char *acls[] = { "allow-query", "allow-query-on",
		"allow-query-cache", "allow-query-cache-on",
		"blackhole", "match-clients", "match-destinations",
		"sortlist", "filter-aaaa", NULL };

	while (acls[i] != NULL) {
		tresult = checkacl(acls[i++], actx, NULL, voptions, config,
				   logctx, mctx);
		if (tresult != ISC_R_SUCCESS)
			result = tresult;
	}
	return (result);
}

static const unsigned char zeros[16];

static isc_result_t
check_dns64(cfg_aclconfctx_t *actx, const cfg_obj_t *voptions,
	    const cfg_obj_t *config, isc_log_t *logctx, isc_mem_t *mctx)
{
	isc_result_t result = ISC_R_SUCCESS;
	const cfg_obj_t *dns64 = NULL;
	const cfg_obj_t *options;
	const cfg_listelt_t *element;
	const cfg_obj_t *map, *obj;
	isc_netaddr_t na, sa;
	unsigned int prefixlen;
	int nbytes;
	int i;

	static const char *acls[] = { "clients", "exclude", "mapped", NULL};

	if (voptions != NULL)
		cfg_map_get(voptions, "dns64", &dns64);
	if (config != NULL && dns64 == NULL) {
		options = NULL;
		cfg_map_get(config, "options", &options);
		if (options != NULL)
			cfg_map_get(options, "dns64", &dns64);
	}
	if (dns64 == NULL)
		return (ISC_R_SUCCESS);

	for (element = cfg_list_first(dns64);
	     element != NULL;
	     element = cfg_list_next(element))
	{
		map = cfg_listelt_value(element);
		obj = cfg_map_getname(map);

		cfg_obj_asnetprefix(obj, &na, &prefixlen);
		if (na.family != AF_INET6) {
			cfg_obj_log(map, logctx, ISC_LOG_ERROR,
				    "dns64 requires a IPv6 prefix");
			result = ISC_R_FAILURE;
			continue;
		}

		if (prefixlen != 32 && prefixlen != 40 && prefixlen != 48 &&
		    prefixlen != 56 && prefixlen != 64 && prefixlen != 96) {
			cfg_obj_log(map, logctx, ISC_LOG_ERROR,
				    "bad prefix length %u [32/40/48/56/64/96]",
				    prefixlen);
			result = ISC_R_FAILURE;
			continue;
		}

		for (i = 0; acls[i] != NULL; i++) {
			obj = NULL;
			(void)cfg_map_get(map, acls[i], &obj);
			if (obj != NULL) {
				dns_acl_t *acl = NULL;
				isc_result_t tresult;

				tresult = cfg_acl_fromconfig(obj, config,
							     logctx, actx,
							     mctx, 0, &acl);
				if (acl != NULL)
					dns_acl_detach(&acl);
				if (tresult != ISC_R_SUCCESS)
					result = tresult;
			}
		}

		obj = NULL;
		(void)cfg_map_get(map, "suffix", &obj);
		if (obj != NULL) {
			isc_netaddr_fromsockaddr(&sa, cfg_obj_assockaddr(obj));
			if (sa.family != AF_INET6) {
				cfg_obj_log(map, logctx, ISC_LOG_ERROR,
					    "dns64 requires a IPv6 suffix");
				result = ISC_R_FAILURE;
				continue;
			}
			nbytes = prefixlen / 8 + 4;
			if (prefixlen >= 32 && prefixlen <= 64)
				nbytes++;
			if (memcmp(sa.type.in6.s6_addr, zeros, nbytes) != 0) {
				char netaddrbuf[ISC_NETADDR_FORMATSIZE];
				isc_netaddr_format(&sa, netaddrbuf,
						   sizeof(netaddrbuf));
				cfg_obj_log(obj, logctx, ISC_LOG_ERROR,
					    "bad suffix '%s' leading "
					    "%u octets not zeros",
					    netaddrbuf, nbytes);
				result = ISC_R_FAILURE;
			}
		}
	}

	return (result);
}

#define CHECK_RRL(cond, pat, val1, val2)				\
	do {								\
		if (!(cond)) {						\
			cfg_obj_log(obj, logctx, ISC_LOG_ERROR,		\
				    pat, val1, val2);			\
			if (result == ISC_R_SUCCESS)			\
				result = ISC_R_RANGE;			\
		    }							\
	} while (0)

#define CHECK_RRL_RATE(rate, def, max_rate, name)			\
	do {								\
		obj = NULL;						\
		mresult = cfg_map_get(map, name, &obj);			\
		if (mresult == ISC_R_SUCCESS) {				\
			rate = cfg_obj_asuint32(obj);			\
			CHECK_RRL(rate <= max_rate, name" %d > %d",	\
				  rate, max_rate);			\
		}							\
	} while (0)

static isc_result_t
check_ratelimit(cfg_aclconfctx_t *actx, const cfg_obj_t *voptions,
		const cfg_obj_t *config, isc_log_t *logctx, isc_mem_t *mctx)
{
	isc_result_t result = ISC_R_SUCCESS;
	isc_result_t mresult;
	const cfg_obj_t *map = NULL;
	const cfg_obj_t *options;
	const cfg_obj_t *obj;
	int min_entries, i;
	int all_per_second;
	int errors_per_second;
	int nodata_per_second;
	int nxdomains_per_second;
	int referrals_per_second;
	int responses_per_second;
	int slip;

	if (voptions != NULL)
		cfg_map_get(voptions, "rate-limit", &map);
	if (config != NULL && map == NULL) {
		options = NULL;
		cfg_map_get(config, "options", &options);
		if (options != NULL)
			cfg_map_get(options, "rate-limit", &map);
	}
	if (map == NULL)
		return (ISC_R_SUCCESS);

	min_entries = 500;
	obj = NULL;
	mresult = cfg_map_get(map, "min-table-size", &obj);
	if (mresult == ISC_R_SUCCESS) {
		min_entries = cfg_obj_asuint32(obj);
		if (min_entries < 1)
			min_entries = 1;
	}

	obj = NULL;
	mresult = cfg_map_get(map, "max-table-size", &obj);
	if (mresult == ISC_R_SUCCESS) {
		i = cfg_obj_asuint32(obj);
		CHECK_RRL(i >= min_entries,
			  "max-table-size %d < min-table-size %d",
			  i, min_entries);
	}

	CHECK_RRL_RATE(responses_per_second, 0, DNS_RRL_MAX_RATE,
		       "responses-per-second");

	CHECK_RRL_RATE(referrals_per_second, responses_per_second,
		       DNS_RRL_MAX_RATE, "referrals-per-second");
	CHECK_RRL_RATE(nodata_per_second, responses_per_second,
		       DNS_RRL_MAX_RATE, "nodata-per-second");
	CHECK_RRL_RATE(nxdomains_per_second, responses_per_second,
		       DNS_RRL_MAX_RATE, "nxdomains-per-second");
	CHECK_RRL_RATE(errors_per_second, responses_per_second,
		       DNS_RRL_MAX_RATE, "errors-per-second");

	CHECK_RRL_RATE(all_per_second, 0, DNS_RRL_MAX_RATE, "all-per-second");

	CHECK_RRL_RATE(slip, 2, DNS_RRL_MAX_SLIP, "slip");

	obj = NULL;
	mresult = cfg_map_get(map, "window", &obj);
	if (mresult == ISC_R_SUCCESS) {
		i = cfg_obj_asuint32(obj);
		CHECK_RRL(i >= 1 && i <= DNS_RRL_MAX_WINDOW,
			  "window %d < 1 or > %d", i, DNS_RRL_MAX_WINDOW);
	}

	obj = NULL;
	mresult = cfg_map_get(map, "qps-scale", &obj);
	if (mresult == ISC_R_SUCCESS) {
		i = cfg_obj_asuint32(obj);
		CHECK_RRL(i >= 1, "invalid 'qps-scale %d'%s", i, "");
	}

	obj = NULL;
	mresult = cfg_map_get(map, "ipv4-prefix-length", &obj);
	if (mresult == ISC_R_SUCCESS) {
		i = cfg_obj_asuint32(obj);
		CHECK_RRL(i >= 8 && i <= 32,
			  "invalid 'ipv4-prefix-length %d'%s", i, "");
	}

	obj = NULL;
	mresult = cfg_map_get(map, "ipv6-prefix-length", &obj);
	if (mresult == ISC_R_SUCCESS) {
		i = cfg_obj_asuint32(obj);
		CHECK_RRL(i >= 16 && i <= DNS_RRL_MAX_PREFIX,
			  "ipv6-prefix-length %d < 16 or > %d",
			  i, DNS_RRL_MAX_PREFIX);
	}

	obj = NULL;
	(void)cfg_map_get(map, "exempt-clients", &obj);
	if (obj != NULL) {
		dns_acl_t *acl = NULL;
		isc_result_t tresult;

		tresult = cfg_acl_fromconfig(obj, config, logctx, actx,
					     mctx, 0, &acl);
		if (acl != NULL)
			dns_acl_detach(&acl);
		if (result == ISC_R_SUCCESS)
			result = tresult;
	}

	return (result);
}

/*
 * Check allow-recursion and allow-recursion-on acls, and also log a
 * warning if they're inconsistent with the "recursion" option.
 */
static isc_result_t
check_recursionacls(cfg_aclconfctx_t *actx, const cfg_obj_t *voptions,
		    const char *viewname, const cfg_obj_t *config,
		    isc_log_t *logctx, isc_mem_t *mctx)
{
	const cfg_obj_t *options, *aclobj, *obj = NULL;
	dns_acl_t *acl = NULL;
	isc_result_t result = ISC_R_SUCCESS, tresult;
	isc_boolean_t recursion;
	const char *forview = " for view ";
	int i = 0;

	static const char *acls[] = { "allow-recursion", "allow-recursion-on",
				      NULL };

	if (voptions != NULL)
		cfg_map_get(voptions, "recursion", &obj);
	if (obj == NULL && config != NULL) {
		options = NULL;
		cfg_map_get(config, "options", &options);
		if (options != NULL)
			cfg_map_get(options, "recursion", &obj);
	}
	if (obj == NULL)
		recursion = ISC_TRUE;
	else
		recursion = cfg_obj_asboolean(obj);

	if (viewname == NULL) {
		viewname = "";
		forview = "";
	}

	for (i = 0; acls[i] != NULL; i++) {
		aclobj = options = NULL;
		acl = NULL;

		if (voptions != NULL)
			cfg_map_get(voptions, acls[i], &aclobj);
		if (config != NULL && aclobj == NULL) {
			options = NULL;
			cfg_map_get(config, "options", &options);
			if (options != NULL)
				cfg_map_get(options, acls[i], &aclobj);
		}
		if (aclobj == NULL)
			continue;

		tresult = cfg_acl_fromconfig(aclobj, config, logctx,
					    actx, mctx, 0, &acl);

		if (tresult != ISC_R_SUCCESS)
			result = tresult;

		if (acl == NULL)
			continue;

		if (recursion == ISC_FALSE && !dns_acl_isnone(acl)) {
			cfg_obj_log(aclobj, logctx, ISC_LOG_WARNING,
				    "both \"recursion no;\" and "
				    "\"%s\" active%s%s",
				    acls[i], forview, viewname);
		}

		if (acl != NULL)
			dns_acl_detach(&acl);
	}

	return (result);
}

static isc_result_t
check_filteraaaa(cfg_aclconfctx_t *actx, const cfg_obj_t *voptions,
		 const char *viewname, const cfg_obj_t *config,
		 isc_log_t *logctx, isc_mem_t *mctx)
{
	const cfg_obj_t *options, *aclobj, *obj;
	dns_acl_t *acl = NULL;
	isc_result_t result = ISC_R_SUCCESS;
	dns_aaaa_t filter4, filter6;
	const char *forview = " for view ";

	if (viewname == NULL) {
		viewname = "";
		forview = "";
	}

	aclobj = options = NULL;
	acl = NULL;

	if (voptions != NULL)
		cfg_map_get(voptions, "filter-aaaa", &aclobj);
	if (config != NULL && aclobj == NULL) {
		options = NULL;
		cfg_map_get(config, "options", &options);
		if (options != NULL)
			cfg_map_get(options, "filter-aaaa", &aclobj);
	}
	if (aclobj == NULL)
		return (result);

	result = cfg_acl_fromconfig(aclobj, config, logctx,
				    actx, mctx, 0, &acl);
	if (result != ISC_R_SUCCESS)
		goto failure;

	obj = NULL;
	if (voptions != NULL)
		cfg_map_get(voptions, "filter-aaaa-on-v4", &obj);
	if (obj == NULL && config != NULL) {
		options = NULL;
		cfg_map_get(config, "options", &options);
		if (options != NULL)
			cfg_map_get(options, "filter-aaaa-on-v4", &obj);
	}

	if (obj == NULL)
		filter4 = dns_aaaa_ok;		/* default */
	else if (cfg_obj_isboolean(obj))
		filter4 = cfg_obj_asboolean(obj) ? dns_aaaa_filter :
						  dns_aaaa_ok;
	else
		filter4 = dns_aaaa_break_dnssec; 	/* break-dnssec */

	obj = NULL;
	if (voptions != NULL)
		cfg_map_get(voptions, "filter-aaaa-on-v6", &obj);
	if (obj == NULL && config != NULL) {
		options = NULL;
		cfg_map_get(config, "options", &options);
		if (options != NULL)
			cfg_map_get(options, "filter-aaaa-on-v6", &obj);
	}

	if (obj == NULL)
		filter6 = dns_aaaa_ok;		/* default */
	else if (cfg_obj_isboolean(obj))
		filter6 = cfg_obj_asboolean(obj) ? dns_aaaa_filter :
						  dns_aaaa_ok;
	else
		filter6 = dns_aaaa_break_dnssec; 	/* break-dnssec */

	if ((filter4 != dns_aaaa_ok || filter6 != dns_aaaa_ok) &&
	    dns_acl_isnone(acl))
	{
		cfg_obj_log(aclobj, logctx, ISC_LOG_WARNING,
			    "\"filter-aaaa\" is 'none;' but "
			    "either filter-aaaa-on-v4 or filter-aaaa-on-v6 "
			    "is enabled%s%s", forview, viewname);
		result = ISC_R_FAILURE;
	} else if (filter4 == dns_aaaa_ok && filter6 == dns_aaaa_ok &&
		   !dns_acl_isnone(acl))
	{
		cfg_obj_log(aclobj, logctx, ISC_LOG_WARNING,
			    "\"filter-aaaa\" is set but "
			    "neither filter-aaaa-on-v4 or filter-aaaa-on-v6 "
			    "is enabled%s%s", forview, viewname);
		result = ISC_R_FAILURE;
	}

 failure:
	if (acl != NULL)
		dns_acl_detach(&acl);

	return (result);
}

typedef struct {
	const char *name;
	unsigned int scale;
	unsigned int max;
} intervaltable;

typedef enum {
	optlevel_config,
	optlevel_options,
	optlevel_view,
	optlevel_zone
} optlevel_t;

static isc_result_t
check_dscp(const cfg_obj_t *options, isc_log_t *logctx) {
       isc_result_t result = ISC_R_SUCCESS;
       const cfg_obj_t *obj = NULL;

       /*
	* Check that DSCP setting is within range
	*/
       obj = NULL;
       (void)cfg_map_get(options, "dscp", &obj);
       if (obj != NULL) {
	       isc_uint32_t dscp = cfg_obj_asuint32(obj);
	       if (dscp >= 64) {
		       cfg_obj_log(obj, logctx, ISC_LOG_ERROR,
				   "'dscp' out of range (0-63)");
		       result = ISC_R_FAILURE;
	       }
       }

       return (result);
}

static isc_result_t
check_name(const char *str) {
	dns_fixedname_t fixed;

	dns_fixedname_init(&fixed);
	return (dns_name_fromstring(dns_fixedname_name(&fixed), str, 0, NULL));
}

static isc_result_t
check_options(const cfg_obj_t *options, isc_log_t *logctx, isc_mem_t *mctx,
	      optlevel_t optlevel)
{
	isc_result_t result = ISC_R_SUCCESS;
	isc_result_t tresult;
	unsigned int i;
	const cfg_obj_t *obj = NULL;
	const cfg_obj_t *resignobj = NULL;
	const cfg_listelt_t *element;
	isc_symtab_t *symtab = NULL;
	dns_fixedname_t fixed;
	const char *str;
	dns_name_t *name;
#ifdef ISC_PLATFORM_USESIT
	isc_buffer_t b;
#endif

	static intervaltable intervals[] = {
	{ "cleaning-interval", 60, 28 * 24 * 60 },	/* 28 days */
	{ "heartbeat-interval", 60, 28 * 24 * 60 },	/* 28 days */
	{ "interface-interval", 60, 28 * 24 * 60 },	/* 28 days */
	{ "max-transfer-idle-in", 60, 28 * 24 * 60 },	/* 28 days */
	{ "max-transfer-idle-out", 60, 28 * 24 * 60 },	/* 28 days */
	{ "max-transfer-time-in", 60, 28 * 24 * 60 },	/* 28 days */
	{ "max-transfer-time-out", 60, 28 * 24 * 60 },	/* 28 days */
	{ "statistics-interval", 60, 28 * 24 * 60 },	/* 28 days */
	};

	static const char *server_contact[] = {
		"empty-server", "empty-contact",
		"dns64-server", "dns64-contact",
		NULL
	};

	/*
	 * Check that fields specified in units of time other than seconds
	 * have reasonable values.
	 */
	for (i = 0; i < sizeof(intervals) / sizeof(intervals[0]); i++) {
		isc_uint32_t val;
		obj = NULL;
		(void)cfg_map_get(options, intervals[i].name, &obj);
		if (obj == NULL)
			continue;
		val = cfg_obj_asuint32(obj);
		if (val > intervals[i].max) {
			cfg_obj_log(obj, logctx, ISC_LOG_ERROR,
				    "%s '%u' is out of range (0..%u)",
				    intervals[i].name, val,
				    intervals[i].max);
			result = ISC_R_RANGE;
		} else if (val > (ISC_UINT32_MAX / intervals[i].scale)) {
			cfg_obj_log(obj, logctx, ISC_LOG_ERROR,
				    "%s '%d' is out of range",
				    intervals[i].name, val);
			result = ISC_R_RANGE;
		}
	}

	obj = NULL;
	cfg_map_get(options, "max-rsa-exponent-size", &obj);
	if (obj != NULL) {
		isc_uint32_t val;

		val = cfg_obj_asuint32(obj);
		if (val != 0 && (val < 35 || val > 4096)) {
			cfg_obj_log(obj, logctx, ISC_LOG_ERROR,
				    "max-rsa-exponent-size '%u' is out of "
				    "range (35..4096)", val);
			result = ISC_R_RANGE;
		}
	}

	obj = NULL;
	cfg_map_get(options, "sig-validity-interval", &obj);
	if (obj != NULL) {
		isc_uint32_t validity, resign = 0;

		validity = cfg_obj_asuint32(cfg_tuple_get(obj, "validity"));
		resignobj = cfg_tuple_get(obj, "re-sign");
		if (!cfg_obj_isvoid(resignobj))
			resign = cfg_obj_asuint32(resignobj);

		if (validity > 3660 || validity == 0) { /* 10 years */
			cfg_obj_log(obj, logctx, ISC_LOG_ERROR,
				    "%s '%u' is out of range (1..3660)",
				    "sig-validity-interval", validity);
			result = ISC_R_RANGE;
		}

		if (!cfg_obj_isvoid(resignobj)) {
			if (resign > 3660 || resign == 0) { /* 10 years */
				cfg_obj_log(obj, logctx, ISC_LOG_ERROR,
					    "%s '%u' is out of range (1..3660)",
					    "sig-validity-interval (re-sign)",
					    validity);
				result = ISC_R_RANGE;
			} else if ((validity > 7 && validity < resign) ||
				   (validity <= 7 && validity * 24 < resign)) {
				cfg_obj_log(obj, logctx, ISC_LOG_ERROR,
					    "validity interval (%u days) "
					    "less than re-signing interval "
					    "(%u %s)", validity, resign,
					    (validity > 7) ? "days" : "hours");
				result = ISC_R_RANGE;
			}
		}
	}

	obj = NULL;
	(void)cfg_map_get(options, "preferred-glue", &obj);
	if (obj != NULL) {
		str = cfg_obj_asstring(obj);
		if (strcasecmp(str, "a") != 0 &&
		    strcasecmp(str, "aaaa") != 0 &&
		    strcasecmp(str, "none") != 0)
			cfg_obj_log(obj, logctx, ISC_LOG_ERROR,
				    "preferred-glue unexpected value '%s'",
				    str);
	}

	obj = NULL;
	(void)cfg_map_get(options, "root-delegation-only", &obj);
	if (obj != NULL) {
		if (!cfg_obj_isvoid(obj)) {
			for (element = cfg_list_first(obj);
			     element != NULL;
			     element = cfg_list_next(element)) {
				const cfg_obj_t *exclude;

				exclude = cfg_listelt_value(element);
				str = cfg_obj_asstring(exclude);
				tresult = check_name(str);
				if (tresult != ISC_R_SUCCESS) {
					cfg_obj_log(obj, logctx, ISC_LOG_ERROR,
						    "bad domain name '%s'",
						    str);
					result = tresult;
				}
			}
		}
	}

	/*
	 * Set supported DNSSEC algorithms.
	 */
	obj = NULL;
	(void)cfg_map_get(options, "disable-algorithms", &obj);
	if (obj != NULL) {
		for (element = cfg_list_first(obj);
		     element != NULL;
		     element = cfg_list_next(element))
		{
			obj = cfg_listelt_value(element);
			tresult = disabled_algorithms(obj, logctx);
			if (tresult != ISC_R_SUCCESS)
				result = tresult;
		}
	}

	/*
	 * Set supported DS/DLV digest types.
	 */
	obj = NULL;
	(void)cfg_map_get(options, "disable-ds-digests", &obj);
	if (obj != NULL) {
		for (element = cfg_list_first(obj);
		     element != NULL;
		     element = cfg_list_next(element))
		{
			obj = cfg_listelt_value(element);
			tresult = disabled_ds_digests(obj, logctx);
			if (tresult != ISC_R_SUCCESS)
				result = tresult;
		}
	}

	dns_fixedname_init(&fixed);
	name = dns_fixedname_name(&fixed);

	/*
	 * Check the DLV zone name.
	 */
	obj = NULL;
	(void)cfg_map_get(options, "dnssec-lookaside", &obj);
	if (obj != NULL) {
		tresult = isc_symtab_create(mctx, 100, freekey, mctx,
					    ISC_FALSE, &symtab);
		if (tresult != ISC_R_SUCCESS)
			result = tresult;
		for (element = cfg_list_first(obj);
		     element != NULL;
		     element = cfg_list_next(element))
		{
			const char *dlv;
			const cfg_obj_t *dlvobj, *anchor;

			obj = cfg_listelt_value(element);

			anchor = cfg_tuple_get(obj, "trust-anchor");
			dlvobj = cfg_tuple_get(obj, "domain");
			dlv = cfg_obj_asstring(dlvobj);

			/*
			 * If domain is "auto" or "no" and trust anchor
			 * is missing, skip remaining tests
			 */
			if (cfg_obj_isvoid(anchor)) {
				if (!strcasecmp(dlv, "no")) {
					continue;
				}
				if (!strcasecmp(dlv, "auto")) {
					cfg_obj_log(obj, logctx,
						    ISC_LOG_WARNING,
						    "dnssec-lookaside 'auto' "
						    "is no longer supported");
					continue;
				}
			}

			tresult = dns_name_fromstring(name, dlv, 0, NULL);
			if (tresult != ISC_R_SUCCESS) {
				cfg_obj_log(obj, logctx, ISC_LOG_ERROR,
					    "bad domain name '%s'", dlv);
				result = tresult;
				continue;
			}
			if (symtab != NULL) {
				tresult = nameexist(obj, dlv, 1, symtab,
						    "dnssec-lookaside '%s': "
						    "already exists; previous "
						    "definition: %s:%u",
						    logctx, mctx);
				if (tresult != ISC_R_SUCCESS &&
				    result == ISC_R_SUCCESS)
					result = tresult;
			}

			/*
			 * XXXMPA to be removed when multiple lookaside
			 * namespaces are supported.
			 */
			if (!dns_name_equal(dns_rootname, name)) {
				cfg_obj_log(obj, logctx, ISC_LOG_ERROR,
					    "dnssec-lookaside '%s': "
					    "non-root not yet supported", dlv);
				if (result == ISC_R_SUCCESS)
					result = ISC_R_FAILURE;
			}

			if (cfg_obj_isvoid(anchor)) {
				cfg_obj_log(obj, logctx, ISC_LOG_ERROR,
					    "dnssec-lookaside requires "
					    "either or 'no' or a "
					    "domain and trust anchor");
				if (result == ISC_R_SUCCESS)
					result = ISC_R_FAILURE;
				continue;
			}

			dlv = cfg_obj_asstring(anchor);
			tresult = dns_name_fromstring(name, dlv, 0, NULL);
			if (tresult != ISC_R_SUCCESS) {
				cfg_obj_log(anchor, logctx, ISC_LOG_ERROR,
					    "bad domain name '%s'", dlv);
				if (result == ISC_R_SUCCESS)
					result = tresult;
				continue;
			}
			if (dns_name_equal(&dlviscorg, name)) {
				cfg_obj_log(anchor, logctx, ISC_LOG_WARNING,
					    "dlv.isc.org has been shut down: "
					    "dnssec-lookaside ignored");
				continue;
			}
		}

		if (symtab != NULL)
			isc_symtab_destroy(&symtab);
	}

	/*
	 * Check auto-dnssec at the view/options level
	 */
	obj = NULL;
	(void)cfg_map_get(options, "auto-dnssec", &obj);
	if (obj != NULL) {
		const char *arg = cfg_obj_asstring(obj);
		if (optlevel != optlevel_zone && strcasecmp(arg, "off") != 0) {
			cfg_obj_log(obj, logctx, ISC_LOG_ERROR,
				    "auto-dnssec may only be activated at the "
				    "zone level");
			result = ISC_R_FAILURE;
		}
	}

	/*
	 * Check dnssec-must-be-secure.
	 */
	obj = NULL;
	(void)cfg_map_get(options, "dnssec-must-be-secure", &obj);
	if (obj != NULL) {
		tresult = isc_symtab_create(mctx, 100, freekey, mctx,
					    ISC_FALSE, &symtab);
		if (tresult != ISC_R_SUCCESS)
			result = tresult;
		for (element = cfg_list_first(obj);
		     element != NULL;
		     element = cfg_list_next(element))
		{
			obj = cfg_listelt_value(element);
			tresult = mustbesecure(obj, symtab, logctx, mctx);
			if (tresult != ISC_R_SUCCESS)
				result = tresult;
		}
		if (symtab != NULL)
			isc_symtab_destroy(&symtab);
	}

	/*
	 * Check server/contacts for syntactic validity.
	 */
	for (i= 0; server_contact[i] != NULL; i++) {
		obj = NULL;
		(void)cfg_map_get(options, server_contact[i], &obj);
		if (obj != NULL) {
			str = cfg_obj_asstring(obj);
			if (check_name(str) != ISC_R_SUCCESS) {
				cfg_obj_log(obj, logctx, ISC_LOG_ERROR,
					    "%s: invalid name '%s'",
					    server_contact[i], str);
				result = ISC_R_FAILURE;
			}
		}
	}

	/*
	 * Check empty zone configuration.
	 */
	obj = NULL;
	(void)cfg_map_get(options, "disable-empty-zone", &obj);
	for (element = cfg_list_first(obj);
	     element != NULL;
	     element = cfg_list_next(element))
	{
		obj = cfg_listelt_value(element);
		str = cfg_obj_asstring(obj);
		if (check_name(str) != ISC_R_SUCCESS) {
			cfg_obj_log(obj, logctx, ISC_LOG_ERROR,
				    "disable-empty-zone: invalid name '%s'",
				    str);
			result = ISC_R_FAILURE;
		}
	}

	/*
	 * Check that server-id is not too long.
	 * 1024 bytes should be big enough.
	 */
	obj = NULL;
	(void)cfg_map_get(options, "server-id", &obj);
	if (obj != NULL && cfg_obj_isstring(obj) &&
	    strlen(cfg_obj_asstring(obj)) > 1024U) {
		cfg_obj_log(obj, logctx, ISC_LOG_ERROR,
			    "'server-id' too big (>1024 bytes)");
		result = ISC_R_FAILURE;
	}

	tresult = check_dscp(options, logctx);
	if (tresult != ISC_R_SUCCESS)
		result = tresult;

#ifdef ISC_PLATFORM_USESIT
	obj = NULL;
	(void) cfg_map_get(options, "sit-secret", &obj);
	if (obj != NULL) {
		unsigned char secret[32];

		memset(secret, 0, sizeof(secret));
		isc_buffer_init(&b, secret, sizeof(secret));
		tresult = isc_hex_decodestring(cfg_obj_asstring(obj), &b);
		if (tresult == ISC_R_NOSPACE) {
			cfg_obj_log(obj, logctx, ISC_LOG_ERROR,
				    "sit-secret: too long");
		} else if (tresult != ISC_R_SUCCESS) {
			cfg_obj_log(obj, logctx, ISC_LOG_ERROR,
				    "sit-secret: invalid hex string");
		}
		if (tresult != ISC_R_SUCCESS)
			result = tresult;
#ifdef AES_SIT
		if (tresult == ISC_R_SUCCESS &&
		    isc_buffer_usedlength(&b) != ISC_AES128_KEYLENGTH) {
			cfg_obj_log(obj, logctx, ISC_LOG_ERROR,
				    "AES sit-secret must be on 128 bits");
			result = ISC_R_RANGE;
		}
#endif
#ifdef HMAC_SHA1_SIT
		if (tresult == ISC_R_SUCCESS &&
		    isc_buffer_usedlength(&b) != ISC_SHA1_DIGESTLENGTH) {
			cfg_obj_log(obj, logctx, ISC_LOG_ERROR,
				    "SHA1 sit-secret must be on 160 bits");
			result = ISC_R_RANGE;
		}
#endif
#ifdef HMAC_SHA256_SIT
		if (tresult == ISC_R_SUCCESS &&
		    isc_buffer_usedlength(&b) != ISC_SHA256_DIGESTLENGTH) {
			cfg_obj_log(obj, logctx, ISC_LOG_ERROR,
				    "SHA256 sit-secret must be on 256 bits");
			result = ISC_R_RANGE;
		}
#endif
	}
#endif

	return (result);
}

static isc_result_t
get_masters_def(const cfg_obj_t *cctx, const char *name, const cfg_obj_t **ret) {
	isc_result_t result;
	const cfg_obj_t *masters = NULL;
	const cfg_listelt_t *elt;

	result = cfg_map_get(cctx, "masters", &masters);
	if (result != ISC_R_SUCCESS)
		return (result);
	for (elt = cfg_list_first(masters);
	     elt != NULL;
	     elt = cfg_list_next(elt)) {
		const cfg_obj_t *list;
		const char *listname;

		list = cfg_listelt_value(elt);
		listname = cfg_obj_asstring(cfg_tuple_get(list, "name"));

		if (strcasecmp(listname, name) == 0) {
			*ret = list;
			return (ISC_R_SUCCESS);
		}
	}
	return (ISC_R_NOTFOUND);
}

static isc_result_t
validate_masters(const cfg_obj_t *obj, const cfg_obj_t *config,
		 isc_uint32_t *countp, isc_log_t *logctx, isc_mem_t *mctx)
{
	isc_result_t result = ISC_R_SUCCESS;
	isc_result_t tresult;
	isc_uint32_t count = 0;
	isc_symtab_t *symtab = NULL;
	isc_symvalue_t symvalue;
	const cfg_listelt_t *element;
	const cfg_listelt_t **stack = NULL;
	isc_uint32_t stackcount = 0, pushed = 0;
	const cfg_obj_t *list;

	REQUIRE(countp != NULL);
	result = isc_symtab_create(mctx, 100, NULL, NULL, ISC_FALSE, &symtab);
	if (result != ISC_R_SUCCESS) {
		*countp = count;
		return (result);
	}

 newlist:
	list = cfg_tuple_get(obj, "addresses");
	element = cfg_list_first(list);
 resume:
	for ( ;
	     element != NULL;
	     element = cfg_list_next(element))
	{
		const char *listname;
		const cfg_obj_t *addr;
		const cfg_obj_t *key;

		addr = cfg_tuple_get(cfg_listelt_value(element),
				     "masterselement");
		key = cfg_tuple_get(cfg_listelt_value(element), "key");

		if (cfg_obj_issockaddr(addr)) {
			count++;
			continue;
		}
		if (!cfg_obj_isvoid(key)) {
			cfg_obj_log(key, logctx, ISC_LOG_ERROR,
				    "unexpected token '%s'",
				    cfg_obj_asstring(key));
			if (result == ISC_R_SUCCESS)
				result = ISC_R_FAILURE;
		}
		listname = cfg_obj_asstring(addr);
		symvalue.as_cpointer = addr;
		tresult = isc_symtab_define(symtab, listname, 1, symvalue,
					    isc_symexists_reject);
		if (tresult == ISC_R_EXISTS)
			continue;
		tresult = get_masters_def(config, listname, &obj);
		if (tresult != ISC_R_SUCCESS) {
			if (result == ISC_R_SUCCESS)
				result = tresult;
			cfg_obj_log(addr, logctx, ISC_LOG_ERROR,
				    "unable to find masters list '%s'",
				    listname);
			continue;
		}
		/* Grow stack? */
		if (stackcount == pushed) {
			void * newstack;
			isc_uint32_t newlen = stackcount + 16;
			size_t newsize, oldsize;

			newsize = newlen * sizeof(*stack);
			oldsize = stackcount * sizeof(*stack);
			newstack = isc_mem_get(mctx, newsize);
			if (newstack == NULL)
				goto cleanup;
			if (stackcount != 0) {
				void *ptr;

				DE_CONST(stack, ptr);
				memmove(newstack, stack, oldsize);
				isc_mem_put(mctx, ptr, oldsize);
			}
			stack = newstack;
			stackcount = newlen;
		}
		stack[pushed++] = cfg_list_next(element);
		goto newlist;
	}
	if (pushed != 0) {
		element = stack[--pushed];
		goto resume;
	}
 cleanup:
	if (stack != NULL) {
		void *ptr;

		DE_CONST(stack, ptr);
		isc_mem_put(mctx, ptr, stackcount * sizeof(*stack));
	}
	isc_symtab_destroy(&symtab);
	*countp = count;
	return (result);
}

static isc_result_t
check_update_policy(const cfg_obj_t *policy, isc_log_t *logctx) {
	isc_result_t result = ISC_R_SUCCESS;
	isc_result_t tresult;
	const cfg_listelt_t *element;
	const cfg_listelt_t *element2;
	dns_fixedname_t fixed_id, fixed_name;
	dns_name_t *id, *name;
	const char *str;

	/* Check for "update-policy local;" */
	if (cfg_obj_isstring(policy) &&
	    strcmp("local", cfg_obj_asstring(policy)) == 0)
		return (ISC_R_SUCCESS);

	/* Now check the grant policy */
	for (element = cfg_list_first(policy);
	     element != NULL;
	     element = cfg_list_next(element))
	{
		const cfg_obj_t *stmt = cfg_listelt_value(element);
		const cfg_obj_t *identity = cfg_tuple_get(stmt, "identity");
		const cfg_obj_t *matchtype = cfg_tuple_get(stmt, "matchtype");
		const cfg_obj_t *dname = cfg_tuple_get(stmt, "name");
		const cfg_obj_t *typelist = cfg_tuple_get(stmt, "types");
		dns_ssumatchtype_t mtype;

		dns_fixedname_init(&fixed_id);
		dns_fixedname_init(&fixed_name);
		id = dns_fixedname_name(&fixed_id);
		name = dns_fixedname_name(&fixed_name);

		tresult = dns_ssu_mtypefromstring(cfg_obj_asstring(matchtype),
						  &mtype);
		if (tresult != ISC_R_SUCCESS) {
			cfg_obj_log(identity, logctx, ISC_LOG_ERROR,
				    "has a bad match-type");
		}

		str = cfg_obj_asstring(identity);
		tresult = dns_name_fromstring(id, str, 1, NULL);
		if (tresult != ISC_R_SUCCESS) {
			cfg_obj_log(identity, logctx, ISC_LOG_ERROR,
				    "'%s' is not a valid name", str);
			result = tresult;
		}

		/*
		 * There is no name field for subzone.
		 */
		if (tresult == ISC_R_SUCCESS &&
		    mtype != dns_ssumatchtype_subdomain)
		{
			str = cfg_obj_asstring(dname);
			tresult = dns_name_fromstring(name, str, 0, NULL);
			if (tresult != ISC_R_SUCCESS) {
				cfg_obj_log(dname, logctx, ISC_LOG_ERROR,
					    "'%s' is not a valid name", str);
				result = tresult;
			}
		}

		if (tresult == ISC_R_SUCCESS &&
		    mtype == dns_ssumatchtype_wildcard &&
		    !dns_name_iswildcard(name))
		{
			cfg_obj_log(identity, logctx, ISC_LOG_ERROR,
				    "'%s' is not a wildcard", str);
			result = ISC_R_FAILURE;
		}

		/*
		 * For some match types, the name should be a placeholder
		 * value, either "." or the same as identity.
		 */
		switch (mtype) {
		case dns_ssumatchtype_self:
		case dns_ssumatchtype_selfsub:
		case dns_ssumatchtype_selfwild:
			if (tresult == ISC_R_SUCCESS &&
			    (!dns_name_equal(id, name) &&
			     !dns_name_equal(dns_rootname, name))) {
				cfg_obj_log(identity, logctx, ISC_LOG_ERROR,
					    "identity and name fields are not "
					    "the same");
				result = ISC_R_FAILURE;
			}
			break;
		case dns_ssumatchtype_selfkrb5:
		case dns_ssumatchtype_selfms:
		case dns_ssumatchtype_subdomainms:
		case dns_ssumatchtype_subdomainkrb5:
		case dns_ssumatchtype_tcpself:
		case dns_ssumatchtype_6to4self:
			if (tresult == ISC_R_SUCCESS &&
			    !dns_name_equal(dns_rootname, name)) {
				cfg_obj_log(identity, logctx, ISC_LOG_ERROR,
					    "name field not set to "
					    "placeholder value '.'");
				result = ISC_R_FAILURE;
			}
			break;
		case dns_ssumatchtype_name:
		case dns_ssumatchtype_subdomain:
		case dns_ssumatchtype_wildcard:
		case dns_ssumatchtype_external:
		case dns_ssumatchtype_local:
			break;
		default:
			INSIST(0);
		}

		for (element2 = cfg_list_first(typelist);
		     element2 != NULL;
		     element2 = cfg_list_next(element2))
		{
			const cfg_obj_t *typeobj;
			isc_textregion_t r;
			dns_rdatatype_t type;

			typeobj = cfg_listelt_value(element2);
			DE_CONST(cfg_obj_asstring(typeobj), r.base);
			r.length = strlen(r.base);

			tresult = dns_rdatatype_fromtext(&type, &r);
			if (tresult != ISC_R_SUCCESS) {
				cfg_obj_log(typeobj, logctx, ISC_LOG_ERROR,
					    "'%s' is not a valid type", r.base);
				result = tresult;
			}
		}
	}
	return (result);
}

#define MASTERZONE	1
#define SLAVEZONE	2
#define STUBZONE	4
#define HINTZONE	8
#define FORWARDZONE	16
#define DELEGATIONZONE	32
#define STATICSTUBZONE	64
#define REDIRECTZONE	128
#define INVIEWZONE	256
#define STREDIRECTZONE	0	/* Set to REDIRECTZONE to allow xfr-in. */
#define CHECKACL	512

typedef struct {
	const char *name;
	int allowed;
} optionstable;

static isc_result_t
check_nonzero(const cfg_obj_t *options, isc_log_t *logctx) {
	isc_result_t result = ISC_R_SUCCESS;
	const cfg_obj_t *obj = NULL;
	unsigned int i;

	static const char *nonzero[] = { "max-retry-time", "min-retry-time",
				 "max-refresh-time", "min-refresh-time" };
	/*
	 * Check if value is zero.
	 */
	for (i = 0; i < sizeof(nonzero) / sizeof(nonzero[0]); i++) {
		obj = NULL;
		if (cfg_map_get(options, nonzero[i], &obj) == ISC_R_SUCCESS &&
		    cfg_obj_asuint32(obj) == 0) {
			cfg_obj_log(obj, logctx, ISC_LOG_ERROR,
				    "'%s' must not be zero", nonzero[i]);
			result = ISC_R_FAILURE;
		}
	}
	return (result);
}

static isc_result_t
check_zoneconf(const cfg_obj_t *zconfig, const cfg_obj_t *voptions,
	       const cfg_obj_t *config, isc_symtab_t *symtab,
	       isc_symtab_t *files, isc_symtab_t *inview,
	       const char *viewname, dns_rdataclass_t defclass,
	       cfg_aclconfctx_t *actx, isc_log_t *logctx, isc_mem_t *mctx)
{
	const char *znamestr;
	const char *typestr = NULL;
	const char *target = NULL;
	unsigned int ztype;
	const cfg_obj_t *zoptions, *goptions = NULL;
	const cfg_obj_t *obj = NULL;
	const cfg_obj_t *inviewobj = NULL;
	isc_result_t result = ISC_R_SUCCESS;
	isc_result_t tresult;
	unsigned int i;
	dns_rdataclass_t zclass;
	dns_fixedname_t fixedname;
	dns_name_t *zname = NULL;
	isc_buffer_t b;
	isc_boolean_t root = ISC_FALSE;
	isc_boolean_t rfc1918 = ISC_FALSE;
	isc_boolean_t ula = ISC_FALSE;
	const cfg_listelt_t *element;
	isc_boolean_t dlz;
	dns_masterformat_t masterformat;
	isc_boolean_t ddns = ISC_FALSE;

	static optionstable options[] = {
	{ "allow-notify", SLAVEZONE | CHECKACL },
	{ "allow-query", MASTERZONE | SLAVEZONE | STUBZONE | REDIRECTZONE |
	  CHECKACL | STATICSTUBZONE },
	{ "allow-transfer", MASTERZONE | SLAVEZONE | CHECKACL },
	{ "allow-update", MASTERZONE | CHECKACL },
	{ "allow-update-forwarding", SLAVEZONE | CHECKACL },
	{ "also-notify", MASTERZONE | SLAVEZONE },
	{ "auto-dnssec", MASTERZONE | SLAVEZONE },
	{ "check-dup-records", MASTERZONE },
	{ "check-mx", MASTERZONE },
	{ "check-mx-cname", MASTERZONE },
	{ "check-srv-cname", MASTERZONE },
	{ "check-wildcard", MASTERZONE },
	{ "database", MASTERZONE | SLAVEZONE | STUBZONE | REDIRECTZONE },
	{ "delegation-only", HINTZONE | STUBZONE | FORWARDZONE |
	  DELEGATIONZONE },
	{ "dialup", MASTERZONE | SLAVEZONE | STUBZONE | STREDIRECTZONE },
	{ "dnssec-dnskey-kskonly", MASTERZONE | SLAVEZONE },
	{ "dnssec-loadkeys-interval", MASTERZONE | SLAVEZONE },
	{ "dnssec-secure-to-insecure", MASTERZONE },
	{ "file", MASTERZONE | SLAVEZONE | STUBZONE | HINTZONE | REDIRECTZONE },
	{ "forward", MASTERZONE | SLAVEZONE | STUBZONE | STATICSTUBZONE |
	  FORWARDZONE },
	{ "forwarders", MASTERZONE | SLAVEZONE | STUBZONE | STATICSTUBZONE |
	  FORWARDZONE },
	{ "integrity-check", MASTERZONE },
	{ "ixfr-base", MASTERZONE | SLAVEZONE },
	{ "ixfr-tmp-file", MASTERZONE | SLAVEZONE },
	{ "journal", MASTERZONE | SLAVEZONE | STREDIRECTZONE },
	{ "key-directory", MASTERZONE | SLAVEZONE },
	{ "maintain-ixfr-base", MASTERZONE | SLAVEZONE | STREDIRECTZONE },
	{ "masterfile-format", MASTERZONE | SLAVEZONE | STUBZONE |
	  REDIRECTZONE },
	{ "masters", SLAVEZONE | STUBZONE | REDIRECTZONE },
	{ "max-ixfr-log-size", MASTERZONE | SLAVEZONE | STREDIRECTZONE },
	{ "max-records", MASTERZONE | SLAVEZONE | STUBZONE | STREDIRECTZONE |
	  STATICSTUBZONE | REDIRECTZONE },
	{ "max-refresh-time", SLAVEZONE | STUBZONE | STREDIRECTZONE },
	{ "max-retry-time", SLAVEZONE | STUBZONE | STREDIRECTZONE },
	{ "max-transfer-idle-in", SLAVEZONE | STUBZONE | STREDIRECTZONE },
	{ "max-transfer-idle-out", MASTERZONE | SLAVEZONE },
	{ "max-transfer-time-in", SLAVEZONE | STUBZONE | STREDIRECTZONE },
	{ "max-transfer-time-out", MASTERZONE | SLAVEZONE },
	{ "max-zone-ttl", MASTERZONE | REDIRECTZONE },
	{ "min-refresh-time", SLAVEZONE | STUBZONE | STREDIRECTZONE },
	{ "min-retry-time", SLAVEZONE | STUBZONE | STREDIRECTZONE },
	{ "notify", MASTERZONE | SLAVEZONE },
	{ "notify-source", MASTERZONE | SLAVEZONE },
	{ "notify-source-v6", MASTERZONE | SLAVEZONE },
	{ "pubkey", MASTERZONE | SLAVEZONE | STUBZONE },
	{ "request-ixfr", SLAVEZONE | REDIRECTZONE },
	{ "server-addresses", STATICSTUBZONE },
	{ "server-names", STATICSTUBZONE },
	{ "sig-re-signing-interval", MASTERZONE | SLAVEZONE },
	{ "sig-signing-nodes", MASTERZONE | SLAVEZONE },
	{ "sig-signing-signatures", MASTERZONE | SLAVEZONE },
	{ "sig-signing-type", MASTERZONE | SLAVEZONE },
	{ "sig-validity-interval", MASTERZONE | SLAVEZONE },
	{ "signing", MASTERZONE | SLAVEZONE },
	{ "transfer-source", SLAVEZONE | STUBZONE | STREDIRECTZONE },
	{ "transfer-source-v6", SLAVEZONE | STUBZONE | STREDIRECTZONE },
	{ "try-tcp-refresh", SLAVEZONE | STREDIRECTZONE },
	{ "update-check-ksk", MASTERZONE | SLAVEZONE },
	{ "update-policy", MASTERZONE },
	{ "zone-statistics", MASTERZONE | SLAVEZONE | STUBZONE |
	  STATICSTUBZONE | REDIRECTZONE },
	};

	static optionstable dialups[] = {
	{ "notify", MASTERZONE | SLAVEZONE | STREDIRECTZONE },
	{ "notify-passive", SLAVEZONE | STREDIRECTZONE },
	{ "refresh", SLAVEZONE | STUBZONE | STREDIRECTZONE },
	{ "passive", SLAVEZONE | STUBZONE | STREDIRECTZONE },
	};

	znamestr = cfg_obj_asstring(cfg_tuple_get(zconfig, "name"));

	zoptions = cfg_tuple_get(zconfig, "options");

	if (config != NULL)
		cfg_map_get(config, "options", &goptions);

	inviewobj = NULL;
	(void)cfg_map_get(zoptions, "in-view", &inviewobj);
	if (inviewobj != NULL) {
		target = cfg_obj_asstring(inviewobj);
		ztype = INVIEWZONE;
	} else {
		obj = NULL;
		(void)cfg_map_get(zoptions, "type", &obj);
		if (obj == NULL) {
			cfg_obj_log(zconfig, logctx, ISC_LOG_ERROR,
				    "zone '%s': type not present", znamestr);
			return (ISC_R_FAILURE);
		}

		typestr = cfg_obj_asstring(obj);
		if (strcasecmp(typestr, "master") == 0) {
			ztype = MASTERZONE;
		} else if (strcasecmp(typestr, "slave") == 0) {
			ztype = SLAVEZONE;
		} else if (strcasecmp(typestr, "stub") == 0) {
			ztype = STUBZONE;
		} else if (strcasecmp(typestr, "static-stub") == 0) {
			ztype = STATICSTUBZONE;
		} else if (strcasecmp(typestr, "forward") == 0) {
			ztype = FORWARDZONE;
		} else if (strcasecmp(typestr, "hint") == 0) {
			ztype = HINTZONE;
		} else if (strcasecmp(typestr, "delegation-only") == 0) {
			ztype = DELEGATIONZONE;
		} else if (strcasecmp(typestr, "redirect") == 0) {
			ztype = REDIRECTZONE;
		} else {
			cfg_obj_log(obj, logctx, ISC_LOG_ERROR,
				    "zone '%s': invalid type %s",
				    znamestr, typestr);
			return (ISC_R_FAILURE);
		}

		if (ztype == REDIRECTZONE && strcmp(znamestr, ".") != 0) {
			cfg_obj_log(zconfig, logctx, ISC_LOG_ERROR,
				    "redirect zones must be called \".\"");
			return (ISC_R_FAILURE);
		}
	}

	obj = cfg_tuple_get(zconfig, "class");
	if (cfg_obj_isstring(obj)) {
		isc_textregion_t r;

		DE_CONST(cfg_obj_asstring(obj), r.base);
		r.length = strlen(r.base);
		result = dns_rdataclass_fromtext(&zclass, &r);
		if (result != ISC_R_SUCCESS) {
			cfg_obj_log(obj, logctx, ISC_LOG_ERROR,
				    "zone '%s': invalid class %s",
				    znamestr, r.base);
			return (ISC_R_FAILURE);
		}
		if (zclass != defclass) {
			cfg_obj_log(obj, logctx, ISC_LOG_ERROR,
				    "zone '%s': class '%s' does not "
				    "match view/default class",
				    znamestr, r.base);
			return (ISC_R_FAILURE);
		}
	} else {
		zclass = defclass;
	}

	/*
	 * Look for an already existing zone.
	 * We need to make this canonical as isc_symtab_define()
	 * deals with strings.
	 */
	dns_fixedname_init(&fixedname);
	isc_buffer_constinit(&b, znamestr, strlen(znamestr));
	isc_buffer_add(&b, strlen(znamestr));
	tresult = dns_name_fromtext(dns_fixedname_name(&fixedname), &b,
				    dns_rootname, DNS_NAME_DOWNCASE, NULL);
	if (tresult != ISC_R_SUCCESS) {
		cfg_obj_log(zconfig, logctx, ISC_LOG_ERROR,
			    "zone '%s': is not a valid name", znamestr);
		result = ISC_R_FAILURE;
	} else {
		char namebuf[DNS_NAME_FORMATSIZE + 128];
		char *tmp = namebuf;
		size_t len = sizeof(namebuf);

		zname = dns_fixedname_name(&fixedname);
		dns_name_format(zname, namebuf, sizeof(namebuf));
		tresult = nameexist(zconfig, namebuf, ztype == HINTZONE ? 1 :
				    ztype == REDIRECTZONE ? 2 : 3,
				    symtab, "zone '%s': already exists "
				    "previous definition: %s:%u", logctx, mctx);
		if (tresult != ISC_R_SUCCESS)
			result = tresult;
		if (dns_name_equal(zname, dns_rootname))
			root = ISC_TRUE;
		else if (dns_name_isrfc1918(zname))
			rfc1918 = ISC_TRUE;
		else if (dns_name_isula(zname))
			ula = ISC_TRUE;
		tmp += strlen(tmp);
		len -= strlen(tmp);
		(void)snprintf(tmp, len, "%u/%s", zclass,
			       (ztype == INVIEWZONE) ? target :
				(viewname != NULL) ? viewname : "_default");
		switch (ztype) {
		case INVIEWZONE:
			tresult = isc_symtab_lookup(inview, namebuf, 0, NULL);
			if (tresult != ISC_R_SUCCESS) {
				cfg_obj_log(inviewobj, logctx, ISC_LOG_ERROR,
					    "'in-view' zone '%s' "
					    "does not exist in view '%s', "
					    "or view '%s' is not yet defined",
					    znamestr, target, target);
				if (result == ISC_R_SUCCESS) {
					result = tresult;
				}
			}
			break;

		case FORWARDZONE:
		case REDIRECTZONE:
		case DELEGATIONZONE:
			break;

		case MASTERZONE:
		case SLAVEZONE:
		case HINTZONE:
		case STUBZONE:
		case STATICSTUBZONE:
			tmp = isc_mem_strdup(mctx, namebuf);
			if (tmp != NULL) {
				isc_symvalue_t symvalue;

				symvalue.as_cpointer = NULL;
				tresult = isc_symtab_define(inview, tmp, 1,
					   symvalue, isc_symexists_replace);
				if (tresult == ISC_R_NOMEMORY) {
					isc_mem_free(mctx, tmp);
				}
				if (result == ISC_R_SUCCESS &&
				    tresult != ISC_R_SUCCESS)
					result = tresult;
			} else if (result != ISC_R_SUCCESS) {
				result = ISC_R_NOMEMORY;
			}
			break;

		default:
			INSIST(0);
		}
	}

	if (ztype == INVIEWZONE) {
		const cfg_obj_t *fwd = NULL;
		unsigned int maxopts = 1;

		(void)cfg_map_get(zoptions, "forward", &fwd);
		if (fwd != NULL)
			maxopts++;
		fwd = NULL;
		(void)cfg_map_get(zoptions, "forwarders", &fwd);
		if (fwd != NULL)
			maxopts++;
		if (cfg_map_count(zoptions) > maxopts) {
			cfg_obj_log(zconfig, logctx, ISC_LOG_ERROR,
				    "zone '%s': 'in-view' used "
				    "with incompatible zone options",
				    znamestr);
			if (result == ISC_R_SUCCESS)
				result = ISC_R_FAILURE;
		}
		return (result);
	}

	/*
	 * Check if value is zero.
	 */
	if (check_nonzero(zoptions, logctx) != ISC_R_SUCCESS)
		result = ISC_R_FAILURE;

	/*
	 * Look for inappropriate options for the given zone type.
	 * Check that ACLs expand correctly.
	 */
	for (i = 0; i < sizeof(options) / sizeof(options[0]); i++) {
		obj = NULL;
		if ((options[i].allowed & ztype) == 0 &&
		    cfg_map_get(zoptions, options[i].name, &obj) ==
		    ISC_R_SUCCESS)
		{
			if (strcmp(options[i].name, "allow-update") != 0 ||
			    ztype != SLAVEZONE) {
				cfg_obj_log(obj, logctx, ISC_LOG_ERROR,
					    "option '%s' is not allowed "
					    "in '%s' zone '%s'",
					    options[i].name, typestr,
					    znamestr);
					result = ISC_R_FAILURE;
			} else
				cfg_obj_log(obj, logctx, ISC_LOG_WARNING,
					    "option '%s' is not allowed "
					    "in '%s' zone '%s'",
					    options[i].name, typestr,
					    znamestr);
		}
		obj = NULL;
		if ((options[i].allowed & ztype) != 0 &&
		    (options[i].allowed & CHECKACL) != 0) {

			tresult = checkacl(options[i].name, actx, zconfig,
					   voptions, config, logctx, mctx);
			if (tresult != ISC_R_SUCCESS)
				result = tresult;
		}

	}

	/*
	 * Master & slave zones may have an "also-notify" field, but
	 * shouldn't if notify is disabled.
	 */
	if (ztype == MASTERZONE || ztype == SLAVEZONE ) {
		isc_boolean_t donotify = ISC_TRUE;

		obj = NULL;
		tresult = cfg_map_get(zoptions, "notify", &obj);
		if (tresult != ISC_R_SUCCESS && voptions != NULL)
			tresult = cfg_map_get(voptions, "notify", &obj);
		if (tresult != ISC_R_SUCCESS && goptions != NULL)
			tresult = cfg_map_get(goptions, "notify", &obj);
		if (tresult == ISC_R_SUCCESS) {
			if (cfg_obj_isboolean(obj))
				donotify = cfg_obj_asboolean(obj);
			else {
				const char *notifystr = cfg_obj_asstring(obj);
				if (ztype != MASTERZONE &&
				    strcasecmp(notifystr, "master-only") == 0)
					donotify = ISC_FALSE;
			}
		}

		obj = NULL;
		tresult = cfg_map_get(zoptions, "also-notify", &obj);
		if (tresult == ISC_R_SUCCESS && !donotify) {
			cfg_obj_log(zoptions, logctx, ISC_LOG_WARNING,
				    "zone '%s': 'also-notify' set but "
				    "'notify' is disabled", znamestr);
		}
		if (tresult != ISC_R_SUCCESS && voptions != NULL)
			tresult = cfg_map_get(voptions, "also-notify", &obj);
		if (tresult != ISC_R_SUCCESS && goptions != NULL)
			tresult = cfg_map_get(goptions, "also-notify", &obj);
		if (tresult == ISC_R_SUCCESS && donotify) {
			isc_uint32_t count;
			tresult = validate_masters(obj, config, &count,
						   logctx, mctx);
			if (tresult != ISC_R_SUCCESS && result == ISC_R_SUCCESS)
				result = tresult;
		}
	}

	/*
	 * Slave & stub zones must have a "masters" field.
	 */
	if (ztype == SLAVEZONE || ztype == STUBZONE) {
		obj = NULL;
		if (cfg_map_get(zoptions, "masters", &obj) != ISC_R_SUCCESS) {
			cfg_obj_log(zoptions, logctx, ISC_LOG_ERROR,
				    "zone '%s': missing 'masters' entry",
				    znamestr);
			result = ISC_R_FAILURE;
		} else {
			isc_uint32_t count;
			tresult = validate_masters(obj, config, &count,
						   logctx, mctx);
			if (tresult != ISC_R_SUCCESS && result == ISC_R_SUCCESS)
				result = tresult;
			if (tresult == ISC_R_SUCCESS && count == 0) {
				cfg_obj_log(zoptions, logctx, ISC_LOG_ERROR,
					    "zone '%s': empty 'masters' entry",
					    znamestr);
				result = ISC_R_FAILURE;
			}
		}
	}

	/*
	 * Master zones can't have both "allow-update" and "update-policy".
	 */
	if (ztype == MASTERZONE || ztype == SLAVEZONE) {
		isc_boolean_t signing = ISC_FALSE;
		isc_result_t res1, res2, res3;
		const cfg_obj_t *au = NULL;
		const char *arg;

		obj = NULL;
		res1 = cfg_map_get(zoptions, "allow-update", &au);
		obj = NULL;
		res2 = cfg_map_get(zoptions, "update-policy", &obj);
		if (res1 == ISC_R_SUCCESS && res2 == ISC_R_SUCCESS) {
			cfg_obj_log(obj, logctx, ISC_LOG_ERROR,
				    "zone '%s': 'allow-update' is ignored "
				    "when 'update-policy' is present",
				    znamestr);
			result = ISC_R_FAILURE;
		} else if (res2 == ISC_R_SUCCESS) {
			res3 = check_update_policy(obj, logctx);
			if (res3 != ISC_R_SUCCESS)
				result = ISC_R_FAILURE;
		}

		/*
		 * To determine whether auto-dnssec is allowed,
		 * we should also check for allow-update at the
		 * view and options levels.
		 */
		if (res1 != ISC_R_SUCCESS && voptions != NULL)
			res1 = cfg_map_get(voptions, "allow-update", &au);
		if (res1 != ISC_R_SUCCESS && goptions != NULL)
			res1 = cfg_map_get(goptions, "allow-update", &au);

		if (res2 == ISC_R_SUCCESS)
			ddns = ISC_TRUE;
		else if (res1 == ISC_R_SUCCESS) {
			dns_acl_t *acl = NULL;
			res1 = cfg_acl_fromconfig(au, config, logctx,
						  actx, mctx, 0, &acl);
			if (res1 != ISC_R_SUCCESS) {
				cfg_obj_log(au, logctx, ISC_LOG_ERROR,
					    "acl expansion failed: %s",
					    isc_result_totext(result));
				result = ISC_R_FAILURE;
			} else if (acl != NULL) {
				if (!dns_acl_isnone(acl))
					ddns = ISC_TRUE;
				dns_acl_detach(&acl);
			}
		}

		obj = NULL;
		res1 = cfg_map_get(zoptions, "inline-signing", &obj);
		if (res1 == ISC_R_SUCCESS)
			signing = cfg_obj_asboolean(obj);

		obj = NULL;
		arg = "off";
		res3 = cfg_map_get(zoptions, "auto-dnssec", &obj);
		if (res3 == ISC_R_SUCCESS)
			arg = cfg_obj_asstring(obj);
		if (strcasecmp(arg, "off") != 0 && !ddns && !signing) {
			cfg_obj_log(obj, logctx, ISC_LOG_ERROR,
				    "'auto-dnssec %s;' requires%s "
				    "inline-signing to be configured for "
				    "the zone", arg,
				    (ztype == MASTERZONE) ?
					 " dynamic DNS or" : "");
			result = ISC_R_FAILURE;
		}

		obj = NULL;
		res1 = cfg_map_get(zoptions, "sig-signing-type", &obj);
		if (res1 == ISC_R_SUCCESS) {
			isc_uint32_t type = cfg_obj_asuint32(obj);
			if (type < 0xff00U || type > 0xffffU)
				cfg_obj_log(obj, logctx, ISC_LOG_ERROR,
					    "sig-signing-type: %u out of "
					    "range [%u..%u]", type,
					    0xff00U, 0xffffU);
			result = ISC_R_FAILURE;
		}

		obj = NULL;
		res1 = cfg_map_get(zoptions, "dnssec-dnskey-kskonly", &obj);
		if (res1 == ISC_R_SUCCESS && ztype == SLAVEZONE && !signing) {
			cfg_obj_log(obj, logctx, ISC_LOG_ERROR,
				    "dnssec-dnskey-kskonly: requires "
				    "inline-signing when used in slave zone");
			result = ISC_R_FAILURE;
		}

		obj = NULL;
		res1 = cfg_map_get(zoptions, "dnssec-loadkeys-interval", &obj);
		if (res1 == ISC_R_SUCCESS && ztype == SLAVEZONE && !signing) {
			cfg_obj_log(obj, logctx, ISC_LOG_ERROR,
				    "dnssec-loadkeys-interval: requires "
				    "inline-signing when used in slave zone");
			result = ISC_R_FAILURE;
		}

		obj = NULL;
		res1 = cfg_map_get(zoptions, "update-check-ksk", &obj);
		if (res1 == ISC_R_SUCCESS && ztype == SLAVEZONE && !signing) {
			cfg_obj_log(obj, logctx, ISC_LOG_ERROR,
				    "update-check-ksk: requires "
				    "inline-signing when used in slave zone");
			result = ISC_R_FAILURE;
		}
	}

	/*
	 * Check the excessively complicated "dialup" option.
	 */
	if (ztype == MASTERZONE || ztype == SLAVEZONE || ztype == STUBZONE) {
		const cfg_obj_t *dialup = NULL;
		(void)cfg_map_get(zoptions, "dialup", &dialup);
		if (dialup != NULL && cfg_obj_isstring(dialup)) {
			const char *str = cfg_obj_asstring(dialup);
			for (i = 0;
			     i < sizeof(dialups) / sizeof(dialups[0]);
			     i++)
			{
				if (strcasecmp(dialups[i].name, str) != 0)
					continue;
				if ((dialups[i].allowed & ztype) == 0) {
					cfg_obj_log(obj, logctx,
						    ISC_LOG_ERROR,
						    "dialup type '%s' is not "
						    "allowed in '%s' "
						    "zone '%s'",
						    str, typestr, znamestr);
					result = ISC_R_FAILURE;
				}
				break;
			}
			if (i == sizeof(dialups) / sizeof(dialups[0])) {
				cfg_obj_log(obj, logctx, ISC_LOG_ERROR,
					    "invalid dialup type '%s' in zone "
					    "'%s'", str, znamestr);
				result = ISC_R_FAILURE;
			}
		}
	}

	/*
	 * Check that forwarding is reasonable.
	 */
	obj = NULL;
	if (root) {
		if (voptions != NULL)
			(void)cfg_map_get(voptions, "forwarders", &obj);
		if (obj == NULL && goptions != NULL)
			(void)cfg_map_get(goptions, "forwarders", &obj);
	}
	if (check_forward(zoptions, obj, logctx) != ISC_R_SUCCESS)
		result = ISC_R_FAILURE;

	/*
	 * Check that a RFC 1918 / ULA reverse zone is not forward first
	 * unless explictly configured to be so.
	 */
	if (ztype == FORWARDZONE && (rfc1918 || ula)) {
		obj = NULL;
		(void)cfg_map_get(zoptions, "forward", &obj);
		if (obj == NULL) {
			/*
			 * Forward mode not explicity configured.
			 */
			if (voptions != NULL)
				cfg_map_get(voptions, "forward", &obj);
			if (obj == NULL && goptions != NULL)
				cfg_map_get(goptions, "forward", &obj);
			if (obj == NULL ||
			    strcasecmp(cfg_obj_asstring(obj), "first") == 0)
				cfg_obj_log(zconfig, logctx, ISC_LOG_WARNING,
					    "inherited 'forward first;' for "
					    "%s zone '%s' - did you want "
					    "'forward only;'?",
					    rfc1918 ? "rfc1918" : "ula",
					    znamestr);
		}
	}

	/*
	 * Check validity of static stub server addresses.
	 */
	obj = NULL;
	(void)cfg_map_get(zoptions, "server-addresses", &obj);
	if (ztype == STATICSTUBZONE && obj != NULL) {
		for (element = cfg_list_first(obj);
		     element != NULL;
		     element = cfg_list_next(element))
		{
			isc_sockaddr_t sa;
			isc_netaddr_t na;
			obj = cfg_listelt_value(element);
			sa = *cfg_obj_assockaddr(obj);

			if (isc_sockaddr_getport(&sa) != 0) {
				result = ISC_R_FAILURE;
				cfg_obj_log(obj, logctx, ISC_LOG_ERROR,
					    "port is not configurable for "
					    "static stub server-addresses");
			}

			isc_netaddr_fromsockaddr(&na, &sa);
			if (isc_netaddr_getzone(&na) != 0) {
				result = ISC_R_FAILURE;
				cfg_obj_log(obj, logctx, ISC_LOG_ERROR,
					    "scoped address is not allowed "
					    "for static stub "
					    "server-addresses");
			}
		}
	}

	/*
	 * Check validity of static stub server names.
	 */
	obj = NULL;
	(void)cfg_map_get(zoptions, "server-names", &obj);
	if (zname != NULL && ztype == STATICSTUBZONE && obj != NULL) {
		for (element = cfg_list_first(obj);
		     element != NULL;
		     element = cfg_list_next(element))
		{
			const char *snamestr;
			dns_fixedname_t fixed_sname;
			isc_buffer_t b2;
			dns_name_t *sname;

			obj = cfg_listelt_value(element);
			snamestr = cfg_obj_asstring(obj);

			dns_fixedname_init(&fixed_sname);
			isc_buffer_constinit(&b2, snamestr, strlen(snamestr));
			isc_buffer_add(&b2, strlen(snamestr));
			sname = dns_fixedname_name(&fixed_sname);
			tresult = dns_name_fromtext(sname, &b2, dns_rootname,
						    0, NULL);
			if (tresult != ISC_R_SUCCESS) {
				cfg_obj_log(zconfig, logctx, ISC_LOG_ERROR,
					    "server-name '%s' is not a valid "
					    "name", snamestr);
				result = ISC_R_FAILURE;
			} else if (dns_name_issubdomain(sname, zname)) {
				cfg_obj_log(zconfig, logctx, ISC_LOG_ERROR,
					    "server-name '%s' must not be a "
					    "subdomain of zone name '%s'",
					    snamestr, znamestr);
				result = ISC_R_FAILURE;
			}
		}
	}


	/*
	 * Check that max-zone-ttl isn't used with masterfile-format map
	 */
	masterformat = dns_masterformat_text;
	obj = NULL;
	(void)cfg_map_get(zoptions, "masterfile-format", &obj);
	if (obj != NULL) {
		const char *masterformatstr = cfg_obj_asstring(obj);
		if (strcasecmp(masterformatstr, "text") == 0)
			masterformat = dns_masterformat_text;
		else if (strcasecmp(masterformatstr, "raw") == 0)
			masterformat = dns_masterformat_raw;
		else if (strcasecmp(masterformatstr, "map") == 0)
			masterformat = dns_masterformat_map;
		else
			INSIST(0);
	}

	if (masterformat == dns_masterformat_map) {
		obj = NULL;
		(void)cfg_map_get(zoptions, "max-zone-ttl", &obj);
		if (obj == NULL && voptions != NULL)
			(void)cfg_map_get(voptions, "max-zone-ttl", &obj);
		if (obj == NULL && goptions !=NULL)
			(void)cfg_map_get(goptions, "max-zone-ttl", &obj);
		if (obj != NULL) {
			cfg_obj_log(zconfig, logctx, ISC_LOG_ERROR,
				    "zone '%s': 'max-zone-ttl' is not "
				    "compatible with 'masterfile-format map'",
				    znamestr);
			result = ISC_R_FAILURE;
		}
	}

	/*
	 * Warn if key-directory doesn't exist
	 */
	obj = NULL;
	tresult = cfg_map_get(zoptions, "key-directory", &obj);
	if (tresult == ISC_R_SUCCESS) {
		const char *dir = cfg_obj_asstring(obj);
		tresult = isc_file_isdirectory(dir);
		switch (tresult) {
		case ISC_R_SUCCESS:
			break;
		case ISC_R_FILENOTFOUND:
			cfg_obj_log(obj, logctx, ISC_LOG_WARNING,
				    "key-directory: '%s' does not exist",
				    dir);
			break;
		case ISC_R_INVALIDFILE:
			cfg_obj_log(obj, logctx, ISC_LOG_WARNING,
				    "key-directory: '%s' is not a directory",
				    dir);
			break;
		default:
			cfg_obj_log(obj, logctx, ISC_LOG_WARNING,
				    "key-directory: '%s' %s",
				    dir, isc_result_totext(tresult));
			result = tresult;
		}
	}

	/*
	 * Check various options.
	 */
	tresult = check_options(zoptions, logctx, mctx, optlevel_zone);
	if (tresult != ISC_R_SUCCESS)
		result = tresult;

	/*
	 * If the zone type is rbt/rbt64 then master/hint zones
	 * require file clauses.
	 * If inline signing is used, then slave zones require a
	 * file clause as well
	 */
	obj = NULL;
	dlz = ISC_FALSE;
	tresult = cfg_map_get(zoptions, "dlz", &obj);
	if (tresult == ISC_R_SUCCESS)
		dlz = ISC_TRUE;

	obj = NULL;
	tresult = cfg_map_get(zoptions, "database", &obj);
	if (dlz && tresult == ISC_R_SUCCESS) {
		cfg_obj_log(zconfig, logctx, ISC_LOG_ERROR,
				    "zone '%s': cannot specify both 'dlz' "
				    "and 'database'", znamestr);
		result = ISC_R_FAILURE;
	} else if (!dlz &&
	    (tresult == ISC_R_NOTFOUND ||
	    (tresult == ISC_R_SUCCESS &&
	     (strcmp("rbt", cfg_obj_asstring(obj)) == 0 ||
	      strcmp("rbt64", cfg_obj_asstring(obj)) == 0))))
	{
		isc_result_t res1;
		const cfg_obj_t *fileobj = NULL;
		tresult = cfg_map_get(zoptions, "file", &fileobj);
		obj = NULL;
		res1 = cfg_map_get(zoptions, "inline-signing", &obj);
		if ((tresult != ISC_R_SUCCESS &&
		    (ztype == MASTERZONE || ztype == HINTZONE ||
		     (ztype == SLAVEZONE && res1 == ISC_R_SUCCESS &&
		      cfg_obj_asboolean(obj))))) {
			cfg_obj_log(zconfig, logctx, ISC_LOG_ERROR,
			    "zone '%s': missing 'file' entry",
			    znamestr);
			result = tresult;
		} else if (tresult == ISC_R_SUCCESS &&
			   (ztype == SLAVEZONE || ddns)) {
			tresult = fileexist(fileobj, files, ISC_TRUE, logctx);
			if (tresult != ISC_R_SUCCESS)
				result = tresult;
		} else if (tresult == ISC_R_SUCCESS &&
			   (ztype == MASTERZONE || ztype == HINTZONE)) {
			tresult = fileexist(fileobj, files, ISC_FALSE, logctx);
			if (tresult != ISC_R_SUCCESS)
				result = tresult;
		}
	}

	return (result);
}


typedef struct keyalgorithms {
	const char *name;
	isc_uint16_t size;
} algorithmtable;

isc_result_t
bind9_check_key(const cfg_obj_t *key, isc_log_t *logctx) {
	const cfg_obj_t *algobj = NULL;
	const cfg_obj_t *secretobj = NULL;
	const char *keyname = cfg_obj_asstring(cfg_map_getname(key));
	const char *algorithm;
	int i;
	size_t len = 0;
	isc_result_t result;
	isc_buffer_t buf;
	unsigned char secretbuf[1024];
	static const algorithmtable algorithms[] = {
#ifndef PK11_MD5_DISABLE
		{ "hmac-md5", 128 },
		{ "hmac-md5.sig-alg.reg.int", 0 },
		{ "hmac-md5.sig-alg.reg.int.", 0 },
#endif
		{ "hmac-sha1", 160 },
		{ "hmac-sha224", 224 },
		{ "hmac-sha256", 256 },
		{ "hmac-sha384", 384 },
		{ "hmac-sha512", 512 },
		{  NULL, 0 }
	};

	(void)cfg_map_get(key, "algorithm", &algobj);
	(void)cfg_map_get(key, "secret", &secretobj);
	if (secretobj == NULL || algobj == NULL) {
		cfg_obj_log(key, logctx, ISC_LOG_ERROR,
			    "key '%s' must have both 'secret' and "
			    "'algorithm' defined",
			    keyname);
		return (ISC_R_FAILURE);
	}

	isc_buffer_init(&buf, secretbuf, sizeof(secretbuf));
	result = isc_base64_decodestring(cfg_obj_asstring(secretobj), &buf);
	if (result != ISC_R_SUCCESS) {
		cfg_obj_log(secretobj, logctx, ISC_LOG_ERROR,
			    "bad secret '%s'", isc_result_totext(result));
		return (result);
	}

	algorithm = cfg_obj_asstring(algobj);
	for (i = 0; algorithms[i].name != NULL; i++) {
		len = strlen(algorithms[i].name);
		if (strncasecmp(algorithms[i].name, algorithm, len) == 0 &&
		    (algorithm[len] == '\0' ||
		     (algorithms[i].size != 0 && algorithm[len] == '-')))
			break;
	}
	if (algorithms[i].name == NULL) {
		cfg_obj_log(algobj, logctx, ISC_LOG_ERROR,
			    "unknown algorithm '%s'", algorithm);
		return (ISC_R_NOTFOUND);
	}
	if (algorithm[len] == '-') {
		isc_uint16_t digestbits;
		result = isc_parse_uint16(&digestbits, algorithm + len + 1, 10);
		if (result == ISC_R_SUCCESS || result == ISC_R_RANGE) {
			if (result == ISC_R_RANGE ||
			    digestbits > algorithms[i].size) {
				cfg_obj_log(algobj, logctx, ISC_LOG_ERROR,
					    "key '%s' digest-bits too large "
					    "[%u..%u]", keyname,
					    algorithms[i].size / 2,
					    algorithms[i].size);
				return (ISC_R_RANGE);
			}
			if ((digestbits % 8) != 0) {
				cfg_obj_log(algobj, logctx, ISC_LOG_ERROR,
					    "key '%s' digest-bits not multiple"
					    " of 8", keyname);
				return (ISC_R_RANGE);
			}
			/*
			 * Recommended minima for hmac algorithms.
			 */
			if ((digestbits < (algorithms[i].size / 2U) ||
			     (digestbits < 80U)))
				cfg_obj_log(algobj, logctx, ISC_LOG_WARNING,
					    "key '%s' digest-bits too small "
					    "[<%u]", keyname,
					    algorithms[i].size/2);
		} else {
			cfg_obj_log(algobj, logctx, ISC_LOG_ERROR,
				    "key '%s': unable to parse digest-bits",
				    keyname);
			return (result);
		}
	}
	return (ISC_R_SUCCESS);
}

static isc_result_t
fileexist(const cfg_obj_t *obj, isc_symtab_t *symtab, isc_boolean_t writeable,
	  isc_log_t *logctx)
{
	isc_result_t result;
	isc_symvalue_t symvalue;
	unsigned int line;
	const char *file;

	result = isc_symtab_lookup(symtab, cfg_obj_asstring(obj), 0, &symvalue);
	if (result == ISC_R_SUCCESS) {
		if (writeable) {
			file = cfg_obj_file(symvalue.as_cpointer);
			line = cfg_obj_line(symvalue.as_cpointer);
			cfg_obj_log(obj, logctx, ISC_LOG_ERROR,
				    "writeable file '%s': already in use: "
				    "%s:%u", cfg_obj_asstring(obj),
				    file, line);
			return (ISC_R_EXISTS);
		}
		result = isc_symtab_lookup(symtab, cfg_obj_asstring(obj), 2,
					   &symvalue);
		if (result == ISC_R_SUCCESS) {
			file = cfg_obj_file(symvalue.as_cpointer);
			line = cfg_obj_line(symvalue.as_cpointer);
			cfg_obj_log(obj, logctx, ISC_LOG_ERROR,
				    "writeable file '%s': already in use: "
				    "%s:%u", cfg_obj_asstring(obj),
				    file, line);
			return (ISC_R_EXISTS);
		}
		return (ISC_R_SUCCESS);
	}

	symvalue.as_cpointer = obj;
	result = isc_symtab_define(symtab, cfg_obj_asstring(obj),
				   writeable ? 2 : 1, symvalue,
				   isc_symexists_reject);
	return (result);
}

/*
 * Check key list for duplicates key names and that the key names
 * are valid domain names as these keys are used for TSIG.
 *
 * Check the key contents for validity.
 */
static isc_result_t
check_keylist(const cfg_obj_t *keys, isc_symtab_t *symtab,
	      isc_mem_t *mctx, isc_log_t *logctx)
{
	char namebuf[DNS_NAME_FORMATSIZE];
	dns_fixedname_t fname;
	dns_name_t *name;
	isc_result_t result = ISC_R_SUCCESS;
	isc_result_t tresult;
	const cfg_listelt_t *element;

	dns_fixedname_init(&fname);
	name = dns_fixedname_name(&fname);
	for (element = cfg_list_first(keys);
	     element != NULL;
	     element = cfg_list_next(element))
	{
		const cfg_obj_t *key = cfg_listelt_value(element);
		const char *keyid = cfg_obj_asstring(cfg_map_getname(key));
		isc_symvalue_t symvalue;
		isc_buffer_t b;
		char *keyname;

		isc_buffer_constinit(&b, keyid, strlen(keyid));
		isc_buffer_add(&b, strlen(keyid));
		tresult = dns_name_fromtext(name, &b, dns_rootname,
					    0, NULL);
		if (tresult != ISC_R_SUCCESS) {
			cfg_obj_log(key, logctx, ISC_LOG_ERROR,
				    "key '%s': bad key name", keyid);
			result = tresult;
			continue;
		}
		tresult = bind9_check_key(key, logctx);
		if (tresult != ISC_R_SUCCESS)
			return (tresult);

		dns_name_format(name, namebuf, sizeof(namebuf));
		keyname = isc_mem_strdup(mctx, namebuf);
		if (keyname == NULL)
			return (ISC_R_NOMEMORY);
		symvalue.as_cpointer = key;
		tresult = isc_symtab_define(symtab, keyname, 1, symvalue,
					    isc_symexists_reject);
		if (tresult == ISC_R_EXISTS) {
			const char *file;
			unsigned int line;

			RUNTIME_CHECK(isc_symtab_lookup(symtab, keyname,
					    1, &symvalue) == ISC_R_SUCCESS);
			file = cfg_obj_file(symvalue.as_cpointer);
			line = cfg_obj_line(symvalue.as_cpointer);

			if (file == NULL)
				file = "<unknown file>";
			cfg_obj_log(key, logctx, ISC_LOG_ERROR,
				    "key '%s': already exists "
				    "previous definition: %s:%u",
				    keyid, file, line);
			isc_mem_free(mctx, keyname);
			result = tresult;
		} else if (tresult != ISC_R_SUCCESS) {
			isc_mem_free(mctx, keyname);
			return (tresult);
		}
	}
	return (result);
}

static struct {
	const char *v4;
	const char *v6;
} sources[] = {
	{ "transfer-source", "transfer-source-v6" },
	{ "notify-source", "notify-source-v6" },
	{ "query-source", "query-source-v6" },
	{ NULL, NULL }
};

/*
 * RNDC keys are not normalised unlike TSIG keys.
 *
 * 	"foo." is different to "foo".
 */
static isc_boolean_t
rndckey_exists(const cfg_obj_t *keylist, const char *keyname) {
	const cfg_listelt_t *element;
	const cfg_obj_t *obj;
	const char *str;

	if (keylist == NULL)
		return (ISC_FALSE);

	for (element = cfg_list_first(keylist);
	     element != NULL;
	     element = cfg_list_next(element))
	{
		obj = cfg_listelt_value(element);
		str = cfg_obj_asstring(cfg_map_getname(obj));
		if (!strcasecmp(str, keyname))
			return (ISC_TRUE);
	}
	return (ISC_FALSE);
}

static isc_result_t
check_servers(const cfg_obj_t *config, const cfg_obj_t *voptions,
	      isc_symtab_t *symtab, isc_log_t *logctx)
{
	dns_fixedname_t fname;
	isc_result_t result = ISC_R_SUCCESS;
	isc_result_t tresult;
	const cfg_listelt_t *e1, *e2;
	const cfg_obj_t *v1, *v2, *keys;
	const cfg_obj_t *servers;
	isc_netaddr_t n1, n2;
	unsigned int p1, p2;
	const cfg_obj_t *obj;
	char buf[ISC_NETADDR_FORMATSIZE];
	char namebuf[DNS_NAME_FORMATSIZE];
	const char *xfr;
	const char *keyval;
	isc_buffer_t b;
	int source;
	dns_name_t *keyname;

	servers = NULL;
	if (voptions != NULL)
		(void)cfg_map_get(voptions, "server", &servers);
	if (servers == NULL)
		(void)cfg_map_get(config, "server", &servers);
	if (servers == NULL)
		return (ISC_R_SUCCESS);

	for (e1 = cfg_list_first(servers); e1 != NULL; e1 = cfg_list_next(e1)) {
		v1 = cfg_listelt_value(e1);
		cfg_obj_asnetprefix(cfg_map_getname(v1), &n1, &p1);
		/*
		 * Check that unused bits are zero.
		 */
		tresult = isc_netaddr_prefixok(&n1, p1);
		if (tresult != ISC_R_SUCCESS) {
			INSIST(tresult == ISC_R_FAILURE);
			isc_netaddr_format(&n1, buf, sizeof(buf));
			cfg_obj_log(v1, logctx, ISC_LOG_ERROR,
				    "server '%s/%u': invalid prefix "
				    "(extra bits specified)", buf, p1);
			result = tresult;
		}
		source = 0;
		do {
			obj = NULL;
			if (n1.family == AF_INET)
				xfr = sources[source].v6;
			else
				xfr = sources[source].v4;
			(void)cfg_map_get(v1, xfr, &obj);
			if (obj != NULL) {
				isc_netaddr_format(&n1, buf, sizeof(buf));
				cfg_obj_log(v1, logctx, ISC_LOG_ERROR,
					    "server '%s/%u': %s not legal",
					    buf, p1, xfr);
				result = ISC_R_FAILURE;
			}
		} while (sources[++source].v4 != NULL);
		e2 = e1;
		while ((e2 = cfg_list_next(e2)) != NULL) {
			v2 = cfg_listelt_value(e2);
			cfg_obj_asnetprefix(cfg_map_getname(v2), &n2, &p2);
			if (p1 == p2 && isc_netaddr_equal(&n1, &n2)) {
				const char *file = cfg_obj_file(v1);
				unsigned int line = cfg_obj_line(v1);

				if (file == NULL)
					file = "<unknown file>";

				isc_netaddr_format(&n2, buf, sizeof(buf));
				cfg_obj_log(v2, logctx, ISC_LOG_ERROR,
					    "server '%s/%u': already exists "
					    "previous definition: %s:%u",
					    buf, p2, file, line);
				result = ISC_R_FAILURE;
			}
		}
		keys = NULL;
		cfg_map_get(v1, "keys", &keys);
		if (keys != NULL) {
			/*
			 * Normalize key name.
			 */
			keyval = cfg_obj_asstring(keys);
			dns_fixedname_init(&fname);
			isc_buffer_constinit(&b, keyval, strlen(keyval));
			isc_buffer_add(&b, strlen(keyval));
			keyname = dns_fixedname_name(&fname);
			tresult = dns_name_fromtext(keyname, &b, dns_rootname,
						    0, NULL);
			if (tresult != ISC_R_SUCCESS) {
				cfg_obj_log(keys, logctx, ISC_LOG_ERROR,
					    "bad key name '%s'", keyval);
				result = ISC_R_FAILURE;
				continue;
			}
			dns_name_format(keyname, namebuf, sizeof(namebuf));
			tresult = isc_symtab_lookup(symtab, namebuf, 1, NULL);
			if (tresult != ISC_R_SUCCESS) {
				cfg_obj_log(keys, logctx, ISC_LOG_ERROR,
					    "unknown key '%s'", keyval);
				result = ISC_R_FAILURE;
			}
		}
	}
	return (result);
}

#define ROOT_KSK_2010	0x1
#define ROOT_KSK_2017	0x2
#define DLV_KSK_KEY	0x4

static isc_result_t
check_trusted_key(const cfg_obj_t *key, isc_boolean_t managed,
		  unsigned int *keyflags, isc_log_t *logctx)
{
	const char *keystr, *keynamestr;
	dns_fixedname_t fkeyname;
	dns_name_t *keyname;
	isc_buffer_t b;
	isc_region_t r;
	isc_result_t result = ISC_R_SUCCESS;
	isc_result_t tresult;
	isc_uint32_t flags, proto, alg;
	unsigned char keydata[4096];

	flags = cfg_obj_asuint32(cfg_tuple_get(key, "flags"));
	proto = cfg_obj_asuint32(cfg_tuple_get(key, "protocol"));
	alg = cfg_obj_asuint32(cfg_tuple_get(key, "algorithm"));

	dns_fixedname_init(&fkeyname);
	keyname = dns_fixedname_name(&fkeyname);
	keynamestr = cfg_obj_asstring(cfg_tuple_get(key, "name"));

	isc_buffer_constinit(&b, keynamestr, strlen(keynamestr));
	isc_buffer_add(&b, strlen(keynamestr));
	result = dns_name_fromtext(keyname, &b, dns_rootname, 0, NULL);
	if (result != ISC_R_SUCCESS) {
		cfg_obj_log(key, logctx, ISC_LOG_WARNING, "bad key name: %s\n",
			    isc_result_totext(result));
		result = ISC_R_FAILURE;
	}

	if (flags > 0xffff) {
		cfg_obj_log(key, logctx, ISC_LOG_WARNING,
			    "flags too big: %u\n", flags);
		result = ISC_R_FAILURE;
	}
	if (proto > 0xff) {
		cfg_obj_log(key, logctx, ISC_LOG_WARNING,
			    "protocol too big: %u\n", proto);
		result = ISC_R_FAILURE;
	}
	if (alg > 0xff) {
		cfg_obj_log(key, logctx, ISC_LOG_WARNING,
			    "algorithm too big: %u\n", alg);
		result = ISC_R_FAILURE;
	}

	if (managed) {
		const char *initmethod;
		initmethod = cfg_obj_asstring(cfg_tuple_get(key, "init"));

		if (strcasecmp(initmethod, "initial-key") != 0) {
			cfg_obj_log(key, logctx, ISC_LOG_ERROR,
				    "managed key '%s': "
				    "invalid initialization method '%s'",
				    keynamestr, initmethod);
			result = ISC_R_FAILURE;
		}
	}

	isc_buffer_init(&b, keydata, sizeof(keydata));

	keystr = cfg_obj_asstring(cfg_tuple_get(key, "key"));
	tresult = isc_base64_decodestring(keystr, &b);

	if (tresult != ISC_R_SUCCESS) {
		cfg_obj_log(key, logctx, ISC_LOG_ERROR,
			    "%s", isc_result_totext(tresult));
		result = ISC_R_FAILURE;
	} else {
		isc_buffer_usedregion(&b, &r);

		if ((alg == DST_ALG_RSASHA1 || alg == DST_ALG_RSAMD5) &&
		    r.length > 1 && r.base[0] == 1 && r.base[1] == 3)
			cfg_obj_log(key, logctx, ISC_LOG_WARNING,
				    "%s key '%s' has a weak exponent",
				    managed ? "managed" : "trusted",
				    keynamestr);
	}

	if (result == ISC_R_SUCCESS && dns_name_equal(keyname, dns_rootname)) {
		static const unsigned char root_ksk_2010[] = {
			0x03, 0x01, 0x00, 0x01, 0xa8, 0x00, 0x20, 0xa9,
			0x55, 0x66, 0xba, 0x42, 0xe8, 0x86, 0xbb, 0x80,
			0x4c, 0xda, 0x84, 0xe4, 0x7e, 0xf5, 0x6d, 0xbd,
			0x7a, 0xec, 0x61, 0x26, 0x15, 0x55, 0x2c, 0xec,
			0x90, 0x6d, 0x21, 0x16, 0xd0, 0xef, 0x20, 0x70,
			0x28, 0xc5, 0x15, 0x54, 0x14, 0x4d, 0xfe, 0xaf,
			0xe7, 0xc7, 0xcb, 0x8f, 0x00, 0x5d, 0xd1, 0x82,
			0x34, 0x13, 0x3a, 0xc0, 0x71, 0x0a, 0x81, 0x18,
			0x2c, 0xe1, 0xfd, 0x14, 0xad, 0x22, 0x83, 0xbc,
			0x83, 0x43, 0x5f, 0x9d, 0xf2, 0xf6, 0x31, 0x32,
			0x51, 0x93, 0x1a, 0x17, 0x6d, 0xf0, 0xda, 0x51,
			0xe5, 0x4f, 0x42, 0xe6, 0x04, 0x86, 0x0d, 0xfb,
			0x35, 0x95, 0x80, 0x25, 0x0f, 0x55, 0x9c, 0xc5,
			0x43, 0xc4, 0xff, 0xd5, 0x1c, 0xbe, 0x3d, 0xe8,
			0xcf, 0xd0, 0x67, 0x19, 0x23, 0x7f, 0x9f, 0xc4,
			0x7e, 0xe7, 0x29, 0xda, 0x06, 0x83, 0x5f, 0xa4,
			0x52, 0xe8, 0x25, 0xe9, 0xa1, 0x8e, 0xbc, 0x2e,
			0xcb, 0xcf, 0x56, 0x34, 0x74, 0x65, 0x2c, 0x33,
			0xcf, 0x56, 0xa9, 0x03, 0x3b, 0xcd, 0xf5, 0xd9,
			0x73, 0x12, 0x17, 0x97, 0xec, 0x80, 0x89, 0x04,
			0x1b, 0x6e, 0x03, 0xa1, 0xb7, 0x2d, 0x0a, 0x73,
			0x5b, 0x98, 0x4e, 0x03, 0x68, 0x73, 0x09, 0x33,
			0x23, 0x24, 0xf2, 0x7c, 0x2d, 0xba, 0x85, 0xe9,
			0xdb, 0x15, 0xe8, 0x3a, 0x01, 0x43, 0x38, 0x2e,
			0x97, 0x4b, 0x06, 0x21, 0xc1, 0x8e, 0x62, 0x5e,
			0xce, 0xc9, 0x07, 0x57, 0x7d, 0x9e, 0x7b, 0xad,
			0xe9, 0x52, 0x41, 0xa8, 0x1e, 0xbb, 0xe8, 0xa9,
			0x01, 0xd4, 0xd3, 0x27, 0x6e, 0x40, 0xb1, 0x14,
			0xc0, 0xa2, 0xe6, 0xfc, 0x38, 0xd1, 0x9c, 0x2e,
			0x6a, 0xab, 0x02, 0x64, 0x4b, 0x28, 0x13, 0xf5,
			0x75, 0xfc, 0x21, 0x60, 0x1e, 0x0d, 0xee, 0x49,
			0xcd, 0x9e, 0xe9, 0x6a, 0x43, 0x10, 0x3e, 0x52,
			0x4d, 0x62, 0x87, 0x3d };
		static const unsigned char root_ksk_2017[] = {
			0x03, 0x01, 0x00, 0x01, 0xac, 0xff, 0xb4, 0x09,
			0xbc, 0xc9, 0x39, 0xf8, 0x31, 0xf7, 0xa1, 0xe5,
			0xec, 0x88, 0xf7, 0xa5, 0x92, 0x55, 0xec, 0x53,
			0x04, 0x0b, 0xe4, 0x32, 0x02, 0x73, 0x90, 0xa4,
			0xce, 0x89, 0x6d, 0x6f, 0x90, 0x86, 0xf3, 0xc5,
			0xe1, 0x77, 0xfb, 0xfe, 0x11, 0x81, 0x63, 0xaa,
			0xec, 0x7a, 0xf1, 0x46, 0x2c, 0x47, 0x94, 0x59,
			0x44, 0xc4, 0xe2, 0xc0, 0x26, 0xbe, 0x5e, 0x98,
			0xbb, 0xcd, 0xed, 0x25, 0x97, 0x82, 0x72, 0xe1,
			0xe3, 0xe0, 0x79, 0xc5, 0x09, 0x4d, 0x57, 0x3f,
			0x0e, 0x83, 0xc9, 0x2f, 0x02, 0xb3, 0x2d, 0x35,
			0x13, 0xb1, 0x55, 0x0b, 0x82, 0x69, 0x29, 0xc8,
			0x0d, 0xd0, 0xf9, 0x2c, 0xac, 0x96, 0x6d, 0x17,
			0x76, 0x9f, 0xd5, 0x86, 0x7b, 0x64, 0x7c, 0x3f,
			0x38, 0x02, 0x9a, 0xbd, 0xc4, 0x81, 0x52, 0xeb,
			0x8f, 0x20, 0x71, 0x59, 0xec, 0xc5, 0xd2, 0x32,
			0xc7, 0xc1, 0x53, 0x7c, 0x79, 0xf4, 0xb7, 0xac,
			0x28, 0xff, 0x11, 0x68, 0x2f, 0x21, 0x68, 0x1b,
			0xf6, 0xd6, 0xab, 0xa5, 0x55, 0x03, 0x2b, 0xf6,
			0xf9, 0xf0, 0x36, 0xbe, 0xb2, 0xaa, 0xa5, 0xb3,
			0x77, 0x8d, 0x6e, 0xeb, 0xfb, 0xa6, 0xbf, 0x9e,
			0xa1, 0x91, 0xbe, 0x4a, 0xb0, 0xca, 0xea, 0x75,
			0x9e, 0x2f, 0x77, 0x3a, 0x1f, 0x90, 0x29, 0xc7,
			0x3e, 0xcb, 0x8d, 0x57, 0x35, 0xb9, 0x32, 0x1d,
			0xb0, 0x85, 0xf1, 0xb8, 0xe2, 0xd8, 0x03, 0x8f,
			0xe2, 0x94, 0x19, 0x92, 0x54, 0x8c, 0xee, 0x0d,
			0x67, 0xdd, 0x45, 0x47, 0xe1, 0x1d, 0xd6, 0x3a,
			0xf9, 0xc9, 0xfc, 0x1c, 0x54, 0x66, 0xfb, 0x68,
			0x4c, 0xf0, 0x09, 0xd7, 0x19, 0x7c, 0x2c, 0xf7,
			0x9e, 0x79, 0x2a, 0xb5, 0x01, 0xe6, 0xa8, 0xa1,
			0xca, 0x51, 0x9a, 0xf2, 0xcb, 0x9b, 0x5f, 0x63,
			0x67, 0xe9, 0x4c, 0x0d, 0x47, 0x50, 0x24, 0x51,
			0x35, 0x7b, 0xe1, 0xb5 };
		if (flags == 257 && proto == 3 && alg == 8 &&
		    isc_buffer_usedlength(&b) == sizeof(root_ksk_2010) &&
		    !memcmp(keydata, root_ksk_2010, sizeof(root_ksk_2010))) {
			*keyflags |= ROOT_KSK_2010;
		}
		if (flags == 257 && proto == 3 && alg == 8 &&
		    isc_buffer_usedlength(&b) == sizeof(root_ksk_2017) &&
		    !memcmp(keydata, root_ksk_2017, sizeof(root_ksk_2017))) {
			*keyflags |= ROOT_KSK_2017;
		}
	}
	if (result == ISC_R_SUCCESS && dns_name_equal(keyname, &dlviscorg)) {
		static const unsigned char dlviscorgkey[] = {
			0x04, 0x40, 0x00, 0x00, 0x03, 0xc7, 0x32, 0xef,
			0xf9, 0xa2, 0x7c, 0xeb, 0x10, 0x4e, 0xf3, 0xd5,
			0xe8, 0x26, 0x86, 0x0f, 0xd6, 0x3c, 0xed, 0x3e,
			0x8e, 0xea, 0x19, 0xad, 0x6d, 0xde, 0xb9, 0x61,
			0x27, 0xe0, 0xcc, 0x43, 0x08, 0x4d, 0x7e, 0x94,
			0xbc, 0xb6, 0x6e, 0xb8, 0x50, 0xbf, 0x9a, 0xcd,
			0xdf, 0x64, 0x4a, 0xb4, 0xcc, 0xd7, 0xe8, 0xc8,
			0xfb, 0xd2, 0x37, 0x73, 0x78, 0xd0, 0xf8, 0x5e,
			0x49, 0xd6, 0xe7, 0xc7, 0x67, 0x24, 0xd3, 0xc2,
			0xc6, 0x7f, 0x3e, 0x8c, 0x01, 0xa5, 0xd8, 0x56,
			0x4b, 0x2b, 0xcb, 0x7e, 0xd6, 0xea, 0xb8, 0x5b,
			0xe9, 0xe7, 0x03, 0x7a, 0x8e, 0xdb, 0xe0, 0xcb,
			0xfa, 0x4e, 0x81, 0x0f, 0x89, 0x9e, 0xc0, 0xc2,
			0xdb, 0x21, 0x81, 0x70, 0x7b, 0x43, 0xc6, 0xef,
			0x74, 0xde, 0xf5, 0xf6, 0x76, 0x90, 0x96, 0xf9,
			0xe9, 0xd8, 0x60, 0x31, 0xd7, 0xb9, 0xca, 0x65,
			0xf8, 0x04, 0x8f, 0xe8, 0x43, 0xe7, 0x00, 0x2b,
			0x9d, 0x3f, 0xc6, 0xf2, 0x6f, 0xd3, 0x41, 0x6b,
			0x7f, 0xc9, 0x30, 0xea, 0xe7, 0x0c, 0x4f, 0x01,
			0x65, 0x80, 0xf7, 0xbe, 0x8e, 0x71, 0xb1, 0x3c,
			0xf1, 0x26, 0x1c, 0x0b, 0x5e, 0xfd, 0x44, 0x64,
			0x63, 0xad, 0x99, 0x7e, 0x42, 0xe8, 0x04, 0x00,
			0x03, 0x2c, 0x74, 0x3d, 0x22, 0xb4, 0xb6, 0xb6,
			0xbc, 0x80, 0x7b, 0xb9, 0x9b, 0x05, 0x95, 0x5c,
			0x3b, 0x02, 0x1e, 0x53, 0xf4, 0x70, 0xfe, 0x64,
			0x71, 0xfe, 0xfc, 0x30, 0x30, 0x24, 0xe0, 0x35,
			0xba, 0x0c, 0x40, 0xab, 0x54, 0x76, 0xf3, 0x57,
			0x0e, 0xb6, 0x09, 0x0d, 0x21, 0xd9, 0xc2, 0xcd,
			0xf1, 0x89, 0x15, 0xc5, 0xd5, 0x17, 0xfe, 0x6a,
			0x5f, 0x54, 0x99, 0x97, 0xd2, 0x6a, 0xff, 0xf8,
			0x35, 0x62, 0xca, 0x8c, 0x7c, 0xe9, 0x4f, 0x9f,
			0x64, 0xfd, 0x54, 0xad, 0x4c, 0x33, 0x74, 0x61,
			0x4b, 0x96, 0xac, 0x13, 0x61 };
		if (flags == 257 && proto == 3 && alg == 5 &&
		    isc_buffer_usedlength(&b) == sizeof(dlviscorgkey) &&
		    !memcmp(keydata, dlviscorgkey, sizeof(dlviscorgkey))) {
			*keyflags |= DLV_KSK_KEY;
		}
	}

	return (result);
}

static isc_result_t
check_rpz(const char *rpz_catz, const cfg_obj_t *rpz_obj,
	  const char *viewname, isc_symtab_t *symtab, isc_log_t *logctx)
{
	const cfg_listelt_t *element;
	const cfg_obj_t *obj, *nameobj, *zoneobj;
	const char *zonename, *zonetype;
	const char *forview = " for view ";
	isc_symvalue_t value;
	isc_result_t result, tresult;
	dns_fixedname_t fixed;
	dns_name_t *name;
	char namebuf[DNS_NAME_FORMATSIZE];

	if (viewname == NULL) {
		viewname = "";
		forview = "";
	}
	result = ISC_R_SUCCESS;

	dns_fixedname_init(&fixed);
	name = dns_fixedname_name(&fixed);
	obj = cfg_tuple_get(rpz_obj, "zone list");
	for (element = cfg_list_first(obj);
	     element != NULL;
	     element = cfg_list_next(element)) {
		obj = cfg_listelt_value(element);
		nameobj = cfg_tuple_get(obj, "zone name");
		zonename = cfg_obj_asstring(nameobj);
		zonetype = "";

		tresult = dns_name_fromstring(name, zonename, 0, NULL);
		if (tresult != ISC_R_SUCCESS) {
			cfg_obj_log(nameobj, logctx, ISC_LOG_ERROR,
				   "bad domain name '%s'", zonename);
			if (result == ISC_R_SUCCESS)
				result = tresult;
			continue;
		}
		dns_name_format(name, namebuf, sizeof(namebuf));
		tresult = isc_symtab_lookup(symtab, namebuf, 3, &value);
		if (tresult == ISC_R_SUCCESS) {
			obj = NULL;
			zoneobj = value.as_cpointer;
			if (zoneobj != NULL && cfg_obj_istuple(zoneobj))
				zoneobj = cfg_tuple_get(zoneobj, "options");
			if (zoneobj != NULL && cfg_obj_ismap(zoneobj))
				(void)cfg_map_get(zoneobj, "type", &obj);
			if (obj != NULL)
				zonetype = cfg_obj_asstring(obj);
		}
		if (strcasecmp(zonetype, "master") != 0 &&
		    strcasecmp(zonetype, "slave") != 0) {
			cfg_obj_log(nameobj, logctx, ISC_LOG_ERROR,
				    "%s '%s'%s%s is not a master or slave zone",
				    rpz_catz, zonename, forview, viewname);
			if (result == ISC_R_SUCCESS)
				result = ISC_R_FAILURE;
		}
	}
	return (result);
}

static isc_result_t
check_viewconf(const cfg_obj_t *config, const cfg_obj_t *voptions,
	       const char *viewname, dns_rdataclass_t vclass,
	       isc_symtab_t *files, isc_symtab_t *inview,
	       isc_log_t *logctx, isc_mem_t *mctx)
{
	const cfg_obj_t *zones = NULL;
	const cfg_obj_t *keys = NULL;
	const cfg_listelt_t *element, *element2;
	isc_symtab_t *symtab = NULL;
	isc_result_t result = ISC_R_SUCCESS;
	isc_result_t tresult = ISC_R_SUCCESS;
	cfg_aclconfctx_t *actx = NULL;
	const cfg_obj_t *obj;
	const cfg_obj_t *options = NULL;
	const cfg_obj_t *opts = NULL;
	isc_boolean_t enablednssec, enablevalidation;
	const char *valstr = "no";
	unsigned int tflags, mflags;

	/*
	 * Get global options block
	 */
	(void)cfg_map_get(config, "options", &options);

	/*
	 * The most relevant options for this view
	 */
	if (voptions != NULL)
		opts = voptions;
	else
		opts = options;

	/*
	 * Check that all zone statements are syntactically correct and
	 * there are no duplicate zones.
	 */
	tresult = isc_symtab_create(mctx, 1000, freekey, mctx,
				    ISC_FALSE, &symtab);
	if (tresult != ISC_R_SUCCESS)
		return (ISC_R_NOMEMORY);

	cfg_aclconfctx_create(mctx, &actx);

	if (voptions != NULL)
		(void)cfg_map_get(voptions, "zone", &zones);
	else
		(void)cfg_map_get(config, "zone", &zones);

	for (element = cfg_list_first(zones);
	     element != NULL;
	     element = cfg_list_next(element))
	{
		const cfg_obj_t *zone = cfg_listelt_value(element);

		tresult = check_zoneconf(zone, voptions, config, symtab,
					 files, inview, viewname, vclass,
					 actx, logctx, mctx);
		if (tresult != ISC_R_SUCCESS)
			result = ISC_R_FAILURE;
	}

	/*
	 * Check that the response-policy refers to zones that exist.
	 */
	if (opts != NULL) {
		obj = NULL;
		if (cfg_map_get(opts, "response-policy", &obj) == ISC_R_SUCCESS
		    && check_rpz("response-policy zone", obj,
				 viewname, symtab, logctx) != ISC_R_SUCCESS)
			result = ISC_R_FAILURE;
	}

	isc_symtab_destroy(&symtab);

	/*
	 * Check that forwarding is reasonable.
	 */
	if (opts != NULL && check_forward(opts, NULL, logctx) != ISC_R_SUCCESS)
		result = ISC_R_FAILURE;

	/*
	 * Check non-zero options at the global and view levels.
	 */
	if (options != NULL && check_nonzero(options, logctx) != ISC_R_SUCCESS)
		result = ISC_R_FAILURE;
	if (voptions != NULL &&check_nonzero(voptions, logctx) != ISC_R_SUCCESS)
		result = ISC_R_FAILURE;

	/*
	 * Check that dual-stack-servers is reasonable.
	 */
	if (opts != NULL && check_dual_stack(opts, logctx) != ISC_R_SUCCESS)
		result = ISC_R_FAILURE;

	/*
	 * Check that rrset-order is reasonable.
	 */
	if (opts != NULL && check_order(opts, logctx) != ISC_R_SUCCESS)
		result = ISC_R_FAILURE;

	/*
	 * Check that all key statements are syntactically correct and
	 * there are no duplicate keys.
	 */
	tresult = isc_symtab_create(mctx, 1000, freekey, mctx,
				    ISC_FALSE, &symtab);
	if (tresult != ISC_R_SUCCESS)
		goto cleanup;

	(void)cfg_map_get(config, "key", &keys);
	tresult = check_keylist(keys, symtab, mctx, logctx);
	if (tresult == ISC_R_EXISTS)
		result = ISC_R_FAILURE;
	else if (tresult != ISC_R_SUCCESS) {
		result = tresult;
		goto cleanup;
	}

	if (voptions != NULL) {
		keys = NULL;
		(void)cfg_map_get(voptions, "key", &keys);
		tresult = check_keylist(keys, symtab, mctx, logctx);
		if (tresult == ISC_R_EXISTS)
			result = ISC_R_FAILURE;
		else if (tresult != ISC_R_SUCCESS) {
			result = tresult;
			goto cleanup;
		}
	}

	/*
	 * Global servers can refer to keys in views.
	 */
	if (check_servers(config, voptions, symtab, logctx) != ISC_R_SUCCESS)
		result = ISC_R_FAILURE;

	isc_symtab_destroy(&symtab);

	/*
	 * Check that dnssec-enable/dnssec-validation are sensible.
	 */
	obj = NULL;
	if (voptions != NULL)
		(void)cfg_map_get(voptions, "dnssec-enable", &obj);
	if (obj == NULL && options != NULL)
		(void)cfg_map_get(options, "dnssec-enable", &obj);
	if (obj == NULL)
		enablednssec = ISC_TRUE;
	else
		enablednssec = cfg_obj_asboolean(obj);

	obj = NULL;
	if (voptions != NULL)
		(void)cfg_map_get(voptions, "dnssec-validation", &obj);
	if (obj == NULL && options != NULL)
		(void)cfg_map_get(options, "dnssec-validation", &obj);
	if (obj == NULL) {
		enablevalidation = enablednssec;
		valstr = "yes";
	} else if (cfg_obj_isboolean(obj)) {
		enablevalidation = cfg_obj_asboolean(obj);
		valstr = enablevalidation ? "yes" : "no";
	} else {
		enablevalidation = ISC_TRUE;
		valstr = "auto";
	}

	if (enablevalidation && !enablednssec)
		cfg_obj_log(obj, logctx, ISC_LOG_WARNING,
			    "'dnssec-validation %s;' and 'dnssec-enable no;'",
			    valstr);

	/*
	 * Check trusted-keys and managed-keys.
	 */
	keys = NULL;
	if (voptions != NULL)
		(void)cfg_map_get(voptions, "trusted-keys", &keys);
	if (keys == NULL)
		(void)cfg_map_get(config, "trusted-keys", &keys);

	tflags = 0;
	for (element = cfg_list_first(keys);
	     element != NULL;
	     element = cfg_list_next(element))
	{
		const cfg_obj_t *keylist = cfg_listelt_value(element);
		for (element2 = cfg_list_first(keylist);
		     element2 != NULL;
		     element2 = cfg_list_next(element2)) {
			obj = cfg_listelt_value(element2);
			tresult = check_trusted_key(obj, ISC_FALSE, &tflags,
						    logctx);
			if (tresult != ISC_R_SUCCESS)
				result = tresult;
		}
	}

	if ((tflags & ROOT_KSK_2010) != 0 && (tflags & ROOT_KSK_2017) == 0) {
		cfg_obj_log(keys, logctx, ISC_LOG_WARNING,
			    "trusted-key for root from 2010 without updated "
			    "trusted-key from 2017: THIS WILL FAIL AFTER "
			    "KEY ROLLOVER");
	}

	if ((tflags & DLV_KSK_KEY) != 0) {
		cfg_obj_log(keys, logctx, ISC_LOG_WARNING,
			    "trusted-key for dlv.isc.org still present; "
			    "dlv.isc.org has been shut down");
	}

	keys = NULL;
	if (voptions != NULL)
		(void)cfg_map_get(voptions, "managed-keys", &keys);
	if (keys == NULL)
		(void)cfg_map_get(config, "managed-keys", &keys);

	mflags = 0;
	for (element = cfg_list_first(keys);
	     element != NULL;
	     element = cfg_list_next(element))
	{
		const cfg_obj_t *keylist = cfg_listelt_value(element);
		for (element2 = cfg_list_first(keylist);
		     element2 != NULL;
		     element2 = cfg_list_next(element2)) {
			obj = cfg_listelt_value(element2);
			tresult = check_trusted_key(obj, ISC_TRUE, &mflags,
						    logctx);
			if (tresult != ISC_R_SUCCESS)
				result = tresult;
		}
	}

	if ((mflags & ROOT_KSK_2010) != 0 && (mflags & ROOT_KSK_2017) == 0) {
		cfg_obj_log(keys, logctx, ISC_LOG_WARNING,
			    "managed-key for root from 2010 without updated "
			    "managed-key from 2017");
	}

	if ((mflags & DLV_KSK_KEY) != 0) {
		cfg_obj_log(keys, logctx, ISC_LOG_WARNING,
			    "managed-key for dlv.isc.org still present; "
			    "dlv.isc.org has been shut down");
	}

	if ((tflags & (ROOT_KSK_2010|ROOT_KSK_2017)) != 0 &&
	    (mflags & (ROOT_KSK_2010|ROOT_KSK_2017)) != 0)
	{
		cfg_obj_log(keys, logctx, ISC_LOG_WARNING,
			    "both trusted-keys and managed-keys for the ICANN "
			    "root are present");
	}

	/*
	 * Check options.
	 */
	if (voptions != NULL)
		tresult = check_options(voptions, logctx, mctx,
					optlevel_view);
	else
		tresult = check_options(config, logctx, mctx,
					optlevel_config);
	if (tresult != ISC_R_SUCCESS)
		result = tresult;

	tresult = check_viewacls(actx, voptions, config, logctx, mctx);
	if (tresult != ISC_R_SUCCESS)
		result = tresult;

	tresult = check_recursionacls(actx, voptions, viewname,
				      config, logctx, mctx);
	if (tresult != ISC_R_SUCCESS)
		result = tresult;

	tresult = check_filteraaaa(actx, voptions, viewname, config,
				   logctx, mctx);
	if (tresult != ISC_R_SUCCESS)
		result = tresult;

	tresult = check_dns64(actx, voptions, config, logctx, mctx);
	if (tresult != ISC_R_SUCCESS)
		result = tresult;

	tresult = check_ratelimit(actx, voptions, config, logctx, mctx);
	if (tresult != ISC_R_SUCCESS)
		result = tresult;

 cleanup:
	if (symtab != NULL)
		isc_symtab_destroy(&symtab);
	if (actx != NULL)
		cfg_aclconfctx_detach(&actx);

	return (result);
}

static const char *
default_channels[] = {
	"default_syslog",
	"default_stderr",
	"default_debug",
	"null",
	NULL
};

static isc_result_t
bind9_check_logging(const cfg_obj_t *config, isc_log_t *logctx,
		    isc_mem_t *mctx)
{
	const cfg_obj_t *categories = NULL;
	const cfg_obj_t *category;
	const cfg_obj_t *channels = NULL;
	const cfg_obj_t *channel;
	const cfg_listelt_t *element;
	const cfg_listelt_t *delement;
	const char *channelname;
	const char *catname;
	const cfg_obj_t *fileobj = NULL;
	const cfg_obj_t *syslogobj = NULL;
	const cfg_obj_t *nullobj = NULL;
	const cfg_obj_t *stderrobj = NULL;
	const cfg_obj_t *logobj = NULL;
	isc_result_t result = ISC_R_SUCCESS;
	isc_result_t tresult;
	isc_symtab_t *symtab = NULL;
	isc_symvalue_t symvalue;
	int i;

	(void)cfg_map_get(config, "logging", &logobj);
	if (logobj == NULL)
		return (ISC_R_SUCCESS);

	result = isc_symtab_create(mctx, 100, NULL, NULL, ISC_FALSE, &symtab);
	if (result != ISC_R_SUCCESS)
		return (result);

	symvalue.as_cpointer = NULL;
	for (i = 0; default_channels[i] != NULL; i++) {
		tresult = isc_symtab_define(symtab, default_channels[i], 1,
					    symvalue, isc_symexists_replace);
		if (tresult != ISC_R_SUCCESS)
			result = tresult;
	}

	cfg_map_get(logobj, "channel", &channels);

	for (element = cfg_list_first(channels);
	     element != NULL;
	     element = cfg_list_next(element))
	{
		channel = cfg_listelt_value(element);
		channelname = cfg_obj_asstring(cfg_map_getname(channel));
		fileobj = syslogobj = nullobj = stderrobj = NULL;
		(void)cfg_map_get(channel, "file", &fileobj);
		(void)cfg_map_get(channel, "syslog", &syslogobj);
		(void)cfg_map_get(channel, "null", &nullobj);
		(void)cfg_map_get(channel, "stderr", &stderrobj);
		i = 0;
		if (fileobj != NULL)
			i++;
		if (syslogobj != NULL)
			i++;
		if (nullobj != NULL)
			i++;
		if (stderrobj != NULL)
			i++;
		if (i != 1) {
			cfg_obj_log(channel, logctx, ISC_LOG_ERROR,
				    "channel '%s': exactly one of file, syslog, "
				    "null, and stderr must be present",
				     channelname);
			result = ISC_R_FAILURE;
		}
		tresult = isc_symtab_define(symtab, channelname, 1,
					    symvalue, isc_symexists_replace);
		if (tresult != ISC_R_SUCCESS)
			result = tresult;
	}

	cfg_map_get(logobj, "category", &categories);

	for (element = cfg_list_first(categories);
	     element != NULL;
	     element = cfg_list_next(element))
	{
		category = cfg_listelt_value(element);
		catname = cfg_obj_asstring(cfg_tuple_get(category, "name"));
		if (isc_log_categorybyname(logctx, catname) == NULL) {
			cfg_obj_log(category, logctx, ISC_LOG_ERROR,
				    "undefined category: '%s'", catname);
			result = ISC_R_FAILURE;
		}
		channels = cfg_tuple_get(category, "destinations");
		for (delement = cfg_list_first(channels);
		     delement != NULL;
		     delement = cfg_list_next(delement))
		{
			channel = cfg_listelt_value(delement);
			channelname = cfg_obj_asstring(channel);
			tresult = isc_symtab_lookup(symtab, channelname, 1,
						    &symvalue);
			if (tresult != ISC_R_SUCCESS) {
				cfg_obj_log(channel, logctx, ISC_LOG_ERROR,
					    "undefined channel: '%s'",
					    channelname);
				result = tresult;
			}
		}
	}
	isc_symtab_destroy(&symtab);
	return (result);
}

static isc_result_t
bind9_check_controlskeys(const cfg_obj_t *control, const cfg_obj_t *keylist,
			 isc_log_t *logctx)
{
	isc_result_t result = ISC_R_SUCCESS;
	const cfg_obj_t *control_keylist;
	const cfg_listelt_t *element;
	const cfg_obj_t *key;
	const char *keyval;

	control_keylist = cfg_tuple_get(control, "keys");
	if (cfg_obj_isvoid(control_keylist))
		return (ISC_R_SUCCESS);

	for (element = cfg_list_first(control_keylist);
	     element != NULL;
	     element = cfg_list_next(element))
	{
		key = cfg_listelt_value(element);
		keyval = cfg_obj_asstring(key);

		if (!rndckey_exists(keylist, keyval)) {
			cfg_obj_log(key, logctx, ISC_LOG_ERROR,
				    "unknown key '%s'", keyval);
			result = ISC_R_NOTFOUND;
		}
	}
	return (result);
}

static isc_result_t
bind9_check_controls(const cfg_obj_t *config, isc_log_t *logctx,
		     isc_mem_t *mctx)
{
	isc_result_t result = ISC_R_SUCCESS, tresult;
	cfg_aclconfctx_t *actx = NULL;
	const cfg_listelt_t *element, *element2;
	const cfg_obj_t *allow;
	const cfg_obj_t *control;
	const cfg_obj_t *controls;
	const cfg_obj_t *controlslist = NULL;
	const cfg_obj_t *inetcontrols;
	const cfg_obj_t *unixcontrols;
	const cfg_obj_t *keylist = NULL;
	const char *path;
	isc_uint32_t perm, mask;
	dns_acl_t *acl = NULL;
	isc_sockaddr_t addr;
	int i;

	(void)cfg_map_get(config, "controls", &controlslist);
	if (controlslist == NULL)
		return (ISC_R_SUCCESS);

	(void)cfg_map_get(config, "key", &keylist);

	cfg_aclconfctx_create(mctx, &actx);

	/*
	 * INET: Check allow clause.
	 * UNIX: Check "perm" for sanity, check path length.
	 */
	for (element = cfg_list_first(controlslist);
	     element != NULL;
	     element = cfg_list_next(element)) {
		controls = cfg_listelt_value(element);
		unixcontrols = NULL;
		inetcontrols = NULL;
		(void)cfg_map_get(controls, "unix", &unixcontrols);
		(void)cfg_map_get(controls, "inet", &inetcontrols);
		for (element2 = cfg_list_first(inetcontrols);
		     element2 != NULL;
		     element2 = cfg_list_next(element2)) {
			control = cfg_listelt_value(element2);
			allow = cfg_tuple_get(control, "allow");
			tresult = cfg_acl_fromconfig(allow, config, logctx,
						     actx, mctx, 0, &acl);
			if (acl != NULL)
				dns_acl_detach(&acl);
			if (tresult != ISC_R_SUCCESS)
				result = tresult;
			tresult = bind9_check_controlskeys(control, keylist,
							   logctx);
			if (tresult != ISC_R_SUCCESS)
				result = tresult;
		}
		for (element2 = cfg_list_first(unixcontrols);
		     element2 != NULL;
		     element2 = cfg_list_next(element2)) {
			control = cfg_listelt_value(element2);
			path = cfg_obj_asstring(cfg_tuple_get(control, "path"));
			tresult = isc_sockaddr_frompath(&addr, path);
			if (tresult == ISC_R_NOSPACE) {
				cfg_obj_log(control, logctx, ISC_LOG_ERROR,
					    "unix control '%s': path too long",
					    path);
				result = ISC_R_NOSPACE;
			}
			perm = cfg_obj_asuint32(cfg_tuple_get(control, "perm"));
			for (i = 0; i < 3; i++) {
#ifdef NEED_SECURE_DIRECTORY
				mask = (0x1 << (i*3));	/* SEARCH */
#else
				mask = (0x6 << (i*3)); 	/* READ + WRITE */
#endif
				if ((perm & mask) == mask)
					break;
			}
			if (i == 0) {
				cfg_obj_log(control, logctx, ISC_LOG_WARNING,
					    "unix control '%s' allows access "
					    "to everyone", path);
			} else if (i == 3) {
				cfg_obj_log(control, logctx, ISC_LOG_WARNING,
					    "unix control '%s' allows access "
					    "to nobody", path);
			}
			tresult = bind9_check_controlskeys(control, keylist,
							   logctx);
			if (tresult != ISC_R_SUCCESS)
				result = tresult;
		}
	}
	cfg_aclconfctx_detach(&actx);
	return (result);
}

isc_result_t
bind9_check_namedconf(const cfg_obj_t *config, isc_log_t *logctx,
		      isc_mem_t *mctx)
{
	const cfg_obj_t *options = NULL;
	const cfg_obj_t *views = NULL;
	const cfg_obj_t *acls = NULL;
	const cfg_obj_t *kals = NULL;
	const cfg_obj_t *obj;
	const cfg_listelt_t *velement;
	isc_result_t result = ISC_R_SUCCESS;
	isc_result_t tresult;
	isc_symtab_t *symtab = NULL;
	isc_symtab_t *files = NULL;
	isc_symtab_t *inview = NULL;

	static const char *builtin[] = { "localhost", "localnets",
					 "any", "none"};

	(void)cfg_map_get(config, "options", &options);

	if (options != NULL &&
	    check_options(options, logctx, mctx,
			  optlevel_options) != ISC_R_SUCCESS)
		result = ISC_R_FAILURE;

	if (bind9_check_logging(config, logctx, mctx) != ISC_R_SUCCESS)
		result = ISC_R_FAILURE;

	if (bind9_check_controls(config, logctx, mctx) != ISC_R_SUCCESS)
		result = ISC_R_FAILURE;

	(void)cfg_map_get(config, "view", &views);

	if (views != NULL && options != NULL)
		if (check_dual_stack(options, logctx) != ISC_R_SUCCESS)
			result = ISC_R_FAILURE;

	/*
	 * Use case insensitve comparision as not all file systems are
	 * case sensitive. This will prevent people using FOO.DB and foo.db
	 * on case sensitive file systems but that shouldn't be a major issue.
	 */
	tresult = isc_symtab_create(mctx, 100, NULL, NULL, ISC_FALSE,
				    &files);
	if (tresult != ISC_R_SUCCESS) {
		result = tresult;
		goto cleanup;
	}

	tresult = isc_symtab_create(mctx, 100, freekey, mctx,
				    ISC_TRUE, &inview);
	if (tresult != ISC_R_SUCCESS) {
		result = tresult;
		goto cleanup;
	}

	if (views == NULL) {
		tresult = check_viewconf(config, NULL, NULL, dns_rdataclass_in,
					 files, inview, logctx, mctx);
		if (result == ISC_R_SUCCESS && tresult != ISC_R_SUCCESS) {
			result = ISC_R_FAILURE;
		}
	} else {
		const cfg_obj_t *zones = NULL;

		(void)cfg_map_get(config, "zone", &zones);
		if (zones != NULL) {
			cfg_obj_log(zones, logctx, ISC_LOG_ERROR,
				    "when using 'view' statements, "
				    "all zones must be in views");
			result = ISC_R_FAILURE;
		}
	}

	tresult = isc_symtab_create(mctx, 100, NULL, NULL, ISC_TRUE, &symtab);
	if (tresult != ISC_R_SUCCESS) {
		result = tresult;
		goto cleanup;
	}
	for (velement = cfg_list_first(views);
	     velement != NULL;
	     velement = cfg_list_next(velement))
	{
		const cfg_obj_t *view = cfg_listelt_value(velement);
		const cfg_obj_t *vname = cfg_tuple_get(view, "name");
		const cfg_obj_t *voptions = cfg_tuple_get(view, "options");
		const cfg_obj_t *vclassobj = cfg_tuple_get(view, "class");
		dns_rdataclass_t vclass = dns_rdataclass_in;
		const char *key = cfg_obj_asstring(vname);
		isc_symvalue_t symvalue;
		unsigned int symtype;

		tresult = ISC_R_SUCCESS;
		if (cfg_obj_isstring(vclassobj)) {
			isc_textregion_t r;

			DE_CONST(cfg_obj_asstring(vclassobj), r.base);
			r.length = strlen(r.base);
			tresult = dns_rdataclass_fromtext(&vclass, &r);
			if (tresult != ISC_R_SUCCESS)
				cfg_obj_log(vclassobj, logctx, ISC_LOG_ERROR,
					    "view '%s': invalid class %s",
					    cfg_obj_asstring(vname), r.base);
		}
		symtype = vclass + 1;
		if (tresult == ISC_R_SUCCESS && symtab != NULL) {
			symvalue.as_cpointer = view;
			tresult = isc_symtab_define(symtab, key, symtype,
						    symvalue,
						    isc_symexists_reject);
			if (tresult == ISC_R_EXISTS) {
				const char *file;
				unsigned int line;
				RUNTIME_CHECK(isc_symtab_lookup(symtab, key,
					 symtype, &symvalue) == ISC_R_SUCCESS);
				file = cfg_obj_file(symvalue.as_cpointer);
				line = cfg_obj_line(symvalue.as_cpointer);
				cfg_obj_log(view, logctx, ISC_LOG_ERROR,
					    "view '%s': already exists "
					    "previous definition: %s:%u",
					    key, file, line);
				result = tresult;
			} else if (tresult != ISC_R_SUCCESS) {
				result = tresult;
			} else if ((strcasecmp(key, "_bind") == 0 &&
				    vclass == dns_rdataclass_ch) ||
				   (strcasecmp(key, "_default") == 0 &&
				    vclass == dns_rdataclass_in)) {
				cfg_obj_log(view, logctx, ISC_LOG_ERROR,
					    "attempt to redefine builtin view "
					    "'%s'", key);
				result = ISC_R_EXISTS;
			}
		}
		if (tresult == ISC_R_SUCCESS)
			tresult = check_viewconf(config, voptions, key, vclass,
						 files, inview, logctx, mctx);
		if (tresult != ISC_R_SUCCESS)
			result = ISC_R_FAILURE;
	}

	if (views != NULL && options != NULL) {
		obj = NULL;
		tresult = cfg_map_get(options, "cache-file", &obj);
		if (tresult == ISC_R_SUCCESS) {
			cfg_obj_log(obj, logctx, ISC_LOG_ERROR,
				    "'cache-file' cannot be a global "
				    "option if views are present");
			result = ISC_R_FAILURE;
		}
	}

	cfg_map_get(config, "acl", &acls);

	if (acls != NULL) {
		const cfg_listelt_t *elt;
		const cfg_listelt_t *elt2;
		const char *aclname;

		for (elt = cfg_list_first(acls);
		     elt != NULL;
		     elt = cfg_list_next(elt)) {
			const cfg_obj_t *acl = cfg_listelt_value(elt);
			unsigned int line = cfg_obj_line(acl);
			unsigned int i;

			aclname = cfg_obj_asstring(cfg_tuple_get(acl, "name"));
			for (i = 0;
			     i < sizeof(builtin) / sizeof(builtin[0]);
			     i++)
				if (strcasecmp(aclname, builtin[i]) == 0) {
					cfg_obj_log(acl, logctx, ISC_LOG_ERROR,
						    "attempt to redefine "
						    "builtin acl '%s'",
						    aclname);
					result = ISC_R_FAILURE;
					break;
				}

			for (elt2 = cfg_list_next(elt);
			     elt2 != NULL;
			     elt2 = cfg_list_next(elt2)) {
				const cfg_obj_t *acl2 = cfg_listelt_value(elt2);
				const char *name;
				name = cfg_obj_asstring(cfg_tuple_get(acl2,
								      "name"));
				if (strcasecmp(aclname, name) == 0) {
					const char *file = cfg_obj_file(acl);

					if (file == NULL)
						file = "<unknown file>";

					cfg_obj_log(acl2, logctx, ISC_LOG_ERROR,
						    "attempt to redefine "
						    "acl '%s' previous "
						    "definition: %s:%u",
						     name, file, line);
					result = ISC_R_FAILURE;
				}
			}
		}
	}

	tresult = cfg_map_get(config, "kal", &kals);
	if (tresult == ISC_R_SUCCESS) {
		const cfg_listelt_t *elt;
		const cfg_listelt_t *elt2;
		const char *aclname;

		for (elt = cfg_list_first(kals);
		     elt != NULL;
		     elt = cfg_list_next(elt)) {
			const cfg_obj_t *acl = cfg_listelt_value(elt);

			aclname = cfg_obj_asstring(cfg_tuple_get(acl, "name"));

			for (elt2 = cfg_list_next(elt);
			     elt2 != NULL;
			     elt2 = cfg_list_next(elt2)) {
				const cfg_obj_t *acl2 = cfg_listelt_value(elt2);
				const char *name;
				name = cfg_obj_asstring(cfg_tuple_get(acl2,
								      "name"));
				if (strcasecmp(aclname, name) == 0) {
					const char *file = cfg_obj_file(acl);
					unsigned int line = cfg_obj_line(acl);

					if (file == NULL)
						file = "<unknown file>";

					cfg_obj_log(acl2, logctx, ISC_LOG_ERROR,
						    "attempt to redefine "
						    "kal '%s' previous "
						    "definition: %s:%u",
						     name, file, line);
					result = ISC_R_FAILURE;
				}
			}
		}
	}

cleanup:
	if (symtab != NULL)
		isc_symtab_destroy(&symtab);
	if (inview != NULL)
		isc_symtab_destroy(&inview);
	if (files != NULL)
		isc_symtab_destroy(&files);

	return (result);
}
