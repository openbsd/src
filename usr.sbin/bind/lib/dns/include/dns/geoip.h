/*
 * Copyright (C) 2013  Internet Systems Consortium, Inc. ("ISC")
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

#ifndef DNS_GEOIP_H
#define DNS_GEOIP_H 1

/*****
 ***** Module Info
 *****/

/*! \file dns/acl.h
 * \brief
 * Address match list handling.
 */

/***
 *** Imports
 ***/

#include <isc/lang.h>
#include <isc/magic.h>
#include <isc/netaddr.h>
#include <isc/refcount.h>

#include <dns/name.h>
#include <dns/types.h>
#include <dns/iptable.h>

#ifdef HAVE_GEOIP
#include <GeoIP.h>
#else
typedef void GeoIP;
#endif

/***
 *** Types
 ***/

typedef enum {
	dns_geoip_countrycode,
	dns_geoip_countrycode3,
	dns_geoip_countryname,
	dns_geoip_region,
	dns_geoip_regionname,
	dns_geoip_country_code,
	dns_geoip_country_code3,
	dns_geoip_country_name,
	dns_geoip_region_countrycode,
	dns_geoip_region_code,
	dns_geoip_region_name,
	dns_geoip_city_countrycode,
	dns_geoip_city_countrycode3,
	dns_geoip_city_countryname,
	dns_geoip_city_region,
	dns_geoip_city_regionname,
	dns_geoip_city_name,
	dns_geoip_city_postalcode,
	dns_geoip_city_metrocode,
	dns_geoip_city_areacode,
	dns_geoip_city_continentcode,
	dns_geoip_city_timezonecode,
	dns_geoip_isp_name,
	dns_geoip_org_name,
	dns_geoip_as_asnum,
	dns_geoip_domain_name,
	dns_geoip_netspeed_id
} dns_geoip_subtype_t;

typedef struct dns_geoip_elem {
	dns_geoip_subtype_t subtype;
	GeoIP *db;
	union {
		char as_string[256];
		int as_int;
	};
} dns_geoip_elem_t;

typedef struct dns_geoip_databases {
	GeoIP *country_v4;			/* DB 1        */
	GeoIP *city_v4;				/* DB 2 or 6   */
	GeoIP *region;				/* DB 3 or 7   */
	GeoIP *isp;				/* DB 4        */
	GeoIP *org;				/* DB 5        */
	GeoIP *as;				/* DB 9        */
	GeoIP *netspeed;			/* DB 10       */
	GeoIP *domain;				/* DB 11       */
	GeoIP *country_v6;			/* DB 12       */
	GeoIP *city_v6;				/* DB 30 or 31 */
} dns_geoip_databases_t;

/***
 *** Functions
 ***/

ISC_LANG_BEGINDECLS

isc_boolean_t
dns_geoip_match(const isc_netaddr_t *reqaddr,
		const dns_geoip_databases_t *geoip,
		const dns_geoip_elem_t *elt);

void
dns_geoip_shutdown(void);

ISC_LANG_ENDDECLS
#endif /* DNS_GEOIP_H */
