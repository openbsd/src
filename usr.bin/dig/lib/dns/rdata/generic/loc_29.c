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

/* $Id: loc_29.c,v 1.13 2020/09/14 08:40:43 florian Exp $ */

/* Reviewed: Wed Mar 15 18:13:09 PST 2000 by explorer */

/* RFC1876 */

#ifndef RDATA_GENERIC_LOC_29_C
#define RDATA_GENERIC_LOC_29_C

static inline isc_result_t
totext_loc(ARGS_TOTEXT) {
	int d1, m1, s1, fs1;
	int d2, m2, s2, fs2;
	unsigned long latitude;
	unsigned long longitude;
	unsigned long altitude;
	int north;
	int east;
	int below;
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
		north = 1;
		latitude -= 0x80000000;
	} else {
		north = 0;
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
		east = 1;
		longitude -= 0x80000000;
	} else {
		east = 0;
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
		below = 1;
		altitude = 10000000 - altitude;
	} else {
		below =0;
		altitude -= 10000000;
	}

	snprintf(buf, sizeof(buf),
		 "%d %d %d.%03d %s %d %d %d.%03d %s %s%lu.%02lum %s %s %s",
		 d1, m1, s1, fs1, north ? "N" : "S",
		 d2, m2, s2, fs2, east ? "E" : "W",
		 below ? "-" : "", altitude/100, altitude % 100,
		 sbuf, hbuf, vbuf);

	return (isc_str_tobuffer(buf, target));
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
		return (isc_mem_tobuffer(target, sr.base, sr.length));
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
	return (isc_mem_tobuffer(target, sr.base, 16));
}

static inline isc_result_t
towire_loc(ARGS_TOWIRE) {
	UNUSED(cctx);

	REQUIRE(rdata->type == dns_rdatatype_loc);
	REQUIRE(rdata->length != 0);

	return (isc_mem_tobuffer(target, rdata->data, rdata->length));
}

#endif	/* RDATA_GENERIC_LOC_29_C */
