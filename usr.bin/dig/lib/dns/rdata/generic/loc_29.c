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

/* $Id: loc_29.c,v 1.2 2020/02/20 18:08:51 florian Exp $ */

/* Reviewed: Wed Mar 15 18:13:09 PST 2000 by explorer */

/* RFC1876 */

#ifndef RDATA_GENERIC_LOC_29_C
#define RDATA_GENERIC_LOC_29_C

#define RRTYPE_LOC_ATTRIBUTES (0)

static inline isc_result_t
totext_loc(ARGS_TOTEXT) {
	int d1, m1, s1, fs1;
	int d2, m2, s2, fs2;
	unsigned long latitude;
	unsigned long longitude;
	unsigned long altitude;
	isc_boolean_t north;
	isc_boolean_t east;
	isc_boolean_t below;
	isc_region_t sr;
	char buf[sizeof("89 59 59.999 N 179 59 59.999 E "
			"42849672.95m 90000000m 90000000m 90000000m")];
	char sbuf[sizeof("90000000m")];
	char hbuf[sizeof("90000000m")];
	char vbuf[sizeof("90000000m")];
	unsigned char size, hp, vp;
	unsigned long poweroften[8] = { 1, 10, 100, 1000,
					10000, 100000, 1000000, 10000000 };

	UNUSED(tctx);

	REQUIRE(rdata->type == dns_rdatatype_loc);
	REQUIRE(rdata->length != 0);

	dns_rdata_toregion(rdata, &sr);

	if (sr.base[0] != 0)
		return (ISC_R_NOTIMPLEMENTED);

	REQUIRE(rdata->length == 16);

	size = sr.base[1];
	INSIST((size&0x0f) < 10 && (size>>4) < 10);
	if ((size&0x0f)> 1) {
		snprintf(sbuf, sizeof(sbuf),
			 "%lum", (size>>4) * poweroften[(size&0x0f)-2]);
	} else {
		snprintf(sbuf, sizeof(sbuf),
			 "0.%02lum", (size>>4) * poweroften[(size&0x0f)]);
	}
	hp = sr.base[2];
	INSIST((hp&0x0f) < 10 && (hp>>4) < 10);
	if ((hp&0x0f)> 1) {
		snprintf(hbuf, sizeof(hbuf),
			"%lum", (hp>>4) * poweroften[(hp&0x0f)-2]);
	} else {
		snprintf(hbuf, sizeof(hbuf),
			 "0.%02lum", (hp>>4) * poweroften[(hp&0x0f)]);
	}
	vp = sr.base[3];
	INSIST((vp&0x0f) < 10 && (vp>>4) < 10);
	if ((vp&0x0f)> 1) {
		snprintf(vbuf, sizeof(vbuf),
			 "%lum", (vp>>4) * poweroften[(vp&0x0f)-2]);
	} else {
		snprintf(vbuf, sizeof(vbuf),
			 "0.%02lum", (vp>>4) * poweroften[(vp&0x0f)]);
	}
	isc_region_consume(&sr, 4);

	latitude = uint32_fromregion(&sr);
	isc_region_consume(&sr, 4);
	if (latitude >= 0x80000000) {
		north = ISC_TRUE;
		latitude -= 0x80000000;
	} else {
		north = ISC_FALSE;
		latitude = 0x80000000 - latitude;
	}
	fs1 = (int)(latitude % 1000);
	latitude /= 1000;
	s1 = (int)(latitude % 60);
	latitude /= 60;
	m1 = (int)(latitude % 60);
	latitude /= 60;
	d1 = (int)latitude;
	INSIST(latitude <= 90U);

	longitude = uint32_fromregion(&sr);
	isc_region_consume(&sr, 4);
	if (longitude >= 0x80000000) {
		east = ISC_TRUE;
		longitude -= 0x80000000;
	} else {
		east = ISC_FALSE;
		longitude = 0x80000000 - longitude;
	}
	fs2 = (int)(longitude % 1000);
	longitude /= 1000;
	s2 = (int)(longitude % 60);
	longitude /= 60;
	m2 = (int)(longitude % 60);
	longitude /= 60;
	d2 = (int)longitude;
	INSIST(longitude <= 180U);

	altitude = uint32_fromregion(&sr);
	isc_region_consume(&sr, 4);
	if (altitude < 10000000U) {
		below = ISC_TRUE;
		altitude = 10000000 - altitude;
	} else {
		below =ISC_FALSE;
		altitude -= 10000000;
	}

	snprintf(buf, sizeof(buf),
		 "%d %d %d.%03d %s %d %d %d.%03d %s %s%lu.%02lum %s %s %s",
		 d1, m1, s1, fs1, north ? "N" : "S",
		 d2, m2, s2, fs2, east ? "E" : "W",
		 below ? "-" : "", altitude/100, altitude % 100,
		 sbuf, hbuf, vbuf);

	return (str_totext(buf, target));
}

static inline isc_result_t
fromwire_loc(ARGS_FROMWIRE) {
	isc_region_t sr;
	unsigned char c;
	unsigned long latitude;
	unsigned long longitude;

	REQUIRE(type == dns_rdatatype_loc);

	UNUSED(type);
	UNUSED(rdclass);
	UNUSED(dctx);
	UNUSED(options);

	isc_buffer_activeregion(source, &sr);
	if (sr.length < 1)
		return (ISC_R_UNEXPECTEDEND);
	if (sr.base[0] != 0) {
		/* Treat as unknown. */
		isc_buffer_forward(source, sr.length);
		return (mem_tobuffer(target, sr.base, sr.length));
	}
	if (sr.length < 16)
		return (ISC_R_UNEXPECTEDEND);

	/*
	 * Size.
	 */
	c = sr.base[1];
	if (c != 0)
		if ((c&0xf) > 9 || ((c>>4)&0xf) > 9 || ((c>>4)&0xf) == 0)
			return (ISC_R_RANGE);

	/*
	 * Horizontal precision.
	 */
	c = sr.base[2];
	if (c != 0)
		if ((c&0xf) > 9 || ((c>>4)&0xf) > 9 || ((c>>4)&0xf) == 0)
			return (ISC_R_RANGE);

	/*
	 * Vertical precision.
	 */
	c = sr.base[3];
	if (c != 0)
		if ((c&0xf) > 9 || ((c>>4)&0xf) > 9 || ((c>>4)&0xf) == 0)
			return (ISC_R_RANGE);
	isc_region_consume(&sr, 4);

	/*
	 * Latitude.
	 */
	latitude = uint32_fromregion(&sr);
	if (latitude < (0x80000000UL - 90 * 3600000) ||
	    latitude > (0x80000000UL + 90 * 3600000))
		return (ISC_R_RANGE);
	isc_region_consume(&sr, 4);

	/*
	 * Longitude.
	 */
	longitude = uint32_fromregion(&sr);
	if (longitude < (0x80000000UL - 180 * 3600000) ||
	    longitude > (0x80000000UL + 180 * 3600000))
		return (ISC_R_RANGE);

	/*
	 * Altitude.
	 * All values possible.
	 */

	isc_buffer_activeregion(source, &sr);
	isc_buffer_forward(source, 16);
	return (mem_tobuffer(target, sr.base, 16));
}

static inline isc_result_t
towire_loc(ARGS_TOWIRE) {
	UNUSED(cctx);

	REQUIRE(rdata->type == dns_rdatatype_loc);
	REQUIRE(rdata->length != 0);

	return (mem_tobuffer(target, rdata->data, rdata->length));
}

static inline int
compare_loc(ARGS_COMPARE) {
	isc_region_t r1;
	isc_region_t r2;

	REQUIRE(rdata1->type == rdata2->type);
	REQUIRE(rdata1->rdclass == rdata2->rdclass);
	REQUIRE(rdata1->type == dns_rdatatype_loc);
	REQUIRE(rdata1->length != 0);
	REQUIRE(rdata2->length != 0);

	dns_rdata_toregion(rdata1, &r1);
	dns_rdata_toregion(rdata2, &r2);
	return (isc_region_compare(&r1, &r2));
}

static inline isc_result_t
fromstruct_loc(ARGS_FROMSTRUCT) {
	dns_rdata_loc_t *loc = source;
	uint8_t c;

	REQUIRE(type == dns_rdatatype_loc);
	REQUIRE(source != NULL);
	REQUIRE(loc->common.rdtype == type);
	REQUIRE(loc->common.rdclass == rdclass);

	UNUSED(type);
	UNUSED(rdclass);

	if (loc->v.v0.version != 0)
		return (ISC_R_NOTIMPLEMENTED);
	RETERR(uint8_tobuffer(loc->v.v0.version, target));

	c = loc->v.v0.size;
	if ((c&0xf) > 9 || ((c>>4)&0xf) > 9 || ((c>>4)&0xf) == 0)
		return (ISC_R_RANGE);
	RETERR(uint8_tobuffer(loc->v.v0.size, target));

	c = loc->v.v0.horizontal;
	if ((c&0xf) > 9 || ((c>>4)&0xf) > 9 || ((c>>4)&0xf) == 0)
		return (ISC_R_RANGE);
	RETERR(uint8_tobuffer(loc->v.v0.horizontal, target));

	c = loc->v.v0.vertical;
	if ((c&0xf) > 9 || ((c>>4)&0xf) > 9 || ((c>>4)&0xf) == 0)
		return (ISC_R_RANGE);
	RETERR(uint8_tobuffer(loc->v.v0.vertical, target));

	if (loc->v.v0.latitude < (0x80000000UL - 90 * 3600000) ||
	    loc->v.v0.latitude > (0x80000000UL + 90 * 3600000))
		return (ISC_R_RANGE);
	RETERR(uint32_tobuffer(loc->v.v0.latitude, target));

	if (loc->v.v0.longitude < (0x80000000UL - 180 * 3600000) ||
	    loc->v.v0.longitude > (0x80000000UL + 180 * 3600000))
		return (ISC_R_RANGE);
	RETERR(uint32_tobuffer(loc->v.v0.longitude, target));
	return (uint32_tobuffer(loc->v.v0.altitude, target));
}

static inline isc_result_t
tostruct_loc(ARGS_TOSTRUCT) {
	dns_rdata_loc_t *loc = target;
	isc_region_t r;
	uint8_t version;

	REQUIRE(rdata->type == dns_rdatatype_loc);
	REQUIRE(target != NULL);
	REQUIRE(rdata->length != 0);

	dns_rdata_toregion(rdata, &r);
	version = uint8_fromregion(&r);
	if (version != 0)
		return (ISC_R_NOTIMPLEMENTED);

	loc->common.rdclass = rdata->rdclass;
	loc->common.rdtype = rdata->type;
	ISC_LINK_INIT(&loc->common, link);

	loc->v.v0.version = version;
	isc_region_consume(&r, 1);
	loc->v.v0.size = uint8_fromregion(&r);
	isc_region_consume(&r, 1);
	loc->v.v0.horizontal = uint8_fromregion(&r);
	isc_region_consume(&r, 1);
	loc->v.v0.vertical = uint8_fromregion(&r);
	isc_region_consume(&r, 1);
	loc->v.v0.latitude = uint32_fromregion(&r);
	isc_region_consume(&r, 4);
	loc->v.v0.longitude = uint32_fromregion(&r);
	isc_region_consume(&r, 4);
	loc->v.v0.altitude = uint32_fromregion(&r);
	isc_region_consume(&r, 4);
	return (ISC_R_SUCCESS);
}

static inline void
freestruct_loc(ARGS_FREESTRUCT) {
	dns_rdata_loc_t *loc = source;

	REQUIRE(source != NULL);
	REQUIRE(loc->common.rdtype == dns_rdatatype_loc);

	UNUSED(source);
	UNUSED(loc);
}

static inline isc_result_t
additionaldata_loc(ARGS_ADDLDATA) {
	REQUIRE(rdata->type == dns_rdatatype_loc);

	UNUSED(rdata);
	UNUSED(add);
	UNUSED(arg);

	return (ISC_R_SUCCESS);
}

static inline isc_result_t
digest_loc(ARGS_DIGEST) {
	isc_region_t r;

	REQUIRE(rdata->type == dns_rdatatype_loc);

	dns_rdata_toregion(rdata, &r);

	return ((digest)(arg, &r));
}

static inline isc_boolean_t
checkowner_loc(ARGS_CHECKOWNER) {

	REQUIRE(type == dns_rdatatype_loc);

	UNUSED(name);
	UNUSED(type);
	UNUSED(rdclass);
	UNUSED(wildcard);

	return (ISC_TRUE);
}

static inline isc_boolean_t
checknames_loc(ARGS_CHECKNAMES) {

	REQUIRE(rdata->type == dns_rdatatype_loc);

	UNUSED(rdata);
	UNUSED(owner);
	UNUSED(bad);

	return (ISC_TRUE);
}

static inline int
casecompare_loc(ARGS_COMPARE) {
	return (compare_loc(rdata1, rdata2));
}

#endif	/* RDATA_GENERIC_LOC_29_C */
