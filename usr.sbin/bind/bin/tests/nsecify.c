/*
 * Copyright (C) 2004  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 1999-2001, 2003  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
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

/* $ISC: nsecify.c,v 1.3.2.2 2004/08/28 06:25:30 marka Exp $ */

#include <config.h>

#include <stdlib.h>

#include <isc/mem.h>
#include <isc/print.h>
#include <isc/string.h>

#include <dns/db.h>
#include <dns/dbiterator.h>
#include <dns/fixedname.h>
#include <dns/nsec.h>
#include <dns/rdataset.h>
#include <dns/rdatasetiter.h>
#include <dns/result.h>

static isc_mem_t *mctx = NULL;

static inline void
fatal(const char *message) {
	fprintf(stderr, "%s\n", message);
	exit(1);
}

static inline void
check_result(isc_result_t result, const char *message) {
	if (result != ISC_R_SUCCESS) {
		fprintf(stderr, "%s: %s\n", message,
			isc_result_totext(result));
		exit(1);
	}
}

static inline isc_boolean_t
active_node(dns_db_t *db, dns_dbversion_t *version, dns_dbnode_t *node) {
	dns_rdatasetiter_t *rdsiter;
	isc_boolean_t active = ISC_FALSE;
	isc_result_t result;
	dns_rdataset_t rdataset;

	dns_rdataset_init(&rdataset);
	rdsiter = NULL;
	result = dns_db_allrdatasets(db, node, version, 0, &rdsiter);
	check_result(result, "dns_db_allrdatasets()");
	result = dns_rdatasetiter_first(rdsiter);
	while (result == ISC_R_SUCCESS) {
		dns_rdatasetiter_current(rdsiter, &rdataset);
		if (rdataset.type != dns_rdatatype_nsec)
			active = ISC_TRUE;
		dns_rdataset_disassociate(&rdataset);
		if (!active)
			result = dns_rdatasetiter_next(rdsiter);
		else
			result = ISC_R_NOMORE;
	}
	if (result != ISC_R_NOMORE)
		fatal("rdataset iteration failed");
	dns_rdatasetiter_destroy(&rdsiter);

	if (!active) {
		/*
		 * Make sure there is no NSEC record for this node.
		 */
		result = dns_db_deleterdataset(db, node, version,
					       dns_rdatatype_nsec, 0);
		if (result == DNS_R_UNCHANGED)
			result = ISC_R_SUCCESS;
		check_result(result, "dns_db_deleterdataset");
	}

	return (active);
}

static inline isc_result_t
next_active(dns_db_t *db, dns_dbversion_t *version, dns_dbiterator_t *dbiter,
	    dns_name_t *name, dns_dbnode_t **nodep)
{
	isc_result_t result;
	isc_boolean_t active;

	do {
		active = ISC_FALSE;
		result = dns_dbiterator_current(dbiter, nodep, name);
		if (result == ISC_R_SUCCESS) {
			active = active_node(db, version, *nodep);
			if (!active) {
				dns_db_detachnode(db, nodep);
				result = dns_dbiterator_next(dbiter);
			}
		}
	} while (result == ISC_R_SUCCESS && !active);

	return (result);
}

static void
nsecify(char *filename) {
	isc_result_t result;
	dns_db_t *db;
	dns_dbversion_t *wversion;
	dns_dbnode_t *node, *nextnode;
	char *origintext;
	dns_fixedname_t fname, fnextname;
	dns_name_t *name, *nextname, *target;
	isc_buffer_t b;
	size_t len;
	dns_dbiterator_t *dbiter;
	char newfilename[1024];

	dns_fixedname_init(&fname);
	name = dns_fixedname_name(&fname);
	dns_fixedname_init(&fnextname);
	nextname = dns_fixedname_name(&fnextname);

	origintext = strrchr(filename, '/');
	if (origintext == NULL)
		origintext = filename;
	else
		origintext++;	/* Skip '/'. */
	len = strlen(origintext);
	isc_buffer_init(&b, origintext, len);
	isc_buffer_add(&b, len);
	result = dns_name_fromtext(name, &b, dns_rootname, ISC_FALSE, NULL);
	check_result(result, "dns_name_fromtext()");

	db = NULL;
	result = dns_db_create(mctx, "rbt", name, dns_dbtype_zone,
			       dns_rdataclass_in, 0, NULL, &db);
	check_result(result, "dns_db_create()");
	result = dns_db_load(db, filename);
	if (result == DNS_R_SEENINCLUDE)
		result = ISC_R_SUCCESS;
	check_result(result, "dns_db_load()");
	wversion = NULL;
	result = dns_db_newversion(db, &wversion);
	check_result(result, "dns_db_newversion()");
	dbiter = NULL;
	result = dns_db_createiterator(db, ISC_FALSE, &dbiter);
	check_result(result, "dns_db_createiterator()");
	result = dns_dbiterator_first(dbiter);
	node = NULL;
	result = next_active(db, wversion, dbiter, name, &node);
	while (result == ISC_R_SUCCESS) {
		nextnode = NULL;
		result = dns_dbiterator_next(dbiter);
		if (result == ISC_R_SUCCESS)
			result = next_active(db, wversion, dbiter, nextname,
					     &nextnode);
		if (result == ISC_R_SUCCESS)
			target = nextname;
		else if (result == ISC_R_NOMORE)
			target = dns_db_origin(db);
		else {
			target = NULL;	/* Make compiler happy. */
			fatal("db iteration failed");
		}
		dns_nsec_build(db, wversion, node, target, 3600); /* XXX BEW */
		dns_db_detachnode(db, &node);
		node = nextnode;
	}
	if (result != ISC_R_NOMORE)
		fatal("db iteration failed");
	dns_dbiterator_destroy(&dbiter);
	/*
	 * XXXRTH  For now, we don't increment the SOA serial.
	 */
	dns_db_closeversion(db, &wversion, ISC_TRUE);
	len = strlen(filename);
	if (len + 4 + 1 > sizeof(newfilename))
		fatal("filename too long");
	sprintf(newfilename, "%s.new", filename);
	result = dns_db_dump(db, NULL, newfilename);
	check_result(result, "dns_db_dump");
	dns_db_detach(&db);
}

int
main(int argc, char *argv[]) {
	int i;
	isc_result_t result;

	dns_result_register();

	result = isc_mem_create(0, 0, &mctx);
	check_result(result, "isc_mem_create()");

	argc--;
	argv++;

	for (i = 0; i < argc; i++)
		nsecify(argv[i]);

	/* isc_mem_stats(mctx, stdout); */
	isc_mem_destroy(&mctx);

	return (0);
}
