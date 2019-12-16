/*
 * Copyright (C) 2013-2015  Internet Systems Consortium, Inc. ("ISC")
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

/* $Id: geoip_test.c,v 1.1 2019/12/16 16:31:34 deraadt Exp $ */

/*! \file */

#include <config.h>

#include <atf-c.h>

#include <unistd.h>

#include <isc/print.h>
#include <isc/types.h>

#include <dns/geoip.h>

#include "dnstest.h"

#ifdef HAVE_GEOIP
#include <GeoIP.h>

/* We use GeoIP databases from the 'geoip' system test */
#define TEST_GEOIP_DATA "../../../bin/tests/system/geoip/data"

/*
 * Helper functions
 * (Mostly copied from bin/named/geoip.c)
 */
static dns_geoip_databases_t geoip = {
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
};

static void
init_geoip_db(GeoIP **dbp, GeoIPDBTypes edition, GeoIPDBTypes fallback,
	      GeoIPOptions method, const char *name)
{
	char *info;
	GeoIP *db;

	REQUIRE(dbp != NULL);

	db = *dbp;

	if (db != NULL) {
		GeoIP_delete(db);
		db = *dbp = NULL;
	}

	if (! GeoIP_db_avail(edition)) {
		fprintf(stderr, "GeoIP %s (type %d) DB not available\n",
			name, edition);
		goto fail;
	}

	fprintf(stderr, "initializing GeoIP %s (type %d) DB\n",
		name, edition);

	db = GeoIP_open_type(edition, method);
	if (db == NULL) {
		fprintf(stderr,
			"failed to initialize GeoIP %s (type %d) DB%s\n",
			name, edition, fallback == 0
			 ? "; geoip matches using this database will fail"
			 : "");
		goto fail;
	}

	info = GeoIP_database_info(db);
	if (info != NULL)
		fprintf(stderr, "%s\n", info);

	*dbp = db;
	return;

 fail:
	if (fallback != 0)
		init_geoip_db(dbp, fallback, 0, method, name);
}

static void
load_geoip(const char *dir) {
	GeoIPOptions method;

#ifdef _WIN32
	method = GEOIP_STANDARD;
#else
	method = GEOIP_MMAP_CACHE;
#endif

	if (dir != NULL) {
		char *p;
		DE_CONST(dir, p);
		GeoIP_setup_custom_directory(p);
	}

	init_geoip_db(&geoip.country_v4, GEOIP_COUNTRY_EDITION, 0,
		      method, "Country (IPv4)");
#ifdef HAVE_GEOIP_V6
	init_geoip_db(&geoip.country_v6, GEOIP_COUNTRY_EDITION_V6, 0,
		      method, "Country (IPv6)");
#endif

	init_geoip_db(&geoip.city_v4, GEOIP_CITY_EDITION_REV1,
		      GEOIP_CITY_EDITION_REV0, method, "City (IPv4)");
#if defined(HAVE_GEOIP_V6) && defined(HAVE_GEOIP_CITY_V6)
	init_geoip_db(&geoip.city_v6, GEOIP_CITY_EDITION_REV1_V6,
		      GEOIP_CITY_EDITION_REV0_V6, method, "City (IPv6)");
#endif

	init_geoip_db(&geoip.region, GEOIP_REGION_EDITION_REV1,
		      GEOIP_REGION_EDITION_REV0, method, "Region");
	init_geoip_db(&geoip.isp, GEOIP_ISP_EDITION, 0,
		      method, "ISP");
	init_geoip_db(&geoip.org, GEOIP_ORG_EDITION, 0,
		      method, "Org");
	init_geoip_db(&geoip.as, GEOIP_ASNUM_EDITION, 0,
		      method, "AS");
	init_geoip_db(&geoip.domain, GEOIP_DOMAIN_EDITION, 0,
		      method, "Domain");
	init_geoip_db(&geoip.netspeed, GEOIP_NETSPEED_EDITION, 0,
		      method, "NetSpeed");
}

static isc_boolean_t
do_lookup_string(const char *addr, dns_geoip_subtype_t subtype,
		 const char *string)
{
	dns_geoip_elem_t elt;
	struct in_addr in4;
	isc_netaddr_t na;

	inet_pton(AF_INET, addr, &in4);
	isc_netaddr_fromin(&na, &in4);

	elt.subtype = subtype;
	strcpy(elt.as_string, string);

	return (dns_geoip_match(&na, &geoip, &elt));
}

static isc_boolean_t
do_lookup_string_v6(const char *addr, dns_geoip_subtype_t subtype,
		    const char *string)
{
	dns_geoip_elem_t elt;
	struct in6_addr in6;
	isc_netaddr_t na;

	inet_pton(AF_INET6, addr, &in6);
	isc_netaddr_fromin6(&na, &in6);

	elt.subtype = subtype;
	strcpy(elt.as_string, string);

	return (dns_geoip_match(&na, &geoip, &elt));
}

static isc_boolean_t
do_lookup_int(const char *addr, dns_geoip_subtype_t subtype, int id) {
	dns_geoip_elem_t elt;
	struct in_addr in4;
	isc_netaddr_t na;

	inet_pton(AF_INET, addr, &in4);
	isc_netaddr_fromin(&na, &in4);

	elt.subtype = subtype;
	elt.as_int = id;

	return (dns_geoip_match(&na, &geoip, &elt));
}

/*
 * Individual unit tests
 */

/* GeoIP country matching */
ATF_TC(country);
ATF_TC_HEAD(country, tc) {
	atf_tc_set_md_var(tc, "descr", "test country database matching");
}
ATF_TC_BODY(country, tc) {
	isc_result_t result;
	isc_boolean_t match;

	UNUSED(tc);

	result = dns_test_begin(NULL, ISC_TRUE);
	ATF_REQUIRE(result == ISC_R_SUCCESS);

	/* Use databases from the geoip system test */
	load_geoip(TEST_GEOIP_DATA);

	if (geoip.country_v4 == NULL) {
		dns_test_end();
		atf_tc_skip("Database not available");
	}

	match = do_lookup_string("10.53.0.1", dns_geoip_country_code, "AU");
	ATF_CHECK(match);

	match = do_lookup_string("10.53.0.1",
				 dns_geoip_country_code3, "AUS");
	ATF_CHECK(match);

	match = do_lookup_string("10.53.0.1",
				 dns_geoip_country_name, "Australia");
	ATF_CHECK(match);

	dns_test_end();
}

/* GeoIP country (ipv6) matching */
ATF_TC(country_v6);
ATF_TC_HEAD(country_v6, tc) {
	atf_tc_set_md_var(tc, "descr", "test country (ipv6) database matching");
}
ATF_TC_BODY(country_v6, tc) {
	isc_result_t result;
	isc_boolean_t match;

	UNUSED(tc);

	result = dns_test_begin(NULL, ISC_TRUE);
	ATF_REQUIRE(result == ISC_R_SUCCESS);

	/* Use databases from the geoip system test */
	load_geoip(TEST_GEOIP_DATA);

	if (geoip.country_v6 == NULL) {
		dns_test_end();
		atf_tc_skip("Database not available");
	}

	match = do_lookup_string_v6("fd92:7065:b8e:ffff::1",
				    dns_geoip_country_code, "AU");
	ATF_CHECK(match);

	match = do_lookup_string_v6("fd92:7065:b8e:ffff::1",
				    dns_geoip_country_code3, "AUS");
	ATF_CHECK(match);

	match = do_lookup_string_v6("fd92:7065:b8e:ffff::1",
				    dns_geoip_country_name, "Australia");
	ATF_CHECK(match);

	dns_test_end();
}

/* GeoIP city (ipv4) matching */
ATF_TC(city);
ATF_TC_HEAD(city, tc) {
	atf_tc_set_md_var(tc, "descr", "test city database matching");
}
ATF_TC_BODY(city, tc) {
	isc_result_t result;
	isc_boolean_t match;

	UNUSED(tc);

	result = dns_test_begin(NULL, ISC_TRUE);
	ATF_REQUIRE(result == ISC_R_SUCCESS);

	/* Use databases from the geoip system test */
	load_geoip(TEST_GEOIP_DATA);

	if (geoip.city_v4 == NULL) {
		dns_test_end();
		atf_tc_skip("Database not available");
	}

	match = do_lookup_string("10.53.0.1",
				 dns_geoip_city_continentcode, "NA");
	ATF_CHECK(match);

	match = do_lookup_string("10.53.0.1",
				 dns_geoip_city_countrycode, "US");
	ATF_CHECK(match);

	match = do_lookup_string("10.53.0.1",
				 dns_geoip_city_countrycode3, "USA");
	ATF_CHECK(match);

	match = do_lookup_string("10.53.0.1",
				 dns_geoip_city_countryname, "United States");
	ATF_CHECK(match);

	match = do_lookup_string("10.53.0.1",
				 dns_geoip_city_region, "CA");
	ATF_CHECK(match);

	match = do_lookup_string("10.53.0.1",
				 dns_geoip_city_regionname, "California");
	ATF_CHECK(match);

	match = do_lookup_string("10.53.0.1",
				 dns_geoip_city_name, "Redwood City");
	ATF_CHECK(match);

	match = do_lookup_string("10.53.0.1",
				 dns_geoip_city_postalcode, "94063");
	ATF_CHECK(match);

	match = do_lookup_int("10.53.0.1", dns_geoip_city_areacode, 650);
	ATF_CHECK(match);

	match = do_lookup_int("10.53.0.1", dns_geoip_city_metrocode, 807);
	ATF_CHECK(match);

	dns_test_end();
}

/* GeoIP city (ipv6) matching */
ATF_TC(city_v6);
ATF_TC_HEAD(city_v6, tc) {
	atf_tc_set_md_var(tc, "descr", "test city (ipv6) database matching");
}
ATF_TC_BODY(city_v6, tc) {
	isc_result_t result;
	isc_boolean_t match;

	UNUSED(tc);

	result = dns_test_begin(NULL, ISC_TRUE);
	ATF_REQUIRE(result == ISC_R_SUCCESS);

	/* Use databases from the geoip system test */
	load_geoip(TEST_GEOIP_DATA);

	if (geoip.city_v6 == NULL) {
		dns_test_end();
		atf_tc_skip("Database not available");
	}

	match = do_lookup_string_v6("fd92:7065:b8e:ffff::1",
				    dns_geoip_city_continentcode, "NA");
	ATF_CHECK(match);

	match = do_lookup_string_v6("fd92:7065:b8e:ffff::1",
				    dns_geoip_city_countrycode, "US");
	ATF_CHECK(match);

	match = do_lookup_string_v6("fd92:7065:b8e:ffff::1",
				    dns_geoip_city_countrycode3, "USA");
	ATF_CHECK(match);

	match = do_lookup_string_v6("fd92:7065:b8e:ffff::1",
				    dns_geoip_city_countryname,
				    "United States");
	ATF_CHECK(match);

	match = do_lookup_string_v6("fd92:7065:b8e:ffff::1",
				    dns_geoip_city_region, "CA");
	ATF_CHECK(match);

	match = do_lookup_string_v6("fd92:7065:b8e:ffff::1",
				    dns_geoip_city_regionname, "California");
	ATF_CHECK(match);

	match = do_lookup_string_v6("fd92:7065:b8e:ffff::1",
				    dns_geoip_city_name, "Redwood City");
	ATF_CHECK(match);

	match = do_lookup_string_v6("fd92:7065:b8e:ffff::1",
				    dns_geoip_city_postalcode, "94063");
	ATF_CHECK(match);

	dns_test_end();
}


/* GeoIP region matching */
ATF_TC(region);
ATF_TC_HEAD(region, tc) {
	atf_tc_set_md_var(tc, "descr", "test region database matching");
}
ATF_TC_BODY(region, tc) {
	isc_result_t result;
	isc_boolean_t match;

	UNUSED(tc);

	result = dns_test_begin(NULL, ISC_TRUE);
	ATF_REQUIRE(result == ISC_R_SUCCESS);

	/* Use databases from the geoip system test */
	load_geoip(TEST_GEOIP_DATA);

	if (geoip.region == NULL) {
		dns_test_end();
		atf_tc_skip("Database not available");
	}

	match = do_lookup_string("10.53.0.1",
				 dns_geoip_region_code, "CA");
	ATF_CHECK(match);

	match = do_lookup_string("10.53.0.1",
				 dns_geoip_region_name, "California");
	ATF_CHECK(match);

	match = do_lookup_string("10.53.0.1",
				 dns_geoip_region_countrycode, "US");
	ATF_CHECK(match);

	dns_test_end();
}

/*
 * GeoIP best-database matching
 * (With no specified databse and a city database available, answers
 * should come from city database.  With city database unavailable, region
 * database.  Region database unavailable, country database.)
 */
ATF_TC(best);
ATF_TC_HEAD(best, tc) {
	atf_tc_set_md_var(tc, "descr", "test best database matching");
}
ATF_TC_BODY(best, tc) {
	isc_result_t result;
	isc_boolean_t match;

	UNUSED(tc);

	result = dns_test_begin(NULL, ISC_TRUE);
	ATF_REQUIRE(result == ISC_R_SUCCESS);

	/* Use databases from the geoip system test */
	load_geoip(TEST_GEOIP_DATA);

	if (geoip.region == NULL) {
		dns_test_end();
		atf_tc_skip("Database not available");
	}

	match = do_lookup_string("10.53.0.4",
				 dns_geoip_countrycode, "US");
	ATF_CHECK(match);

	match = do_lookup_string("10.53.0.4",
				 dns_geoip_countrycode3, "USA");
	ATF_CHECK(match);

	match = do_lookup_string("10.53.0.4",
				 dns_geoip_countryname, "United States");
	ATF_CHECK(match);

	match = do_lookup_string("10.53.0.4",
				 dns_geoip_regionname, "Virginia");
	ATF_CHECK(match);

	match = do_lookup_string("10.53.0.4",
				 dns_geoip_region, "VA");
	ATF_CHECK(match);

	GeoIP_delete(geoip.city_v4);
	geoip.city_v4 = NULL;

	match = do_lookup_string("10.53.0.4",
				 dns_geoip_countrycode, "AU");
	ATF_CHECK(match);

	/*
	 * Note, region doesn't support code3 or countryname, so
	 * the next two would be answered from the country database instead
	 */
	match = do_lookup_string("10.53.0.4",
				 dns_geoip_countrycode3, "CAN");
	ATF_CHECK(match);

	match = do_lookup_string("10.53.0.4",
				 dns_geoip_countryname, "Canada");
	ATF_CHECK(match);

	GeoIP_delete(geoip.region);
	geoip.region = NULL;

	match = do_lookup_string("10.53.0.4",
				 dns_geoip_countrycode, "CA");
	ATF_CHECK(match);

	match = do_lookup_string("10.53.0.4",
				 dns_geoip_countrycode3, "CAN");
	ATF_CHECK(match);

	match = do_lookup_string("10.53.0.4",
				 dns_geoip_countryname, "Canada");
	ATF_CHECK(match);

	dns_test_end();
}


/* GeoIP asnum matching */
ATF_TC(asnum);
ATF_TC_HEAD(asnum, tc) {
	atf_tc_set_md_var(tc, "descr", "test asnum database matching");
}
ATF_TC_BODY(asnum, tc) {
	isc_result_t result;
	isc_boolean_t match;

	UNUSED(tc);

	result = dns_test_begin(NULL, ISC_TRUE);
	ATF_REQUIRE(result == ISC_R_SUCCESS);

	/* Use databases from the geoip system test */
	load_geoip(TEST_GEOIP_DATA);

	if (geoip.as == NULL) {
		dns_test_end();
		atf_tc_skip("Database not available");
	}


	match = do_lookup_string("10.53.0.3", dns_geoip_as_asnum,
				 "AS100003 Three Network Labs");
	ATF_CHECK(match);

	dns_test_end();
}

/* GeoIP isp matching */
ATF_TC(isp);
ATF_TC_HEAD(isp, tc) {
	atf_tc_set_md_var(tc, "descr", "test isp database matching");
}
ATF_TC_BODY(isp, tc) {
	isc_result_t result;
	isc_boolean_t match;

	UNUSED(tc);

	result = dns_test_begin(NULL, ISC_TRUE);
	ATF_REQUIRE(result == ISC_R_SUCCESS);

	/* Use databases from the geoip system test */
	load_geoip(TEST_GEOIP_DATA);

	if (geoip.isp == NULL) {
		dns_test_end();
		atf_tc_skip("Database not available");
	}

	match = do_lookup_string("10.53.0.1", dns_geoip_isp_name,
				 "One Systems, Inc.");
	ATF_CHECK(match);

	dns_test_end();
}

/* GeoIP org matching */
ATF_TC(org);
ATF_TC_HEAD(org, tc) {
	atf_tc_set_md_var(tc, "descr", "test org database matching");
}
ATF_TC_BODY(org, tc) {
	isc_result_t result;
	isc_boolean_t match;

	UNUSED(tc);

	result = dns_test_begin(NULL, ISC_TRUE);
	ATF_REQUIRE(result == ISC_R_SUCCESS);

	/* Use databases from the geoip system test */
	load_geoip(TEST_GEOIP_DATA);

	if (geoip.org == NULL) {
		dns_test_end();
		atf_tc_skip("Database not available");
	}

	match = do_lookup_string("10.53.0.2", dns_geoip_org_name,
				 "Two Technology Ltd.");
	ATF_CHECK(match);

	dns_test_end();
}

/* GeoIP domain matching */
ATF_TC(domain);
ATF_TC_HEAD(domain, tc) {
	atf_tc_set_md_var(tc, "descr", "test domain database matching");
}
ATF_TC_BODY(domain, tc) {
	isc_result_t result;
	isc_boolean_t match;

	UNUSED(tc);

	result = dns_test_begin(NULL, ISC_TRUE);
	ATF_REQUIRE(result == ISC_R_SUCCESS);

	/* Use databases from the geoip system test */
	load_geoip(TEST_GEOIP_DATA);

	if (geoip.domain == NULL) {
		dns_test_end();
		atf_tc_skip("Database not available");
	}

	match = do_lookup_string("10.53.0.4",
				 dns_geoip_domain_name, "four.com");
	ATF_CHECK(match);

	dns_test_end();
}

/* GeoIP netspeed matching */
ATF_TC(netspeed);
ATF_TC_HEAD(netspeed, tc) {
	atf_tc_set_md_var(tc, "descr", "test netspeed database matching");
}
ATF_TC_BODY(netspeed, tc) {
	isc_result_t result;
	isc_boolean_t match;

	UNUSED(tc);

	result = dns_test_begin(NULL, ISC_TRUE);
	ATF_REQUIRE(result == ISC_R_SUCCESS);

	/* Use databases from the geoip system test */
	load_geoip(TEST_GEOIP_DATA);

	if (geoip.netspeed == NULL) {
		dns_test_end();
		atf_tc_skip("Database not available");
	}

	match = do_lookup_int("10.53.0.1", dns_geoip_netspeed_id, 0);
	ATF_CHECK(match);

	match = do_lookup_int("10.53.0.2", dns_geoip_netspeed_id, 1);
	ATF_CHECK(match);

	match = do_lookup_int("10.53.0.3", dns_geoip_netspeed_id, 2);
	ATF_CHECK(match);

	match = do_lookup_int("10.53.0.4", dns_geoip_netspeed_id, 3);
	ATF_CHECK(match);

	dns_test_end();
}
#else
ATF_TC(untested);
ATF_TC_HEAD(untested, tc) {
	atf_tc_set_md_var(tc, "descr", "skipping geoip test");
}
ATF_TC_BODY(untested, tc) {
	UNUSED(tc);
	atf_tc_skip("GeoIP not available");
}
#endif

/*
 * Main
 */
ATF_TP_ADD_TCS(tp) {
#ifdef HAVE_GEOIP
	ATF_TP_ADD_TC(tp, country);
	ATF_TP_ADD_TC(tp, country_v6);
	ATF_TP_ADD_TC(tp, city);
	ATF_TP_ADD_TC(tp, city_v6);
	ATF_TP_ADD_TC(tp, region);
	ATF_TP_ADD_TC(tp, best);
	ATF_TP_ADD_TC(tp, asnum);
	ATF_TP_ADD_TC(tp, isp);
	ATF_TP_ADD_TC(tp, org);
	ATF_TP_ADD_TC(tp, domain);
	ATF_TP_ADD_TC(tp, netspeed);
#else
	ATF_TP_ADD_TC(tp, untested);
#endif

	return (atf_no_error());
}

