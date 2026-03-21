/*
 * zonec.h -- zone compiler.
 *
 * Copyright (c) 2001-2006, NLnet Labs. All rights reserved.
 *
 * See LICENSE for the license.
 *
 */

#ifndef ZONEC_H
#define ZONEC_H

#include "namedb.h"

#define NSEC_WINDOW_COUNT     256
#define NSEC_WINDOW_BITS_COUNT 256
#define NSEC_WINDOW_BITS_SIZE  (NSEC_WINDOW_BITS_COUNT / 8)

#define IPSECKEY_NOGATEWAY      0       /* RFC 4025 */
#define IPSECKEY_IP4            1
#define IPSECKEY_IP6            2
#define IPSECKEY_DNAME          3

#define AMTRELAY_NOGATEWAY      0       /* RFC 8777 */
#define AMTRELAY_IP4            1
#define AMTRELAY_IP6            2
#define AMTRELAY_DNAME          3

#define LINEBUFSZ 1024

#define DEFAULT_TTL 3600

/* Some zones, such as DNS-SD zones, have RRsets with many RRs in them.
 * By minimizing the need to reallocate the list of RRs in an RRset,
 * we reduce memory fragmentation significantly for such zones.
 * To this end, `domain`, `type`, `rrset`, `rrset_prev`, `rr_count` and
 * `rrs` are used to commit the RRs within an RRset, that are grouped
 * together in a zone file, to the database in batches. 
 */
struct collect_rrs {
	struct domain *domain;
	int type;
	struct rrset *rrset;
#ifdef PACKED_STRUCTS
	struct rrset *rrset_prev;
#endif
	int rr_count;
	/* When the RRset is more than 256 RRs, the set will be committed in
	 * batches of 256 RRs (and resized if needed) */
	struct rr* rrs[256];
};

/* parse a zone into memory. name is origin. zonefile is file to read.
 * returns number of errors; failure may have read a partial zone */
unsigned int zonec_read(
	struct namedb *database,
	struct domain_table *domains,
	const char *name,
	const char *zonefile,
	struct zone *zone);

/** check SSHFP type for failures and emit warnings */
void check_sshfp(void);
void apex_rrset_checks(struct namedb* db, rrset_type* rrset,
	domain_type* domain);

#endif /* ZONEC_H */
