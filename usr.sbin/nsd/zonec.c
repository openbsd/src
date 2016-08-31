/*
 * zonec.c -- zone compiler.
 *
 * Copyright (c) 2001-2006, NLnet Labs. All rights reserved.
 *
 * See LICENSE for the license.
 *
 */

#include "config.h"

#include <assert.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#include <netinet/in.h>

#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif

#include "zonec.h"

#include "dname.h"
#include "dns.h"
#include "namedb.h"
#include "rdata.h"
#include "region-allocator.h"
#include "util.h"
#include "zparser.h"
#include "options.h"
#include "nsec3.h"

#define ILNP_MAXDIGITS 4
#define ILNP_NUMGROUPS 4

const dname_type *error_dname;
domain_type *error_domain;

static time_t startzonec = 0;
static long int totalrrs = 0;

extern uint8_t nsecbits[NSEC_WINDOW_COUNT][NSEC_WINDOW_BITS_SIZE];
extern uint16_t nsec_highest_rcode;


/*
 * Allocate SIZE+sizeof(uint16_t) bytes and store SIZE in the first
 * element.  Return a pointer to the allocation.
 */
static uint16_t *
alloc_rdata(region_type *region, size_t size)
{
	uint16_t *result = region_alloc(region, sizeof(uint16_t) + size);
	*result = size;
	return result;
}

uint16_t *
alloc_rdata_init(region_type *region, const void *data, size_t size)
{
	uint16_t *result = region_alloc(region, sizeof(uint16_t) + size);
	*result = size;
	memcpy(result + 1, data, size);
	return result;
}

/*
 * These are parser function for generic zone file stuff.
 */
uint16_t *
zparser_conv_hex(region_type *region, const char *hex, size_t len)
{
	/* convert a hex value to wireformat */
	uint16_t *r = NULL;
	uint8_t *t;
	int i;

	if (len % 2 != 0) {
		zc_error_prev_line("number of hex digits must be a multiple of 2");
	} else if (len > MAX_RDLENGTH * 2) {
		zc_error_prev_line("hex data exceeds maximum rdata length (%d)",
				   MAX_RDLENGTH);
	} else {
		/* the length part */
		r = alloc_rdata(region, len/2);
		t = (uint8_t *)(r + 1);

		/* Now process octet by octet... */
		while (*hex) {
			*t = 0;
			for (i = 16; i >= 1; i -= 15) {
				if (isxdigit((unsigned char)*hex)) {
					*t += hexdigit_to_int(*hex) * i;
				} else {
					zc_error_prev_line(
						"illegal hex character '%c'",
						(int) *hex);
					return NULL;
				}
				++hex;
			}
			++t;
		}
	}
	return r;
}

/* convert hex, precede by a 1-byte length */
uint16_t *
zparser_conv_hex_length(region_type *region, const char *hex, size_t len)
{
	uint16_t *r = NULL;
	uint8_t *t;
	int i;
	if (len % 2 != 0) {
		zc_error_prev_line("number of hex digits must be a multiple of 2");
	} else if (len > 255 * 2) {
		zc_error_prev_line("hex data exceeds 255 bytes");
	} else {
		uint8_t *l;

		/* the length part */
		r = alloc_rdata(region, len/2+1);
		t = (uint8_t *)(r + 1);

		l = t++;
		*l = '\0';

		/* Now process octet by octet... */
		while (*hex) {
			*t = 0;
			for (i = 16; i >= 1; i -= 15) {
				if (isxdigit((unsigned char)*hex)) {
					*t += hexdigit_to_int(*hex) * i;
				} else {
					zc_error_prev_line(
						"illegal hex character '%c'",
						(int) *hex);
					return NULL;
				}
				++hex;
			}
			++t;
			++*l;
		}
	}
	return r;
}

uint16_t *
zparser_conv_time(region_type *region, const char *time)
{
	/* convert a time YYHM to wireformat */
	uint16_t *r = NULL;
	struct tm tm;

	/* Try to scan the time... */
	if (!strptime(time, "%Y%m%d%H%M%S", &tm)) {
		zc_error_prev_line("date and time is expected");
	} else {
		uint32_t l = htonl(mktime_from_utc(&tm));
		r = alloc_rdata_init(region, &l, sizeof(l));
	}
	return r;
}

uint16_t *
zparser_conv_services(region_type *region, const char *protostr,
		      char *servicestr)
{
	/*
	 * Convert a protocol and a list of service port numbers
	 * (separated by spaces) in the rdata to wireformat
	 */
	uint16_t *r = NULL;
	uint8_t *p;
	uint8_t bitmap[65536/8];
	char sep[] = " ";
	char *word;
	int max_port = -8;
	/* convert a protocol in the rdata to wireformat */
	struct protoent *proto;

	memset(bitmap, 0, sizeof(bitmap));

	proto = getprotobyname(protostr);
	if (!proto) {
		proto = getprotobynumber(atoi(protostr));
	}
	if (!proto) {
		zc_error_prev_line("unknown protocol '%s'", protostr);
		return NULL;
	}

	for (word = strtok(servicestr, sep); word; word = strtok(NULL, sep)) {
		struct servent *service;
		int port;

		service = getservbyname(word, proto->p_name);
		if (service) {
			/* Note: ntohs not ntohl!  Strange but true.  */
			port = ntohs((uint16_t) service->s_port);
		} else {
			char *end;
			port = strtol(word, &end, 10);
			if (*end != '\0') {
				zc_error_prev_line("unknown service '%s' for protocol '%s'",
						   word, protostr);
				continue;
			}
		}

		if (port < 0 || port > 65535) {
			zc_error_prev_line("bad port number %d", port);
		} else {
			set_bit(bitmap, port);
			if (port > max_port)
				max_port = port;
		}
	}

	r = alloc_rdata(region, sizeof(uint8_t) + max_port / 8 + 1);
	p = (uint8_t *) (r + 1);
	*p = proto->p_proto;
	memcpy(p + 1, bitmap, *r-1);

	return r;
}

uint16_t *
zparser_conv_serial(region_type *region, const char *serialstr)
{
	uint16_t *r = NULL;
	uint32_t serial;
	const char *t;

	serial = strtoserial(serialstr, &t);
	if (*t != '\0') {
		zc_error_prev_line("serial is expected");
	} else {
		serial = htonl(serial);
		r = alloc_rdata_init(region, &serial, sizeof(serial));
	}
	return r;
}

uint16_t *
zparser_conv_period(region_type *region, const char *periodstr)
{
	/* convert a time period (think TTL's) to wireformat) */
	uint16_t *r = NULL;
	uint32_t period;
	const char *end;

	/* Allocate required space... */
	period = strtottl(periodstr, &end);
	if (*end != '\0') {
		zc_error_prev_line("time period is expected");
	} else {
		period = htonl(period);
		r = alloc_rdata_init(region, &period, sizeof(period));
	}
	return r;
}

uint16_t *
zparser_conv_short(region_type *region, const char *text)
{
	uint16_t *r = NULL;
	uint16_t value;
	char *end;

	value = htons((uint16_t) strtol(text, &end, 10));
	if (*end != '\0') {
		zc_error_prev_line("integer value is expected");
	} else {
		r = alloc_rdata_init(region, &value, sizeof(value));
	}
	return r;
}

uint16_t *
zparser_conv_byte(region_type *region, const char *text)
{
	uint16_t *r = NULL;
	uint8_t value;
	char *end;

	value = (uint8_t) strtol(text, &end, 10);
	if (*end != '\0') {
		zc_error_prev_line("integer value is expected");
	} else {
		r = alloc_rdata_init(region, &value, sizeof(value));
	}
	return r;
}

uint16_t *
zparser_conv_algorithm(region_type *region, const char *text)
{
	const lookup_table_type *alg;
	uint8_t id;

	alg = lookup_by_name(dns_algorithms, text);
	if (alg) {
		id = (uint8_t) alg->id;
	} else {
		char *end;
		id = (uint8_t) strtol(text, &end, 10);
		if (*end != '\0') {
			zc_error_prev_line("algorithm is expected");
			return NULL;
		}
	}

	return alloc_rdata_init(region, &id, sizeof(id));
}

uint16_t *
zparser_conv_certificate_type(region_type *region, const char *text)
{
	/* convert an algorithm string to integer */
	const lookup_table_type *type;
	uint16_t id;

	type = lookup_by_name(dns_certificate_types, text);
	if (type) {
		id = htons((uint16_t) type->id);
	} else {
		char *end;
		id = htons((uint16_t) strtol(text, &end, 10));
		if (*end != '\0') {
			zc_error_prev_line("certificate type is expected");
			return NULL;
		}
	}

	return alloc_rdata_init(region, &id, sizeof(id));
}

uint16_t *
zparser_conv_a(region_type *region, const char *text)
{
	in_addr_t address;
	uint16_t *r = NULL;

	if (inet_pton(AF_INET, text, &address) != 1) {
		zc_error_prev_line("invalid IPv4 address '%s'", text);
	} else {
		r = alloc_rdata_init(region, &address, sizeof(address));
	}
	return r;
}

uint16_t *
zparser_conv_aaaa(region_type *region, const char *text)
{
	uint8_t address[IP6ADDRLEN];
	uint16_t *r = NULL;

	if (inet_pton(AF_INET6, text, address) != 1) {
		zc_error_prev_line("invalid IPv6 address '%s'", text);
	} else {
		r = alloc_rdata_init(region, address, sizeof(address));
	}
	return r;
}


uint16_t *
zparser_conv_ilnp64(region_type *region, const char *text)
{
	uint16_t *r = NULL;
	int ngroups, num;
	unsigned long hex;
	const char *ch;
	char digits[ILNP_MAXDIGITS+1];
	unsigned int ui[ILNP_NUMGROUPS];
	uint16_t a[ILNP_NUMGROUPS];

	ngroups = 1; /* Always at least one group */
	num = 0;
	for (ch = text; *ch != '\0'; ch++) {
		if (*ch == ':') {
			if (num <= 0) {
				zc_error_prev_line("ilnp64: empty group of "
					"digits is not allowed");
				return NULL;
			}
			digits[num] = '\0';
			hex = (unsigned long) strtol(digits, NULL, 16);
			num = 0;
			ui[ngroups - 1] = hex;
			if (ngroups >= ILNP_NUMGROUPS) {
				zc_error_prev_line("ilnp64: more than %d groups "
					"of digits", ILNP_NUMGROUPS);
				return NULL;
			}
			ngroups++;
		} else {
			/* Our grammar is stricter than the one accepted by
			 * strtol. */
			if (!isxdigit((unsigned char)*ch)) {
				zc_error_prev_line("ilnp64: invalid "
					"(non-hexadecimal) character %c", *ch);
				return NULL;
			}
			if (num >= ILNP_MAXDIGITS) {
				zc_error_prev_line("ilnp64: more than %d digits "
					"in a group", ILNP_MAXDIGITS);
				return NULL;
			}
			digits[num++] = *ch;
		}
	}
	if (num <= 0) {
		zc_error_prev_line("ilnp64: empty group of digits is not "
			"allowed");
		return NULL;
	}
	digits[num] = '\0';
	hex = (unsigned long) strtol(digits, NULL, 16);
	ui[ngroups - 1] = hex;
	if (ngroups < 4) {
		zc_error_prev_line("ilnp64: less than %d groups of digits",
			ILNP_NUMGROUPS);
		return NULL;
	}

	a[0] = htons(ui[0]);
	a[1] = htons(ui[1]);
	a[2] = htons(ui[2]);
	a[3] = htons(ui[3]);
	r = alloc_rdata_init(region, a, sizeof(a));
	return r;
}

static uint16_t *
zparser_conv_eui48(region_type *region, const char *text)
{
	uint8_t nums[6];
	uint16_t *r = NULL;
	unsigned int a, b, c, d, e, f;
	int l;

	if (sscanf(text, "%2x-%2x-%2x-%2x-%2x-%2x%n",
		&a, &b, &c, &d, &e, &f, &l) != 6 ||
		l != (int)strlen(text)){
		zc_error_prev_line("eui48: invalid rr");
		return NULL;
	}
	nums[0] = (uint8_t)a;
	nums[1] = (uint8_t)b;
	nums[2] = (uint8_t)c;
	nums[3] = (uint8_t)d;
	nums[4] = (uint8_t)e;
	nums[5] = (uint8_t)f;
	r = alloc_rdata_init(region, nums, sizeof(nums));
	return r;
}

static uint16_t *
zparser_conv_eui64(region_type *region, const char *text)
{
	uint8_t nums[8];
	uint16_t *r = NULL;
	unsigned int a, b, c, d, e, f, g, h;
	int l;
	if (sscanf(text, "%2x-%2x-%2x-%2x-%2x-%2x-%2x-%2x%n",
		&a, &b, &c, &d, &e, &f, &g, &h, &l) != 8 ||
		l != (int)strlen(text)) {
		zc_error_prev_line("eui64: invalid rr");
		return NULL;
	}
	nums[0] = (uint8_t)a;
	nums[1] = (uint8_t)b;
	nums[2] = (uint8_t)c;
	nums[3] = (uint8_t)d;
	nums[4] = (uint8_t)e;
	nums[5] = (uint8_t)f;
	nums[6] = (uint8_t)g;
	nums[7] = (uint8_t)h;
	r = alloc_rdata_init(region, nums, sizeof(nums));
	return r;
}

uint16_t *
zparser_conv_eui(region_type *region, const char *text, size_t len)
{
	uint16_t *r = NULL;
	int nnum, num;
	const char* ch;

	nnum = len/8;
	num = 1;
	for (ch = text; *ch != '\0'; ch++) {
		if (*ch == '-') {
			num++;
		} else if (!isxdigit((unsigned char)*ch)) {
			zc_error_prev_line("eui%u: invalid (non-hexadecimal) "
				"character %c", (unsigned) len, *ch);
			return NULL;
		}
	}
	if (num != nnum) {
		zc_error_prev_line("eui%u: wrong number of hex numbers",
			(unsigned) len);
		return NULL;
	}

	switch (len) {
		case 48:
			r = zparser_conv_eui48(region, text);
			break;
		case 64:
			r = zparser_conv_eui64(region, text);
		break;
		default:
			zc_error_prev_line("eui%u: invalid length",
				(unsigned) len);
			return NULL;
			break;
	}
	return r;
}

uint16_t *
zparser_conv_text(region_type *region, const char *text, size_t len)
{
	uint16_t *r = NULL;
	uint8_t *p;

	if (len > 255) {
		zc_error_prev_line("text string is longer than 255 characters,"
				   " try splitting it into multiple parts");
		len = 255;
	}
	r = alloc_rdata(region, len + 1);
	p = (uint8_t *) (r + 1);
	*p = len;
	memcpy(p + 1, text, len);
	return r;
}

/* for CAA Value [RFC 6844] */
uint16_t *
zparser_conv_long_text(region_type *region, const char *text, size_t len)
{
	uint16_t *r = NULL;
	if (len > MAX_RDLENGTH) {
		zc_error_prev_line("text string is longer than max rdlen");
		return NULL;
	}
	r = alloc_rdata_init(region, text, len);
	return r;
}

/* for CAA Tag [RFC 6844] */
uint16_t *
zparser_conv_tag(region_type *region, const char *text, size_t len)
{
	uint16_t *r = NULL;
	uint8_t *p;
	const char* ptr;

	if (len < 1) {
		zc_error_prev_line("invalid tag: zero length");
		return NULL;
	}
	if (len > 15) {
		zc_error_prev_line("invalid tag %s: longer than 15 characters (%u)",
			text, (unsigned) len);
		return NULL;
	}
	for (ptr = text; *ptr; ptr++) {
		if (!isdigit((unsigned char)*ptr) && !islower((unsigned char)*ptr)) {
			zc_error_prev_line("invalid tag %s: contains invalid char %c",
				text, *ptr);
			return NULL;
		}
	}
	r = alloc_rdata(region, len + 1);
	p = (uint8_t *) (r + 1);
	*p = len;
	memmove(p + 1, text, len);
	return r;
}

uint16_t *
zparser_conv_dns_name(region_type *region, const uint8_t* name, size_t len)
{
	uint16_t* r = NULL;
	uint8_t* p = NULL;
	r = alloc_rdata(region, len);
	p = (uint8_t *) (r + 1);
	memcpy(p, name, len);

	return r;
}

uint16_t *
zparser_conv_b32(region_type *region, const char *b32)
{
	uint8_t buffer[B64BUFSIZE];
	uint16_t *r = NULL;
	int i;

	if(strcmp(b32, "-") == 0) {
		return alloc_rdata_init(region, "", 1);
	}
	i = b32_pton(b32, buffer+1, B64BUFSIZE-1);
	if (i == -1 || i > 255) {
		zc_error_prev_line("invalid base32 data");
	} else {
		buffer[0] = i; /* store length byte */
		r = alloc_rdata_init(region, buffer, i+1);
	}
	return r;
}

uint16_t *
zparser_conv_b64(region_type *region, const char *b64)
{
	uint8_t buffer[B64BUFSIZE];
	uint16_t *r = NULL;
	int i;

	i = __b64_pton(b64, buffer, B64BUFSIZE);
	if (i == -1) {
		zc_error_prev_line("invalid base64 data");
	} else {
		r = alloc_rdata_init(region, buffer, i);
	}
	return r;
}

uint16_t *
zparser_conv_rrtype(region_type *region, const char *text)
{
	uint16_t *r = NULL;
	uint16_t type = rrtype_from_string(text);

	if (type == 0) {
		zc_error_prev_line("unrecognized RR type '%s'", text);
	} else {
		type = htons(type);
		r = alloc_rdata_init(region, &type, sizeof(type));
	}
	return r;
}

uint16_t *
zparser_conv_nxt(region_type *region, uint8_t nxtbits[])
{
	/* nxtbits[] consists of 16 bytes with some zero's in it
	 * copy every byte with zero to r and write the length in
	 * the first byte
	 */
	uint16_t i;
	uint16_t last = 0;

	for (i = 0; i < 16; i++) {
		if (nxtbits[i] != 0)
			last = i + 1;
	}

	return alloc_rdata_init(region, nxtbits, last);
}


/* we potentially have 256 windows, each one is numbered. empty ones
 * should be discarded
 */
uint16_t *
zparser_conv_nsec(region_type *region,
		  uint8_t nsecbits[NSEC_WINDOW_COUNT][NSEC_WINDOW_BITS_SIZE])
{
	/* nsecbits contains up to 64K of bits which represent the
	 * types available for a name. Walk the bits according to
	 * nsec++ draft from jakob
	 */
	uint16_t *r;
	uint8_t *ptr;
	size_t i,j;
	uint16_t window_count = 0;
	uint16_t total_size = 0;
	uint16_t window_max = 0;

	/* The used windows.  */
	int used[NSEC_WINDOW_COUNT];
	/* The last byte used in each the window.  */
	int size[NSEC_WINDOW_COUNT];

	window_max = 1 + (nsec_highest_rcode / 256);

	/* used[i] is the i-th window included in the nsec
	 * size[used[0]] is the size of window 0
	 */

	/* walk through the 256 windows */
	for (i = 0; i < window_max; ++i) {
		int empty_window = 1;
		/* check each of the 32 bytes */
		for (j = 0; j < NSEC_WINDOW_BITS_SIZE; ++j) {
			if (nsecbits[i][j] != 0) {
				size[i] = j + 1;
				empty_window = 0;
			}
		}
		if (!empty_window) {
			used[window_count] = i;
			window_count++;
		}
	}

	for (i = 0; i < window_count; ++i) {
		total_size += sizeof(uint16_t) + size[used[i]];
	}

	r = alloc_rdata(region, total_size);
	ptr = (uint8_t *) (r + 1);

	/* now walk used and copy it */
	for (i = 0; i < window_count; ++i) {
		ptr[0] = used[i];
		ptr[1] = size[used[i]];
		memcpy(ptr + 2, &nsecbits[used[i]], size[used[i]]);
		ptr += size[used[i]] + 2;
	}

	return r;
}

/* Parse an int terminated in the specified range. */
static int
parse_int(const char *str,
	  char **end,
	  int *result,
	  const char *name,
	  int min,
	  int max)
{
	*result = (int) strtol(str, end, 10);
	if (*result < min || *result > max) {
		zc_error_prev_line("%s must be within the range [%d .. %d]",
				   name,
				   min,
				   max);
		return 0;
	} else {
		return 1;
	}
}

/* RFC1876 conversion routines */
static unsigned int poweroften[10] = {1, 10, 100, 1000, 10000, 100000,
				1000000,10000000,100000000,1000000000};

/*
 * Converts ascii size/precision X * 10**Y(cm) to 0xXY.
 * Sets the given pointer to the last used character.
 *
 */
static uint8_t
precsize_aton (char *cp, char **endptr)
{
	unsigned int mval = 0, cmval = 0;
	uint8_t retval = 0;
	int exponent;
	int mantissa;

	while (isdigit((unsigned char)*cp))
		mval = mval * 10 + hexdigit_to_int(*cp++);

	if (*cp == '.') {	/* centimeters */
		cp++;
		if (isdigit((unsigned char)*cp)) {
			cmval = hexdigit_to_int(*cp++) * 10;
			if (isdigit((unsigned char)*cp)) {
				cmval += hexdigit_to_int(*cp++);
			}
		}
	}

	if(mval >= poweroften[7]) {
		/* integer overflow possible for *100 */
		mantissa = mval / poweroften[7];
		exponent = 9; /* max */
	}
	else {
		cmval = (mval * 100) + cmval;

		for (exponent = 0; exponent < 9; exponent++)
			if (cmval < poweroften[exponent+1])
				break;

		mantissa = cmval / poweroften[exponent];
	}
	if (mantissa > 9)
		mantissa = 9;

	retval = (mantissa << 4) | exponent;

	if (*cp == 'm') cp++;

	*endptr = cp;

	return (retval);
}

/*
 * Parses a specific part of rdata.
 *
 * Returns:
 *
 *	number of elements parsed
 *	zero on error
 *
 */
uint16_t *
zparser_conv_loc(region_type *region, char *str)
{
	uint16_t *r;
	uint32_t *p;
	int i;
	int deg, min, secs;	/* Secs is stored times 1000.  */
	uint32_t lat = 0, lon = 0, alt = 0;
	/* encoded defaults: version=0 sz=1m hp=10000m vp=10m */
	uint8_t vszhpvp[4] = {0, 0x12, 0x16, 0x13};
	char *start;
	double d;

	for(;;) {
		deg = min = secs = 0;

		/* Degrees */
		if (*str == '\0') {
			zc_error_prev_line("unexpected end of LOC data");
			return NULL;
		}

		if (!parse_int(str, &str, &deg, "degrees", 0, 180))
			return NULL;
		if (!isspace((unsigned char)*str)) {
			zc_error_prev_line("space expected after degrees");
			return NULL;
		}
		++str;

		/* Minutes? */
		if (isdigit((unsigned char)*str)) {
			if (!parse_int(str, &str, &min, "minutes", 0, 60))
				return NULL;
			if (!isspace((unsigned char)*str)) {
				zc_error_prev_line("space expected after minutes");
				return NULL;
			}
			++str;
		}

		/* Seconds? */
		if (isdigit((unsigned char)*str)) {
			start = str;
			if (!parse_int(str, &str, &i, "seconds", 0, 60)) {
				return NULL;
			}

			if (*str == '.' && !parse_int(str + 1, &str, &i, "seconds fraction", 0, 999)) {
				return NULL;
			}

			if (!isspace((unsigned char)*str)) {
				zc_error_prev_line("space expected after seconds");
				return NULL;
			}
			/* No need for precision specifiers, it's a double */
			if (sscanf(start, "%lf", &d) != 1) {
				zc_error_prev_line("error parsing seconds");
			}

			if (d < 0.0 || d > 60.0) {
				zc_error_prev_line("seconds not in range 0.0 .. 60.0");
			}

			secs = (int) (d * 1000.0 + 0.5);
			++str;
		}

		switch(*str) {
		case 'N':
		case 'n':
			lat = ((uint32_t)1<<31) + (deg * 3600000 + min * 60000 + secs);
			break;
		case 'E':
		case 'e':
			lon = ((uint32_t)1<<31) + (deg * 3600000 + min * 60000 + secs);
			break;
		case 'S':
		case 's':
			lat = ((uint32_t)1<<31) - (deg * 3600000 + min * 60000 + secs);
			break;
		case 'W':
		case 'w':
			lon = ((uint32_t)1<<31) - (deg * 3600000 + min * 60000 + secs);
			break;
		default:
			zc_error_prev_line("invalid latitude/longtitude: '%c'", *str);
			return NULL;
		}
		++str;

		if (lat != 0 && lon != 0)
			break;

		if (!isspace((unsigned char)*str)) {
			zc_error_prev_line("space expected after latitude/longitude");
			return NULL;
		}
		++str;
	}

	/* Altitude */
	if (*str == '\0') {
		zc_error_prev_line("unexpected end of LOC data");
		return NULL;
	}

	if (!isspace((unsigned char)*str)) {
		zc_error_prev_line("space expected before altitude");
		return NULL;
	}
	++str;

	start = str;

	/* Sign */
	if (*str == '+' || *str == '-') {
		++str;
	}

	/* Meters of altitude... */
	if(strtol(str, &str, 10) == LONG_MAX) {
		zc_error_prev_line("altitude too large, number overflow");
		return NULL;
	}
	switch(*str) {
	case ' ':
	case '\0':
	case 'm':
		break;
	case '.':
		if (!parse_int(str + 1, &str, &i, "altitude fraction", 0, 99)) {
			return NULL;
		}
		if (!isspace((unsigned char)*str) && *str != '\0' && *str != 'm') {
			zc_error_prev_line("altitude fraction must be a number");
			return NULL;
		}
		break;
	default:
		zc_error_prev_line("altitude must be expressed in meters");
		return NULL;
	}
	if (!isspace((unsigned char)*str) && *str != '\0')
		++str;

	if (sscanf(start, "%lf", &d) != 1) {
		zc_error_prev_line("error parsing altitude");
	}

	alt = (uint32_t) (10000000.0 + d * 100 + 0.5);

	if (!isspace((unsigned char)*str) && *str != '\0') {
		zc_error_prev_line("unexpected character after altitude");
		return NULL;
	}

	/* Now parse size, horizontal precision and vertical precision if any */
	for(i = 1; isspace((unsigned char)*str) && i <= 3; i++) {
		vszhpvp[i] = precsize_aton(str + 1, &str);

		if (!isspace((unsigned char)*str) && *str != '\0') {
			zc_error_prev_line("invalid size or precision");
			return NULL;
		}
	}

	/* Allocate required space... */
	r = alloc_rdata(region, 16);
	p = (uint32_t *) (r + 1);

	memmove(p, vszhpvp, 4);
	write_uint32(p + 1, lat);
	write_uint32(p + 2, lon);
	write_uint32(p + 3, alt);

	return r;
}

/*
 * Convert an APL RR RDATA element.
 */
uint16_t *
zparser_conv_apl_rdata(region_type *region, char *str)
{
	int negated = 0;
	uint16_t address_family;
	uint8_t prefix;
	uint8_t maximum_prefix;
	uint8_t length;
	uint8_t address[IP6ADDRLEN];
	char *colon = strchr(str, ':');
	char *slash = strchr(str, '/');
	int af;
	int rc;
	uint16_t rdlength;
	uint16_t *r;
	uint8_t *t;
	char *end;
	long p;

	if (!colon) {
		zc_error("address family separator is missing");
		return NULL;
	}
	if (!slash) {
		zc_error("prefix separator is missing");
		return NULL;
	}

	*colon = '\0';
	*slash = '\0';

	if (*str == '!') {
		negated = 1;
		++str;
	}

	if (strcmp(str, "1") == 0) {
		address_family = htons(1);
		af = AF_INET;
		length = sizeof(in_addr_t);
		maximum_prefix = length * 8;
	} else if (strcmp(str, "2") == 0) {
		address_family = htons(2);
		af = AF_INET6;
		length = IP6ADDRLEN;
		maximum_prefix = length * 8;
	} else {
		zc_error("invalid address family '%s'", str);
		return NULL;
	}

	rc = inet_pton(af, colon + 1, address);
	if (rc == 0) {
		zc_error("invalid address '%s'", colon + 1);
		return NULL;
	} else if (rc == -1) {
		zc_error("inet_pton failed: %s", strerror(errno));
		return NULL;
	}

	/* Strip trailing zero octets.	*/
	while (length > 0 && address[length - 1] == 0)
		--length;


	p = strtol(slash + 1, &end, 10);
	if (p < 0 || p > maximum_prefix) {
		zc_error("prefix not in the range 0 .. %d", maximum_prefix);
		return NULL;
	} else if (*end != '\0') {
		zc_error("invalid prefix '%s'", slash + 1);
		return NULL;
	}
	prefix = (uint8_t) p;

	rdlength = (sizeof(address_family) + sizeof(prefix) + sizeof(length)
		    + length);
	r = alloc_rdata(region, rdlength);
	t = (uint8_t *) (r + 1);

	memcpy(t, &address_family, sizeof(address_family));
	t += sizeof(address_family);
	memcpy(t, &prefix, sizeof(prefix));
	t += sizeof(prefix);
	memcpy(t, &length, sizeof(length));
	if (negated) {
		*t |= APL_NEGATION_MASK;
	}
	t += sizeof(length);
	memcpy(t, address, length);

	return r;
}

/*
 * Below some function that also convert but not to wireformat
 * but to "normal" (int,long,char) types
 */

uint32_t
zparser_ttl2int(const char *ttlstr, int* error)
{
	/* convert a ttl value to a integer
	 * return the ttl in a int
	 * -1 on error
	 */

	uint32_t ttl;
	const char *t;

	ttl = strtottl(ttlstr, &t);
	if (*t != 0) {
		zc_error_prev_line("invalid TTL value: %s",ttlstr);
		*error = 1;
	}

	return ttl;
}


void
zadd_rdata_wireformat(uint16_t *data)
{
	if (parser->current_rr.rdata_count >= MAXRDATALEN) {
		zc_error_prev_line("too many rdata elements");
	} else {
		parser->current_rr.rdatas[parser->current_rr.rdata_count].data
			= data;
		++parser->current_rr.rdata_count;
	}
}

/**
 * Used for TXT RR's to grow with undefined number of strings.
 */
void
zadd_rdata_txt_wireformat(uint16_t *data, int first)
{
	rdata_atom_type *rd;
	if (parser->current_rr.rdata_count >= MAXRDATALEN) {
		zc_error_prev_line("too many rdata txt elements");
		return;
	}
	
	/* First STR in str_seq, allocate 65K in first unused rdata
	 * else find last used rdata */
	if (first) {
		rd = &parser->current_rr.rdatas[parser->current_rr.rdata_count];
		if ((rd->data = (uint16_t *) region_alloc(parser->rr_region,
			sizeof(uint16_t) + 65535 * sizeof(uint8_t))) == NULL) {
			zc_error_prev_line("Could not allocate memory for TXT RR");
			return;
		}
		parser->current_rr.rdata_count++;
		rd->data[0] = 0;
	}
	else
		rd = &parser->current_rr.rdatas[parser->current_rr.rdata_count-1];
	
	if ((size_t)rd->data[0] + (size_t)data[0] > 65535) {
		zc_error_prev_line("too large rdata element");
		return;
	}
	
	memcpy((uint8_t *)rd->data + 2 + rd->data[0], data + 1, data[0]);
	rd->data[0] += data[0];
}

/**
 * Clean up after last call of zadd_rdata_txt_wireformat
 */
void
zadd_rdata_txt_clean_wireformat()
{
	uint16_t *tmp_data;
	rdata_atom_type *rd = &parser->current_rr.rdatas[parser->current_rr.rdata_count-1];
	if(!rd || !rd->data)
		return; /* previous syntax failure */
	if ((tmp_data = (uint16_t *) region_alloc(parser->region, 
		((size_t)rd->data[0]) + ((size_t)2))) != NULL) {
		memcpy(tmp_data, rd->data, rd->data[0] + 2);
		/* rd->data of u16+65535 freed when rr_region is freed */
		rd->data = tmp_data;
	}
	else {
		/* We could not get memory in non-volatile region */
		zc_error_prev_line("could not allocate memory for rdata");
		return;
	}
}

void
zadd_rdata_domain(domain_type *domain)
{
	if (parser->current_rr.rdata_count >= MAXRDATALEN) {
		zc_error_prev_line("too many rdata elements");
	} else {
		parser->current_rr.rdatas[parser->current_rr.rdata_count].domain
			= domain;
		domain->usage ++; /* new reference to domain */
		++parser->current_rr.rdata_count;
	}
}

void
parse_unknown_rdata(uint16_t type, uint16_t *wireformat)
{
	buffer_type packet;
	uint16_t size;
	ssize_t rdata_count;
	ssize_t i;
	rdata_atom_type *rdatas;

	if (wireformat) {
		size = *wireformat;
	} else {
		return;
	}

	buffer_create_from(&packet, wireformat + 1, *wireformat);
	rdata_count = rdata_wireformat_to_rdata_atoms(parser->region,
						      parser->db->domains,
						      type,
						      size,
						      &packet,
						      &rdatas);
	if (rdata_count == -1) {
		zc_error_prev_line("bad unknown RDATA");
		return;
	}

	for (i = 0; i < rdata_count; ++i) {
		if (rdata_atom_is_domain(type, i)) {
			zadd_rdata_domain(rdatas[i].domain);
		} else {
			zadd_rdata_wireformat(rdatas[i].data);
		}
	}
}


/*
 * Compares two rdata arrays.
 *
 * Returns:
 *
 *	zero if they are equal
 *	non-zero if not
 *
 */
static int
zrdatacmp(uint16_t type, rr_type *a, rr_type *b)
{
	int i = 0;

	assert(a);
	assert(b);

	/* One is shorter than another */
	if (a->rdata_count != b->rdata_count)
		return 1;

	/* Compare element by element */
	for (i = 0; i < a->rdata_count; ++i) {
		if (rdata_atom_is_domain(type, i)) {
			if (rdata_atom_domain(a->rdatas[i])
			    != rdata_atom_domain(b->rdatas[i]))
			{
				return 1;
			}
		} else if(rdata_atom_is_literal_domain(type, i)) {
			if (rdata_atom_size(a->rdatas[i])
			    != rdata_atom_size(b->rdatas[i]))
				return 1;
			if (!dname_equal_nocase(rdata_atom_data(a->rdatas[i]),
				   rdata_atom_data(b->rdatas[i]),
				   rdata_atom_size(a->rdatas[i])))
				return 1;
		} else {
			if (rdata_atom_size(a->rdatas[i])
			    != rdata_atom_size(b->rdatas[i]))
			{
				return 1;
			}
			if (memcmp(rdata_atom_data(a->rdatas[i]),
				   rdata_atom_data(b->rdatas[i]),
				   rdata_atom_size(a->rdatas[i])) != 0)
			{
				return 1;
			}
		}
	}

	/* Otherwise they are equal */
	return 0;
}

/*
 *
 * Opens a zone file.
 *
 * Returns:
 *
 *	- pointer to the parser structure
 *	- NULL on error and errno set
 *
 */
static int
zone_open(const char *filename, uint32_t ttl, uint16_t klass,
	  const dname_type *origin)
{
	/* Open the zone file... */
	if (strcmp(filename, "-") == 0) {
		yyin = stdin;
		filename = "<stdin>";
	} else if (!(yyin = fopen(filename, "r"))) {
		return 0;
	}

	zparser_init(filename, ttl, klass, origin);

	return 1;
}


void
set_bitnsec(uint8_t bits[NSEC_WINDOW_COUNT][NSEC_WINDOW_BITS_SIZE],
	    uint16_t index)
{
	/*
	 * The bits are counted from left to right, so bit #0 is the
	 * left most bit.
	 */
	uint8_t window = index / 256;
	uint8_t bit = index % 256;

	bits[window][bit / 8] |= (1 << (7 - bit % 8));
}


static int
has_soa(domain_type* domain)
{
	rrset_type* p = NULL;
	if(!domain) return 0;
	for(p = domain->rrsets; p; p = p->next)
		if(rrset_rrtype(p) == TYPE_SOA)
			return 1;
	return 0;
}

int
process_rr(void)
{
	zone_type *zone = parser->current_zone;
	rr_type *rr = &parser->current_rr;
	rrset_type *rrset;
	size_t max_rdlength;
	int i;
	rrtype_descriptor_type *descriptor
		= rrtype_descriptor_by_type(rr->type);

	/* We only support IN class */
	if (rr->klass != CLASS_IN) {
		if(zone_is_slave(zone->opts))
			zc_warning_prev_line("only class IN is supported");
		else
			zc_error_prev_line("only class IN is supported");
		return 0;
	}

	/* Make sure the maximum RDLENGTH does not exceed 65535 bytes.	*/
	max_rdlength = rdata_maximum_wireformat_size(
		descriptor, rr->rdata_count, rr->rdatas);

	if (max_rdlength > MAX_RDLENGTH) {
		zc_error_prev_line("maximum rdata length exceeds %d octets", MAX_RDLENGTH);
		return 0;
	}
	/* we have the zone already */
	assert(zone);
	if (rr->type == TYPE_SOA) {
		if (rr->owner != zone->apex) {
			zc_error_prev_line(
				"SOA record with invalid domain name");
			return 0;
		}
		if(has_soa(rr->owner)) {
			if(zone_is_slave(zone->opts))
				zc_warning_prev_line("this SOA record was already encountered");
			else
				zc_error_prev_line("this SOA record was already encountered");
			return 0;
		}
		rr->owner->is_apex = 1;
	}

	if (!domain_is_subdomain(rr->owner, zone->apex))
	{
		if(zone_is_slave(zone->opts))
			zc_warning_prev_line("out of zone data");
		else
			zc_error_prev_line("out of zone data");
		return 0;
	}

	/* Do we have this type of rrset already? */
	rrset = domain_find_rrset(rr->owner, zone, rr->type);
	if (!rrset) {
		rrset = (rrset_type *) region_alloc(parser->region,
						    sizeof(rrset_type));
		rrset->zone = zone;
		rrset->rr_count = 1;
		rrset->rrs = (rr_type *) region_alloc(parser->region,
						      sizeof(rr_type));
		rrset->rrs[0] = *rr;

		/* Add it */
		domain_add_rrset(rr->owner, rrset);
	} else {
		rr_type* o;
		if (rr->type != TYPE_RRSIG && rrset->rrs[0].ttl != rr->ttl) {
			zc_warning_prev_line(
				"%s TTL %u does not match the TTL %u of the %s RRset",
				domain_to_string(rr->owner), (unsigned)rr->ttl,
				(unsigned)rrset->rrs[0].ttl,
				rrtype_to_string(rr->type));
		}

		/* Search for possible duplicates... */
		for (i = 0; i < rrset->rr_count; i++) {
			if (!zrdatacmp(rr->type, rr, &rrset->rrs[i])) {
				break;
			}
		}

		/* Discard the duplicates... */
		if (i < rrset->rr_count) {
			return 0;
		}
		if(rrset->rr_count == 65535) {
			zc_error_prev_line("too many RRs for domain RRset");
			return 0;
		}

		/* Add it... */
		o = rrset->rrs;
		rrset->rrs = (rr_type *) region_alloc_array(parser->region,
			(rrset->rr_count + 1), sizeof(rr_type));
		memcpy(rrset->rrs, o, (rrset->rr_count) * sizeof(rr_type));
		region_recycle(parser->region, o,
			(rrset->rr_count) * sizeof(rr_type));
		rrset->rrs[rrset->rr_count] = *rr;
		++rrset->rr_count;
	}

	if(rr->type == TYPE_DNAME && rrset->rr_count > 1) {
		if(zone_is_slave(zone->opts))
			zc_warning_prev_line("multiple DNAMEs at the same name");
		else
			zc_error_prev_line("multiple DNAMEs at the same name");
	}
	if(rr->type == TYPE_CNAME && rrset->rr_count > 1) {
		if(zone_is_slave(zone->opts))
			zc_warning_prev_line("multiple CNAMEs at the same name");
		else
			zc_error_prev_line("multiple CNAMEs at the same name");
	}
	if((rr->type == TYPE_DNAME && domain_find_rrset(rr->owner, zone, TYPE_CNAME))
	 ||(rr->type == TYPE_CNAME && domain_find_rrset(rr->owner, zone, TYPE_DNAME))) {
		if(zone_is_slave(zone->opts))
			zc_warning_prev_line("DNAME and CNAME at the same name");
		else
			zc_error_prev_line("DNAME and CNAME at the same name");
	}
	if(domain_find_rrset(rr->owner, zone, TYPE_CNAME) &&
		domain_find_non_cname_rrset(rr->owner, zone)) {
		if(zone_is_slave(zone->opts))
			zc_warning_prev_line("CNAME and other data at the same name");
		else
			zc_error_prev_line("CNAME and other data at the same name");
	}

	/* Check we have SOA */
	if(rr->owner == zone->apex)
		apex_rrset_checks(parser->db, rrset, rr->owner);

	if(parser->line % ZONEC_PCT_COUNT == 0 && time(NULL) > startzonec + ZONEC_PCT_TIME) {
		struct stat buf;
		startzonec = time(NULL);
		buf.st_size = 0;
		fstat(fileno(yyin), &buf);
		if(buf.st_size == 0) buf.st_size = 1;
		VERBOSITY(1, (LOG_INFO, "parse %s %d %%",
			parser->current_zone->opts->name,
			(int)((uint64_t)ftell(yyin)*(uint64_t)100/(uint64_t)buf.st_size)));
	}
	++totalrrs;
	return 1;
}

/*
 * Find rrset type for any zone
 */
static rrset_type*
domain_find_rrset_any(domain_type *domain, uint16_t type)
{
	rrset_type *result = domain->rrsets;
	while (result) {
		if (rrset_rrtype(result) == type) {
			return result;
		}
		result = result->next;
	}
	return NULL;
}

/*
 * Check for DNAME type. Nothing is allowed below it
 */
static void
check_dname(zone_type* zone)
{
	domain_type* domain;
	for(domain = zone->apex; domain && domain_is_subdomain(domain,
		zone->apex); domain=domain_next(domain))
	{
		if(domain->is_existing) {
			/* there may not be DNAMEs above it */
			domain_type* parent = domain->parent;
#ifdef NSEC3
			if(domain_has_only_NSEC3(domain, NULL))
				continue;
#endif
			while(parent) {
				if(domain_find_rrset_any(parent, TYPE_DNAME)) {
					zc_error("While checking node %s,",
						domain_to_string(domain));
					zc_error("DNAME at %s has data below it. "
						"This is not allowed (rfc 2672).",
						domain_to_string(parent));
					return;
				}
				parent = parent->parent;
			}
		}
	}
}

/*
 * Reads the specified zone into the memory
 * nsd_options can be NULL if no config file is passed.
 */
unsigned int
zonec_read(const char* name, const char* zonefile, zone_type* zone)
{
	const dname_type *dname;

	totalrrs = 0;
	startzonec = time(NULL);
	parser->errors = 0;

	dname = dname_parse(parser->rr_region, name);
	if (!dname) {
		zc_error("incorrect zone name '%s'", name);
		return 1;
	}

#ifndef ROOT_SERVER
	/* Is it a root zone? Are we a root server then? Idiot proof. */
	if (dname->label_count == 1) {
		zc_error("not configured as a root server");
		return 1;
	}
#endif

	/* Open the zone file */
	if (!zone_open(zonefile, 3600, CLASS_IN, dname)) {
		zc_error("cannot open '%s': %s", zonefile, strerror(errno));
		return 1;
	}
	parser->current_zone = zone;

	/* Parse and process all RRs.  */
	yyparse();

	/* remove origin if it was unused */
	if(parser->origin != error_domain)
		domain_table_deldomain(parser->db, parser->origin);
	/* rr_region has been emptied by now */
	dname = dname_parse(parser->rr_region, name);

	/* check if zone file contained a correct SOA record */
	if (!parser->current_zone) {
		zc_error("zone configured as '%s' has no content.", name);
	} else if(!parser->current_zone->soa_rrset ||
		parser->current_zone->soa_rrset->rr_count == 0) {
		zc_error("zone configured as '%s' has no SOA record.", name);
	} else if(dname_compare(domain_dname(
		parser->current_zone->soa_rrset->rrs[0].owner), dname) != 0) {
		zc_error("zone configured as '%s', but SOA has owner '%s'.",
			name, domain_to_string(
			parser->current_zone->soa_rrset->rrs[0].owner));
	}

	fclose(yyin);
	if(!zone_is_slave(zone->opts))
		check_dname(zone);

	parser->filename = NULL;
	return parser->errors;
}


/*
 * setup parse
 */
void
zonec_setup_parser(namedb_type* db)
{
	region_type* rr_region = region_create(xalloc, free);
	parser = zparser_create(db->region, rr_region, db);
	assert(parser);
	/* Unique pointers used to mark errors.	 */
	error_dname = (dname_type *) region_alloc(db->region, 1);
	error_domain = (domain_type *) region_alloc(db->region, 1);
	/* Open the network database */
	setprotoent(1);
	setservent(1);
}

/** desetup parse */
void
zonec_desetup_parser(void)
{
	if(parser) {
		endservent();
		endprotoent();
		region_destroy(parser->rr_region);
		/* removed when parser->region(=db->region) is destroyed:
		 * region_recycle(parser->region, (void*)error_dname, 1);
		 * region_recycle(parser->region, (void*)error_domain, 1); */
		/* clear memory for exit, but this is not portable to
		 * other versions of lex. yylex_destroy(); */
	}
}

static domain_table_type* orig_domains = NULL;
static region_type* orig_region = NULL;
static region_type* orig_dbregion = NULL;

/** setup for string parse */
void
zonec_setup_string_parser(region_type* region, domain_table_type* domains)
{
	assert(parser); /* global parser must be setup */
	orig_domains = parser->db->domains;
	orig_region = parser->region;
	orig_dbregion = parser->db->region;
	parser->region = region;
	parser->db->region = region;
	parser->db->domains = domains;
	zparser_init("string", 3600, CLASS_IN, domain_dname(domains->root));
}

/** desetup string parse */
void
zonec_desetup_string_parser(void)
{
	parser->region = orig_region;
	parser->db->domains = orig_domains;
	parser->db->region = orig_dbregion;
}

/** parse a string into temporary storage */
int
zonec_parse_string(region_type* region, domain_table_type* domains,
	zone_type* zone, char* str, domain_type** parsed, int* num_rrs)
{
	int errors;
	zonec_setup_string_parser(region, domains);
	parser->current_zone = zone;
	parser->errors = 0;
	totalrrs = 0;
	startzonec = time(NULL)+100000; /* disable */
	parser_push_stringbuf(str);
	yyparse();
	parser_pop_stringbuf();
	errors = parser->errors;
	*num_rrs = totalrrs;
	if(*num_rrs == 0)
		*parsed = NULL;
	else	*parsed = parser->prev_dname;
	/* remove origin if it was not used during the parse */
	if(parser->origin != error_domain)
		domain_table_deldomain(parser->db, parser->origin);
	zonec_desetup_string_parser();
	return errors;
}
