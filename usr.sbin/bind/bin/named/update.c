/*
 * Copyright (C) 1999-2002  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
 * INTERNET SOFTWARE CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* $ISC: update.c,v 1.88.2.3 2002/02/08 03:57:15 marka Exp $ */

#include <config.h>

#include <isc/print.h>
#include <isc/string.h>
#include <isc/taskpool.h>
#include <isc/util.h>

#include <dns/db.h>
#include <dns/dbiterator.h>
#include <dns/diff.h>
#include <dns/dnssec.h>
#include <dns/events.h>
#include <dns/fixedname.h>
#include <dns/journal.h>
#include <dns/message.h>
#include <dns/nxt.h>
#include <dns/rdataclass.h>
#include <dns/rdataset.h>
#include <dns/rdatasetiter.h>
#include <dns/soa.h>
#include <dns/ssu.h>
#include <dns/view.h>
#include <dns/zone.h>
#include <dns/zt.h>

#include <named/client.h>
#include <named/log.h>
#include <named/update.h>

/*
 * This module implements dynamic update as in RFC2136.
 */

/*
  XXX TODO:
  - document strict minimality
*/

/**************************************************************************/

/*
 * Log level for tracing dynamic update protocol requests.
 */
#define LOGLEVEL_PROTOCOL	ISC_LOG_INFO

/*
 * Log level for low-level debug tracing.
 */
#define LOGLEVEL_DEBUG 		ISC_LOG_DEBUG(8)

/*
 * Check an operation for failure.  These macros all assume that
 * the function using them has a 'result' variable and a 'failure'
 * label.
 */
#define CHECK(op) \
	do { result = (op); 				  	 \
	       if (result != ISC_R_SUCCESS) goto failure; 	 \
	} while (0)

/*
 * Fail unconditionally with result 'code', which must not
 * be ISC_R_SUCCESS.  The reason for failure presumably has
 * been logged already.
 *
 * The test against ISC_R_SUCCESS is there to keep the Solaris compiler
 * from complaining about "end-of-loop code not reached".
 */

#define FAIL(code) \
	do {							\
		result = (code);				\
		if (result != ISC_R_SUCCESS) goto failure;	\
	} while (0)

/*
 * Fail unconditionally and log as a client error.
 * The test against ISC_R_SUCCESS is there to keep the Solaris compiler
 * from complaining about "end-of-loop code not reached".
 */
#define FAILC(code, msg) \
	do {							\
		result = (code);				\
		update_log(client, zone, LOGLEVEL_PROTOCOL,   	\
			      "update failed: %s (%s)",		\
		      	      msg, isc_result_totext(result));	\
		if (result != ISC_R_SUCCESS) goto failure;	\
	} while (0)

/*
 * Fail unconditionally and log as a server error.
 * The test against ISC_R_SUCCESS is there to keep the Solaris compiler
 * from complaining about "end-of-loop code not reached".
 */
#define FAILS(code, msg) \
	do {							\
		result = (code);				\
		update_log(client, zone, LOGLEVEL_PROTOCOL,		\
			      "error: %s: %s", 			\
			      msg, isc_result_totext(result));	\
		if (result != ISC_R_SUCCESS) goto failure;	\
	} while (0)

/**************************************************************************/

typedef struct rr rr_t;

struct rr {
	/* dns_name_t name; */
	isc_uint32_t 		ttl;
	dns_rdata_t 		rdata;
};

typedef struct update_event update_event_t;

struct update_event {
	ISC_EVENT_COMMON(update_event_t);
	dns_zone_t 		*zone;
	isc_result_t		result;
	dns_message_t		*answer;
};

/**************************************************************************/
/*
 * Forward declarations.
 */

static void update_action(isc_task_t *task, isc_event_t *event);
static void updatedone_action(isc_task_t *task, isc_event_t *event);
static isc_result_t send_forward_event(ns_client_t *client, dns_zone_t *zone);
static void forward_done(isc_task_t *task, isc_event_t *event);

/**************************************************************************/

static void
update_log(ns_client_t *client, dns_zone_t *zone,
	   int level, const char *fmt, ...) ISC_FORMAT_PRINTF(4, 5);

static void
update_log(ns_client_t *client, dns_zone_t *zone,
	   int level, const char *fmt, ...)
{
	va_list ap;
	char message[4096];
	char namebuf[DNS_NAME_FORMATSIZE];
	char classbuf[DNS_RDATACLASS_FORMATSIZE];

	if (client == NULL || zone == NULL)
		return;

	if (isc_log_wouldlog(ns_g_lctx, level) == ISC_FALSE)
		return;

	dns_name_format(dns_zone_getorigin(zone), namebuf,
			sizeof(namebuf));
	dns_rdataclass_format(dns_zone_getclass(zone), classbuf,
			      sizeof(classbuf));

	va_start(ap, fmt);
	vsnprintf(message, sizeof message, fmt, ap);
	va_end(ap);

	ns_client_log(client, NS_LOGCATEGORY_UPDATE, NS_LOGMODULE_UPDATE,
		      level, "updating zone '%s/%s': %s",
		      namebuf, classbuf, message);
}

/*
 * Update a single RR in version 'ver' of 'db' and log the
 * update in 'diff'.
 *
 * Ensures:
 *   '*tuple' == NULL.  Either the tuple is freed, or its
 *         ownership has been transferred to the diff.
 */
static isc_result_t
do_one_tuple(dns_difftuple_t **tuple,
	     dns_db_t *db, dns_dbversion_t *ver,
	     dns_diff_t *diff)
{
	dns_diff_t temp_diff;
	isc_result_t result;

	/*
	 * Create a singleton diff.
	 */
	dns_diff_init(diff->mctx, &temp_diff);
	ISC_LIST_APPEND(temp_diff.tuples, *tuple, link);

	/*
	 * Apply it to the database.
	 */
	result = dns_diff_apply(&temp_diff, db, ver);
	ISC_LIST_UNLINK(temp_diff.tuples, *tuple, link);
	if (result != ISC_R_SUCCESS) {
		dns_difftuple_free(tuple);
		return (result);
	}

	/*
	 * Merge it into the current pending journal entry.
	 */
	dns_diff_appendminimal(diff, tuple);

	/*
	 * Do not clear temp_diff.
	 */
	return (ISC_R_SUCCESS);
}

/*
 * Perform the updates in 'updates' in version 'ver' of 'db' and log the
 * update in 'diff'.
 *
 * Ensures:
 *   'updates' is empty.
 */
static isc_result_t
do_diff(dns_diff_t *updates, dns_db_t *db, dns_dbversion_t *ver,
	dns_diff_t *diff)
{
	isc_result_t result;
	while (! ISC_LIST_EMPTY(updates->tuples)) {
		dns_difftuple_t *t = ISC_LIST_HEAD(updates->tuples);
		ISC_LIST_UNLINK(updates->tuples, t, link);
		CHECK(do_one_tuple(&t, db, ver, diff));
	}
	return (ISC_R_SUCCESS);

 failure:
	dns_diff_clear(diff);
	return (result);
}

static isc_result_t
update_one_rr(dns_db_t *db, dns_dbversion_t *ver, dns_diff_t *diff,
	      dns_diffop_t op, dns_name_t *name,
	      dns_ttl_t ttl, dns_rdata_t *rdata)
{
	dns_difftuple_t *tuple = NULL;
	isc_result_t result;
	result = dns_difftuple_create(diff->mctx, op,
				      name, ttl, rdata, &tuple);
	if (result != ISC_R_SUCCESS)
		return (result);
	return (do_one_tuple(&tuple, db, ver, diff));
}

/**************************************************************************/
/*
 * Callback-style iteration over rdatasets and rdatas.
 *
 * foreach_rrset() can be used to iterate over the RRsets
 * of a name and call a callback function with each
 * one.  Similarly, foreach_rr() can be used to iterate
 * over the individual RRs at name, optionally restricted
 * to RRs of a given type.
 *
 * The callback functions are called "actions" and take
 * two arguments: a void pointer for passing arbitrary
 * context information, and a pointer to the current RRset
 * or RR.  By convention, their names end in "_action".
 */

/*
 * XXXRTH  We might want to make this public somewhere in libdns.
 */

/*
 * Function type for foreach_rrset() iterator actions.
 */
typedef isc_result_t rrset_func(void *data, dns_rdataset_t *rrset);

/*
 * Function type for foreach_rr() iterator actions.
 */
typedef isc_result_t rr_func(void *data, rr_t *rr);

/*
 * Internal context struct for foreach_node_rr().
 */
typedef struct {
	rr_func *	rr_action;
	void *		rr_action_data;
} foreach_node_rr_ctx_t;

/*
 * Internal helper function for foreach_node_rr().
 */
static isc_result_t
foreach_node_rr_action(void *data, dns_rdataset_t *rdataset) {
	isc_result_t result;
	foreach_node_rr_ctx_t *ctx = data;
	for (result = dns_rdataset_first(rdataset);
	     result == ISC_R_SUCCESS;
	     result = dns_rdataset_next(rdataset))
	{
		rr_t rr = { 0, DNS_RDATA_INIT };
		
		dns_rdataset_current(rdataset, &rr.rdata);
		rr.ttl = rdataset->ttl;
		result = (*ctx->rr_action)(ctx->rr_action_data, &rr);
		if (result != ISC_R_SUCCESS)
			return (result);
	}
	if (result != ISC_R_NOMORE)
		return (result);
	return (ISC_R_SUCCESS);
}

/*
 * For each rdataset of 'name' in 'ver' of 'db', call 'action'
 * with the rdataset and 'action_data' as arguments.  If the name
 * does not exist, do nothing.
 *
 * If 'action' returns an error, abort iteration and return the error.
 */
static isc_result_t
foreach_rrset(dns_db_t *db,
	      dns_dbversion_t *ver,
	      dns_name_t *name,
	      rrset_func *action,
	      void *action_data)
{
	isc_result_t result;
	dns_dbnode_t *node;
	dns_rdatasetiter_t *iter;

	node = NULL;
	result = dns_db_findnode(db, name, ISC_FALSE, &node);
	if (result == ISC_R_NOTFOUND)
		return (ISC_R_SUCCESS);
	if (result != ISC_R_SUCCESS)
		return (result);

	iter = NULL;
	result = dns_db_allrdatasets(db, node, ver,
				     (isc_stdtime_t) 0, &iter);
	if (result != ISC_R_SUCCESS)
		goto cleanup_node;

	for (result = dns_rdatasetiter_first(iter);
	     result == ISC_R_SUCCESS;
	     result = dns_rdatasetiter_next(iter))
	{
		dns_rdataset_t rdataset;

		dns_rdataset_init(&rdataset);
		dns_rdatasetiter_current(iter, &rdataset);

		result = (*action)(action_data, &rdataset);

		dns_rdataset_disassociate(&rdataset);
		if (result != ISC_R_SUCCESS)
			goto cleanup_iterator;
	}
	if (result == ISC_R_NOMORE)
		result = ISC_R_SUCCESS;

 cleanup_iterator:
	dns_rdatasetiter_destroy(&iter);

 cleanup_node:
	dns_db_detachnode(db, &node);

	return (result);
}

/*
 * For each RR of 'name' in 'ver' of 'db', call 'action'
 * with the RR and 'action_data' as arguments.  If the name
 * does not exist, do nothing.
 *
 * If 'action' returns an error, abort iteration
 * and return the error.
 */
static isc_result_t
foreach_node_rr(dns_db_t *db,
	    dns_dbversion_t *ver,
	    dns_name_t *name,
	    rr_func *rr_action,
	    void *rr_action_data)
{
	foreach_node_rr_ctx_t ctx;
	ctx.rr_action = rr_action;
	ctx.rr_action_data = rr_action_data;
	return (foreach_rrset(db, ver, name,
			      foreach_node_rr_action, &ctx));
}


/*
 * For each of the RRs specified by 'db', 'ver', 'name', 'type',
 * (which can be dns_rdatatype_any to match any type), and 'covers', call
 * 'action' with the RR and 'action_data' as arguments. If the name
 * does not exist, or if no RRset of the given type exists at the name,
 * do nothing.
 *
 * If 'action' returns an error, abort iteration and return the error.
 */
static isc_result_t
foreach_rr(dns_db_t *db,
	   dns_dbversion_t *ver,
	   dns_name_t *name,
	   dns_rdatatype_t type,
	   dns_rdatatype_t covers,
	   rr_func *rr_action,
	   void *rr_action_data)
{

	isc_result_t result;
	dns_dbnode_t *node;
	dns_rdataset_t rdataset;

	if (type == dns_rdatatype_any)
		return (foreach_node_rr(db, ver, name,
					rr_action, rr_action_data));

	node = NULL;
	result = dns_db_findnode(db, name, ISC_FALSE, &node);
	if (result == ISC_R_NOTFOUND)
		return (ISC_R_SUCCESS);
	if (result != ISC_R_SUCCESS)
		return (result);

	dns_rdataset_init(&rdataset);
	result = dns_db_findrdataset(db, node, ver, type, covers,
				     (isc_stdtime_t) 0, &rdataset, NULL);
	if (result == ISC_R_NOTFOUND) {
		result = ISC_R_SUCCESS;
		goto cleanup_node;
	}
	if (result != ISC_R_SUCCESS)
		goto cleanup_node;

	for (result = dns_rdataset_first(&rdataset);
	     result == ISC_R_SUCCESS;
	     result = dns_rdataset_next(&rdataset))
	{
		rr_t rr = { 0, DNS_RDATA_INIT };
		dns_rdataset_current(&rdataset, &rr.rdata);
		rr.ttl = rdataset.ttl;
		result = (*rr_action)(rr_action_data, &rr);
		if (result != ISC_R_SUCCESS)
			goto cleanup_rdataset;
	}
	if (result != ISC_R_NOMORE)
		goto cleanup_rdataset;
	result = ISC_R_SUCCESS;

 cleanup_rdataset:
	dns_rdataset_disassociate(&rdataset);
 cleanup_node:
	dns_db_detachnode(db, &node);

	return (result);
}

/**************************************************************************/
/*
 * Various tests on the database contents (for prerequisites, etc).
 */

/*
 * Function type for predicate functions that compare a database RR 'db_rr'
 * against an update RR 'update_rr'.
 */
typedef isc_boolean_t rr_predicate(dns_rdata_t *update_rr, dns_rdata_t *db_rr);

/*
 * Helper function for rrset_exists().
 */
static isc_result_t
rrset_exists_action(void *data, rr_t *rr) {
	UNUSED(data);
	UNUSED(rr);
	return (ISC_R_EXISTS);
}

/*
 * Utility macro for RR existence checking functions.
 *
 * If the variable 'result' has the value ISC_R_EXISTS or
 * ISC_R_SUCCESS, set *exists to ISC_TRUE or ISC_FALSE,
 * respectively, and return success.
 *
 * If 'result' has any other value, there was a failure.
 * Return the failure result code and do not set *exists.
 *
 * This would be more readable as "do { if ... } while(0)",
 * but that form generates tons of warnings on Solaris 2.6.
 */
#define RETURN_EXISTENCE_FLAG 				\
	return ((result == ISC_R_EXISTS) ? 		\
		(*exists = ISC_TRUE, ISC_R_SUCCESS) : 	\
		((result == ISC_R_SUCCESS) ?		\
		 (*exists = ISC_FALSE, ISC_R_SUCCESS) :	\
		 result))

/*
 * Set '*exists' to true iff an rrset of the given type exists,
 * to false otherwise.
 */
static isc_result_t
rrset_exists(dns_db_t *db, dns_dbversion_t *ver,
	     dns_name_t *name, dns_rdatatype_t type, dns_rdatatype_t covers,
	     isc_boolean_t *exists)
{
	isc_result_t result;
	result = foreach_rr(db, ver, name, type, covers,
			    rrset_exists_action, NULL);
	RETURN_EXISTENCE_FLAG;
}

/*
 * Helper function for cname_incompatible_rrset_exists.
 */
static isc_result_t
cname_compatibility_action(void *data, dns_rdataset_t *rrset) {
	UNUSED(data);
	if (rrset->type != dns_rdatatype_cname &&
	    ! dns_rdatatype_isdnssec(rrset->type))
		return (ISC_R_EXISTS);
	return (ISC_R_SUCCESS);
}

/*
 * Check whether there is an rrset incompatible with adding a CNAME RR,
 * i.e., anything but another CNAME (which can be replaced) or a
 * DNSSEC RR (which can coexist).
 *
 * If such an incompatible rrset exists, set '*exists' to ISC_TRUE.
 * Otherwise, set it to ISC_FALSE.
 */
static isc_result_t
cname_incompatible_rrset_exists(dns_db_t *db, dns_dbversion_t *ver,
				dns_name_t *name, isc_boolean_t *exists) {
	isc_result_t result;
	result = foreach_rrset(db, ver, name,
			       cname_compatibility_action, NULL);
	RETURN_EXISTENCE_FLAG;
}

/*
 * Helper function for rr_count().
 */
static isc_result_t
count_rr_action(void *data, rr_t *rr) {
	int *countp = data;
	UNUSED(rr);
	(*countp)++;
	return (ISC_R_SUCCESS);
}

/*
 * Count the number of RRs of 'type' belonging to 'name' in 'ver' of 'db'.
 */
static isc_result_t
rr_count(dns_db_t *db, dns_dbversion_t *ver, dns_name_t *name,
	 dns_rdatatype_t type, dns_rdatatype_t covers, int *countp)
{
	*countp = 0;
	return (foreach_rr(db, ver, name, type, covers,
			   count_rr_action, countp));
}

/*
 * Context struct and helper function for name_exists().
 */

static isc_result_t
name_exists_action(void *data, dns_rdataset_t *rrset) {
	UNUSED(data);
	UNUSED(rrset);
	return (ISC_R_EXISTS);
}

/*
 * Set '*exists' to true iff the given name exists, to false otherwise.
 */
static isc_result_t
name_exists(dns_db_t *db, dns_dbversion_t *ver, dns_name_t *name,
	    isc_boolean_t *exists)
{
	isc_result_t result;
	result = foreach_rrset(db, ver, name,
			       name_exists_action, NULL);
	RETURN_EXISTENCE_FLAG;
}

typedef struct {
	dns_name_t *name, *signer;
	dns_ssutable_t *table;
} ssu_check_t;

static isc_result_t
ssu_checkrule(void *data, dns_rdataset_t *rrset) {
	ssu_check_t *ssuinfo = data;
	isc_boolean_t result;

	/*
	 * If we're deleting all records, it's ok to delete SIG and NXT even
	 * if we're normally not allowed to.
	 */
	if (rrset->type == dns_rdatatype_sig ||
	    rrset->type == dns_rdatatype_nxt)
		return (ISC_TRUE);
	result = dns_ssutable_checkrules(ssuinfo->table, ssuinfo->signer,
					 ssuinfo->name, rrset->type);
	return (result == ISC_TRUE ? ISC_R_SUCCESS : ISC_R_FAILURE);
}

static isc_boolean_t
ssu_checkall(dns_db_t *db, dns_dbversion_t *ver, dns_name_t *name,
	     dns_ssutable_t *ssutable, dns_name_t *signer)
{
	isc_result_t result;
	ssu_check_t ssuinfo;

	ssuinfo.name = name;
	ssuinfo.table = ssutable;
	ssuinfo.signer = signer;
	result = foreach_rrset(db, ver, name, ssu_checkrule, &ssuinfo);
	return (ISC_TF(result == ISC_R_SUCCESS));
}

/**************************************************************************/
/*
 * Checking of "RRset exists (value dependent)" prerequisites.
 *
 * In the RFC2136 section 3.2.5, this is the pseudocode involving
 * a variable called "temp", a mapping of <name, type> tuples to rrsets.
 *
 * Here, we represent the "temp" data structure as (non-minimial) "dns_diff_t"
 * where each typle has op==DNS_DIFFOP_EXISTS.
 */


/*
 * Append a tuple asserting the existence of the RR with
 * 'name' and 'rdata' to 'diff'.
 */
static isc_result_t
temp_append(dns_diff_t *diff, dns_name_t *name, dns_rdata_t *rdata) {
	isc_result_t result;
	dns_difftuple_t *tuple = NULL;

	REQUIRE(DNS_DIFF_VALID(diff));
	CHECK(dns_difftuple_create(diff->mctx, DNS_DIFFOP_EXISTS,
				      name, 0, rdata, &tuple));
	ISC_LIST_APPEND(diff->tuples, tuple, link);
 failure:
	return (result);
}

/*
 * Compare two rdatasets represented as sorted lists of tuples.
 * All list elements must have the same owner name and type.
 * Return ISC_R_SUCCESS if the rdatasets are equal, rcode(dns_rcode_nxrrset)
 * if not.
 */
static isc_result_t
temp_check_rrset(dns_difftuple_t *a, dns_difftuple_t *b) {
	for (;;) {
		if (a == NULL || b == NULL)
			break;
		INSIST(a->op == DNS_DIFFOP_EXISTS &&
		       b->op == DNS_DIFFOP_EXISTS);
		INSIST(a->rdata.type == b->rdata.type);
		INSIST(dns_name_equal(&a->name, &b->name));
		if (dns_rdata_compare(&a->rdata, &b->rdata) != 0)
			return (DNS_R_NXRRSET);
		a = ISC_LIST_NEXT(a, link);
		b = ISC_LIST_NEXT(b, link);
	}
	if (a != NULL || b != NULL)
		return (DNS_R_NXRRSET);
	return (ISC_R_SUCCESS);
}

/*
 * A comparison function defining the sorting order for the entries
 * in the "temp" data structure.  The major sort key is the owner name,
 * followed by the type and rdata.
 */
static int
temp_order(const void *av, const void *bv) {
	dns_difftuple_t const * const *ap = av;
	dns_difftuple_t const * const *bp = bv;
	dns_difftuple_t const *a = *ap;
	dns_difftuple_t const *b = *bp;
	int r;
	r = dns_name_compare(&a->name, &b->name);
	if (r != 0)
		return (r);
	r = (b->rdata.type - a->rdata.type);
	if (r != 0)
		return (r);
	r = dns_rdata_compare(&a->rdata, &b->rdata);
	return (r);
}

/*
 * Check the "RRset exists (value dependent)" prerequisite information
 * in 'temp' against the contents of the database 'db'.
 *
 * Return ISC_R_SUCCESS if the prerequisites are satisfied,
 * rcode(dns_rcode_nxrrset) if not.
 */

static isc_result_t
temp_check(isc_mem_t *mctx, dns_diff_t *temp, dns_db_t *db,
	   dns_dbversion_t *ver)
{
	isc_result_t result;
	dns_name_t *name;
	dns_dbnode_t *node;
	dns_difftuple_t *t;
	dns_diff_t trash;

	/* Exit early if the list is empty (for efficiency only). */
	if (ISC_LIST_HEAD(temp->tuples) == NULL)
		return (ISC_R_SUCCESS);

	/*
	 * Sort the prerequisite records by owner name,
	 * type, and rdata.
	 */
	result = dns_diff_sort(temp, temp_order);
	if (result != ISC_R_SUCCESS)
		return (result);

	dns_diff_init(mctx, &trash);

	/*
	 * For each name and type in the prerequisites,
	 * construct a sorted rdata list of the corresponding
	 * database contents, and compare the lists.
	 */
	t = ISC_LIST_HEAD(temp->tuples);
	while (t != NULL) {
		name = &t->name;

		/* A new unique name begins here. */
		node = NULL;
		result = dns_db_findnode(db, name, ISC_FALSE, &node);
		if (result == ISC_R_NOTFOUND)
			return (DNS_R_NXRRSET);
		if (result != ISC_R_SUCCESS)
			return (result);

		/* A new unique type begins here. */
		while (t != NULL && dns_name_equal(&t->name, name)) {
			dns_rdatatype_t type, covers;
			dns_rdataset_t rdataset;
			dns_diff_t d_rrs; /* Database RRs with
						this name and type */
 			dns_diff_t u_rrs; /* Update RRs with
						this name and type */

			type = t->rdata.type;
			if (type == dns_rdatatype_sig)
				covers = dns_rdata_covers(&t->rdata);
			else
				covers = 0;

			/*
			 * Collect all database RRs for this name and type
			 * onto d_rrs and sort them.
			 */
			dns_rdataset_init(&rdataset);
			result = dns_db_findrdataset(db, node, ver, type,
						     covers, (isc_stdtime_t) 0,
						     &rdataset, NULL);
			if (result != ISC_R_SUCCESS) {
				dns_db_detachnode(db, &node);
				return (DNS_R_NXRRSET);
			}

			dns_diff_init(mctx, &d_rrs);
			dns_diff_init(mctx, &u_rrs);

			for (result = dns_rdataset_first(&rdataset);
			     result == ISC_R_SUCCESS;
			     result = dns_rdataset_next(&rdataset))
			{
				dns_rdata_t rdata = DNS_RDATA_INIT;
				dns_rdataset_current(&rdataset, &rdata);
				result = temp_append(&d_rrs, name, &rdata);
				if (result != ISC_R_SUCCESS)
					goto failure;
			}
			if (result != ISC_R_NOMORE)
				goto failure;
			result = dns_diff_sort(&d_rrs, temp_order);
			if (result != ISC_R_SUCCESS)
				goto failure;

			/*
			 * Collect all update RRs for this name and type
			 * onto u_rrs.  No need to sort them here -
			 * they are already sorted.
			 */
			while (t != NULL &&
			       dns_name_equal(&t->name, name) &&
			       t->rdata.type == type)
			{
				dns_difftuple_t *next =
					ISC_LIST_NEXT(t, link);
				ISC_LIST_UNLINK(temp->tuples, t, link);
				ISC_LIST_APPEND(u_rrs.tuples, t, link);
				t = next;
			}

			/* Compare the two sorted lists. */
			result = temp_check_rrset(ISC_LIST_HEAD(u_rrs.tuples),
						  ISC_LIST_HEAD(d_rrs.tuples));
			if (result != ISC_R_SUCCESS)
				goto failure;

			/*
			 * We are done with the tuples, but we can't free
			 * them yet because "name" still points into one
			 * of them.  Move them on a temporary list.
			 */
			ISC_LIST_APPENDLIST(trash.tuples, u_rrs.tuples, link);
			ISC_LIST_APPENDLIST(trash.tuples, d_rrs.tuples, link);
			dns_rdataset_disassociate(&rdataset);

			continue;

		    failure:
			dns_diff_clear(&d_rrs);
			dns_diff_clear(&u_rrs);
			dns_diff_clear(&trash);
			dns_rdataset_disassociate(&rdataset);
			dns_db_detachnode(db, &node);
			return (result);
		}

		dns_db_detachnode(db, &node);
	}

	dns_diff_clear(&trash);
	return (ISC_R_SUCCESS);
}

/**************************************************************************/
/*
 * Conditional deletion of RRs.
 */

/*
 * Context structure for delete_if().
 */

typedef struct {
	rr_predicate *predicate;
	dns_db_t *db;
	dns_dbversion_t *ver;
	dns_diff_t *diff;
	dns_name_t *name;
	dns_rdata_t *update_rr;
} conditional_delete_ctx_t;

/*
 * Predicate functions for delete_if().
 */

/*
 * Return true iff 'update_rr' is neither a SOA nor an NS RR.
 */
static isc_boolean_t
type_not_soa_nor_ns_p(dns_rdata_t *update_rr, dns_rdata_t *db_rr) {
	UNUSED(update_rr);
	return ((db_rr->type != dns_rdatatype_soa &&
		 db_rr->type != dns_rdatatype_ns) ?
		ISC_TRUE : ISC_FALSE);
}

/*
 * Return true always.
 */
static isc_boolean_t
true_p(dns_rdata_t *update_rr, dns_rdata_t *db_rr) {
	UNUSED(update_rr);
	UNUSED(db_rr);
	return (ISC_TRUE);
}

/*
 * Return true iff the two RRs have identical rdata.
 */
static isc_boolean_t
rr_equal_p(dns_rdata_t *update_rr, dns_rdata_t *db_rr) {
	/*
	 * XXXRTH  This is not a problem, but we should consider creating
	 *         dns_rdata_equal() (that used dns_name_equal()), since it
	 *         would be faster.  Not a priority.
	 */
	return (dns_rdata_compare(update_rr, db_rr) == 0 ?
		ISC_TRUE : ISC_FALSE);
}

/*
 * Return true iff 'update_rr' should replace 'db_rr' according
 * to the special RFC2136 rules for CNAME, SOA, and WKS records.
 *
 * RFC2136 does not mention NXT or DNAME, but multiple NXTs or DNAMEs
 * make little sense, so we replace those, too.
 */
static isc_boolean_t
replaces_p(dns_rdata_t *update_rr, dns_rdata_t *db_rr) {
	if (db_rr->type != update_rr->type)
		return (ISC_FALSE);
	if (db_rr->type == dns_rdatatype_cname)
		return (ISC_TRUE);
	if (db_rr->type == dns_rdatatype_dname)
		return (ISC_TRUE);
	if (db_rr->type == dns_rdatatype_soa)
		return (ISC_TRUE);
	if (db_rr->type == dns_rdatatype_nxt)
		return (ISC_TRUE);
	if (db_rr->type == dns_rdatatype_wks) {
		/*
		 * Compare the address and protocol fields only.  These
		 * form the first five bytes of the RR data.  Do a
		 * raw binary comparison; unpacking the WKS RRs using
		 * dns_rdata_tostruct() might be cleaner in some ways,
		 * but it would require us to pass around an mctx.
		 */
		INSIST(db_rr->length >= 5 && update_rr->length >= 5);
		return (memcmp(db_rr->data, update_rr->data, 5) == 0 ?
			ISC_TRUE : ISC_FALSE);
	}
	return (ISC_FALSE);
}

/*
 * Internal helper function for delete_if().
 */
static isc_result_t
delete_if_action(void *data, rr_t *rr) {
	conditional_delete_ctx_t *ctx = data;
	if ((*ctx->predicate)(ctx->update_rr, &rr->rdata)) {
		isc_result_t result;
		result = update_one_rr(ctx->db, ctx->ver, ctx->diff,
				       DNS_DIFFOP_DEL, ctx->name,
				       rr->ttl, &rr->rdata);
		return (result);
	} else {
		return (ISC_R_SUCCESS);
	}
}

/*
 * Conditionally delete RRs.  Apply 'predicate' to the RRs
 * specified by 'db', 'ver', 'name', and 'type' (which can
 * be dns_rdatatype_any to match any type).  Delete those
 * RRs for which the predicate returns true, and log the
 * deletions in 'diff'.
 */
static isc_result_t
delete_if(rr_predicate *predicate,
	  dns_db_t *db,
	  dns_dbversion_t *ver,
	  dns_name_t *name,
	  dns_rdatatype_t type,
	  dns_rdatatype_t covers,
	  dns_rdata_t *update_rr,
	  dns_diff_t *diff)
{
	conditional_delete_ctx_t ctx;
	ctx.predicate = predicate;
	ctx.db = db;
	ctx.ver = ver;
	ctx.diff = diff;
	ctx.name = name;
	ctx.update_rr = update_rr;
	return (foreach_rr(db, ver, name, type, covers,
			   delete_if_action, &ctx));
}

/**************************************************************************/
/*
 * Prepare an RR for the addition of the new RR 'ctx->update_rr',
 * with TTL 'ctx->update_rr_ttl', to its rdataset, by deleting
 * the RRs if it is replaced by the new RR or has a conflicting TTL.
 * The necessary changes are appended to ctx->del_diff and ctx->add_diff;
 * we need to do all deletions before any additions so that we don't run
 * into transient states with conflicting TTLs.
 */

typedef struct {
	dns_db_t *db;
	dns_dbversion_t *ver;
	dns_diff_t *diff;
	dns_name_t *name;
	dns_rdata_t *update_rr;
	dns_ttl_t update_rr_ttl;
	isc_boolean_t ignore_add;
	dns_diff_t del_diff;
	dns_diff_t add_diff;
} add_rr_prepare_ctx_t;

static isc_result_t
add_rr_prepare_action(void *data, rr_t *rr) {
	isc_result_t result = ISC_R_SUCCESS;	
	add_rr_prepare_ctx_t *ctx = data;
	dns_difftuple_t *tuple = NULL;

	/*
	 * If the update RR is a "duplicate" of the update RR,
	 * the update should be silently ignored.
	 */
	if (dns_rdata_compare(&rr->rdata, ctx->update_rr) == 0 &&
	    rr->ttl == ctx->update_rr_ttl) {
		ctx->ignore_add = ISC_TRUE;
	}

	/*
	 * If this RR is "equal" to the update RR, it should
	 * be deleted before the update RR is added.
	 */
	if (replaces_p(ctx->update_rr, &rr->rdata)) {
		CHECK(dns_difftuple_create(ctx->del_diff.mctx,
					   DNS_DIFFOP_DEL, ctx->name,
					   rr->ttl,
					   &rr->rdata,
					   &tuple));
		dns_diff_append(&ctx->del_diff, &tuple);
		return (ISC_R_SUCCESS);
	}

	/*
	 * If this RR differs in TTL from the update RR,
	 * its TTL must be adjusted.
	 */
	if (rr->ttl != ctx->update_rr_ttl) {
		CHECK(dns_difftuple_create(ctx->del_diff.mctx,
					   DNS_DIFFOP_DEL, ctx->name,
					   rr->ttl,
					   &rr->rdata,
					   &tuple));
		dns_diff_append(&ctx->del_diff, &tuple);
		CHECK(dns_difftuple_create(ctx->add_diff.mctx,
					   DNS_DIFFOP_ADD, ctx->name,
					   ctx->update_rr_ttl,
					   &rr->rdata,
					   &tuple));
		dns_diff_append(&ctx->add_diff, &tuple);
	}
 failure:
	return (result);
}

/**************************************************************************/
/*
 * Miscellaneous subroutines.
 */

/*
 * Extract a single update RR from 'section' of dynamic update message
 * 'msg', with consistency checking.
 *
 * Stores the owner name, rdata, and TTL of the update RR at 'name',
 * 'rdata', and 'ttl', respectively.
 */
static void
get_current_rr(dns_message_t *msg, dns_section_t section,
	       dns_rdataclass_t zoneclass,
	       dns_name_t **name, dns_rdata_t *rdata, dns_rdatatype_t *covers,
	       dns_ttl_t *ttl,
	       dns_rdataclass_t *update_class)
{
	dns_rdataset_t *rdataset;
	isc_result_t result;
	dns_message_currentname(msg, section, name);
	rdataset = ISC_LIST_HEAD((*name)->list);
	INSIST(rdataset != NULL);
	INSIST(ISC_LIST_NEXT(rdataset, link) == NULL);
	*covers = rdataset->covers;
	*ttl = rdataset->ttl;
	result = dns_rdataset_first(rdataset);
	INSIST(result == ISC_R_SUCCESS);
	dns_rdataset_current(rdataset, rdata);
	INSIST(dns_rdataset_next(rdataset) == ISC_R_NOMORE);
	*update_class = rdata->rdclass;
	rdata->rdclass = zoneclass;
}

/*
 * Increment the SOA serial number of database 'db', version 'ver'.
 * Replace the SOA record in the database, and log the
 * change in 'diff'.
 */

	/*
	 * XXXRTH  Failures in this routine will be worth logging, when
	 *         we have a logging system.  Failure to find the zonename
	 *	   or the SOA rdataset warrant at least an UNEXPECTED_ERROR().
	 */

static isc_result_t
increment_soa_serial(dns_db_t *db, dns_dbversion_t *ver,
		     dns_diff_t *diff, isc_mem_t *mctx)
{
	dns_difftuple_t *deltuple = NULL;
	dns_difftuple_t *addtuple = NULL;
	isc_uint32_t serial;
	isc_result_t result;

	CHECK(dns_db_createsoatuple(db, ver, mctx, DNS_DIFFOP_DEL, &deltuple));
	CHECK(dns_difftuple_copy(deltuple, &addtuple));
	addtuple->op = DNS_DIFFOP_ADD;

	serial = dns_soa_getserial(&addtuple->rdata);

	/* RFC1982 */
	serial = (serial + 1) & 0xFFFFFFFF;
	if (serial == 0)
		serial = 1;

	dns_soa_setserial(serial, &addtuple->rdata);
	CHECK(do_one_tuple(&deltuple, db, ver, diff));
	CHECK(do_one_tuple(&addtuple, db, ver, diff));
	result = ISC_R_SUCCESS;

 failure:
	if (addtuple != NULL)
		dns_difftuple_free(&addtuple);
	if (deltuple != NULL)
		dns_difftuple_free(&deltuple);
	return (result);
}

/*
 * Check that the new SOA record at 'update_rdata' does not
 * illegally cause the SOA serial number to decrease or stay
 * unchanged relative to the existing SOA in 'db'.
 *
 * Sets '*ok' to ISC_TRUE if the update is legal, ISC_FALSE if not.
 *
 * William King points out that RFC2136 is inconsistent about
 * the case where the serial number stays unchanged:
 *
 *   section 3.4.2.2 requires a server to ignore a SOA update request
 *   if the serial number on the update SOA is less_than_or_equal to
 *   the zone SOA serial.
 *
 *   section 3.6 requires a server to ignore a SOA update request if
 *   the serial is less_than the zone SOA serial.
 *
 * Paul says 3.4.2.2 is correct.
 *
 */
static isc_result_t
check_soa_increment(dns_db_t *db, dns_dbversion_t *ver,
		    dns_rdata_t *update_rdata,
		    isc_boolean_t *ok)
{
	isc_uint32_t db_serial;
	isc_uint32_t update_serial;
	isc_result_t result;

	update_serial = dns_soa_getserial(update_rdata);

	result = dns_db_getsoaserial(db, ver, &db_serial);
	if (result != ISC_R_SUCCESS)
		return (result);

	if (DNS_SERIAL_GE(db_serial, update_serial)) {
		*ok = ISC_FALSE;
	} else {
		*ok = ISC_TRUE;
	}

	return (ISC_R_SUCCESS);

}

/**************************************************************************/
/*
 * Incremental updating of NXTs and SIGs.
 */

#define MAXZONEKEYS 32	/* Maximum number of zone keys supported. */

/*
 * We abuse the dns_diff_t type to represent a set of domain names
 * affected by the update.
 */
static isc_result_t
namelist_append_name(dns_diff_t *list, dns_name_t *name) {
	isc_result_t result;
	dns_difftuple_t *tuple = NULL;
	static dns_rdata_t dummy_rdata = { NULL, 0, 0, 0, 0,
					   { (void*)(-1), (void*)(-1) } };
	CHECK(dns_difftuple_create(list->mctx, DNS_DIFFOP_EXISTS, name, 0,
				   &dummy_rdata, &tuple));
	dns_diff_append(list, &tuple);
 failure:
	return (result);
}

static isc_result_t
namelist_append_subdomain(dns_db_t *db, dns_name_t *name, dns_diff_t *affected)
{
	isc_result_t result;
	dns_fixedname_t fixedname;
	dns_name_t *child;
	dns_dbiterator_t *dbit = NULL;

	dns_fixedname_init(&fixedname);
	child = dns_fixedname_name(&fixedname);

	CHECK(dns_db_createiterator(db, ISC_FALSE, &dbit));

	for (result = dns_dbiterator_seek(dbit, name);
	     result == ISC_R_SUCCESS;
	     result = dns_dbiterator_next(dbit))
	{
		dns_dbnode_t *node = NULL;
		result = dns_dbiterator_current(dbit, &node, child);
		dns_db_detachnode(db, &node);
		CHECK(result);
		if (! dns_name_issubdomain(child, name))
			break;
		CHECK(namelist_append_name(affected, child));
	}
 failure:
	if (dbit != NULL)
		dns_dbiterator_destroy(&dbit);
	return (result);
}



/*
 * Helper function for non_nxt_rrset_exists().
 */
static isc_result_t
is_non_nxt_action(void *data, dns_rdataset_t *rrset) {
	UNUSED(data);
	if (!(rrset->type == dns_rdatatype_nxt ||
	      (rrset->type == dns_rdatatype_sig &&
	       rrset->covers == dns_rdatatype_nxt)))
		return (ISC_R_EXISTS);
	return (ISC_R_SUCCESS);
}

/*
 * Check whether there is an rrset other than a NXT or SIG NXT,
 * i.e., anything that justifies the continued existence of a name
 * after a secure update.
 *
 * If such an rrset exists, set '*exists' to ISC_TRUE.
 * Otherwise, set it to ISC_FALSE.
 */
static isc_result_t
non_nxt_rrset_exists(dns_db_t *db, dns_dbversion_t *ver,
		     dns_name_t *name, isc_boolean_t *exists)
{
	isc_result_t result;
	result = foreach_rrset(db, ver, name,
			       is_non_nxt_action, NULL);
	RETURN_EXISTENCE_FLAG;
}

/*
 * A comparison function for sorting dns_diff_t:s by name.
 */
static int
name_order(const void *av, const void *bv) {
	dns_difftuple_t const * const *ap = av;
	dns_difftuple_t const * const *bp = bv;
	dns_difftuple_t const *a = *ap;
	dns_difftuple_t const *b = *bp;
	return (dns_name_compare(&a->name, &b->name));
}

static isc_result_t
uniqify_name_list(dns_diff_t *list) {
	isc_result_t result;
	dns_difftuple_t *p, *q;

	CHECK(dns_diff_sort(list, name_order));

	p = ISC_LIST_HEAD(list->tuples);
	while (p != NULL) {
		do {
			q = ISC_LIST_NEXT(p, link);
			if (q == NULL || ! dns_name_equal(&p->name, &q->name))
				break;
			ISC_LIST_UNLINK(list->tuples, q, link);
			dns_difftuple_free(&q);
		} while (1);
		p = ISC_LIST_NEXT(p, link);
	}
 failure:
	return (result);
}


static isc_result_t
is_glue(dns_db_t *db, dns_dbversion_t *ver, dns_name_t *name,
	isc_boolean_t *flag)
{
	isc_result_t result;
	dns_fixedname_t foundname;
	dns_fixedname_init(&foundname);
	result = dns_db_find(db, name, ver, dns_rdatatype_any,
			     DNS_DBFIND_GLUEOK | DNS_DBFIND_NOWILD,
			     (isc_stdtime_t) 0, NULL,
			     dns_fixedname_name(&foundname),
			     NULL, NULL);
	if (result == ISC_R_SUCCESS) {
		*flag = ISC_FALSE;
		return (ISC_R_SUCCESS);
	} else if (result == DNS_R_ZONECUT) {
		/*
		 * We are at the zonecut.  The name will have an NXT, but
		 * non-delegation will be omitted from the type bit map.
		 */
		*flag = ISC_FALSE;
		return (ISC_R_SUCCESS);
	} else if (result == DNS_R_GLUE || result == DNS_R_DNAME) {
		*flag = ISC_TRUE;
		return (ISC_R_SUCCESS);
	} else {
		return (result);
	}
}

/*
 * Find the next/previous name that has a NXT record.
 * In other words, skip empty database nodes and names that
 * have had their NXTs removed because they are obscured by
 * a zone cut.
 */
static isc_result_t
next_active(dns_db_t *db, dns_dbversion_t *ver, dns_name_t *oldname,
     dns_name_t *newname, isc_boolean_t forward)
{
	isc_result_t result;
	dns_dbiterator_t *dbit = NULL;
	isc_boolean_t has_nxt;
	unsigned int wraps = 0;

	CHECK(dns_db_createiterator(db, ISC_FALSE, &dbit));

	CHECK(dns_dbiterator_seek(dbit, oldname));
	do {
		dns_dbnode_t *node = NULL;

		if (forward)
			result = dns_dbiterator_next(dbit);
		else
			result = dns_dbiterator_prev(dbit);
		if (result == ISC_R_NOMORE) {
			/*
			 * Wrap around.
			 */
			if (forward)
				CHECK(dns_dbiterator_first(dbit));
			else
				CHECK(dns_dbiterator_last(dbit));
			wraps++;
			if (wraps == 2) {
				isc_log_write(ns_g_lctx, NS_LOGCATEGORY_UPDATE,
					      NS_LOGMODULE_UPDATE,
					      ISC_LOG_ERROR,
					      "secure zone with no NXTs");
				result = DNS_R_BADZONE;
				goto failure;
			}
		}
		dns_dbiterator_current(dbit, &node, newname);
		dns_db_detachnode(db, &node);

		/*
		 * The iterator may hold the tree lock, and
		 * rrset_exists() calls dns_db_findnode() which
		 * may try to reacquire it.  To avoid deadlock
		 * we must pause the iterator first.
		 */
		CHECK(dns_dbiterator_pause(dbit));
		CHECK(rrset_exists(db, ver, newname,
				   dns_rdatatype_nxt, 0, &has_nxt));

	} while (! has_nxt);
 failure:
	if (dbit != NULL)
		dns_dbiterator_destroy(&dbit);

	return (result);
}

/*
 * Add a NXT record for "name", recording the change in "diff".
 * The existing NXT is removed.
 */
static isc_result_t
add_nxt(dns_db_t *db, dns_dbversion_t *ver, dns_name_t *name, dns_diff_t *diff)
{
	isc_result_t result;
	dns_dbnode_t *node = NULL;
	unsigned char buffer[DNS_NXT_BUFFERSIZE];
	dns_rdata_t rdata = DNS_RDATA_INIT;
	dns_difftuple_t *tuple = NULL;
	dns_fixedname_t fixedname;
	dns_name_t *target;

	dns_fixedname_init(&fixedname);
	target = dns_fixedname_name(&fixedname);

	/*
	 * Find the successor name, aka NXT target.
	 */
	CHECK(next_active(db, ver, name, target, ISC_TRUE));

	/*
	 * Create the NXT RDATA.
	 */
	CHECK(dns_db_findnode(db, name, ISC_FALSE, &node));
	dns_rdata_init(&rdata);
	CHECK(dns_nxt_buildrdata(db, ver, node, target, buffer, &rdata));
	dns_db_detachnode(db, &node);

	/*
	 * Delete the old NXT and record the change.
	 */
	CHECK(delete_if(true_p, db, ver, name, dns_rdatatype_nxt, 0,
			NULL, diff));
	/*
	 * Add the new NXT and record the change.
	 */
	CHECK(dns_difftuple_create(diff->mctx, DNS_DIFFOP_ADD, name,
				   3600,	/* XXXRTH */
				   &rdata, &tuple));
	CHECK(do_one_tuple(&tuple, db, ver, diff));
	INSIST(tuple == NULL);

 failure:
	if (node != NULL)
		dns_db_detachnode(db, &node);
	return (result);
}

/*
 * Add a placeholder NXT record for "name", recording the change in "diff".
 */
static isc_result_t
add_placeholder_nxt(dns_db_t *db, dns_dbversion_t *ver, dns_name_t *name,
		    dns_diff_t *diff) {
	isc_result_t result;
	dns_difftuple_t *tuple = NULL;
	isc_region_t r;
	unsigned char data[1] = { 0 }; /* The root domain, no bits. */
	dns_rdata_t rdata = DNS_RDATA_INIT;

	r.base = data;
	r.length = sizeof data;
	dns_rdata_fromregion(&rdata, dns_db_class(db), dns_rdatatype_nxt, &r);
	CHECK(dns_difftuple_create(diff->mctx, DNS_DIFFOP_ADD, name, 0,
				   &rdata, &tuple));
	CHECK(do_one_tuple(&tuple, db, ver, diff));
 failure:
	return (result);
}

static isc_result_t
find_zone_keys(dns_db_t *db, dns_dbversion_t *ver, isc_mem_t *mctx,
	       unsigned int maxkeys, dst_key_t **keys, unsigned int *nkeys)
{
	isc_result_t result;
	dns_dbnode_t *node = NULL;
	CHECK(dns_db_findnode(db, dns_db_origin(db), ISC_FALSE, &node));
	CHECK(dns_dnssec_findzonekeys(db, ver, node, dns_db_origin(db),
				      mctx, maxkeys, keys, nkeys));
 failure:
	if (node != NULL)
		dns_db_detachnode(db, &node);
	return (result);
}

/*
 * Add SIG records for an RRset, recording the change in "diff".
 */
static isc_result_t
add_sigs(dns_db_t *db, dns_dbversion_t *ver, dns_name_t *name,
	 dns_rdatatype_t type, dns_diff_t *diff, dst_key_t **keys,
	 unsigned int nkeys, isc_mem_t *mctx, isc_stdtime_t inception,
	 isc_stdtime_t expire)
{
	isc_result_t result;
	dns_dbnode_t *node = NULL;
	dns_rdataset_t rdataset;
	dns_rdata_t sig_rdata = DNS_RDATA_INIT;
	isc_buffer_t buffer;
	unsigned char data[1024]; /* XXX */
	unsigned int i;

	dns_rdataset_init(&rdataset);
	isc_buffer_init(&buffer, data, sizeof(data));

	/* Get the rdataset to sign. */
	CHECK(dns_db_findnode(db, name, ISC_FALSE, &node));
	CHECK(dns_db_findrdataset(db, node, ver, type, 0,
				  (isc_stdtime_t) 0,
				  &rdataset, NULL));
	dns_db_detachnode(db, &node);

	for (i = 0; i < nkeys; i++) {
		/* Calculate the signature, creating a SIG RDATA. */
		CHECK(dns_dnssec_sign(name, &rdataset, keys[i],
				      &inception, &expire,
				      mctx, &buffer, &sig_rdata));

		/* Update the database and journal with the SIG. */
		/* XXX inefficient - will cause dataset merging */
		CHECK(update_one_rr(db, ver, diff, DNS_DIFFOP_ADD, name,
				    rdataset.ttl, &sig_rdata));
		dns_rdata_reset(&sig_rdata);
	}

 failure:
	if (dns_rdataset_isassociated(&rdataset))
		dns_rdataset_disassociate(&rdataset);
	if (node != NULL)
		dns_db_detachnode(db, &node);
	return (result);
}

/*
 * Update SIG and NXT records affected by an update.  The original
 * update, including the SOA serial update but exluding the SIG & NXT
 * changes, is in "diff" and has already been applied to "newver" of "db".
 * The database version prior to the update is "oldver".
 *
 * The necessary SIG and NXT changes will be applied to "newver"
 * and added (as a minimal diff) to "diff".
 *
 * The SIGs generated will be valid for 'sigvalidityinterval' seconds.
 */
static isc_result_t
update_signatures(isc_mem_t *mctx, dns_db_t *db, dns_dbversion_t *oldver,
		  dns_dbversion_t *newver, dns_diff_t *diff,
		  isc_uint32_t sigvalidityinterval)
{
	isc_result_t result;
	dns_difftuple_t *t;
	dns_diff_t diffnames;
	dns_diff_t affected;
	dns_diff_t sig_diff;
	dns_diff_t nxt_diff;
	dns_diff_t nxt_mindiff;
	isc_boolean_t flag;
	dst_key_t *zone_keys[MAXZONEKEYS];
	unsigned int nkeys = 0;
	unsigned int i;
	isc_stdtime_t now, inception, expire;

	dns_diff_init(mctx, &diffnames);
	dns_diff_init(mctx, &affected);

	dns_diff_init(mctx, &sig_diff);
	dns_diff_init(mctx, &nxt_diff);
	dns_diff_init(mctx, &nxt_mindiff);

	result = find_zone_keys(db, newver, mctx,
				MAXZONEKEYS, zone_keys, &nkeys);
	if (result != ISC_R_SUCCESS) {
		isc_log_write(ns_g_lctx, NS_LOGCATEGORY_UPDATE,
			      NS_LOGMODULE_UPDATE, ISC_LOG_ERROR,
			      "could not get zone keys for secure "
			      "dynamic update");
		goto failure;
	}

	isc_stdtime_get(&now);
	inception = now - 3600; /* Allow for some clock skew. */
	expire = now + sigvalidityinterval;

	/*
	 * Find all RRsets directly affected by the update, and
	 * update their SIGs.  Also build a list of names affected
	 * by the update in "diffnames".
	 */
	CHECK(dns_diff_sort(diff, temp_order));

	t = ISC_LIST_HEAD(diff->tuples);
	while (t != NULL) {
		dns_name_t *name = &t->name;
		/* Now "name" is a new, unique name affected by the update. */

		CHECK(namelist_append_name(&diffnames, name));

		while (t != NULL && dns_name_equal(&t->name, name)) {
			dns_rdatatype_t type;
			type = t->rdata.type;

			/*
			 * Now "name" and "type" denote a new unique RRset
			 * affected by the update.
			 */

			/* Don't sign SIGs. */
			if (type == dns_rdatatype_sig)
				goto skip;

			/*
			 * Delete all old SIGs covering this type, since they
			 * are all invalid when the signed RRset has changed.
			 * We may not be able to recreate all of them - tough.
			 */
			CHECK(delete_if(true_p, db, newver, name,
					dns_rdatatype_sig, type,
					NULL, &sig_diff));

			/*
			 * If this RRset still exists after the update,
			 * add a new signature for it.
			 */
			CHECK(rrset_exists(db, newver, name, type, 0, &flag));
			if (flag) {
				CHECK(add_sigs(db, newver, name, type,
					       &sig_diff, zone_keys, nkeys,
					       mctx, inception, expire));
			}
		skip:
			/* Skip any other updates to the same RRset. */
			while (t != NULL &&
			       dns_name_equal(&t->name, name) &&
			       t->rdata.type == type)
			{
				t = ISC_LIST_NEXT(t, link);
			}
		}
	}

	/* Remove orphaned NXTs and SIG NXTs. */
	for (t = ISC_LIST_HEAD(diffnames.tuples);
	     t != NULL;
	     t = ISC_LIST_NEXT(t, link))
	{
		CHECK(non_nxt_rrset_exists(db, newver, &t->name, &flag));
		if (! flag) {
			CHECK(delete_if(true_p, db, newver, &t->name,
					dns_rdatatype_any, 0,
					NULL, &sig_diff));
		}
	}

	/*
	 * When a name is created or deleted, its predecessor needs to
	 * have its NXT updated.
	 */
	for (t = ISC_LIST_HEAD(diffnames.tuples);
	     t != NULL;
	     t = ISC_LIST_NEXT(t, link))
	{
		isc_boolean_t existed, exists;
		dns_fixedname_t fixedname;
		dns_name_t *prevname;

		dns_fixedname_init(&fixedname);
		prevname = dns_fixedname_name(&fixedname);

		CHECK(name_exists(db, oldver, &t->name, &existed));
		CHECK(name_exists(db, newver, &t->name, &exists));
		if (exists == existed)
			continue;

		/*
		 * Find the predecessor.
		 * When names become obscured or unobscured in this update
		 * transaction, we may find the wrong predecessor because
		 * the NXTs have not yet been updated to reflect the delegation
		 * change.  This should not matter because in this case,
		 * the correct predecessor is either the delegation node or
		 * a newly unobscured node, and those nodes are on the
		 * "affected" list in any case.
		 */
		CHECK(next_active(db, newver, &t->name, prevname, ISC_FALSE));
		CHECK(namelist_append_name(&affected, prevname));
	}

	/*
	 * Find names potentially affected by delegation changes
	 * (obscured by adding an NS or DNAME, or unobscured by
	 * removing one).
	 */
	for (t = ISC_LIST_HEAD(diffnames.tuples);
	     t != NULL;
	     t = ISC_LIST_NEXT(t, link))
	{
		isc_boolean_t ns_existed, dname_existed;
		isc_boolean_t ns_exists, dname_exists;

		CHECK(rrset_exists(db, oldver, &t->name, dns_rdatatype_ns, 0,
				   &ns_existed));
		CHECK(rrset_exists(db, oldver, &t->name, dns_rdatatype_dname, 0,
				   &dname_existed));
		CHECK(rrset_exists(db, newver, &t->name, dns_rdatatype_ns, 0,
				   &ns_exists));
		CHECK(rrset_exists(db, newver, &t->name, dns_rdatatype_dname, 0,
				   &dname_exists));
		if ((ns_exists || dname_exists) == (ns_existed || dname_existed))
			continue;
		/*
		 * There was a delegation change.  Mark all subdomains
		 * of t->name as potentially needing a NXT update.
		 */
		CHECK(namelist_append_subdomain(db, &t->name, &affected));
	}

	ISC_LIST_APPENDLIST(affected.tuples, diffnames.tuples, link);
	INSIST(ISC_LIST_EMPTY(diffnames.tuples));

	CHECK(uniqify_name_list(&affected));

	/*
	 * Determine which names should have NXTs, and delete/create
	 * NXTs to make it so.  We don't know the final NXT targets yet,
	 * so we just create placeholder NXTs with arbitrary contents
	 * to indicate that their respective owner names should be part of
	 * the NXT chain.
	 */
	for (t = ISC_LIST_HEAD(affected.tuples);
	     t != NULL;
	     t = ISC_LIST_NEXT(t, link))
	{
		isc_boolean_t exists;
		CHECK(name_exists(db, newver, &t->name, &exists));
		if (! exists)
			continue;
		CHECK(is_glue(db, newver, &t->name, &flag));
		if (flag) {
			/*
			 * This name is obscured.  Delete any
			 * existing NXT record.
			 */
			CHECK(delete_if(true_p, db, newver, &t->name,
					dns_rdatatype_nxt, 0,
					NULL, &nxt_diff));
		} else {
			/*
			 * This name is not obscured.  It should have a NXT.
			 */
			CHECK(rrset_exists(db, newver, &t->name,
					   dns_rdatatype_nxt, 0, &flag));
			if (! flag) {
				add_placeholder_nxt(db, newver, &t->name,
						    diff);
			}
		}
	}

	/*
	 * Now we know which names are part of the NXT chain.
	 * Make them all point at their correct targets.
	 */
	for (t = ISC_LIST_HEAD(affected.tuples);
	     t != NULL;
	     t = ISC_LIST_NEXT(t, link))
	{
		CHECK(rrset_exists(db, newver, &t->name,
				   dns_rdatatype_nxt, 0, &flag));
		if (flag) {
			/*
			 * There is a NXT, but we don't know if it is correct.
			 * Delete it and create a correct one to be sure.
			 * If the update was unnecessary, the diff minimization
			 * will take care of eliminating it from the journal,
			 * IXFRs, etc.
			 *
			 * The SIG bit should always be set in the NXTs
			 * we generate, because they will all get SIG NXTs.
			 * (XXX what if the zone keys are missing?).
			 * Because the SIG NXTs have not necessarily been
			 * created yet, the correctness of the bit mask relies
			 * on the assumption that NXTs are only created if
			 * there is other data, and if there is other data,
			 * there are other SIGs.
			 */
			CHECK(add_nxt(db, newver, &t->name, &nxt_diff));
		}
	}

	/*
	 * Minimize the set of NXT updates so that we don't
	 * have to regenerate the SIG NXTs for NXTs that were
	 * replaced with identical ones.
	 */
	while ((t = ISC_LIST_HEAD(nxt_diff.tuples)) != NULL) {
		ISC_LIST_UNLINK(nxt_diff.tuples, t, link);
		dns_diff_appendminimal(&nxt_mindiff, &t);
	}

	/* Update SIG NXTs. */
	for (t = ISC_LIST_HEAD(nxt_mindiff.tuples);
	     t != NULL;
	     t = ISC_LIST_NEXT(t, link))
	{
		if (t->op == DNS_DIFFOP_DEL) {
			CHECK(delete_if(true_p, db, newver, &t->name,
					dns_rdatatype_sig, dns_rdatatype_nxt,
					NULL, &sig_diff));
		} else if (t->op == DNS_DIFFOP_ADD) {
			CHECK(add_sigs(db, newver, &t->name, dns_rdatatype_nxt,
				       &sig_diff, zone_keys, nkeys, mctx,
				       inception, expire));
		} else {
			INSIST(0);
		}
	}

	/* Record our changes for the journal. */
	while ((t = ISC_LIST_HEAD(sig_diff.tuples)) != NULL) {
		ISC_LIST_UNLINK(sig_diff.tuples, t, link);
		dns_diff_appendminimal(diff, &t);
	}
	while ((t = ISC_LIST_HEAD(nxt_mindiff.tuples)) != NULL) {
		ISC_LIST_UNLINK(nxt_mindiff.tuples, t, link);
		dns_diff_appendminimal(diff, &t);
	}

	INSIST(ISC_LIST_EMPTY(sig_diff.tuples));
	INSIST(ISC_LIST_EMPTY(nxt_diff.tuples));
	INSIST(ISC_LIST_EMPTY(nxt_mindiff.tuples));

 failure:
	dns_diff_clear(&sig_diff);
	dns_diff_clear(&nxt_diff);
	dns_diff_clear(&nxt_mindiff);

	dns_diff_clear(&affected);
	dns_diff_clear(&diffnames);

	for (i = 0; i < nkeys; i++)
		dst_key_free(&zone_keys[i]);

	return (result);
}


/**************************************************************************/
/*
 * The actual update code in all its glory.  We try to follow
 * the RFC2136 pseudocode as closely as possible.
 */

static isc_result_t
send_update_event(ns_client_t *client, dns_zone_t *zone) {
	isc_result_t result = ISC_R_SUCCESS;
	update_event_t *event = NULL;
	isc_task_t *zonetask = NULL;
	ns_client_t *evclient;

	event = (update_event_t *)
		isc_event_allocate(client->mctx, client, DNS_EVENT_UPDATE,
				   update_action, NULL, sizeof(*event));
	if (event == NULL)
		FAIL(ISC_R_NOMEMORY);
	event->zone = zone;
	event->result = ISC_R_SUCCESS;

	evclient = NULL;
	ns_client_attach(client, &evclient);
	event->ev_arg = evclient;

	dns_zone_gettask(zone, &zonetask);
	isc_task_send(zonetask, (isc_event_t **)&event);

 failure:
	if (event != NULL)
		isc_event_free((isc_event_t **)&event);
	return (result);
}

static void
respond(ns_client_t *client, isc_result_t result) {
	isc_result_t msg_result;

	msg_result = dns_message_reply(client->message, ISC_TRUE);
	if (msg_result != ISC_R_SUCCESS)
		goto msg_failure;
	client->message->rcode = dns_result_torcode(result);

	ns_client_send(client);
	return;

 msg_failure:
	isc_log_write(ns_g_lctx, NS_LOGCATEGORY_UPDATE, NS_LOGMODULE_UPDATE,
		      ISC_LOG_ERROR,
		      "could not create update response message: %s",
		      isc_result_totext(msg_result));
	ns_client_next(client, msg_result);
}

void
ns_update_start(ns_client_t *client, isc_result_t sigresult) {
	dns_message_t *request = client->message;
	isc_result_t result;
	dns_name_t *zonename;
	dns_rdataset_t *zone_rdataset;
	dns_zone_t *zone = NULL;

	/*
	 * Interpret the zone section.
	 */
	result = dns_message_firstname(request, DNS_SECTION_ZONE);
	if (result != ISC_R_SUCCESS)
		FAILC(DNS_R_FORMERR,
		      "update zone section empty");

	/*
	 * The zone section must contain exactly one "question", and
	 * it must be of type SOA.
	 */
	zonename = NULL;
	dns_message_currentname(request, DNS_SECTION_ZONE, &zonename);
	zone_rdataset = ISC_LIST_HEAD(zonename->list);
	if (zone_rdataset->type != dns_rdatatype_soa)
		FAILC(DNS_R_FORMERR,
		      "update zone section contains non-SOA");
	if (ISC_LIST_NEXT(zone_rdataset, link) != NULL)
		FAILC(DNS_R_FORMERR,
		      "update zone section contains multiple RRs");

	/* The zone section must have exactly one name. */
	result = dns_message_nextname(request, DNS_SECTION_ZONE);
	if (result != ISC_R_NOMORE)
		FAILC(DNS_R_FORMERR,
		      "update zone section contains multiple RRs");

	result = dns_zt_find(client->view->zonetable, zonename, 0, NULL,
			     &zone);
	if (result != ISC_R_SUCCESS)
		FAILC(DNS_R_NOTAUTH,
		      "not authoritative for update zone");

	switch(dns_zone_gettype(zone)) {
	case dns_zone_master:
		/*
		 * We can now fail due to a bad signature as we now know
		 * that we are the master.
		 */
		if (sigresult != ISC_R_SUCCESS)
			FAIL(sigresult);
		CHECK(send_update_event(client, zone));
		break;
	case dns_zone_slave:
		CHECK(ns_client_checkacl(client, "update forwarding",
					 dns_zone_getforwardacl(zone),
					 ISC_FALSE, ISC_LOG_ERROR));
		CHECK(send_forward_event(client, zone));
		break;
	default:
		FAILC(DNS_R_NOTAUTH,
		      "not authoritative for update zone");
	}
	return;

 failure:
	/*
	 * We failed without having sent an update event to the zone.
	 * We are still in the client task context, so we can
	 * simply give an error response without switching tasks.
	 */
	respond(client, result);
	if (zone != NULL)
		dns_zone_detach(&zone);
}

static void
update_action(isc_task_t *task, isc_event_t *event) {
	update_event_t *uev = (update_event_t *) event;
	dns_zone_t *zone = uev->zone;
	ns_client_t *client = (ns_client_t *)event->ev_arg;

	isc_result_t result;
	dns_db_t *db = NULL;
	dns_dbversion_t *oldver = NULL;
	dns_dbversion_t *ver = NULL;
	dns_diff_t diff; 	/* Pending updates. */
	dns_diff_t temp; 	/* Pending RR existence assertions. */
	isc_boolean_t soa_serial_changed = ISC_FALSE;
	isc_mem_t *mctx = client->mctx;
	dns_rdatatype_t covers;
	dns_message_t *request = client->message;
	dns_rdataclass_t zoneclass;
	dns_name_t *zonename;
	dns_ssutable_t *ssutable = NULL;

	INSIST(event->ev_type == DNS_EVENT_UPDATE);

	dns_diff_init(mctx, &diff);
	dns_diff_init(mctx, &temp);

	CHECK(dns_zone_getdb(zone, &db));
	zonename = dns_db_origin(db);
	zoneclass = dns_db_class(db);
	dns_zone_getssutable(zone, &ssutable);
	dns_db_currentversion(db, &oldver);
	CHECK(dns_db_newversion(db, &ver));

	/*
	 * Check prerequisites.
	 */

	for (result = dns_message_firstname(request, DNS_SECTION_PREREQUISITE);
	     result == ISC_R_SUCCESS;
	     result = dns_message_nextname(request, DNS_SECTION_PREREQUISITE))
	{
		dns_name_t *name = NULL;
		dns_rdata_t rdata = DNS_RDATA_INIT;
		dns_ttl_t ttl;
		dns_rdataclass_t update_class;
		isc_boolean_t flag;

		get_current_rr(request, DNS_SECTION_PREREQUISITE, zoneclass,
			       &name, &rdata, &covers, &ttl, &update_class);

		if (ttl != 0)
			FAILC(DNS_R_FORMERR, "prerequisite TTL is not zero");

		if (! dns_name_issubdomain(name, zonename))
			FAILC(DNS_R_NOTZONE,
				"prerequisite name is out of zone");

		if (update_class == dns_rdataclass_any) {
			if (rdata.length != 0)
				FAILC(DNS_R_FORMERR,
				      "class ANY prerequisite "
				      "RDATA is not empty");
			if (rdata.type == dns_rdatatype_any) {
				CHECK(name_exists(db, ver, name, &flag));
				if (! flag) {
					FAILC(DNS_R_NXDOMAIN,
					      "'name in use' prerequisite "
					      "not satisfied");
				}
			} else {
				CHECK(rrset_exists(db, ver, name,
						   rdata.type, covers, &flag));
				if (! flag) {
					/* RRset does not exist. */
					FAILC(DNS_R_NXRRSET,
					"'rrset exists (value independent)' "
					"prerequisite not satisfied");
				}
			}
		} else if (update_class == dns_rdataclass_none) {
			if (rdata.length != 0)
				FAILC(DNS_R_FORMERR,
				      "class NONE prerequisite "
				      "RDATA is not empty");
			if (rdata.type == dns_rdatatype_any) {
				CHECK(name_exists(db, ver, name, &flag));
				if (flag) {
					FAILC(DNS_R_YXDOMAIN,
					      "'name not in use' prerequisite "
					      "not satisfied");
				}
			} else {
				CHECK(rrset_exists(db, ver, name,
						   rdata.type, covers, &flag));
				if (flag) {
					/* RRset exists. */
					FAILC(DNS_R_YXRRSET,
					      "'rrset does not exist' "
					      "prerequisite not satisfied");
				}
			}
		} else if (update_class == zoneclass) {
			/* "temp<rr.name, rr.type> += rr;" */
			result = temp_append(&temp, name, &rdata);
			if (result != ISC_R_SUCCESS) {
				UNEXPECTED_ERROR(__FILE__, __LINE__,
					 "temp entry creation failed: %s",
						 dns_result_totext(result));
				FAIL(ISC_R_UNEXPECTED);
			}
		} else {
			FAILC(DNS_R_FORMERR, "malformed prerequisite");
		}
	}
	if (result != ISC_R_NOMORE)
		FAIL(result);

	/*
	 * Perform the final check of the "rrset exists (value dependent)"
	 * prerequisites.
	 */
	result = temp_check(mctx, &temp, db, ver);
	if (result != ISC_R_SUCCESS)
		FAILC(result, "'RRset exists (value dependent)' "
		      "prerequisite not satisfied");

	update_log(client, zone, LOGLEVEL_DEBUG,
		   "prerequisites are OK");

	/*
	 * Check Requestor's Permissions.  It seems a bit silly to do this
	 * only after prerequisite testing, but that is what RFC2136 says.
	 */
	if (ssutable == NULL) {
		char msg[DNS_RDATACLASS_FORMATSIZE + DNS_NAME_FORMATSIZE
			 + sizeof("update '/'")];
		ns_client_aclmsg("update", zonename, client->view->rdclass,
                                 msg, sizeof(msg));
		CHECK(ns_client_checkacl(client, msg,
					 dns_zone_getupdateacl(zone),
					 ISC_FALSE, ISC_LOG_ERROR));
	} else if (client->signer == NULL) {
		/* This gets us a free log message. */
		char msg[DNS_RDATACLASS_FORMATSIZE + DNS_NAME_FORMATSIZE
			 + sizeof("update '/'")];
		ns_client_aclmsg("update", zonename, client->view->rdclass,
                                 msg, sizeof(msg));
		CHECK(ns_client_checkacl(client, msg, NULL, ISC_FALSE,
					 ISC_LOG_ERROR));
	}

	/*
	 * Perform the Update Section Prescan.
	 */

	for (result = dns_message_firstname(request, DNS_SECTION_UPDATE);
	     result == ISC_R_SUCCESS;
	     result = dns_message_nextname(request, DNS_SECTION_UPDATE))
	{
		dns_name_t *name = NULL;
		dns_rdata_t rdata = DNS_RDATA_INIT;
		dns_ttl_t ttl;
		dns_rdataclass_t update_class;
		get_current_rr(request, DNS_SECTION_UPDATE, zoneclass,
			       &name, &rdata, &covers, &ttl, &update_class);

		if (! dns_name_issubdomain(name, zonename))
			FAILC(DNS_R_NOTZONE,
			      "update RR is outside zone");
		if (update_class == zoneclass) {
			/*
			 * Check for meta-RRs.  The RFC2136 pseudocode says
			 * check for ANY|AXFR|MAILA|MAILB, but the text adds
			 * "or any other QUERY metatype"
			 */
			if (dns_rdatatype_ismeta(rdata.type)) {
				FAILC(DNS_R_FORMERR,
				      "meta-RR in update");
			}
		} else if (update_class == dns_rdataclass_any) {
			if (ttl != 0 || rdata.length != 0 ||
			    (dns_rdatatype_ismeta(rdata.type) &&
			     rdata.type != dns_rdatatype_any))
				FAILC(DNS_R_FORMERR,
				      "meta-RR in update");
		} else if (update_class == dns_rdataclass_none) {
			if (ttl != 0 ||
			    dns_rdatatype_ismeta(rdata.type))
				FAILC(DNS_R_FORMERR,
				      "meta-RR in update");
		} else {
			update_log(client, zone, ISC_LOG_WARNING,
				   "update RR has incorrect class %d",
				   update_class);
			FAIL(DNS_R_FORMERR);
		}
		/*
		 * draft-ietf-dnsind-simple-secure-update-01 says
		 * "Unlike traditional dynamic update, the client
		 * is forbidden from updating NXT records."
		 */
		if (dns_db_issecure(db)) {
			if (rdata.type == dns_rdatatype_nxt) {
				FAILC(DNS_R_REFUSED,
				      "explicit NXT updates are not allowed "
				      "in secure zones");
			}
			else if (rdata.type == dns_rdatatype_sig) {
				FAILC(DNS_R_REFUSED,
				      "explicit SIG updates are currently not "
				      "supported in secure zones");
			}
		}

		if (ssutable != NULL && client->signer != NULL) {
			if (rdata.type != dns_rdatatype_any) {
				if (!dns_ssutable_checkrules(ssutable,
							     client->signer,
							     name, rdata.type))
					FAILC(DNS_R_REFUSED,
					      "rejected by secure update");
			}
			else {
				if (!ssu_checkall(db, ver, name, ssutable,
						  client->signer))
					FAILC(DNS_R_REFUSED,
					      "rejected by secure update");
			}
		}
	}
	if (result != ISC_R_NOMORE)
		FAIL(result);

	update_log(client, zone, LOGLEVEL_DEBUG,
		   "update section prescan OK");

	/*
	 * Process the Update Section.
	 */

	for (result = dns_message_firstname(request, DNS_SECTION_UPDATE);
	     result == ISC_R_SUCCESS;
	     result = dns_message_nextname(request, DNS_SECTION_UPDATE))
	{
		dns_name_t *name = NULL;
		dns_rdata_t rdata = DNS_RDATA_INIT;
		dns_ttl_t ttl;
		dns_rdataclass_t update_class;
		isc_boolean_t flag;

		get_current_rr(request, DNS_SECTION_UPDATE, zoneclass,
			       &name, &rdata, &covers, &ttl, &update_class);

		if (update_class == zoneclass) {
			if (rdata.type == dns_rdatatype_cname) {
				CHECK(cname_incompatible_rrset_exists(db, ver,
								      name,
								      &flag));
				if (flag) {
					update_log(client, zone,
						   LOGLEVEL_PROTOCOL,
						   "attempt to add CNAME "
						   "alongside non-CNAME "
						   "ignored");
					continue;
				}
			} else {
				CHECK(rrset_exists(db, ver, name,
						   dns_rdatatype_cname, 0,
						   &flag));
				if (flag &&
				    ! dns_rdatatype_isdnssec(rdata.type))
				{
					update_log(client, zone,
						   LOGLEVEL_PROTOCOL,
						   "attempt to add non-CNAME "
						   "alongside CNAME ignored");
					continue;
				}
			}
			if (rdata.type == dns_rdatatype_soa) {
				isc_boolean_t ok;
				CHECK(rrset_exists(db, ver, name,
						   dns_rdatatype_soa, 0,
						   &flag));
				if (! flag) {
					update_log(client, zone,
						   LOGLEVEL_PROTOCOL,
						   "attempt to create 2nd "
						   "SOA ignored");
					continue;
				}
				CHECK(check_soa_increment(db, ver, &rdata,
							  &ok));
				if (! ok) {
					update_log(client, zone,
						   LOGLEVEL_PROTOCOL,
						   "SOA update failed to "
						   "increment serial, "
						   "ignoring it");
					continue;
				}
				soa_serial_changed = ISC_TRUE;
			}
			
			update_log(client, zone,
				   LOGLEVEL_PROTOCOL, "adding an RR");

			/* Prepare the affected RRset for the addition. */
			{
				add_rr_prepare_ctx_t ctx;
				ctx.db = db;
				ctx.ver = ver;
				ctx.diff = &diff;
				ctx.name = name;
				ctx.update_rr = &rdata;
				ctx.update_rr_ttl = ttl;
				ctx.ignore_add = ISC_FALSE;
				dns_diff_init(mctx, &ctx.del_diff);
				dns_diff_init(mctx, &ctx.add_diff);
				CHECK(foreach_rr(db, ver, name, rdata.type, covers,
						 add_rr_prepare_action, &ctx));

				if (ctx.ignore_add) {
					dns_diff_clear(&ctx.del_diff);
					dns_diff_clear(&ctx.add_diff);
				} else {
					CHECK(do_diff(&ctx.del_diff, db, ver, &diff));
					CHECK(do_diff(&ctx.add_diff, db, ver, &diff));
					CHECK(update_one_rr(db, ver, &diff,
							    DNS_DIFFOP_ADD,
							    name, ttl, &rdata));
				}
			}
		} else if (update_class == dns_rdataclass_any) {
			if (rdata.type == dns_rdatatype_any) {
				update_log(client, zone,
					   LOGLEVEL_PROTOCOL,
					   "delete all rrsets from a name");
				if (dns_name_equal(name, zonename)) {
					CHECK(delete_if(type_not_soa_nor_ns_p,
							db, ver, name,
							dns_rdatatype_any, 0,
							&rdata, &diff));
				} else {
					CHECK(delete_if(true_p, db, ver, name,
							dns_rdatatype_any, 0,
							&rdata, &diff));
				}
			} else if (dns_name_equal(name, zonename) &&
				   (rdata.type == dns_rdatatype_soa ||
				    rdata.type == dns_rdatatype_ns)) {
				update_log(client, zone,
					   LOGLEVEL_PROTOCOL,
					   "attempt to delete all SOA "
					   "or NS records ignored");
				continue;
			} else {
				update_log(client, zone,
					   LOGLEVEL_PROTOCOL,
					   "deleting an rrset");
				CHECK(delete_if(true_p, db, ver, name,
						rdata.type, covers, &rdata,
						&diff));
			}
		} else if (update_class == dns_rdataclass_none) {
			/*
			 * The (name == zonename) condition appears in
			 * RFC2136 3.4.2.4 but is missing from the pseudocode.
			 */
			if (dns_name_equal(name, zonename)) {
				if (rdata.type == dns_rdatatype_soa) {
					update_log(client, zone,
						   LOGLEVEL_PROTOCOL,
						   "attempt to delete SOA "
						   "ignored");
					continue;
				}
				if (rdata.type == dns_rdatatype_ns) {
					int count;
					CHECK(rr_count(db, ver, name,
						       dns_rdatatype_ns,
						       0, &count));
					if (count == 1) {
						update_log(client, zone,
							   LOGLEVEL_PROTOCOL,
							   "attempt to "
							   "delete last "
							   "NS ignored");
						continue;
					}
				}
			}
			update_log(client, zone,
				   LOGLEVEL_PROTOCOL,
				   "deleting an RR");
			CHECK(delete_if(rr_equal_p, db, ver, name,
					rdata.type, covers, &rdata, &diff));
		}
	}
	if (result != ISC_R_NOMORE)
		FAIL(result);

	/*
	 * If any changes were made, increment the SOA serial number,
	 * update SIGs and NXTs (if zone is secure), and write the update
	 * to the journal.
	 */
	if (! ISC_LIST_EMPTY(diff.tuples)) {
		char *journalfile;
		dns_journal_t *journal;

		/*
		 * Increment the SOA serial, but only if it was not
		 * changed as a result of an update operation.
		 */
		if (! soa_serial_changed) {
			CHECK(increment_soa_serial(db, ver, &diff, mctx));
		}

		if (dns_db_issecure(db)) {
			result = update_signatures(mctx, db, oldver, ver,
			   &diff, dns_zone_getsigvalidityinterval(zone));
			if (result != ISC_R_SUCCESS) {
				update_log(client, zone,
					   ISC_LOG_ERROR,
					   "SIG/NXT update failed: %s",
					   isc_result_totext(result));
				goto failure;
			}
		}

		journalfile = dns_zone_getjournal(zone);
		if (journalfile != NULL) {
			update_log(client, zone, LOGLEVEL_DEBUG,
				   "writing journal %s", journalfile);

			journal = NULL;
			result = dns_journal_open(mctx, journalfile,
						  ISC_TRUE, &journal);
			if (result != ISC_R_SUCCESS)
				FAILS(result, "journal open failed");

			result = dns_journal_write_transaction(journal, &diff);
			if (result != ISC_R_SUCCESS) {
				dns_journal_destroy(&journal);
				FAILS(result, "journal write failed");
			}

			dns_journal_destroy(&journal);
		}
	}

	/*
	 * XXXRTH  Just a note that this committing code will have to change
	 *         to handle databases that need two-phase commit, but this
	 *	   isn't a priority.
	 */
	update_log(client, zone, LOGLEVEL_DEBUG,
		   "committing update transaction");
	dns_db_closeversion(db, &ver, ISC_TRUE);

	/*
	 * Mark the zone as dirty so that it will be written to disk.
	 */
	dns_zone_markdirty(zone);

	/*
	 * Notify slaves of the change we just made.
	 */
	dns_zone_notify(zone);
	result = ISC_R_SUCCESS;
	goto common;

 failure:
	/*
	 * The reason for failure should have been logged at this point.
	 */
	if (ver != NULL) {
		update_log(client, zone, LOGLEVEL_DEBUG, 
			   "rolling back");
		dns_db_closeversion(db, &ver, ISC_FALSE);
	}

 common:
	dns_diff_clear(&temp);
	dns_diff_clear(&diff);

	if (oldver != NULL)
		dns_db_closeversion(db, &oldver, ISC_FALSE);

	if (db != NULL)
		dns_db_detach(&db);

	if (ssutable != NULL)
		dns_ssutable_detach(&ssutable);

	if (zone != NULL)
		dns_zone_detach(&zone);

	isc_task_detach(&task);
	uev->result = result;
	uev->ev_type = DNS_EVENT_UPDATEDONE;
	uev->ev_action = updatedone_action;
	isc_task_send(client->task, &event);
	INSIST(event == NULL);
}

static void
updatedone_action(isc_task_t *task, isc_event_t *event) {
	update_event_t *uev = (update_event_t *) event;
	ns_client_t *client = (ns_client_t *) event->ev_arg;

	UNUSED(task);

	INSIST(event->ev_type == DNS_EVENT_UPDATEDONE);
	INSIST(task == client->task);

	respond(client, uev->result);
	ns_client_detach(&client);
	isc_event_free(&event);
}

/*
 * Update forwarding support.
 */

static void
forward_fail(isc_task_t *task, isc_event_t *event) {
        ns_client_t *client = (ns_client_t *)event->ev_arg;

	UNUSED(task);

	respond(client, DNS_R_SERVFAIL);
	ns_client_detach(&client);
	isc_event_free((isc_event_t **)&event);
}


static void
forward_callback(void *arg, isc_result_t result, dns_message_t *answer) {
	update_event_t *uev = arg;
	ns_client_t *client = uev->ev_arg;

	if (result != ISC_R_SUCCESS) {
		INSIST(answer == NULL);
		uev->ev_type = DNS_EVENT_UPDATEDONE;
		uev->ev_action = forward_fail;
	} else {
		uev->ev_type = DNS_EVENT_UPDATEDONE;
		uev->ev_action = forward_done;
		uev->answer = answer;
	}
	isc_task_send(client->task, (isc_event_t**)&uev);
}

static void
forward_done(isc_task_t *task, isc_event_t *event) {
	update_event_t *uev = (update_event_t *) event;
	ns_client_t *client = (ns_client_t *)event->ev_arg;

	UNUSED(task);

	ns_client_sendraw(client, uev->answer);
	dns_message_destroy(&uev->answer);
	isc_event_free((isc_event_t **)&event);
	ns_client_detach(&client);
}

static void
forward_action(isc_task_t *task, isc_event_t *event) {
	update_event_t *uev = (update_event_t *) event;
	dns_zone_t *zone = uev->zone;
	ns_client_t *client = (ns_client_t *)event->ev_arg;
	isc_result_t result;

	result = dns_zone_forwardupdate(zone, client->message,
					forward_callback, event);
	if (result != ISC_R_SUCCESS) {
		uev->ev_type = DNS_EVENT_UPDATEDONE;
		uev->ev_action = forward_fail;
		isc_task_send(client->task, &event);
	}
	dns_zone_detach(&zone);
	isc_task_detach(&task);
}

static isc_result_t
send_forward_event(ns_client_t *client, dns_zone_t *zone) {
	isc_result_t result = ISC_R_SUCCESS;
	update_event_t *event = NULL;
	isc_task_t *zonetask = NULL;
	ns_client_t *evclient;

	event = (update_event_t *)
		isc_event_allocate(client->mctx, client, DNS_EVENT_UPDATE,
				   forward_action, NULL, sizeof(*event));
	if (event == NULL)
		FAIL(ISC_R_NOMEMORY);
	event->zone = zone;
	event->result = ISC_R_SUCCESS;

	evclient = NULL;
	ns_client_attach(client, &evclient);
	event->ev_arg = evclient;

	dns_zone_gettask(zone, &zonetask);
	isc_task_send(zonetask, (isc_event_t **)&event);

 failure:
	if (event != NULL)
		isc_event_free((isc_event_t **)&event);
	return (result);
}
