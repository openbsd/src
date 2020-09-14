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

/* $Id: name.c,v 1.14 2020/09/14 08:40:43 florian Exp $ */

/*! \file */
#include <ctype.h>
#include <stdlib.h>
#include <isc/buffer.h>
#include <isc/hash.h>

#include <string.h>
#include <isc/util.h>

#include <dns/compress.h>
#include <dns/fixedname.h>
#include <dns/name.h>
#include <dns/result.h>

typedef enum {
	ft_init = 0,
	ft_start,
	ft_ordinary,
	ft_initialescape,
	ft_escape,
	ft_escdecimal,
	ft_at
} ft_state;

typedef enum {
	fw_start = 0,
	fw_ordinary,
	fw_newcurrent
} fw_state;

static char digitvalue[256] = {
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,	/*16*/
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /*32*/
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /*48*/
	 0,  1,  2,  3,  4,  5,  6,  7,  8,  9, -1, -1, -1, -1, -1, -1, /*64*/
	-1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, /*80*/
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /*96*/
	-1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, /*112*/
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /*128*/
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /*256*/
};

static unsigned char maptolower[] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
	0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
	0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
	0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
	0x40, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
	0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
	0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
	0x78, 0x79, 0x7a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
	0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
	0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
	0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
	0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f,
	0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
	0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
	0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
	0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
	0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
	0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
	0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7,
	0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
	0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,
	0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
	0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7,
	0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
	0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7,
	0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
	0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
	0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
};

#define CONVERTTOASCII(c)
#define CONVERTFROMASCII(c)

#define INIT_OFFSETS(name, var, default_offsets) \
	if ((name)->offsets != NULL)		 \
		var = (name)->offsets;		 \
	else \
		var = (default_offsets);

#define SETUP_OFFSETS(name, var, default_offsets) \
	if ((name)->offsets != NULL)		  \
		var = (name)->offsets;		  \
	else { \
		var = (default_offsets);      \
		set_offsets(name, var, NULL); \
	}

/*%
 * Note:  If additional attributes are added that should not be set for
 *	  empty names, MAKE_EMPTY() must be changed so it clears them.
 */
#define MAKE_EMPTY(name) \
do { \
	name->ndata = NULL; \
	name->length = 0; \
	name->labels = 0; \
	name->attributes &= ~DNS_NAMEATTR_ABSOLUTE; \
} while (0);

/*%
 * A name is "bindable" if it can be set to point to a new value, i.e.
 * name->ndata and name->length may be changed.
 */
#define BINDABLE(name) \
	((name->attributes & (DNS_NAMEATTR_READONLY|DNS_NAMEATTR_DYNAMIC)) \
	 == 0)

/*%
 * Note that the name data must be a char array, not a string
 * literal, to avoid compiler warnings about discarding
 * the const attribute of a string.
 */
static unsigned char root_ndata[] = { "" };
static unsigned char root_offsets[] = { 0 };

static dns_name_t root = DNS_NAME_INITABSOLUTE(root_ndata, root_offsets);

/* XXXDCL make const? */
dns_name_t *dns_rootname = &root;

static void
set_offsets(const dns_name_t *name, unsigned char *offsets,
	    dns_name_t *set_name);

void
dns_name_init(dns_name_t *name, unsigned char *offsets) {
	/*
	 * Initialize 'name'.
	 */
	name->ndata = NULL;
	name->length = 0;
	name->labels = 0;
	name->attributes = 0;
	name->offsets = offsets;
	name->buffer = NULL;
	ISC_LINK_INIT(name, link);
	ISC_LIST_INIT(name->list);

}

void
dns_name_reset(dns_name_t *name) {
	REQUIRE(BINDABLE(name));

	name->ndata = NULL;
	name->length = 0;
	name->labels = 0;
	name->attributes &= ~DNS_NAMEATTR_ABSOLUTE;
	if (name->buffer != NULL)
		isc_buffer_clear(name->buffer);
}

void
dns_name_invalidate(dns_name_t *name) {
	/*
	 * Make 'name' invalid.
	 */

	name->ndata = NULL;
	name->length = 0;
	name->labels = 0;
	name->attributes = 0;
	name->offsets = NULL;
	name->buffer = NULL;
	ISC_LINK_INIT(name, link);
}

void
dns_name_setbuffer(dns_name_t *name, isc_buffer_t *buffer) {
	/*
	 * Dedicate a buffer for use with 'name'.
	 */

	REQUIRE((buffer != NULL && name->buffer == NULL) ||
		(buffer == NULL));

	name->buffer = buffer;
}

int
dns_name_isabsolute(const dns_name_t *name) {

	/*
	 * Does 'name' end in the root label?
	 */

	if ((name->attributes & DNS_NAMEATTR_ABSOLUTE) != 0)
		return (1);
	return (0);
}

unsigned int
dns_name_hash(dns_name_t *name, int case_sensitive) {
	unsigned int length;

	/*
	 * Provide a hash value for 'name'.
	 */

	if (name->labels == 0)
		return (0);

	length = name->length;
	if (length > 16)
		length = 16;

	return (isc_hash_function_reverse(name->ndata, length,
					  case_sensitive, NULL));
}

dns_namereln_t
dns_name_fullcompare(const dns_name_t *name1, const dns_name_t *name2,
		     int *orderp, unsigned int *nlabelsp)
{
	unsigned int l1, l2, l, count1, count2, count, nlabels;
	int cdiff, ldiff, chdiff;
	unsigned char *label1, *label2;
	unsigned char *offsets1, *offsets2;
	dns_offsets_t odata1, odata2;
	dns_namereln_t namereln = dns_namereln_none;

	/*
	 * Determine the relative ordering under the DNSSEC order relation of
	 * 'name1' and 'name2', and also determine the hierarchical
	 * relationship of the names.
	 *
	 * Note: It makes no sense for one of the names to be relative and the
	 * other absolute.  If both names are relative, then to be meaningfully
	 * compared the caller must ensure that they are both relative to the
	 * same domain.
	 */

	REQUIRE(orderp != NULL);
	REQUIRE(nlabelsp != NULL);
	/*
	 * Either name1 is absolute and name2 is absolute, or neither is.
	 */
	REQUIRE((name1->attributes & DNS_NAMEATTR_ABSOLUTE) ==
		(name2->attributes & DNS_NAMEATTR_ABSOLUTE));

	if (name1 == name2) {
		*orderp = 0;
		*nlabelsp = name1->labels;
		return (dns_namereln_equal);
	}

	SETUP_OFFSETS(name1, offsets1, odata1);
	SETUP_OFFSETS(name2, offsets2, odata2);

	nlabels = 0;
	l1 = name1->labels;
	l2 = name2->labels;
	if (l2 > l1) {
		l = l1;
		ldiff = 0 - (l2 - l1);
	} else {
		l = l2;
		ldiff = l1 - l2;
	}

	offsets1 += l1;
	offsets2 += l2;

	while (l > 0) {
		l--;
		offsets1--;
		offsets2--;
		label1 = &name1->ndata[*offsets1];
		label2 = &name2->ndata[*offsets2];
		count1 = *label1++;
		count2 = *label2++;

		/*
		 * We dropped bitstring labels, and we don't support any
		 * other extended label types.
		 */
		INSIST(count1 <= 63 && count2 <= 63);

		cdiff = (int)count1 - (int)count2;
		if (cdiff < 0)
			count = count1;
		else
			count = count2;

		/* Loop unrolled for performance */
		while (count > 3) {
			chdiff = (int)maptolower[label1[0]] -
				 (int)maptolower[label2[0]];
			if (chdiff != 0) {
				*orderp = chdiff;
				goto done;
			}
			chdiff = (int)maptolower[label1[1]] -
				 (int)maptolower[label2[1]];
			if (chdiff != 0) {
				*orderp = chdiff;
				goto done;
			}
			chdiff = (int)maptolower[label1[2]] -
				 (int)maptolower[label2[2]];
			if (chdiff != 0) {
				*orderp = chdiff;
				goto done;
			}
			chdiff = (int)maptolower[label1[3]] -
				 (int)maptolower[label2[3]];
			if (chdiff != 0) {
				*orderp = chdiff;
				goto done;
			}
			count -= 4;
			label1 += 4;
			label2 += 4;
		}
		while (count-- > 0) {
			chdiff = (int)maptolower[*label1++] -
				 (int)maptolower[*label2++];
			if (chdiff != 0) {
				*orderp = chdiff;
				goto done;
			}
		}
		if (cdiff != 0) {
			*orderp = cdiff;
			goto done;
		}
		nlabels++;
	}

	*orderp = ldiff;
	if (ldiff < 0)
		namereln = dns_namereln_contains;
	else if (ldiff > 0)
		namereln = dns_namereln_subdomain;
	else
		namereln = dns_namereln_equal;
	*nlabelsp = nlabels;
	return (namereln);

 done:
	*nlabelsp = nlabels;
	if (nlabels > 0)
		namereln = dns_namereln_commonancestor;

	return (namereln);
}

int
dns_name_compare(const dns_name_t *name1, const dns_name_t *name2) {
	int order;
	unsigned int nlabels;

	/*
	 * Determine the relative ordering under the DNSSEC order relation of
	 * 'name1' and 'name2'.
	 *
	 * Note: It makes no sense for one of the names to be relative and the
	 * other absolute.  If both names are relative, then to be meaningfully
	 * compared the caller must ensure that they are both relative to the
	 * same domain.
	 */

	(void)dns_name_fullcompare(name1, name2, &order, &nlabels);

	return (order);
}

int
dns_name_equal(const dns_name_t *name1, const dns_name_t *name2) {
	unsigned int l, count;
	unsigned char c;
	unsigned char *label1, *label2;

	/*
	 * Are 'name1' and 'name2' equal?
	 *
	 * Note: It makes no sense for one of the names to be relative and the
	 * other absolute.  If both names are relative, then to be meaningfully
	 * compared the caller must ensure that they are both relative to the
	 * same domain.
	 */

	/*
	 * Either name1 is absolute and name2 is absolute, or neither is.
	 */
	REQUIRE((name1->attributes & DNS_NAMEATTR_ABSOLUTE) ==
		(name2->attributes & DNS_NAMEATTR_ABSOLUTE));

	if (name1 == name2)
		return (1);

	if (name1->length != name2->length)
		return (0);

	l = name1->labels;

	if (l != name2->labels)
		return (0);

	label1 = name1->ndata;
	label2 = name2->ndata;
	while (l-- > 0) {
		count = *label1++;
		if (count != *label2++)
			return (0);

		INSIST(count <= 63); /* no bitstring support */

		/* Loop unrolled for performance */
		while (count > 3) {
			c = maptolower[label1[0]];
			if (c != maptolower[label2[0]])
				return (0);
			c = maptolower[label1[1]];
			if (c != maptolower[label2[1]])
				return (0);
			c = maptolower[label1[2]];
			if (c != maptolower[label2[2]])
				return (0);
			c = maptolower[label1[3]];
			if (c != maptolower[label2[3]])
				return (0);
			count -= 4;
			label1 += 4;
			label2 += 4;
		}
		while (count-- > 0) {
			c = maptolower[*label1++];
			if (c != maptolower[*label2++])
				return (0);
		}
	}

	return (1);
}

int
dns_name_caseequal(const dns_name_t *name1, const dns_name_t *name2) {

	/*
	 * Are 'name1' and 'name2' equal?
	 *
	 * Note: It makes no sense for one of the names to be relative and the
	 * other absolute.  If both names are relative, then to be meaningfully
	 * compared the caller must ensure that they are both relative to the
	 * same domain.
	 */

	/*
	 * Either name1 is absolute and name2 is absolute, or neither is.
	 */
	REQUIRE((name1->attributes & DNS_NAMEATTR_ABSOLUTE) ==
		(name2->attributes & DNS_NAMEATTR_ABSOLUTE));

	if (name1->length != name2->length)
		return (0);

	if (memcmp(name1->ndata, name2->ndata, name1->length) != 0)
		return (0);

	return (1);
}

int
dns_name_issubdomain(const dns_name_t *name1, const dns_name_t *name2) {
	int order;
	unsigned int nlabels;
	dns_namereln_t namereln;

	/*
	 * Is 'name1' a subdomain of 'name2'?
	 *
	 * Note: It makes no sense for one of the names to be relative and the
	 * other absolute.  If both names are relative, then to be meaningfully
	 * compared the caller must ensure that they are both relative to the
	 * same domain.
	 */

	namereln = dns_name_fullcompare(name1, name2, &order, &nlabels);
	if (namereln == dns_namereln_subdomain ||
	    namereln == dns_namereln_equal)
		return (1);

	return (0);
}

unsigned int
dns_name_countlabels(const dns_name_t *name) {
	/*
	 * How many labels does 'name' have?
	 */

	ENSURE(name->labels <= 128);

	return (name->labels);
}

void
dns_name_getlabel(const dns_name_t *name, unsigned int n, dns_label_t *label) {
	unsigned char *offsets;
	dns_offsets_t odata;

	/*
	 * Make 'label' refer to the 'n'th least significant label of 'name'.
	 */

	REQUIRE(name->labels > 0);
	REQUIRE(n < name->labels);
	REQUIRE(label != NULL);

	SETUP_OFFSETS(name, offsets, odata);

	label->base = &name->ndata[offsets[n]];
	if (n == name->labels - 1)
		label->length = name->length - offsets[n];
	else
		label->length = offsets[n + 1] - offsets[n];
}

void
dns_name_getlabelsequence(const dns_name_t *source,
			  unsigned int first, unsigned int n,
			  dns_name_t *target)
{
	unsigned char *offsets;
	dns_offsets_t odata;
	unsigned int firstoffset, endoffset;

	/*
	 * Make 'target' refer to the 'n' labels including and following
	 * 'first' in 'source'.
	 */

	REQUIRE(first <= source->labels);
	REQUIRE(n <= source->labels - first); /* note first+n could overflow */
	REQUIRE(BINDABLE(target));

	SETUP_OFFSETS(source, offsets, odata);

	if (first == source->labels)
		firstoffset = source->length;
	else
		firstoffset = offsets[first];

	if (first + n == source->labels)
		endoffset = source->length;
	else
		endoffset = offsets[first + n];

	target->ndata = &source->ndata[firstoffset];
	target->length = endoffset - firstoffset;

	if (first + n == source->labels && n > 0 &&
	    (source->attributes & DNS_NAMEATTR_ABSOLUTE) != 0)
		target->attributes |= DNS_NAMEATTR_ABSOLUTE;
	else
		target->attributes &= ~DNS_NAMEATTR_ABSOLUTE;

	target->labels = n;

	/*
	 * If source and target are the same, and we're making target
	 * a prefix of source, the offsets table is correct already
	 * so we don't need to call set_offsets().
	 */
	if (target->offsets != NULL &&
	    (target != source || first != 0))
		set_offsets(target, target->offsets, NULL);
}

void
dns_name_clone(const dns_name_t *source, dns_name_t *target) {

	/*
	 * Make 'target' refer to the same name as 'source'.
	 */

	REQUIRE(BINDABLE(target));

	target->ndata = source->ndata;
	target->length = source->length;
	target->labels = source->labels;
	target->attributes = source->attributes &
		(unsigned int)~(DNS_NAMEATTR_READONLY | DNS_NAMEATTR_DYNAMIC |
				DNS_NAMEATTR_DYNOFFSETS);
	if (target->offsets != NULL && source->labels > 0) {
		if (source->offsets != NULL)
			memmove(target->offsets, source->offsets,
				source->labels);
		else
			set_offsets(target, target->offsets, NULL);
	}
}

void
dns_name_fromregion(dns_name_t *name, const isc_region_t *r) {
	unsigned char *offsets;
	dns_offsets_t odata;
	unsigned int len;
	isc_region_t r2;

	/*
	 * Make 'name' refer to region 'r'.
	 */

	REQUIRE(r != NULL);
	REQUIRE(BINDABLE(name));

	INIT_OFFSETS(name, offsets, odata);

	if (name->buffer != NULL) {
		isc_buffer_clear(name->buffer);
		isc_buffer_availableregion(name->buffer, &r2);
		len = (r->length < r2.length) ? r->length : r2.length;
		if (len > DNS_NAME_MAXWIRE)
			len = DNS_NAME_MAXWIRE;
		if (len != 0)
			memmove(r2.base, r->base, len);
		name->ndata = r2.base;
		name->length = len;
	} else {
		name->ndata = r->base;
		name->length = (r->length <= DNS_NAME_MAXWIRE) ?
			r->length : DNS_NAME_MAXWIRE;
	}

	if (r->length > 0)
		set_offsets(name, offsets, name);
	else {
		name->labels = 0;
		name->attributes &= ~DNS_NAMEATTR_ABSOLUTE;
	}

	if (name->buffer != NULL)
		isc_buffer_add(name->buffer, name->length);
}

void
dns_name_toregion(dns_name_t *name, isc_region_t *r) {
	/*
	 * Make 'r' refer to 'name'.
	 */

	REQUIRE(r != NULL);

	r->base = name->ndata;
	r->length = name->length;
}

isc_result_t
dns_name_fromtext(dns_name_t *name, isc_buffer_t *source,
		  const dns_name_t *origin, unsigned int options,
		  isc_buffer_t *target)
{
	unsigned char *ndata, *label = NULL;
	char *tdata;
	char c;
	ft_state state;
	unsigned int value = 0, count = 0;
	unsigned int n1 = 0, n2 = 0;
	unsigned int tlen, nrem, nused, digits = 0, labels, tused;
	int done;
	unsigned char *offsets;
	dns_offsets_t odata;
	int downcase;

	/*
	 * Convert the textual representation of a DNS name at source
	 * into uncompressed wire form stored in target.
	 *
	 * Notes:
	 *	Relative domain names will have 'origin' appended to them
	 *	unless 'origin' is NULL, in which case relative domain names
	 *	will remain relative.
	 */

	downcase = (options & DNS_NAME_DOWNCASE) != 0;

	if (target == NULL && name->buffer != NULL) {
		target = name->buffer;
		isc_buffer_clear(target);
	}

	REQUIRE(BINDABLE(name));

	INIT_OFFSETS(name, offsets, odata);
	offsets[0] = 0;

	/*
	 * Make 'name' empty in case of failure.
	 */
	MAKE_EMPTY(name);

	/*
	 * Set up the state machine.
	 */
	tdata = (char *)source->base + source->current;
	tlen = isc_buffer_remaininglength(source);
	tused = 0;
	ndata = isc_buffer_used(target);
	nrem = isc_buffer_availablelength(target);
	if (nrem > 255)
		nrem = 255;
	nused = 0;
	labels = 0;
	done = 0;
	state = ft_init;

	while (nrem > 0 && tlen > 0 && !done) {
		c = *tdata++;
		tlen--;
		tused++;

		switch (state) {
		case ft_init:
			/*
			 * Is this the root name?
			 */
			if (c == '.') {
				if (tlen != 0)
					return (DNS_R_EMPTYLABEL);
				labels++;
				*ndata++ = 0;
				nrem--;
				nused++;
				done = 1;
				break;
			}
			if (c == '@' && tlen == 0) {
				state = ft_at;
				break;
			}

			/* FALLTHROUGH */
		case ft_start:
			label = ndata;
			ndata++;
			nrem--;
			nused++;
			count = 0;
			if (c == '\\') {
				state = ft_initialescape;
				break;
			}
			state = ft_ordinary;
			if (nrem == 0)
				return (ISC_R_NOSPACE);
			/* FALLTHROUGH */
		case ft_ordinary:
			if (c == '.') {
				if (count == 0)
					return (DNS_R_EMPTYLABEL);
				*label = count;
				labels++;
				INSIST(labels <= 127);
				offsets[labels] = nused;
				if (tlen == 0) {
					labels++;
					*ndata++ = 0;
					nrem--;
					nused++;
					done = 1;
				}
				state = ft_start;
			} else if (c == '\\') {
				state = ft_escape;
			} else {
				if (count >= 63)
					return (DNS_R_LABELTOOLONG);
				count++;
				CONVERTTOASCII(c);
				if (downcase)
					c = maptolower[c & 0xff];
				*ndata++ = c;
				nrem--;
				nused++;
			}
			break;
		case ft_initialescape:
			if (c == '[') {
				/*
				 * This looks like a bitstring label, which
				 * was deprecated.  Intentionally drop it.
				 */
				return (DNS_R_BADLABELTYPE);
			}
			state = ft_escape;
			POST(state);
			/* FALLTHROUGH */
		case ft_escape:
			if (!isdigit(c & 0xff)) {
				if (count >= 63)
					return (DNS_R_LABELTOOLONG);
				count++;
				CONVERTTOASCII(c);
				if (downcase)
					c = maptolower[c & 0xff];
				*ndata++ = c;
				nrem--;
				nused++;
				state = ft_ordinary;
				break;
			}
			digits = 0;
			value = 0;
			state = ft_escdecimal;
			/* FALLTHROUGH */
		case ft_escdecimal:
			if (!isdigit(c & 0xff))
				return (DNS_R_BADESCAPE);
			value *= 10;
			value += digitvalue[c & 0xff];
			digits++;
			if (digits == 3) {
				if (value > 255)
					return (DNS_R_BADESCAPE);
				if (count >= 63)
					return (DNS_R_LABELTOOLONG);
				count++;
				if (downcase)
					value = maptolower[value];
				*ndata++ = value;
				nrem--;
				nused++;
				state = ft_ordinary;
			}
			break;
		default:
			FATAL_ERROR(__FILE__, __LINE__,
				    "Unexpected state %d", state);
			/* Does not return. */
		}
	}

	if (!done) {
		if (nrem == 0)
			return (ISC_R_NOSPACE);
		INSIST(tlen == 0);
		if (state != ft_ordinary && state != ft_at)
			return (ISC_R_UNEXPECTEDEND);
		if (state == ft_ordinary) {
			INSIST(count != 0);
			*label = count;
			labels++;
			INSIST(labels <= 127);
			offsets[labels] = nused;
		}
		if (origin != NULL) {
			if (nrem < origin->length)
				return (ISC_R_NOSPACE);
			label = origin->ndata;
			n1 = origin->length;
			nrem -= n1;
			POST(nrem);
			while (n1 > 0) {
				n2 = *label++;
				INSIST(n2 <= 63); /* no bitstring support */
				*ndata++ = n2;
				n1 -= n2 + 1;
				nused += n2 + 1;
				while (n2 > 0) {
					c = *label++;
					if (downcase)
						c = maptolower[c & 0xff];
					*ndata++ = c;
					n2--;
				}
				labels++;
				if (n1 > 0) {
					INSIST(labels <= 127);
					offsets[labels] = nused;
				}
			}
			if ((origin->attributes & DNS_NAMEATTR_ABSOLUTE) != 0)
				name->attributes |= DNS_NAMEATTR_ABSOLUTE;
		}
	} else
		name->attributes |= DNS_NAMEATTR_ABSOLUTE;

	name->ndata = (unsigned char *)target->base + target->used;
	name->labels = labels;
	name->length = nused;

	isc_buffer_forward(source, tused);
	isc_buffer_add(target, name->length);

	return (ISC_R_SUCCESS);
}

isc_result_t
dns_name_totext(dns_name_t *name, int omit_final_dot,
		isc_buffer_t *target)
{
	unsigned int options = DNS_NAME_MASTERFILE;

	if (omit_final_dot)
		options |= DNS_NAME_OMITFINALDOT;
	return (dns_name_totext2(name, options, target));
}

isc_result_t
dns_name_totext2(dns_name_t *name, unsigned int options, isc_buffer_t *target)
{
	unsigned char *ndata;
	char *tdata;
	unsigned int nlen, tlen;
	unsigned char c;
	unsigned int trem, count;
	unsigned int labels;
	int saw_root = 0;
	int omit_final_dot = options & DNS_NAME_OMITFINALDOT;

	/*
	 * This function assumes the name is in proper uncompressed
	 * wire format.
	 */

	ndata = name->ndata;
	nlen = name->length;
	labels = name->labels;
	tdata = isc_buffer_used(target);
	tlen = isc_buffer_availablelength(target);

	trem = tlen;

	if (labels == 0 && nlen == 0) {
		/*
		 * Special handling for an empty name.
		 */
		if (trem == 0)
			return (ISC_R_NOSPACE);

		/*
		 * The names of these booleans are misleading in this case.
		 * This empty name is not necessarily from the root node of
		 * the DNS root zone, nor is a final dot going to be included.
		 * They need to be set this way, though, to keep the "@"
		 * from being trounced.
		 */
		saw_root = 1;
		omit_final_dot = 0;
		*tdata++ = '@';
		trem--;

		/*
		 * Skip the while() loop.
		 */
		nlen = 0;
	} else if (nlen == 1 && labels == 1 && *ndata == '\0') {
		/*
		 * Special handling for the root label.
		 */
		if (trem == 0)
			return (ISC_R_NOSPACE);

		saw_root = 1;
		omit_final_dot = 0;
		*tdata++ = '.';
		trem--;

		/*
		 * Skip the while() loop.
		 */
		nlen = 0;
	}

	while (labels > 0 && nlen > 0 && trem > 0) {
		labels--;
		count = *ndata++;
		nlen--;
		if (count == 0) {
			saw_root = 1;
			break;
		}
		if (count < 64) {
			INSIST(nlen >= count);
			while (count > 0) {
				c = *ndata;
				switch (c) {
				/* Special modifiers in zone files. */
				case 0x40: /* '@' */
				case 0x24: /* '$' */
					if ((options & DNS_NAME_MASTERFILE) == 0)
						goto no_escape;
					/* FALLTHROUGH */
				case 0x22: /* '"' */
				case 0x28: /* '(' */
				case 0x29: /* ')' */
				case 0x2E: /* '.' */
				case 0x3B: /* ';' */
				case 0x5C: /* '\\' */
					if (trem < 2)
						return (ISC_R_NOSPACE);
					*tdata++ = '\\';
					CONVERTFROMASCII(c);
					*tdata++ = c;
					ndata++;
					trem -= 2;
					nlen--;
					break;
				no_escape:
				default:
					if (c > 0x20 && c < 0x7f) {
						if (trem == 0)
							return (ISC_R_NOSPACE);
						CONVERTFROMASCII(c);
						*tdata++ = c;
						ndata++;
						trem--;
						nlen--;
					} else {
						if (trem < 4)
							return (ISC_R_NOSPACE);
						*tdata++ = 0x5c;
						*tdata++ = 0x30 +
							   ((c / 100) % 10);
						*tdata++ = 0x30 +
							   ((c / 10) % 10);
						*tdata++ = 0x30 + (c % 10);
						trem -= 4;
						ndata++;
						nlen--;
					}
				}
				count--;
			}
		} else {
			FATAL_ERROR(__FILE__, __LINE__,
				    "Unexpected label type %02x", count);
			/* NOTREACHED */
		}

		/*
		 * The following assumes names are absolute.  If not, we
		 * fix things up later.  Note that this means that in some
		 * cases one more byte of text buffer is required than is
		 * needed in the final output.
		 */
		if (trem == 0)
			return (ISC_R_NOSPACE);
		*tdata++ = '.';
		trem--;
	}

	if (nlen != 0 && trem == 0)
		return (ISC_R_NOSPACE);

	if (!saw_root || omit_final_dot)
		trem++;

	isc_buffer_add(target, tlen - trem);

	return (ISC_R_SUCCESS);
}

isc_result_t
dns_name_downcase(dns_name_t *source, dns_name_t *name, isc_buffer_t *target) {
	unsigned char *sndata, *ndata;
	unsigned int nlen, count, labels;
	isc_buffer_t buffer;

	/*
	 * Downcase 'source'.
	 */

	if (source == name) {
		REQUIRE((name->attributes & DNS_NAMEATTR_READONLY) == 0);
		isc_buffer_init(&buffer, source->ndata, source->length);
		target = &buffer;
		ndata = source->ndata;
	} else {
		REQUIRE(BINDABLE(name));
		if (target == NULL) {
			target = name->buffer;
			isc_buffer_clear(name->buffer);
		}
		ndata = (unsigned char *)target->base + target->used;
		name->ndata = ndata;
	}

	sndata = source->ndata;
	nlen = source->length;
	labels = source->labels;

	if (nlen > (target->length - target->used)) {
		MAKE_EMPTY(name);
		return (ISC_R_NOSPACE);
	}

	while (labels > 0 && nlen > 0) {
		labels--;
		count = *sndata++;
		*ndata++ = count;
		nlen--;
		if (count < 64) {
			INSIST(nlen >= count);
			while (count > 0) {
				*ndata++ = maptolower[(*sndata++)];
				nlen--;
				count--;
			}
		} else {
			FATAL_ERROR(__FILE__, __LINE__,
				    "Unexpected label type %02x", count);
			/* Does not return. */
		}
	}

	if (source != name) {
		name->labels = source->labels;
		name->length = source->length;
		if ((source->attributes & DNS_NAMEATTR_ABSOLUTE) != 0)
			name->attributes = DNS_NAMEATTR_ABSOLUTE;
		else
			name->attributes = 0;
		if (name->labels > 0 && name->offsets != NULL)
			set_offsets(name, name->offsets, NULL);
	}

	isc_buffer_add(target, name->length);

	return (ISC_R_SUCCESS);
}

static void
set_offsets(const dns_name_t *name, unsigned char *offsets,
	    dns_name_t *set_name)
{
	unsigned int offset, count, length, nlabels;
	unsigned char *ndata;
	int absolute;

	ndata = name->ndata;
	length = name->length;
	offset = 0;
	nlabels = 0;
	absolute = 0;
	while (offset != length) {
		INSIST(nlabels < 128);
		offsets[nlabels++] = offset;
		count = *ndata++;
		offset++;
		INSIST(count <= 63);
		offset += count;
		ndata += count;
		INSIST(offset <= length);
		if (count == 0) {
			absolute = 1;
			break;
		}
	}
	if (set_name != NULL) {
		INSIST(set_name == name);

		set_name->labels = nlabels;
		set_name->length = offset;
		if (absolute)
			set_name->attributes |= DNS_NAMEATTR_ABSOLUTE;
		else
			set_name->attributes &= ~DNS_NAMEATTR_ABSOLUTE;
	}
	INSIST(nlabels == name->labels);
	INSIST(offset == name->length);
}

isc_result_t
dns_name_fromwire(dns_name_t *name, isc_buffer_t *source,
		  dns_decompress_t *dctx, unsigned int options,
		  isc_buffer_t *target)
{
	unsigned char *cdata, *ndata;
	unsigned int cused; /* Bytes of compressed name data used */
	unsigned int nused, labels, n, nmax;
	unsigned int current, new_current, biggest_pointer;
	int done;
	fw_state state = fw_start;
	unsigned int c;
	unsigned char *offsets;
	dns_offsets_t odata;
	int downcase;
	int seen_pointer;

	/*
	 * Copy the possibly-compressed name at source into target,
	 * decompressing it.  Loop prevention is performed by checking
	 * the new pointer against biggest_pointer.
	 */

	downcase = (options & DNS_NAME_DOWNCASE) != 0;

	if (target == NULL && name->buffer != NULL) {
		target = name->buffer;
		isc_buffer_clear(target);
	}

	REQUIRE(dctx != NULL);
	REQUIRE(BINDABLE(name));

	INIT_OFFSETS(name, offsets, odata);

	/*
	 * Make 'name' empty in case of failure.
	 */
	MAKE_EMPTY(name);

	/*
	 * Initialize things to make the compiler happy; they're not required.
	 */
	n = 0;
	new_current = 0;

	/*
	 * Set up.
	 */
	labels = 0;
	done = 0;

	ndata = isc_buffer_used(target);
	nused = 0;
	seen_pointer = 0;

	/*
	 * Find the maximum number of uncompressed target name
	 * bytes we are willing to generate.  This is the smaller
	 * of the available target buffer length and the
	 * maximum legal domain name length (255).
	 */
	nmax = isc_buffer_availablelength(target);
	if (nmax > DNS_NAME_MAXWIRE)
		nmax = DNS_NAME_MAXWIRE;

	cdata = isc_buffer_current(source);
	cused = 0;

	current = source->current;
	biggest_pointer = current;

	/*
	 * Note:  The following code is not optimized for speed, but
	 * rather for correctness.  Speed will be addressed in the future.
	 */

	while (current < source->active && !done) {
		c = *cdata++;
		current++;
		if (!seen_pointer)
			cused++;

		switch (state) {
		case fw_start:
			if (c < 64) {
				offsets[labels] = nused;
				labels++;
				if (nused + c + 1 > nmax)
					goto full;
				nused += c + 1;
				*ndata++ = c;
				if (c == 0)
					done = 1;
				n = c;
				state = fw_ordinary;
			} else if (c >= 128 && c < 192) {
				/*
				 * 14 bit local compression pointer.
				 * Local compression is no longer an
				 * IETF draft.
				 */
				return (DNS_R_BADLABELTYPE);
			} else if (c >= 192) {
				/*
				 * Ordinary 14-bit pointer.
				 */
				if ((dctx->allowed & DNS_COMPRESS_GLOBAL14) ==
				    0)
					return (DNS_R_DISALLOWED);
				new_current = c & 0x3F;
				state = fw_newcurrent;
			} else
				return (DNS_R_BADLABELTYPE);
			break;
		case fw_ordinary:
			if (downcase)
				c = maptolower[c];
			*ndata++ = c;
			n--;
			if (n == 0)
				state = fw_start;
			break;
		case fw_newcurrent:
			new_current *= 256;
			new_current += c;
			if (new_current >= biggest_pointer)
				return (DNS_R_BADPOINTER);
			biggest_pointer = new_current;
			current = new_current;
			cdata = (unsigned char *)source->base + current;
			seen_pointer = 1;
			state = fw_start;
			break;
		default:
			FATAL_ERROR(__FILE__, __LINE__,
				    "Unknown state %d", state);
			/* Does not return. */
		}
	}

	if (!done)
		return (ISC_R_UNEXPECTEDEND);

	name->ndata = (unsigned char *)target->base + target->used;
	name->labels = labels;
	name->length = nused;
	name->attributes |= DNS_NAMEATTR_ABSOLUTE;

	isc_buffer_forward(source, cused);
	isc_buffer_add(target, name->length);

	return (ISC_R_SUCCESS);

 full:
	if (nmax == DNS_NAME_MAXWIRE)
		/*
		 * The name did not fit even though we had a buffer
		 * big enough to fit a maximum-length name.
		 */
		return (DNS_R_NAMETOOLONG);
	else
		/*
		 * The name might fit if only the caller could give us a
		 * big enough buffer.
		 */
		return (ISC_R_NOSPACE);
}

isc_result_t
dns_name_towire(const dns_name_t *name, dns_compress_t *cctx,
		isc_buffer_t *target)
{
	unsigned int methods;
	uint16_t offset;
	dns_name_t gp;	/* Global compression prefix */
	int gf;	/* Global compression target found */
	uint16_t go;	/* Global compression offset */
	dns_offsets_t clo;
	dns_name_t clname;

	/*
	 * Convert 'name' into wire format, compressing it as specified by the
	 * compression context 'cctx', and storing the result in 'target'.
	 */

	REQUIRE(cctx != NULL);

	/*
	 * If 'name' doesn't have an offsets table, make a clone which
	 * has one.
	 */
	if (name->offsets == NULL) {
		dns_name_init(&clname, clo);
		dns_name_clone(name, &clname);
		name = &clname;
	}
	dns_name_init(&gp, NULL);

	offset = target->used;	/*XXX*/

	methods = dns_compress_getmethods(cctx);

	if ((name->attributes & DNS_NAMEATTR_NOCOMPRESS) == 0 &&
	    (methods & DNS_COMPRESS_GLOBAL14) != 0)
		gf = dns_compress_findglobal(cctx, name, &gp, &go);
	else
		gf = 0;

	/*
	 * If the offset is too high for 14 bit global compression, we're
	 * out of luck.
	 */
	if (gf && go >= 0x4000)
		gf = 0;

	/*
	 * Will the compression pointer reduce the message size?
	 */
	if (gf && (gp.length + 2) >= name->length)
		gf = 0;

	if (gf) {
		if (target->length - target->used < gp.length)
			return (ISC_R_NOSPACE);
		if (gp.length != 0) {
			unsigned char *base = target->base;
			(void)memmove(base + target->used, gp.ndata,
				      (size_t)gp.length);
		}
		isc_buffer_add(target, gp.length);
		go |= 0xc000;
		if (target->length - target->used < 2)
			return (ISC_R_NOSPACE);
		isc_buffer_putuint16(target, go);
		if (gp.length != 0)
			dns_compress_add(cctx, name, &gp, offset);
	} else {
		if (target->length - target->used < name->length)
			return (ISC_R_NOSPACE);
		if (name->length != 0) {
			unsigned char *base = target->base;
			(void)memmove(base + target->used, name->ndata,
				      (size_t)name->length);
		}
		isc_buffer_add(target, name->length);
		dns_compress_add(cctx, name, name, offset);
	}
	return (ISC_R_SUCCESS);
}

isc_result_t
dns_name_concatenate(dns_name_t *prefix, dns_name_t *suffix, dns_name_t *name,
		     isc_buffer_t *target)
{
	unsigned char *ndata, *offsets;
	unsigned int nrem, labels, prefix_length, length;
	int copy_prefix = 1;
	int copy_suffix = 1;
	int absolute = 0;
	dns_name_t tmp_name;
	dns_offsets_t odata;

	/*
	 * Concatenate 'prefix' and 'suffix'.
	 */

	if (prefix == NULL || prefix->labels == 0)
		copy_prefix = 0;
	if (suffix == NULL || suffix->labels == 0)
		copy_suffix = 0;
	if (copy_prefix &&
	    (prefix->attributes & DNS_NAMEATTR_ABSOLUTE) != 0) {
		absolute = 1;
		REQUIRE(!copy_suffix);
	}
	if (name == NULL) {
		dns_name_init(&tmp_name, odata);
		name = &tmp_name;
	}
	if (target == NULL) {
		INSIST(name->buffer != NULL);
		target = name->buffer;
		isc_buffer_clear(name->buffer);
	}

	REQUIRE(BINDABLE(name));

	/*
	 * Set up.
	 */
	nrem = target->length - target->used;
	ndata = (unsigned char *)target->base + target->used;
	if (nrem > DNS_NAME_MAXWIRE)
		nrem = DNS_NAME_MAXWIRE;
	length = 0;
	prefix_length = 0;
	labels = 0;
	if (copy_prefix) {
		prefix_length = prefix->length;
		length += prefix_length;
		labels += prefix->labels;
	}
	if (copy_suffix) {
		length += suffix->length;
		labels += suffix->labels;
	}
	if (length > DNS_NAME_MAXWIRE) {
		MAKE_EMPTY(name);
		return (DNS_R_NAMETOOLONG);
	}
	if (length > nrem) {
		MAKE_EMPTY(name);
		return (ISC_R_NOSPACE);
	}

	if (copy_suffix) {
		if ((suffix->attributes & DNS_NAMEATTR_ABSOLUTE) != 0)
			absolute = 1;
		memmove(ndata + prefix_length, suffix->ndata, suffix->length);
	}

	/*
	 * If 'prefix' and 'name' are the same object, and the object has
	 * a dedicated buffer, and we're using it, then we don't have to
	 * copy anything.
	 */
	if (copy_prefix && (prefix != name || prefix->buffer != target))
		memmove(ndata, prefix->ndata, prefix_length);

	name->ndata = ndata;
	name->labels = labels;
	name->length = length;
	if (absolute)
		name->attributes = DNS_NAMEATTR_ABSOLUTE;
	else
		name->attributes = 0;

	if (name->labels > 0 && name->offsets != NULL) {
		INIT_OFFSETS(name, offsets, odata);
		set_offsets(name, offsets, NULL);
	}

	isc_buffer_add(target, name->length);

	return (ISC_R_SUCCESS);
}

isc_result_t
dns_name_dup(const dns_name_t *source,
	     dns_name_t *target)
{
	/*
	 * Make 'target' a dynamically allocated copy of 'source'.
	 */

	REQUIRE(source->length > 0);
	REQUIRE(BINDABLE(target));

	/*
	 * Make 'target' empty in case of failure.
	 */
	MAKE_EMPTY(target);

	target->ndata = malloc(source->length);
	if (target->ndata == NULL)
		return (ISC_R_NOMEMORY);

	memmove(target->ndata, source->ndata, source->length);

	target->length = source->length;
	target->labels = source->labels;
	target->attributes = DNS_NAMEATTR_DYNAMIC;
	if ((source->attributes & DNS_NAMEATTR_ABSOLUTE) != 0)
		target->attributes |= DNS_NAMEATTR_ABSOLUTE;
	if (target->offsets != NULL) {
		if (source->offsets != NULL)
			memmove(target->offsets, source->offsets,
				source->labels);
		else
			set_offsets(target, target->offsets, NULL);
	}

	return (ISC_R_SUCCESS);
}

isc_result_t
dns_name_dupwithoffsets(dns_name_t *source,
			dns_name_t *target)
{
	/*
	 * Make 'target' a read-only dynamically allocated copy of 'source'.
	 * 'target' will also have a dynamically allocated offsets table.
	 */

	REQUIRE(source->length > 0);
	REQUIRE(BINDABLE(target));
	REQUIRE(target->offsets == NULL);

	/*
	 * Make 'target' empty in case of failure.
	 */
	MAKE_EMPTY(target);

	target->ndata = malloc(source->length + source->labels);
	if (target->ndata == NULL)
		return (ISC_R_NOMEMORY);

	memmove(target->ndata, source->ndata, source->length);

	target->length = source->length;
	target->labels = source->labels;
	target->attributes = DNS_NAMEATTR_DYNAMIC | DNS_NAMEATTR_DYNOFFSETS |
		DNS_NAMEATTR_READONLY;
	if ((source->attributes & DNS_NAMEATTR_ABSOLUTE) != 0)
		target->attributes |= DNS_NAMEATTR_ABSOLUTE;
	target->offsets = target->ndata + source->length;
	if (source->offsets != NULL)
		memmove(target->offsets, source->offsets, source->labels);
	else
		set_offsets(target, target->offsets, NULL);

	return (ISC_R_SUCCESS);
}

void
dns_name_free(dns_name_t *name) {
	/*
	 * Free 'name'.
	 */

	REQUIRE((name->attributes & DNS_NAMEATTR_DYNAMIC) != 0);

	free(name->ndata);
	dns_name_invalidate(name);
}

int
dns_name_dynamic(dns_name_t *name) {

	/*
	 * Returns whether there is dynamic memory associated with this name.
	 */

	return ((name->attributes & DNS_NAMEATTR_DYNAMIC) != 0 ?
		1 : 0);
}

void
dns_name_format(dns_name_t *name, char *cp, unsigned int size) {
	isc_result_t result;
	isc_buffer_t buf;

	REQUIRE(size > 0);

	/*
	 * Leave room for null termination after buffer.
	 */
	isc_buffer_init(&buf, cp, size - 1);
	result = dns_name_totext(name, 1, &buf);
	if (result == ISC_R_SUCCESS) {
		/*
		 * Null terminate.
		 */
		isc_region_t r;
		isc_buffer_usedregion(&buf, &r);
		((char *) r.base)[r.length] = '\0';

	} else
		snprintf(cp, size, "<unknown>");
}

/*
 * dns_name_fromstring() -- convert directly from a string to a name,
 * allocating memory as needed
 */
isc_result_t
dns_name_fromstring2(dns_name_t *target, const char *src,
		     const dns_name_t *origin, unsigned int options)
{
	isc_result_t result;
	isc_buffer_t buf;
	dns_fixedname_t fn;
	dns_name_t *name;

	REQUIRE(src != NULL);

	isc_buffer_init(&buf, (void *)src, strlen(src));
	isc_buffer_add(&buf, strlen(src));
	if (BINDABLE(target) && target->buffer != NULL)
		name = target;
	else {
		dns_fixedname_init(&fn);
		name = dns_fixedname_name(&fn);
	}

	result = dns_name_fromtext(name, &buf, origin, options, NULL);
	if (result != ISC_R_SUCCESS)
		return (result);

	if (name != target)
		result = dns_name_dupwithoffsets(name, target);
	return (result);
}

isc_result_t
dns_name_copy(dns_name_t *source, dns_name_t *dest, isc_buffer_t *target) {
	unsigned char *ndata;

	/*
	 * Make dest a copy of source.
	 */

	REQUIRE(target != NULL || dest->buffer != NULL);

	if (target == NULL) {
		target = dest->buffer;
		isc_buffer_clear(dest->buffer);
	}

	REQUIRE(BINDABLE(dest));

	/*
	 * Set up.
	 */
	if (target->length - target->used < source->length)
		return (ISC_R_NOSPACE);

	ndata = (unsigned char *)target->base + target->used;
	dest->ndata = target->base;

	if (source->length != 0)
		memmove(ndata, source->ndata, source->length);

	dest->ndata = ndata;
	dest->labels = source->labels;
	dest->length = source->length;
	if ((source->attributes & DNS_NAMEATTR_ABSOLUTE) != 0)
		dest->attributes = DNS_NAMEATTR_ABSOLUTE;
	else
		dest->attributes = 0;

	if (dest->labels > 0 && dest->offsets != NULL) {
		if (source->offsets != NULL)
			memmove(dest->offsets, source->offsets, source->labels);
		else
			set_offsets(dest, dest->offsets, NULL);
	}

	isc_buffer_add(target, dest->length);

	return (ISC_R_SUCCESS);
}
