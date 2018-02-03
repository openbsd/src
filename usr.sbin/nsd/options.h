/*
 * options.h -- nsd.conf options definitions and prototypes
 *
 * Copyright (c) 2001-2006, NLnet Labs. All rights reserved.
 *
 * See LICENSE for the license.
 *
 */

#ifndef OPTIONS_H
#define OPTIONS_H

#include "config.h"
#include <stdarg.h>
#include "region-allocator.h"
#include "rbtree.h"
struct query;
struct dname;
struct tsig_key;
struct buffer;
struct nsd;

typedef struct nsd_options nsd_options_type;
typedef struct pattern_options pattern_options_type;
typedef struct zone_options zone_options_type;
typedef struct ip_address_option ip_address_option_type;
typedef struct acl_options acl_options_type;
typedef struct key_options key_options_type;
typedef struct config_parser_state config_parser_state_type;
/*
 * Options global for nsd.
 */
struct nsd_options {
	/* config file name */
	char* configfile;
	/* options for zones, by apex, contains zone_options */
	rbtree_type* zone_options;
	/* patterns, by name, contains pattern_options */
	rbtree_type* patterns;

	/* free space in zonelist file, contains zonelist_bucket */
	rbtree_type* zonefree;
	/* number of free space lines in zonelist file */
	size_t zonefree_number;
	/* zonelist file if open */
	FILE* zonelist;
	/* last offset in file (or 0 if none) */
	off_t zonelist_off;

	/* tree of zonestat names and their id values, entries are struct
	 * zonestatname with malloced key=stringname. The number of items
	 * is the max statnameid, no items are freed from this. 
	 * kept correct in the xfrd process, and on startup. */
	rbtree_type* zonestatnames;

	/* rbtree of keys defined, by name */
	rbtree_type* keys;

	/* list of ip addresses to bind to (or NULL for all) */
	struct ip_address_option* ip_addresses;

	int ip_transparent;
	int ip_freebind;
	int debug_mode;
	int verbosity;
	int hide_version;
	int do_ip4;
	int do_ip6;
	const char* database;
	const char* identity;
	const char* version;
	const char* logfile;
	int server_count;
	int tcp_count;
	int tcp_query_count;
	int tcp_timeout;
	int tcp_mss;
	int outgoing_tcp_mss;
	size_t ipv4_edns_size;
	size_t ipv6_edns_size;
	const char* pidfile;
	const char* port;
	int statistics;
	const char* chroot;
	const char* username;
	const char* zonesdir;
	const char* xfrdfile;
	const char* xfrdir;
	const char* zonelistfile;
	const char* nsid;
	int xfrd_reload_timeout;
	int zonefiles_check;
	int zonefiles_write;
	int log_time_ascii;
	int round_robin;
	int minimal_responses;
	int reuseport;

        /** remote control section. enable toggle. */
	int control_enable;
	/** the interfaces the remote control should listen on */
	struct ip_address_option* control_interface;
	/** port number for the control port */
	int control_port;
	/** private key file for server */
	char* server_key_file;
	/** certificate file for server */
	char* server_cert_file;
	/** private key file for nsd-control */
	char* control_key_file;
	/** certificate file for nsd-control */
	char* control_cert_file;

#ifdef RATELIMIT
	/** number of buckets in rrl hashtable */
	size_t rrl_size;
	/** max qps for queries, 0 is nolimit */
	size_t rrl_ratelimit;
	/** ratio of slipped responses, 0 is noslip */
	size_t rrl_slip;
	/** ip prefix length */
	size_t rrl_ipv4_prefix_length;
	size_t rrl_ipv6_prefix_length;
	/** max qps for whitelisted queries, 0 is nolimit */
	size_t rrl_whitelist_ratelimit;
#endif

	region_type* region;
};

struct ip_address_option {
	struct ip_address_option* next;
	char* address;
};

/*
 * Pattern of zone options, used to contain options for zone(s).
 */
struct pattern_options {
	rbnode_type node;
	const char* pname; /* name of the pattern, key of rbtree */
	const char* zonefile;
	struct acl_options* allow_notify;
	struct acl_options* request_xfr;
	struct acl_options* notify;
	struct acl_options* provide_xfr;
	struct acl_options* outgoing_interface;
	const char* zonestats;
#ifdef RATELIMIT
	uint16_t rrl_whitelist; /* bitmap with rrl types */
#endif
	uint8_t allow_axfr_fallback;
	uint8_t allow_axfr_fallback_is_default;
	uint8_t notify_retry;
	uint8_t notify_retry_is_default;
	uint8_t implicit; /* pattern is implicit, part_of_config zone used */
	uint8_t xfrd_flags;
	uint32_t max_refresh_time;
	uint8_t max_refresh_time_is_default;
	uint32_t min_refresh_time;
	uint8_t min_refresh_time_is_default;
	uint32_t max_retry_time;
	uint8_t max_retry_time_is_default;
	uint32_t min_retry_time;
	uint8_t min_retry_time_is_default;
	uint64_t size_limit_xfr;
	uint8_t multi_master_check;
} ATTR_PACKED;

#define PATTERN_IMPLICIT_MARKER "_implicit_"

/*
 * Options for a zone
 */
struct zone_options {
	/* key is dname of apex */
	rbnode_type node;

	/* is apex of the zone */
	const char* name;
	/* if not part of config, the offset and linesize of zonelist entry */
	off_t off;
	int linesize;
	/* pattern for the zone options, if zone is part_of_config, this is
	 * a anonymous pattern created in-place */
	struct pattern_options* pattern;
	/* zone is fixed into the main config, not in zonelist, cannot delete */
	uint8_t part_of_config;
} ATTR_PACKED;

union acl_addr_storage {
#ifdef INET6
	struct in_addr addr;
	struct in6_addr addr6;
#else
	struct in_addr addr;
#endif
};

/*
 * Access control list element
 */
struct acl_options {
	struct acl_options* next;

	/* options */
	time_t ixfr_disabled;
	int bad_xfr_count;
	uint8_t use_axfr_only;
	uint8_t allow_udp;

	/* ip address range */
	const char* ip_address_spec;
	uint8_t is_ipv6;
	unsigned int port;	/* is 0(no port) or suffix @port value */
	union acl_addr_storage addr;
	union acl_addr_storage range_mask;
	enum {
		acl_range_single = 0,	/* single address */
		acl_range_mask = 1,	/* 10.20.30.40&255.255.255.0 */
		acl_range_subnet = 2,	/* 10.20.30.40/28 */
		acl_range_minmax = 3	/* 10.20.30.40-10.20.30.60 (mask=max) */
	} rangetype;

	/* key */
	uint8_t nokey;
	uint8_t blocked;
	const char* key_name;
	struct key_options* key_options;
} ATTR_PACKED;

/*
 * Key definition
 */
struct key_options {
	rbnode_type node; /* key of tree is name */
	char* name;
	char* algorithm;
	char* secret;
	struct tsig_key* tsig_key;
} ATTR_PACKED;

/** zone list free space */
struct zonelist_free {
	struct zonelist_free* next;
	off_t off;
};
/** zonelist free bucket for a particular line length */
struct zonelist_bucket {
	rbnode_type node; /* key is ptr to linesize */
	int linesize;
	struct zonelist_free* list;
};

/* default zonefile write interval if database is "", in seconds */
#define ZONEFILES_WRITE_INTERVAL 3600

struct zonestatname {
	rbnode_type node; /* key is malloced string with cooked zonestat name */
	unsigned id; /* index in nsd.zonestat array */
};

/*
 * Used during options parsing
 */
struct config_parser_state {
	char* filename;
	const char* chroot;
	int line;
	int errors;
	int server_settings_seen;
	struct nsd_options* opt;
	struct pattern_options* current_pattern;
	struct zone_options* current_zone;
	struct key_options* current_key;
	struct ip_address_option* current_ip_address_option;
	struct acl_options* current_allow_notify;
	struct acl_options* current_request_xfr;
	struct acl_options* current_notify;
	struct acl_options* current_provide_xfr;
	struct acl_options* current_outgoing_interface;
	void (*err)(void*,const char*);
	void* err_arg;
};

extern config_parser_state_type* cfg_parser;

/* region will be put in nsd_options struct. Returns empty options struct. */
struct nsd_options* nsd_options_create(region_type* region);
/* the number of zones that are configured */
static inline size_t nsd_options_num_zones(struct nsd_options* opt)
{ return opt->zone_options->count; }
/* insert a zone into the main options tree, returns 0 on error */
int nsd_options_insert_zone(struct nsd_options* opt, struct zone_options* zone);
/* insert a pattern into the main options tree, returns 0 on error */
int nsd_options_insert_pattern(struct nsd_options* opt,
	struct pattern_options* pat);

/* parses options file. Returns false on failure. callback, if nonNULL,
 * gets called with error strings, default prints. */
int parse_options_file(struct nsd_options* opt, const char* file,
	void (*err)(void*,const char*), void* err_arg);
struct zone_options* zone_options_create(region_type* region);
void zone_options_delete(struct nsd_options* opt, struct zone_options* zone);
/* find a zone by apex domain name, or NULL if not found. */
struct zone_options* zone_options_find(struct nsd_options* opt,
	const struct dname* apex);
struct pattern_options* pattern_options_create(region_type* region);
struct pattern_options* pattern_options_find(struct nsd_options* opt, const char* name);
int pattern_options_equal(struct pattern_options* p, struct pattern_options* q);
void pattern_options_remove(struct nsd_options* opt, const char* name);
void pattern_options_add_modify(struct nsd_options* opt,
	struct pattern_options* p);
void pattern_options_marshal(struct buffer* buffer, struct pattern_options* p);
struct pattern_options* pattern_options_unmarshal(region_type* r,
	struct buffer* b);
struct key_options* key_options_create(region_type* region);
void key_options_insert(struct nsd_options* opt, struct key_options* key);
struct key_options* key_options_find(struct nsd_options* opt, const char* name);
void key_options_remove(struct nsd_options* opt, const char* name);
int key_options_equal(struct key_options* p, struct key_options* q);
void key_options_add_modify(struct nsd_options* opt, struct key_options* key);
/* read in zone list file. Returns false on failure */
int parse_zone_list_file(struct nsd_options* opt);
/* create zone entry and add to the zonelist file */
struct zone_options* zone_list_add(struct nsd_options* opt, const char* zname,
	const char* pname);
/* create zonelist entry, do not insert in file (called by _add) */
struct zone_options* zone_list_zone_insert(struct nsd_options* opt,
	const char* nm, const char* patnm, int linesize, off_t off);
void zone_list_del(struct nsd_options* opt, struct zone_options* zone);
void zone_list_compact(struct nsd_options* opt);
void zone_list_close(struct nsd_options* opt);

/* create zonestat name tree , for initially created zones */
void options_zonestatnames_create(struct nsd_options* opt);
/* Get zonestat id for zone options, add new entry if necessary.
 * instantiates the pattern's zonestat string */
unsigned getzonestatid(struct nsd_options* opt, struct zone_options* zopt);
/* create string, same options as zonefile but no chroot changes */
const char* config_cook_string(struct zone_options* zone, const char* input);

#if defined(HAVE_SSL)
/* tsig must be inited, adds all keys in options to tsig. */
void key_options_tsig_add(struct nsd_options* opt);
#endif

/* check acl list, acl number that matches if passed(0..),
 * or failure (-1) if dropped */
/* the reason why (the acl) is returned too (or NULL) */
int acl_check_incoming(struct acl_options* acl, struct query* q,
	struct acl_options** reason);
int acl_addr_matches_host(struct acl_options* acl, struct acl_options* host);
int acl_addr_matches(struct acl_options* acl, struct query* q);
int acl_key_matches(struct acl_options* acl, struct query* q);
int acl_addr_match_mask(uint32_t* a, uint32_t* b, uint32_t* mask, size_t sz);
int acl_addr_match_range(uint32_t* minval, uint32_t* x, uint32_t* maxval, size_t sz);

/* returns true if acls are both from the same host */
int acl_same_host(struct acl_options* a, struct acl_options* b);
/* find acl by number in the list */
struct acl_options* acl_find_num(struct acl_options* acl, int num);

/* see if two acl lists are the same (same elements in same order, or empty) */
int acl_list_equal(struct acl_options* p, struct acl_options* q);
/* see if two acl are the same */
int acl_equal(struct acl_options* p, struct acl_options* q);

/* see if a zone is a slave or a master zone */
int zone_is_slave(struct zone_options* opt);
/* create zonefile name, returns static pointer (perhaps to options data) */
const char* config_make_zonefile(struct zone_options* zone, struct nsd* nsd);

#define ZONEC_PCT_TIME 5 /* seconds, then it starts to print pcts */
#define ZONEC_PCT_COUNT 100000 /* elements before pct check is done */

/* parsing helpers */
void c_error(const char* msg);
void c_error_msg(const char* fmt, ...) ATTR_FORMAT(printf, 1, 2);
struct acl_options* parse_acl_info(region_type* region, char* ip,
	const char* key);
/* true if ipv6 address, false if ipv4 */
int parse_acl_is_ipv6(const char* p);
/* returns range type. mask is the 2nd part of the range */
int parse_acl_range_type(char* ip, char** mask);
/* parses subnet mask, fills 0 mask as well */
void parse_acl_range_subnet(char* p, void* addr, int maxbits);
/* clean up options */
void nsd_options_destroy(struct nsd_options* opt);
/* replace occurrences of one with two in buf, pass length of buffer */
void replace_str(char* buf, size_t len, const char* one, const char* two);
/* apply pattern to the existing pattern in the parser */
void config_apply_pattern(const char* name);

#endif /* OPTIONS_H */
