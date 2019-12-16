/*
 * Copyright (C) 2011-2013, 2015  Internet Systems Consortium, Inc. ("ISC")
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

/* $Id: master_test.c,v 1.1 2019/12/16 16:31:34 deraadt Exp $ */

/*! \file */

#include <config.h>

#include <atf-c.h>

#include <stdio.h>
#include <unistd.h>

#include <isc/print.h>
#include <isc/xml.h>

#include <dns/cache.h>
#include <dns/callbacks.h>
#include <dns/db.h>
#include <dns/master.h>
#include <dns/masterdump.h>
#include <dns/name.h>
#include <dns/rdata.h>
#include <dns/rdatalist.h>
#include <dns/rdataset.h>

#include "dnstest.h"

/*
 * Helper functions
 */

#define	BUFLEN		255
#define	BIGBUFLEN	(70 * 1024)
#define TEST_ORIGIN	"test"

static dns_masterrawheader_t header;
static isc_boolean_t headerset;

dns_name_t dns_origin;
char origin[sizeof(TEST_ORIGIN)];
unsigned char name_buf[BUFLEN];
dns_rdatacallbacks_t callbacks;
char *include_file = NULL;

static isc_result_t
add_callback(void *arg, dns_name_t *owner, dns_rdataset_t *dataset);

static void
rawdata_callback(dns_zone_t *zone, dns_masterrawheader_t *header);

static isc_result_t
add_callback(void *arg, dns_name_t *owner, dns_rdataset_t *dataset) {
	char buf[BIGBUFLEN];
	isc_buffer_t target;
	isc_result_t result;

	UNUSED(arg);

	isc_buffer_init(&target, buf, BIGBUFLEN);
	result = dns_rdataset_totext(dataset, owner, ISC_FALSE, ISC_FALSE,
				     &target);
	return(result);
}

static void
rawdata_callback(dns_zone_t *zone, dns_masterrawheader_t *h) {
	UNUSED(zone);
	header = *h;
	headerset = ISC_TRUE;
}

static isc_result_t
setup_master(void (*warn)(struct dns_rdatacallbacks *, const char *, ...),
	     void (*error)(struct dns_rdatacallbacks *, const char *, ...))
{
	isc_result_t		result;
	int			len;
	isc_buffer_t		source;
	isc_buffer_t		target;

	strcpy(origin, TEST_ORIGIN);
	len = strlen(origin);
	isc_buffer_init(&source, origin, len);
	isc_buffer_add(&source, len);
	isc_buffer_setactive(&source, len);
	isc_buffer_init(&target, name_buf, BUFLEN);
	dns_name_init(&dns_origin, NULL);
	dns_master_initrawheader(&header);

	result = dns_name_fromtext(&dns_origin, &source, dns_rootname,
				   0, &target);
	if (result != ISC_R_SUCCESS)
		return(result);

	dns_rdatacallbacks_init_stdio(&callbacks);
	callbacks.add = add_callback;
	callbacks.rawdata = rawdata_callback;
	callbacks.zone = NULL;
	if (warn != NULL)
		callbacks.warn = warn;
	if (error != NULL)
		callbacks.error = error;
	headerset = ISC_FALSE;
	return (result);
}

static isc_result_t
test_master(const char *testfile, dns_masterformat_t format,
	    void (*warn)(struct dns_rdatacallbacks *, const char *, ...),
	    void (*error)(struct dns_rdatacallbacks *, const char *, ...))
{
	isc_result_t		result;

	result = setup_master(warn, error);
	if (result != ISC_R_SUCCESS)
		return(result);

	result = dns_master_loadfile2(testfile, &dns_origin, &dns_origin,
				      dns_rdataclass_in, ISC_TRUE,
				      &callbacks, mctx, format);
	return (result);
}

static void
include_callback(const char *filename, void *arg) {
	char **argp = (char **) arg;
	*argp = isc_mem_strdup(mctx, filename);
}

/*
 * Individual unit tests
 */

/* Successful load test */
ATF_TC(load);
ATF_TC_HEAD(load, tc) {
	atf_tc_set_md_var(tc, "descr", "dns_master_loadfile() loads a "
				       "valid master file and returns success");
}
ATF_TC_BODY(load, tc) {
	isc_result_t result;

	UNUSED(tc);

	result = dns_test_begin(NULL, ISC_FALSE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = test_master("testdata/master/master1.data",
			     dns_masterformat_text, NULL, NULL);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	dns_test_end();
}


/* Unepxected end of file test */
ATF_TC(unexpected);
ATF_TC_HEAD(unexpected, tc) {
	atf_tc_set_md_var(tc, "descr", "dns_master_loadfile() returns "
				       "DNS_R_UNEXPECTED when file ends "
				       "too soon");
}
ATF_TC_BODY(unexpected, tc) {
	isc_result_t result;

	UNUSED(tc);

	result = dns_test_begin(NULL, ISC_FALSE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = test_master("testdata/master/master2.data",
			     dns_masterformat_text, NULL, NULL);
	ATF_REQUIRE_EQ(result, ISC_R_UNEXPECTEDEND);

	dns_test_end();
}


/* No owner test */
ATF_TC(noowner);
ATF_TC_HEAD(noowner, tc) {
	atf_tc_set_md_var(tc, "descr", "dns_master_loadfile() accepts broken "
				       "zones with no TTL for first record "
				       "if it is an SOA");
}
ATF_TC_BODY(noowner, tc) {
	isc_result_t result;

	UNUSED(tc);

	result = dns_test_begin(NULL, ISC_FALSE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = test_master("testdata/master/master3.data",
			     dns_masterformat_text, NULL, NULL);
	ATF_REQUIRE_EQ(result, DNS_R_NOOWNER);

	dns_test_end();
}


/* No TTL test */
ATF_TC(nottl);
ATF_TC_HEAD(nottl, tc) {
	atf_tc_set_md_var(tc, "descr", "dns_master_loadfile() returns "
				       "DNS_R_NOOWNER when no owner name "
				       "is specified");
}

ATF_TC_BODY(nottl, tc) {
	isc_result_t result;

	UNUSED(tc);

	result = dns_test_begin(NULL, ISC_FALSE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = test_master("testdata/master/master4.data",
			     dns_masterformat_text, NULL, NULL);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	dns_test_end();
}


/* Bad class test */
ATF_TC(badclass);
ATF_TC_HEAD(badclass, tc) {
	atf_tc_set_md_var(tc, "descr", "dns_master_loadfile() returns "
				       "DNS_R_BADCLASS when record class "
				       "doesn't match zone class");
}
ATF_TC_BODY(badclass, tc) {
	isc_result_t result;

	UNUSED(tc);

	result = dns_test_begin(NULL, ISC_FALSE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = test_master("testdata/master/master5.data",
			     dns_masterformat_text, NULL, NULL);
	ATF_REQUIRE_EQ(result, DNS_R_BADCLASS);

	dns_test_end();
}

/* Too big rdata test */
ATF_TC(toobig);
ATF_TC_HEAD(toobig, tc) {
	atf_tc_set_md_var(tc, "descr", "dns_master_loadfile() returns "
				       "ISC_R_NOSPACE when record is too big");
}
ATF_TC_BODY(toobig, tc) {
	isc_result_t result;

	UNUSED(tc);

	result = dns_test_begin(NULL, ISC_FALSE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = test_master("testdata/master/master15.data",
			     dns_masterformat_text, NULL, NULL);
	ATF_REQUIRE_EQ(result, ISC_R_NOSPACE);

	dns_test_end();
}

/* Maximum rdata test */
ATF_TC(maxrdata);
ATF_TC_HEAD(maxrdata, tc) {
	atf_tc_set_md_var(tc, "descr", "dns_master_loadfile() returns "
				       "ISC_R_SUCCESS when record is maximum "
				       "size");
}
ATF_TC_BODY(maxrdata, tc) {
	isc_result_t result;

	UNUSED(tc);

	result = dns_test_begin(NULL, ISC_FALSE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = test_master("testdata/master/master16.data",
			     dns_masterformat_text, NULL, NULL);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	dns_test_end();
}

/* DNSKEY test */
ATF_TC(dnskey);
ATF_TC_HEAD(dnskey, tc) {
	atf_tc_set_md_var(tc, "descr", "dns_master_loadfile() understands "
				       "DNSKEY with key material");
}
ATF_TC_BODY(dnskey, tc) {
	isc_result_t result;

	UNUSED(tc);

	result = dns_test_begin(NULL, ISC_FALSE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = test_master("testdata/master/master6.data",
			     dns_masterformat_text, NULL, NULL);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	dns_test_end();
}


/* DNSKEY with no key material test */
ATF_TC(dnsnokey);
ATF_TC_HEAD(dnsnokey, tc) {
	atf_tc_set_md_var(tc, "descr", "dns_master_loadfile() understands "
				       "DNSKEY with no key material");
}
ATF_TC_BODY(dnsnokey, tc) {
	isc_result_t result;

	UNUSED(tc);

	result = dns_test_begin(NULL, ISC_FALSE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = test_master("testdata/master/master7.data",
			     dns_masterformat_text, NULL, NULL);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	dns_test_end();
}

/* Include test */
ATF_TC(include);
ATF_TC_HEAD(include, tc) {
	atf_tc_set_md_var(tc, "descr", "dns_master_loadfile() understands "
				       "$INCLUDE");
}
ATF_TC_BODY(include, tc) {
	isc_result_t result;

	UNUSED(tc);

	result = dns_test_begin(NULL, ISC_FALSE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = test_master("testdata/master/master8.data",
			     dns_masterformat_text, NULL, NULL);
	ATF_REQUIRE_EQ(result, DNS_R_SEENINCLUDE);

	dns_test_end();
}

/* Include file list test */
ATF_TC(master_includelist);
ATF_TC_HEAD(master_includelist, tc) {
	atf_tc_set_md_var(tc, "descr", "dns_master_loadfile4() returns "
				       "names of included file");
}
ATF_TC_BODY(master_includelist, tc) {
	isc_result_t result;
	char *filename = NULL;

	UNUSED(tc);

	result = dns_test_begin(NULL, ISC_FALSE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = setup_master(NULL, NULL);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = dns_master_loadfile4("testdata/master/master8.data",
				      &dns_origin, &dns_origin,
				      dns_rdataclass_in, 0, ISC_TRUE,
				      &callbacks, include_callback,
				      &filename, mctx, dns_masterformat_text);
	ATF_CHECK_EQ(result, DNS_R_SEENINCLUDE);
	ATF_CHECK(filename != NULL);
	if (filename != NULL) {
		ATF_CHECK_STREQ(filename, "testdata/master/master7.data");
		isc_mem_free(mctx, filename);
	}

	dns_test_end();
}

/* Include failure test */
ATF_TC(includefail);
ATF_TC_HEAD(includefail, tc) {
	atf_tc_set_md_var(tc, "descr", "dns_master_loadfile() understands "
				       "$INCLUDE failures");
}
ATF_TC_BODY(includefail, tc) {
	isc_result_t result;

	UNUSED(tc);

	result = dns_test_begin(NULL, ISC_FALSE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = test_master("testdata/master/master9.data",
			     dns_masterformat_text, NULL, NULL);
	ATF_REQUIRE_EQ(result, DNS_R_BADCLASS);

	dns_test_end();
}


/* Non-empty blank lines test */
ATF_TC(blanklines);
ATF_TC_HEAD(blanklines, tc) {
	atf_tc_set_md_var(tc, "descr", "dns_master_loadfile() handles "
				       "non-empty blank lines");
}
ATF_TC_BODY(blanklines, tc) {
	isc_result_t result;

	UNUSED(tc);

	result = dns_test_begin(NULL, ISC_FALSE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = test_master("testdata/master/master10.data",
			     dns_masterformat_text, NULL, NULL);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	dns_test_end();
}

/* SOA leading zeroes test */
ATF_TC(leadingzero);
ATF_TC_HEAD(leadingzero, tc) {
	atf_tc_set_md_var(tc, "descr", "dns_master_loadfile() allows "
				       "leading zeroes in SOA");
}
ATF_TC_BODY(leadingzero, tc) {
	isc_result_t result;

	UNUSED(tc);

	result = dns_test_begin(NULL, ISC_FALSE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = test_master("testdata/master/master11.data",
			     dns_masterformat_text, NULL, NULL);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	dns_test_end();
}

ATF_TC(totext);
ATF_TC_HEAD(totext, tc) {
	atf_tc_set_md_var(tc, "descr", "masterfile totext tests");
}
ATF_TC_BODY(totext, tc) {
	isc_result_t result;
	dns_rdataset_t rdataset;
	dns_rdatalist_t rdatalist;
	isc_buffer_t target;
	unsigned char buf[BIGBUFLEN];

	UNUSED(tc);

	result = dns_test_begin(NULL, ISC_FALSE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	/* First, test with an empty rdataset */
	dns_rdatalist_init(&rdatalist);
	rdatalist.rdclass = dns_rdataclass_in;
	rdatalist.type = dns_rdatatype_none;
	rdatalist.covers = dns_rdatatype_none;

	dns_rdataset_init(&rdataset);
	result = dns_rdatalist_tordataset(&rdatalist, &rdataset);
	ATF_CHECK_EQ(result, ISC_R_SUCCESS);

	isc_buffer_init(&target, buf, BIGBUFLEN);
	result = dns_master_rdatasettotext(dns_rootname,
					   &rdataset, &dns_master_style_debug,
					   &target);
	ATF_CHECK_EQ(result, ISC_R_SUCCESS);
	ATF_CHECK_EQ(isc_buffer_usedlength(&target), 0);

	/*
	 * XXX: We will also need to add tests for dumping various
	 * rdata types, classes, etc, and comparing the results against
	 * known-good output.
	 */

	dns_test_end();
}

/* Raw load */
ATF_TC(loadraw);
ATF_TC_HEAD(loadraw, tc) {
	atf_tc_set_md_var(tc, "descr", "dns_master_loadfile() loads a "
				       "valid raw file and returns success");
}
ATF_TC_BODY(loadraw, tc) {
	isc_result_t result;

	UNUSED(tc);

	result = dns_test_begin(NULL, ISC_FALSE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	/* Raw format version 0 */
	result = test_master("testdata/master/master12.data",
			     dns_masterformat_raw, NULL, NULL);
	ATF_CHECK_STREQ(isc_result_totext(result), "success");
	ATF_CHECK(headerset);
	ATF_CHECK_EQ(header.flags, 0);

	/* Raw format version 1, no source serial  */
	result = test_master("testdata/master/master13.data",
			     dns_masterformat_raw, NULL, NULL);
	ATF_CHECK_STREQ(isc_result_totext(result), "success");
	ATF_CHECK(headerset);
	ATF_CHECK_EQ(header.flags, 0);

	/* Raw format version 1, source serial == 2011120101 */
	result = test_master("testdata/master/master14.data",
			     dns_masterformat_raw, NULL, NULL);
	ATF_CHECK_STREQ(isc_result_totext(result), "success");
	ATF_CHECK(headerset);
	ATF_CHECK((header.flags & DNS_MASTERRAW_SOURCESERIALSET) != 0);
	ATF_CHECK_EQ(header.sourceserial, 2011120101);

	dns_test_end();
}

/* Raw dump*/
ATF_TC(dumpraw);
ATF_TC_HEAD(dumpraw, tc) {
	atf_tc_set_md_var(tc, "descr", "dns_master_dump*() functions "
				       "dump valid raw files");
}
ATF_TC_BODY(dumpraw, tc) {
	isc_result_t result;
	dns_db_t *db = NULL;
	dns_dbversion_t *version = NULL;
	char myorigin[sizeof(TEST_ORIGIN)];
	dns_name_t dnsorigin;
	isc_buffer_t source, target;
	unsigned char namebuf[BUFLEN];
	int len;

	UNUSED(tc);

	strcpy(myorigin, TEST_ORIGIN);
	len = strlen(myorigin);
	isc_buffer_init(&source, myorigin, len);
	isc_buffer_add(&source, len);
	isc_buffer_setactive(&source, len);
	isc_buffer_init(&target, namebuf, BUFLEN);
	dns_name_init(&dnsorigin, NULL);
	result = dns_name_fromtext(&dnsorigin, &source, dns_rootname,
				   0, &target);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = dns_test_begin(NULL, ISC_FALSE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = dns_db_create(mctx, "rbt", &dnsorigin, dns_dbtype_zone,
			       dns_rdataclass_in, 0, NULL, &db);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = dns_db_load(db, "testdata/master/master1.data");
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	dns_db_currentversion(db, &version);

	result = dns_master_dump2(mctx, db, version,
				  &dns_master_style_default, "test.dump",
				  dns_masterformat_raw);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = test_master("test.dump", dns_masterformat_raw, NULL, NULL);
	ATF_CHECK_STREQ(isc_result_totext(result), "success");
	ATF_CHECK(headerset);
	ATF_CHECK_EQ(header.flags, 0);

	dns_master_initrawheader(&header);
	header.sourceserial = 12345;
	header.flags |= DNS_MASTERRAW_SOURCESERIALSET;

	unlink("test.dump");
	result = dns_master_dump3(mctx, db, version,
				  &dns_master_style_default, "test.dump",
				  dns_masterformat_raw, &header);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = test_master("test.dump", dns_masterformat_raw, NULL, NULL);
	ATF_CHECK_STREQ(isc_result_totext(result), "success");
	ATF_CHECK(headerset);
	ATF_CHECK((header.flags & DNS_MASTERRAW_SOURCESERIALSET) != 0);
	ATF_CHECK_EQ(header.sourceserial, 12345);

	unlink("test.dump");
	dns_db_closeversion(db, &version, ISC_FALSE);
	dns_db_detach(&db);
	dns_test_end();
}

static const char *warn_expect_value;
static isc_boolean_t warn_expect_result;

static void
warn_expect(struct dns_rdatacallbacks *mycallbacks, const char *fmt, ...) {
	char buf[4096];
	va_list ap;

	UNUSED(mycallbacks);

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	if (warn_expect_value != NULL && strstr(buf, warn_expect_value) != NULL)
		warn_expect_result = ISC_TRUE;
}

/* Origin change test */
ATF_TC(neworigin);
ATF_TC_HEAD(neworigin, tc) {
	atf_tc_set_md_var(tc, "descr", "dns_master_loadfile() rejects "
				       "zones with inherited name following "
				       "$ORIGIN");
}
ATF_TC_BODY(neworigin, tc) {
	isc_result_t result;

	UNUSED(tc);

	result = dns_test_begin(NULL, ISC_FALSE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	warn_expect_value = "record with inherited owner";
	warn_expect_result = ISC_FALSE;
	result = test_master("testdata/master/master17.data",
			     dns_masterformat_text, warn_expect, NULL);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
	ATF_CHECK_MSG(warn_expect_result, "'%s' warning not emitted",
		      warn_expect_value);

	dns_test_end();
}

/*
 * Main
 */
ATF_TP_ADD_TCS(tp) {
	ATF_TP_ADD_TC(tp, load);
	ATF_TP_ADD_TC(tp, unexpected);
	ATF_TP_ADD_TC(tp, noowner);
	ATF_TP_ADD_TC(tp, nottl);
	ATF_TP_ADD_TC(tp, badclass);
	ATF_TP_ADD_TC(tp, dnskey);
	ATF_TP_ADD_TC(tp, dnsnokey);
	ATF_TP_ADD_TC(tp, include);
	ATF_TP_ADD_TC(tp, master_includelist);
	ATF_TP_ADD_TC(tp, includefail);
	ATF_TP_ADD_TC(tp, blanklines);
	ATF_TP_ADD_TC(tp, leadingzero);
	ATF_TP_ADD_TC(tp, totext);
	ATF_TP_ADD_TC(tp, loadraw);
	ATF_TP_ADD_TC(tp, dumpraw);
	ATF_TP_ADD_TC(tp, toobig);
	ATF_TP_ADD_TC(tp, maxrdata);
	ATF_TP_ADD_TC(tp, neworigin);

	return (atf_no_error());
}

