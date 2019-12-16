/*
 * Copyright (C) 2011, 2012, 2014, 2015  Internet Systems Consortium, Inc. ("ISC")
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

/* $Id: dbversion_test.c,v 1.1 2019/12/16 16:31:34 deraadt Exp $ */

/*! \file */

#include <config.h>

#include <atf-c.h>

#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <isc/file.h>
#include <isc/result.h>
#include <isc/serial.h>
#include <isc/stdtime.h>
#include <isc/msgcat.h>

#include <dns/db.h>
#include <dns/rdatalist.h>
#include <dns/rdataset.h>
#include <dns/rdatasetiter.h>
#include <dns/nsec3.h>

#include "dnstest.h"

static char tempname[11] = "dtXXXXXXXX";

static void
local_callback(const char *file, int line, isc_assertiontype_t type,
	       const char *cond)
{
	UNUSED(file); UNUSED(line); UNUSED(type); UNUSED(cond);
	if (strcmp(tempname, "dtXXXXXXXX"))
		unlink(tempname);
	atf_tc_pass();
	exit(0);
}

static dns_db_t *db1 = NULL, *db2 = NULL;
static dns_dbversion_t *v1 = NULL, *v2 = NULL;

static void
setup_db(void) {
	isc_result_t result;
	result = dns_db_create(mctx, "rbt", dns_rootname, dns_dbtype_zone,
			       dns_rdataclass_in, 0, NULL, &db1);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
	dns_db_newversion(db1, &v1);

	result = dns_db_create(mctx, "rbt", dns_rootname, dns_dbtype_zone,
			       dns_rdataclass_in, 0, NULL, &db2);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
	dns_db_newversion(db2, &v2);
}

static void
close_db(void) {
	if (v1 != NULL) {
		dns_db_closeversion(db1, &v1, ISC_FALSE);
		ATF_REQUIRE_EQ(v1, NULL);
	}
	if (db1 != NULL) {
		dns_db_detach(&db1);
		ATF_REQUIRE_EQ(db1, NULL);
	}

	if (v2 != NULL) {
		dns_db_closeversion(db2, &v2, ISC_FALSE);
		ATF_REQUIRE_EQ(v2, NULL);
	}
	if (db2 != NULL) {
		dns_db_detach(&db2);
		ATF_REQUIRE_EQ(db2, NULL);
	}
}

#define VERSION(callback) ((callback == NULL) ? v1 : v2)
#define VERSIONP(callback) ((callback == NULL) ? &v1 : &v2)
/*
 * Individual unit tests
 */
static void
attachversion(isc_assertioncallback_t callback) {
	isc_result_t result;
	dns_dbversion_t *v = NULL;

	result = dns_test_begin(NULL, ISC_FALSE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	setup_db();

	isc_assertion_setcallback(callback);
	dns_db_attachversion(db1, VERSION(callback), &v);
	if (callback != NULL)
		atf_tc_fail("dns_db_attachversion did not assert");

	ATF_REQUIRE_EQ(v, v1);
	dns_db_closeversion(db1, &v, ISC_FALSE);
	ATF_REQUIRE_EQ(v, NULL);

	close_db();
	dns_test_end();
}

ATF_TC(attachversion);
ATF_TC_HEAD(attachversion, tc) {
  atf_tc_set_md_var(tc, "descr", "check dns_db_attachversion passes with matching db/verison");
}
ATF_TC_BODY(attachversion, tc) {

	UNUSED(tc);

	attachversion(NULL);
}

ATF_TC(attachversion_bad);
ATF_TC_HEAD(attachversion_bad, tc) {
  atf_tc_set_md_var(tc, "descr", "check dns_db_attachversion aborts with mis-matching db/verison");
}
ATF_TC_BODY(attachversion_bad, tc) {

	UNUSED(tc);

	attachversion(local_callback);
}

static void
closeversion(isc_assertioncallback_t callback) {
	isc_result_t result;

	result = dns_test_begin(NULL, ISC_FALSE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	setup_db();

	isc_assertion_setcallback(callback);
	dns_db_closeversion(db1, VERSIONP(callback), ISC_FALSE);
	if (callback != NULL)
		atf_tc_fail("dns_db_closeversion did not assert");
	ATF_REQUIRE_EQ(v1, NULL);

	close_db();
	dns_test_end();
}

ATF_TC(closeversion);
ATF_TC_HEAD(closeversion, tc) {
  atf_tc_set_md_var(tc, "descr", "check dns_db_closeversion passes with matching db/verison");
}
ATF_TC_BODY(closeversion, tc) {

	UNUSED(tc);

	closeversion(NULL);
}

ATF_TC(closeversion_bad);
ATF_TC_HEAD(closeversion_bad, tc) {
  atf_tc_set_md_var(tc, "descr", "check dns_db_closeversion asserts with mis-matching db/verison");
}
ATF_TC_BODY(closeversion_bad, tc) {

	UNUSED(tc);

	closeversion(local_callback);
}

static void
find(isc_assertioncallback_t callback) {
	isc_result_t result;
	dns_rdataset_t rdataset;
	dns_fixedname_t fixed;

	result = dns_test_begin(NULL, ISC_FALSE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	setup_db();

	dns_rdataset_init(&rdataset);
	dns_fixedname_init(&fixed);

	isc_assertion_setcallback(callback);
	result = dns_db_find(db1, dns_rootname, VERSION(callback),
			     dns_rdatatype_soa, 0, 0, NULL,
			     dns_fixedname_name(&fixed), &rdataset, NULL);
	if (callback != NULL)
		atf_tc_fail("dns_db_find did not assert");
	ATF_REQUIRE_EQ(result, DNS_R_NXDOMAIN);

	close_db();

	dns_test_end();
}
ATF_TC(find);
ATF_TC_HEAD(find, tc) {
  atf_tc_set_md_var(tc, "descr", "check dns_db_find passes with matching db/version");
}
ATF_TC_BODY(find, tc) {

	UNUSED(tc);

	find(NULL);
}

ATF_TC(find_bad);
ATF_TC_HEAD(find_bad, tc) {
  atf_tc_set_md_var(tc, "descr", "check dns_db_find asserts with mis-matching db/version");
}
ATF_TC_BODY(find_bad, tc) {

	UNUSED(tc);

	find(local_callback);
}

static void
allrdatasets(isc_assertioncallback_t callback) {
	isc_result_t result;
	dns_dbnode_t *node = NULL;
	dns_rdatasetiter_t *iterator = NULL;

	result = dns_test_begin(NULL, ISC_FALSE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	setup_db();

	result = dns_db_findnode(db1, dns_rootname, ISC_FALSE, &node);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	isc_assertion_setcallback(callback);
	result = dns_db_allrdatasets(db1, node, VERSION(callback), 0,
				     &iterator);
	if (callback != NULL)
		atf_tc_fail("dns_db_allrdatasets did not assert");
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	dns_rdatasetiter_destroy(&iterator);
	ATF_REQUIRE_EQ(iterator, NULL);

	dns_db_detachnode(db1, &node);
	ATF_REQUIRE_EQ(node, NULL);

	close_db();

	dns_test_end();
}

ATF_TC(allrdatasets);
ATF_TC_HEAD(allrdatasets, tc) {
  atf_tc_set_md_var(tc, "descr", "check dns_db_allrdatasets passes with matching db/version");
}
ATF_TC_BODY(allrdatasets, tc) {

	UNUSED(tc);

	allrdatasets(NULL);
}

ATF_TC(allrdatasets_bad);
ATF_TC_HEAD(allrdatasets_bad, tc) {
  atf_tc_set_md_var(tc, "descr", "check dns_db_allrdatasets aborts with mis-matching db/version");
}
ATF_TC_BODY(allrdatasets_bad, tc) {

	UNUSED(tc);

	allrdatasets(local_callback);
}

static void
findrdataset(isc_assertioncallback_t callback) {
	isc_result_t result;
	dns_rdataset_t rdataset;
	dns_fixedname_t fixed;
	dns_dbnode_t *node = NULL;

	result = dns_test_begin(NULL, ISC_FALSE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	setup_db();

	dns_rdataset_init(&rdataset);
	dns_fixedname_init(&fixed);

	result = dns_db_findnode(db1, dns_rootname, ISC_FALSE, &node);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	isc_assertion_setcallback(callback);
	result = dns_db_findrdataset(db1, node, VERSION(callback),
				     dns_rdatatype_soa, 0, 0, &rdataset, NULL);
	if (callback != NULL)
		atf_tc_fail("dns_db_findrdataset did not assert");
	ATF_REQUIRE_EQ(result, ISC_R_NOTFOUND);

	dns_db_detachnode(db1, &node);
	ATF_REQUIRE_EQ(node, NULL);

	close_db();

	dns_test_end();
}

ATF_TC(findrdataset);
ATF_TC_HEAD(findrdataset, tc) {
  atf_tc_set_md_var(tc, "descr", "check dns_db_findrdataset passes with matching db/version");
}
ATF_TC_BODY(findrdataset, tc) {

	UNUSED(tc);

	findrdataset(NULL);
}

ATF_TC(findrdataset_bad);
ATF_TC_HEAD(findrdataset_bad, tc) {
  atf_tc_set_md_var(tc, "descr", "check dns_db_findrdataset aborts with mis-matching db/version");
}
ATF_TC_BODY(findrdataset_bad, tc) {

	UNUSED(tc);

	findrdataset(local_callback);
}

static void
deleterdataset(isc_assertioncallback_t callback) {
	isc_result_t result;
	dns_rdataset_t rdataset;
	dns_fixedname_t fixed;
	dns_dbnode_t *node = NULL;

	result = dns_test_begin(NULL, ISC_FALSE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	setup_db();

	dns_rdataset_init(&rdataset);
	dns_fixedname_init(&fixed);

	result = dns_db_findnode(db1, dns_rootname, ISC_FALSE, &node);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	isc_assertion_setcallback(callback);
	result = dns_db_deleterdataset(db1, node, VERSION(callback),
				       dns_rdatatype_soa, 0);
	if (callback != NULL)
		atf_tc_fail("dns_db_deleterdataset did not assert");
	ATF_REQUIRE_EQ(result, DNS_R_UNCHANGED);

	dns_db_detachnode(db1, &node);
	ATF_REQUIRE_EQ(node, NULL);

	close_db();

	dns_test_end();
}

ATF_TC(deleterdataset);
ATF_TC_HEAD(deleterdataset, tc) {
  atf_tc_set_md_var(tc, "descr", "check dns_db_deleterdataset passes with matching db/version");
}
ATF_TC_BODY(deleterdataset, tc) {

	UNUSED(tc);

	deleterdataset(NULL);
}

ATF_TC(deleterdataset_bad);
ATF_TC_HEAD(deleterdataset_bad, tc) {
  atf_tc_set_md_var(tc, "descr", "check dns_db_deleterdataset aborts with mis-matching db/version");
}
ATF_TC_BODY(deleterdataset_bad, tc) {

	UNUSED(tc);

	deleterdataset(local_callback);
}

static void
subtract(isc_assertioncallback_t callback) {
	isc_result_t result;
	dns_rdataset_t rdataset;
	dns_fixedname_t fixed;
	dns_dbnode_t *node = NULL;
	dns_rdatalist_t rdatalist;

	result = dns_test_begin(NULL, ISC_FALSE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	setup_db();

	dns_rdataset_init(&rdataset);
	dns_rdatalist_init(&rdatalist);
	dns_fixedname_init(&fixed);

	rdatalist.rdclass = dns_rdataclass_in;

	result = dns_rdatalist_tordataset(&rdatalist, &rdataset);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = dns_db_findnode(db1, dns_rootname, ISC_FALSE, &node);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	isc_assertion_setcallback(callback);
	result = dns_db_subtractrdataset(db1, node, VERSION(callback),
					 &rdataset, 0, NULL);
	if (callback != NULL)
		atf_tc_fail("dns_db_subtractrdataset did not assert");
	ATF_REQUIRE_EQ(result, DNS_R_UNCHANGED);

	dns_db_detachnode(db1, &node);
	ATF_REQUIRE_EQ(node, NULL);

	close_db();

	dns_test_end();
}

ATF_TC(subtractrdataset);
ATF_TC_HEAD(subtractrdataset, tc) {
  atf_tc_set_md_var(tc, "descr", "check dns_db_subtractrdataset passes with matching db/version");
}
ATF_TC_BODY(subtractrdataset, tc) {

	UNUSED(tc);

	subtract(NULL);
}

ATF_TC(subtractrdataset_bad);
ATF_TC_HEAD(subtractrdataset_bad, tc) {
  atf_tc_set_md_var(tc, "descr", "check dns_db_subtractrdataset aborts with mis-matching db/version");
}
ATF_TC_BODY(subtractrdataset_bad, tc) {

	UNUSED(tc);

	subtract(local_callback);
}

static void
dump(isc_assertioncallback_t callback) {
	isc_result_t result;
	FILE *f = NULL;

	result = dns_test_begin(NULL, ISC_FALSE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	setup_db();

	result = isc_file_openunique(tempname, &f);
	fclose(f);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	isc_assertion_setcallback(callback);
	result = dns_db_dump(db1, VERSION(callback), tempname);
	(void)unlink(tempname);
	if (callback != NULL)
		atf_tc_fail("dns_db_dump did not assert");
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	close_db();

	dns_test_end();
}

ATF_TC(dump);
ATF_TC_HEAD(dump, tc) {
  atf_tc_set_md_var(tc, "descr", "check dns_db_dump passes with matching db/version");
}
ATF_TC_BODY(dump, tc) {

	UNUSED(tc);

	dump(NULL);
}

ATF_TC(dump_bad);
ATF_TC_HEAD(dump_bad, tc) {
  atf_tc_set_md_var(tc, "descr", "check dns_db_dump aborts with mis-matching db/version");
}
ATF_TC_BODY(dump_bad, tc) {

	UNUSED(tc);

	dump(local_callback);
}

static void
addrdataset(isc_assertioncallback_t callback) {
	isc_result_t result;
	dns_rdataset_t rdataset;
	dns_fixedname_t fixed;
	dns_dbnode_t *node = NULL;
	dns_rdatalist_t rdatalist;

	result = dns_test_begin(NULL, ISC_FALSE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	setup_db();

	dns_rdataset_init(&rdataset);
	dns_rdatalist_init(&rdatalist);
	dns_fixedname_init(&fixed);

	rdatalist.rdclass = dns_rdataclass_in;

	result = dns_rdatalist_tordataset(&rdatalist, &rdataset);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = dns_db_findnode(db1, dns_rootname, ISC_FALSE, &node);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	isc_assertion_setcallback(callback);
	result = dns_db_addrdataset(db1, node, VERSION(callback), 0, &rdataset,
				    0, NULL);
	if (callback != NULL)
		atf_tc_fail("dns_db_adddataset did not assert");
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	dns_db_detachnode(db1, &node);
	ATF_REQUIRE_EQ(node, NULL);

	close_db();

	dns_test_end();
}

ATF_TC(addrdataset);
ATF_TC_HEAD(addrdataset, tc) {
  atf_tc_set_md_var(tc, "descr", "check dns_db_addrdataset passes with matching db/version");
}
ATF_TC_BODY(addrdataset, tc) {

	UNUSED(tc);

	addrdataset(NULL);
}

ATF_TC(addrdataset_bad);
ATF_TC_HEAD(addrdataset_bad, tc) {
  atf_tc_set_md_var(tc, "descr", "check dns_db_addrdataset aborts with mis-matching db/version");
}
ATF_TC_BODY(addrdataset_bad, tc) {

	UNUSED(tc);

	addrdataset(local_callback);
}

static void
getnsec3parameters(isc_assertioncallback_t callback) {
	isc_result_t result;
	dns_hash_t hash;
	isc_uint8_t flags;
	isc_uint16_t iterations;
	unsigned char salt[DNS_NSEC3_SALTSIZE];
	size_t salt_length = sizeof(salt);

	result = dns_test_begin(NULL, ISC_FALSE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	setup_db();

	isc_assertion_setcallback(callback);
	result = dns_db_getnsec3parameters(db1, VERSION(callback), &hash,
					   &flags, &iterations, salt,
					   &salt_length);
	if (callback != NULL)
		atf_tc_fail("dns_db_dump did not assert");
	ATF_REQUIRE_EQ(result, ISC_R_NOTFOUND);

	close_db();

	dns_test_end();
}

ATF_TC(getnsec3parameters);
ATF_TC_HEAD(getnsec3parameters, tc) {
  atf_tc_set_md_var(tc, "descr", "check dns_db_getnsec3parameters passes with matching db/version");
}
ATF_TC_BODY(getnsec3parameters, tc) {

	UNUSED(tc);

	getnsec3parameters(NULL);
}

ATF_TC(getnsec3parameters_bad);
ATF_TC_HEAD(getnsec3parameters_bad, tc) {
  atf_tc_set_md_var(tc, "descr", "check dns_db_getnsec3parameters aborts with mis-matching db/version");
}
ATF_TC_BODY(getnsec3parameters_bad, tc) {

	UNUSED(tc);

	getnsec3parameters(local_callback);
}

static void
resigned(isc_assertioncallback_t callback) {
	isc_result_t result;
	dns_rdataset_t rdataset, added;
	dns_dbnode_t *node = NULL;
	dns_rdatalist_t rdatalist;
	dns_rdata_rrsig_t rrsig;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	isc_buffer_t b;
	unsigned char buf[1024];

	result = dns_test_begin(NULL, ISC_FALSE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	setup_db();

	/*
	 * Create a dummy RRSIG record and set a resigning time.
	 */
	dns_rdataset_init(&added);
	dns_rdataset_init(&rdataset);
	dns_rdatalist_init(&rdatalist);
	isc_buffer_init(&b, buf, sizeof(buf));

	DNS_RDATACOMMON_INIT(&rrsig, dns_rdatatype_rrsig, dns_rdataclass_in);
	rrsig.covered = dns_rdatatype_a;
	rrsig.algorithm = 100;
	rrsig.labels = 0;
	rrsig.originalttl = 0;
	rrsig.timeexpire = 3600;
	rrsig.timesigned = 0;
	rrsig.keyid = 0;
	dns_name_init(&rrsig.signer, NULL);
	dns_name_clone(dns_rootname, &rrsig.signer);
	rrsig.siglen = 0;
	rrsig.signature = NULL;

	result = dns_rdata_fromstruct(&rdata, dns_rdataclass_in,
				      dns_rdatatype_rrsig, &rrsig, &b);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	rdatalist.rdclass = dns_rdataclass_in;
	rdatalist.type = dns_rdatatype_rrsig;
	ISC_LIST_APPEND(rdatalist.rdata, &rdata, link);

	result = dns_rdatalist_tordataset(&rdatalist, &rdataset);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	rdataset.attributes |= DNS_RDATASETATTR_RESIGN;
	rdataset.resign = 7200;

	result = dns_db_findnode(db1, dns_rootname, ISC_FALSE, &node);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = dns_db_addrdataset(db1, node, v1, 0, &rdataset, 0, &added);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	dns_db_detachnode(db1, &node);
	ATF_REQUIRE_EQ(node, NULL);

	isc_assertion_setcallback(callback);
	dns_db_resigned(db1, &added, VERSION(callback));
	if (callback != NULL)
		atf_tc_fail("dns_db_resigned did not assert");

	dns_rdataset_disassociate(&added);

	close_db();

	dns_test_end();
}

ATF_TC(resigned);
ATF_TC_HEAD(resigned, tc) {
  atf_tc_set_md_var(tc, "descr", "check dns_rdataset_resigned passes with matching db/version");
}
ATF_TC_BODY(resigned, tc) {

	UNUSED(tc);

	resigned(NULL);
}

ATF_TC(resigned_bad);
ATF_TC_HEAD(resigned_bad, tc) {
  atf_tc_set_md_var(tc, "descr", "check dns_rdataset_resigned aborts with mis-matching db/version");
}
ATF_TC_BODY(resigned_bad, tc) {

	UNUSED(tc);

	resigned(local_callback);
}

/*
 * Main
 */
ATF_TP_ADD_TCS(tp) {
	ATF_TP_ADD_TC(tp, dump);
	ATF_TP_ADD_TC(tp, dump_bad);
	ATF_TP_ADD_TC(tp, find);
	ATF_TP_ADD_TC(tp, find_bad);
	ATF_TP_ADD_TC(tp, allrdatasets);
	ATF_TP_ADD_TC(tp, allrdatasets_bad);
	ATF_TP_ADD_TC(tp, findrdataset);
	ATF_TP_ADD_TC(tp, findrdataset_bad);
	ATF_TP_ADD_TC(tp, addrdataset);
	ATF_TP_ADD_TC(tp, addrdataset_bad);
	ATF_TP_ADD_TC(tp, deleterdataset);
	ATF_TP_ADD_TC(tp, deleterdataset_bad);
	ATF_TP_ADD_TC(tp, subtractrdataset);
	ATF_TP_ADD_TC(tp, subtractrdataset_bad);
	ATF_TP_ADD_TC(tp, attachversion);
	ATF_TP_ADD_TC(tp, attachversion_bad);
	ATF_TP_ADD_TC(tp, closeversion);
	ATF_TP_ADD_TC(tp, closeversion_bad);
	ATF_TP_ADD_TC(tp, getnsec3parameters);
	ATF_TP_ADD_TC(tp, getnsec3parameters_bad);
	ATF_TP_ADD_TC(tp, resigned);
	ATF_TP_ADD_TC(tp, resigned_bad);

	return (atf_no_error());
}
