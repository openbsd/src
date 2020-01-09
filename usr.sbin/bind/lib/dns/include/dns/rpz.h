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

/* $Id: rpz.h,v 1.3 2020/01/09 18:17:16 florian Exp $ */


#ifndef DNS_RPZ_H
#define DNS_RPZ_H 1

#include <isc/event.h>
#include <isc/lang.h>
#include <isc/refcount.h>
#include <isc/rwlock.h>

#include <dns/fixedname.h>
#include <dns/rdata.h>
#include <dns/types.h>

ISC_LANG_BEGINDECLS

#define DNS_RPZ_PREFIX		"rpz-"
/*
 * Sub-zones of various trigger types.
 */
#define DNS_RPZ_CLIENT_IP_ZONE	DNS_RPZ_PREFIX"client-ip"
#define DNS_RPZ_IP_ZONE		DNS_RPZ_PREFIX"ip"
#define DNS_RPZ_NSIP_ZONE	DNS_RPZ_PREFIX"nsip"
#define DNS_RPZ_NSDNAME_ZONE	DNS_RPZ_PREFIX"nsdname"
/*
 * Special policies.
 */
#define DNS_RPZ_PASSTHRU_NAME	DNS_RPZ_PREFIX"passthru"
#define DNS_RPZ_DROP_NAME	DNS_RPZ_PREFIX"drop"
#define DNS_RPZ_TCP_ONLY_NAME	DNS_RPZ_PREFIX"tcp-only"


typedef uint8_t		dns_rpz_prefix_t;

typedef enum {
	DNS_RPZ_TYPE_BAD,
	DNS_RPZ_TYPE_CLIENT_IP,
	DNS_RPZ_TYPE_QNAME,
	DNS_RPZ_TYPE_IP,
	DNS_RPZ_TYPE_NSDNAME,
	DNS_RPZ_TYPE_NSIP
} dns_rpz_type_t;

/*
 * Require DNS_RPZ_POLICY_PASSTHRU < DNS_RPZ_POLICY_DROP
 * < DNS_RPZ_POLICY_TCP_ONLY DNS_RPZ_POLICY_NXDOMAIN < DNS_RPZ_POLICY_NODATA
 * < DNS_RPZ_POLICY_CNAME to choose among competing policies.
 */
typedef enum {
	DNS_RPZ_POLICY_GIVEN = 0,	/* 'given': what policy record says */
	DNS_RPZ_POLICY_DISABLED = 1,	/* log what would have happened */
	DNS_RPZ_POLICY_PASSTHRU = 2,	/* 'passthru': do not rewrite */
	DNS_RPZ_POLICY_DROP = 3,	/* 'drop': do not respond */
	DNS_RPZ_POLICY_TCP_ONLY = 4,	/* 'tcp-only': answer UDP with TC=1 */
	DNS_RPZ_POLICY_NXDOMAIN = 5,	/* 'nxdomain': answer with NXDOMAIN */
	DNS_RPZ_POLICY_NODATA = 6,	/* 'nodata': answer with ANCOUNT=0 */
	DNS_RPZ_POLICY_CNAME = 7,	/* 'cname x': answer with x's rrsets */
	DNS_RPZ_POLICY_RECORD,
	DNS_RPZ_POLICY_WILDCNAME,
	DNS_RPZ_POLICY_MISS,
	DNS_RPZ_POLICY_ERROR
} dns_rpz_policy_t;

typedef uint8_t	    dns_rpz_num_t;

#define DNS_RPZ_MAX_ZONES   32
#if DNS_RPZ_MAX_ZONES > 32
# if DNS_RPZ_MAX_ZONES > 64
#  error "rpz zone bit masks must fit in a word"
# endif
typedef uint64_t	    dns_rpz_zbits_t;
#else
typedef uint32_t	    dns_rpz_zbits_t;
#endif

#define DNS_RPZ_ALL_ZBITS   ((dns_rpz_zbits_t)-1)

#define DNS_RPZ_INVALID_NUM DNS_RPZ_MAX_ZONES

#define DNS_RPZ_ZBIT(n)	    (((dns_rpz_zbits_t)1) << (dns_rpz_num_t)(n))

/*
 * Mask of the specified and higher numbered policy zones
 * Avoid hassles with (1<<33) or (1<<65)
 */
#define DNS_RPZ_ZMASK(n)    ((dns_rpz_zbits_t)((((n) >= DNS_RPZ_MAX_ZONES-1) ? \
						0 : (1<<((n)+1))) -1))

/*
 * The trigger counter type.
 */
typedef size_t dns_rpz_trigger_counter_t;

/*
 * The number of triggers of each type in a response policy zone.
 */
typedef struct dns_rpz_triggers dns_rpz_triggers_t;
struct dns_rpz_triggers {
	dns_rpz_trigger_counter_t	client_ipv4;
	dns_rpz_trigger_counter_t	client_ipv6;
	dns_rpz_trigger_counter_t	qname;
	dns_rpz_trigger_counter_t	ipv4;
	dns_rpz_trigger_counter_t	ipv6;
	dns_rpz_trigger_counter_t	nsdname;
	dns_rpz_trigger_counter_t	nsipv4;
	dns_rpz_trigger_counter_t	nsipv6;
};

/*
 * A single response policy zone.
 */
typedef struct dns_rpz_zone dns_rpz_zone_t;
struct dns_rpz_zone {
	isc_refcount_t	refs;
	dns_rpz_num_t	num;		/* ordinal in list of policy zones */
	dns_name_t	origin;		/* Policy zone name */
	dns_name_t	client_ip;	/* DNS_RPZ_CLIENT_IP_ZONE.origin. */
	dns_name_t	ip;		/* DNS_RPZ_IP_ZONE.origin. */
	dns_name_t	nsdname;	/* DNS_RPZ_NSDNAME_ZONE.origin */
	dns_name_t	nsip;		/* DNS_RPZ_NSIP_ZONE.origin. */
	dns_name_t	passthru;	/* DNS_RPZ_PASSTHRU_NAME. */
	dns_name_t	drop;		/* DNS_RPZ_DROP_NAME. */
	dns_name_t	tcp_only;	/* DNS_RPZ_TCP_ONLY_NAME. */
	dns_name_t	cname;		/* override value for ..._CNAME */
	dns_ttl_t	max_policy_ttl;
	dns_rpz_policy_t policy;	/* DNS_RPZ_POLICY_GIVEN or override */
};

/*
 * Radix tree node for response policy IP addresses
 */
typedef struct dns_rpz_cidr_node dns_rpz_cidr_node_t;

/*
 * Bitfields indicating which policy zones have policies of
 * which type.
 */
typedef struct dns_rpz_have dns_rpz_have_t;
struct dns_rpz_have {
	dns_rpz_zbits_t	    client_ipv4;
	dns_rpz_zbits_t	    client_ipv6;
	dns_rpz_zbits_t	    client_ip;
	dns_rpz_zbits_t	    qname;
	dns_rpz_zbits_t	    ipv4;
	dns_rpz_zbits_t	    ipv6;
	dns_rpz_zbits_t	    ip;
	dns_rpz_zbits_t	    nsdname;
	dns_rpz_zbits_t	    nsipv4;
	dns_rpz_zbits_t	    nsipv6;
	dns_rpz_zbits_t	    nsip;
	dns_rpz_zbits_t	    qname_skip_recurse;
};

/*
 * Policy options
 */
typedef struct dns_rpz_popt dns_rpz_popt_t;
struct dns_rpz_popt {
	dns_rpz_zbits_t	    no_rd_ok;
	isc_boolean_t	    break_dnssec;
	isc_boolean_t	    qname_wait_recurse;
	unsigned int	    min_ns_labels;
	dns_rpz_num_t	    num_zones;
};

/*
 * Response policy zones known to a view.
 */
typedef struct dns_rpz_zones dns_rpz_zones_t;
struct dns_rpz_zones {
	dns_rpz_popt_t		p;
	dns_rpz_zone_t		*zones[DNS_RPZ_MAX_ZONES];
	dns_rpz_triggers_t	triggers[DNS_RPZ_MAX_ZONES];

	/*
	 * RPZ policy version number (initially 0, increases whenever
	 * the server is reconfigured with new zones or policy)
	 */
	int			rpz_ver;

	dns_rpz_zbits_t		defined;

	/*
	 * The set of records for a policy zone are in one of these states:
	 *	never loaded		    load_begun=0  have=0
	 *	during initial loading	    load_begun=1  have=0
	 *				and rbtdb->rpzsp == rbtdb->load_rpzsp
	 *	after good load		    load_begun=1  have!=0
	 *	after failed initial load   load_begun=1  have=0
	 *				and rbtdb->load_rpzsp == NULL
	 *	reloading after failure	    load_begun=1  have=0
	 *	reloading after success
	 *		main rpzs	    load_begun=1  have!=0
	 *		load rpzs	    load_begun=1  have=0
	 */
	dns_rpz_zbits_t		load_begun;
	dns_rpz_have_t		have;

	/*
	 * total_triggers maintains the total number of triggers in all
	 * policy zones in the view. It is only used to print summary
	 * statistics after a zone load of how the trigger counts
	 * changed.
	 */
	dns_rpz_triggers_t	total_triggers;

	isc_mem_t		*mctx;
	isc_refcount_t		refs;
	/*
	 * One lock for short term read-only search that guarantees the
	 * consistency of the pointers.
	 * A second lock for maintenance that guarantees no other thread
	 * is adding or deleting nodes.
	 */
	isc_rwlock_t		search_lock;
	isc_mutex_t		maint_lock;

	dns_rpz_cidr_node_t	*cidr;
	dns_rbt_t		*rbt;
};


/*
 * context for finding the best policy
 */
typedef struct {
	unsigned int		state;
# define DNS_RPZ_REWRITTEN	0x0001
# define DNS_RPZ_DONE_CLIENT_IP	0x0002	/* client IP address checked */
# define DNS_RPZ_DONE_QNAME	0x0004	/* qname checked */
# define DNS_RPZ_DONE_QNAME_IP	0x0008	/* IP addresses of qname checked */
# define DNS_RPZ_DONE_NSDNAME	0x0010	/* NS name missed; checking addresses */
# define DNS_RPZ_DONE_IPv4	0x0020
# define DNS_RPZ_RECURSING	0x0040
# define DNS_RPZ_ACTIVE		0x0080
	/*
	 * Best match so far.
	 */
	struct {
		dns_rpz_type_t		type;
		dns_rpz_zone_t		*rpz;
		dns_rpz_prefix_t	prefix;
		dns_rpz_policy_t	policy;
		dns_ttl_t		ttl;
		isc_result_t		result;
		dns_zone_t		*zone;
		dns_db_t		*db;
		dns_dbversion_t		*version;
		dns_dbnode_t		*node;
		dns_rdataset_t		*rdataset;
	} m;
	/*
	 * State for chasing IP addresses and NS names including recursion.
	 */
	struct {
		unsigned int		label;
		dns_db_t		*db;
		dns_rdataset_t		*ns_rdataset;
		dns_rdatatype_t		r_type;
		isc_result_t		r_result;
		dns_rdataset_t		*r_rdataset;
	} r;

	/*
	 * State of real query while recursing for NSIP or NSDNAME.
	 */
	struct {
		isc_result_t		result;
		isc_boolean_t		is_zone;
		isc_boolean_t		authoritative;
		dns_zone_t		*zone;
		dns_db_t		*db;
		dns_dbnode_t		*node;
		dns_rdataset_t		*rdataset;
		dns_rdataset_t		*sigrdataset;
		dns_rdatatype_t		qtype;
	} q;

	/*
	 * A copy of the 'have' and 'p' structures and the RPZ
	 * policy version as of the beginning of RPZ processing,
	 * used to avoid problems when policy is updated while
	 * RPZ recursion is ongoing.
	 */
	dns_rpz_have_t		have;
	dns_rpz_popt_t		popt;
	int			rpz_ver;

	/*
	 * p_name: current policy owner name
	 * r_name: recursing for this name to possible policy triggers
	 * f_name: saved found name from before recursion
	 */
	dns_name_t		*p_name;
	dns_name_t		*r_name;
	dns_name_t		*fname;
	dns_fixedname_t		_p_namef;
	dns_fixedname_t		_r_namef;
	dns_fixedname_t		_fnamef;
} dns_rpz_st_t;

#define DNS_RPZ_TTL_DEFAULT		5
#define DNS_RPZ_MAX_TTL_DEFAULT		DNS_RPZ_TTL_DEFAULT

/*
 * So various response policy zone messages can be turned up or down.
 */
#define DNS_RPZ_ERROR_LEVEL	ISC_LOG_WARNING
#define DNS_RPZ_INFO_LEVEL	ISC_LOG_INFO
#define DNS_RPZ_DEBUG_LEVEL1	ISC_LOG_DEBUG(1)
#define DNS_RPZ_DEBUG_LEVEL2	ISC_LOG_DEBUG(2)
#define DNS_RPZ_DEBUG_LEVEL3	ISC_LOG_DEBUG(3)
#define DNS_RPZ_DEBUG_QUIET	(DNS_RPZ_DEBUG_LEVEL3+1)

const char *
dns_rpz_type2str(dns_rpz_type_t type);

dns_rpz_policy_t
dns_rpz_str2policy(const char *str);

const char *
dns_rpz_policy2str(dns_rpz_policy_t policy);

dns_rpz_policy_t
dns_rpz_decode_cname(dns_rpz_zone_t *rpz, dns_rdataset_t *rdataset,
		     dns_name_t *selfname);

isc_result_t
dns_rpz_new_zones(dns_rpz_zones_t **rpzsp, isc_mem_t *mctx);

void
dns_rpz_attach_rpzs(dns_rpz_zones_t *source, dns_rpz_zones_t **target);

void
dns_rpz_detach_rpzs(dns_rpz_zones_t **rpzsp);

isc_result_t
dns_rpz_beginload(dns_rpz_zones_t **load_rpzsp,
		  dns_rpz_zones_t *rpzs, dns_rpz_num_t rpz_num);

isc_result_t
dns_rpz_ready(dns_rpz_zones_t *rpzs,
	      dns_rpz_zones_t **load_rpzsp, dns_rpz_num_t rpz_num);

isc_result_t
dns_rpz_add(dns_rpz_zones_t *rpzs, dns_rpz_num_t rpz_num, dns_name_t *name);

void
dns_rpz_delete(dns_rpz_zones_t *rpzs, dns_rpz_num_t rpz_num, dns_name_t *name);

dns_rpz_num_t
dns_rpz_find_ip(dns_rpz_zones_t *rpzs, dns_rpz_type_t rpz_type,
		dns_rpz_zbits_t zbits, const isc_netaddr_t *netaddr,
		dns_name_t *ip_name, dns_rpz_prefix_t *prefixp);

dns_rpz_zbits_t
dns_rpz_find_name(dns_rpz_zones_t *rpzs, dns_rpz_type_t rpz_type,
		  dns_rpz_zbits_t zbits, dns_name_t *trig_name);

ISC_LANG_ENDDECLS

#endif /* DNS_RPZ_H */

